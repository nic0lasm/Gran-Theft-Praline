#include "GTPCrowd.h"
#include "Materials/MaterialInstance.h"
#include "Materials/Material.h"
#include "Components/SkeletalMeshComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Texture.h"
#include "EngineUtils.h"
#include "GTPAudioWorld.h"
#include "GTPImpactLogic.h"
#include "GTPPedestrians.h"
#include "GTPThreat.h"
#include "GTPVehicle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/UnrealType.h"
#if WITH_GTP_MASSTRAFFIC
#include "GTPTraffic.h"
#include "MassMovementFragments.h"
#include "MassTrafficFragments.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGTPCrowd, Log, All);

namespace GTPCrowdLocal
{
/**
 * A person seen by the MassTraffic cars: their radius (cm) in MassTraffic's sphere test, -CrowdObstacleRadius=<cm>.
 * A car's own sphere is its agent radius, which MassTraffic takes for half its length, on every side: a person on the
 * other half of a crossing (6 m long in walkways.csv, at the median) can stand inside it and stop the car. 0 keeps a
 * person a point and frees the next 40 cm (gate 57: the longest queues waited behind people 2 to 5 m beside their
 * path). 40 was Epic's walker radius.
 */
constexpr float TrafficObstacleRadius = 0.f;
}

// ---------------------------------------------------------------- subsystem

bool UGTPCrowdSubsystem::IsRequested()
{
	// The default: -Pedestrians=legacy or a number (legacy density) keeps the old pedestrian system.
	FString Mode;
	return !FParse::Value(FCommandLine::Get(), TEXT("Pedestrians="), Mode) || Mode.Equals(TEXT("mass"), ESearchCase::IgnoreCase);
}

bool UGTPCrowdSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsRequested() && World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGTPCrowdSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPCrowdSubsystem, STATGROUP_Tickables);
}

void UGTPCrowdSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FString File = FPaths::ProjectContentDir() / TEXT("City/Pedestrians")
		/ (UGameplayStatics::GetCurrentLevelName(&InWorld, true) + TEXT(".walkways.csv"));
	const int32 Edges = UGTPPedestrianSubsystem::LoadWalkwaysFromCsv(File, Graph);
	if (Edges == 0)
	{
		UE_LOG(LogGTPCrowd, Warning, TEXT("crowd: off, %s not found"), *File);
		return;
	}
	int32 Override = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("PedMass="), Override))
	{
		CrowdCount = FMath::Max(0, Override);
	}
	// Crossings: in groups when a car is about to go over one (GTPCrowd.h, CrossingCycleSeconds).
	int32 CrossingWait = 1;
	FParse::Value(FCommandLine::Get(), TEXT("CrowdCrossingWait="), CrossingWait);
	bCrossingWait = CrossingWait != 0;
	CrossingCars.Init(0, static_cast<int32>(Graph.Edges.size()));
	for (int32 Edge = 0; Edge < CrossingCars.Num(); ++Edge)
	{
		if (Graph.IsValidEdge(Edge) && Graph.Edges[Edge].Kind == GTPWalk::EKind::Crossing && Graph.Edges[Edge].Points.size() >= 2)
		{
			CrossingEdges.Add(Edge);
		}
	}
	if (bCrossingWait)
	{
		Settings.CrossingCycleSeconds = CrossingCycleSeconds;
		Settings.CrossingWalkSeconds = CrossingWalkSeconds;
		Settings.CrossingWaitMax = CrossingWaitMaxSeconds;
	}
	// Cars standing on a walkway are walked round (-CrowdAvoidCars=0: walked through, as before candidate 64).
	int32 AvoidCars = 1;
	FParse::Value(FCommandLine::Get(), TEXT("CrowdAvoidCars="), AvoidCars);
	bAvoidCars = AvoidCars != 0;
	int32 Hair = 1;
	FParse::Value(FCommandLine::Get(), TEXT("CrowdHair="), Hair);
	bHair = Hair != 0;
	int32 Grooms = 1;
	FParse::Value(FCommandLine::Get(), TEXT("CrowdGrooms="), Grooms);
	bGrooms = Grooms != 0;
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: crossings: %d in the graph; %s"), CrossingEdges.Num(), bCrossingWait
		? *FString::Printf(TEXT("with a car about to go over one, people step off the kerb for %.0f s every %.0f s and wait for the rest, %.0f s at most"),
			CrossingWalkSeconds, CrossingCycleSeconds, CrossingWaitMaxSeconds)
		: TEXT("nobody waits for a car (-CrowdCrossingWait=0)"));
	Spawn(InWorld);
	// Hear the shots even when no near character could be dressed (no mesh yet): the far crowd still runs.
	if (UGTPAudioWorld* Audio = InWorld.GetSubsystem<UGTPAudioWorld>())
	{
		LoudHandle = Audio->OnLoudSound.AddUObject(this, &UGTPCrowdSubsystem::OnLoud);
	}
}

void UGTPCrowdSubsystem::Deinitialize()
{
	bRunning = false;
	if (UGTPAudioWorld* Audio = GetWorld() ? GetWorld()->GetSubsystem<UGTPAudioWorld>() : nullptr)
	{
		Audio->OnLoudSound.Remove(LoudHandle);
	}
	for (AGTPPedestrian* A : NearActors)
	{
		if (A)
		{
			A->Destroy();
		}
	}
	NearActors.Reset();
	Entities.Reset();
	if (InstanceActor)
	{
		InstanceActor->Destroy();
		InstanceActor = nullptr;
	}
	Instances = nullptr;
	Super::Deinitialize();
}

float UGTPCrowdSubsystem::GroundAt(const FVector& Where) const
{
	// Somebody just placed: the town is flat around the player, so his ground is the surface nearest the player's.
	return GroundUnder(Where, PlayerGroundZ, 400.f);
}

float UGTPCrowdSubsystem::GroundUnder(const FVector& Where, float Reference, float Up, bool* bOutUnderSomething) const
{
	if (bOutUnderSomething)
	{
		*bOutUnderSomething = false;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return Reference;
	}
	// The town only (static objects), as the cars' own ground traces: a visibility trace also stopped on the roof or
	// the bonnet of a car (gate of candidate 59). And the first surface met from above is not always the ground: the
	// top of a tree, a parasol or a signal pole was taken for it and people walked in the air at that height (video of
	// gate 62, 96 to 140 s). So the surfaces are met one component after the other, down to one he can step onto.
	const FVector Start(Where.X, Where.Y, Reference + Up);
	const FVector End(Where.X, Where.Y, Reference - 1200.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPCrowdGround), false);
	const FCollisionObjectQueryParams Town(ECC_WorldStatic);
	float Heights[4];
	int32 Count = 0;
	while (Count < 4)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, Start, End, Town, Params))
		{
			break;
		}
		Heights[Count++] = static_cast<float>(Hit.ImpactPoint.Z);
		const UPrimitiveComponent* Met = Hit.GetComponent();
		if (Heights[Count - 1] <= Reference + GroundStepUp || !Met)
		{
			break;
		}
		Params.AddIgnoredComponent(Met);
	}
	const int32 Pick = GTPWalk::PickGround(Heights, Count, Reference, GroundStepUp);
	if (Pick < 0)
	{
		return Reference;
	}
	if (bOutUnderSomething)
	{
		*bOutUnderSomething = Pick > 0;
	}
	return Heights[Pick];
}

bool UGTPCrowdSubsystem::PlaceAroundPlayer(GTPWalk::FWalker& Walker, GTPWalk::FRng& Rng) const
{
	for (int32 Try = 0; Try < 60; ++Try)
	{
		if (!GTPWalk::Place(Graph, Walker, Rng, Settings))
		{
			return false;
		}
		GTPWalk::FPoint P, D;
		GTPWalk::Locate(Graph, Walker, P, D);
		const float Distance = FVector::Dist2D(FVector(P.X, P.Y, 0.f), FVector(PlayerLocation.X, PlayerLocation.Y, 0.f));
		if (Distance >= MinSpawnDistance && Distance <= MaxSpawnDistance)
		{
			return true;
		}
	}
	return false;
}

void UGTPCrowdSubsystem::Spawn(UWorld& InWorld)
{
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(&InWorld, 0))
	{
		PlayerLocation = Pawn->GetActorLocation();
		PlayerGroundZ = PlayerLocation.Z - 90.f;
	}
	UMassEntitySubsystem* Mass = InWorld.GetSubsystem<UMassEntitySubsystem>();
	if (!Mass)
	{
		UE_LOG(LogGTPCrowd, Warning, TEXT("crowd: off, no Mass entity subsystem"));
		return;
	}
	FMassEntityManager& EntityManager = Mass->GetMutableEntityManager();

	// The instanced mesh: the baked walking person, else a capsule stand-in.
	InstanceActor = InWorld.SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	Instances = NewObject<UInstancedStaticMeshComponent>(InstanceActor, TEXT("CrowdInstances"));
	UStaticMesh* Mesh = Cast<UStaticMesh>(CrowdMesh.TryLoad());
	BaseScale = FVector(1.f);
	if (!Mesh)
	{
		// The engine cylinder is 100 cm wide and 100 cm high: a person is 60 cm wide and 170 cm tall.
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		BaseScale = FVector(0.6f, 0.6f, 1.7f);
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: no %s, drawing capsule stand-ins"), *CrowdMesh.ToString());
	}
	Instances->SetStaticMesh(Mesh);
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(true);
	Instances->bCastContactShadow = false;
	InstanceActor->SetRootComponent(Instances);
	Instances->RegisterComponent();

	TArray<const UScriptStruct*> Composition{
		FGTPCrowdWalkerFragment::StaticStruct(), FGTPCrowdLookFragment::StaticStruct(), FGTPCrowdTag::StaticStruct()};
#if WITH_GTP_MASSTRAFFIC
	// With MassTraffic cars in the map, a person also carries what Epic's "Traffic Obstacle" trait
	// gives (UMassTrafficObstacleTrait: transform, velocity, radius), so that the cars can brake for
	// them. The tag FMassTrafficObstacleTag itself is worn on a crossing only (UGTPCrowdMoveProcessor).
	const bool bTrafficObstacles = AGTPTrafficNetwork::IsMassTrafficWanted(&InWorld);
	if (bTrafficObstacles)
	{
		Composition.Append({FTransformFragment::StaticStruct(), FMassVelocityFragment::StaticStruct(), FAgentRadiusFragment::StaticStruct()});
	}
#endif
	Archetype = EntityManager.CreateArchetype(Composition);
	float ObstacleRadius = GTPCrowdLocal::TrafficObstacleRadius;
	FParse::Value(FCommandLine::Get(), TEXT("CrowdObstacleRadius="), ObstacleRadius);
	ObstacleRadius = FMath::Max(ObstacleRadius, 0.f);
	GTPWalk::FRng Rng(987654321u);
	int32 Placed = 0;
	for (int32 Index = 0; Index < CrowdCount; ++Index)
	{
		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype);
		FGTPCrowdWalkerFragment& WalkFrag = EntityManager.GetFragmentDataChecked<FGTPCrowdWalkerFragment>(Entity);
		FGTPCrowdLookFragment& Look = EntityManager.GetFragmentDataChecked<FGTPCrowdLookFragment>(Entity);
		if (PlaceAroundPlayer(WalkFrag.Walker, Rng))
		{
			GTPWalk::FPoint P, D;
			GTPWalk::Locate(Graph, WalkFrag.Walker, P, D);
			WalkFrag.Location = FVector(P.X, P.Y, 0.f);
			WalkFrag.GroundZ = GroundAt(WalkFrag.Location);
			WalkFrag.Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			++Placed;
		}
		Look.Scale = Rng.Range(1.f - HeightSpread, 1.f + HeightSpread);
		Look.InstanceIndex = Instances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -100000.f), BaseScale * Look.Scale), false);
		if (FAgentRadiusFragment* Radius = EntityManager.GetFragmentDataPtr<FAgentRadiusFragment>(Entity))
		{
			Radius->Radius = ObstacleRadius;
		}
		Entities.Add(Entity);
	}
	SetupNearActors(InWorld);
	bRunning = true;
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %d people (%d placed) on %d walkway edges"), CrowdCount, Placed, static_cast<int32>(Graph.Edges.size()));
#if WITH_GTP_MASSTRAFFIC
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %s, radius %.0f cm (-CrowdObstacleRadius=<cm>)"), bTrafficObstacles
		? TEXT("traffic obstacles on crossings (MassTraffic cars brake for people crossing)")
		: TEXT("no traffic obstacles (no MassTraffic cars in this map)"), ObstacleRadius);
#endif
}

void UGTPCrowdSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bRunning)
	{
		return;
	}
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		PlayerLocation = Pawn->GetActorLocation();
		PlayerGroundZ = PlayerLocation.Z - 90.f;
	}
	UpdateNear(DeltaTime);
	NextCarLook -= DeltaTime;
	if ((bCrossingWait || bAvoidCars) && NextCarLook <= 0.f)
	{
		NextCarLook = 0.25f;
		LookAtCars();
	}
	NextPanicSpread -= DeltaTime;
	if (NextPanicSpread <= 0.f)
	{
		NextPanicSpread = 0.2f;
		SpreadPanic();
	}
	if (Instances)
	{
		Instances->MarkRenderStateDirty();
	}
}

void UGTPCrowdSubsystem::LookAtCars()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	// The driving MassTraffic cars around the player, as far as the crowd goes plus what a fast car covers in the time
	// people look ahead. Asked here, between two frames of Mass, never from a processor (GTPMassTrafficCars.h).
	const float FastCarReach = 1400.f * Settings.CrossingCarSeconds + 500.f;
	CarsSeen.Reset();
	bCarsToSee = GTPMassTrafficCars::Find(*World, PlayerLocation, RecycleDistance + FastCarReach, CarsSeen);
	// And the car the player drives: nobody steps in front of it either.
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0); Pawn && !Pawn->IsA<ACharacter>())
	{
		const FVector Velocity = Pawn->GetVelocity();
		if (Velocity.SizeSquared2D() > FMath::Square(Settings.CrossingCarFastSpeed))
		{
			FGTPMassTrafficCar& Car = CarsSeen.AddDefaulted_GetRef();
			Car.Location = Pawn->GetActorLocation();
			Car.Forward = Velocity.GetSafeNormal2D();
			Car.SpeedCmS = Velocity.Size2D();
			Car.RadiusCm = 250.f;
		}
	}
	// The cars people walk round: every vehicle of the game standing or rolling slowly near the player (his own, the
	// traffic's, the police's), as the box of its body seen from above, and the slow MassTraffic cars that have no
	// such actor. A fast car is not walked round: people run from it or are knocked down.
	StandingCars.Reset();
	if (bAvoidCars)
	{
		const double Reach = FMath::Square(static_cast<double>(CarAvoidDistance));
		const double Slow = FMath::Square(static_cast<double>(Settings.CrossingCarFastSpeed));
		for (TActorIterator<AGTPVehicle> It(World); It; ++It)
		{
			const UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(It->GetRootComponent());
			if (!Body || FVector::DistSquared2D(It->GetActorLocation(), PlayerLocation) > Reach || It->GetVelocity().SizeSquared2D() > Slow)
			{
				continue;
			}
			const FTransform& Transform = It->GetActorTransform();
			const FVector Forward = Transform.GetUnitAxis(EAxis::X).GetSafeNormal2D();
			if (Forward.IsNearlyZero())
			{
				continue;
			}
			// The car's own box (as AGTPTrafficNetwork::FindCarAt reads it).
			const FBox Local = Body->CalcBounds(FTransform::Identity).GetBox();
			const FVector Centre = Transform.TransformPosition(Local.GetCenter());
			const FVector Extent = Local.GetExtent() * Transform.GetScale3D().GetAbs();
			GTPWalk::FCarBox& Box = StandingCars.AddDefaulted_GetRef();
			Box.Center = GTPWalk::FPoint{static_cast<float>(Centre.X), static_cast<float>(Centre.Y)};
			Box.Forward = GTPWalk::FPoint{static_cast<float>(Forward.X), static_cast<float>(Forward.Y)};
			Box.HalfLength = FMath::Clamp(static_cast<float>(Extent.X), 150.f, 450.f);
			Box.HalfWidth = FMath::Clamp(static_cast<float>(Extent.Y), 60.f, 150.f);
		}
		const int32 Actors = StandingCars.Num();
		for (const FGTPMassTrafficCar& Car : CarsSeen)
		{
			const FVector Forward = Car.Forward.GetSafeNormal2D();
			if (Car.Id == 0 || Car.SpeedCmS > Settings.CrossingCarFastSpeed || Forward.IsNearlyZero() || FVector::DistSquared2D(Car.Location, PlayerLocation) > Reach)
			{
				continue;
			}
			bool bHasActor = false;
			for (int32 Index = 0; Index < Actors && !bHasActor; ++Index)
			{
				bHasActor = FMath::Square(StandingCars[Index].Center.X - Car.Location.X) + FMath::Square(StandingCars[Index].Center.Y - Car.Location.Y) < FMath::Square(200.0);
			}
			if (!bHasActor)
			{
				GTPWalk::FCarBox& Box = StandingCars.AddDefaulted_GetRef();
				Box.Center = GTPWalk::FPoint{static_cast<float>(Car.Location.X), static_cast<float>(Car.Location.Y)};
				Box.Forward = GTPWalk::FPoint{static_cast<float>(Forward.X), static_cast<float>(Forward.Y)};
				Box.HalfLength = FMath::Clamp(Car.RadiusCm, 150.f, 450.f);
			}
		}
	}
	const double LogNow = World->GetTimeSeconds();
	const bool bLogNow = LogNow - LastCrossingLog >= 10.0;
	if (bLogNow && bAvoidCars)
	{
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: cars: %d standing or slow within %.0f m of the player; people going round one %d, standing in front of one %d, within the body of one %d; since the start: most within a body at once %d, turned back at one %d, something high not taken for the ground %d times"),
			StandingCars.Num(), CarAvoidDistance / 100.f, GoingRoundCar, StandingAtCar, InsideCar, MostInsideCar, TurnedBackAtCar, GroundRefusals);
	}
	if (bLogNow)
	{
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: ground: %d people standing more than 1.5 m above the player's ground within 60 m of him now (highest %.1f m); since the start: most at once %d, highest %.1f m, a surface above the ground left aside %d times"),
			HighNow, HighestNow / 100.f, MostHigh, HighestEver / 100.f, UnderSomething);
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: bodies: %d full characters; people within %.0f m of the player without one %d now (most %d), of whom not drawn because nearer than %.0f m %d now (most %d)"),
			NearActors.Num(), NearRadius / 100.f, WithoutBodyNow, MostWithoutBody, StandInMinDistance / 100.f, NotDrawnNow, MostNotDrawn);
	}
	if (bLogNow && bAssembled)
	{
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: dressed %d times: groom on the head %d (not made %d), far hair mesh %d, hair painted on the scalp %d, eyebrows painted %d, left bald %d, no head material that takes hair %d; grooms shown now %d (most at once %d); without a top %d, without trousers or a skirt %d, without shoes %d (women %d), without a face %d"),
			Dressed, HairGrooms, GroomsNotMade, HairMeshes, HairPainted, BrowsPainted, LeftBald, NoHeadMaterial, GroomsShown, MostGroomsShown, WithoutTop, WithoutBottom, WithoutShoes, WithoutShoesWomen, WithoutFace);
	}
	if (!bCrossingWait)
	{
		LastCrossingLog = bLogNow ? LogNow : LastCrossingLog;
		return;
	}
	CrossingsLookedAt = CrossingsWithCarNear = CrossingsWithCarDangerous = 0;
	for (const int32 Edge : CrossingEdges)
	{
		CrossingCars[Edge] = 0;
		const GTPWalk::FEdge& Crossing = Graph.Edges[Edge];
		const GTPWalk::FPoint& A = Crossing.Points.front();
		const GTPWalk::FPoint& B = Crossing.Points.back();
		const FVector Mid((A.X + B.X) * 0.5f, (A.Y + B.Y) * 0.5f, 0.f);
		if (FVector::DistSquared2D(Mid, PlayerLocation) > FMath::Square(RecycleDistance))
		{
			continue;
		}
		++CrossingsLookedAt;
		bool bNear = false, bDangerous = false;
		const double Reach = FMath::Square(static_cast<double>(FastCarReach + Crossing.Length));
		for (const FGTPMassTrafficCar& Car : CarsSeen)
		{
			if (FVector::DistSquared2D(Car.Location, Mid) > Reach)
			{
				continue;
			}
			const FVector Forward = Car.Forward.GetSafeNormal2D();
			if (Forward.IsNearlyZero())
			{
				continue;
			}
			GTPWalk::LookAtCar(Crossing, GTPWalk::FPoint{static_cast<float>(Car.Location.X), static_cast<float>(Car.Location.Y)},
				GTPWalk::FPoint{static_cast<float>(Forward.X), static_cast<float>(Forward.Y)}, Car.SpeedCmS, Car.RadiusCm, Settings, bNear, bDangerous);
			if (bDangerous)
			{
				break;
			}
		}
		CrossingCars[Edge] = static_cast<uint8>((bNear ? CrossingHasCarNear : 0) | (bDangerous ? CrossingHasCarDangerous : 0));
		CrossingsWithCarNear += bNear ? 1 : 0;
		CrossingsWithCarDangerous += bDangerous ? 1 : 0;
	}
	// Every 10 s: who waits at a kerb, for the gate to read that a queue of cars is let through and that nobody
	// stays there (docs/gameplay/pedestrians-mass.md).
	const double Now = World->GetTimeSeconds();
	if (Now - LastCrossingLog >= 10.0)
	{
		LastCrossingLog = Now;
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: crossings: %d waiting at the kerb (longest wait now %.0f s), %d on a crossing; since the start: most waiting at once %d, longest wait %.0f s, %d stepped off after the longest wait allowed (%.0f s); cars: %d seen%s, %d of the %d crossings around have one near (%d dangerous)"),
			WaitingAtKerb, LongestWaitNow, OnCrossing, MostWaitingAtKerb, LongestWaitEver, GaveUpWaiting, Settings.CrossingWaitMax,
			CarsSeen.Num(), bCarsToSee ? TEXT("") : TEXT(" (no MassTraffic cars to ask for)"), CrossingsWithCarNear, CrossingsLookedAt, CrossingsWithCarDangerous);
	}
}

void UGTPCrowdSubsystem::ReportCars(int32 Round, int32 Standing, int32 Inside, int32 TurnedBack, int32 GroundRefused)
{
	GoingRoundCar = Round;
	StandingAtCar = Standing;
	InsideCar = Inside;
	MostInsideCar = FMath::Max(MostInsideCar, Inside);
	TurnedBackAtCar += TurnedBack;
	GroundRefusals += GroundRefused;
}

void UGTPCrowdSubsystem::ReportGround(int32 High, float Highest, int32 Under)
{
	HighNow = High;
	MostHigh = FMath::Max(MostHigh, High);
	HighestNow = Highest;
	HighestEver = FMath::Max(HighestEver, Highest);
	UnderSomething += Under;
}

void UGTPCrowdSubsystem::ReportCrossings(int32 Waiting, int32 Crossing, float LongestWait, int32 GaveUp)
{
	WaitingAtKerb = Waiting;
	OnCrossing = Crossing;
	LongestWaitNow = LongestWait;
	MostWaitingAtKerb = FMath::Max(MostWaitingAtKerb, Waiting);
	LongestWaitEver = FMath::Max(LongestWaitEver, LongestWait);
	GaveUpWaiting += GaveUp;
}

// ---------------------------------------------------------------- processors

UGTPCrowdMoveProcessor::UGTPCrowdMoveProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	bRequiresGameThreadExecution = true;
}

void UGTPCrowdMoveProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Query.AddRequirement<FGTPCrowdWalkerFragment>(EMassFragmentAccess::ReadWrite);
	Query.AddTagRequirement<FGTPCrowdTag>(EMassFragmentPresence::All);
#if WITH_GTP_MASSTRAFFIC
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	Query.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
#endif
	Query.RegisterWithProcessor(*this);
}

void UGTPCrowdMoveProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UGTPCrowdSubsystem* Crowd = Context.GetWorld() ? Context.GetWorld()->GetSubsystem<UGTPCrowdSubsystem>() : nullptr;
	if (!Crowd)
	{
		return;
	}
	const GTPWalk::FGraph& Graph = Crowd->GetGraph();
	const GTPWalk::FSettings& Settings = Crowd->GetSettings();
	const FVector Player = Crowd->GetPlayerLocation();
	const float Recycle = Crowd->GetRecycleDistance();
	const double Now = Context.GetWorld()->GetTimeSeconds();
	int32 Waiting = 0, OnCrossing = 0, GaveUp = 0;
	float LongestWait = 0.f;
	const TArray<GTPWalk::FCarBox>& CarBoxes = Crowd->GetStandingCars();
	int32 RoundCar = 0, AtCar = 0, InCar = 0, BackAtCar = 0, GroundRefused = 0;
	int32 High = 0, UnderSomething = 0;
	float Highest = 0.f;
	const float PlayerGround = Crowd->GetPlayerGroundZ();
	Query.ForEachEntityChunk(Context, [this, Crowd, &Graph, &Settings, Player, Recycle, Now, &Waiting, &OnCrossing, &GaveUp, &LongestWait,
		&CarBoxes, &RoundCar, &AtCar, &InCar, &BackAtCar, &GroundRefused, &High, &UnderSomething, &Highest, PlayerGround](FMassExecutionContext& Chunk)
	{
		const float Dt = Chunk.GetDeltaTimeSeconds();
		const TArrayView<FGTPCrowdWalkerFragment> Walkers = Chunk.GetMutableFragmentView<FGTPCrowdWalkerFragment>();
#if WITH_GTP_MASSTRAFFIC
		// Epic's traffic obstacle: where the person is and how fast they go, for MassTraffic
		// (UMassTrafficFindObstaclesProcessor binds an obstacle to the car behind it on the nearby
		// lanes, UpdateDistanceToNearestObstacle brakes that car for it). The tag is worn on a
		// crossing only: on the sidewalk a car passing 2 m away would brake for nothing, and every
		// obstacle costs a lane search a frame. Adding or removing it moves the person to another
		// chunk at the end of the phase, once per crossing. Who waits at the kerb for a car does not
		// wear it: the car would wait for them as they wait for it.
		const TArrayView<FTransformFragment> Transforms = Chunk.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassVelocityFragment> Velocities = Chunk.GetMutableFragmentView<FMassVelocityFragment>();
		const bool bObstacleChunk = Chunk.DoesArchetypeHaveTag<FMassTrafficObstacleTag>();
		auto Publish = [&Chunk, &Graph, &Transforms, &Velocities, bObstacleChunk](int32 Index, const FGTPCrowdWalkerFragment& W, const FVector& Velocity, bool bWaitingAtKerb)
		{
			if (Transforms.IsEmpty() || Velocities.IsEmpty())
			{
				return;
			}
			const bool bOnCrossing = !bWaitingAtKerb && W.Walker.Edge >= 0 && W.Walker.Edge < static_cast<int>(Graph.Edges.size())
				&& Graph.Edges[W.Walker.Edge].Kind == GTPWalk::EKind::Crossing;
			Transforms[Index].GetMutableTransform() = FTransform(FRotator(0.f, W.Yaw, 0.f), FVector(W.Location.X, W.Location.Y, W.GroundZ));
			Velocities[Index].Value = Velocity;
			if (bOnCrossing && !bObstacleChunk)
			{
				Chunk.Defer().AddTag<FMassTrafficObstacleTag>(Chunk.GetEntity(Index));
			}
			else if (!bOnCrossing && bObstacleChunk)
			{
				Chunk.Defer().RemoveTag<FMassTrafficObstacleTag>(Chunk.GetEntity(Index));
			}
		};
#endif
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			FGTPCrowdWalkerFragment& W = Walkers[Index];
			if (W.Walker.Edge < 0)
			{
				// Not placed yet (no free walkway near the player at spawn): try again.
				if (Crowd->PlaceAroundPlayer(W.Walker, Rng))
				{
					GTPWalk::FPoint P, D;
					GTPWalk::Locate(Graph, W.Walker, P, D);
					W.Location = FVector(P.X, P.Y, 0.f);
					W.GroundZ = Crowd->GroundAt(W.Location);
				}
#if WITH_GTP_MASSTRAFFIC
				Publish(Index, W, FVector::ZeroVector, false);
#endif
				continue;
			}
			// About to step onto a crossing: is a car about to go over it, and is it this crossing's turn to walk?
			bool bCrossingUnsafe = false;
			if (Graph.IsValidEdge(W.Walker.Edge) && Graph.Edges[W.Walker.Edge].Kind == GTPWalk::EKind::Crossing && W.Walker.S < 30.f)
			{
				const uint8 Cars = Crowd->GetCrossingCars(W.Walker.Edge);
				bCrossingUnsafe = Cars != 0 && GTPWalk::CrossingUnsafe(W.Walker.Edge, Now,
					(Cars & UGTPCrowdSubsystem::CrossingHasCarNear) != 0, (Cars & UGTPCrowdSubsystem::CrossingHasCarDangerous) != 0, Settings);
			}
			// A car standing in the way: round it on the walkway, never through it; right in front of it he stands
			// (GTPWalk::AvoidBox). Not for somebody who stands for another reason (hands up, a window, a crash).
			GTPWalk::EAround Around = GTPWalk::EAround::Clear;
			if (W.Walker.Pause <= 0.f)
			{
				for (const GTPWalk::FCarBox& Car : CarBoxes)
				{
					const double Reach = static_cast<double>(Car.HalfLength) + Settings.CarLookAhead + 150.0;
					if (FMath::Square(Car.Center.X - W.Location.X) + FMath::Square(Car.Center.Y - W.Location.Y) > FMath::Square(Reach))
					{
						continue;
					}
					const bool bWasForward = W.Walker.bForward;
					const GTPWalk::EAround This = GTPWalk::AvoidBox(Graph, W.Walker, Car, Settings, Dt);
					BackAtCar += W.Walker.bForward != bWasForward ? 1 : 0;
					if (This == GTPWalk::EAround::Wait)
					{
						Around = This;
						break;
					}
					Around = static_cast<int>(This) > static_cast<int>(Around) ? This : Around;
				}
			}
			RoundCar += Around == GTPWalk::EAround::Around ? 1 : 0;
			AtCar += Around == GTPWalk::EAround::Wait ? 1 : 0;
			InCar += Around == GTPWalk::EAround::Inside ? 1 : 0;
			const float WaitedBefore = W.Walker.CrossingWait;
			const GTPWalk::FStepResult R = Around == GTPWalk::EAround::Wait ? GTPWalk::FStepResult() : GTPWalk::Step(Graph, W.Walker, Dt, Rng, Settings, bCrossingUnsafe);
			if (R.bWaitingToCross)
			{
				++Waiting;
				LongestWait = FMath::Max(LongestWait, W.Walker.CrossingWait);
				GaveUp += WaitedBefore < Settings.CrossingWaitMax && W.Walker.CrossingWait >= Settings.CrossingWaitMax ? 1 : 0;
			}
			else if (Graph.IsValidEdge(W.Walker.Edge) && Graph.Edges[W.Walker.Edge].Kind == GTPWalk::EKind::Crossing)
			{
				++OnCrossing;
			}
			GTPWalk::FPoint P, D;
			GTPWalk::Locate(Graph, W.Walker, P, D);
			W.Speed = R.Speed;
			const float Target = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			W.Yaw = FMath::RInterpTo(FRotator(0.f, W.Yaw, 0.f), FRotator(0.f, Target, 0.f), Dt, 8.f).Yaw;
			const FVector Previous = W.Location;
			W.Location = FVector(P.X, P.Y, 0.f);
			if (FVector::Dist2D(W.Location, Player) > Recycle)
			{
				if (Crowd->PlaceAroundPlayer(W.Walker, Rng))
				{
					GTPWalk::Locate(Graph, W.Walker, P, D);
					W.Location = FVector(P.X, P.Y, 0.f);
					W.GroundZ = Crowd->GroundAt(W.Location);
				}
			}
			else if (FVector::DistSquared2D(Previous, W.Location) > 2500.f || Rng.Next() < Dt * 0.5f)
			{
				// New ground height after a long step and, now and then, as the ground changes along the walk. Something
				// more than 40 cm higher than the ground 60 cm behind him is not ground (a bench, a planter, a parked
				// car that is part of the town): he does not step up onto it. Stairs and ramps rise less than that.
				// The ground is looked for from head height above his own: what hangs higher is never met, and what
				// stands on the ground under him (a bench, a bin) is left for the ground beneath it (GroundUnder).
				bool bUnder = false;
				const float Ground = Crowd->GroundUnder(W.Location, W.GroundZ, 150.f, &bUnder);
				UnderSomething += bUnder ? 1 : 0;
				if (Ground > W.GroundZ + 40.f && Ground > Crowd->GroundUnder(W.Location - FVector(D.X, D.Y, 0.f) * 60.f, W.GroundZ, 150.f) + 40.f)
				{
					++GroundRefused;
				}
				else
				{
					W.GroundZ = Ground;
				}
			}
			// For the log: somebody standing well above the player's ground near him walks in the air or on something.
			if (W.GroundZ > PlayerGround + 150.f && FVector::DistSquared2D(W.Location, Player) < FMath::Square(6000.0))
			{
				++High;
				Highest = FMath::Max(Highest, W.GroundZ - PlayerGround);
			}
#if WITH_GTP_MASSTRAFFIC
			Publish(Index, W, FVector(D.X, D.Y, 0.f).GetSafeNormal2D() * W.Speed, R.bWaitingToCross);
#endif
		}
	});
	Crowd->ReportCrossings(Waiting, OnCrossing, LongestWait, GaveUp);
	Crowd->ReportCars(RoundCar, AtCar, InCar, BackAtCar, GroundRefused);
	Crowd->ReportGround(High, Highest, UnderSomething);
}

UGTPCrowdRepresentationProcessor::UGTPCrowdRepresentationProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ExecutionOrder.ExecuteAfter.Add(UGTPCrowdMoveProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UGTPCrowdRepresentationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Query.AddRequirement<FGTPCrowdWalkerFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FGTPCrowdLookFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddTagRequirement<FGTPCrowdTag>(EMassFragmentPresence::All);
	Query.RegisterWithProcessor(*this);
}

void UGTPCrowdRepresentationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UGTPCrowdSubsystem* Crowd = Context.GetWorld() ? Context.GetWorld()->GetSubsystem<UGTPCrowdSubsystem>() : nullptr;
	UInstancedStaticMeshComponent* Instances = Crowd ? Crowd->GetInstances() : nullptr;
	if (!Instances)
	{
		return;
	}
	const float YawOffset = Crowd->GetMeshYawOffset();
	const FVector BaseScale = Crowd->GetBaseScale();
	// No stand-in near the player: a still mannequin a few metres from the camera is what the far mesh must never be
	// (video of gate 62: more people fled round the player than there were full characters).
	const FVector Player = Crowd->GetPlayerLocation();
	const double TooNear = FMath::Square(static_cast<double>(Crowd->GetStandInMinDistance()));
	Query.ForEachEntityChunk(Context, [Instances, YawOffset, BaseScale, Player, TooNear](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FGTPCrowdWalkerFragment> Walkers = Chunk.GetFragmentView<FGTPCrowdWalkerFragment>();
		const TConstArrayView<FGTPCrowdLookFragment> Looks = Chunk.GetFragmentView<FGTPCrowdLookFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			const FGTPCrowdWalkerFragment& W = Walkers[Index];
			const FGTPCrowdLookFragment& L = Looks[Index];
			if (L.InstanceIndex == INDEX_NONE || W.Walker.Edge < 0)
			{
				continue;
			}
			const FTransform Transform = L.bNear || FVector::DistSquared2D(W.Location, Player) < TooNear
				? FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -100000.f), FVector(0.01f))
				: FTransform(FRotator(0.f, W.Yaw + YawOffset, 0.f), FVector(W.Location.X, W.Location.Y, W.GroundZ), BaseScale * L.Scale);
			Instances->UpdateInstanceTransform(L.InstanceIndex, Transform, true, false, true);
		}
	});
}

// ---------------------------------------------------------------- near people (full characters)

FGTPCrowdWalkerFragment* UGTPCrowdSubsystem::WalkerOf(int32 EntityIndex)
{
	UMassEntitySubsystem* Mass = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Mass || !Entities.IsValidIndex(EntityIndex))
	{
		return nullptr;
	}
	return Mass->GetMutableEntityManager().GetFragmentDataPtr<FGTPCrowdWalkerFragment>(Entities[EntityIndex]);
}

FGTPCrowdLookFragment* UGTPCrowdSubsystem::LookOf(int32 EntityIndex)
{
	UMassEntitySubsystem* Mass = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Mass || !Entities.IsValidIndex(EntityIndex))
	{
		return nullptr;
	}
	return Mass->GetMutableEntityManager().GetFragmentDataPtr<FGTPCrowdLookFragment>(Entities[EntityIndex]);
}

