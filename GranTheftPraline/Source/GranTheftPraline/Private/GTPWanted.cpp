#include "GTPWanted.h"

#include "GTPSirenComponent.h"

#include "GTPLog.h"
#include "GTPVehicle.h"
#include "GTPPedestrians.h"
#include "GTPLiteDriver.h"
#include "AIController.h"
#include "CollisionQueryParams.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPWanted, Log, All);

namespace GTPPolice
{
	const FName PoliceTag(TEXT("GTPPolice"));
}

namespace
{
	constexpr float WantedCmToM = 0.01f;

	bool WitnessesDisabledByCommandLine()
	{
		int32 Value = 1;
		FParse::Value(FCommandLine::Get(), TEXT("Witnesses="), Value);
		return Value == 0;
	}
}

// --- Wanted component ---------------------------------------------------------

UGTPWantedComponent::UGTPWantedComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

bool UGTPWantedComponent::IsPoliceEnabled()
{
	int32 Value = 1;
	FParse::Value(FCommandLine::Get(), TEXT("Police="), Value);
	return Value != 0 && !FParse::Param(FCommandLine::Get(), TEXT("NoPolice"));
}

UGTPWantedComponent* UGTPWantedComponent::Find(const UObject* WorldContext)
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	return PC ? PC->FindComponentByClass<UGTPWantedComponent>() : nullptr;
}

void UGTPWantedComponent::ReportCrime(EGTPCrimeKind Kind, const FVector& Location, bool bVictimInvolved)
{
	if (BustedBannerLeft > 0.f || !GetWorld() || !IsComponentTickEnabled())
	{
		return;
	}
	if (!bRequireWitnesses || WitnessesDisabledByCommandLine())
	{
		ApplyCrime(Kind, Location);
		return;
	}

	// Who saw or heard it: police cars and people close enough, with a clear line to the crime.
	std::vector<FGTPWitness> Witnesses;
	const FVector Eye = Location + FVector(0.f, 0.f, 150.f);
	auto AddWitness = [&](const AActor* Who, bool bPolice)
	{
		if (!IsValid(Who))
		{
			return;
		}
		const FVector Where = Who->GetActorLocation();
		FCollisionQueryParams WitnessParams(SCENE_QUERY_STAT(GTPWitnessSight), false, Who);
		FHitResult Hit;
		const bool bSees = FVector::Dist(Where, Location) < 8000.f
			&& !GetWorld()->LineTraceSingleByChannel(Hit, Where + FVector(0.f, 0.f, 150.f), Eye, ECC_Visibility, WitnessParams);
		FGTPWitness Witness;
		Witness.X = static_cast<float>(Where.X) * WantedCmToM;
		Witness.Y = static_cast<float>(Where.Y) * WantedCmToM;
		Witness.bPolice = bPolice;
		Witness.bCanSee = bSees;
		Witnesses.push_back(Witness);
	};
	for (const TWeakObjectPtr<AGTPVehicle>& Car : Police)
	{
		AddWitness(Car.Get(), true);
	}
	for (TActorIterator<AGTPPedestrian> It(GetWorld()); It; ++It)
	{
		AddWitness(*It, false);
	}
	const bool bLoud = Kind == EGTPCrimeKind::ShotsFired || Kind == EGTPCrimeKind::Killing || Kind == EGTPCrimeKind::KillPolice;
	const FGTPWitnessResult Result = GTPWitness::Evaluate(static_cast<float>(Location.X) * WantedCmToM, static_cast<float>(Location.Y) * WantedCmToM,
		bLoud, bVictimInvolved, Witnesses, WitnessSettings);
	if (!Result.bReported)
	{
		GTP_TRACE(LogGTPWanted, TEXT("wanted: crime %d not witnessed"), static_cast<int32>(Kind));
		return;
	}
	if (Result.Delay <= 0.f)
	{
		ApplyCrime(Kind, Location);
		return;
	}
	FPendingReport& Pending = PendingReports.AddDefaulted_GetRef();
	Pending.Kind = Kind;
	Pending.Location = Location;
	Pending.DueTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + Result.Delay;
	GTP_TRACE(LogGTPWanted, TEXT("wanted: crime %d seen, the police are called (%.0f s)"), static_cast<int32>(Kind), Result.Delay);
}

