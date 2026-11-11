#include "GTPDemoRoute.h"

#include "ChaosVehicleMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GTPLog.h"
#include "GTPPlayerController.h"
#include "GTPVehicle.h"
#include "GTPVehicleInteractionComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPDemo, Log, All);

namespace
{
	// Durations in seconds, speeds in km/h.
	constexpr double LookAroundTime = 5.0;
	constexpr double WalkStreetTime = 14.0;
	constexpr double TurnBackTime = 2.0;
	constexpr double WalkToCarMaxTime = 40.0;
	constexpr double EnterMaxTime = 8.0;
	constexpr double CarjackMaxTime = 240.0;
	constexpr double CarjackRetargetTime = 10.0;
	constexpr float CarjackStoppedKmh = 5.f;
	constexpr float CarjackLeadCm = 900.f;
	constexpr double StopMaxTime = 8.0;
	constexpr double ExitMaxTime = 6.0;
	constexpr float CruiseKmh = 40.f;
	constexpr float DoorReachCm = 300.f;

	const TCHAR* PhaseName(int32 Phase)
	{
		static const TCHAR* Names[] = { TEXT("wait for pawn"), TEXT("look around"), TEXT("walk street"), TEXT("turn back"),
			TEXT("walk to car"), TEXT("enter"), TEXT("drive"), TEXT("stop"), TEXT("exit"), TEXT("done"),
			TEXT("carjack") };
		return (Phase >= 0 && Phase < static_cast<int32>(UE_ARRAY_COUNT(Names))) ? Names[Phase] : TEXT("?");
	}
}

void UGTPDemoRoute::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bCarjack = FParse::Param(FCommandLine::Get(), TEXT("DemoCarjack"));
	if (!bCarjack && !FParse::Param(FCommandLine::Get(), TEXT("DemoRoute")))
	{
		return;
	}
	FParse::Value(FCommandLine::Get(), TEXT("DemoDriveTime="), DriveTime);
	bToRiver = FParse::Param(FCommandLine::Get(), TEXT("DemoToRiver"));
	DriveTime = FMath::Clamp(DriveTime, 5.0, 3600.0);
	RouteStart = FPlatformTime::Seconds();
	PhaseStart = RouteStart;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPDemoRoute::Tick));
	GTP_TRACE(LogGTPDemo, TEXT("Demo route enabled (drive %.0f s%s)"), DriveTime, bCarjack ? TEXT(", carjack") : TEXT(""));
}

void UGTPDemoRoute::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	Super::Deinitialize();
}

APlayerController* UGTPDemoRoute::GetController() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}

void UGTPDemoRoute::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStart = FPlatformTime::Seconds();
	GTP_TRACE(LogGTPDemo, TEXT("Demo route %.1f s: %s"), static_cast<float>(PhaseStart - RouteStart), PhaseName(static_cast<int32>(NewPhase)));
}

AGTPVehicle* UGTPDemoRoute::FindCar() const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return nullptr;
	}
	AGTPVehicle* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AGTPVehicle> It(World); It; ++It)
	{
		if (It->IsWrecked() || It->GetDriver() != nullptr || It->HasAIDriver())
		{
			continue;
		}
		const float Dist = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = *It;
		}
	}
	return Best;
}

AGTPVehicle* UGTPDemoRoute::FindTrafficCar() const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return nullptr;
	}
	AGTPVehicle* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AGTPVehicle> It(World); It; ++It)
	{
		if (It->IsWrecked() || !It->HasAIDriver())
		{
			continue;
		}
		const float Dist = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = *It;
		}
	}
	return Best;
}

void UGTPDemoRoute::DriveInputs(AGTPVehicle* InCar, float Throttle, float Brake, float Steering, bool bHandbrake) const
{
	if (UChaosVehicleMovementComponent* Move = InCar ? InCar->GetVehicleMovementComponent() : nullptr)
	{
		Move->SetThrottleInput(Throttle);
		Move->SetBrakeInput(Brake);
		Move->SetSteeringInput(Steering);
		Move->SetHandbrakeInput(bHandbrake);
	}
}