void UGTPCrowdSubsystem::SetupNearActors(UWorld& InWorld)
{
	// Assembled characters (City Sample Crowds) use the animations baked for the MetaHuman skeleton.
	{
		// On by default since candidate 43 (gate of candidate 42: game p50 11.49 ms with it, no material error);
		// -CrowdCitySample=0 brings the mannequins of before back, and so does a project without the pack.
		bool Probe = false;
		int32 CitySampleOn = 1;
		FParse::Value(FCommandLine::Get(), TEXT("CrowdCitySample="), CitySampleOn);
		if (!CrowdCharacterRoot.IsEmpty() && CitySampleOn != 0)
		{
			// Asked of the registry, not loaded: loading the male body here pulled its materials in before anything was
			// dressed (suspected of the two packaged "uncooked shader map" errors that come right at this line).
			const FString BodyPath = CrowdCharacterRoot + TEXT("/Male/NormalWeight/Meshes/m_tal_nrw_body.m_tal_nrw_body");
			Probe = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetByObjectPath(FSoftObjectPath(BodyPath)).IsValid();
		}
		const UAnimSequenceBase* BakedWalk = Probe ? Cast<UAnimSequenceBase>(CrowdWalkAnimation.TryLoad()) : nullptr;
		bAssembled = Probe && BakedWalk;
		if (bAssembled)
		{
			WalkAnimation = CrowdWalkAnimation;
			IdleAnimation = CrowdIdleAnimation;
			RunAnimation = CrowdRunAnimation;
			// The hands-up loop too, when it has been baked for this skeleton (the mannequin's own stays otherwise,
			// and the line at the end of this function says the skeletons differ).
			// Asked of the files, not of the asset registry: the registry of an editor run did not know the loop baked
			// a few minutes before (candidate 46: "ANOTHER SKELETON" with MH_A_HandsUp on the disk).
			const bool bBakedHandsUp = FPackageName::DoesPackageExist(CrowdHandsUpAnimation.GetLongPackageName());
			if (bBakedHandsUp)
			{
				HandsUpAnimation = CrowdHandsUpAnimation;
			}
			UE_LOG(LogGTPCrowd, Log, TEXT("crowd: hands-up loop baked for the City Sample skeleton (%s): %s"), *CrowdHandsUpAnimation.GetLongPackageName(),
				bBakedHandsUp ? TEXT("found") : TEXT("no such package, the mannequin's is used"));
		}
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: assembled City Sample characters %s (body %s, baked walk %s)"),
			bAssembled ? TEXT("on") : TEXT("off, mannequins"), Probe ? TEXT("found") : TEXT("missing"), BakedWalk ? TEXT("found") : TEXT("missing"));
	}
	// -CrowdOwnLoops=0 keeps the loops baked on the MetaHuman skeleton (to tell one from the other in a test).
	int32 OwnLoops = 1;
	FParse::Value(FCommandLine::Get(), TEXT("CrowdOwnLoops="), OwnLoops);
	if (bAssembled && OwnLoops != 0 && !CrowdSkeletonAnimationFolder.IsEmpty())
	{
		// The loops above are baked on the skeleton of /Game/MetaHumans, the bodies of the pack have their own (same
		// name, another asset). The same loops baked on the pack's skeleton (retarget_player_animations.py with
		// GTP_RTG_TARGET_MESH, prefix CS_) are taken when they are there: an animation on its own skeleton.
		int32 Taken = 0;
		for (FSoftObjectPath* Loop : {&WalkAnimation, &IdleAnimation, &RunAnimation, &HandsUpAnimation})
		{
			const FString Name = Loop->GetAssetName();
			const FString Own = TEXT("CS_") + (Name.StartsWith(TEXT("MH_")) ? Name.RightChop(3) : Name);
			const FString Package = CrowdSkeletonAnimationFolder / Own;
			if (FPackageName::DoesPackageExist(Package))
			{
				*Loop = FSoftObjectPath(Package + TEXT(".") + Own);
				++Taken;
			}
		}
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: loops baked on the pack's own skeleton (%s/CS_*): %d of 4 taken"), *CrowdSkeletonAnimationFolder, Taken);
	}
	Walk = Cast<UAnimSequenceBase>(WalkAnimation.TryLoad());
	Idle = Cast<UAnimSequenceBase>(IdleAnimation.TryLoad());
	Run = Cast<UAnimSequenceBase>(RunAnimation.TryLoad());
	HandsUp = Cast<UAnimSequenceBase>(HandsUpAnimation.TryLoad());
	// Fear animations baked from MoCap Online (tools/pedestrians/bake_fear_animations.py, prefix MF_), found by name.
	if (bAssembled)
	{
		FAssetRegistryModule& Reg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> Anims;
		Reg.Get().GetAssetsByPath(FName(TEXT("/Game/MetaHumans/GTP/Anims")), Anims, false);
		auto Find = [&Anims](std::initializer_list<const TCHAR*> Words) -> UAnimSequenceBase*
		{
			for (const FAssetData& A : Anims)
			{
				const FString N = A.AssetName.ToString();
				bool bAll = N.StartsWith(TEXT("MF_"));
				for (const TCHAR* W : Words)
				{
					bAll = bAll && N.Contains(W);
				}
				if (bAll)
				{
					return Cast<UAnimSequenceBase>(A.GetAsset());
				}
			}
			return nullptr;
		};
		FearRun = Find({TEXT("Scared"), TEXT("Run")});
		if (!FearRun)
		{
			FearRun = Find({TEXT("Run")});
		}
		Cower = Find({TEXT("Cower")});
		if (!Cower)
		{
			Cower = Find({TEXT("Crouch")});
		}
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: fear animations: run %s, cower %s"), FearRun ? *FearRun->GetName() : TEXT("none"), Cower ? *Cower->GetName() : TEXT("none"));
	}
	USkeletalMesh* Mesh = nullptr;
	if (const ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(&InWorld, 0)))
	{
		Mesh = Player->GetMesh() ? Player->GetMesh()->GetSkeletalMeshAsset() : nullptr;
	}
	if (bAssembled && !Mesh)
	{
		Mesh = LoadObject<USkeletalMesh>(nullptr, *(CrowdCharacterRoot + TEXT("/Male/NormalWeight/Meshes/m_tal_nrw_body.m_tal_nrw_body")));
	}
	if (!Mesh || !Walk)
	{
		UE_LOG(LogGTPCrowd, Warning, TEXT("crowd: no near characters (mesh %s, walk %s)"), Mesh ? TEXT("ok") : TEXT("none"), Walk ? TEXT("ok") : TEXT("none"));
		return;
	}
	if (!Idle)
	{
		Idle = Walk;
	}
	if (!bAssembled)
	{
		// The mannequins (-CrowdCitySample=0, or no pack): which body and which materials they really are, so a
		// material the cook failed can be told from one they do not use.
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: mannequin body %s"), *Mesh->GetPathName());
		const TArray<FSkeletalMaterial>& Slots = Mesh->GetMaterials();
		for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
		{
			const UMaterialInterface* Material = Slots[Slot].MaterialInterface;
			const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
			UE_LOG(LogGTPCrowd, Log, TEXT("crowd: mannequin material slot %d = %s (parent %s)"), Slot, Material ? *Material->GetPathName() : TEXT("none"),
				Instance && Instance->Parent ? *Instance->Parent->GetPathName() : TEXT("-"));
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// -CrowdNear=N: the number of full characters, to measure what they cost without a new build (24 until candidate 65).
	FParse::Value(FCommandLine::Get(), TEXT("CrowdNear="), NearCount);
	NearCount = FMath::Clamp(NearCount, 0, 96);
	for (int32 Slot = 0; Slot < NearCount; ++Slot)
	{
		AGTPPedestrian* Actor = InWorld.SpawnActor<AGTPPedestrian>(FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, Params);
		if (!Actor)
		{
			continue;
		}
		Actor->GetBody()->SetSkeletalMesh(Mesh);
		if (bAssembled)
		{
			DressFromCitySample(Actor);
		}
		// The mannequin faces +Y, the MetaHuman bodies of the pack face +X.
		Actor->GetBody()->SetRelativeRotation(FRotator(0.f, bAssembled ? 0.f : MeshYawOffset, 0.f));
		Actor->ApplyLook(ReactionRng.Range(1.f - HeightSpread, 1.f + HeightSpread), FLinearColor::White, {FName(TEXT("Tint"))}, ReactionRng.Range(0.92f, 1.08f));
		Actor->SetDetail(false, false, -1);
		if (UGTPThreatSubsystem::IsEnabled())
		{
			if (UGTPThreatResponseComponent* Fear = NewObject<UGTPThreatResponseComponent>(Actor, TEXT("Fear")))
			{
				const float Pick = ReactionRng.Next();
				Fear->SetTemperament(Pick < 0.45f ? EGTPTemperament::Fleeing : (Pick < 0.85f ? EGTPTemperament::Compliant : EGTPTemperament::Frozen));
				Fear->RegisterComponent();
				Fear->OnThreatState.AddWeakLambda(this, [this](AActor* Body, EGTPThreatState /*Old*/, EGTPThreatState New)
				{
					OnActorThreat(Body, static_cast<int32>(New));
				});
			}
		}
		NearActors.Add(Actor);
		NearEntity.Add(INDEX_NONE);
	}
	// An animation plays right only on its own skeleton: the mannequin's hands-up on a City Sample body is the case to see.
	const USkeleton* BodySkeleton = NearActors.Num() > 0 && NearActors[0]->GetBody() && NearActors[0]->GetBody()->GetSkeletalMeshAsset()
		? NearActors[0]->GetBody()->GetSkeletalMeshAsset()->GetSkeleton() : nullptr;
	const USkeleton* HandsSkeleton = HandsUp ? HandsUp->GetSkeleton() : nullptr;
	const USkeleton* WalkSkeleton = Walk ? Walk->GetSkeleton() : nullptr;
	// Full paths: two skeletons of the same name live in the project (/Game/MetaHumans and the City Sample pack).
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %d near characters, hands-up animation %s (skeleton %s, body skeleton %s: %s); walk %s (skeleton %s: %s)"), NearActors.Num(),
		HandsUp ? *HandsUp->GetName() : TEXT("missing"), HandsSkeleton ? *HandsSkeleton->GetPathName() : TEXT("none"),
		BodySkeleton ? *BodySkeleton->GetPathName() : TEXT("none"),
		!HandsUp ? TEXT("no hands shown") : (HandsSkeleton == BodySkeleton ? TEXT("same") : TEXT("ANOTHER SKELETON")),
		Walk ? *Walk->GetName() : TEXT("missing"), WalkSkeleton ? *WalkSkeleton->GetPathName() : TEXT("none"),
		WalkSkeleton == BodySkeleton ? TEXT("same") : TEXT("ANOTHER SKELETON"));
}

void UGTPCrowdSubsystem::SayWhatIsPlayed(const AGTPPedestrian* Actor, const UAnimSequenceBase* Asked, const TCHAR* What)
{
	// Once per kind: the animation the body really plays after the ask. An animation of another skeleton may be
	// refused by the engine, and then the body keeps the previous one (or none): the readback tells.
	if (!Actor || !Asked || SaidPlayed.Contains(FString(What)))
	{
		return;
	}
	SaidPlayed.Add(FString(What));
	const USkeletalMeshComponent* Body = Actor->GetBody();
	const UAnimSingleNodeInstance* Single = Body ? Body->GetSingleNodeInstance() : nullptr;
	const UAnimationAsset* Played = Single ? Single->GetAnimationAsset() : nullptr;
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %s asked %s, the body plays %s (%s)"), What, *Asked->GetName(), Played ? *Played->GetName() : TEXT("nothing"),
		Played == Asked ? TEXT("taken") : TEXT("REFUSED"));
	// The pose itself is read 2 s later (PoseToSay): the animation needs a few frames on screen before its bones say anything.
	PoseToSay.Add({Actor, FString(What), GetWorld() ? GetWorld()->GetTimeSeconds() + 2.0 : 0.0});
}

void UGTPCrowdSubsystem::OnLoud(const FVector& Where, float Radius, AActor* Cause)
{
	// Everybody within earshot runs from the noise, near or far.
	// Sirens and engines re-emit every frame: one pass per half second is plenty.
	const double Now = FPlatformTime::Seconds();
	// One reaction per source and half second at most, wherever the source moves to (crashes re-emit every frame in a pile-up).
	double& Last = LastLoudBySource.FindOrAdd(Cause);
	if (Now - Last < 0.5)
	{
		return;
	}
	Last = Now;
	const GTPWalk::FPoint Point{float(Where.X), float(Where.Y)};
	int32 Fled = 0;
	for (int32 Index = 0; Index < Entities.Num(); ++Index)
	{
		FGTPCrowdWalkerFragment* W = WalkerOf(Index);
		// Not the one held at gunpoint: his fear is his body's to decide (it runs when it is afraid enough); a siren
		// passing sent him running with his state still "hands up" (gate of candidate 48).
		if (W && W->Walker.Edge >= 0 && FVector::Dist2D(W->Location, Where) <= Radius
			&& !(W->Walker.PauseKind == GTPWalk::EPause::HandsUp && W->Walker.Pause > 0.f))
		{
			GTPWalk::Flee(Graph, W->Walker, Point, Settings);
			++Fled;
			// One fear told everywhere: the body of somebody who runs from a noise says "fleeing" too (it said "calm",
			// and the hold-up test chased for 90 s a calm person who ran at 16 m; gate of candidate 49).
			const int32 Slot = NearEntity.IndexOfByKey(Index);
			if (UGTPThreatResponseComponent* Fear = NearActors.IsValidIndex(Slot) && NearActors[Slot] ? NearActors[Slot]->FindComponentByClass<UGTPThreatResponseComponent>() : nullptr)
			{
				Fear->Panic();
			}
		}
	}
	if (Now - LastLoudLog > 2.0)
	{
		LastLoudLog = Now;
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: loud sound r=%.0f cm, %d of %d flee"), Radius, Fled, Entities.Num());
	}
}