void UGTPWantedComponent::TickPendingReports(float Now)
{
	for (int32 Index = PendingReports.Num() - 1; Index >= 0; --Index)
	{
		if (Now >= PendingReports[Index].DueTime)
		{
			const FPendingReport Report = PendingReports[Index];
			PendingReports.RemoveAtSwap(Index);
			ApplyCrime(Report.Kind, Report.Location);
		}
	}
}

void UGTPWantedComponent::ApplyCrime(EGTPCrimeKind Kind, const FVector& Location)
{
	if (BustedBannerLeft > 0.f || !GetWorld() || !IsComponentTickEnabled())
	{
		return;
	}
	const int32 Before = State.GetStars();
	if (!State.AddCrime(Kind, static_cast<float>(GetWorld()->GetTimeSeconds())))
	{
		return;
	}
	if (Before == 0)
	{
		LastKnownLocation = Location;
	}
	NotifyIfChanged();
	GTP_TRACE(LogGTPWanted, TEXT("wanted: crime %d, heat %.0f, %d stars"), static_cast<int32>(Kind), State.GetHeat(), State.GetStars());
}

void UGTPWantedComponent::ForceStars(int32 Stars)
{
	State.ForceStars(Stars);
	NotifyIfChanged();
	if (const APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			LastKnownLocation = Pawn->GetActorLocation();
		}
	}
	GTP_TRACE(LogGTPWanted, TEXT("wanted: level forced to %d stars"), State.GetStars());
}

void UGTPWantedComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!IsPoliceEnabled())
	{
		GTP_TRACE(LogGTPWanted, TEXT("police: off"));
		SetComponentTickEnabled(false);
		return;
	}
	int32 Stars = 0;
	float Delay = 5.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("WantedStars="), Stars) && Stars > 0)
	{
		FParse::Value(FCommandLine::Get(), TEXT("WantedDelay="), Delay);
		ScriptedStars = Stars;
		ScriptedTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + Delay;
	}
	bHoldLevel = FParse::Param(FCommandLine::Get(), TEXT("WantedHold"));
}

void UGTPWantedComponent::EnsureMinStars(int32 Stars)
{
	if (Stars > State.GetStars())
	{
		ForceStars(Stars);
	}
}

void UGTPWantedComponent::NotifyIfChanged()
{
	const int32 Stars = State.GetStars();
	const int32 Phase = static_cast<int32>(State.GetPhase());
	if (Stars != LastNotifiedStars || Phase != LastNotifiedPhase)
	{
		LastNotifiedStars = Stars;
		LastNotifiedPhase = Phase;
		OnWantedChanged.Broadcast(Stars, State.GetPhase());
	}
}

void UGTPWantedComponent::ClearWanted()
{
	State.Clear();
	bSeenNow = false;
	NotifyIfChanged();
}

void UGTPWantedComponent::RegisterPolice(AGTPVehicle* Car)
{
	Police.AddUnique(Car);
}

void UGTPWantedComponent::UnregisterPolice(AGTPVehicle* Car)
{
	Police.Remove(Car);
}

bool UGTPWantedComponent::CanSeePlayer(const AGTPVehicle* Car, const FVector& PlayerLocation) const
{
	const double Distance = FVector::Dist(Car->GetActorLocation(), PlayerLocation);
	if (Distance <= NoticeRange)
	{
		return true;
	}
	if (Distance > SightRange)
	{
		return false;
	}
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceSight), false, Car);
	if (PC && PC->GetPawn())
	{
		Params.AddIgnoredActor(PC->GetPawn());
	}
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Car->GetActorLocation() + FVector(0.f, 0.f, 150.f),
		PlayerLocation + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params);
}

void UGTPWantedComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (BustedBannerLeft > 0.f)
	{
		BustedBannerLeft = FMath::Max(0.f, BustedBannerLeft - DeltaTime);
	}
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	if (ScriptedStars > 0 && ScriptedTime >= 0.f && Now >= ScriptedTime)
	{
		ForceStars(ScriptedStars);
		ScriptedTime = -1.f;
	}
	if (PendingReports.Num() > 0)
	{
		TickPendingReports(Now);
	}
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || State.GetStars() == 0)
	{
		bSeenNow = false;
		NotifyIfChanged();
		return;
	}

	const FVector PlayerLocation = Pawn->GetActorLocation();
	bool bSeen = false;
	float Nearest = -1.f;
	for (int32 Index = Police.Num() - 1; Index >= 0; --Index)
	{
		const AGTPVehicle* Car = Police[Index].Get();
		if (!IsValid(Car))
		{
			Police.RemoveAt(Index);
			continue;
		}
		const float Metres = static_cast<float>(FVector::Dist(Car->GetActorLocation(), PlayerLocation)) * WantedCmToM;
		Nearest = Nearest < 0.f ? Metres : FMath::Min(Nearest, Metres);
		bSeen = bSeen || CanSeePlayer(Car, PlayerLocation);
	}
	bSeenNow = bSeen;
	if (bSeen)
	{
		LastKnownLocation = PlayerLocation;
	}
	// Until the first unit has arrived the level cannot drop: nobody has had the chance to look.
	const bool bHold = bSeen || bHoldLevel || (bPoliceAvailable && Police.Num() == 0);

	if (Now >= NextTraceTime)
	{
		NextTraceTime = Now + 2.f;
		GTP_TRACE(LogGTPWanted, TEXT("wanted: t=%.0f stars=%d phase=%d units=%d nearest=%.0f m seen=%d arrest=%.2f"), Now, State.GetStars(),
			static_cast<int32>(State.GetPhase()), Police.Num(), Nearest, bSeen ? 1 : 0, State.GetArrestProgress());
	}

	const AGTPVehicle* Vehicle = Cast<AGTPVehicle>(Pawn);
	const float SpeedKmh = Vehicle ? FMath::Abs(Vehicle->GetSpeedKmh()) : 0.f;
	const int32 StarsBefore = State.GetStars();
	State.Tick(DeltaTime, bHold);
	if (State.UpdateArrest(DeltaTime, Nearest, SpeedKmh, Vehicle == nullptr))
	{
		OnBusted();
	}
	else if (StarsBefore > 0 && State.GetStars() == 0)
	{
		GTP_TRACE(LogGTPWanted, TEXT("wanted: level lost"));
	}
	NotifyIfChanged();
}

void UGTPWantedComponent::OnBusted()
{
	GTP_TRACE(LogGTPWanted, TEXT("wanted: busted"));
	State.Clear();
	bSeenNow = false;
	BustedBannerLeft = BustedSeconds;
	NotifyIfChanged();
	OnArrested.Broadcast();
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		return;
	}
	if (AGTPVehicle* Vehicle = Cast<AGTPVehicle>(PC->GetPawn()))
	{
		Vehicle->RequestExit();
	}
	PC->SetIgnoreMoveInput(true);
	GetWorld()->GetTimerManager().SetTimer(BustedTimer, this, &UGTPWantedComponent::EndBusted, BustedSeconds, false);
}

void UGTPWantedComponent::EndBusted()
{
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (ArrestReturnEnabled && PC->GetPawn())
		{
			// Back to the game at the station: the police have taken the player there.
			PC->GetPawn()->TeleportTo(ArrestReturnLocation, PC->GetPawn()->GetActorRotation());
		}
		PC->SetIgnoreMoveInput(false);
	}
}

// --- Police director -------------------------------------------------------------

AGTPPoliceDirector::AGTPPoliceDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	Random.Initialize(0x504F4C49);
}

void AGTPPoliceDirector::BeginPlay()
{
	Super::BeginPlay();
	FParse::Value(FCommandLine::Get(), TEXT("PoliceMax="), MaxUnits);
	MaxUnits = FMath::Clamp(MaxUnits, 0, 6);
	LoadedPoliceClass = PoliceVehicleClass.TryLoadClass<AGTPVehicle>();
	LoadedDriverClass = PoliceDriverClass.TryLoadClass<ACharacter>();
	int32 LiteClassSwitch = 0;
	FParse::Value(FCommandLine::Get(), TEXT("LiteDriverClass="), LiteClassSwitch);
	if (LiteClassSwitch != 0)
	{
		// Camera-less occupant: no camera host is ever created for a police driver.
		LoadedDriverClass = AGTPLiteDriver::StaticClass();
	}
	NextSpawnTime = GetWorld()->GetTimeSeconds() + 3.f;
	GTP_TRACE(LogGTPWanted, TEXT("police: on, up to %d units, class %s"), MaxUnits, LoadedPoliceClass ? *LoadedPoliceClass->GetName() : TEXT("traffic cars"));
}

void AGTPPoliceDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
	{
		Wanted->SetPoliceAvailable(false);
	}
	Super::EndPlay(EndPlayReason);
}

bool AGTPPoliceDirector::FindSpawnPoint(const FVector& PlayerLocation, int32& OutLane, int32& OutPoint, FTransform& OutTransform) const
{
	const TArray<FGTPTrafficLane>& Lanes = Network->GetLanes();
	if (Lanes.Num() == 0)
	{
		return false;
	}
	FRandomStream& Dice = const_cast<FRandomStream&>(Random);
	for (int32 Try = 0; Try < 60; ++Try)
	{
		const int32 Lane = Dice.RandRange(0, Lanes.Num() - 1);
		const TArray<FVector>& Points = Lanes[Lane].Points;
		if (Points.Num() < 4 || Lanes[Lane].Next.Num() == 0)
		{
			continue;
		}
		const int32 Point = Dice.RandRange(1, Points.Num() - 3);
		FVector Location = Points[Point];
		const double Distance = FVector::Dist2D(Location, PlayerLocation);
		if (Distance < MinSpawnDistance || Distance > MaxSpawnDistance)
		{
			continue;
		}
		FHitResult Ground;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceGround), false);
		if (!GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			continue;
		}
		Location.Z = Ground.ImpactPoint.Z + 40.f;
		bool bFree = true;
		for (const FUnit& Unit : Units)
		{
			if (Unit.Car.IsValid() && FVector::Dist(Unit.Car->GetActorLocation(), Location) < 1500.f)
			{
				bFree = false;
				break;
			}
		}
		if (!bFree || Network->IsSeenByPlayer(Location))
		{
			continue;
		}
		const FVector Direction = (Points[Point + 1] - Points[Point]).GetSafeNormal2D();
		OutLane = Lane;
		OutPoint = Point;
		OutTransform = FTransform(Direction.Rotation(), Location);
		return true;
	}
	return false;
}

