#include "GTPDemoRoute.h"

#include "ChaosVehicleMovementComponent.h"
#include "Engine/GameInstance.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GTPDeterminism.h"
#include "GTPFarTraffic.h"
#include "GTPLeftCars.h"
#include "GTPFlightCircuit.h"
#include "GTPFlightPedals.h"
#include "GTPLog.h"
#include "GTPPedestrians.h"
#include "GTPPlayerController.h"
#include "GTPRiverRecovery.h"
#include "GTPRoadWidth.h"
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

	const TCHAR* LimitName(int32 Limit)
	{
		static const TCHAR* Names[] = { TEXT("lane"), TEXT("bend"), TEXT("follow"), TEXT("obstacle"), TEXT("pathend") };
		return (Limit >= 0 && Limit < 5) ? Names[Limit] : TEXT("?");
	}

	/**
	 * Who is in the car, said in the log after a relocation. Gate 88's long take, 87 to 92 s: put on a lane, the car
	 * stood on its tail 0.6 s after its physics came back and lay on its side for 4 s, a man standing upright where it
	 * had been put (gate 86: "on its nose, a passer-by under it"). The pictures do not say who he is. Named here: the
	 * car's driver and what hangs from him, every character within 5 m (with or without collision), and every body in
	 * the car's box that a car collides with.
	 */
	void DemoWhoIsInTheCar(const AGTPVehicle* Car, const TCHAR* When, bool bWithTheTown)
	{
		UWorld* World = Car ? Car->GetWorld() : nullptr;
		if (!World)
		{
			return;
		}
		const FTransform CarFrame = Car->GetActorTransform();
		auto Told = [&CarFrame](const AActor* Who)
		{
			const FVector In = CarFrame.InverseTransformPosition(Who->GetActorLocation());
			const ACharacter* Body = Cast<ACharacter>(Who);
			const UCapsuleComponent* Capsule = Body ? Body->GetCapsuleComponent() : nullptr;
			const UCharacterMovementComponent* Legs = Body ? Body->GetCharacterMovement() : nullptr;
			const USkeletalMeshComponent* Skin = Body ? Body->GetMesh() : nullptr;
			return FString::Printf(TEXT("%s (%s) at %.0f %.0f %.0f of the car, collision %d, capsule against a car %d, hidden %d, attached to %s, movement mode %d, mesh simulating %d, mesh %.0f cm from its actor"),
				*Who->GetName(), *Who->GetClass()->GetName(), In.X, In.Y, In.Z, Who->GetActorEnableCollision() ? 1 : 0,
				Capsule ? static_cast<int32>(Capsule->GetCollisionResponseToChannel(ECC_Vehicle)) : -1, Who->IsHidden() ? 1 : 0,
				Who->GetAttachParentActor() ? *Who->GetAttachParentActor()->GetName() : TEXT("nothing"),
				Legs ? static_cast<int32>(Legs->MovementMode.GetValue()) : -1, Skin && Skin->IsSimulatingPhysics() ? 1 : 0,
				Skin ? FVector::Dist(Skin->GetComponentLocation(), Who->GetActorLocation()) : 0.0);
		};
		GTP_TRACE(LogGTPDemo, TEXT("Demo chase, who is in the car (%s): the car at %s, up %.2f, %.0f km/h"), When, *Car->GetActorLocation().ToCompactString(),
			Car->GetActorUpVector().Z, Car->GetSpeedKmh());
		if (const ACharacter* Seated = Car->GetDriver())
		{
			GTP_TRACE(LogGTPDemo, TEXT("Demo chase, who is in the car: its driver %s"), *Told(Seated));
			TArray<AActor*> Hanging;
			Seated->GetAttachedActors(Hanging, true, true);
			for (const AActor* Carried : Hanging)
			{
				if (Carried)
				{
					GTP_TRACE(LogGTPDemo, TEXT("Demo chase, who is in the car: hanging from the driver, %s"), *Told(Carried));
				}
			}
		}
		else
		{
			GTP_TRACE(LogGTPDemo, TEXT("Demo chase, who is in the car: it has no driver"));
		}
		int32 Near = 0;
		for (TActorIterator<APawn> It(World); It && Near < 8; ++It)
		{
			if (*It != Car && *It != Car->GetDriver() && FVector::Dist(It->GetActorLocation(), Car->GetActorLocation()) < 500.0)
			{
				++Near;
				GTP_TRACE(LogGTPDemo, TEXT("Demo chase, who is in the car: within 5 m, %s"), *Told(*It));
			}
		}
		// The people of the crowd are plain actors, without collision while they stand or walk.
		int32 People = 0;
		for (TActorIterator<AGTPPedestrian> It(World); It && People < 8; ++It)
		{
			if (!It->IsHidden() && FVector::Dist(It->GetActorLocation(), Car->GetActorLocation()) < 500.0)
			{
				++People;
				GTP_TRACE(LogGTPDemo, TEXT("Demo chase, who is in the car: of the crowd within 5 m, %s"), *Told(*It));
			}
		}
		// Every car within 8 m, whatever it is: a far traffic puppet is drawn without collision, and no overlap finds it.
		int32 CarsNear = 0;
		for (TActorIterator<AGTPVehicle> It(World); It && CarsNear < 6; ++It)
		{
			if (*It != Car && FVector::Dist(It->GetActorLocation(), Car->GetActorLocation()) < 800.0)
			{
				++CarsNear;
				const FVector In = CarFrame.InverseTransformPosition(It->GetActorLocation());
				const UPrimitiveComponent* Shell = Cast<UPrimitiveComponent>(It->GetRootComponent());
				GTP_TRACE(LogGTPDemo, TEXT("Demo chase, who is in the car: a car within 8 m, %s at %.0f %.0f %.0f of the car, hidden %d, collision %d, simulating %d, AI driver %d, %.0f km/h"),
					*It->GetName(), In.X, In.Y, In.Z, It->IsHidden() ? 1 : 0, It->GetActorEnableCollision() ? 1 : 0,
					Shell && Shell->IsSimulatingPhysics() ? 1 : 0, It->HasAIDriver() ? 1 : 0, It->GetSpeedKmh());
			}
		}
		TArray<FOverlapResult> Inside;
		FCollisionObjectQueryParams Kinds;
		Kinds.AddObjectTypesToQuery(ECC_Pawn);
		Kinds.AddObjectTypesToQuery(ECC_PhysicsBody);
		Kinds.AddObjectTypesToQuery(ECC_Vehicle);
		Kinds.AddObjectTypesToQuery(ECC_WorldDynamic);
		if (bWithTheTown)
		{
			// Upright and just put down, the box is clear of the road: what the town has in it is in the car.
			Kinds.AddObjectTypesToQuery(ECC_WorldStatic);
		}
		FCollisionQueryParams NotTheCar(SCENE_QUERY_STAT(GTPDemoInCar), false, Car);
		World->OverlapMultiByObjectType(Inside, Car->GetActorLocation() + Car->GetActorUpVector() * 95.f, Car->GetActorQuat(), Kinds,
			FCollisionShape::MakeBox(FVector(250.f, 110.f, 60.f)), NotTheCar);
		TSet<const UPrimitiveComponent*> Parts;
		for (const FOverlapResult& Touch : Inside)
		{
			const UPrimitiveComponent* Part = Touch.GetComponent();
			if (!Part || !Touch.GetActor() || Parts.Contains(Part) || Parts.Num() >= 8)
			{
				continue;
			}
			Parts.Add(Part);
			GTP_TRACE(LogGTPDemo, TEXT("Demo chase, who is in the car: in its box, %s of %s (%s), object type %d, against a car %d, simulating %d, its actor attached to %s"),
				*Part->GetName(), *Touch.GetActor()->GetName(), *Touch.GetActor()->GetClass()->GetName(), static_cast<int32>(Part->GetCollisionObjectType()),
				static_cast<int32>(Part->GetCollisionResponseToChannel(ECC_Vehicle)), Part->IsSimulatingPhysics() ? 1 : 0,
				Touch.GetActor()->GetAttachParentActor() ? *Touch.GetActor()->GetAttachParentActor()->GetName() : TEXT("nothing"));
		}
		GTP_TRACE(LogGTPDemo, TEXT("Demo chase, who is in the car: %d pawn(s) and %d of the crowd within 5 m, %d car(s) within 8 m, %d body(ies) in its box (35 to 155 cm over its origin%s)"),
			Near, People, CarsNear, Parts.Num(), bWithTheTown ? TEXT(", the town's included") : TEXT(""));
	}

	/**
	 * The far traffic cars within 15 m of the place the player's car is put on start again elsewhere. They are drawn
	 * and have no collision: the overlap tests of the relocation do not see one standing there, and the far traffic
	 * hands it over as a driven car, with its collision, once the player is that close.
	 */
	int32 DemoSendFarCarsAway(UWorld* InWorld, const FVector& PutThere)
	{
		int32 FarSent = 0;
		for (TActorIterator<AGTPFarTraffic> Far(InWorld); Far; ++Far)
		{
			FarSent += Far->SendAway(PutThere, 1500.f);
		}
		return FarSent;
	}

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

