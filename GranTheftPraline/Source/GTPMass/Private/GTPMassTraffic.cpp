#include "GTPMassTraffic.h"

#include "GTPTraffic.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "EngineUtils.h"
#include "MassSpawner.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "UObject/UObjectIterator.h"
#include "ZoneGraphData.h"
#if WITH_GTP_MASSTRAFFIC
#include "MassCommonFragments.h"
#include "MassEntityQuery.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassTrafficFragments.h"
#include "MassTrafficSettings.h"
#include "MassTrafficSubsystem.h"
#include "MassZoneGraphNavigationFragments.h"
#include "ZoneGraphSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGTPMassTraffic, Log, All);

const FName UGTPMassTrafficSubsystem::SpawnerTag(TEXT("GTPMassTraffic"));

bool UGTPMassTrafficSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGTPMassTrafficSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!AGTPTrafficNetwork::IsMassTrafficRequested())
	{
		return;
	}
#if WITH_GTP_MASSTRAFFIC
	// Started in the order of their GTPMassTrafficOrder<n> tag (place_masstraffic.py): intersections,
	// then parked cars, then the driving cars that need both.
	TArray<TPair<int32, AMassSpawner*>> Spawners;
	const FString OrderPrefix = SpawnerTag.ToString() + TEXT("Order");
	for (TActorIterator<AMassSpawner> It(&InWorld); It; ++It)
	{
		if (!It->ActorHasTag(SpawnerTag))
		{
			continue;
		}
		int32 Order = 100;
		for (const FName& Tag : It->Tags)
		{
			const FString Text = Tag.ToString();
			if (Text.StartsWith(OrderPrefix))
			{
				Order = FCString::Atoi(*Text.RightChop(OrderPrefix.Len()));
			}
		}
		Spawners.Emplace(Order, *It);
	}
	Spawners.StableSort([](const TPair<int32, AMassSpawner*>& A, const TPair<int32, AMassSpawner*>& B) { return A.Key < B.Key; });
	if (!Spawners.IsEmpty())
	{
		// Before any car chooses a lane.
		HandleDeadEnds();
	}
	for (const TPair<int32, AMassSpawner*>& Entry : Spawners)
	{
		Entry.Value->DoSpawning();
		++StartedSpawners;
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic: started %s (order %d)"), *Entry.Value->GetName(), Entry.Key);
	}
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic: %d spawner(s) started (tag %s)"), StartedSpawners, *SpawnerTag.ToString());
	if (StartedSpawners == 0)
	{
		UE_LOG(LogGTPMassTraffic, Warning, TEXT("MassTraffic: no spawner tagged %s in the map, run tools/city/unreal/place_masstraffic.py"),
			*SpawnerTag.ToString());
	}
	LogCounts();
	InWorld.GetTimerManager().SetTimer(CountTimer, FTimerDelegate::CreateUObject(this, &UGTPMassTrafficSubsystem::LogCounts), 5.f, true);
	if (StartedSpawners > 0 && !FParse::Param(FCommandLine::Get(), TEXT("NoDeadEndRecycle")))
	{
		InWorld.GetTimerManager().SetTimer(RecycleTimer, FTimerDelegate::CreateUObject(this, &UGTPMassTrafficSubsystem::RecycleAtDeadEnds), 1.f, true);
	}
	if (StartedSpawners > 0 && !FParse::Param(FCommandLine::Get(), TEXT("NoLoopGuard")))
	{
		// Does nothing until LinkDeadEndsToEntries has linked a dead end.
		InWorld.GetTimerManager().SetTimer(LoopTimer, FTimerDelegate::CreateUObject(this, &UGTPMassTrafficSubsystem::GuardLoops), 0.25f, true);
	}
#else
	UE_LOG(LogGTPMassTraffic, Warning, TEXT("MassTraffic: -Traffic=mass but the plugin is not installed (Plugins/Traffic and Plugins/RuleProcessor from City Sample), legacy traffic kept"));
#endif
}

namespace GTPMassTrafficLocal
{
/** True for an actor of City Sample's car class (MassTraffic's display actors), found by name. */
bool IsMassCarActor(const AActor* Actor)
{
	for (const UClass* Class = Actor->GetClass(); Class; Class = Class->GetSuperClass())
	{
		if (Class->GetFName() == TEXT("CitySampleMassVehicleBase"))
		{
			return true;
		}
	}
	return false;
}
}

void UGTPMassTrafficSubsystem::LogCounts()
{
	using namespace GTPMassTrafficLocal;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	++CountLogs;
	// Every 5 s for the first minute, then every 30 s.
	if (CountLogs > 12 && CountLogs % 6 != 0)
	{
		return;
	}
	int32 GraphData = 0;
	int32 Lanes = 0;
	int32 Zones = 0;
	for (TActorIterator<AZoneGraphData> It(World); It; ++It)
	{
		++GraphData;
		Lanes += It->GetStorage().Lanes.Num();
		Zones += It->GetStorage().Zones.Num();
	}
	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	const FVector PlayerLocation = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	double Nearest = TNumericLimits<double>::Max();
	FVector NearestLocation = FVector::ZeroVector;
	auto Consider = [&](const FVector& Location)
	{
		const double Distance = FVector::Dist(Location, PlayerLocation);
		if (Distance < Nearest)
		{
			Nearest = Distance;
			NearestLocation = Location;
		}
	};
	// Far cars: instances drawn by Mass's visualizer actor.
	int32 Instances = 0;
	for (TObjectIterator<UInstancedStaticMeshComponent> It; It; ++It)
	{
		const AActor* Owner = It->GetOwner();
		if (It->GetWorld() != World || !Owner || !Owner->GetClass()->GetName().Contains(TEXT("MassVisualizer")))
		{
			continue;
		}
		const int32 Count = It->GetInstanceCount();
		Instances += Count;
		FTransform Transform;
		for (int32 Index = 0; Index < FMath::Min(Count, 200); ++Index)
		{
			if (It->GetInstanceTransform(Index, Transform, true))
			{
				Consider(Transform.GetLocation());
			}
		}
	}
	// Near cars: City Sample's car actors put in by Mass.
	int32 Actors = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (IsMassCarActor(*It))
		{
			++Actors;
			Consider(It->GetActorLocation());
		}
	}
	const bool bAny = Nearest < TNumericLimits<double>::Max();
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic count t=%.0fs: zonegraph data=%d lanes=%d zones=%d | cars: instances=%d actors=%d nearest=%s"),
		World->GetTimeSeconds(), GraphData, Lanes, Zones, Instances, Actors,
		bAny ? *FString::Printf(TEXT("%.0f m at %s"), Nearest / 100.0, *NearestLocation.ToCompactString()) : TEXT("none"));