bool AGTPPoliceDirector::SpawnUnit(const FVector& PlayerLocation)
{
	TSubclassOf<AGTPVehicle> Class = LoadedPoliceClass;
	if (!Class && Network->LoadedVehicleClasses.Num() > 0)
	{
		Class = Network->LoadedVehicleClasses[Random.RandRange(0, Network->LoadedVehicleClasses.Num() - 1)];
	}
	if (!Class || !(LoadedDriverClass.Get() || Network->LoadedDriverClass.Get()))
	{
		return false;
	}
	int32 Lane = 0, Point = 0;
	FTransform Transform;
	if (!FindSpawnPoint(PlayerLocation, Lane, Point, Transform))
	{
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	AGTPVehicle* Car = GetWorld()->SpawnActor<AGTPVehicle>(Class, Transform, Params);
	if (!Car)
	{
		return false;
	}
	Car->Tags.AddUnique(GTPPolice::PoliceTag);

	FUnit& Unit = Units.AddDefaulted_GetRef();
	Unit.Car = Car;
	// Chase tuning: harder bends and braking than the calm city driving.
	Unit.Settings.MaxLateralAccel = 4.f;
	Unit.Settings.ComfortDecel = 5.f;
	Unit.Settings.SafeGap = 6.f;
	Unit.Settings.ThrottleGain = 0.5f;
	// The driver, the light bar and the registration come on the next frames: one heavy step per frame.
	Unit.Stage = 1;
	GTP_TRACE(LogGTPWanted, TEXT("police: %s on lane %d at %s"), *Car->GetName(), Lane, *Transform.GetLocation().ToCompactString());
	return true;
}

bool AGTPPoliceDirector::SeatDriver(FUnit& Unit)
{
	AGTPVehicle* Car = Unit.Car.Get();
	TSubclassOf<ACharacter> DriverClass = LoadedDriverClass.Get() != nullptr ? LoadedDriverClass : Network->LoadedDriverClass;
	if (!Car || !DriverClass)
	{
		return false;
	}
	FActorSpawnParameters DriverParams;
	DriverParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Driver = GetWorld()->SpawnActor<ACharacter>(DriverClass, Car->GetActorTransform(), DriverParams);
	if (!Driver)
	{
		return false;
	}
	Driver->AIControllerClass = AAIController::StaticClass();
	Driver->SpawnDefaultController();
	Car->SeatOccupant(Driver);
	Network->MakeCheap(Car, Driver);
	if (!Car->HasAIDriver())
	{
		// Not seated: do not leave an orphan character (and its controller) behind.
		if (AController* Controller = Driver->GetController())
		{
			Controller->Destroy();
		}
		Driver->Destroy();
		return false;
	}
	return true;
}

void AGTPPoliceDirector::RepositionUnit(FUnit& Unit, const FVector& PlayerLocation)
{
	AGTPVehicle* Car = Unit.Car.Get();
	int32 Lane = 0, Point = 0;
	FTransform Transform;
	if (!Car || !FindSpawnPoint(PlayerLocation, Lane, Point, Transform))
	{
		return;
	}
	// A move instead of a new car and driver: no spawn hitch.
	Car->SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	if (USkeletalMeshComponent* Body = Car->GetMesh())
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	Unit.Follower = FGTPLaneFollower();
	Unit.bOnLanes = false;
	Unit.StoppedTime = 0.f;
	GTP_TRACE(LogGTPWanted, TEXT("police: %s moved to lane %d"), *Car->GetName(), Lane);
}

void AGTPPoliceDirector::AddLightBar(FUnit& Unit)
{
	AGTPVehicle* Car = Unit.Car.Get();
	if (!Car)
	{
		return;
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Car);
		Light->SetupAttachment(Car->GetRootComponent());
		Light->SetRelativeLocation(LightBarOffset + FVector(0.f, Side == 0 ? -30.f : 30.f, 0.f));
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetIntensity(600.f);
		Light->SetAttenuationRadius(1500.f);
		Light->SetLightColor(Side == 0 ? FLinearColor(0.1f, 0.25f, 1.f) : FLinearColor(1.f, 0.08f, 0.05f));
		Light->SetCastShadows(false);
		Light->SetVisibility(false);
		Light->RegisterComponent();
		Unit.Lights.Add(Light);
	}
}

void AGTPPoliceDirector::BlinkLights(FUnit& Unit, bool bOn, bool bPhase)
{
	// 0 = off, 1 = blue, 2 = red: touch the lights only when the state changes.
	const int8 Wanted = !bOn ? 0 : (bPhase ? 1 : 2);
	if (Unit.LightState == Wanted)
	{
		return;
	}
	Unit.LightState = Wanted;
	for (int32 Index = 0; Index < Unit.Lights.Num(); ++Index)
	{
		if (UPointLightComponent* Light = Unit.Lights[Index].Get())
		{
			Light->SetVisibility(Wanted == Index + 1);
		}
	}
}

void AGTPPoliceDirector::UpdateSiren(FUnit& Unit, int32 Stars, bool bPursuit)
{
	const EGTPSirenMode Mode = Stars <= 0 ? EGTPSirenMode::Off : (bPursuit ? EGTPSirenMode::Yelp : EGTPSirenMode::TwoTone);
	if (Unit.SirenState == static_cast<int8>(Mode))
	{
		return;
	}
	Unit.SirenState = static_cast<int8>(Mode);
	// Null when game audio is off (-Audio=0): nothing to do then.
	UGTPSirenComponent::SetSiren(Unit.Car.Get(), Mode);
}

bool AGTPPoliceDirector::HasLineOfSight(const AGTPVehicle* Car, const AActor& Player, const FVector& Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceLOS), false, Car);
	Params.AddIgnoredActor(&Player);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Car->GetActorLocation() + FVector(0.f, 0.f, 150.f),
		Target + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params);
}

