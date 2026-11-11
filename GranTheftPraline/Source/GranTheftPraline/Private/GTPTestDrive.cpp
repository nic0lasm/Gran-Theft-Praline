#include "GTPTestDrive.h"

#include "ChaosVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GTPDriveLogic.h"
#include "GTPLog.h"
#include "GTPTraffic.h"
#include "GTPVehicle.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPTestDrive, Log, All);

namespace GTPTestDriveLocal
{
	constexpr double PosLogInterval = 5.0;
	constexpr double PlaceTimeout = 20.0;
	constexpr double EnterTimeout = 10.0;
	/** Ground to actor origin of a car (cm), as the traffic ghosts (GTPTraffic.h). */
	constexpr float RideHeightCm = 40.f;
	/** The car must stand this close to the first point when the drive starts. */
	constexpr double StartToleranceCm = 300.0;
	/** Physics mode: farther than this from the path point being followed = off the path. */
	constexpr double OffPathCm = 1500.0;
	/** The heading looks this many seconds behind and ahead, so it turns smoothly through the path points. */
	constexpr double HeadingWindow = 0.4;

	const TCHAR* PhaseName(int32 Phase)
	{
		static const TCHAR* Names[] = { TEXT("wait for pawn"), TEXT("place"), TEXT("enter"), TEXT("hold"), TEXT("testdrive"), TEXT("done") };
		return (Phase >= 0 && Phase < static_cast<int32>(UE_ARRAY_COUNT(Names))) ? Names[Phase] : TEXT("?");
	}
}

TArray<UGTPTestDrive::FPoint> UGTPTestDrive::LoadPath(const FString& File)
{
	TArray<FPoint> Out;
	TArray<FString> Rows;
	if (!FFileHelper::LoadFileToStringArray(Rows, *File))
	{
		return Out;
	}
	for (int32 Index = 1; Index < Rows.Num(); ++Index)
	{
		TArray<FString> Fields;
		Rows[Index].TrimStartAndEnd().ParseIntoArray(Fields, TEXT(","));
		if (Fields.Num() < 4)
		{
			continue;
		}
		FPoint Point;
		Point.T = FCString::Atod(*Fields[0]);
		Point.Location = FVector2D(FCString::Atod(*Fields[1]), FCString::Atod(*Fields[2]));
		Point.SpeedKmh = FCString::Atof(*Fields[3]);
		if (Out.Num() > 0 && Point.T <= Out.Last().T)
		{
			continue; // time must grow: a broken row is dropped, not replayed backwards
		}
		Out.Add(Point);
	}
	if (Out.Num() < 2)
	{
		Out.Reset();
	}
	return Out;
}

void UGTPTestDrive::Sample(const TArray<FPoint>& InPath, double T, FVector2D& OutLocation, float& OutYaw, float& OutSpeedKmh)
{
	OutLocation = FVector2D::ZeroVector;
	OutYaw = 0.f;
	OutSpeedKmh = 0.f;
	if (InPath.Num() < 2)
	{
		return;
	}
	auto At = [&InPath](double Time, float* Speed) -> FVector2D
	{
		Time = FMath::Clamp(Time, InPath[0].T, InPath.Last().T);
		// Binary search for the segment holding Time.
		int32 Lo = 0;
		int32 Hi = InPath.Num() - 1;
		while (Hi - Lo > 1)
		{
			const int32 Mid = (Lo + Hi) / 2;
			(InPath[Mid].T <= Time ? Lo : Hi) = Mid;
		}
		const double Span = InPath[Hi].T - InPath[Lo].T;
		const double Alpha = Span > 0.0 ? (Time - InPath[Lo].T) / Span : 0.0;
		if (Speed)
		{
			*Speed = FMath::Lerp(InPath[Lo].SpeedKmh, InPath[Hi].SpeedKmh, static_cast<float>(Alpha));
		}
		return FMath::Lerp(InPath[Lo].Location, InPath[Hi].Location, Alpha);
	};
	OutLocation = At(T, &OutSpeedKmh);
	FVector2D Ahead = At(T + GTPTestDriveLocal::HeadingWindow, nullptr) - At(T - GTPTestDriveLocal::HeadingWindow, nullptr);
	if (Ahead.SizeSquared() < 1.0)
	{
		// Standing still (start, end): face along the path from here.
		int32 Next = 1;
		while (Next < InPath.Num() - 1 && InPath[Next].T <= T)
		{
			++Next;
		}
		Ahead = InPath[Next].Location - InPath[Next - 1].Location;
	}
	OutYaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Ahead.Y, Ahead.X)));
}