bool UGTPDemoRoute::Tick(float DeltaTime)
{
	APlayerController* PC = GetController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const double Now = FPlatformTime::Seconds();
	const double T = Now - PhaseStart;

	if (!PC || !Pawn)
	{
		return true;
	}

	switch (Phase)
	{
	case EPhase::WaitForPawn:
		if (Cast<ACharacter>(Pawn))
		{
			StreetYaw = Pawn->GetActorRotation().Yaw;
			StartControlYaw = PC->GetControlRotation().Yaw;
			SetPhase(EPhase::LookAround);
		}
		break;

	case EPhase::LookAround:
	{
		// A slow pan left then right, back to the street axis.
		const float Pan = 60.f * static_cast<float>(FMath::Sin(2.0 * UE_DOUBLE_PI * T / LookAroundTime));
		PC->SetControlRotation(FRotator(-8.f, StartControlYaw + Pan, 0.f));
		if (T >= LookAroundTime)
		{
			SetPhase(bCarjack ? EPhase::Carjack : EPhase::WalkStreet);
		}
		break;
	}

	case EPhase::WalkStreet:
	{
		const float Sway = 12.f * static_cast<float>(FMath::Sin(2.0 * UE_DOUBLE_PI * T / 7.0));
		const FRotator View(-6.f, StreetYaw + Sway, 0.f);
		PC->SetControlRotation(View);
		Pawn->AddMovementInput(FRotator(0.f, StreetYaw, 0.f).Vector(), 1.f);
		if (T >= WalkStreetTime)
		{
			SetPhase(EPhase::TurnBack);
		}
		break;
	}

	case EPhase::TurnBack:
	{
		const float Alpha = FMath::Clamp(static_cast<float>(T / TurnBackTime), 0.f, 1.f);
		PC->SetControlRotation(FRotator(-6.f, StreetYaw + 180.f * FMath::SmoothStep(0.f, 1.f, Alpha), 0.f));
		if (T >= TurnBackTime)
		{
			Car = FindCar();
			if (!Car.IsValid())
			{
				UE_LOG(LogGTPDemo, Warning, TEXT("Demo route: no free car found (start with -TestVehicle); staying on foot"));
				SetPhase(EPhase::Done);
				break;
			}
			SetPhase(EPhase::WalkToCar);
		}
		break;
	}

	case EPhase::WalkToCar:
	{
		AGTPVehicle* Target = Car.Get();
		if (!Target)
		{
			SetPhase(EPhase::Done);
			break;
		}
		const FVector Door = Target->GetDoorLocation(EGTPVehicleSide::Left);
		FVector ToDoor = Door - Pawn->GetActorLocation();
		ToDoor.Z = 0.f;
		const FRotator Desired(-8.f, ToDoor.Rotation().Yaw, 0.f);
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Desired, DeltaTime, 3.f));
		if (ToDoor.Size() > DoorReachCm * 0.5f)
		{
			Pawn->AddMovementInput(ToDoor.GetSafeNormal(), 1.f);
		}
		const AGTPPlayerController* GTPController = Cast<AGTPPlayerController>(PC);
		UGTPVehicleInteractionComponent* Interaction = GTPController ? GTPController->GetVehicleInteraction() : nullptr;
		if (Interaction && ToDoor.Size() <= DoorReachCm && Interaction->FindBestVehicle() == Target)
		{
			Interaction->HandleEnterExit();
			SetPhase(EPhase::Enter);
		}
		else if (T >= WalkToCarMaxTime)
		{
			UE_LOG(LogGTPDemo, Warning, TEXT("Demo route: could not reach the car in %.0f s; staying on foot"), WalkToCarMaxTime);
			SetPhase(EPhase::Done);
		}
		break;
	}

	case EPhase::Carjack:
	{
		AGTPVehicle* Target = Car.Get();
		if (!Target || !Target->HasAIDriver() || Now - CarjackRetarget > CarjackRetargetTime)
		{
			AGTPVehicle* Nearest = FindTrafficCar();
			CarjackRetarget = Now;
			if (Nearest && Nearest != Target)
			{
				GTP_TRACE(LogGTPDemo, TEXT("Demo carjack: heading for %s, %.0f m away, %.0f km/h"), *Nearest->GetName(),
					FVector::Dist(Nearest->GetActorLocation(), Pawn->GetActorLocation()) / 100.f, Nearest->GetSpeedKmh());
			}
			Car = Nearest;
			Target = Nearest;
		}
		if (!Target)
		{
			if (T >= CarjackMaxTime)
			{
				GTP_TRACE(LogGTPDemo, TEXT("Demo carjack: no car with a driver in %.0f s; staying on foot"), CarjackMaxTime);
				SetPhase(EPhase::Done);
			}
			break;
		}
		// A moving car: stand on its way ahead of it, so that it stops for the
		// pedestrian. A stopped one: walk to the driver door.
		const float TargetSpeed = FMath::Abs(Target->GetSpeedKmh());
		const FVector Door = Target->GetDoorLocation(EGTPVehicleSide::Left);
		const FVector Goal = TargetSpeed > CarjackStoppedKmh
			? Target->GetActorLocation() + Target->GetActorForwardVector() * CarjackLeadCm
			: Door;
		FVector ToGoal = Goal - Pawn->GetActorLocation();
		ToGoal.Z = 0.f;
		const FRotator Desired(-8.f, ToGoal.Rotation().Yaw, 0.f);
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Desired, DeltaTime, 3.f));
		if (ToGoal.Size() > 60.f)
		{
			Pawn->AddMovementInput(ToGoal.GetSafeNormal(), 1.f);
		}
		FVector ToDoor = Door - Pawn->GetActorLocation();
		ToDoor.Z = 0.f;
		const AGTPPlayerController* GTPController = Cast<AGTPPlayerController>(PC);
		UGTPVehicleInteractionComponent* Interaction = GTPController ? GTPController->GetVehicleInteraction() : nullptr;
		if (Interaction && TargetSpeed <= CarjackStoppedKmh && ToDoor.Size() <= DoorReachCm && Interaction->FindBestVehicle() == Target)
		{
			GTP_TRACE(LogGTPDemo, TEXT("Demo carjack: %s stopped (%.1f km/h), at the driver door, pressing enter"), *Target->GetName(),
				TargetSpeed);
			Interaction->HandleEnterExit();
			SetPhase(EPhase::Enter);
		}
		else if (T >= CarjackMaxTime)
		{
			GTP_TRACE(LogGTPDemo, TEXT("Demo carjack: no car taken in %.0f s; staying on foot"), CarjackMaxTime);
			SetPhase(EPhase::Done);
		}
		break;
	}

	case EPhase::Enter:
		if (Pawn == Car.Get())
		{
			if (bCarjack)
			{
				GTP_TRACE(LogGTPDemo, TEXT("Demo carjack: driver pulled out, player at the wheel of %s, driving off"), *Pawn->GetName());
			}
			StreetYaw = Pawn->GetActorRotation().Yaw;
			SetPhase(EPhase::Drive);
		}
		else if (T >= EnterMaxTime)
		{
			UE_LOG(LogGTPDemo, Warning, TEXT("Demo route: entry did not finish in %.0f s"), EnterMaxTime);
			SetPhase(EPhase::Done);
		}
		break;

	case EPhase::Drive:
	{
		AGTPVehicle* Driven = Cast<AGTPVehicle>(Pawn);
		if (!Driven)
		{
			SetPhase(EPhase::Done);
			break;
		}
		// On a traffic lane: drive it like the AI drivers do (bends, junctions turned
		// at random, stop behind whatever is in front). The street of the start is
		// closed to cars and has no lane: hold its heading until a lane is near.
		if (!Follower.IsActive() && Now >= NextLaneSearch)
		{
			NextLaneSearch = Now + 1.0;
			TActorIterator<AGTPTrafficNetwork> Network(Driven->GetWorld());
			if (Network && Follower.StartNear(*Network, Driven->GetActorLocation(), static_cast<float>(Driven->GetActorRotation().Yaw), 900.f))
			{
				GTP_TRACE(LogGTPDemo, TEXT("Demo route: following the traffic lanes"));
				if (bToRiver && !bGoalSet)
				{
					// The Rhône is east of the Presqu'île: the lane point farthest east is on the bridge.
					for (const FGTPTrafficLane& Lane : Network->GetLanes())
					{
						for (const FVector& Point : Lane.Points)
						{
							if (!bGoalSet || Point.X > RiverGoal.X)
							{
								RiverGoal = Point;
								bGoalSet = true;
							}
						}
					}
					if (bGoalSet)
					{
						Follower.SetGoal(RiverGoal);
						GTP_TRACE(LogGTPDemo, TEXT("Demo route: heading for the river at %s"), *RiverGoal.ToCompactString());
					}
				}
			}
			else if (Network)
			{
				// No lane here (pedestrian street of the start): head for the nearest
				// one, aiming a little further along it so the car meets it at an angle.
				FVector Join;
				if (Network->FindJoinPoint(Driven->GetActorLocation(), 60000.f, 1500.f, Join))
				{
					const float JoinYaw = static_cast<float>((Join - Driven->GetActorLocation()).Rotation().Yaw);
					if (FMath::Abs(FRotator::NormalizeAxis(JoinYaw - StreetYaw)) > 2.f)
					{
						GTP_TRACE(LogGTPDemo, TEXT("Demo route: heading for the lane at %.0f m"), FVector::Dist2D(Join, Driven->GetActorLocation()) / 100.0);
					}
					StreetYaw = JoinYaw;
				}
			}
		}
		if (Follower.IsActive() && bGoalSet && bToRiver && FVector::Dist2D(Driven->GetActorLocation(), RiverGoal) < 3000.f)
		{
			// Across the bridge: roam from here (back over it at the next junctions).
			Follower.ClearGoal();
			bToRiver = false;
			GTP_TRACE(LogGTPDemo, TEXT("Demo route: reached the river, roaming"));
		}
		if (Follower.IsActive())
		{
			FGTPDriveCommand Command;
			if (Follower.Step(Driven->GetActorLocation(), static_cast<float>(Driven->GetActorRotation().Yaw), Driven->GetSpeedKmh(), CruiseKmh,
				UGTPAIDriverComponent::ProbeAhead(Driven, 4000.f, 90.f), Command))
			{
				DriveInputs(Driven, Command.Throttle, Command.Brake, Command.Steering, false);
				if (T >= DriveTime)
				{
					SetPhase(EPhase::Stop);
				}
				break;
			}
			GTP_TRACE(LogGTPDemo, TEXT("Demo route: end of the lanes, holding the heading"));
			StreetYaw = static_cast<float>(Driven->GetActorRotation().Yaw);
		}
		// Hold the street heading with a light steering correction and keep a
		// city speed; a gentle weave in the middle shows the handling.
		const float Weave = (T > 15.0 && T < 30.0) ? 10.f * static_cast<float>(FMath::Sin(2.0 * UE_DOUBLE_PI * (T - 15.0) / 7.5)) : 0.f;
		const float YawError = FRotator::NormalizeAxis(StreetYaw + Weave - Driven->GetActorRotation().Yaw);
		const float Steering = FMath::Clamp(YawError / 25.f, -1.f, 1.f);
		const float Speed = Driven->GetSpeedKmh();
		// Stuck against something (bench, bollard): back off for a moment with
		// the wheels turned the other way, then carry on.
		if (Now < UnstickUntil)
		{
			DriveInputs(Driven, 0.f, 0.6f, -UnstickSteer, false);
			break;
		}
		if (T > 4.0 && FMath::Abs(Speed) < 3.f)
		{
			if (StuckSince <= 0.0)
			{
				StuckSince = Now;
			}
			else if (Now - StuckSince > 1.5)
			{
				UnstickSteer = YawError >= 0.f ? 1.f : -1.f;
				UnstickUntil = Now + 2.0;
				StuckSince = 0.0;
				GTP_TRACE(LogGTPDemo, TEXT("Demo route: car stuck, backing off"));
				break;
			}
		}
		else
		{
			StuckSince = 0.0;
		}
		const float Throttle = Speed < CruiseKmh ? FMath::Clamp((CruiseKmh - Speed) / 15.f, 0.25f, 0.8f) : 0.f;
		DriveInputs(Driven, Throttle, 0.f, Steering, false);
		if (T >= DriveTime)
		{
			SetPhase(EPhase::Stop);
		}
		break;
	}

	case EPhase::Stop:
	{
		AGTPVehicle* Driven = Cast<AGTPVehicle>(Pawn);
		if (!Driven)
		{
			SetPhase(EPhase::Done);
			break;
		}
		DriveInputs(Driven, 0.f, 1.f, 0.f, false);
		if (FMath::Abs(Driven->GetSpeedKmh()) < 2.f || T >= StopMaxTime)
		{
			DriveInputs(Driven, 0.f, 0.f, 0.f, true);
			const AGTPPlayerController* GTPController = Cast<AGTPPlayerController>(PC);
			if (UGTPVehicleInteractionComponent* Interaction = GTPController ? GTPController->GetVehicleInteraction() : nullptr)
			{
				Interaction->HandleEnterExit();
			}
			SetPhase(EPhase::Exit);
		}
		break;
	}

	case EPhase::Exit:
		if (Cast<ACharacter>(Pawn))
		{
			GTP_TRACE(LogGTPDemo, TEXT("Demo route: back on foot"));
			SetPhase(EPhase::Done);
		}
		else if (T >= ExitMaxTime)
		{
			UE_LOG(LogGTPDemo, Warning, TEXT("Demo route: exit did not finish in %.0f s"), ExitMaxTime);
			SetPhase(EPhase::Done);
		}
		break;

	case EPhase::Done:
		TickHandle.Reset();
		return false;
	}
	return true;
}