void AGTPPoliceDirector::DriveStraight(FUnit& Unit, const FVector& Target, float MaxSpeedKmh)
{
	AGTPVehicle* Car = Unit.Car.Get();
	const FVector Location = Car->GetActorLocation();
	const FVector Direction = (Target - Location).GetSafeNormal2D();
	std::vector<FGTPDrivePoint> Path;
	Path.push_back({static_cast<float>(Location.X) * WantedCmToM, static_cast<float>(Location.Y) * WantedCmToM});
	const FVector End = Target + Direction * 2000.f;
	Path.push_back({static_cast<float>(End.X) * WantedCmToM, static_cast<float>(End.Y) * WantedCmToM});
	int Closest = 0;
	const float Obstacle = UGTPAIDriverComponent::ProbeAhead(Car, 4000.f, 100.f);
	const float Speed = Car->GetSpeedKmh() / 3.6f;
	const FGTPDriveCommand Command = GTPDrive::Decide(Path, Path[0].X, Path[0].Y, FMath::DegreesToRadians(static_cast<float>(Car->GetActorRotation().Yaw)),
		Speed, MaxSpeedKmh / 3.6f, Obstacle, Unit.Settings, Closest);
	Car->SetAIDriveInput(Command.Throttle, Command.Brake, Command.Steering, Command.Brake >= 1.f && Speed < 0.5f);
	Unit.bOnLanes = false;
}

void AGTPPoliceDirector::DriveOnLanes(FUnit& Unit, const FVector& Goal, bool bHasGoal, float MaxSpeedKmh)
{
	AGTPVehicle* Car = Unit.Car.Get();
	const FVector Location = Car->GetActorLocation();
	const float Yaw = static_cast<float>(Car->GetActorRotation().Yaw);
	if (!Unit.bOnLanes || !Unit.Follower.IsActive())
	{
		Unit.Follower = FGTPLaneFollower();
		if (!Unit.Follower.StartNear(Network, Location, Yaw, 1500.f))
		{
			// Off the lanes (after a straight chase): hold still until the next try.
			Car->SetAIDriveInput(0.f, 1.f, 0.f, true);
			return;
		}
		Unit.bOnLanes = true;
	}
	if (bHasGoal)
	{
		Unit.Follower.SetGoal(Goal);
	}
	else
	{
		Unit.Follower.ClearGoal();
	}
	const float Obstacle = UGTPAIDriverComponent::ProbeAhead(Car, 4000.f, 90.f);
	FGTPDriveCommand Command;
	if (!Unit.Follower.Step(Location, Yaw, Car->GetSpeedKmh(), MaxSpeedKmh, Obstacle, Command))
	{
		Unit.bOnLanes = false;
		Car->SetAIDriveInput(0.f, 1.f, 0.f, true);
		return;
	}
	Car->SetAIDriveInput(Command.Throttle, Command.Brake, Command.Steering, Command.Brake >= 1.f && Car->GetSpeedKmh() < 2.f);
}

void AGTPPoliceDirector::DriveUnit(FUnit& Unit, UGTPWantedComponent& Wanted, const AActor& Player, float DeltaSeconds)
{
	AGTPVehicle* Car = Unit.Car.Get();
	const int32 Stars = Wanted.GetStars();
	const FVector Location = Car->GetActorLocation();
	Unit.StoppedTime = FMath::Abs(Car->GetSpeedKmh()) < 1.f ? Unit.StoppedTime + DeltaSeconds : 0.f;

	if (Stars == 0)
	{
		// Level lost or arrest: back to ordinary driving on the lanes.
		DriveOnLanes(Unit, FVector::ZeroVector, false, 40.f);
		return;
	}
	const float ChaseKmh = GTPWanted::ChaseSpeedKmh(Stars);
	const bool bPursuit = Wanted.GetPhase() == EGTPWantedPhase::Pursuit;
	const FVector Target = bPursuit ? Player.GetActorLocation() : Wanted.GetLastKnownLocation();
	const float Distance = static_cast<float>(FVector::Dist2D(Location, Target));
	if (bPursuit && HasLineOfSight(Car, Player, Target))
	{
		DriveStraight(Unit, Target, ChaseKmh);
	}
	else if (Distance > 2500.f)
	{
		// No way through the buildings: take the lanes, always towards the target.
		DriveOnLanes(Unit, Target, true, ChaseKmh * 0.8f);
	}
	else if (!bPursuit && Distance > 1200.f && HasLineOfSight(Car, Player, Target))
	{
		DriveStraight(Unit, Target, 40.f);
	}
	else
	{
		// Arrived at the search zone: roam the lanes there.
		DriveOnLanes(Unit, Target, true, 35.f);
	}
}