#if WITH_GTP_MASSTRAFFIC
	// How close the network itself comes: a car cannot be nearer than the nearest road lane.
	if (const UMassTrafficSettings* Settings = GetDefault<UMassTrafficSettings>(); Settings && Player)
	{
		double NearestLane = TNumericLimits<double>::Max();
		int32 LanesWithin50 = 0;
		for (TActorIterator<AZoneGraphData> It(World); It; ++It)
		{
			const FZoneGraphStorage& Storage = It->GetStorage();
			for (const FZoneLaneData& Lane : Storage.Lanes)
			{
				if (!Settings->TrafficLaneFilter.Pass(Lane.Tags) || Settings->IntersectionLaneFilter.Pass(Lane.Tags))
				{
					continue;
				}
				double LaneDistance = TNumericLimits<double>::Max();
				for (int32 Point = Lane.PointsBegin; Point + 1 < Lane.PointsEnd; ++Point)
				{
					LaneDistance = FMath::Min(LaneDistance, FMath::PointDistToSegment(PlayerLocation, Storage.LanePoints[Point], Storage.LanePoints[Point + 1]));
				}
				NearestLane = FMath::Min(NearestLane, LaneDistance);
				LanesWithin50 += LaneDistance <= 5000.0 ? 1 : 0;
			}
		}
		if (NearestLane < TNumericLimits<double>::Max())
		{
			UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic lanes near the player: nearest road lane %.0f m, %d road lane(s) within 50 m"),
				NearestLane / 100.0, LanesWithin50);
		}
	}
	if (!Loops.IsEmpty())
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic loops: %d car(s) left the map and came back at an entry so far, %d of them in the player's sight; %d of %d entry lane(s) held closed now (in sight), %d at most"),
			LoopJumps, LoopJumpsInSight, EntriesInSight.Num(), LoopEntries, MostEntriesHeld);
	}
	LogSimulatedCars(*World, PlayerLocation);
#endif
	if (Lanes == 0 && CountLogs == 1)
	{
		UE_LOG(LogGTPMassTraffic, Warning, TEXT("MassTraffic: the map has no built ZoneGraph lanes, no car can spawn (build the ZoneGraph and save the map)"));
	}
	if (!bViewPlaced && bAny && Player && FParse::Param(FCommandLine::Get(), TEXT("MassTrafficView")))
	{
		// 12 m from the car, a little above, facing it.
		const FVector Away = (PlayerLocation - NearestLocation).GetSafeNormal2D().IsNearlyZero() ? FVector(1.0, 0.0, 0.0)
			: (PlayerLocation - NearestLocation).GetSafeNormal2D();
		const FVector Spot = NearestLocation + Away * 1200.0 + FVector(0.0, 0.0, 200.0);
		Player->TeleportTo(Spot, (NearestLocation - Spot).Rotation().GetNormalized(), false, true);
		if (AController* Controller = Player->GetController())
		{
			Controller->SetControlRotation((NearestLocation - Spot).Rotation());
		}
		bViewPlaced = true;
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic view: player put at %s, looking at the car at %s"), *Spot.ToCompactString(), *NearestLocation.ToCompactString());
	}
}