void UGTPDemoRoute::StartEncore()
{
	APlayerController* PC = GetController();
	if (AGTPVehicle* Driven = PC ? Cast<AGTPVehicle>(PC->GetPawn()) : nullptr)
	{
		// WaitForPawn waits for the player on foot before putting him back at the start.
		Driven->RequestExit();
	}
	if (UPrimitiveComponent* Body = bRelocateHeld && RelocatedCar.IsValid() ? Cast<UPrimitiveComponent>(RelocatedCar->GetRootComponent()) : nullptr)
	{
		Body->SetSimulatePhysics(true);
	}
	bCarjack = true;
	bChase = true;
	bLevelFree = false;
	bGaveUp = false;
	Car.Reset();
	HeldMove = FVector::ZeroVector;
	StuckSince = 0.0;
	UnstickUntil = 0.0;
	UnstickSteer = 0.f;
	RamUntil = 0.0;
	SlowSince = 0.0;
	ChaseStuckCount = 0;
	HeldUpAt = FVector::ZeroVector;
	HeldUpHere = 0;
	bRelocateHeld = false;
	RelocatedCar.Reset();
	RelocateHoldUntil = 0.0;
	bRelocationWatched = false;
	bRelocateBlockedTold = false;
	RelocateFarSent = 0;
	OffLaneSince = 0.0;
	bOnLaneOnce = false;
	NextDriveDiag = 0.0;
	PeakKmh = 0.f;
	NextRelocate = 0.0;
	NextClear = 0.0;
	NextChaseLog = 0.0;
	for (float& Seconds : LimitSeconds)
	{
		Seconds = 0.f;
	}
	NextLimitLog = 0.0;
	LastLimitTime = 0.0;
	CarjackRetarget = 0.0;
	Follower = FGTPLaneFollower();
	NextLaneSearch = 0.0;
	LastLoggedSlot = -1;
	DriveTime = 150.0;
	FParse::Value(FCommandLine::Get(), TEXT("DemoDriveTime="), DriveTime);
	DriveTime = FMath::Clamp(DriveTime, 5.0, 3600.0);
	RouteStart = Clock();
	SetPhase(EPhase::WaitForPawn);
	if (!TickHandle.IsValid())
	{
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPDemoRoute::Tick));
	}
	GTP_TRACE(LogGTPDemo, TEXT("Demo chase: encore, a second theft and flight from the start (%.0f s of driving)"), DriveTime);
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
		if (It->IsWrecked() || It->GetDriver() != nullptr || It->HasAIDriver() || It->IsShownByMassTraffic())
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

void UGTPDemoRoute::DriveInputs(AGTPVehicle* InCar, float Throttle, float Brake, float Steering, bool bHandbrake, bool bReverse) const
{
	if (UChaosVehicleMovementComponent* Move = InCar ? InCar->GetVehicleMovementComponent() : nullptr)
	{
		if (bChase)
		{
			// The chase script's pedals are a driver's: the brake brakes, by the amount asked. The player's car has
			// Chaos's reverse-as-brake on (AGTPVehicle::PossessedBy), made for a key held down: a dab of the brake at
			// speed then goes through the reverse gear and back to first. Same rule as AGTPVehicle::SetAIDriveInput:
			// reverse only when the script asks for it (backing off), and out of reverse as soon as it does not.
			Move->bReverseAsBrake = bReverse;
			if (!bReverse && Move->GetCurrentGear() < 0)
			{
				Move->SetTargetGear(Throttle > 0.05f ? 1 : 0, true);
			}
			else if (!bReverse && Throttle > 0.05f && Brake < 0.05f && Move->GetCurrentGear() == 0 && FMath::Abs(InCar->GetSpeedKmh()) < 1.f)
			{
				// Pulling away from a stop in neutral (never at speed: the gearbox passes through neutral at each shift).
				Move->SetTargetGear(1, true);
			}
		}
		Move->SetThrottleInput(Throttle);
		Move->SetBrakeInput(Brake);
		Move->SetSteeringInput(Steering);
		Move->SetHandbrakeInput(bHandbrake);
		// The script's steering stands: without this the pull of the car's dents replaced it (AGTPVehicle::UpdateDriveAssist).
		InCar->NoteScriptedSteering();
	}
}