void UGTPCrowdSubsystem::OnActorThreat(AActor* Body, int32 NewState)
{
	const UGTPThreatSubsystem* Threat = GetWorld() ? GetWorld()->GetSubsystem<UGTPThreatSubsystem>() : nullptr;
	const int32 Slot = NearActors.IndexOfByKey(Body);
	if (!Threat || Slot == INDEX_NONE || NearEntity[Slot] == INDEX_NONE)
	{
		return;
	}
	FGTPCrowdWalkerFragment* W = WalkerOf(NearEntity[Slot]);
	if (!W)
	{
		return;
	}
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %s threat state %d"), *Body->GetName(), NewState);
	const FVector Origin = Threat->GetOrigin();
	const GTPWalk::FPoint From{float(Origin.X), float(Origin.Y)};
	switch (static_cast<EGTPThreatState>(NewState))
	{
	case EGTPThreatState::Alarmed:
		GTPWalk::Freeze(W->Walker, From, 3.f, GTPWalk::EPause::Gawk);
		break;
	case EGTPThreatState::HandsUp:
	case EGTPThreatState::Complying:
		GTPWalk::Freeze(W->Walker, From, 60.f, GTPWalk::EPause::HandsUp);
		break;
	case EGTPThreatState::Backing:
	case EGTPThreatState::Fleeing:
		GTPWalk::Flee(Graph, W->Walker, From, Settings);
		break;
	case EGTPThreatState::Calm:
		if (W->Walker.PauseKind == GTPWalk::EPause::HandsUp)
		{
			W->Walker.Pause = 0.f;
		}
		break;
	default:
		break;
	}
}

void UGTPCrowdSubsystem::ReadPlayerCar()
{
	PlayerCar = FPlayerCar();
	const APawn* Pawn = GetWorld() ? UGameplayStatics::GetPlayerPawn(GetWorld(), 0) : nullptr;
	// A car, not the player on foot: he runs at 500 cm/s, right on this threshold, and knocked down like a car the
	// people he walked up to (candidate 49 in the editor: "walking at 500 cm/s", people upright then lying).
	if (!Pawn || Pawn->IsA<ACharacter>() || Pawn->GetVelocity().Size2D() <= 500.f)
	{
		return;
	}
	const FTransform& Transform = Pawn->GetActorTransform();
	const FVector Forward = Transform.GetUnitAxis(EAxis::X).GetSafeNormal2D();
	const UPrimitiveComponent* Hull = Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
	if (Forward.IsNearlyZero() || !Hull)
	{
		return;
	}
	// The car's own box, as for the cars standing on a walkway. Its bounds are often wider than its body: held to
	// what a car or a van can be.
	const FBox Local = Hull->CalcBounds(FTransform::Identity).GetBox();
	const FVector Extent = Local.GetExtent() * Transform.GetScale3D().GetAbs();
	PlayerCar.bValid = true;
	PlayerCar.Centre = Transform.TransformPosition(Local.GetCenter());
	PlayerCar.Forward = Forward;
	PlayerCar.HalfLength = FMath::Clamp(static_cast<float>(Extent.X), 150.f, 300.f);
	PlayerCar.HalfWidth = FMath::Clamp(static_cast<float>(Extent.Y), 70.f, 115.f);
	PlayerCar.AlongSpeed = static_cast<float>(FVector::DotProduct(Pawn->GetVelocity(), Forward));
}

bool UGTPCrowdSubsystem::PlayerCarReaches(const FVector& Person) const
{
	if (!PlayerCar.bValid)
	{
		return false;
	}
	const FVector To = Person - PlayerCar.Centre;
	if (To.SizeSquared2D() > FMath::Square(900.0))
	{
		return false;
	}
	const float Along = static_cast<float>(To.X * PlayerCar.Forward.X + To.Y * PlayerCar.Forward.Y);
	const float Side = static_cast<float>(To.Y * PlayerCar.Forward.X - To.X * PlayerCar.Forward.Y);
	return GTPImpact::CarReaches(Along, Side, PlayerCar.HalfLength, PlayerCar.HalfWidth, PlayerCar.AlongSpeed);
}

void UGTPCrowdSubsystem::HitByPlayerVehicle(int32 Slot)
{
	AGTPPedestrian* Actor = NearActors.IsValidIndex(Slot) ? NearActors[Slot].Get() : nullptr;
	if (!IsValid(Actor) || !NearEntity.IsValidIndex(Slot))
	{
		return;
	}
	FGTPCrowdWalkerFragment* W = WalkerOf(NearEntity[Slot]);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Actor || !W || !Pawn)
	{
		return;
	}
	// How hard, and what the body leaves with: the rule shared with the player run over (GTPImpactLogic.h). The
	// car's own collision then keeps pushing the body, within the bounds AGTPPedestrian holds it to.
	const FVector Velocity = Pawn->GetVelocity();
	const FVector ToCar = Pawn->GetActorLocation() - W->Location;
	GTPImpact::FCarHitInput In;
	In.CarVelocity = {static_cast<float>(Velocity.X), static_cast<float>(Velocity.Y), 0.f};
	In.ToCar = {static_cast<float>(ToCar.X), static_cast<float>(ToCar.Y), 0.f};
	const GTPImpact::FCarHitResult Hit = GTPImpact::ClassifyCarHit(In, GTPImpact::FSettings());
	// Brushed by a flank, the car is hardly coming at him: he is still shoved over, the way the car goes.
	const FVector Launch = Hit.Severity >= GTPImpact::ESeverity::Knock
		? FVector(Hit.LaunchVelocity.X, Hit.LaunchVelocity.Y, Hit.LaunchVelocity.Z)
		: Velocity.GetSafeNormal2D() * 150.0 + FVector(0.0, 0.0, 40.0);
	if (Actor->StartRagdoll(Launch, Pawn))
	{
		++CarHits;
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %s hit by the player's car at %.0f km/h (closing %.0f, severity %d): leaves at %.0f cm/s along the ground, %.0f up; he was %.0f cm from the car's centre"),
			*Actor->GetName(), Velocity.Size2D() * 0.036, Hit.RelativeKmh, static_cast<int32>(Hit.Severity), Launch.Size2D(), Launch.Z, ToCar.Size2D());
		// The ones around stop and look.
		const GTPWalk::FPoint Point{float(W->Location.X), float(W->Location.Y)};
		for (int32 Index = 0; Index < Entities.Num(); ++Index)
		{
			FGTPCrowdWalkerFragment* Other = WalkerOf(Index);
			if (Other && Other != W && Other->Walker.Edge >= 0 && FVector::Dist2D(Other->Location, W->Location) < 2500.f)
			{
				GTPWalk::Gawk(Other->Walker, Point, ReactionRng, Settings);
			}
		}
		GTPWalk::Freeze(W->Walker, Point, 10.f, GTPWalk::EPause::None);
	}
}