#if WITH_GTP_MASSTRAFFIC
void UGTPMassTrafficSubsystem::LogSimulatedCars(UWorld& World, const FVector& PlayerLocation)
{
	// The simulated cars themselves, drawn or not: how many there are, how many move and how fast,
	// how many near the player, how many stand at the end of a dead-end lane. Tells sparse traffic
	// (few cars for many lanes) from frozen traffic (cars waiting at intersections that never open,
	// or gathered at dead ends: MassTraffic expects a road network with none, its lane choice says
	// "Should never happen").
	UMassEntitySubsystem* Mass = World.GetSubsystem<UMassEntitySubsystem>();
	if (!Mass)
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic cars: no Mass entity subsystem"));
		return;
	}
	// Car lanes (MassTraffic's lane filter) and those of them with no lane after them.
	const UMassTrafficSettings* Settings = GetDefault<UMassTrafficSettings>();
	TSet<int32> DeadEnds;
	FZoneGraphDataHandle DataHandle;
	double Length = 0.0;
	int32 CarLanes = 0;
	for (TActorIterator<AZoneGraphData> It(&World); It && Settings; ++It)
	{
		const FZoneGraphStorage& Storage = It->GetStorage();
		DataHandle = Storage.DataHandle;
		for (int32 LaneIndex = 0; LaneIndex < Storage.Lanes.Num(); ++LaneIndex)
		{
			const FZoneLaneData& Lane = Storage.Lanes[LaneIndex];
			if (!Settings->TrafficLaneFilter.Pass(Lane.Tags))
			{
				continue;
			}
			++CarLanes;
			if (Lane.PointsEnd > Lane.PointsBegin && Storage.LanePointProgressions.IsValidIndex(Lane.PointsEnd - 1))
			{
				Length += Storage.LanePointProgressions[Lane.PointsEnd - 1];
			}
			bool bOutgoing = false;
			for (int32 Link = Lane.LinksBegin; Link < Lane.LinksEnd && !bOutgoing; ++Link)
			{
				bOutgoing = Storage.LaneLinks.IsValidIndex(Link) && Storage.LaneLinks[Link].Type == EZoneLaneLinkType::Outgoing;
			}
			if (!bOutgoing)
			{
				DeadEnds.Add(LaneIndex);
			}
		}
		break; // our maps have one ZoneGraph data actor
	}
	const TSharedRef<FMassEntityManager> Manager = Mass->GetMutableEntityManager().AsShared();
	FMassEntityQuery Query;
	Query.Initialize(Manager);
	Query.AddTagRequirement<FMassTrafficVehicleTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassTrafficVehicleControlFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassTrafficObstacleAvoidanceFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassTrafficRandomFractionFragment>(EMassFragmentAccess::ReadOnly);
	const UMassTrafficSubsystem* Traffic = World.GetSubsystem<UMassTrafficSubsystem>();
	// Junction lanes -> their junction (MassTraffic's intersection entities and their periods), to say
	// why a car waits at a closed one.
	TMap<const FZoneGraphTrafficLaneData*, const FMassTrafficIntersectionFragment*> LaneJunction;
	{
		FMassEntityQuery JunctionQuery;
		JunctionQuery.Initialize(Manager);
		JunctionQuery.AddRequirement<FMassTrafficIntersectionFragment>(EMassFragmentAccess::ReadOnly);
		FMassExecutionContext JunctionContext(*Manager, 0.f);
		JunctionQuery.ForEachEntityChunk(JunctionContext, [&LaneJunction](FMassExecutionContext& Chunk)
		{
			const TConstArrayView<FMassTrafficIntersectionFragment> Junctions = Chunk.GetFragmentView<FMassTrafficIntersectionFragment>();
			for (const FMassTrafficIntersectionFragment& Junction : Junctions)
			{
				for (const FMassTrafficPeriod& Period : Junction.Periods)
				{
					for (const FZoneGraphTrafficLaneData* PeriodLane : Period.VehicleLanes)
					{
						LaneJunction.Add(PeriodLane, &Junction);
					}
				}
			}
		});
	}
	// Lanes with a car on them (driving or waiting for recycling), and the cars waiting at a closed
	// junction lane: that lane and the room the car needs after it (Epic's GetSpaceTakenByVehicleOnLane).
	TSet<const FZoneGraphTrafficLaneData*> LanesWithCars;
	TArray<TPair<const FZoneGraphTrafficLaneData*, float>> ClosedWaits;
	const FVector2D GapRange = Settings ? Settings->MinimumDistanceToNextVehicleRange : FVector2D(80.0, 500.0);
	int32 Cars = 0;
	int32 Moving = 0;
	int32 AtDeadEnd = 0;
	// Where the stopped cars stand, first match wins (MassTraffic's own reasons to stop, see
	// ShouldStopAtLaneExit and the obstacle avoidance processors).
	int32 StopInBranch = 0;      // in a branch that only leads to dead ends (cut, recycled)
	int32 StopInJunction = 0;    // on an intersection lane: the junction cannot change period
	int32 StopForObstacle = 0;   // a pedestrian on a crossing or the player within 20 m ahead
	int32 StopBehindCar = 0;     // the car ahead within 10 m
	int32 StopNoNextLane = 0;    // at the lane end, no next lane chosen or none left
	int32 StopJunctionClosed = 0; // at the lane end, the junction lane it chose is closed
	int32 StopJunctionOpen = 0;  // at the lane end, the junction lane is open: no room after it
	int32 StopLaneEnd = 0;       // at the end of a lane into another plain lane
	int32 StopHeldAtEdge = 0;    // at a dead end whose entry is held closed (GuardLoops: in sight)
	int32 StopOther = 0;
	int32 Within100 = 0;
	int32 Within200 = 0;
	double SpeedSum = 0.0;
	double Nearest = TNumericLimits<double>::Max();
	FMassExecutionContext Context(*Manager, 0.f);
	Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FTransformFragment> Transforms = Chunk.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassTrafficVehicleControlFragment> Controls = Chunk.GetFragmentView<FMassTrafficVehicleControlFragment>();
		const TConstArrayView<FMassZoneGraphLaneLocationFragment> Locations = Chunk.GetFragmentView<FMassZoneGraphLaneLocationFragment>();
		const TConstArrayView<FAgentRadiusFragment> Radii = Chunk.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FMassTrafficObstacleAvoidanceFragment> Avoidances = Chunk.GetFragmentView<FMassTrafficObstacleAvoidanceFragment>();
		const TConstArrayView<FMassTrafficRandomFractionFragment> Fractions = Chunk.GetFragmentView<FMassTrafficRandomFractionFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			++Cars;
			if (Traffic)
			{
				LanesWithCars.Add(Traffic->GetTrafficLaneData(Locations[Index].LaneHandle));
			}
			const FMassTrafficVehicleControlFragment& Control = Controls[Index];
			const float Speed = Control.Speed; // cm/s along the lane
			SpeedSum += Speed;
			Moving += Speed > 100.f ? 1 : 0;
			const FMassZoneGraphLaneLocationFragment& Location = Locations[Index];
			if (Speed <= 100.f && Location.LaneHandle.DataHandle == DataHandle && DeadEnds.Contains(Location.LaneHandle.Index))
			{
				++AtDeadEnd;
			}
			if (Speed <= 100.f)
			{
				const FZoneGraphTrafficLaneData* Lane = Traffic ? Traffic->GetTrafficLaneData(Location.LaneHandle) : nullptr;
				const FMassTrafficObstacleAvoidanceFragment& Avoidance = Avoidances[Index];
				const bool bAtLaneEnd = Location.DistanceAlongLane >= Location.LaneLength - Radii[Index].Radius - 500.f;
				if (IsInDeadEndBranch(Location.LaneHandle.DataHandle.Index, Location.LaneHandle.Index))
				{
					++StopInBranch;
				}
				else if (Lane && Lane->ConstData.bIsIntersectionLane)
				{
					++StopInJunction;
				}
				else if (Avoidance.DistanceToCollidingObstacle < 2000.f)
				{
					++StopForObstacle;
				}
				else if (Avoidance.DistanceToNext < 1000.f)
				{
					++StopBehindCar;
				}
				else if (bAtLaneEnd && (!Control.NextLane || (Lane && Lane->NextLanes.IsEmpty())))
				{
					++StopNoNextLane;
				}
				else if (bAtLaneEnd && Control.NextLane->ConstData.bIsIntersectionLane)
				{
					if (Control.NextLane->bIsOpen)
					{
						++StopJunctionOpen;
					}
					else
					{
						++StopJunctionClosed;
						const float Fraction = static_cast<float>(Fractions[Index].RandomFraction);
						ClosedWaits.Emplace(Control.NextLane, 2.f * Radii[Index].Radius + FMath::Lerp(static_cast<float>(GapRange.X), static_cast<float>(GapRange.Y), Fraction));
					}
				}
				else if (bAtLaneEnd && !Control.NextLane->bIsOpen)
				{
					++StopHeldAtEdge;
				}
				else if (bAtLaneEnd)
				{
					++StopLaneEnd;
				}
				else
				{
					++StopOther;
				}
			}
			const double Distance = FVector::Dist(Transforms[Index].GetTransform().GetLocation(), PlayerLocation);
			Within100 += Distance < 10000.0 ? 1 : 0;
			Within200 += Distance < 20000.0 ? 1 : 0;
			Nearest = FMath::Min(Nearest, Distance);
		}
	});
	// Cars handed to MassTraffic's recycling (RecycleAtDeadEnds) and not moved yet, and of those the
	// ones its overseer cannot take (its query needs Epic's damage fragment, among others).
	int32 Recyclable = 0;
	int32 RecyclableWithoutDamage = 0;
	int32 RecyclableInJunction = 0;
	{
		FMassEntityQuery RecyclableQuery;
		RecyclableQuery.Initialize(Manager);
		RecyclableQuery.AddTagRequirement<FMassTrafficRecyclableVehicleTag>(EMassFragmentPresence::All);
		RecyclableQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		RecyclableQuery.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
		FMassExecutionContext RecyclableContext(*Manager, 0.f);
		RecyclableQuery.ForEachEntityChunk(RecyclableContext, [&](FMassExecutionContext& Chunk)
		{
			Recyclable += Chunk.GetNumEntities();
			const TConstArrayView<FMassZoneGraphLaneLocationFragment> Locations = Chunk.GetFragmentView<FMassZoneGraphLaneLocationFragment>();
			for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
			{
				RecyclableWithoutDamage += Manager->GetFragmentDataPtr<FMassTrafficVehicleDamageFragment>(Chunk.GetEntity(Index)) ? 0 : 1;
				const FZoneGraphTrafficLaneData* Lane = Traffic ? Traffic->GetTrafficLaneData(Locations[Index].LaneHandle) : nullptr;
				LanesWithCars.Add(Lane);
				RecyclableInJunction += Lane && Lane->ConstData.bIsIntersectionLane ? 1 : 0;
			}
		});
	}
	FString Network;
	if (!bNetworkLogged && Cars > 0)
	{
		// Once, when the cars are there: the network they drive on and the density.
		bNetworkLogged = true;
		Network = FString::Printf(TEXT(" | car lanes=%d (%d dead ends), %.1f km, %.1f cars per km"), CarLanes, DeadEnds.Num(),
			Length / 100000.0, Length > 0.0 ? Cars / (Length / 100000.0) : 0.0);
	}
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic cars: simulated=%d moving=%d (over 1 m/s) stopped on a dead-end lane=%d waiting for recycling=%d (without Epic's damage fragment=%d) recycled so far=%d mean speed=%.0f km/h | within 100 m=%d within 200 m=%d nearest=%s%s"),
		Cars, Moving, AtDeadEnd, Recyclable, RecyclableWithoutDamage, Recycled, Cars > 0 ? SpeedSum / Cars * 0.036 : 0.0, Within100, Within200,
		Cars > 0 ? *FString::Printf(TEXT("%.0f m"), Nearest / 100.0) : TEXT("none"), *Network);
	if (Cars > Moving)
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic stops: %d stopped | in a dead-end branch=%d inside a junction=%d for a pedestrian or the player=%d behind a car=%d | at the lane end: no next lane=%d junction closed=%d junction open (no room after it)=%d plain lane=%d held at the map's edge (in sight)=%d | other=%d"),
			Cars - Moving, StopInBranch, StopInJunction, StopForObstacle, StopBehindCar, StopNoNextLane, StopJunctionClosed, StopJunctionOpen,
			StopLaneEnd, StopHeldAtEdge, StopOther);
	}
	if (!ClosedWaits.IsEmpty())
	{
		// Why the junction lane stays closed, in the order MassTraffic checks it: ShouldStopAtLaneExit
		// (room after the junction), then UMassTrafficUpdateIntersectionsProcessor (a stop-sign junction
		// opens a side whose car is ready, once the lanes of the period before are clear).
		int32 NoLaneAfter = 0;
		int32 AfterTooShort = 0;
		int32 AfterFull = 0;
		int32 InNoPeriod = 0;
		int32 ClearingCar = 0;
		int32 ClearingEmpty = 0;
		int32 ClearingReserved = 0;
		int32 Turn = 0;
		int32 NotReady = 0;
		for (const TPair<const FZoneGraphTrafficLaneData*, float>& Wait : ClosedWaits)
		{
			const FZoneGraphTrafficLaneData& Next = *Wait.Key;
			const float Needed = Wait.Value;
			const FZoneGraphTrafficLaneData* After = Next.NextLanes.Num() == 1 ? Next.NextLanes[0] : nullptr;
			const FMassTrafficIntersectionFragment* const* Junction = LaneJunction.Find(&Next);
			if (!After)
			{
				++NoLaneAfter;
				continue;
			}
			if (After->Length < Needed)
			{
				++AfterTooShort;
				continue;
			}
			if (After->SpaceAvailableFromStartOfLaneForVehicle(*Manager, true, false) - FMath::Max(Next.Length - Next.SpaceAvailable, 0.f) < Needed)
			{
				++AfterFull;
				continue;
			}
			if (!Junction)
			{
				++InNoPeriod;
				continue;
			}
			const FMassTrafficIntersectionFragment& Fragment = **Junction;
			bool bCar = false;
			bool bEmpty = false;
			bool bReserved = false;
			if (Fragment.Periods.IsValidIndex(Fragment.CurrentPeriodIndex) && static_cast<float>(Fragment.PeriodTimeRemaining) <= 0.f)
			{
				const FMassTrafficPeriod& Period = Fragment.Periods[Fragment.CurrentPeriodIndex];
				for (int32 Index = 0; Index < Period.NumVehicleLanes(EMassTrafficIntersectionVehicleLaneType::VehicleLane_ClosedInNextPeriod); ++Index)
				{
					const FZoneGraphTrafficLaneData* Closing = Period.GetVehicleLane(Index, EMassTrafficIntersectionVehicleLaneType::VehicleLane_ClosedInNextPeriod);
					if (!Closing)
					{
						continue;
					}
					if (Closing->NumVehiclesOnLane > 0)
					{
						(LanesWithCars.Contains(Closing) ? bCar : bEmpty) = true;
					}
					bReserved |= Closing->NumReservedVehiclesOnLane > 0;
				}
			}
			if (bEmpty)
			{
				++ClearingEmpty;
			}
			else if (bCar)
			{
				++ClearingCar;
			}
			else if (bReserved)
			{
				++ClearingReserved;
			}
			else if (Next.bIsVehicleReadyToUseLane)
			{
				++Turn;
			}
			else
			{
				++NotReady;
			}
		}
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic junction waits: %d car(s) at a closed junction lane | lane after it: none=%d shorter than the car=%d full=%d | lane in no period=%d | junction clearing a lane: car inside=%d counted but empty=%d reserved=%d | waiting its turn=%d not seen ready=%d | waiting for recycling inside a junction=%d"),
			ClosedWaits.Num(), NoLaneAfter, AfterTooShort, AfterFull, InNoPeriod, ClearingCar, ClearingEmpty, ClearingReserved, Turn, NotReady,
			RecyclableInJunction);
	}
}