void AGTPPoliceDirector::RemoveUnit(int32 Index)
{
	FUnit& Unit = Units[Index];
	if (AGTPVehicle* Car = Unit.Car.Get())
	{
		if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
		{
			Wanted->UnregisterPolice(Car);
		}
		Network->Recycle(Car);
	}
	Units.RemoveAtSwap(Index);
}

void AGTPPoliceDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Network)
	{
		for (TActorIterator<AGTPTrafficNetwork> It(GetWorld()); It; ++It)
		{
			Network = *It;
			break;
		}
	}
	UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Network || !Wanted || !Player || MaxUnits == 0)
	{
		return;
	}
	Wanted->SetPoliceAvailable(Network->GetLanes().Num() > 0 && (LoadedPoliceClass.Get() != nullptr || Network->LoadedVehicleClasses.Num() > 0));

	const int32 Stars = Wanted->GetStars();
	const int32 Desired = GTPWanted::UnitsForStars(Stars, MaxUnits);
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	const bool bPhase = FMath::Fmod(Now, LightPeriod * 2.f) < LightPeriod;
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();

	for (int32 Index = Units.Num() - 1; Index >= 0; --Index)
	{
		FUnit& Unit = Units[Index];
		AGTPVehicle* Car = Unit.Car.Get();
		if (!IsValid(Car))
		{
			Units.RemoveAtSwap(Index);
			continue;
		}
		if (Unit.Stage == 1)
		{
			if (!SeatDriver(Unit))
			{
				Car->Destroy();
				Units.RemoveAtSwap(Index);
				continue;
			}
			Unit.Stage = 2;
			continue;
		}
		if (Unit.Stage == 2)
		{
			AddLightBar(Unit);
			Wanted->RegisterPolice(Car);
			Unit.Stage = 0;
			continue;
		}
		if (!Car->HasAIDriver() || Car->IsWrecked())
		{
			// Stolen or destroyed: no longer a unit (a wreck stays where it is).
			Wanted->UnregisterPolice(Car);
			BlinkLights(Unit, false, false);
			UpdateSiren(Unit, 0, false);
			Units.RemoveAtSwap(Index);
			continue;
		}
		Unit.IdleTime = Stars > 0 ? 0.f : Unit.IdleTime + DeltaSeconds;
		// A unit left far from a new chase is moved near the player (out of sight) instead of creating another.
		if (Stars > 0 && Index < Desired && Now >= Unit.NextRepositionTime
			&& FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) > MaxSpawnDistance * 1.5f
			&& !Network->IsSeenByPlayer(Car->GetActorLocation()))
		{
			Unit.NextRepositionTime = Now + 3.f;
			RepositionUnit(Unit, Player->GetActorLocation());
		}
		DriveUnit(Unit, *Wanted, *Player, DeltaSeconds);
		// With -VehicleLod=1 the light bar is dark beyond LightBarDistance: two lights nobody can tell apart from far away.
		const bool bLightsNear = !Network->bVehicleLod
			|| FVector::Dist(Car->GetActorLocation(), Player->GetActorLocation()) < LightBarDistance;
		BlinkLights(Unit, Stars > 0 && bLightsNear, bPhase);
		UpdateSiren(Unit, Stars, Wanted->GetPhase() == EGTPWantedPhase::Pursuit);

		const FVector View = Viewer && Viewer->PlayerCameraManager ? Viewer->PlayerCameraManager->GetCameraLocation() : Player->GetActorLocation();
		Network->UpdateDetail(Car, static_cast<float>(FVector::Dist(View, Car->GetActorLocation())));

		// Taken away out of sight: too many for the level, too far, stuck, or the level is gone.
		const bool bTooFar = FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) > LeashDistance;
		const bool bStuck = Unit.StoppedTime > 12.f && !(Stars > 0 && FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) < 1500.f);
		if ((bTooFar || bStuck || (Stars == 0 && Unit.IdleTime >= IdleLifetime)) && !Network->IsSeenByPlayer(Car->GetActorLocation()))
		{
			RemoveUnit(Index);
		}
	}

	if (Units.Num() < Desired && Now >= NextSpawnTime)
	{
		SpawnUnit(Player->GetActorLocation());
		NextSpawnTime = Now + SpawnInterval;
	}
}
