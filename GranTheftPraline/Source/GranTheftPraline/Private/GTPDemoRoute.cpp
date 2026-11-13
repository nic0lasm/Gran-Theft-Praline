#include "GTPDemoRoute.h"

#include "ChaosVehicleMovementComponent.h"
#include "Engine/GameInstance.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GTPDeterminism.h"
#include "GTPLog.h"
#include "GTPPlayerController.h"
#include "GTPVehicle.h"
#include "GTPVehicleInteractionComponent.h"
#include "GTPWanted.h"
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
	/** Deterministic runs: decision step and the interval of the position log (simulated seconds). */
	constexpr double FixedStep = 1.0 / 30.0;
	constexpr double PosLogInterval = 5.0;
	constexpr int32 MaxStepsPerFrame = 6;

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
	bChase = FParse::Param(FCommandLine::Get(), TEXT("PoliceChaseDemo"));
	bCarjack = FParse::Param(FCommandLine::Get(), TEXT("DemoCarjack")) || bChase;
	if (!bCarjack && !FParse::Param(FCommandLine::Get(), TEXT("DemoRoute")))
	{
		return;
	}
	bDeterministic = GTPDeterminism::IsOn();
	{
		double WalkCm = WalkStreetCm;
		if (FParse::Value(FCommandLine::Get(), TEXT("DemoWalkCm="), WalkCm))
		{
			WalkStreetCm = static_cast<float>(FMath::Clamp(WalkCm, 500.0, 50000.0));
		}
	}
	if (bChase)
	{
		DriveTime = 150.0;
	}
	FParse::Value(FCommandLine::Get(), TEXT("DemoDriveTime="), DriveTime);
	bToRiver = FParse::Param(FCommandLine::Get(), TEXT("DemoToRiver"));
	DriveTime = FMath::Clamp(DriveTime, 5.0, 3600.0);
	RouteStart = Clock();
	PhaseStart = RouteStart;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPDemoRoute::Tick));
	GTP_TRACE(LogGTPDemo, TEXT("Demo route enabled (drive %.0f s%s%s)"), DriveTime, bCarjack ? TEXT(", carjack") : TEXT(""), bDeterministic ? TEXT(", deterministic") : TEXT(""));
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

double UGTPDemoRoute::Clock() const
{
	return bDeterministic ? SimTime : FPlatformTime::Seconds();
}

void UGTPDemoRoute::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStart = Clock();
	if (NewPhase == EPhase::WalkStreet)
	{
		const APlayerController* PC = GetController();
		if (const APawn* Pawn = PC ? PC->GetPawn() : nullptr)
		{
			WalkOrigin = Pawn->GetActorLocation();
		}
	}
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
	if (!bDeterministic)
	{
		return RunStep(DeltaTime);
	}
	// Deterministic: the route is advanced by simulated game time in fixed steps, so a slow
	// frame or a different frame rate changes how many steps run, never what they decide.
	const APlayerController* PC = GetController();
	const UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!World)
	{
		return true;
	}
	const double WorldTime = World->GetTimeSeconds();
	if (LastWorldTime < 0.0)
	{
		LastWorldTime = WorldTime;
	}
	Accumulator += FMath::Max(0.0, WorldTime - LastWorldTime);
	LastWorldTime = WorldTime;
	bool bKeepGoing = true;
	for (int32 Steps = 0; Accumulator >= FixedStep && Steps < MaxStepsPerFrame && bKeepGoing; ++Steps)
	{
		Accumulator -= FixedStep;
		SimTime += FixedStep;
		bKeepGoing = RunStep(static_cast<float>(FixedStep));
	}
	if (Accumulator > FixedStep * MaxStepsPerFrame)
	{
		Accumulator = 0.0; // after a long hitch, do not replay it
	}
	// Movement input is consumed by the character every frame: hold the last decision.
	if (bKeepGoing && !HeldMove.IsNearlyZero())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->AddMovementInput(HeldMove, 1.f);
		}
	}
	return bKeepGoing;
}