namespace GTPMassTrafficLocal
{
/**
 * MassTraffic's ShouldStopAtLaneExit lets a car into a junction only when the lane after it has room
 * for the car and its gap to the next one (2 x radius + MinimumDistanceToNextVehicleRange): a lane
 * after a junction shorter than that never has, and the car stopped before it never chooses again (no
 * new choice that close to the line). 10 m with Epic's 5 m upper gap and a 5 m car, or
 * -TrafficMinRoomAfterJunction=<m>.
 */
float WallCm()
{
	static const float Value = []
	{
		const UMassTrafficSettings* Settings = GetDefault<UMassTrafficSettings>();
		float Metres = 5.f + (Settings ? static_cast<float>(Settings->MinimumDistanceToNextVehicleRange.Y) : 500.f) / 100.f;
		FParse::Value(FCommandLine::Get(), TEXT("TrafficMinRoomAfterJunction="), Metres);
		return Metres * 100.f;
	}();
	return Value;
}

/** A junction lane no car can drive through: MassTraffic skips one with more than one lane after it. */
bool IsWall(const FZoneGraphTrafficLaneData& Lane)
{
	return Lane.ConstData.bIsIntersectionLane
		&& (Lane.NextLanes.Num() != 1 || !Lane.NextLanes[0] || Lane.NextLanes[0]->Length < WallCm());
}
}