void UGTPCrowdSubsystem::UpdateNear(float DeltaTime)
{
	if (NearActors.Num() == 0)
	{
		return;
	}
	// A near actor destroyed under us (level change, kill volume, recycling) leaves its slot: purge it, free its person.
	for (int32 Slot = NearActors.Num() - 1; Slot >= 0; --Slot)
	{
		if (!IsValid(NearActors[Slot]))
		{
			if (NearEntity.IsValidIndex(Slot) && NearEntity[Slot] != INDEX_NONE)
			{
				if (FGTPCrowdLookFragment* Look = LookOf(NearEntity[Slot]))
				{
					Look->bNear = false;
				}
			}
			UE_LOG(LogGTPCrowd, Warning, TEXT("crowd: near actor %d was destroyed, slot dropped"), Slot);
			NearActors.RemoveAt(Slot);
			if (NearEntity.IsValidIndex(Slot))
			{
				NearEntity.RemoveAt(Slot);
			}
		}
	}
	if (NearActors.Num() == 0 || NearEntity.Num() != NearActors.Num())
	{
		return;
	}
	const FVector2D Player2D(PlayerLocation.X, PlayerLocation.Y);
	// Candidates within the radius, the closest first, one per near actor.
	TArray<TPair<float, int32>> Candidates;
	for (int32 Index = 0; Index < Entities.Num(); ++Index)
	{
		const FGTPCrowdWalkerFragment* W = WalkerOf(Index);
		if (W && W->Walker.Edge >= 0)
		{
			const float D = FVector2D::Distance(FVector2D(W->Location.X, W->Location.Y), Player2D);
			if (D <= NearRadius)
			{
				Candidates.Emplace(D, Index);
			}
		}
	}
	Candidates.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	TSet<int32> Wanted;
	for (int32 I = 0; I < FMath::Min(Candidates.Num(), NearActors.Num()); ++I)
	{
		Wanted.Add(Candidates[I].Value);
	}
	// More people near the player than full characters: those left without one, and those of them too near to be
	// shown as a stand-in (UGTPCrowdRepresentationProcessor does not draw them).
	WithoutBodyNow = FMath::Max(0, Candidates.Num() - NearActors.Num());
	NotDrawnNow = 0;
	for (int32 I = NearActors.Num(); I < Candidates.Num(); ++I)
	{
		NotDrawnNow += Candidates[I].Key < StandInMinDistance ? 1 : 0;
	}
	MostWithoutBody = FMath::Max(MostWithoutBody, WithoutBodyNow);
	MostNotDrawn = FMath::Max(MostNotDrawn, NotDrawnNow);
	// Release the actors whose person left the closest set, then give free actors to the new ones.
	for (int32 Slot = 0; Slot < NearActors.Num(); ++Slot)
	{
		if (NearEntity[Slot] != INDEX_NONE && !Wanted.Contains(NearEntity[Slot]))
		{
			if (FGTPCrowdLookFragment* Look = LookOf(NearEntity[Slot]))
			{
				Look->bNear = false;
			}
			// The person goes on without a body that feels fear: nothing would ever lower his hands (measured in the
			// code: 60 s standing, on a crossing the cars wait all that time). And the body's fear, state and death
			// stay with nobody: the next person it is given to starts calm.
			++BodiesReleased;
			if (FGTPCrowdWalkerFragment* Gone = WalkerOf(NearEntity[Slot]))
			{
				if (Gone->Walker.PauseKind == GTPWalk::EPause::HandsUp)
				{
					Gone->Walker.Pause = 0.f;
					Gone->Walker.PauseKind = GTPWalk::EPause::None;
					++HandsUpReset;
				}
			}
			if (UGTPThreatResponseComponent* Fear = NearActors[Slot]->FindComponentByClass<UGTPThreatResponseComponent>())
			{
				FearReset += Fear->GetState() != EGTPThreatState::Calm || Fear->GetFear() > 0.f ? 1 : 0;
				DeadReset += Fear->IsDead() ? 1 : 0;
				Fear->Forget();
			}
			NearActors[Slot]->ResetBody();
			NearActors[Slot]->SetDetail(false, false, -1);
			NearActors[Slot]->SetActorLocation(FVector(0.f, 0.f, -100000.f), false, nullptr, ETeleportType::TeleportPhysics);
			NearEntity[Slot] = INDEX_NONE;
		}
	}
	for (const int32 EntityIndex : Wanted)
	{
		if (NearEntity.Contains(EntityIndex))
		{
			continue;
		}
		const int32 Free = NearEntity.IndexOfByKey(INDEX_NONE);
		if (Free == INDEX_NONE)
		{
			break;
		}
		NearEntity[Free] = EntityIndex;
		if (FGTPCrowdLookFragment* Look = LookOf(EntityIndex))
		{
			Look->bNear = true;
		}
		NearActors[Free]->SetDetail(true, true, -1);
	}
	// The bones of a body 2 s after it began a kind of animation: standing or not, where the head and the hands are.
	for (int32 Index = PoseToSay.Num() - 1; Index >= 0; --Index)
	{
		const FPoseToSay Ask = PoseToSay[Index];
		if (GetWorld() && GetWorld()->GetTimeSeconds() >= Ask.When)
		{
			PoseToSay.RemoveAtSwap(Index);
			const AGTPPedestrian* Actor = Ask.Actor.Get();
			const USkeletalMeshComponent* Body = Actor ? Actor->GetBody() : nullptr;
			if (Body && Body->GetBoneIndex(TEXT("head")) != INDEX_NONE)
			{
				const FVector Root = Body->GetComponentLocation();
				const FVector Head = Body->GetSocketLocation(TEXT("head"));
				const UAnimSingleNodeInstance* Single = Body->GetSingleNodeInstance();
				const UAnimationAsset* Played = Single ? Single->GetAnimationAsset() : nullptr;
				UE_LOG(LogGTPCrowd, Log, TEXT("crowd: pose of %s 2 s after \"%s\": head %.0f above its root and %.0f beside it (%s), pelvis %.0f, hands %.0f %.0f from the head, now plays %s, rendered %.1f s ago, %.0f cm from the player"),
					*Actor->GetName(), *Ask.What, Head.Z - Root.Z, FVector::Dist2D(Head, Root), Head.Z - Root.Z > 100.f ? TEXT("upright") : TEXT("NOT UPRIGHT"),
					Body->GetSocketLocation(TEXT("pelvis")).Z - Root.Z, Body->GetSocketLocation(TEXT("hand_l")).Z - Head.Z, Body->GetSocketLocation(TEXT("hand_r")).Z - Head.Z,
					Played ? *Played->GetName() : TEXT("nothing"), GetWorld()->GetTimeSeconds() - Body->GetLastRenderTime(), FVector::Dist(Root, PlayerLocation));
			}
		}
	}
	// Every 10 s, the proof of the above: nobody stands with his hands up, without a body, more than 30 m from the player.
	if (const UWorld* World = GetWorld())
	{
		const double Now = World->GetTimeSeconds();
		if (Now - LastReleaseLog >= 10.0)
		{
			LastReleaseLog = Now;
			int32 FrozenFar = 0;
			for (int32 Index = 0; Index < Entities.Num(); ++Index)
			{
				const FGTPCrowdWalkerFragment* Person = WalkerOf(Index);
				FrozenFar += Person && Person->Walker.Edge >= 0 && Person->Walker.PauseKind == GTPWalk::EPause::HandsUp && Person->Walker.Pause > 0.f
					&& !NearEntity.Contains(Index) && FVector::Dist2D(Person->Location, PlayerLocation) > 3000.f ? 1 : 0;
			}
			// And of the body put back upright: a body that lies without physics is one that fell for somebody before.
			int32 Fallen = 0, LyingStill = 0;
			for (const AGTPPedestrian* Near : NearActors)
			{
				if (Near && Near->GetBody() && Near->GetActorLocation().Z > -50000.0)
				{
					Fallen += Near->IsRagdoll() ? 1 : 0;
					LyingStill += !Near->IsRagdoll() && Near->GetBody()->GetUpVector().Z < 0.7 ? 1 : 0;
				}
			}
			UE_LOG(LogGTPCrowd, Log, TEXT("crowd: bodies released %d (hands up reset %d, fear reset %d, dead reset %d), frozen far from the player %d, held up put back standing %d, fallen %d, lying without physics %d"),
				BodiesReleased, HandsUpReset, FearReset, DeadReset, FrozenFar, HeldPutBack, Fallen, LyingStill);
			// And of the bodies thrown by the player's car: none higher than a car's roof, none left lying on a car.
			const AGTPPedestrian::FRagdollTally& Thrown = AGTPPedestrian::GetRagdollTally();
			UE_LOG(LogGTPCrowd, Log, TEXT("crowd: run over: %d people hit by the player's car, %d bodies fell; highest a pelvis went above its ground %.0f cm (bound %.0f), fastest part found %.0f cm/s, %d frames brought back within bounds, %d bodies slid off a car, %d dropped through one"),
				CarHits, Thrown.Started, Thrown.HighestCm, GTPImpact::FBodyBounds().MaxHeightCm, Thrown.FastestCmS, Thrown.Bounded, Thrown.SlidOff, Thrown.Dropped);
		}
	}
	const UGTPThreatSubsystem* HoldUp = UGTPThreatSubsystem::IsEnabled() && GetWorld() ? GetWorld()->GetSubsystem<UGTPThreatSubsystem>() : nullptr;
	// The player's car, once for everybody: its box and its speed, to know whom it reaches this frame.
	ReadPlayerCar();
	// Follow the entities: position, and the animation that matches what the person does.
	int32 ShownGrooms = 0;
	for (int32 Slot = 0; Slot < NearActors.Num(); ++Slot)
	{
		AGTPPedestrian* Actor = NearActors[Slot];
		const int32 EntityIndex = NearEntity[Slot];
		FGTPCrowdWalkerFragment* W = EntityIndex != INDEX_NONE ? WalkerOf(EntityIndex) : nullptr;
		if (!Actor || !W)
		{
			continue;
		}
		if (Actor->IsRagdoll())
		{
			// Run over: the person stays where his body lies, down, until the body is given to somebody else. He used to
			// walk on unseen after 10 s while the body stayed on the road, and his state could go "hands up" under a gun.
			W->Walker.Pause = FMath::Max(W->Walker.Pause, 1.f);
			W->Speed = 0.f;
			if (UGTPThreatResponseComponent* Fear = Actor->FindComponentByClass<UGTPThreatResponseComponent>())
			{
				Fear->StayDown();
			}
			continue;
		}
		if (PlayerCarReaches(W->Location))
		{
			HitByPlayerVehicle(Slot);
			continue;
		}
		// One authority for a person who has a body: the body's fear. The walker is put in line with it every frame,
		// whatever happened in between (an event missed while the body was changing hands, a panic that overrode
		// the pause): held up = standing with the hands up, fleeing = running, neither = no hands-up pause left.
		if (const UGTPThreatResponseComponent* Fear = HoldUp ? Actor->FindComponentByClass<UGTPThreatResponseComponent>() : nullptr)
		{
			const EGTPThreatState State = Fear->GetState();
			const bool bHeld = State == EGTPThreatState::HandsUp || State == EGTPThreatState::Complying;
			const bool bStands = W->Walker.PauseKind == GTPWalk::EPause::HandsUp && W->Walker.Pause > 0.f && W->Walker.FleeTimeLeft <= 0.f;
			// Away from the gun when there is one; from what he already ran from otherwise (a noise).
			const FVector Origin = HoldUp->GetOrigin();
			const GTPWalk::FPoint From = HoldUp->HasThreat() ? GTPWalk::FPoint{float(Origin.X), float(Origin.Y)} : W->Walker.Threat;
			if (bHeld && !bStands)
			{
				GTPWalk::Freeze(W->Walker, From, 60.f, GTPWalk::EPause::HandsUp);
				++HeldPutBack;
			}
			else if (!bHeld && W->Walker.PauseKind == GTPWalk::EPause::HandsUp)
			{
				W->Walker.Pause = 0.f;
				W->Walker.PauseKind = GTPWalk::EPause::None;
			}
			else if ((State == EGTPThreatState::Fleeing || State == EGTPThreatState::Backing) && W->Walker.FleeTimeLeft <= 0.f)
			{
				GTPWalk::Flee(Graph, W->Walker, From, Settings);
			}
			if (bHeld)
			{
				// Standing since this very frame: the pose must not wait for the walker's next step.
				W->Speed = 0.f;
			}
		}
		Actor->SetActorLocationAndRotation(FVector(W->Location.X, W->Location.Y, W->GroundZ), FRotator(0.f, W->Yaw, 0.f), false, nullptr, ETeleportType::None);
		// The economy of the legacy pedestrians and of the AI cars, which the near characters of the crowd never had:
		// beyond NearDetailDistance no shadow and a coarser level, and a pose refreshed less often with distance (the
		// garments and the face follow the body's pose, so they are refreshed at the body's rate).
		const float PlayerDistance = FVector::Dist2D(W->Location, PlayerLocation);
		const bool bDetail = NearDetailDistance <= 0.f || PlayerDistance <= NearDetailDistance;
		const USkeletalMesh* BodyMesh = Actor->GetBody() ? Actor->GetBody()->GetSkeletalMeshAsset() : nullptr;
		const int32 FarLevel = FMath::Clamp(NearFarLOD, 0, FMath::Max(0, (BodyMesh ? BodyMesh->GetLODNum() : 1) - 1));
		Actor->SetDetail(true, bDetail, bDetail ? -1 : FarLevel);
		// A groom only near the player (its tick is its cost); further away the far hair mesh and the painted scalp.
		ShownGrooms += Actor->SetHairNear(bGrooms && PlayerDistance <= CrowdGroomDistance) ? 1 : 0;
		if (bNearReduceAnimRate)
		{
			Actor->SetAnimInterval(PlayerDistance < 1500.f ? 0.f : (PlayerDistance < 2500.f ? 0.05f : 0.083f));
		}
		const bool bMoving = W->Speed > 1.f;
		if (!bMoving)
		{
			const bool bHandsUp = W->Walker.PauseKind == GTPWalk::EPause::HandsUp && HandsUp;
			const bool bGawk = W->Walker.PauseKind == GTPWalk::EPause::Gawk && Cower;
			UAnimSequenceBase* Still = bHandsUp ? HandsUp.Get() : (bGawk ? Cower.Get() : Idle.Get());
			Actor->PlayLoop(Still, 1.f);
			SayWhatIsPlayed(Actor, Still, bHandsUp ? TEXT("hands up") : (bGawk ? TEXT("cower") : TEXT("idle")));
		}
		else if (W->Walker.FleeTimeLeft > 0.f && Run)
		{
			Actor->PlayLoop(FearRun ? FearRun.Get() : Run.Get(), W->Speed / RunAnimationSpeed);
		}
		else
		{
			Actor->PlayLoop(Walk, W->Speed / WalkAnimationSpeed);
			SayWhatIsPlayed(Actor, Walk, TEXT("walk"));
		}
	}
	GroomsShown = ShownGrooms;
	MostGroomsShown = FMath::Max(MostGroomsShown, ShownGrooms);
}

void UGTPCrowdSubsystem::SpreadPanic()
{
	// Each person who starts running passes the panic on once to the quiet people around, a few steps at most
	// (same rule as the legacy pedestrians: GTPWalk::CatchPanic).
	PanicSpread.SetNumZeroed(Entities.Num());
	for (int32 Index = 0; Index < Entities.Num(); ++Index)
	{
		FGTPCrowdWalkerFragment* W = WalkerOf(Index);
		if (!W || W->Walker.Edge < 0)
		{
			continue;
		}
		if (W->Walker.FleeTimeLeft <= 0.f)
		{
			PanicSpread[Index] = 0;
			continue;
		}
		if (PanicSpread[Index])
		{
			continue;
		}
		PanicSpread[Index] = 1;
		const GTPWalk::FPoint Source{float(W->Location.X), float(W->Location.Y)};
		for (int32 Other = 0; Other < Entities.Num(); ++Other)
		{
			FGTPCrowdWalkerFragment* O = Other != Index ? WalkerOf(Other) : nullptr;
			if (O && O->Walker.Edge >= 0 && FVector::Dist2D(O->Location, W->Location) <= Settings.PanicRadius
				&& !(O->Walker.PauseKind == GTPWalk::EPause::HandsUp && O->Walker.Pause > 0.f))
			{
				if (GTPWalk::CatchPanic(Graph, O->Walker, W->Walker.Generation, Source, Settings, ReactionRng))
				{
					const int32 Slot = NearEntity.IndexOfByKey(Other);
					if (UGTPThreatResponseComponent* Fear = NearActors.IsValidIndex(Slot) && NearActors[Slot] ? NearActors[Slot]->FindComponentByClass<UGTPThreatResponseComponent>() : nullptr)
					{
						Fear->Panic();
					}
				}
			}
		}
	}
}