void UGTPTestDrive::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString Name;
	if (!FParse::Value(FCommandLine::Get(), TEXT("TestDrive="), Name) || Name.IsEmpty())
	{
		return;
	}
	const FString File = (Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")))
		? Name
		: FPaths::ProjectContentDir() / TEXT("Test/Drives") / (Name + TEXT(".csv"));
	Path = LoadPath(File);
	if (Path.Num() == 0)
	{
		UE_LOG(LogGTPTestDrive, Error, TEXT("Test drive: no path in %s"), *File);
		GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive error: no path in %s"), *File);
		return;
	}
	FString Mode;
	if (FParse::Value(FCommandLine::Get(), TEXT("TestDriveMode="), Mode))
	{
		bRail = !Mode.Equals(TEXT("physics"), ESearchCase::IgnoreCase);
	}
	bLoop = FParse::Param(FCommandLine::Get(), TEXT("TestDriveLoop"));
	bQuit = FParse::Param(FCommandLine::Get(), TEXT("TestDriveQuit"));
	FParse::Value(FCommandLine::Get(), TEXT("TestDriveDelay="), Delay);
	Delay = FMath::Clamp(Delay, 0.0, 120.0);
	DrivePoints.reserve(Path.Num());
	for (const FPoint& Point : Path)
	{
		DrivePoints.push_back({ static_cast<float>(Point.Location.X * 0.01), static_cast<float>(Point.Location.Y * 0.01) });
	}
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPTestDrive::Tick));
	GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive %s: %d points, %.1f s, mode %s%s"), *Name, Path.Num(), Path.Last().T,
		bRail ? TEXT("rail") : TEXT("physics"), bLoop ? TEXT(", loop") : TEXT(""));
}

void UGTPTestDrive::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	Super::Deinitialize();
}

APlayerController* UGTPTestDrive::GetController() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}

double UGTPTestDrive::GameTime() const
{
	const APlayerController* PC = GetController();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	return World ? World->GetTimeSeconds() : 0.0;
}

void UGTPTestDrive::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStart = GameTime();
	GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive %.1f s: %s"), static_cast<float>(PhaseStart), GTPTestDriveLocal::PhaseName(static_cast<int32>(NewPhase)));
}

AGTPVehicle* UGTPTestDrive::FindCar() const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return nullptr;
	}
	AGTPVehicle* Best = nullptr;
	double BestDist = TNumericLimits<double>::Max();
	for (TActorIterator<AGTPVehicle> It(World); It; ++It)
	{
		if (It->IsWrecked() || It->GetDriver() != nullptr || It->HasAIDriver())
		{
			continue;
		}
		const double Dist = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = *It;
		}
	}
	return Best;
}

float UGTPTestDrive::GroundZ(const AGTPVehicle* InCar, const FVector2D& At, float FallbackZ) const
{
	UWorld* World = InCar ? InCar->GetWorld() : nullptr;
	if (!World)
	{
		return FallbackZ;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPTestDriveGround), false, InCar);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	const FVector From(At.X, At.Y, FallbackZ + 300.f);
	const FVector To(At.X, At.Y, FallbackZ - 1000.f);
	if (World->LineTraceSingleByObjectType(Hit, From, To, Objects, Params))
	{
		return static_cast<float>(Hit.ImpactPoint.Z) + GTPTestDriveLocal::RideHeightCm;
	}
	return FallbackZ;
}

void UGTPTestDrive::PutAtStart(AGTPVehicle* InCar)
{
	FVector2D Start;
	float Yaw = 0.f;
	float Kmh = 0.f;
	Sample(Path, Path[0].T, Start, Yaw, Kmh);
	LastZ = GroundZ(InCar, Start, static_cast<float>(InCar->GetActorLocation().Z));
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(InCar->GetRootComponent()))
	{
		if (Root->IsSimulatingPhysics())
		{
			Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}
	InCar->SetActorLocationAndRotation(FVector(Start.X, Start.Y, LastZ), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::ResetPhysics);
}

void UGTPTestDrive::LogPosition(const AGTPVehicle* InCar, double T)
{
	const int32 Slot = static_cast<int32>(T / GTPTestDriveLocal::PosLogInterval);
	if (Slot <= LastLoggedSlot || !InCar)
	{
		return;
	}
	LastLoggedSlot = Slot;
	const FVector Location = InCar->GetActorLocation();
	float Kmh = InCar->GetSpeedKmh();
	if (bRail)
	{
		FVector2D Unused;
		float Yaw = 0.f;
		UGTPTestDrive::Sample(Path, T, Unused, Yaw, Kmh);
	}
	GTP_TRACE(LogGTPTestDrive, TEXT("DemoPos t=%.1f x=%.0f y=%.0f yaw=%.0f kmh=%.0f phase=testdrive"), Slot * GTPTestDriveLocal::PosLogInterval, Location.X, Location.Y,
		FRotator::NormalizeAxis(InCar->GetActorRotation().Yaw), Kmh);
}