void UGTPMassTrafficSubsystem::HandleDeadEnds()
{
	UWorld* World = GetWorld();
	UMassTrafficSubsystem* Traffic = World ? World->GetSubsystem<UMassTrafficSubsystem>() : nullptr;
	if (!Traffic || bDeadEndsHandled)
	{
		return;
	}
	bool bAnyLane = false;
	for (const FMassTrafficZoneGraphData* Data : Traffic->GetMutableTrafficZoneGraphData())
	{
		bAnyLane |= Data && !Data->TrafficLaneDataArray.IsEmpty();
	}
	if (!bAnyLane)
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic dead ends: no traffic lane data yet, tried again later"));
		return;
	}
	bDeadEndsHandled = true;
	FString Mode = TEXT("link");
	FParse::Value(FCommandLine::Get(), TEXT("TrafficDeadEnds="), Mode);
	if (FParse::Param(FCommandLine::Get(), TEXT("NoDeadEndCut")))
	{
		Mode = TEXT("none");
	}
	if (Mode == TEXT("cut"))
	{
		CutDeadEndBranches();
	}
	else if (Mode == TEXT("none"))
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic dead ends: left as built (-TrafficDeadEnds=none)"));
	}
	else
	{
		LinkDeadEndsToEntries();
	}
}