void UGTPCrowdSubsystem::FindHair()
{
	bHairLooked = true;
	if (CrowdCharacterRoot.IsEmpty())
	{
		return;
	}
	// What the project holds of City Sample's hair, asked of the registry (nothing is loaded here): the grooms of its
	// heads (Hair_<size>_<style>) with their cards (Hair_<size>_<style>_CardsMesh_…), the bindings Epic made of each
	// groom for each face (GB_<face mesh>_<groom>), the hair meshes it shows from a distance (…_Helmet_LOD5 or
	// …_Mesh_LOD5), the baked textures its head material paints on the scalp (…_baked) and the baked eyebrows. The
	// styles of City Sample's female heads go to the women. Classes by name: HairStrands is not a dependency.
	static const TCHAR* const FemaleStyles[] = {TEXT("Coil"), TEXT("Pixie"), TEXT("LowPonytail"), TEXT("Updo"), TEXT("BobCurly"), TEXT("Ponytail")};
	TArray<FAssetData> Found;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPath(FName(*CrowdCharacterRoot), Found, true);
	const FTopLevelAssetPath FixedMeshClass = UStaticMesh::StaticClass()->GetClassPathName();
	const FTopLevelAssetPath SkinnedMeshClass = USkeletalMesh::StaticClass()->GetClassPathName();
	int32 Kept = 0;
	for (const FAssetData& Asset : Found)
	{
		const FString Name = Asset.AssetName.ToString();
		const FString ClassName = Asset.AssetClassPath.GetAssetName().ToString();
		if (ClassName == TEXT("GroomBindingAsset"))
		{
			HairBindings.Add(Name, Asset.GetSoftObjectPath());
			continue;
		}
		const bool bBaked = Name.EndsWith(TEXT("_baked"));
		if (bBaked && Name.StartsWith(TEXT("Eyebrows_")))
		{
			EyebrowTextures.Add(Asset.GetSoftObjectPath());
			continue;
		}
		// Hair_S_AfroFade, Hair_S_AfroFade_baked, Hair_S_AfroFade_Helmet_LOD5: the style is the third word.
		TArray<FString> Words;
		Name.ParseIntoArray(Words, TEXT("_"));
		const bool bGroom = ClassName == TEXT("GroomAsset") && Words.Num() == 3;
		const bool bCards = Name.Contains(TEXT("_CardsMesh_"));
		const bool bMesh = !bCards && Name.EndsWith(TEXT("_LOD5")) && (Asset.AssetClassPath == FixedMeshClass || Asset.AssetClassPath == SkinnedMeshClass);
		if (!Name.StartsWith(TEXT("Hair_")) || Words.Num() < 3 || (!bBaked && !bMesh && !bGroom && !bCards))
		{
			continue;
		}
		const FString Style = Words[2];
		if (!CrowdHairKeptForPlayer.IsEmpty() && Style.Contains(CrowdHairKeptForPlayer))
		{
			++Kept;
			continue;
		}
		bool bFemale = false;
		for (const TCHAR* Female : FemaleStyles)
		{
			bFemale |= Style.Equals(Female, ESearchCase::IgnoreCase);
		}
		TArray<FHairStyle>& Styles = HairStyles[bFemale ? 1 : 0];
		FHairStyle* Entry = Styles.FindByPredicate([&Style](const FHairStyle& Other) { return Other.Style == Style; });
		if (!Entry)
		{
			Entry = &Styles.AddDefaulted_GetRef();
			Entry->Style = Style;
			Entry->GroomName = FString::Printf(TEXT("%s_%s_%s"), *Words[0], *Words[1], *Words[2]);
		}
		if (bGroom)
		{
			Entry->Groom = Asset.GetSoftObjectPath();
		}
		else if (bCards)
		{
			Entry->bHasCards = true;
		}
		else if (bBaked)
		{
			Entry->Baked = Asset.GetSoftObjectPath();
		}
		else
		{
			Entry->Mesh = Asset.GetSoftObjectPath();
			Entry->bMeshIsSkinned = Asset.AssetClassPath == SkinnedMeshClass;
		}
	}
	for (int32 Sex = 0; Sex < 2; ++Sex)
	{
		FString List;
		for (const FHairStyle& Style : HairStyles[Sex])
		{
			int32 Bindings = 0;
			for (const TPair<FString, FSoftObjectPath>& Binding : HairBindings)
			{
				Bindings += Binding.Key.EndsWith(TEXT("_") + Style.GroomName) ? 1 : 0;
			}
			List += FString::Printf(TEXT("%s%s (groom %s, cards %s, bindings %d, far mesh %s, painted %s)"), List.IsEmpty() ? TEXT("") : TEXT(", "), *Style.Style,
				Style.Groom.IsValid() ? TEXT("yes") : TEXT("no"), Style.bHasCards ? TEXT("yes") : TEXT("no"), Bindings,
				Style.Mesh.IsValid() ? (Style.bMeshIsSkinned ? TEXT("skinned") : TEXT("yes")) : TEXT("no"), Style.Baked.IsValid() ? TEXT("yes") : TEXT("no"));
		}
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd hair: %s: %d styles in the project under %s: %s"), Sex == 1 ? TEXT("women") : TEXT("men"), HairStyles[Sex].Num(), *CrowdCharacterRoot,
			List.IsEmpty() ? TEXT("NONE, they stay bald") : *List);
	}
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd hair: %d groom bindings and %d baked eyebrows in the project; %d assets left to the player (style \"%s\"), %d assets looked at; grooms %s, shown within %.0f m of the player"),
		HairBindings.Num(), EyebrowTextures.Num(), Kept, *CrowdHairKeptForPlayer, Found.Num(),
		!bGrooms ? TEXT("off (-CrowdGrooms=0)") : (bCrowdGroomCards ? TEXT("as hair cards, only those that have cards") : TEXT("as the asset's own levels")), CrowdGroomDistance / 100.f);
}

void UGTPCrowdSubsystem::GiveHair(AGTPPedestrian* Actor, bool bFemale)
{
	if (!bHairLooked)
	{
		FindHair();
	}
	USkeletalMeshComponent* Face = Actor ? Actor->GetFace() : nullptr;
	if (!Face)
	{
		return;
	}
	const FString FaceName = Face->GetSkeletalMeshAsset() ? Face->GetSkeletalMeshAsset()->GetName() : FString();
	// The colour first, for everybody: City Sample's hair and head materials tint the hair and the eyebrows from a
	// melanin and a redness (each head has its own; here one per person, dark more often than fair).
	const float Roll = ReactionRng.Next();
	const float Melanin = Roll < 0.55f ? ReactionRng.Range(0.8f, 1.f) : (Roll < 0.88f ? ReactionRng.Range(0.45f, 0.8f) : ReactionRng.Range(0.15f, 0.45f));
	const float Redness = ReactionRng.Range(0.f, 0.45f);
	// The styles this person can wear: one with a groom bound to his face, a far mesh or a painted scalp.
	auto BindingOf = [this, &FaceName](const FHairStyle& Of) -> const FSoftObjectPath*
	{
		return bGrooms && (Of.bHasCards || !bCrowdGroomCards) ? HairBindings.Find(FString::Printf(TEXT("GB_%s_%s"), *FaceName, *Of.GroomName)) : nullptr;
	};
	TArray<const FHairStyle*> Usable;
	for (const FHairStyle& Candidate : HairStyles[bFemale ? 1 : 0])
	{
		if (Candidate.Baked.IsValid() || Candidate.Mesh.IsValid() || BindingOf(Candidate))
		{
			Usable.Add(&Candidate);
		}
	}
	const bool bBald = Usable.IsEmpty() || (!bFemale && ReactionRng.Next() < CrowdBaldShare);
	const FHairStyle* Style = bBald ? nullptr : Usable[FMath::Min(Usable.Num() - 1, static_cast<int32>(ReactionRng.Next() * Usable.Num()))];
	UTexture* Baked = Style ? Cast<UTexture>(Style->Baked.TryLoad()) : nullptr;
	UTexture* Brows = EyebrowTextures.Num() > 0
		? Cast<UTexture>(EyebrowTextures[FMath::Min(EyebrowTextures.Num() - 1, static_cast<int32>(ReactionRng.Next() * EyebrowTextures.Num()))].TryLoad()) : nullptr;
	// Painted on the face: every material of it that has the head material's HairAttributeMap takes the baked hair
	// of the style (the scalp under the hair, and all there is of it from far) and baked eyebrows.
	static const FName HairMap(TEXT("HairAttributeMap"));
	static const FName BrowMap(TEXT("EyebrowsAttributeMap"));
	bool bHeadMaterial = false, bPainted = false, bBrows = false;
	for (int32 Slot = 0; Slot < Face->GetNumMaterials(); ++Slot)
	{
		UMaterialInterface* Material = Face->GetMaterial(Slot);
		UTexture* Current = nullptr;
		if (!Material || !Material->GetTextureParameterValue(FHashedMaterialParameterInfo(HairMap), Current))
		{
			continue;
		}
		bHeadMaterial = true;
		UMaterialInstanceDynamic* Head = Face->CreateDynamicMaterialInstance(Slot, Material);
		if (!Head)
		{
			continue;
		}
		for (const TCHAR* Name : {TEXT("HairMelanin"), TEXT("EyebrowsMelanin"), TEXT("BeardMelanin"), TEXT("MustacheMelanin")})
		{
			Head->SetScalarParameterValue(Name, Melanin);
		}
		for (const TCHAR* Name : {TEXT("HairRedness"), TEXT("EyebrowsRedness"), TEXT("BeardRedness"), TEXT("MustacheRedness")})
		{
			Head->SetScalarParameterValue(Name, Redness);
		}
		// A virtual texture where the material samples an ordinary one (or the reverse) would not draw: left alone, said.
		const bool bSameKind = !Baked || !Current || Baked->IsCurrentlyVirtualTextured() == Current->IsCurrentlyVirtualTextured();
		if (Baked && bSameKind)
		{
			Head->SetTextureParameterValue(HairMap, Baked);
			bPainted = true;
		}
		UTexture* CurrentBrows = nullptr;
		const bool bBrowsHere = Brows && Material->GetTextureParameterValue(FHashedMaterialParameterInfo(BrowMap), CurrentBrows)
			&& (!CurrentBrows || CurrentBrows->IsCurrentlyVirtualTextured() == Brows->IsCurrentlyVirtualTextured());
		if (bBrowsHere)
		{
			Head->SetTextureParameterValue(BrowMap, Brows);
			bBrows = true;
		}
		if ((Baked && bSameKind) || bBrowsHere)
		{
			Head->SetScalarParameterValue(TEXT("ShowBakedGroomTextures"), 1.f);
		}
		const FString Key = Material->GetName() + TEXT("|") + (Style ? Style->Style : FString(TEXT("bald")));
		if (!HairSaid.Contains(Key) && HairSaid.Num() < 40)
		{
			HairSaid.Add(Key);
			float Show = -1.f;
			const bool bShowScalar = Material->GetScalarParameterValue(FHashedMaterialParameterInfo(FName(TEXT("ShowBakedGroomTextures"))), Show);
			UE_LOG(LogGTPCrowd, Log, TEXT("crowd hair: face slot %d %s: HairAttributeMap was %s (%s), %s; eyebrows %s; ShowBakedGroomTextures %s; melanin %.2f redness %.2f"), Slot, *Material->GetName(),
				Current ? *Current->GetName() : TEXT("none"), Current && Current->IsCurrentlyVirtualTextured() ? TEXT("virtual") : TEXT("not virtual"),
				!Baked ? TEXT("left (no baked texture for this person)") : (bSameKind ? *FString::Printf(TEXT("set to %s"), *Baked->GetName()) : *FString::Printf(TEXT("NOT set to %s: not the same kind of texture"), *Baked->GetName())),
				bBrowsHere ? *FString::Printf(TEXT("set to %s"), *Brows->GetName()) : TEXT("left"),
				bShowScalar ? *FString::Printf(TEXT("is a scalar, was %.0f"), Show) : TEXT("is not a scalar of this material"), Melanin, Redness);
		}
	}
	// The colour of a hair component (groom or mesh): the scalars of its materials named after melanin or redness.
	// The names differ from one hair material to the other: those the material has are set, and said once.
	auto Tint = [this, Melanin, Redness](UMeshComponent* Hair, const FString& What)
	{
		for (int32 Slot = 0; Hair && Slot < Hair->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* Material = Hair->GetMaterial(Slot);
			UMaterialInstanceDynamic* Dynamic = Material ? Hair->CreateDynamicMaterialInstance(Slot, Material) : nullptr;
			if (!Dynamic)
			{
				continue;
			}
			TArray<FMaterialParameterInfo> Scalars;
			TArray<FGuid> Ids;
			Material->GetAllScalarParameterInfo(Scalars, Ids);
			FString Names;
			for (const FMaterialParameterInfo& Info : Scalars)
			{
				const FString Name = Info.Name.ToString();
				Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Name;
				if (Name.Contains(TEXT("Melanin")))
				{
					Dynamic->SetScalarParameterValue(Info.Name, Melanin);
				}
				else if (Name.Contains(TEXT("Redness")))
				{
					Dynamic->SetScalarParameterValue(Info.Name, Redness);
				}
			}
			const FString Key = TEXT("tint|") + Material->GetName();
			if (!HairSaid.Contains(Key) && HairSaid.Num() < 40)
			{
				HairSaid.Add(Key);
				UE_LOG(LogGTPCrowd, Log, TEXT("crowd hair: %s slot %d material %s, its scalars: %s"), *What, Slot, *Material->GetPathName(), Names.IsEmpty() ? TEXT("none") : *Names);
			}
		}
	};
	// Near the player: the groom of the style, with the binding Epic made of it for this face.
	UMeshComponent* Groom = nullptr;
	if (const FSoftObjectPath* BindingPath = Style ? BindingOf(*Style) : nullptr)
	{
		UObject* Binding = BindingPath->TryLoad();
		// The groom the binding was made for, read from the binding: two heads of the pack can have a groom of the
		// same name, and a binding holds only on its own.
		UObject* Asset = nullptr;
		if (const FObjectPropertyBase* Made = Binding ? FindFProperty<FObjectPropertyBase>(Binding->GetClass(), TEXT("Groom")) : nullptr)
		{
			Asset = Made->GetObjectPropertyValue_InContainer(Binding);
		}
		const bool bFromBinding = Asset != nullptr;
		if (!Asset)
		{
			Asset = Style->Groom.TryLoad();
		}
		FString Said = Binding ? TEXT("no groom asset") : TEXT("the binding did not load");
		Groom = Binding && Asset ? Actor->SetGroom(Asset, Binding, bCrowdGroomCards, Said) : nullptr;
		GroomsNotMade += Groom ? 0 : 1;
		const FString Key = TEXT("groom|") + BindingPath->GetAssetName();
		if (!HairSaid.Contains(Key) && HairSaid.Num() < 40)
		{
			HairSaid.Add(Key);
			UE_LOG(LogGTPCrowd, Log, TEXT("crowd hair: face %s, style %s: %s (%s)%s"), *FaceName, *Style->Style, *Said,
				bFromBinding ? TEXT("the groom the binding names") : TEXT("the groom found by its name: the binding's Groom was not read"), Groom ? TEXT("") : TEXT(": NO GROOM on this head"));
		}
		Tint(Groom, TEXT("groom ") + Style->GroomName);
	}
	// Further away: the hair mesh of the style, when the project has it.
	UMeshComponent* Hair = nullptr;
	if (Style && Style->Mesh.IsValid())
	{
		UObject* Mesh = Style->Mesh.TryLoad();
		Hair = Actor->SetHair(Cast<UStaticMesh>(Mesh), Cast<USkeletalMesh>(Mesh));
		Tint(Hair, FString::Printf(TEXT("far mesh %s (%s)"), *Style->Mesh.GetAssetName(), Style->bMeshIsSkinned ? TEXT("skinned") : TEXT("fixed to the head bone")));
	}
	NoHeadMaterial += bHeadMaterial ? 0 : 1;
	HairPainted += bPainted ? 1 : 0;
	BrowsPainted += bBrows ? 1 : 0;
	HairMeshes += Hair ? 1 : 0;
	HairGrooms += Groom ? 1 : 0;
	LeftBald += bPainted || Hair || Groom ? 0 : 1;
	if (!bHeadMaterial && !HairSaid.Contains(TEXT("no head material")))
	{
		HairSaid.Add(TEXT("no head material"));
		FString Names;
		for (int32 Slot = 0; Slot < Face->GetNumMaterials(); ++Slot)
		{
			Names += FString::Printf(TEXT("%s%d %s"), Names.IsEmpty() ? TEXT("") : TEXT(", "), Slot, Face->GetMaterial(Slot) ? *Face->GetMaterial(Slot)->GetName() : TEXT("none"));
		}
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd hair: NO material of the face %s has a HairAttributeMap texture: %s"), *FaceName, *Names);
	}
}