bool UGTPDemoRoute::RelocateChase(AGTPVehicle* Driven, APlayerController* PC, const TCHAR* Why)
{
	UWorld* World = Driven ? Driven->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	TActorIterator<AGTPTrafficNetwork> Network(World);
	if (!Network)
	{
		return false;
	}
	const TArray<FGTPTrafficLane>& Lanes = Network->GetLanes();
	const UGTPRiverRecovery* River = World->GetSubsystem<UGTPRiverRecovery>();
	FRandomStream Dice(static_cast<int32>(Clock() * 10.0));
	// The police cars, roadblock cars included: the car is never put within 60 m of one. At the gate of candidate 48
	// it was put 8 m from one and arrested there 4 s later: an arrest made by the script, not by the police.
	TArray<FVector, TInlineAllocator<16>> PoliceAt;
	for (TActorIterator<AGTPVehicle> It(World); It; ++It)
	{
		if (It->ActorHasTag(GTPPolice::PoliceTag))
		{
			PoliceAt.Add(It->GetActorLocation());
		}
	}
	// Pass after pass (GTPRoadWidth::RelocationFits): a main axis that never ends first, then a middle street that never
	// ends, any lane that never ends, a lane with open road behind it (FGTPLaneFollower::OpenRoadMetres), any lane that
	// goes on. Until gate 75 the first pass took any lane with 250 m of open road: at 29 s of its film, a narrow street
	// where the car was wedged between two walls 10 s later. Never a lane a fixed obstacle blocks, and but for the last
	// pass never one driven lately (gate 75's film: put back twice on the way to the white block of the rue de la Bourse).
	// With a flight circuit (GTPFlightCircuit.h) the car is only ever put on a lane of the lap, 60 to 250 m away, and one
	// held up lately (bBlocked) only at the last pass. It is still a relocation: counted, and the film is not a valid one.
	const std::vector<int>& LapLanes = Network->GetFlightCircuit().Lanes;
	const bool bOnLap = !LapLanes.empty();
	// Places passed over because something stood there (said in the line of the relocation).
	int32 SpotsTaken = 0;
	for (int32 Pass = 0; Pass < GTPRoadWidth::RelocationPasses; ++Pass)
	{
		for (int32 Try = 0; Try < 300 && Lanes.Num() > 0; ++Try)
		{
			const int32 LaneIndex = bOnLap ? LapLanes[static_cast<size_t>(Dice.RandRange(0, static_cast<int32>(LapLanes.size()) - 1))] : Dice.RandRange(0, Lanes.Num() - 1);
			if (!Lanes.IsValidIndex(LaneIndex))
			{
				continue;
			}
			const FGTPTrafficLane& Lane = Lanes[LaneIndex];
			if (Lane.Points.Num() < (bOnLap ? 6 : 10) || Lane.Next.Num() == 0)
			{
				continue;
			}
			const int32 Index = Lane.Points.Num() / 3;
			// On the lap the car is put on the line it drives there (GTPFlightCircuit.h), not on the lane's own points: the
			// two are metres apart at a corner, more where the line goes round something that stands on the lane.
			// Gate 86's long take: put on the lane's points of the rue Grolee, the car started at full lock toward its line.
			FVector PutAt = Lane.Points[Index];
			FVector PutToward = Lane.Points[Index + 1];
			bool bPutOnLine = false;
			if (const std::vector<GTPFlightCircuit::FLinePoint>* DrivenThere = bOnLap ? Network->GetFlightCircuit().LineOf(LaneIndex) : nullptr)
			{
				if (DrivenThere->size() >= 3)
				{
					const size_t Third = DrivenThere->size() / 3;
					PutAt = FVector((*DrivenThere)[Third].X * 100.0, (*DrivenThere)[Third].Y * 100.0, PutAt.Z);
					PutToward = FVector((*DrivenThere)[Third + 1].X * 100.0, (*DrivenThere)[Third + 1].Y * 100.0, PutAt.Z);
					bPutOnLine = true;
				}
			}
			// Not where the line is far from its lane (it goes round something there): the follower starts from the lane
			// it finds within 9 m of the car.
			if (bPutOnLine && !Lane.Points.ContainsByPredicate([&PutAt](const FVector& Of) { return FVector::Dist2D(Of, PutAt) < 600.0; }))
			{
				continue;
			}
			const FVector Point = PutAt;
			const double Distance = FVector::Dist2D(Point, Driven->GetActorLocation());
			if (Distance < (bOnLap ? 6000.0 : 8000.0) || Distance > (bOnLap ? 25000.0 : 15000.0))
			{
				continue;
			}
			if (bOnLap)
			{
				if (Lane.bBlocked && Pass + 1 < GTPRoadWidth::RelocationPasses)
				{
					continue;
				}
			}
			else if (!GTPRoadWidth::RelocationFits(Pass, Lane.bBlocked, Lane.bNeverEnds, Pass >= 3 ? FGTPLaneFollower::OpenRoadMetres(Lanes, LaneIndex) : 0.f,
					FGTPLaneFollower::OpenRoadWanted, Lane.CarriagewayM)
				|| (Pass + 1 < GTPRoadWidth::RelocationPasses && Follower.IsRecent(LaneIndex)))
			{
				continue;
			}
			if (PoliceAt.ContainsByPredicate([&Point](const FVector& At) { return FVector::Dist2D(At, Point) < 6000.0; }))
			{
				continue;
			}
			FHitResult Ground;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPChaseRelocate), false, Driven);
			if (!World->LineTraceSingleByChannel(Ground, Point + FVector(0.f, 0.f, 500.f), Point - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
			{
				continue;
			}
			// Never drop the car in or next to the river (the recovery rule would take the player out).
			if (River && (River->IsInWater(FVector(Point.X, Point.Y, Ground.ImpactPoint.Z), 3000.f) || River->IsInWater(Ground.ImpactPoint, 3000.f)))
			{
				continue;
			}
			const FVector From = Driven->GetActorLocation();
			const FVector Direction = (PutToward - Point).GetSafeNormal2D();
			RelocatedTo = FTransform(Direction.Rotation(), FVector(Point.X, Point.Y, Ground.ImpactPoint.Z + 60.f));
			// Never into another car (traffic, police, parked): the solver does not survive a deep overlap.
			if (Network->bPoolSpawnCheck && Network->FindCarAt(Driven, RelocatedTo))
			{
				continue;
			}
			// Nor onto anything else that stops a car (a bike rack, a tree, a bench, a shelter): only cars were looked
			// for, and the lanes are cleared 1.5 m around their middle, not where their points are. Gate 83's long
			// take: put on a lane of the rue Jean de Tournes, the car lay on its roof 2 s later (the probable cause,
			// read in the code, not seen in the log). The car's box from 40 cm above the ground, so the ground and
			// the kerbs do not count.
			{
				const FVector HalfBox(235.f, 120.f, 60.f);
				const FVector BoxCentre(Point.X, Point.Y, Ground.ImpactPoint.Z + 40.f + HalfBox.Z);
				FCollisionQueryParams BoxParams(SCENE_QUERY_STAT(GTPChaseRelocateBox), false, Driven);
				if (World->OverlapBlockingTestByChannel(BoxCentre, RelocatedTo.GetRotation(), ECC_Vehicle, FCollisionShape::MakeBox(HalfBox), BoxParams))
				{
					++SpotsTaken;
					continue;
				}
				// Nor onto a person or a car, whatever they answer to a car's channel (gate 86, rue Grolee: the car stood
				// on its nose 2 s after it was put down, a passer-by under it).
				FCollisionObjectQueryParams Standing;
				Standing.AddObjectTypesToQuery(ECC_Pawn);
				Standing.AddObjectTypesToQuery(ECC_Vehicle);
				Standing.AddObjectTypesToQuery(ECC_PhysicsBody);
				const FVector TallBox(HalfBox.X + 50.f, HalfBox.Y + 50.f, 90.f);
				if (World->OverlapAnyTestByObjectType(FVector(Point.X, Point.Y, Ground.ImpactPoint.Z + 10.f + TallBox.Z), RelocatedTo.GetRotation(), Standing, FCollisionShape::MakeBox(TallBox), BoxParams))
				{
					++SpotsTaken;
					continue;
				}
			}
			// Physics off while the car is moved, and for half a second after: the solver undoes the teleport of a
			// simulated car (seen in GTPTestDrive on the M1, and here on 16/11: put 119 m away, the car was back
			// where it stood 2 s later, seven times in a row). RunStep turns the physics back on.
			if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Driven->GetRootComponent()))
			{
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				Body->SetSimulatePhysics(false);
			}
			Driven->SetActorLocationAndRotation(RelocatedTo.GetLocation(), RelocatedTo.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
			bRelocateHeld = true;
			RelocatedCar = Driven;
			RelocateHoldUntil = Clock() + 0.5;
			bRelocateBlockedTold = false;
			// Gate 88's long take: a grey car of the traffic was in the stolen car while it was held here, and the
			// stolen car stood on its tail 0.3 s after its physics came back. No test above saw it: a far traffic
			// car has no collision. They are sent away from the place now, and at every step of the hold.
			RelocateFarSent = DemoSendFarCarsAway(World, RelocatedTo.GetLocation());
			// The new follower keeps the lanes driven lately: the flight does not come back round to them.
			const TArray<int32> Recent = Follower.GetRecentLanes();
			Follower = FGTPLaneFollower();
			Follower.SetRecentLanes(Recent);
			Follower.bPreferStraight = true;
			Follower.bFollowCircuit = true;
			Follower.AheadMeters = 160.f;
			bOnLaneOnce = false;
			Follower.StartNear(*Network, Driven->GetActorLocation(), static_cast<float>(Driven->GetActorRotation().Yaw), 900.f);
			if (UGTPWantedComponent* Wanted = PC ? PC->FindComponentByClass<UGTPWantedComponent>() : nullptr)
			{
				Wanted->CallPolice();
				// Standing still where the script put it, the car is not the player's doing: no arrest until it has driven off.
				Wanted->SetArrestEnabled(false);
			}
			++ChaseRelocations;
			LastRelocationAt = FPlatformTime::Seconds();
			GTP_TRACE(LogGTPDemo, TEXT("Demo chase: %s at %s, the car is put on lane %d %.0f m away at %s (open road %.0f m, never ends=%d, carriageway %.1f m, %s, pass %d, police told, %d place(s) passed over with something standing there, %s, %d far traffic car(s) sent away from the place)"),
				Why, *From.ToCompactString(), LaneIndex, Distance / 100.0, *Driven->GetActorLocation().ToCompactString(), FGTPLaneFollower::OpenRoadMetres(Lanes, LaneIndex),
				Lane.bNeverEnds ? 1 : 0, Lane.CarriagewayM, ANSI_TO_TCHAR(GTPRoadWidth::ClassName(GTPRoadWidth::Classify(Lane.CarriagewayM))), Pass, SpotsTaken,
				bPutOnLine ? TEXT("on its driving line") : TEXT("on the lane's own points"), RelocateFarSent);
			return true;
		}
	}
	GTP_TRACE(LogGTPDemo, TEXT("Demo chase: %s, no lane 80 to 150 m away to put the car on"), Why);
	return false;
}