void UGTPMassTrafficSubsystem::LinkDeadEndsToEntries()
{
	using namespace GTPMassTrafficLocal;
	UWorld* World = GetWorld();
	UMassTrafficSubsystem* Traffic = World ? World->GetSubsystem<UMassTrafficSubsystem>() : nullptr;
	UZoneGraphSubsystem* Graph = World ? World->GetSubsystem<UZoneGraphSubsystem>() : nullptr;
	if (!Traffic || !Graph)
	{
		return;
	}
	int32 Lanes = 0;
	int32 Walls = 0;
	int32 WallLinks = 0;
	int32 DeadEnds = 0;
	int32 Linked = 0;
	int32 Entries = 0;
	int32 EntriesUsed = 0;
	double JumpCm = 0.0;
	double LongestJumpCm = 0.0;
	Loops.Reset();
	const TArrayView<FMassTrafficZoneGraphData*> AllData = Traffic->GetMutableTrafficZoneGraphData();
	for (int32 Slot = 0; Slot < AllData.Num(); ++Slot)
	{
		FMassTrafficZoneGraphData* Data = AllData[Slot];
		const FZoneGraphStorage* Storage = Data ? Graph->GetZoneGraphStorage(Data->DataHandle) : nullptr;
		if (!Storage || Data->TrafficLaneDataArray.IsEmpty())
		{
			continue;
		}
		TArray<FZoneGraphTrafficLaneData>& All = Data->TrafficLaneDataArray;
		const int32 Num = All.Num();
		const FZoneGraphTrafficLaneData* const First = All.GetData();
		auto IndexOf = [First, Num](const FZoneGraphTrafficLaneData* Lane)
		{
			const int32 Index = Lane ? static_cast<int32>(Lane - First) : INDEX_NONE;
			return Index >= 0 && Index < Num ? Index : INDEX_NONE;
		};
		auto EndPoint = [Storage](const FZoneGraphTrafficLaneData& Lane, bool bStart)
		{
			const FZoneLaneData& ZoneLane = Storage->Lanes[Lane.LaneHandle.Index];
			return Storage->LanePoints[bStart ? ZoneLane.PointsBegin : ZoneLane.PointsEnd - 1];
		};
		// Walls first: out of the choice of the lanes before them.
		TSet<const FZoneGraphTrafficLaneData*> WallLanes;
		for (const FZoneGraphTrafficLaneData& Lane : All)
		{
			if (IsWall(Lane))
			{
				WallLanes.Add(&Lane);
			}
		}
		Walls += WallLanes.Num();
		if (!WallLanes.IsEmpty())
		{
			for (FZoneGraphTrafficLaneData& Lane : All)
			{
				WallLinks += Lane.NextLanes.RemoveAll([&WallLanes](const FZoneGraphTrafficLaneData* Next) { return WallLanes.Contains(Next); });
			}
		}
		// Entries: road lanes no lane leads into, with a way on.
		TArray<int32> Feeders;
		Feeders.SetNumZeroed(Num);
		for (const FZoneGraphTrafficLaneData& Lane : All)
		{
			for (const FZoneGraphTrafficLaneData* Next : Lane.NextLanes)
			{
				const int32 NextIndex = IndexOf(Next);
				if (NextIndex != INDEX_NONE)
				{
					++Feeders[NextIndex];
				}
			}
		}
		TArray<int32> EntryLanes;
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if (!All[Index].ConstData.bIsIntersectionLane && Feeders[Index] == 0 && !All[Index].NextLanes.IsEmpty())
			{
				EntryLanes.Add(Index);
			}
		}
		Entries += EntryLanes.Num();
		TArray<int32> Uses;
		Uses.SetNumZeroed(EntryLanes.Num());
		// Each dead end (a road lane with no lane after it) leads on to the least used entry, then the nearest.
		for (int32 Index = 0; Index < Num; ++Index)
		{
			FZoneGraphTrafficLaneData& Lane = All[Index];
			if (Lane.ConstData.bIsIntersectionLane || !Lane.NextLanes.IsEmpty())
			{
				continue;
			}
			++DeadEnds;
			const FVector End = EndPoint(Lane, false);
			int32 Best = INDEX_NONE;
			double BestCm = 0.0;
			for (int32 Entry = 0; Entry < EntryLanes.Num(); ++Entry)
			{
				if (EntryLanes[Entry] == Index)
				{
					continue;
				}
				const double Cm = FVector::Dist(End, EndPoint(All[EntryLanes[Entry]], true));
				if (Best == INDEX_NONE || Uses[Entry] < Uses[Best] || (Uses[Entry] == Uses[Best] && Cm < BestCm))
				{
					Best = Entry;
					BestCm = Cm;
				}
			}
			if (Best == INDEX_NONE)
			{
				continue;
			}
			Lane.NextLanes.Add(&All[EntryLanes[Best]]);
			FLoop& Loop = Loops.AddDefaulted_GetRef();
			Loop.Slot = Slot;
			Loop.DataIndex = Data->DataHandle.Index;
			Loop.From = Index;
			Loop.To = EntryLanes[Best];
			Loop.FromLane = Lane.LaneHandle.Index;
			Loop.ToLane = All[EntryLanes[Best]].LaneHandle.Index;
			Loop.Exit = End;
			Loop.Entry = EndPoint(All[EntryLanes[Best]], true);
			EntriesUsed += Uses[Best] == 0 ? 1 : 0;
			++Uses[Best];
			++Linked;
			JumpCm += BestCm;
			LongestJumpCm = FMath::Max(LongestJumpCm, BestCm);
		}
		Lanes += Num;
	}
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic dead ends: %d of %d car lane(s) lead nowhere, %d linked to %d of %d entry lane(s) (a lane no lane leads into; the car leaves the map there and comes back at the entry, %.0f m away on average, %.0f m at most), %d left; %d junction lane(s) into a lane under %.0f m taken out of the choice (%d link(s)); no lane closed"),
		DeadEnds, Lanes, Linked, EntriesUsed, Entries, Linked > 0 ? JumpCm / Linked / 100.0 : 0.0, LongestJumpCm / 100.0,
		DeadEnds - Linked, Walls, WallCm() / 100.f, WallLinks);
	LoopEntries = EntriesUsed;
}