bool UGTPCrowdSubsystem::DressFromCitySample(AGTPPedestrian* Actor)
{
	// Path pattern of the pack: <Root>/<Female|Male>/<NormalWeight|OverWeight|UnderWeight>/Meshes/<f|m>_tal_<nrw|ovw|unw>_<part>.
	// -CrowdSex=f|m forces one sex (diagnostic of the packaged material errors).
	FString ForcedSex;
	FParse::Value(FCommandLine::Get(), TEXT("CrowdSex="), ForcedSex);
	const bool bFemaleRoll = ReactionRng.Next() < 0.5f;
	const bool bFemale = ForcedSex.StartsWith(TEXT("f")) ? true : (ForcedSex.StartsWith(TEXT("m")) ? false : bFemaleRoll);
	// Only the normal-weight body is in the migrated subset (the clothes of the other weights do not fit it anyway).
	const TCHAR* WeightDir = TEXT("NormalWeight");
	const TCHAR* WeightTag = TEXT("nrw");
	const FString Sex = bFemale ? TEXT("Female") : TEXT("Male");
	const FString Prefix = bFemale ? TEXT("f") : TEXT("m");
	auto Pick = [this](const TArray<FString>& Options) -> FString
	{
		return Options[FMath::Min(Options.Num() - 1, static_cast<int32>(ReactionRng.Next() * Options.Num()))];
	};
	auto Load = [&](const FString& Part) -> USkeletalMesh*
	{
		const FString Name = FString::Printf(TEXT("%s_tal_%s_%s"), *Prefix, WeightTag, *Part);
		return LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/%s/%s/Meshes/%s.%s"), *CrowdCharacterRoot, *Sex, WeightDir, *Name, *Name));
	};
	// A diagnostic switch is on as "-Name" or "-Name=1" (FParse::Param alone does not see the second form: the gate
	// of candidate 40 ran "-CrowdBodyFromPlayer=1" and the flag stayed off).
	auto Flag = [](const TCHAR* Name) -> bool
	{
		int32 Value = 0;
		return FParse::Param(FCommandLine::Get(), Name) || (FParse::Value(FCommandLine::Get(), *(FString(Name) + TEXT("=")), Value) && Value != 0);
	};
	// Diagnostics for the packaged "uncooked shader map" errors: -CrowdParts=<mask> (1 body, 2 garments, 4 face; 7 = all),
	// -CrowdNoFace. A part left out is NOT LOADED: the errors come when a mesh's materials load, not when it is shown
	// (the first version of these flags loaded everything and only hid the part, so it told nothing).
	int32 PartsMask = 7;
	FParse::Value(FCommandLine::Get(), TEXT("CrowdParts="), PartsMask);
	const bool bWantClothes = (PartsMask & 2) != 0;
	const bool bWantFace = (PartsMask & 4) != 0 && !Flag(TEXT("CrowdNoFace"));
	// -CrowdBodyFromPlayer=1 (diagnostic): keep the body mesh the actor already has (the player's) instead of loading the
	// pack's body, to tell whether the packaged material errors come from the pack's M_BodySynthesized chain.
	const bool bBodyFromPlayer = Flag(TEXT("CrowdBodyFromPlayer"));
	USkeletalMesh* Body = bBodyFromPlayer ? nullptr : Load(TEXT("body"));
	if (!Body && !bBodyFromPlayer)
	{
		return false;
	}
	// A garment that is not in the subset falls back to the next one of its list, so nobody walks out half dressed.
	auto LoadAny = [&](TArray<FString> Options) -> USkeletalMesh*
	{
		const int32 Start = FMath::Min(Options.Num() - 1, static_cast<int32>(ReactionRng.Next() * Options.Num()));
		for (int32 I = 0; I < Options.Num(); ++I)
		{
			if (USkeletalMesh* Found = Load(Options[(Start + I) % Options.Num()]))
			{
				return Found;
			}
		}
		return nullptr;
	};
	TArray<USkeletalMesh*> Clothes;
	if (bWantClothes)
	{
		Clothes.Add(LoadAny((bFemale
			? TArray<FString>{TEXT("buttonDown"), TEXT("buttonOpen"), TEXT("scoopneck"), TEXT("turtleneck"), TEXT("buttonDown_blazer"), TEXT("scoopneck_croppedJacket")}
			: TArray<FString>{TEXT("buttonDown"), TEXT("buttonOpen"), TEXT("crewneck"), TEXT("turtleneck"), TEXT("buttonDown_tie"), TEXT("crewneck_blazer")})));
		Clothes.Add(LoadAny((bFemale
			? TArray<FString>{TEXT("jeans"), TEXT("slacks"), TEXT("skirt"), TEXT("jeans_belt")}
			: TArray<FString>{TEXT("jeans"), TEXT("jeansLong"), TEXT("slacks"), TEXT("slacks_belt")})));
		Clothes.Add(LoadAny((bFemale
			? TArray<FString>{TEXT("loafers"), TEXT("oxfords"), TEXT("dressFlats")}
			: TArray<FString>{TEXT("loafers"), TEXT("oxfords")})));
	}
	// Heads 001 to 003 of each sex are in the migrated subset.
	const int32 FaceIndex = 1 + FMath::Min(2, static_cast<int32>(ReactionRng.Next() * 3));
	const FString FaceName = FString::Printf(TEXT("%s_%03d_nrw_FaceMesh"), *Prefix, FaceIndex);
	USkeletalMesh* Face = bWantFace ? LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/%s/%s_%03d/Face/%s.%s"),
		*CrowdCharacterRoot, *Sex, *Prefix, FaceIndex, *FaceName, *FaceName)) : nullptr;
	static bool bPartsSaid = false;
	if (!bPartsSaid)
	{
		bPartsSaid = true;
		// The physics asset is what lets a person fall when run over or shot (AGTPPedestrian::StartRagdoll refuses without).
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: parts loaded: body %s, garments %s, face %s; body physics asset %s"), bBodyFromPlayer ? TEXT("the actor's own") : TEXT("pack"),
			bWantClothes ? TEXT("yes") : TEXT("no"), bWantFace ? TEXT("yes") : TEXT("no"),
			Body ? (Body->GetPhysicsAsset() ? *Body->GetPhysicsAsset()->GetName() : TEXT("NONE: no ragdoll")) : TEXT("the actor's own"));
	}
	// The face animation blueprint (City Sample's rig) is not loaded unless -CrowdFaceAnim=1: loading it pulls its whole
	// dependency chain, the suspect of the two packaged "uncooked shader map" errors that come right before the first
	// character is dressed; without it the face follows the body by bone name.
	TSubclassOf<UAnimInstance> FaceAnim;
	if (Face && Flag(TEXT("CrowdFaceAnim")))
	{
		FaceAnim = FaceAnimClass.TryLoadClass<UAnimInstance>();
	}
	Actor->Dress(Body, Clothes, Face, FaceAnim);
	++Dressed;
	if (bWantClothes)
	{
		WithoutTop += Clothes[0] ? 0 : 1;
		WithoutBottom += Clothes[1] ? 0 : 1;
		WithoutShoes += Clothes[2] ? 0 : 1;
		WithoutShoesWomen += !Clothes[2] && bFemale ? 1 : 0;
		// Once for each sex: what the first one wears, by mesh name, and what the project lacks for him or her.
		static bool bGarmentsSaid[2] = {false, false};
		if (!bGarmentsSaid[bFemale ? 1 : 0])
		{
			bGarmentsSaid[bFemale ? 1 : 0] = true;
			UE_LOG(LogGTPCrowd, Log, TEXT("crowd: garments of the first %s: top %s, bottom %s, shoes %s (meshes under %s/%s/%s/Meshes)"), bFemale ? TEXT("woman") : TEXT("man"),
				Clothes[0] ? *Clothes[0]->GetName() : TEXT("NONE"), Clothes[1] ? *Clothes[1]->GetName() : TEXT("NONE"),
				Clothes[2] ? *Clothes[2]->GetName() : (bFemale ? TEXT("NONE: no loafers, oxfords or dressFlats mesh in the project, she walks barefoot") : TEXT("NONE: no loafers or oxfords mesh in the project, he walks barefoot")),
				*CrowdCharacterRoot, *Sex, WeightDir);
		}
	}
	WithoutFace += Face ? 0 : 1;
	if (bHair && Face)
	{
		GiveHair(Actor, bFemale);
	}
	int32 DefaultMaterials = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("CrowdDefaultMaterials="), DefaultMaterials) && DefaultMaterials != 0)
	{
		TArray<USkeletalMeshComponent*> All;
		Actor->GetComponents<USkeletalMeshComponent>(All);
		for (USkeletalMeshComponent* Comp : All)
		{
			for (int32 Slot = 0; Slot < Comp->GetNumMaterials(); ++Slot)
			{
				Comp->SetMaterial(Slot, UMaterial::GetDefaultMaterial(MD_Surface));
			}
		}
	}
	// Once: which materials the assembled character really uses (a material missing from the cook shows as an
	// uncooked shader map in the packaged build, and the log says no name).
	static bool bMaterialsListed = false;
	if (!bMaterialsListed)
	{
		bMaterialsListed = true;
		TArray<USkeletalMeshComponent*> Comps;
		Actor->GetComponents<USkeletalMeshComponent>(Comps);
		for (const USkeletalMeshComponent* Comp : Comps)
		{
			for (int32 Slot = 0; Slot < Comp->GetNumMaterials(); ++Slot)
			{
				const UMaterialInterface* Material = Comp->GetMaterial(Slot);
				const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
				UE_LOG(LogGTPCrowd, Log, TEXT("crowd material: %s slot %d = %s (parent %s)"), *Comp->GetName(), Slot,
					Material ? *Material->GetPathName() : TEXT("none"),
					Instance && Instance->Parent ? *Instance->Parent->GetPathName() : TEXT("-"));
			}
		}
	}
	return true;
}