double UGTPDemoRoute::GetSecondsSinceRelocation() const
{
	return LastRelocationAt < 0.0 ? 1.0e9 : FPlatformTime::Seconds() - LastRelocationAt;
}

void UGTPDemoRoute::FreeChaseLevel()
{
	if (bChase && !bLevelFree)
	{
		bLevelFree = true;
		GTP_TRACE(LogGTPDemo, TEXT("Demo chase: the level is no longer scripted (no forced stars, arrest possible)"));
	}
}

bool UGTPDemoRoute::HasFlightEndPlace() const
{
	return bChase && Follower.HasCircuitEndPlace();
}

bool UGTPDemoRoute::IsOnFlightEndPlace() const
{
	return bChase && Follower.IsOnCircuitEndPlace();
}

void UGTPDemoRoute::GiveUpChase()
{
	if (bChase && !bGaveUp)
	{
		bGaveUp = true;
		bLevelFree = true;
		GTP_TRACE(LogGTPDemo, TEXT("Demo chase: the player gives up, the car stops and waits for the police"));
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
				// No hunt for a moving car: the nearest free car (a parked one, the -TestVehicle one) is brought to the
				// player's side. Which model that is changes from run to run. -DemoFreeCar=Car02 makes one of the model
				// whose class name holds that text instead (among the traffic's classes), so that a run can be played
				// again with the same car: the theft was seen in Car05 and Car12, not in Car02 (gates 49 to 52).
				Car = FindCar();
				FString FreeCarModel;
				if (FParse::Value(FCommandLine::Get(), TEXT("DemoFreeCar="), FreeCarModel) && !FreeCarModel.IsEmpty())
				{
					UClass* Model = nullptr;
					if (TActorIterator<AGTPTrafficNetwork> Network(Pawn->GetWorld()); Network)
					{
						for (const TSubclassOf<AGTPVehicle>& Class : Network->LoadedVehicleClasses)
						{
							if (Class.Get() && Class->GetName().Contains(FreeCarModel))
							{
								Model = Class.Get();
								break;
							}
						}
					}
					FActorSpawnParameters Made;
					Made.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					const FVector Beside = Pawn->GetActorLocation() + FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f).Vector() * 500.f + FVector(0.f, 0.f, 100.f);
					AGTPVehicle* Asked = Model ? Pawn->GetWorld()->SpawnActor<AGTPVehicle>(Model, Beside, FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), Made) : nullptr;
					if (Asked)
					{
						Car = Asked;
					}
					GTP_TRACE(LogGTPDemo, TEXT("Demo chase: -DemoFreeCar=%s: %s"), *FreeCarModel,
						Asked ? *FString::Printf(TEXT("%s made"), *Asked->GetName()) : (Model ? TEXT("the car could not be made, the nearest free car is taken") : TEXT("no traffic class of that name, the nearest free car is taken")));
				}
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
					Wanted->ReportCrime(EGTPCrimeKind::CarTheft, Pawn->GetActorLocation(), false, Pawn);
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
			if (UPrimitiveComponent* Body = bRelocateHeld && RelocatedCar.IsValid() ? Cast<UPrimitiveComponent>(RelocatedCar->GetRootComponent()) : nullptr)
			{
				// The player left the car in the half second it was held on its new lane: the car gets its physics back.
				Body->SetSimulatePhysics(true);
			}
			bRelocateHeld = false;
			SetPhase(EPhase::Done);
			break;
		}
		if (bRelocateHeld)
		{
			// Just put on another lane (RelocateChase): held there with the physics off, then given back to the solver.
			Driven->SetActorLocationAndRotation(RelocatedTo.GetLocation(), RelocatedTo.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
			RelocateFarSent += DemoSendFarCarsAway(Driven->GetWorld(), RelocatedTo.GetLocation());
			if (Now < RelocateHoldUntil)
			{
				break;
			}
			// Never back to the solver with another car in it (a ghost of the traffic slides through what stands on its
			// lane, and weighs as much as a wall): a car of the traffic is taken off the road, any other is waited for,
			// 3 s at most.
			if (Now < RelocateHoldUntil + 3.0)
			{
				TActorIterator<AGTPTrafficNetwork> HeldNetwork(Driven->GetWorld());
				if (const AActor* InTheCar = HeldNetwork && HeldNetwork->bPoolSpawnCheck ? HeldNetwork->FindCarAt(Driven, RelocatedTo) : nullptr)
				{
					bool bTakenAway = false;
					for (TActorIterator<AGTPVehicle> It(Driven->GetWorld()); It; ++It)
					{
						if (*It == InTheCar && It->HasAIDriver() && !It->ActorHasTag(TEXT("GTPPolice")))
						{
							bTakenAway = HeldNetwork->Recycle(*It);
							break;
						}
					}
					if (!bRelocateBlockedTold || bTakenAway)
					{
						bRelocateBlockedTold = true;
						GTP_TRACE(LogGTPDemo, TEXT("Demo chase: %s is in the car held on its new lane, %s; the physics wait"), *InTheCar->GetName(),
							bTakenAway ? TEXT("taken off the road") : TEXT("not a car of the traffic to take away"));
					}
					break;
				}
			}
			const double HeldLonger = Now - RelocateHoldUntil;
			RelocateHoldUntil = Now;
			bRelocateHeld = false;
			if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Driven->GetRootComponent()))
			{
				Body->SetSimulatePhysics(true);
				Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				Body->WakeAllRigidBodies();
			}
			Driven->NotePlaced(TEXT("chase relocation"));
			SlowSince = 0.0;
			GTP_TRACE(LogGTPDemo, TEXT("Demo chase: the car is on its new lane at %s, physics back on (held %.1f s longer for a car in it, %d far traffic car(s) sent away from the place)"),
				*Driven->GetActorLocation().ToCompactString(), HeldLonger, RelocateFarSent);
			DemoWhoIsInTheCar(Driven, TEXT("physics back on"), true);
			bRelocationWatched = true;
			break;
		}
		// For 4 s after a relocation: the first time the car leans past 45 degrees, who is in it is said again.
		if (bRelocationWatched && (Now - RelocateHoldUntil > 4.0 || Driven->GetActorUpVector().Z < 0.7071))
		{
			bRelocationWatched = false;
			if (Now - RelocateHoldUntil <= 4.0)
			{
				++RelocationsTipped;
				GTP_TRACE(LogGTPDemo, TEXT("Demo chase: the car leans past 45 degrees %.1f s after its relocation (%d relocation(s) ended so, of %d)"),
					Now - RelocateHoldUntil, RelocationsTipped, ChaseRelocations);
				DemoWhoIsInTheCar(Driven, TEXT("leaning"), false);
			}
		}
		if (bChase && bGaveUp)
		{
			// The player gives up (-GTARun, outcome of the chase): brake to a stop, then the handbrake, still at the
			// wheel. Nothing else of the script runs: no lane, no relocation, no end of the drive time.
			// The arrest is left as the run set it.
			DriveInputs(Driven, 0.f, 1.f, 0.f, FMath::Abs(Driven->GetSpeedKmh()) < 2.f);
			break;
		}
		float Cruise = CruiseKmh;
		if (bChase)
		{
			// From 12 s a credible flight for a city by the width of the street (GTPRoadWidth::FlightKmh): 70 km/h on a main
			// axis, 60 in a middle street, 45 in a narrow one. Until gate 75: 90 everywhere, 72 km/h reached in a street
			// 6 m wide between the facades and the car against a wall at its bend.
			// On the flight circuit of the map (lap and approach, GTPFlightCircuit.h) the lanes were looked at beforehand
			// and are driven on a line without the jogs of the lanes: from 4 s, the speed its row asks, street by street
			// (the kerbs were measured: a one-way street of one lane is 8 to 11 m wide where the lanes file says 3), or
			// without one 100 km/h on a quay and 75 on another main axis.
			const bool bOnCircuit = Follower.IsOnCircuit();
			const float AskedKmh = Follower.GetCircuitKmh();
			const float CircuitKmh = AskedKmh > 0.f ? AskedKmh : GTPFlightCircuit::CruiseKmh(Follower.GetCarriagewayM());
			Cruise = T >= (bOnCircuit ? 4.0 : 12.0) ? (bOnCircuit ? CircuitKmh : GTPRoadWidth::FlightKmh(Follower.GetCarriagewayM())) : 40.f;
			// The way ahead is kept clear of the city's traffic (a demo cannot queue behind a taxi): twice a second, a
			// traffic car in the player's own way is taken off the road, ahead within 35 m and within 2.5 m of the line his
			// car drives on. The cars coming the other way, beside him or crossing further on stay in the street: until
			// candidate 60 every car within 35 m in a cone of 60 degrees went, and the video of gate 59 showed no traffic
			// car around the player in the five frames counted.
			if (Now >= NextClear)
			{
				NextClear = Now + 0.5;
				TActorIterator<AGTPTrafficNetwork> Network(Driven->GetWorld());
				const FVector Forward = Driven->GetActorForwardVector().GetSafeNormal2D();
				for (TActorIterator<AGTPVehicle> It(Driven->GetWorld()); Network && It; ++It)
				{
					AGTPVehicle* Other = *It;
					// A car its driver left in the street (he ran from the shots) has no driver to be known by and stood
					// in the way for good (GTPLeftCars.h).
					const bool bLeftThere = Other != Driven && !Other->HasAIDriver() && Network->IsLeftInTheStreet(Other);
					if (Other == Driven || !(Other->HasAIDriver() || bLeftThere) || Other->ActorHasTag(TEXT("GTPPolice")))
					{
						continue;
					}
					const FVector Offset = Other->GetActorLocation() - Driven->GetActorLocation();
					const double Along = Offset.X * Forward.X + Offset.Y * Forward.Y;
					const double Aside = Offset.X * Forward.Y - Offset.Y * Forward.X;
					if (GTPLeftCars::InTheWay(static_cast<float>(Along) * 0.01f, static_cast<float>(Aside) * 0.01f))
					{
						if (bLeftThere)
						{
							Network->TakeLeftCarAway(Other, TEXT("in the way of the chase script"));
						}
						else
						{
							Network->Recycle(Other);
						}
					}
				}
			}
			// The police keep their roadblocks off the lines of the circuit while this script drives the flight
			// (AGTPPoliceDirector::KeepFlightLineClear): it follows its line, it cannot go round two cars across the
			// street. Gate 91: one stood 13 m past the place where the first flight ended, short of the place Bellecour.
			if (TActorIterator<AGTPPoliceDirector> FlightPolice(Driven->GetWorld()); FlightPolice)
			{
				FlightPolice->KeepFlightLineClear(2.f);
			}
			UGTPWantedComponent* Wanted = PC->FindComponentByClass<UGTPWantedComponent>();
			if (Wanted)
			{
				// Nobody can arrest the player before the flight starts: the chase must show first.
				Wanted->SetArrestEnabled((bLevelFree || T >= 100.0) && GetSecondsSinceRelocation() >= RelocationGraceSeconds);
				if (!bLevelFree && T >= 8.0 && T < 60.0)
				{
					Wanted->EnsureMinStars(T >= 30.0 ? 3 : 2);
				}
				if (Now >= NextChaseLog)
				{
					NextChaseLog = Now + 5.0;
					// The police as the player sees them (GTPChaseTally.h, summed since the level rose): -1 = no unit on the road yet.
					float PoliceKmh = -1.f, PoliceNearest = -1.f;
					if (TActorIterator<AGTPPoliceDirector> Director(Driven->GetWorld()); Director && Director->GetChaseTally().Seconds > 0.0)
					{
						PoliceKmh = Director->GetChaseTally().MeanKmh();
						PoliceNearest = Director->GetChaseTally().MeanNearest();
					}
					GTP_TRACE(LogGTPDemo, TEXT("DemoChase t=%.0f stars=%d phase=%d seen=%d kmh=%.0f peak=%.0f police mean=%.0f km/h nearest mean=%.0f m relocations=%d"), static_cast<float>(T), Wanted->GetStars(),
						Wanted->GetPhaseIndex(), Wanted->IsSeenNow() ? 1 : 0, Driven->GetSpeedKmh(), PeakKmh, PoliceKmh, PoliceNearest, ChaseRelocations);
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
			if (bChase)
			{
				// Before the first plan: the exits are chosen when the lanes are planned, and StartNear plans. Set afterwards,
				// the first 50 m were drawn at random (door 41: into a lane whose way ends on the quay).
				Follower.bPreferStraight = true;
				Follower.bFollowCircuit = true;
				Follower.AheadMeters = 160.f;
				bOnLaneOnce = false;
			}
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
			if (bChase)
			{
				Follower.SetHardDriving();
				Follower.bPreferStraight = true;
				Follower.bFollowCircuit = true;
				Follower.AheadMeters = 160.f;
			}
			// Held up for more than 2.5 s (a car in a narrow street, a corner cut too short): first push through for
			// 3 s, then, if it happens again, back off for 2 s with the wheels turned and go on.
			if (bChase && T > 4.0)
			{
				if (Now < UnstickUntil)
				{
					DriveInputs(Driven, 0.f, 1.f, -UnstickSteer, false, true);
					break;
				}
				if (FMath::Abs(Driven->GetSpeedKmh()) < 3.f)
				{
					SlowSince = SlowSince <= 0.0 ? Now : SlowSince;
					if (Now - SlowSince > 2.0 && Now >= RamUntil)
					{
						SlowSince = 0.0;
						// Against something that does not move (a wall, a post, a block of the map), within 3 m: pushing on it
						// only shows a car pressed on a wall (gate 75's film, 24 to 28 s against a traffic light post).
						bool bStillAhead = false;
						const AActor* StillActor = nullptr;
						FVector StillAt = FVector::ZeroVector;
						{
							// What holds the car: the first thing within 8 m ahead (any channel the car collides with) and the car's own state.
							FHitResult Ahead;
							FCollisionQueryParams HeldParams(SCENE_QUERY_STAT(GTPChaseHeld), false, Driven);
							// Not the car's own driver. Gate 89's long take, 210 to 237 s: held up four times at the same place of
							// the place Bellecour in the van of the second flight, and each time the line named the player's own
							// character "1.5 m ahead", where a van's seat is: what really held the car was never said.
							if (const ACharacter* AtTheWheel = Driven->GetDriver())
							{
								HeldParams.AddIgnoredActor(AtTheWheel);
							}
							// Level ahead and clear of the ground: from 55 to 145 cm above the car's origin. Gate 86's long take: a
							// sphere of 60 cm at 60 cm, along the car's own pitch, touched the road, and two tiles of the terrain
							// (StaticMeshActor_741 and _742, 256 m wide) were read as obstacles "at 0 to 2 m", their lanes blocked
							// and the car put elsewhere. Ground is never an obstacle: nothing whose face looks up counts.
							const FVector From = Driven->GetActorLocation() + FVector(0.f, 0.f, 100.f);
							const FVector To = From + Driven->GetActorForwardVector().GetSafeNormal2D() * 800.f;
							const bool bHit = Driven->GetWorld()->SweepSingleByChannel(Ahead, From, To, FQuat::Identity, ECC_Vehicle, FCollisionShape::MakeSphere(45.f), HeldParams);
							const UPrimitiveComponent* HitBody = bHit ? Ahead.GetComponent() : nullptr;
							const bool bGround = bHit && Ahead.ImpactNormal.Z > 0.7f;
							bStillAhead = HitBody && !bGround && Ahead.Distance < 300.f && !HitBody->IsSimulatingPhysics() && !Cast<APawn>(Ahead.GetActor());
							StillActor = bStillAhead ? Ahead.GetActor() : nullptr;
							StillAt = Ahead.ImpactPoint;
							GTP_TRACE(LogGTPDemo, TEXT("Demo chase: held up at %s yaw=%.0f gear=%d, ahead: %s (%s) at %.1f m%s"), *Driven->GetActorLocation().ToCompactString(),
								Driven->GetActorRotation().Yaw, Driven->GetGear(), bHit ? *GetNameSafe(Ahead.GetActor()) : TEXT("nothing"),
								bHit ? *GetNameSafe(Ahead.GetComponent()) : TEXT("-"), bHit ? Ahead.Distance / 100.f : -1.f,
								bStillAhead ? TEXT(", does not move: backing off") : bGround ? TEXT(", the ground: not an obstacle") : TEXT(""));
						}
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
									Cleared += Network->Recycle(Other) ? 1 : 0;
								}
							}
							if (Cleared > 0)
							{
								GTP_TRACE(LogGTPDemo, TEXT("Demo chase: %d traffic car(s) taken off the road around the player"), Cleared);
							}
						}
						// On its lane against a fixed obstacle that stands on the lane (gate 75's film: a white block 2 m from
						// the middle of the rue de la Bourse, 25 s nose on it; a block on the way from the quai Jean Moulin to
						// the rue Gentil, met at every turn round that block): that lane is blocked for the flights from now
						// on (AGTPTrafficNetwork::MarkLaneBlocked) and the chase goes on at once from another lane.
						if (bStillAhead)
						{
							float ObstacleOffLane = 0.f;
							const int32 BlockedLane = Follower.LaneNear(StillAt, ObstacleOffLane);
							const float CarOffLane = Follower.OffPathMetres(Driven->GetActorLocation());
							TActorIterator<AGTPTrafficNetwork> Network(Driven->GetWorld());
							if (Network && BlockedLane != INDEX_NONE && CarOffLane < 3.f && ObstacleOffLane < 3.5f)
							{
								GTP_TRACE(LogGTPDemo, TEXT("Demo chase: the car is %.1f m off its lane, the obstacle %.1f m off lane %d: lane blocked"),
									CarOffLane, ObstacleOffLane, BlockedLane);
								Network->MarkLaneBlocked(BlockedLane, StillActor, StillAt);
								if (RelocateChase(Driven, PC, TEXT("against a fixed obstacle on its lane")))
								{
									HeldUpHere = 0;
									ChaseStuckCount = 0;
									break;
								}
							}
						}
						// Held up a second time within 15 m (pushed for 3 s, still there): beached, boxed in or against
						// something that does not move. The chase goes on from another lane, as at the end of the lanes.
						const FVector HeldAt = Driven->GetActorLocation();
						HeldUpHere = FVector::Dist2D(HeldAt, HeldUpAt) < 1500.0 ? HeldUpHere + 1 : 1;
						HeldUpAt = HeldAt;
						if (HeldUpHere >= 2)
						{
							// Before the car goes elsewhere: who and what stands in it and round it (the town's bodies too while it is upright).
							DemoWhoIsInTheCar(Driven, TEXT("held up twice in the same place"), Driven->GetActorUpVector().Z > 0.9);
						}
						if (HeldUpHere >= 2 && RelocateChase(Driven, PC, TEXT("held up twice in the same place")))
						{
							HeldUpHere = 0;
							ChaseStuckCount = 0;
							break;
						}
						if (++ChaseStuckCount % 2 == 1 && !bStillAhead)
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
			// The first seconds at the wheel ignore what stands ahead: the taken car starts from a kerb, among the others.
			// What the probe met (pawns, vehicles, physics bodies), named in the DemoDrive line.
			AActor* ObstacleActor = nullptr;
			float ObstacleM = (Now < RamUntil || (bChase && T < 6.0)) ? 1000.f
				: (bChase ? UGTPAIDriverComponent::ProbeAhead(Driven, 900.f, 60.f, &ObstacleActor) : UGTPAIDriverComponent::ProbeAhead(Driven, 4000.f, 90.f, &ObstacleActor));
			// A person the flight can no longer stop for is not braked for (GTPFlightPedals::StopsForPerson; gate 86: the
			// whole brake at 98 km/h for a person at 0 m, the car sideways into a tree). -DemoStopsForPeople: as until then.
			float PersonPassedM = -1.f;
			if (bChase && ObstacleActor && ObstacleM >= 0.f && ObstacleM < 1000.f && (ObstacleActor->IsA<AGTPPedestrian>() || ObstacleActor->IsA<ACharacter>()))
			{
				static const bool bStopsForPeople = FParse::Param(FCommandLine::Get(), TEXT("DemoStopsForPeople"));
				if (!bStopsForPeople && !GTPFlightPedals::StopsForPerson(ObstacleM, Driven->GetSpeedKmh() / 3.6f))
				{
					PersonPassedM = ObstacleM;
					ObstacleM = -1.f;
					++PeopleNotBrakedFor;
				}
			}
			if (Follower.Step(Driven->GetActorLocation(), static_cast<float>(Driven->GetActorRotation().Yaw), Driven->GetSpeedKmh(), Cruise, ObstacleM, Command))
			{
				if (bChase)
				{
					// Where the time goes: seconds spent more than 8 km/h under the cruise speed, by what set the target speed.
					const double Dt = FMath::Clamp(Now - LastLimitTime, 0.0, 0.5);
					LastLimitTime = Now;
					if (Driven->GetSpeedKmh() < Cruise - 8.f && Command.Limit >= 0 && Command.Limit < GTPDrive::LimitCount)
					{
						LimitSeconds[Command.Limit] += static_cast<float>(Dt);
					}
					if (Now >= NextLimitLog)
					{
						NextLimitLog = Now + 10.0;
						GTP_TRACE(LogGTPDemo, TEXT("DemoLimit slow seconds: lane=%.0f bend=%.0f follow=%.0f obstacle=%.0f pathend=%.0f"), LimitSeconds[0], LimitSeconds[1],
							LimitSeconds[2], LimitSeconds[3], LimitSeconds[4]);
					}
				}
				if (bChase && T > 4.0)
				{
					// Off the road: more than 6 m from its lane for 1.5 s (down the bank of the quay on 16/11: 7 m east of
					// the lane and 2 m below the street, then against the ground), or at the edge of the river, where the
					// recovery rule takes the player out of the car and ends the chase. The flight goes on from another lane.
					const FVector CarAt = Driven->GetActorLocation();
					const float OffLane = Follower.OffPathMetres(CarAt);
					// Only a car that has been on its lane since the lanes were last joined: the stolen car starts from the kerb,
					// up to 8 m from the lane it joins (door 45: put elsewhere 5 s after the theft, before it had driven at all).
					bOnLaneOnce = bOnLaneOnce || OffLane < 3.f;
					OffLaneSince = (bOnLaneOnce && OffLane > 6.f) ? (OffLaneSince <= 0.0 ? Now : OffLaneSince) : 0.0;
					const UGTPRiverRecovery* River = Driven->GetWorld()->GetSubsystem<UGTPRiverRecovery>();
					const bool bAtWater = River && River->IsInWater(CarAt, 300.f);
					if ((bAtWater || (OffLaneSince > 0.0 && Now - OffLaneSince > 1.5)) && Now >= NextRelocate)
					{
						NextRelocate = Now + 2.0;
						OffLaneSince = 0.0;
						GTP_TRACE(LogGTPDemo, TEXT("Demo chase: off the road at %s (%.0f m from lane %d, water=%d)"), *CarAt.ToCompactString(), OffLane, Follower.GetLane(), bAtWater ? 1 : 0);
						if (RelocateChase(Driven, PC, bAtWater ? TEXT("at the edge of the river") : TEXT("off its lane")))
						{
							break;
						}
					}
				}
				if (bChase)
				{
					// The flight's pedals (GTPFlightPedals.h): a firm throttle on the straights, and with the wheels turned
					// past a third of lock no throttle while the car is faster than that lock holds. Gate 83: the whole
					// throttle at full lock as a lane was left in a corner, the car 27 m from its lane a second later.
					// -DemoPedalsAlone: the firm throttle only, as until then.
					static const bool bPedalsAlone = FParse::Param(FCommandLine::Get(), TEXT("DemoPedalsAlone"));
					FGTPDriveSettings FlightDriving;
					GTPDrive::SetChaseDriving(FlightDriving);
					GTPFlightPedals::Apply(Command.Throttle, Command.Brake, Command.Steering, Driven->GetSpeedKmh() / 3.6f, Cruise / 3.6f, FlightDriving, !bPedalsAlone);
				}
				// The car squares the steering it is given, then gives its wheels a share of it that falls with speed
				// (GTPSteeringCurve.h): the flight undoes both, so that the wheels turn as the lane follower reckoned
				// (GTPFlightPedals::SteeringSent). Gate 93: 0.07 asked on the quay at 99 km/h, a seventh of it at the wheels,
				// the car 2.2 m off its line.
				// -DemoSteerSquared: the share of the speed made up and the square left, as from gate 93 to gate 99.
				// -DemoSteerAlone: the steering as asked, as until gate 92.
				static const bool bSteerAlone = FParse::Param(FCommandLine::Get(), TEXT("DemoSteerAlone"));
				static const bool bSteerSquared = FParse::Param(FCommandLine::Get(), TEXT("DemoSteerSquared"));
				const float SteerSent = !bChase || bSteerAlone ? Command.Steering
					: (bSteerSquared ? GTPFlightPedals::SteeringShareMadeUp(Command.Steering, Driven->GetSpeedKmh()) : GTPFlightPedals::SteeringSent(Command.Steering, Driven->GetSpeedKmh()));
				DriveInputs(Driven, Command.Throttle, Command.Brake, SteerSent, false);
				PeakKmh = FMath::Max(PeakKmh, Driven->GetSpeedKmh());
				if (bChase && Now >= NextDriveDiag)
				{
					// off: metres from the line the car drives (the path of its follower); xy and yaw: where it is (cm) and
					// where it points; pull: what its dents would add to a player's steering (0 = no side beaten more than the
					// other). Gate 89's long take: 2 s at 98 km/h left of the quay's kerb, then a tree; the log did not say how
					// far the car was from its line, nor whether its steering answered. sent: the steering given to the car;
					// wheels: the share of the lock its front wheels get of it (GTPSteeringCurve::WheelsGet), to set against steer.
					NextDriveDiag = Now + 1.0;
					GTP_TRACE(LogGTPDemo, TEXT("DemoDrive t=%.0f kmh=%.0f gear=%d rpm=%.0f lane=%d off=%.1f m xy=%.0f,%.0f yaw=%.0f pull=%.2f throttle=%.2f brake=%.2f steer=%.2f sent=%.2f wheels=%.3f cruise=%.0f want=%.0f limit=%s obstacle=%.0f m%s"), static_cast<float>(T),
						Driven->GetSpeedKmh(), Driven->GetGear(), Driven->GetEngineRpm(), Follower.GetLane(), Follower.OffPathMetres(Driven->GetActorLocation()),
						Driven->GetActorLocation().X, Driven->GetActorLocation().Y, Driven->GetActorRotation().Yaw, Driven->GetDentPull(), Command.Throttle, Command.Brake, Command.Steering, SteerSent, GTPSteeringCurve::WheelsGet(SteerSent, Driven->GetSpeedKmh()), Cruise, Command.TargetSpeed * 3.6f,
						LimitName(Command.Limit), ObstacleM,
						ObstacleActor ? *FString::Printf(TEXT(" (%s at %s%s)"), *ObstacleActor->GetName(), *ObstacleActor->GetActorLocation().ToCompactString(),
							PersonPassedM >= 0.f ? *FString::Printf(TEXT(", %.0f m ahead, not braked for: %d frame(s) so far"), PersonPassedM, PeopleNotBrakedFor) : TEXT("")) : TEXT(""));
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
				if (RelocateChase(Driven, PC, TEXT("lanes ended")))
				{
					break;
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
			DriveInputs(Driven, 0.f, 0.6f, -UnstickSteer, false, true);
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
			if (UChaosVehicleMovementComponent* Move = Driven->GetVehicleMovementComponent())
			{
				// The pedals go back to the player as AGTPVehicle::PossessedBy set them.
				Move->bReverseAsBrake = true;
			}
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