bool UGTPDemoRoute::RunStep(float DeltaTime)
{
	APlayerController* PC = GetController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const double Now = Clock();
	HeldMove = FVector::ZeroVector;
	const double T = Now - PhaseStart;

	if (!PC || !Pawn)
	{
		return true;
	}

	// Fixed instants, comparable between two runs: time, position, heading, speed.
	if (bDeterministic && Phase != EPhase::WaitForPawn)
	{
		const int32 Slot = static_cast<int32>((Now - RouteStart) / PosLogInterval);
		if (Slot > LastLoggedSlot)
		{
			LastLoggedSlot = Slot;
			const FVector Location = Pawn->GetActorLocation();
			const AGTPVehicle* Driven = Cast<AGTPVehicle>(Pawn);
			GTP_TRACE(LogGTPDemo, TEXT("DemoPos t=%.1f x=%.0f y=%.0f yaw=%.0f kmh=%.0f phase=%s"), Slot * PosLogInterval, Location.X, Location.Y,
				FRotator::NormalizeAxis(Pawn->GetActorRotation().Yaw), Driven ? Driven->GetSpeedKmh() : 0.f, PhaseName(static_cast<int32>(Phase)));
		}
	}

	switch (Phase)
	{
	case EPhase::WaitForPawn:
		if (Cast<ACharacter>(Pawn))
		{
			if (bDeterministic)
			{
				// The route's own time starts here, whatever the level took to load.
				RouteStart = Now;
				LastLoggedSlot = -1;
			}
			if (bChase)
			{
				// The rue de la République is a pedestrian street with no lane: start on the quai Jean Moulin
				// (four lanes, straight for 220 m, heading north). -PoliceChaseStart=X,Y overrides it.
				FVector2D Start(38879.0, -4528.0);
				FString StartText;
				if (FParse::Value(FCommandLine::Get(), TEXT("PoliceChaseStart="), StartText))
				{
					FString Left, Right;
					if (StartText.Split(TEXT(","), &Left, &Right))
					{
						Start = FVector2D(FCString::Atod(*Left), FCString::Atod(*Right));
					}
				}
				FHitResult Ground;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPChaseStart), false, Pawn);
				const FVector Above(Start.X, Start.Y, Pawn->GetActorLocation().Z + 3000.0);
				const bool bFound = Pawn->GetWorld()->LineTraceSingleByChannel(Ground, Above, Above - FVector(0.0, 0.0, 8000.0), ECC_Visibility, Params);
				const FVector Spot(Start.X, Start.Y, bFound ? Ground.ImpactPoint.Z + 100.0 : Pawn->GetActorLocation().Z);
				Pawn->SetActorLocationAndRotation(Spot, FRotator(0.f, -93.f, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
				PC->SetControlRotation(FRotator(0.f, -93.f, 0.f));
				GTP_TRACE(LogGTPDemo, TEXT("Demo chase: start on the quai Jean Moulin at %.0f, %.0f (ground %s)"), Start.X, Start.Y, bFound ? TEXT("found") : TEXT("not found"));
			}
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
			if (bChase)
			{
				// No hunt for a moving car: the free test car (-TestVehicle) is brought to the player's side.
				Car = FindCar();
				if (AGTPVehicle* Free = Car.Get())
				{
					const FVector Forward = FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f).Vector();
					FHitResult Ground;
					FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPChaseCar), false, Free);
					Params.AddIgnoredActor(Pawn);
					const FVector Above = Pawn->GetActorLocation() + Forward * 500.f + FVector(0.f, 0.f, 500.f);
					const bool bFound = Pawn->GetWorld()->LineTraceSingleByChannel(Ground, Above, Above - FVector(0.f, 0.f, 3000.f), ECC_Visibility, Params);
					const FVector Spot = bFound ? FVector(Above.X, Above.Y, Ground.ImpactPoint.Z + 60.f) : Above;
					Free->SetActorLocationAndRotation(Spot, FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
					GTP_TRACE(LogGTPDemo, TEXT("Demo chase: the free car %s is put 5 m ahead of the player"), *Free->GetName());
					SetPhase(EPhase::WalkToCar);
					break;
				}
			}
			SetPhase(bCarjack ? EPhase::Carjack : EPhase::WalkStreet);
		}
		break;
	}

	case EPhase::WalkStreet:
	{
		const float Sway = 12.f * static_cast<float>(FMath::Sin(2.0 * UE_DOUBLE_PI * T / 7.0));
		const FRotator View(-6.f, StreetYaw + Sway, 0.f);
		PC->SetControlRotation(View);
		if (bDeterministic)
		{
			HeldMove = FRotator(0.f, StreetYaw, 0.f).Vector();
		}
		else
		{
			Pawn->AddMovementInput(FRotator(0.f, StreetYaw, 0.f).Vector(), 1.f);
		}
		// Waypoint: a set distance from where the walk began (a duration in a normal run).
		const bool bArrived = bDeterministic ? FVector::Dist2D(Pawn->GetActorLocation(), WalkOrigin) >= WalkStreetCm || T >= 2.0 * WalkStreetTime
			: T >= WalkStreetTime;
		if (bArrived)
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
			if (bDeterministic)
			{
				HeldMove = ToDoor.GetSafeNormal();
			}
			else
			{
				Pawn->AddMovementInput(ToDoor.GetSafeNormal(), 1.f);
			}
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
			if (bDeterministic)
			{
				HeldMove = ToGoal.GetSafeNormal();
			}
			else
			{
				Pawn->AddMovementInput(ToGoal.GetSafeNormal(), 1.f);
			}
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
			if (bChase)
			{
				if (UGTPWantedComponent* Wanted = PC->FindComponentByClass<UGTPWantedComponent>())
				{
					Wanted->ReportCrime(EGTPCrimeKind::CarTheft, Pawn->GetActorLocation());
				}
			}
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
		float Cruise = CruiseKmh;
		if (bChase)
		{
			Cruise = T >= 60.0 ? 70.f : 40.f;
			// The way ahead is kept clear of the city's traffic (a demo cannot queue behind a taxi): traffic cars
			// within 35 m in front of the player are taken off the road twice a second.
			if (Now >= NextClear)
			{
				NextClear = Now + 0.5;
				TActorIterator<AGTPTrafficNetwork> Network(Driven->GetWorld());
				for (TActorIterator<AGTPVehicle> It(Driven->GetWorld()); Network && It; ++It)
				{
					AGTPVehicle* Other = *It;
					if (Other == Driven || !Other->HasAIDriver() || Other->ActorHasTag(TEXT("GTPPolice")))
					{
						continue;
					}
					const FVector Offset = Other->GetActorLocation() - Driven->GetActorLocation();
					if (Offset.Size2D() < 3500.f && FVector::DotProduct(Offset.GetSafeNormal2D(), Driven->GetActorForwardVector().GetSafeNormal2D()) > 0.5f)
					{
						Network->Recycle(Other);
					}
				}
			}
			UGTPWantedComponent* Wanted = PC->FindComponentByClass<UGTPWantedComponent>();
			if (Wanted)
			{
				// Nobody can arrest the player before the flight starts: the chase must show first.
				Wanted->SetArrestEnabled(T >= 100.0);
				if (T >= 8.0 && T < 60.0)
				{
					Wanted->EnsureMinStars(T >= 30.0 ? 3 : 2);
				}
				if (Now >= NextChaseLog)
				{
					NextChaseLog = Now + 5.0;
					GTP_TRACE(LogGTPDemo, TEXT("DemoChase t=%.0f stars=%d phase=%d seen=%d kmh=%.0f"), static_cast<float>(T), Wanted->GetStars(),
						Wanted->GetPhaseIndex(), Wanted->IsSeenNow() ? 1 : 0, Driven->GetSpeedKmh());
				}
			}
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
			Follower.bIgnoreLaneLimit = bChase;
			// Held up for more than 2.5 s (a car in a narrow street, a corner cut too short): first push through for
			// 3 s, then, if it happens again, back off for 2 s with the wheels turned and go on.
			if (bChase && T > 4.0)
			{
				if (Now < UnstickUntil)
				{
					DriveInputs(Driven, 0.f, 1.f, -UnstickSteer, false);
					break;
				}
				if (FMath::Abs(Driven->GetSpeedKmh()) < 3.f)
				{
					SlowSince = SlowSince <= 0.0 ? Now : SlowSince;
					if (Now - SlowSince > 2.0 && Now >= RamUntil)
					{
						SlowSince = 0.0;
						// The demo must not wait on the city's traffic: cars of the traffic within 25 m are taken off the road.
						{
							TActorIterator<AGTPTrafficNetwork> Network(Driven->GetWorld());
							int32 Cleared = 0;
							for (TActorIterator<AGTPVehicle> It(Driven->GetWorld()); Network && It; ++It)
							{
								AGTPVehicle* Other = *It;
								if (Other != Driven && Other->HasAIDriver() && !Other->ActorHasTag(TEXT("GTPPolice"))
									&& FVector::Dist(Other->GetActorLocation(), Driven->GetActorLocation()) < 2500.f)
								{
									Network->Recycle(Other);
									++Cleared;
								}
							}
							if (Cleared > 0)
							{
								GTP_TRACE(LogGTPDemo, TEXT("Demo chase: %d traffic car(s) taken off the road around the player"), Cleared);
							}
						}
						if (++ChaseStuckCount % 2 == 1)
						{
							RamUntil = Now + 3.0;
							GTP_TRACE(LogGTPDemo, TEXT("Demo chase: held up, pushing through"));
						}
						else
						{
							UnstickSteer = (ChaseStuckCount / 2) % 2 == 0 ? 1.f : -1.f;
							UnstickUntil = Now + 2.0;
							GTP_TRACE(LogGTPDemo, TEXT("Demo chase: held up, backing off"));
							break;
						}
					}
				}
				else
				{
					SlowSince = 0.0;
				}
			}
			if (Follower.Step(Driven->GetActorLocation(), static_cast<float>(Driven->GetActorRotation().Yaw), Driven->GetSpeedKmh(), Cruise,
				Now < RamUntil ? 1000.f : (bChase ? UGTPAIDriverComponent::ProbeAhead(Driven, 900.f, 60.f) : UGTPAIDriverComponent::ProbeAhead(Driven, 4000.f, 90.f)), Command))
			{
				if (bChase && Command.Brake < 0.05f && Driven->GetSpeedKmh() < Cruise - 4.f)
				{
					// The lane follower's throttle is soft (half of the time under 0.3 below the set speed): firmer for the chase.
					Command.Throttle = FMath::Max(Command.Throttle, 0.7f);
				}
				DriveInputs(Driven, Command.Throttle, Command.Brake, Command.Steering, false);
				if (bChase && Now >= NextDriveDiag)
				{
					NextDriveDiag = Now + 1.0;
					GTP_TRACE(LogGTPDemo, TEXT("DemoDrive t=%.0f kmh=%.0f lane=%d throttle=%.2f brake=%.2f steer=%.2f cruise=%.0f"), static_cast<float>(T),
						Driven->GetSpeedKmh(), Follower.GetLane(), Command.Throttle, Command.Brake, Command.Steering, Cruise);
				}
				if (bDeterministic && Follower.GetLane() != LastLoggedLane)
				{
					// The route is the lane sequence: two runs took the same way when these lines match.
					LastLoggedLane = Follower.GetLane();
					GTP_TRACE(LogGTPDemo, TEXT("DemoLane t=%.1f lane=%d"), static_cast<float>(Now - RouteStart), LastLoggedLane);
				}
				if (T >= DriveTime)
				{
					SetPhase(EPhase::Stop);
				}
				break;
			}
			if (bChase && Now >= NextRelocate)
			{
				// The lanes of the quarter end here (a dead end of the map): the chase goes on from a lane
				// 80 to 150 m away, the police told by radio where the player is.
				NextRelocate = Now + 2.0;
				TActorIterator<AGTPTrafficNetwork> Network(Driven->GetWorld());
				if (Network)
				{
					const TArray<FGTPTrafficLane>& Lanes = Network->GetLanes();
					FRandomStream Dice(static_cast<int32>(Now * 10.0));
					for (int32 Try = 0; Try < 300 && Lanes.Num() > 0; ++Try)
					{
						const FGTPTrafficLane& Lane = Lanes[Dice.RandRange(0, Lanes.Num() - 1)];
						if (Lane.Points.Num() < 10 || Lane.Next.Num() == 0)
						{
							continue;
						}
						const int32 Index = Lane.Points.Num() / 3;
						const FVector Point = Lane.Points[Index];
						const double Distance = FVector::Dist2D(Point, Driven->GetActorLocation());
						if (Distance < 8000.0 || Distance > 15000.0)
						{
							continue;
						}
						FHitResult Ground;
						FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPChaseRelocate), false, Driven);
						if (!Driven->GetWorld()->LineTraceSingleByChannel(Ground, Point + FVector(0.f, 0.f, 500.f), Point - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
						{
							continue;
						}
						const FVector Direction = (Lane.Points[Index + 1] - Point).GetSafeNormal2D();
						Driven->SetActorLocationAndRotation(FVector(Point.X, Point.Y, Ground.ImpactPoint.Z + 60.f), Direction.Rotation(), false, nullptr, ETeleportType::ResetPhysics);
						if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Driven->GetRootComponent()))
						{
							Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
							Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
						}
						Follower = FGTPLaneFollower();
						Follower.StartNear(*Network, Driven->GetActorLocation(), static_cast<float>(Driven->GetActorRotation().Yaw), 900.f);
						if (UGTPWantedComponent* Wanted = PC->FindComponentByClass<UGTPWantedComponent>())
						{
							Wanted->CallPolice();
						}
						GTP_TRACE(LogGTPDemo, TEXT("Demo chase: lanes ended, the car is put on another lane %.0f m away (police told)"), Distance / 100.0);
						break;
					}
				}
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
		const float Throttle = Speed < Cruise ? FMath::Clamp((Cruise - Speed) / 15.f, 0.25f, 0.8f) : 0.f;
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