void UGTPMassTrafficSubsystem::GuardLoops()
{
	UWorld* World = GetWorld();
	UMassTrafficSubsystem* Traffic = World ? World->GetSubsystem<UMassTrafficSubsystem>() : nullptr;
	UMassEntitySubsystem* Mass = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Traffic || !Mass || Loops.IsEmpty())
	{
		return;
	}
	APlayerController* Controller = World->GetFirstPlayerController();
	const bool bHasView = Controller != nullptr;
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	float Fov = 90.f;
	if (Controller)
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		if (Controller->PlayerCameraManager)
		{
			Fov = Controller->PlayerCameraManager->GetFOVAngle();
		}
	}
	const FVector ViewDirection = ViewRotation.Vector();
	// The horizontal field plus 15 degrees: the corners of a wide screen and a turn of the camera in 0.25 s.
	const double CosHalfField = FMath::Cos(FMath::DegreesToRadians(FMath::Min(Fov * 0.5f + 15.f, 89.f)));
	static const double SightCm = []
	{
		float Metres = 300.f;
		FParse::Value(FCommandLine::Get(), TEXT("TrafficLoopSight="), Metres);
		return Metres * 100.0;
	}();
	constexpr double AlwaysSeenCm = 3000.0;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPTrafficLoopSight), false);
	if (APawn* Pawn = Controller ? Controller->GetPawn() : nullptr)
	{
		Params.AddIgnoredActor(Pawn);
	}
	auto InSight = [&](const FVector& Ground)
	{
		if (!bHasView)
		{
			return false;
		}
		// A car's roof, about.
		const FVector Point = Ground + FVector(0.0, 0.0, 150.0);
		const FVector ToPoint = Point - ViewLocation;
		const double Distance = ToPoint.Size();
		if (Distance > SightCm)
		{
			return false;
		}
		if (Distance > AlwaysSeenCm && FVector::DotProduct(ToPoint / Distance, ViewDirection) < CosHalfField)
		{
			return false;
		}
		FHitResult Hit;
		// Hidden only by something well before the point (not the car or the kerb at it).
		return !World->LineTraceSingleByChannel(Hit, ViewLocation, Point, ECC_Visibility, Params) || Hit.Distance >= Distance - 300.0;
	};
	auto Key = [](int32 DataIndex, int32 Lane) { return (static_cast<int64>(DataIndex) << 32) | static_cast<uint32>(Lane); };
	TSet<int64> InSightNow;
	TSet<int64> Checked;
	for (const FLoop& Loop : Loops)
	{
		const int64 Entry = Key(Loop.DataIndex, Loop.ToLane);
		if (!InSightNow.Contains(Entry) && (InSight(Loop.Exit) || (!Checked.Contains(Entry) && InSight(Loop.Entry))))
		{
			InSightNow.Add(Entry);
		}
		Checked.Add(Entry);
	}
	const TArrayView<FMassTrafficZoneGraphData*> AllData = Traffic->GetMutableTrafficZoneGraphData();
	TMultiMap<int64, int32> SourcesOf;
	for (const FLoop& Loop : Loops)
	{
		FMassTrafficZoneGraphData* Data = AllData.IsValidIndex(Loop.Slot) ? AllData[Loop.Slot] : nullptr;
		if (Data && Data->TrafficLaneDataArray.IsValidIndex(Loop.To))
		{
			Data->TrafficLaneDataArray[Loop.To].bIsOpen = !InSightNow.Contains(Key(Loop.DataIndex, Loop.ToLane));
		}
		SourcesOf.Add(Key(Loop.DataIndex, Loop.ToLane), Loop.FromLane);
	}
	// The cars that looped since the last check: now on an entry, coming from one of its dead ends.
	FMassEntityManager& Manager = Mass->GetMutableEntityManager();
	FMassEntityQuery Query;
	Query.Initialize(Manager.AsShared());
	Query.AddTagRequirement<FMassTrafficVehicleTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FMassTrafficVehicleControlFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
	TMap<uint64, int32> LaneNow;
	FMassExecutionContext Context(Manager, 0.f);
	Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FMassTrafficVehicleControlFragment> Controls = Chunk.GetFragmentView<FMassTrafficVehicleControlFragment>();
		const TConstArrayView<FMassZoneGraphLaneLocationFragment> Locations = Chunk.GetFragmentView<FMassZoneGraphLaneLocationFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			const uint64 Car = Chunk.GetEntity(Index).AsNumber();
			const FZoneGraphLaneHandle& Lane = Locations[Index].LaneHandle;
			LaneNow.Add(Car, Lane.Index);
			const int32* Before = LaneOfCar.Find(Car);
			if (!Before || *Before == Lane.Index)
			{
				continue;
			}
			const int64 Entry = Key(Lane.DataHandle.Index, Lane.Index);
			TArray<int32> Sources;
			SourcesOf.MultiFind(Entry, Sources);
			if (Sources.Contains(Controls[Index].PreviousLaneIndex))
			{
				++LoopJumps;
				LoopJumpsInSight += (InSightNow.Contains(Entry) || EntriesInSight.Contains(Entry)) ? 1 : 0;
			}
		}
	});
	LaneOfCar = MoveTemp(LaneNow);
	EntriesInSight = MoveTemp(InSightNow);
	MostEntriesHeld = FMath::Max(MostEntriesHeld, EntriesInSight.Num());
}

void UGTPMassTrafficSubsystem::CutDeadEndBranches()
{
	using namespace GTPMassTrafficLocal;
	UWorld* World = GetWorld();
	UMassTrafficSubsystem* Traffic = World ? World->GetSubsystem<UMassTrafficSubsystem>() : nullptr;
	if (!Traffic)
	{
		return;
	}
	int32 Lanes = 0;
	int32 Branch = 0;
	int32 BranchJunction = 0;
	int32 LinksCut = 0;
	int32 Walls = 0;
	// Junction lanes into a lane too short for a car and its gap count as dead ends (WallCm).
	for (FMassTrafficZoneGraphData* Data : Traffic->GetMutableTrafficZoneGraphData())
	{
		if (!Data || Data->TrafficLaneDataArray.IsEmpty())
		{
			continue;
		}
		TArray<FZoneGraphTrafficLaneData>& All = Data->TrafficLaneDataArray;
		const int32 Num = All.Num();
		const FZoneGraphTrafficLaneData* const First = All.GetData();
		auto IndexOf = [First, Num](const FZoneGraphTrafficLaneData* Lane)
		{
			const int32 Index = Lane ? static_cast<int32>(Lane - First) : INDEX_NONE;
			return Index >= 0 && Index < Num ? Index : INDEX_NONE;
		};
		// A lane is in a dead-end branch when none of its next lanes leads anywhere but to dead ends:
		// dead ends first, then, backwards, every lane whose next lanes are all in such branches.
		TArray<int32> WaysOn;
		WaysOn.SetNumZeroed(Num);
		TArray<TArray<int32>> Before;
		Before.SetNum(Num);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FZoneGraphTrafficLaneData& Lane = All[Index];
			if (!Lane.NextLanes.IsEmpty() && IsWall(Lane))
			{
				// No way on through this junction lane (MassTraffic also skips a junction lane with
				// more than one lane after it).
				++Walls;
				continue;
			}
			for (const FZoneGraphTrafficLaneData* Next : Lane.NextLanes)
			{
				const int32 NextIndex = IndexOf(Next);
				if (NextIndex != INDEX_NONE)
				{
					++WaysOn[Index];
					Before[NextIndex].Add(Index);
				}
			}
		}
		TArray<bool> InBranch;
		InBranch.Init(false, Num);
		TArray<int32> Queue;
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if (WaysOn[Index] == 0)
			{
				InBranch[Index] = true;
				Queue.Add(Index);
			}
		}
		for (int32 Head = 0; Head < Queue.Num(); ++Head)
		{
			for (const int32 Previous : Before[Queue[Head]])
			{
				if (!InBranch[Previous] && --WaysOn[Previous] == 0)
				{
					InBranch[Previous] = true;
					Queue.Add(Previous);
				}
			}
		}
		// Lanes with a way on forget the branches; lanes inside a branch keep their plain next lanes
		// (a car spawned there drives on to a stop outside any junction) but forget the junction lanes
		// of the branch, so that no car ever stops inside a junction for want of a lane after it.
		for (int32 Index = 0; Index < Num; ++Index)
		{
			FZoneGraphTrafficLaneData& Lane = All[Index];
			const bool bLaneInBranch = InBranch[Index];
			LinksCut += Lane.NextLanes.RemoveAll([&](const FZoneGraphTrafficLaneData* Next)
			{
				const int32 NextIndex = IndexOf(Next);
				return NextIndex != INDEX_NONE && InBranch[NextIndex] && (!bLaneInBranch || Next->ConstData.bIsIntersectionLane);
			});
			if (bLaneInBranch)
			{
				++Branch;
				BranchJunction += Lane.ConstData.bIsIntersectionLane ? 1 : 0;
				if (!Lane.ConstData.bIsIntersectionLane)
				{
					// The overseer only puts the cars it moves (density, recycling) on open lanes: not
					// back into a branch. Only the junctions open and close their own lanes.
					Lane.bIsOpen = false;
				}
				DeadEndBranch.Add(Lane.LaneHandle.Index);
				DeadEndBranchData = Lane.LaneHandle.DataHandle.Index;
			}
		}
		Lanes += Num;
	}
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic dead ends: %d car lane(s) of %d only lead to dead ends (%d of them in junctions, the others closed to the overseer; %d junction lane(s) into a lane under %.0f m counted as dead ends), %d link(s) to them removed from the cars' choice; every other lane keeps a way on"),
		Branch, Lanes, BranchJunction, Walls, WallCm() / 100.f, LinksCut);
}