void UGTPTestDrive::StepRail(AGTPVehicle* InCar, double T)
{
	FVector2D Location;
	float Yaw = 0.f;
	float Kmh = 0.f;
	Sample(Path, T, Location, Yaw, Kmh);
	LastZ = GroundZ(InCar, Location, LastZ);
	InCar->SetActorLocationAndRotation(FVector(Location.X, Location.Y, LastZ), FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
}

void UGTPTestDrive::StepPhysics(AGTPVehicle* InCar, double T)
{
	// Through the car, as the AI drivers: keeps ThrottleAmount (engine sound, load) in step.
	auto Pedals = [InCar](float Throttle, float Brake, float Steering)
	{
		InCar->SetScriptedDriveInput(Throttle, Brake, Steering, false);
	};
	const double Now = GameTime();
	if (Now < UnstickUntil)
	{
		Pedals(0.f, 0.6f, -UnstickSteer); // reverse out (bReverseAsBrake is set on every AGTPVehicle)
		return;
	}
	// The path is the plan; its speeds are the limits (pure pursuit as the AI drivers).
	const FVector Location = InCar->GetActorLocation();
	const int32 Index = FMath::Clamp(Closest, 0, Path.Num() - 1);
	const float LaneSpeed = FMath::Max(Path[Index].SpeedKmh, Path[FMath::Min(Index + 3, Path.Num() - 1)].SpeedKmh) / 3.6f;
	const float Obstacle = UGTPAIDriverComponent::ProbeAhead(InCar, 4000.f, 90.f);
	FGTPDriveSettings Settings;
	const FGTPDriveCommand Command = GTPDrive::Decide(DrivePoints, static_cast<float>(Location.X * 0.01), static_cast<float>(Location.Y * 0.01),
		FMath::DegreesToRadians(InCar->GetActorRotation().Yaw), InCar->GetSpeedKmh() / 3.6f, FMath::Max(LaneSpeed, 2.f), Obstacle, Settings, Closest);
	Pedals(Command.Throttle, Command.Brake, Command.Steering);
	// Stuck (kerb, bollard, a car that does not move): back off for a moment.
	const bool bWantsToMove = Command.TargetSpeed > 1.f && Obstacle < 0.f;
	if (bWantsToMove && InCar->GetSpeedKmh() < 1.f)
	{
		if (StuckSince < 0.0)
		{
			StuckSince = Now;
		}
		else if (Now - StuckSince > 3.0)
		{
			UnstickSteer = Command.Steering >= 0.f ? 1.f : -1.f;
			UnstickUntil = Now + 2.0;
			StuckSince = -1.0;
			GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive %.1f s: stuck at %s, backing off"), static_cast<float>(T), *Location.ToCompactString());
		}
	}
	else
	{
		StuckSince = -1.0;
	}
}

bool UGTPTestDrive::Tick(float DeltaTime)
{
	APlayerController* PC = GetController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!PC || !Pawn)
	{
		return true;
	}
	const double Now = GameTime();
	const double InPhase = Now - PhaseStart;
	AGTPVehicle* Driven = Car.Get();

	switch (Phase)
	{
	case EPhase::WaitForPawn:
		if (Cast<ACharacter>(Pawn))
		{
			SetPhase(EPhase::Place);
		}
		break;

	case EPhase::Place:
	{
		AGTPVehicle* Found = FindCar();
		ACharacter* Character = Cast<ACharacter>(Pawn);
		if (!Found || !Character)
		{
			if (InPhase > GTPTestDriveLocal::PlaceTimeout)
			{
				UE_LOG(LogGTPTestDrive, Error, TEXT("Test drive: no free car (add -TestVehicle)"));
				GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive error: no free car"));
				SetPhase(EPhase::Done);
			}
			break;
		}
		Car = Found;
		PutAtStart(Found);
		const float Yaw = static_cast<float>(Found->GetActorRotation().Yaw);
		const FVector Door = Found->GetDoorLocation(EGTPVehicleSide::Left);
		Character->SetActorLocation(FVector(Door.X, Door.Y, LastZ + 60.f), false, nullptr, ETeleportType::ResetPhysics);
		PC->SetControlRotation(FRotator(0.f, Yaw, 0.f));
		if (!Found->BeginEnter(Character, PC))
		{
			if (InPhase > GTPTestDriveLocal::PlaceTimeout)
			{
				GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive error: the car refused the entry"));
				SetPhase(EPhase::Done);
			}
			break;
		}
		SetPhase(EPhase::Enter);
		break;
	}

	case EPhase::Enter:
		if (Driven && Pawn == Driven)
		{
			// Both modes: physics off while the car is put at the start, so the teleport
			// cannot be undone by the solver (seen on M1: a physics car left 90 m away).
			if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Driven->GetRootComponent()))
			{
				Root->SetSimulatePhysics(false);
			}
			PutAtStart(Driven);
			SetPhase(EPhase::Hold);
		}
		else if (InPhase > GTPTestDriveLocal::EnterTimeout)
		{
			UE_LOG(LogGTPTestDrive, Error, TEXT("Test drive: entry did not finish in %.0f s"), GTPTestDriveLocal::EnterTimeout);
			GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive error: entry did not finish"));
			SetPhase(EPhase::Done);
		}
		break;

	case EPhase::Hold:
		if (Driven)
		{
			StepRail(Driven, Path[0].T); // parked on the first point, physics off
		}
		if (InPhase >= Delay && Driven)
		{
			const double FromStart = FVector::Dist2D(Driven->GetActorLocation(), FVector(Path[0].Location.X, Path[0].Location.Y, 0.0));
			if (FromStart > GTPTestDriveLocal::StartToleranceCm)
			{
				GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive error: car not at the start (%.0f cm away)"), FromStart);
				SetPhase(EPhase::Done);
				break;
			}
			if (!bRail)
			{
				if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Driven->GetRootComponent()))
				{
					Root->SetSimulatePhysics(true);
					Root->SetPhysicsLinearVelocity(FVector::ZeroVector);
					Root->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				}
			}
			LastLocation = Driven->GetActorLocation();
			OffPathSince = -1.0;
			DriveStart = Now;
			LastLoggedSlot = -1;
			Closest = 0;
			SetPhase(EPhase::Drive);
		}
		break;

	case EPhase::Drive:
	{
		if (!Driven || Pawn != Driven)
		{
			GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive error: the player left the car"));
			SetPhase(EPhase::Done);
			break;
		}
		const double T = Path[0].T + (Now - DriveStart);
		if (bRail)
		{
			StepRail(Driven, T);
		}
		else
		{
			StepPhysics(Driven, T);
		}
		LogPosition(Driven, T - Path[0].T);
		{
			// A car that moves farther in one frame than it could drive has been put back by
			// something (physics recovery, traffic pool, a teleport): the run is not valid.
			const FVector Here = Driven->GetActorLocation();
			const double Moved = FVector::Dist2D(Here, LastLocation);
			const double Possible = 200.0 + 3.0 * FMath::Max(DeltaTime, 0.f) * 4000.0; // 144 km/h margin
			if (Moved > Possible && (Now - DriveStart) > 0.5)
			{
				GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive jump t=%.1f %.0f cm from %s to %s"), static_cast<float>(T - Path[0].T), Moved,
					*LastLocation.ToCompactString(), *Here.ToCompactString());
			}
			LastLocation = Here;
			if (!bRail)
			{
				const int32 Near = FMath::Clamp(Closest, 0, Path.Num() - 1);
				const double Off = FVector2D::Distance(FVector2D(Here.X, Here.Y), Path[Near].Location);
				if (Off > GTPTestDriveLocal::OffPathCm)
				{
					if (OffPathSince < 0.0)
					{
						OffPathSince = Now;
					}
					else if (Now - OffPathSince > 5.0)
					{
						GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive error: off the path for 5 s (%.0f cm) at %s"), Off, *Here.ToCompactString());
						SetPhase(EPhase::Done);
						break;
					}
				}
				else
				{
					OffPathSince = -1.0;
				}
			}
		}
		const bool bEnded = bRail ? T >= Path.Last().T
			: (Closest >= Path.Num() - 2 && Driven->GetSpeedKmh() < 2.f) || T > Path.Last().T * 3.0 + 60.0;
		if (bEnded)
		{
			GTP_TRACE(LogGTPTestDrive, TEXT("TestDrive end t=%.1f x=%.0f y=%.0f"), static_cast<float>(T - Path[0].T),
				Driven->GetActorLocation().X, Driven->GetActorLocation().Y);
			if (bLoop)
			{
				// Back to the first point (a rail drive that ends where it began does not jump).
				if (!bRail)
				{
					if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Driven->GetRootComponent()))
					{
						Root->SetSimulatePhysics(false);
					}
					PutAtStart(Driven);
					SetPhase(EPhase::Hold);
				}
				else
				{
					StepRail(Driven, Path[0].T);
				}
				LastLocation = Driven->GetActorLocation();
				DriveStart = Now;
				LastLoggedSlot = -1;
				Closest = 0;
				break;
			}
			if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Driven->GetRootComponent()))
			{
				Root->SetSimulatePhysics(true); // the car is the player's again
			}
			Driven->SetScriptedDriveInput(0.f, 1.f, 0.f, false);
			SetPhase(EPhase::Done);
			if (bQuit)
			{
				FPlatformMisc::RequestExit(false);
			}
		}
		break;
	}

	case EPhase::Done:
		return false;
	}
	return true;
}