void UGTPMassTrafficSubsystem::RecycleAtDeadEnds()
{
	UWorld* World = GetWorld();
	UMassEntitySubsystem* Mass = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	const UMassTrafficSubsystem* Traffic = World ? World->GetSubsystem<UMassTrafficSubsystem>() : nullptr;
	if (!Mass || !Traffic)
	{
		return;
	}
	if (!bDeadEndsHandled)
	{
		// The lane data was not there before the spawners.
		HandleDeadEnds();
	}
	// Out of the player's sight: behind the camera from 40 m, in front of it from 120 m
	// (-DeadEndKeepNear=<m>), the distance at which GTA lets a car go.
	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (APlayerController* Controller = World->GetFirstPlayerController())
	{
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	}
	else if (const APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		ViewLocation = Player->GetActorLocation();
		ViewRotation = Player->GetActorRotation();
	}
	const FVector ViewDirection = ViewRotation.Vector();
	static const double InSightCm = []
	{
		float Metres = 120.f;
		FParse::Value(FCommandLine::Get(), TEXT("DeadEndKeepNear="), Metres);
		return Metres * 100.0;
	}();
	constexpr double OutOfSightCm = 4000.0;
	FMassEntityManager& Manager = Mass->GetMutableEntityManager();
	FMassEntityQuery Query;
	Query.Initialize(Manager.AsShared());
	Query.AddTagRequirement<FMassTrafficVehicleTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassTrafficVehicleControlFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
	TArray<FMassEntityHandle> ToRecycle;
	FMassExecutionContext Context(Manager, 0.f);
	Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FTransformFragment> Transforms = Chunk.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> Radii = Chunk.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FMassTrafficVehicleControlFragment> Controls = Chunk.GetFragmentView<FMassTrafficVehicleControlFragment>();
		const TConstArrayView<FMassZoneGraphLaneLocationFragment> Locations = Chunk.GetFragmentView<FMassZoneGraphLaneLocationFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			const FMassZoneGraphLaneLocationFragment& Location = Locations[Index];
			const FZoneGraphTrafficLaneData* Lane = Traffic->GetTrafficLaneData(Location.LaneHandle);
			if (!Lane)
			{
				continue;
			}
			const bool bStopped = Controls[Index].Speed <= 100.f;
			const bool bInBranch = IsInDeadEndBranch(Location.LaneHandle.DataHandle.Index, Location.LaneHandle.Index);
			const bool bAtDeadEnd = bStopped && Lane->NextLanes.IsEmpty()
				&& Location.DistanceAlongLane >= Location.LaneLength - Radii[Index].Radius - 300.f;
			// A car driving through a junction is let out of it first: a recycled car stands still
			// until MassTraffic moves it, and one standing in a junction blocks it.
			if ((!bInBranch && !bAtDeadEnd) || (Lane->ConstData.bIsIntersectionLane && !bStopped))
			{
				continue;
			}
			const FVector ToCar = Transforms[Index].GetTransform().GetLocation() - ViewLocation;
			const double Distance = ToCar.Size();
			const bool bInSight = Distance > 1.0 && FVector::DotProduct(ToCar / Distance, ViewDirection) > 0.5;
			if (Distance < (bInSight ? InSightCm : OutOfSightCm))
			{
				continue;
			}
			ToRecycle.Add(Chunk.GetEntity(Index));
		}
	});
	for (const FMassEntityHandle& Entity : ToRecycle)
	{
		// MassTraffic's overseer only takes recyclable cars that carry its damage fragment.
		if (!Manager.GetFragmentDataPtr<FMassTrafficVehicleDamageFragment>(Entity))
		{
			Manager.Defer().AddFragment<FMassTrafficVehicleDamageFragment>(Entity);
			++DamageAdded;
		}
		Manager.Defer().SwapTags<FMassTrafficVehicleTag, FMassTrafficRecyclableVehicleTag>(Entity);
	}
	Recycled += ToRecycle.Num();
	if (ToRecycle.Num() > 0 && (Recycled <= 20 || Recycled % 50 < ToRecycle.Num()))
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic dead ends: %d car(s) handed to MassTraffic's recycling (%d in all, %d given Epic's damage fragment)"),
			ToRecycle.Num(), Recycled, DamageAdded);
	}
}
#else
void UGTPMassTrafficSubsystem::LogSimulatedCars(UWorld& World, const FVector& PlayerLocation)
{
}

void UGTPMassTrafficSubsystem::CutDeadEndBranches()
{
}

void UGTPMassTrafficSubsystem::LinkDeadEndsToEntries()
{
}

void UGTPMassTrafficSubsystem::HandleDeadEnds()
{
}

void UGTPMassTrafficSubsystem::GuardLoops()
{
}

void UGTPMassTrafficSubsystem::RecycleAtDeadEnds()
{
}
#endif
