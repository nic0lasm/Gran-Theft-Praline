#include "GTPMassTraffic.h"

#include "GTPTraffic.h"
#include "GTPMassTrafficCars.h"
#include "GTPVehicle.h"
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
#include "UObject/UnrealType.h"
#include "ZoneGraphData.h"
#if WITH_GTP_MASSTRAFFIC
#include "MassCommonFragments.h"
#include "MassEntityQuery.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "MassTrafficFragments.h"
#include "MassTrafficPhysics.h"
#include "MassEntityConfigAsset.h"
#include "MassLODTypes.h"
#include "MassRepresentationFragments.h"
#include "MassSpawnerTypes.h"
#include "MassVisualizationTrait.h"
#include "MassTrafficSettings.h"
#include "MassTrafficSubsystem.h"
#include "MassTrafficVehicleSimulationTrait.h"
#include "MassTrafficVehicleVolumeTrait.h"
#include "MassActorSubsystem.h"
#include "MassMovementFragments.h"
#include "MassZoneGraphNavigationFragments.h"
#include "ZoneGraphSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGTPMassTraffic, Log, All);

#if WITH_GTP_MASSTRAFFIC
namespace GTPMassTrafficCarsQuery
{
	/** GTPMassTrafficCars.h: the driving MassTraffic cars within RadiusCm of Near, at their Mass transform (what is shown). */
	void FindCars(UWorld& World, const FVector& Near, float RadiusCm, TArray<FGTPMassTrafficCar>& Out)
	{
		UMassEntitySubsystem* Mass = World.GetSubsystem<UMassEntitySubsystem>();
		if (!Mass)
		{
			return;
		}
		FMassEntityManager& Manager = Mass->GetMutableEntityManager();
		FMassEntityQuery Query;
		Query.Initialize(Manager.AsShared());
		Query.AddTagRequirement<FMassTrafficVehicleTag>(EMassFragmentPresence::All);
		Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FMassTrafficVehicleControlFragment>(EMassFragmentAccess::ReadOnly);
		Query.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
		const double RadiusSquared = FMath::Square(static_cast<double>(RadiusCm));
		FMassExecutionContext Context(Manager, 0.f);
		Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
		{
			const TConstArrayView<FTransformFragment> Transforms = Chunk.GetFragmentView<FTransformFragment>();
			const TConstArrayView<FMassTrafficVehicleControlFragment> Controls = Chunk.GetFragmentView<FMassTrafficVehicleControlFragment>();
			const TConstArrayView<FAgentRadiusFragment> Radii = Chunk.GetFragmentView<FAgentRadiusFragment>();
			for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
			{
				const FTransform& Transform = Transforms[Index].GetTransform();
				if (FVector::DistSquared(Transform.GetLocation(), Near) > RadiusSquared)
				{
					continue;
				}
				const FMassEntityHandle Entity = Chunk.GetEntity(Index);
				FGTPMassTrafficCar& Car = Out.AddDefaulted_GetRef();
				Car.Location = Transform.GetLocation();
				Car.Forward = Transform.GetRotation().GetForwardVector();
				Car.SpeedCmS = Controls[Index].Speed;
				Car.RadiusCm = Radii[Index].Radius;
				Car.Id = (static_cast<uint64>(static_cast<uint32>(Entity.SerialNumber)) << 32) | static_cast<uint64>(static_cast<uint32>(Entity.Index));
			}
		});
	}
}
#endif

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
	// For the game module (the crash step of -GTARun): where the cars are. Takes the world it is given, holds nothing.
	GTPMassTrafficCars::SetQuery(&GTPMassTrafficCarsQuery::FindCars);
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
		TArray<AMassSpawner*> Ordered;
		for (const TPair<int32, AMassSpawner*>& Entry : Spawners)
		{
			Ordered.Add(Entry.Value);
		}
		ShowNearCarsAsInstances(Ordered);
	}
	for (const TPair<int32, AMassSpawner*>& Entry : Spawners)
	{
		Entry.Value->DoSpawning();
		++StartedSpawners;
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic: started %s (order %d)"), *Entry.Value->GetName(), Entry.Key);
	}
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic: %d spawner(s) started (tag %s)"), StartedSpawners, *SpawnerTag.ToString());
	// The player as an obstacle the cars brake for (UGTPMassTrafficBodyProcessor puts it on him): what Epic's "Traffic
	// Obstacle" trait gives (transform, velocity, radius, the tag) and a car's volume (FMassTrafficVehicleVolumeParameters),
	// with which MassTraffic tests the boxes (WillCollide), not the spheres only: a car on the next lane, or the player on
	// the sidewalk, brakes no car. Two entities, on foot and at the wheel (a shared fragment does not change), the one not
	// in use out of the way. Each box is a little longer than its sphere: MassTraffic tests the boxes where the spheres
	// first meet, they must overlap there.
	int32 PlayerObstacle = 1;
	FParse::Value(FCommandLine::Get(), TEXT("MassTrafficPlayerObstacle="), PlayerObstacle);
	UMassEntitySubsystem* ObstacleMass = InWorld.GetSubsystem<UMassEntitySubsystem>();
	if (StartedSpawners > 0 && PlayerObstacle != 0 && ObstacleMass)
	{
		FMassEntityManager& Manager = ObstacleMass->GetMutableEntityManager();
		const TArray<const UScriptStruct*> Composition{FTransformFragment::StaticStruct(), FMassVelocityFragment::StaticStruct(), FAgentRadiusFragment::StaticStruct(),
			FMassTrafficObstacleTag::StaticStruct(), FGTPPlayerObstacleTag::StaticStruct()};
		const FMassArchetypeHandle Archetype = Manager.CreateArchetype(Composition);
		struct FObstacleKind
		{
			float Radius;
			float HalfLength;
			float HalfWidth;
			const TCHAR* Name;
		};
		// On foot: a walker's radius, as Epic's "Traffic Obstacle" for one (GTPCrowd). At the wheel: our car's half length
		// (the radius tells the radius above 100 cm from the other, UGTPMassTrafficBodyProcessor).
		const FObstacleKind Kinds[] = {{40.f, 80.f, 50.f, TEXT("on foot")}, {230.f, 300.f, 100.f, TEXT("at the wheel")}};
		FString ObstacleReport;
		for (const FObstacleKind& Kind : Kinds)
		{
			const FMassEntityHandle Obstacle = Manager.CreateEntity(Archetype);
			if (FAgentRadiusFragment* Radius = Manager.GetFragmentDataPtr<FAgentRadiusFragment>(Obstacle))
			{
				Radius->Radius = Kind.Radius;
			}
			if (FTransformFragment* Transform = Manager.GetFragmentDataPtr<FTransformFragment>(Obstacle))
			{
				// Out of the way until the processor puts it on the player.
				Transform->SetTransform(FTransform(FVector(0.0, 0.0, -100000.0)));
			}
			FMassTrafficVehicleVolumeParameters Volume;
			Volume.HalfLength = Kind.HalfLength;
			Volume.HalfWidth = Kind.HalfWidth;
			const bool bVolume = Manager.AddConstSharedFragmentToEntity(Obstacle, Manager.GetOrCreateConstSharedFragment(Volume));
			ObstacleReport += FString::Printf(TEXT(" %s entity %d radius %.0f box %.0f x %.0f cm%s;"), Kind.Name, Obstacle.Index, Kind.Radius,
				Kind.HalfLength * 2.f, Kind.HalfWidth * 2.f, bVolume ? TEXT("") : TEXT(" (VOLUME NOT SET: spheres only)"));
		}
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic player obstacle: on,%s the cars brake for the player in their way (-MassTrafficPlayerObstacle=0 leaves it out)"), *ObstacleReport);
	}
	else
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic player obstacle: off (%s)"), PlayerObstacle == 0 ? TEXT("-MassTrafficPlayerObstacle=0") : TEXT("no spawner or no Mass"));
	}
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
	// -MassTrafficPicture[=150,290]: a picture of a traffic car 50 to 70 m from the player at those
	// times (s), with its distance and roll in the log (the stopgap of candidates 48 to 51 laid the
	// cars there on their side while the count said they moved).
	FString PictureAt;
	if (FParse::Value(FCommandLine::Get(), TEXT("MassTrafficPicture="), PictureAt) || FParse::Param(FCommandLine::Get(), TEXT("MassTrafficPicture")))
	{
		TArray<FString> Times;
		PictureAt.ParseIntoArray(Times, TEXT(","));
		for (const FString& Time : Times)
		{
			PictureTimes.Add(FCString::Atof(*Time));
		}
		if (PictureTimes.IsEmpty())
		{
			PictureTimes = {150.f, 290.f};
		}
		InWorld.GetTimerManager().SetTimer(PictureTimer, FTimerDelegate::CreateUObject(this, &UGTPMassTrafficSubsystem::TickTrafficPicture), 0.25f, true);
	}
	if (StartedSpawners > 0)
	{
		// The junction entities come with the first spawner's Mass commands: looked for every second.
		InWorld.GetTimerManager().SetTimer(LightTimer, FTimerDelegate::CreateUObject(this, &UGTPMassTrafficSubsystem::LightJunctions), 1.f, true);
	}
#else
	UE_LOG(LogGTPMassTraffic, Warning, TEXT("MassTraffic: -Traffic=mass but the plugin is not installed (Plugins/Traffic and Plugins/RuleProcessor from City Sample), legacy traffic kept"));
#endif
}

namespace GTPMassTrafficLocal
{
#if WITH_GTP_MASSTRAFFIC
/**
 * A visualization trait's LOD bands (its LODParams, by name: FMassVisualizationLODParameters) for the near cars with a
 * body: High and Medium (the actor) up to Cm, half the count each, Low (instances) from there; Off as it was.
 */
FString SetVisualLODs(UMassVisualizationTrait& Visual, float Cm, int32 Max)
{
	const FStructProperty* Params = FindFProperty<FStructProperty>(Visual.GetClass(), TEXT("LODParams"));
	void* Values = Params ? Params->ContainerPtrToValuePtr<void>(&Visual) : nullptr;
	if (!Values)
	{
		return TEXT(" LODParams not found, left as they are");
	}
	FString Report;
	const TCHAR* Names[] = {TEXT("BaseLODDistance"), TEXT("VisibleLODDistance")};
	for (const TCHAR* Name : Names)
	{
		const FFloatProperty* Starts = FindFProperty<FFloatProperty>(Params->Struct, Name);
		if (!Starts || Starts->GetSize() < static_cast<int32>(4 * sizeof(float)))
		{
			Report += FString::Printf(TEXT(" %s not found;"), Name);
			continue;
		}
		const float Off = *Starts->ContainerPtrToValuePtr<float>(Values, 3);
		const float Edge = FMath::Min(Cm, Off);
		*Starts->ContainerPtrToValuePtr<float>(Values, 0) = 0.f;
		*Starts->ContainerPtrToValuePtr<float>(Values, 1) = Edge * 0.5f;
		*Starts->ContainerPtrToValuePtr<float>(Values, 2) = Edge;
		Report += FString::Printf(TEXT(" %s High 0, Medium %.0f, Low %.0f, Off %.0f m;"), Name, Edge * 0.005f, Edge * 0.01f, Off * 0.01f);
	}
	const FIntProperty* Counts = FindFProperty<FIntProperty>(Params->Struct, TEXT("LODMaxCount"));
	if (Counts && Counts->GetSize() >= static_cast<int32>(4 * sizeof(int32)))
	{
		*Counts->ContainerPtrToValuePtr<int32>(Values, 0) = Max / 2;
		*Counts->ContainerPtrToValuePtr<int32>(Values, 1) = Max - Max / 2;
		Report += FString::Printf(TEXT(" LODMaxCount High %d, Medium %d"), Max / 2, Max - Max / 2);
	}
	else
	{
		Report += TEXT(" LODMaxCount not found");
	}
	return Report;
}
#endif

/** True for an actor of City Sample's car class (MassTraffic's display actors), found by name. */
bool IsMassCarActor(const AActor* Actor)
{
	// Our car shown by MassTraffic near the player (-MassTrafficBodies); hidden, it waits in Mass's actor pool.
	if (const AGTPVehicle* Ours = Cast<AGTPVehicle>(Actor))
	{
		return Ours->IsShownByMassTraffic() && !Ours->IsHidden();
	}
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
		double NearestJunctionLane = TNumericLimits<double>::Max();
		int32 LanesWithin50 = 0;
		for (TActorIterator<AZoneGraphData> It(World); It; ++It)
		{
			const FZoneGraphStorage& Storage = It->GetStorage();
			for (const FZoneLaneData& Lane : Storage.Lanes)
			{
				if (!Settings->TrafficLaneFilter.Pass(Lane.Tags))
				{
					continue;
				}
				double LaneDistance = TNumericLimits<double>::Max();
				for (int32 Point = Lane.PointsBegin; Point + 1 < Lane.PointsEnd; ++Point)
				{
					LaneDistance = FMath::Min(LaneDistance, FMath::PointDistToSegment(PlayerLocation, Storage.LanePoints[Point], Storage.LanePoints[Point + 1]));
				}
				if (Settings->IntersectionLaneFilter.Pass(Lane.Tags))
				{
					NearestJunctionLane = FMath::Min(NearestJunctionLane, LaneDistance);
					continue;
				}
				NearestLane = FMath::Min(NearestLane, LaneDistance);
				LanesWithin50 += LaneDistance <= 5000.0 ? 1 : 0;
			}
		}
		if (NearestLane < TNumericLimits<double>::Max())
		{
			UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic lanes near the player: nearest road lane %.1f m, %d road lane(s) within 50 m, nearest junction lane %.1f m (player at %s)"),
				NearestLane / 100.0, LanesWithin50, NearestJunctionLane < TNumericLimits<double>::Max() ? NearestJunctionLane / 100.0 : -1.0,
				*PlayerLocation.ToCompactString());
		}
	}
	if (!Loops.IsEmpty())
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic loops: %d car(s) left the map and came back at an entry so far, %d of them in the player's sight, %d under MassTraffic's physics; %d of %d entry lane(s) held closed now (in sight), %d at most; %d held for a car under physics on its dead ends, %d at most"),
			LoopJumps, LoopJumpsInSight, LoopJumpsInPhysics, EntriesInSight.Num(), LoopEntries, MostEntriesHeld, EntriesHeldForPhysics, MostHeldForPhysics);
	}
	LogSimulatedCars(*World, PlayerLocation);
	LogPhysicsCars(*World, PlayerLocation);
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
	int32 MovingUpright = 0;
	int32 LyingOver = 0;
	int32 LyingWithin100 = 0;
	double SpeedSum = 0.0;
	int32 Implausible = 0;
	double Nearest = TNumericLimits<double>::Max();
	// Where the player first meets a carriageway: the traffic must be seen there, 50 m around.
	static const FVector2D Watch = []
	{
		// L_City_RueDeLaRepublique: on rue de la Republique, 5 m short of rue Childebert (a one-way
		// street the pedestrian rue de la Republique crosses 205 m up from the start), the first car
		// street the player meets on the way north. The start itself is 50.1 m from any road lane.
		FVector2D Point(16100.0, 30820.0);
		FString Text;
		if (FParse::Value(FCommandLine::Get(), TEXT("TrafficWatchPoint="), Text))
		{
			FString X;
			FString Y;
			if (Text.Split(TEXT(","), &X, &Y))
			{
				Point = FVector2D(FCString::Atod(*X), FCString::Atod(*Y));
			}
		}
		return Point;
	}();
	double WatchNearest = TNumericLimits<double>::Max();
	int32 WatchWithin50 = 0;
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
			// Upright: the roof within 45 degrees of the vertical. A car lying on its side still slides
			// along its lane over 1 m/s (candidate 49) and is not traffic the player can believe in.
			const bool bUpright = Transforms[Index].GetTransform().GetRotation().GetUpVector().Z >= 0.7071;
			LyingOver += bUpright ? 0 : 1;
			MovingUpright += Speed > 100.f && bUpright ? 1 : 0;
			// Over 200 km/h: a car under MassTraffic's physics pulled across the map in one frame (see
			// GuardLoops), counted apart and kept out of the mean.
			if (Speed > 5555.f)
			{
				++Implausible;
			}
			else
			{
				SpeedSum += Speed;
			}
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
			LyingWithin100 += Distance < 10000.0 && !bUpright ? 1 : 0;
			Within200 += Distance < 20000.0 ? 1 : 0;
			Nearest = FMath::Min(Nearest, Distance);
			const double WatchDistance = FVector2D::Distance(FVector2D(Transforms[Index].GetTransform().GetLocation()), Watch);
			WatchNearest = FMath::Min(WatchNearest, WatchDistance);
			WatchWithin50 += WatchDistance < 5000.0 ? 1 : 0;
		}
	});
	// Cars handed to MassTraffic's recycling (RecycleAtDeadEnds) and not moved yet, and of those the
	// ones its overseer cannot take (its query needs Epic's damage fragment, among others).
	int32 Recyclable = 0;
	int32 RecyclableWithoutDamage = 0;
	int32 RecyclableInJunction = 0;
	int32 RecyclableIncomplete = 0;
	int32 RecyclableTrunkOnly = 0;
	int32 RecyclableCommitted = 0;
	int32 RecyclableOver20 = 0;
	double LongestWait = -1.0;
	FVector LongestWaitLocation = FVector::ZeroVector;
	const double Now = World.GetTimeSeconds();
	TMap<uint64, double> StillWaiting;
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
			const TConstArrayView<FTransformFragment> RecyclableTransforms = Chunk.GetFragmentView<FTransformFragment>();
			for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
			{
				const FMassEntityHandle Entity = Chunk.GetEntity(Index);
				RecyclableWithoutDamage += Manager->GetFragmentDataPtr<FMassTrafficVehicleDamageFragment>(Entity) ? 0 : 1;
				const FZoneGraphTrafficLaneData* Lane = Traffic ? Traffic->GetTrafficLaneData(Locations[Index].LaneHandle) : nullptr;
				LanesWithCars.Add(Lane);
				RecyclableInJunction += Lane && Lane->ConstData.bIsIntersectionLane ? 1 : 0;
				// Why the overseer would not take it: a fragment its query needs is missing, the car may
				// only go onto trunk lanes (we have none), or it has just been handed over.
				const bool bComplete = Manager->GetFragmentDataPtr<FAgentRadiusFragment>(Entity)
					&& Manager->GetFragmentDataPtr<FMassTrafficRandomFractionFragment>(Entity)
					&& Manager->GetFragmentDataPtr<FMassTrafficNextVehicleFragment>(Entity)
					&& Manager->GetFragmentDataPtr<FMassTrafficVehicleLaneChangeFragment>(Entity)
					&& Manager->GetFragmentDataPtr<FMassTrafficVehicleLightsFragment>(Entity)
					&& Manager->GetFragmentDataPtr<FMassTrafficInterpolationFragment>(Entity)
					&& Manager->GetFragmentDataPtr<FMassTrafficObstacleAvoidanceFragment>(Entity)
					&& Manager->GetFragmentDataPtr<FMassTrafficLaneOffsetFragment>(Entity)
					&& Manager->GetFragmentDataPtr<FMassRepresentationFragment>(Entity);
				RecyclableIncomplete += bComplete ? 0 : 1;
				const FMassTrafficVehicleControlFragment* Control = Manager->GetFragmentDataPtr<FMassTrafficVehicleControlFragment>(Entity);
				RecyclableTrunkOnly += !Control ? 1 : (Control->bRestrictedToTrunkLanesOnly ? 1 : 0);
				RecyclableCommitted += Control && Control->bCantStopAtLaneExit ? 1 : 0;
				const double* HandedAt = RecycleHandedAt.Find(Entity.AsNumber());
				const double Wait = HandedAt ? Now - *HandedAt : -1.0;
				if (HandedAt)
				{
					// Only the cars RecycleAtDeadEnds handed over (not MassTraffic's own wrecks).
					StillWaiting.Add(Entity.AsNumber(), *HandedAt);
				}
				RecyclableOver20 += Wait > 20.0 ? 1 : 0;
				if (Wait > LongestWait)
				{
					LongestWait = Wait;
					LongestWaitLocation = RecyclableTransforms[Index].GetTransform().GetLocation();
				}
			}
		});
	}
	RecycleHandedAt = MoveTemp(StillWaiting);
	if (Recyclable > 0 && Settings)
	{
		// The lanes the overseer may put a car onto now, by its own tests (UMassTrafficOverseerProcessor:
		// open, not in a junction, not merging nor splitting, no lane change on or off, its density at
		// most LeastBusiestLaneMaxDensity, beyond LeastBusiestLaneDistanceToPlayerRange from the player's
		// view; its test of cars coming from a junction is left out), and of those the ones at least
		// MinTransferDistance from the car that has waited longest.
		FVector ViewLocation = PlayerLocation;
		FRotator ViewRotation = FRotator::ZeroRotator;
		if (APlayerController* Controller = World.GetFirstPlayerController())
		{
			Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
		int32 Targets = 0;
		int32 TargetsFarFromCar = 0;
		int32 LanesSeen = 0;
		if (UMassTrafficSubsystem* MutableTraffic = World.GetSubsystem<UMassTrafficSubsystem>())
		{
			for (const FMassTrafficZoneGraphData* Data : MutableTraffic->GetMutableTrafficZoneGraphData())
			{
				if (!Data)
				{
					continue;
				}
				for (const FZoneGraphTrafficLaneData& Lane : Data->TrafficLaneDataArray)
				{
					++LanesSeen;
					const float ToPlayer = FMath::Max(static_cast<float>(FVector::Distance(Lane.CenterLocation, ViewLocation)) - static_cast<float>(Lane.Radius), 0.f);
					if (!Lane.bIsOpen || Lane.ConstData.bIsIntersectionLane || !Lane.MergingLanes.IsEmpty() || !Lane.SplittingLanes.IsEmpty()
						|| Lane.NumVehiclesLaneChangingOffOfLane != 0 || Lane.NumVehiclesLaneChangingOntoLane != 0
						|| Lane.FunctionalDensity() > Settings->LeastBusiestLaneMaxDensity
						|| !Settings->LeastBusiestLaneDistanceToPlayerRange.Contains(ToPlayer))
					{
						continue;
					}
					++Targets;
					TargetsFarFromCar += LongestWait >= 0.0 && FVector::Distance(Lane.CenterLocation, LongestWaitLocation) >= Settings->MinTransferDistance ? 1 : 0;
				}
			}
		}
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic recycling: %d car(s) waiting, longest %.0f s (%.0f m from the player), %d over 20 s | missing a fragment the overseer needs=%d trunk lanes only=%d committed to their next lane=%d inside a junction=%d | given back to the traffic so far=%d | lanes the overseer may take now=%d of %d, %d of them %.0f m or more from the longest waiting car"),
			Recyclable, LongestWait, LongestWait >= 0.0 ? FVector::Distance(LongestWaitLocation, ViewLocation) / 100.0 : -1.0, RecyclableOver20,
			RecyclableIncomplete, RecyclableTrunkOnly, RecyclableCommitted, RecyclableInJunction, GivenBack, Targets, LanesSeen, TargetsFarFromCar, Settings->MinTransferDistance / 100.0);
	}
	FString Network;
	if (!bNetworkLogged && Cars > 0)
	{
		// Once, when the cars are there: the network they drive on and the density.
		bNetworkLogged = true;
		Network = FString::Printf(TEXT(" | car lanes=%d (%d dead ends), %.1f km, %.1f cars per km"), CarLanes, DeadEnds.Num(),
			Length / 100000.0, Length > 0.0 ? Cars / (Length / 100000.0) : 0.0);
	}
	// moving= counts the upright cars only since candidate 52; moving_with_lying= is the count of
	// candidates 48 to 51, the cars lying over included.
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic cars: simulated=%d moving=%d (over 1 m/s, upright) moving_with_lying=%d lying_over=%d stopped on a dead-end lane=%d waiting for recycling=%d (without Epic's damage fragment=%d) recycled so far=%d mean speed=%.0f km/h (over 200 km/h, left out=%d) | within 100 m=%d within 200 m=%d nearest=%s%s"),
		Cars, MovingUpright, Moving, LyingOver, AtDeadEnd, Recyclable, RecyclableWithoutDamage, Recycled, Cars > Implausible ? SpeedSum / (Cars - Implausible) * 0.036 : 0.0, Implausible, Within100, Within200,
		Cars > 0 ? *FString::Printf(TEXT("%.0f m"), Nearest / 100.0) : TEXT("none"), *Network);
	if (Cars > 0)
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic lying over t=%.0fs: %d of %d car(s) within 100 m of the player (%.0f %%), %d of %d in all (roof over 45 deg from the vertical)"),
			World.GetTimeSeconds(), LyingWithin100, Within100, Within100 > 0 ? 100.0 * LyingWithin100 / Within100 : 0.0, LyingOver, Cars);
	}
	if (Cars > 0)
	{
		++WatchCounts;
		WatchCountsWithCar += WatchWithin50 > 0 ? 1 : 0;
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic watch point X=%.0f Y=%.0f (rue Childebert at rue de la Republique, -TrafficWatchPoint=X,Y): %d car(s) within 50 m now, nearest %.1f m; a car within 50 m in %d of %d count(s) so far"),
			Watch.X, Watch.Y, WatchWithin50, WatchNearest / 100.0, WatchCountsWithCar, WatchCounts);
	}
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

void UGTPMassTrafficSubsystem::TickTrafficPicture()
{
	UWorld* World = GetWorld();
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Player = Controller ? Controller->GetPawn() : nullptr;
	UMassEntitySubsystem* Mass = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Player || !Mass)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	AActor* Camera = PictureCamera.Get();
	if (PicturePhase == 2)
	{
		// The tick after the picture: the view goes back to the player.
		if (Camera && Controller->GetViewTarget() == Camera)
		{
			Controller->SetViewTarget(Player);
		}
		if (Camera)
		{
			Camera->Destroy();
		}
		PictureCamera = nullptr;
		PicturePhase = 0;
		++PicturesTaken;
		return;
	}
	if (PicturePhase == 0 && (!PictureTimes.IsValidIndex(PicturesTaken) || Now < PictureTimes[PicturesTaken]))
	{
		return;
	}
	// The traffic cars: where they are and how they stand.
	struct FSeen
	{
		int32 Index = 0;
		int32 Serial = 0;
		FTransform Transform;
		bool bPhysics = false;
	};
	TArray<FSeen> Seen;
	FMassEntityManager& Manager = Mass->GetMutableEntityManager();
	FMassEntityQuery Query;
	Query.Initialize(Manager.AsShared());
	Query.AddTagRequirement<FMassTrafficVehicleTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	FMassExecutionContext Context(Manager, 0.f);
	Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FTransformFragment> Transforms = Chunk.GetFragmentView<FTransformFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			const FMassEntityHandle Entity = Chunk.GetEntity(Index);
			FSeen Car;
			Car.Index = Entity.Index;
			Car.Serial = Entity.SerialNumber;
			Car.Transform = Transforms[Index].GetTransform();
			Car.bPhysics = Manager.GetFragmentDataPtr<FMassTrafficVehiclePhysicsFragment>(Entity) != nullptr;
			Seen.Add(MoveTemp(Car));
		}
	});
	const FVector PlayerAt = Player->GetActorLocation();
	int32 Within100 = 0;
	int32 LyingWithin100 = 0;
	for (const FSeen& Car : Seen)
	{
		if (FVector::Dist2D(Car.Transform.GetLocation(), PlayerAt) < 10000.0)
		{
			++Within100;
			LyingWithin100 += Car.Transform.GetRotation().GetUpVector().Z < 0.7071 ? 1 : 0;
		}
	}
	if (PicturePhase == 0)
	{
		// The car: 50 to 70 m from the player, the one lying over the most, else the one nearest 60 m.
		int32 Best = INDEX_NONE;
		double BestScore = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Seen.Num(); ++Index)
		{
			const double Distance = FVector::Dist2D(Seen[Index].Transform.GetLocation(), PlayerAt);
			if (Distance < 5000.0 || Distance > 7000.0)
			{
				continue;
			}
			const double Up = Seen[Index].Transform.GetRotation().GetUpVector().Z;
			const double Score = Up < 0.7071 ? Up - 10.0 : FMath::Abs(Distance - 6000.0) / 1000.0;
			if (Score < BestScore)
			{
				BestScore = Score;
				Best = Index;
			}
		}
		if (Best == INDEX_NONE)
		{
			// Tried again every 5 s, for a minute at most.
			const bool bGiveUp = ++PictureRetries > 12;
			UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic picture %d: no traffic car 50 to 70 m from the player at t=%.0fs (%d car(s) within 100 m), %s"),
				PicturesTaken + 1, Now, Within100, bGiveUp ? TEXT("no picture") : TEXT("tried again in 5 s"));
			if (bGiveUp)
			{
				PictureRetries = 0;
				++PicturesTaken;
				return;
			}
			PictureTimes[PicturesTaken] = static_cast<float>(Now) + 5.f;
			return;
		}
		PictureRetries = 0;
		// The eye: where the player stands, else 60 m behind or ahead of the car on its heading; the
		// first one that sees the car.
		const FSeen& Car = Seen[Best];
		const FVector Target = Car.Transform.GetLocation() + FVector(0.0, 0.0, 80.0);
		const FVector Forward = Car.Transform.GetRotation().GetForwardVector().GetSafeNormal2D();
		const FVector Eyes[] = {PlayerAt + FVector(0.0, 0.0, 90.0), Car.Transform.GetLocation() - Forward * 6000.0 + FVector(0.0, 0.0, 170.0),
			Car.Transform.GetLocation() + Forward * 6000.0 + FVector(0.0, 0.0, 170.0)};
		static const TCHAR* EyeNames[] = {TEXT("where the player stands"), TEXT("60 m behind the car"), TEXT("60 m ahead of the car")};
		int32 Eye = 0;
		FString Blocked;
		for (int32 Try = 0; Try < 3; ++Try)
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPMassTrafficPicture), false, Player);
			if (!World->LineTraceSingleByChannel(Hit, Eyes[Try], Target, ECC_Visibility, Params) || FVector::Dist(Hit.ImpactPoint, Target) < 400.0)
			{
				Eye = Try;
				break;
			}
			Blocked += FString::Printf(TEXT("%s%s by %s"), Blocked.IsEmpty() ? TEXT("") : TEXT(", "), EyeNames[Try], *GetNameSafe(Hit.GetActor()));
			Eye = 0;
		}
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACameraActor* Lens = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((Target - Eyes[Eye]).Rotation(), Eyes[Eye]), Spawn);
		if (!Lens)
		{
			UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic picture %d: no camera"), PicturesTaken + 1);
			++PicturesTaken;
			return;
		}
		if (UCameraComponent* Component = Lens->GetCameraComponent())
		{
			Component->SetFieldOfView(70.f);
			Component->SetConstraintAspectRatio(false);
			Component->PostProcessBlendWeight = 1.f;
			Component->PostProcessSettings.bOverride_MotionBlurAmount = true;
			Component->PostProcessSettings.MotionBlurAmount = 0.f;
		}
		Controller->SetViewTarget(Lens);
		PictureCamera = Lens;
		PictureEntityIndex = Car.Index;
		PictureEntitySerial = Car.Serial;
		PictureEye = EyeNames[Eye];
		PictureBlocked = Blocked;
		PictureAskedAt = Now;
		PicturePhase = 1;
		return;
	}
	// Half a second on: the camera turned to the car where it is now, the picture, the car in numbers.
	if (Now < PictureAskedAt + 0.5)
	{
		return;
	}
	const FSeen* Car = Seen.FindByPredicate([this](const FSeen& Each) { return Each.Index == PictureEntityIndex && Each.Serial == PictureEntitySerial; });
	if (!Car || !Camera)
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic picture %d: the car is gone (recycled or out of the traffic), no picture"), PicturesTaken + 1);
		PicturePhase = 2;
		return;
	}
	const FVector Target = Car->Transform.GetLocation() + FVector(0.0, 0.0, 80.0);
	Camera->SetActorRotation((Target - Camera->GetActorLocation()).Rotation());
	const FString Shot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ScreenShotDir(), FString::Printf(TEXT("mass-traffic-%d.png"), PicturesTaken + 1)));
	FScreenshotRequest::RequestScreenshot(Shot, false, false);
	const FRotator Rotation = Car->Transform.Rotator();
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic picture: %s at t=%.0fs, car entity %d (%s) at %s, %.0f m from the player and %.0f m from the camera (%s%s), roll %.0f pitch %.0f, %s; within 100 m of the player %d car(s) lying over of %d"),
		*Shot, Now, PictureEntityIndex, Car->bPhysics ? TEXT("under MassTraffic's simple physics") : TEXT("not under MassTraffic's physics"),
		*Car->Transform.GetLocation().ToCompactString(), FVector::Dist2D(Car->Transform.GetLocation(), PlayerAt) / 100.0,
		FVector::Dist(Target, Camera->GetActorLocation()) / 100.0, *PictureEye, PictureBlocked.IsEmpty() ? TEXT("") : *FString::Printf(TEXT("; view blocked: %s"), *PictureBlocked),
		Rotation.Roll, Rotation.Pitch, Car->Transform.GetRotation().GetUpVector().Z < 0.7071 ? TEXT("LYING OVER") : TEXT("upright"), LyingWithin100, Within100);
	PicturePhase = 2;
}

void UGTPMassTrafficSubsystem::LogPhysicsCars(UWorld& World, const FVector& PlayerLocation)
{
	// The cars MassTraffic drives with its simple physics (FMassTrafficVehiclePhysicsFragment, near the
	// player). With the real wheels on its template (candidates 47 and 48) the traffic moving fell from
	// 102 to 48-53 cars: what each extracted template holds (once each), and how the nearest of these
	// cars drive now. The template has the real wheels and City Sample's centre of mass after gate
	// 54 (stopgap of candidates 48 to 55 with -MassTemplate46); no car gets that physics by default
	// since then (GTPMassModule, -MassTrafficPhysicsRadius=<m> gives it back).
	UMassEntitySubsystem* Mass = World.GetSubsystem<UMassEntitySubsystem>();
	UZoneGraphSubsystem* Graph = World.GetSubsystem<UZoneGraphSubsystem>();
	if (!Mass)
	{
		return;
	}
	struct FCar
	{
		double Distance = 0.0;
		FString Line;
	};
	TArray<FCar> Cars;
	int32 Total = 0;
	int32 Stopped = 0;
	int32 WheelsUp = 0;
	int32 LyingOver = 0;
	FMassEntityManager& Manager = Mass->GetMutableEntityManager();
	FMassEntityQuery Query;
	Query.Initialize(Manager.AsShared());
	Query.AddTagRequirement<FMassTrafficVehicleTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassTrafficVehicleControlFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassTrafficPIDVehicleControlFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassTrafficVehiclePhysicsFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassTrafficObstacleAvoidanceFragment>(EMassFragmentAccess::ReadOnly);
	FMassExecutionContext Context(Manager, 0.f);
	Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FTransformFragment> Transforms = Chunk.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassTrafficVehicleControlFragment> Controls = Chunk.GetFragmentView<FMassTrafficVehicleControlFragment>();
		const TConstArrayView<FMassTrafficPIDVehicleControlFragment> Inputs = Chunk.GetFragmentView<FMassTrafficPIDVehicleControlFragment>();
		const TConstArrayView<FMassTrafficVehiclePhysicsFragment> Physics = Chunk.GetFragmentView<FMassTrafficVehiclePhysicsFragment>();
		const TConstArrayView<FMassZoneGraphLaneLocationFragment> Locations = Chunk.GetFragmentView<FMassZoneGraphLaneLocationFragment>();
		const TConstArrayView<FMassTrafficObstacleAvoidanceFragment> Avoidances = Chunk.GetFragmentView<FMassTrafficObstacleAvoidanceFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			const FMassTrafficSimpleVehiclePhysicsSim& Sim = Physics[Index].VehicleSim;
			const FMassTrafficSimpleVehiclePhysicsConfig& Config = Sim.Setup();
			const int32 NumWheels = FMath::Min(Sim.WheelSims.Num(), Sim.SuspensionSims.Num());
			if (!LoggedTemplates.Contains(&Config))
			{
				LoggedTemplates.Add(&Config);
				FString Wheels;
				for (int32 Wheel = 0; Wheel < NumWheels; ++Wheel)
				{
					// Only what the sims expose: TVehicleSystem<Chaos::FSimpleWheelConfig / FSimpleSuspensionConfig>::Setup()
					// is not exported by Chaos Vehicles in 5.8 (LNK2019 on the PC), so radius and springs are not read here.
					Wheels += FString::Printf(TEXT(" | wheel %d at %s driven=%d"), Wheel,
						*Sim.SuspensionSims[Wheel].GetLocalRestingPosition().ToCompactString(), Sim.WheelSims[Wheel].EngineEnabled ? 1 : 0);
				}
				UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic physics template: mass %.0f kg (peripheral %.0f), centre of mass %s, body to actor %s, inverse inertia %s, %d driven wheel(s), %d wheel(s)%s"),
					Config.Mass, Config.PeripheralMass, *Config.CenterOfMass.ToCompactString(), *Config.BodyToActor.GetTranslation().ToCompactString(),
					*Config.InverseMomentOfInertia.ToCompactString(), Config.NumDrivenWheels, NumWheels, *Wheels);
			}
			++Total;
			const float Kmh = Controls[Index].Speed * 0.036f;
			Stopped += Kmh < 3.6f ? 1 : 0;
			int32 InContact = 0;
			for (int32 Wheel = 0; Wheel < NumWheels; ++Wheel)
			{
				InContact += Sim.WheelSims[Wheel].InContact() ? 1 : 0;
			}
			WheelsUp += InContact < NumWheels ? 1 : 0;
			const FTransform& Transform = Transforms[Index].GetTransform();
			// What the player sees: a car whose roof leans more than 45 degrees from the vertical. The gate of
			// candidate 49 counted 97 cars moving while 5 of the 6 nearest of these lay on their side.
			LyingOver += Transform.GetRotation().GetUpVector().Z < 0.7071 ? 1 : 0;
			const double Distance = FVector::Dist(Transform.GetLocation(), PlayerLocation);
			if (Cars.Num() >= 6 && Distance >= Cars.Last().Distance)
			{
				continue;
			}
			// Where it stands against its lane: height above it and distance beside it.
			FString OnLane = TEXT("lane not found");
			FZoneGraphLaneLocation LanePoint;
			if (Graph && Graph->CalculateLocationAlongLane(Locations[Index].LaneHandle, Locations[Index].DistanceAlongLane, LanePoint))
			{
				const FVector Offset = Transform.GetLocation() - LanePoint.Position;
				const double Above = FVector::DotProduct(Offset, LanePoint.Up);
				const FVector Flat = Offset - LanePoint.Up * Above;
				const double Beside = (Flat - LanePoint.Direction * FVector::DotProduct(Flat, LanePoint.Direction)).Size();
				OnLane = FString::Printf(TEXT("%.0f cm above its lane, %.0f cm beside it, yaw off %.0f deg"), Above, Beside,
					FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Transform.GetRotation().GetForwardVector().GetSafeNormal2D(), LanePoint.Direction.GetSafeNormal2D()), -1.0, 1.0))));
			}
			FString Wheels;
			for (int32 Wheel = 0; Wheel < NumWheels; ++Wheel)
			{
				const Chaos::FSimpleWheelSim& WheelSim = Sim.WheelSims[Wheel];
				Wheels += FString::Printf(TEXT(" | w%d contact=%d load %.0f slip %.0f drive %.0f at %s"), Wheel, WheelSim.InContact() ? 1 : 0, WheelSim.GetWheelLoadForce(),
					WheelSim.GetSlipMagnitude(), WheelSim.GetDriveTorque(), Sim.WheelLocalLocations.IsValidIndex(Wheel) ? *Sim.WheelLocalLocations[Wheel].ToCompactString() : TEXT("?"));
			}
			const FRotator Rotation = Transform.Rotator();
			const FMassTrafficPIDVehicleControlFragment& Input = Inputs[Index];
			FCar Car;
			Car.Distance = Distance;
			auto MetresOrNone = [](float Cm) { return Cm < 1.e7f ? FString::Printf(TEXT("%.0f m"), Cm / 100.f) : FString(TEXT("none")); };
			Car.Line = FString::Printf(TEXT("%.0f m from the player: %.0f km/h, throttle %.2f brake %.2f steer %.2f handbrake %d, gear %d %.0f rpm, pitch %.1f roll %.1f, %s, next car %s, obstacle %s%s"),
				Distance / 100.0, Kmh, Input.Throttle, Input.Brake, Input.Steering, Input.bHandbrake ? 1 : 0, Sim.TransmissionSim.GetCurrentGear(),
				Sim.EngineSim.GetEngineRPM(), Rotation.Pitch, Rotation.Roll, *OnLane,
				*MetresOrNone(Avoidances[Index].DistanceToNext), *MetresOrNone(Avoidances[Index].DistanceToCollidingObstacle), *Wheels);
			Cars.Add(MoveTemp(Car));
			Cars.Sort([](const FCar& A, const FCar& B) { return A.Distance < B.Distance; });
			if (Cars.Num() > 6)
			{
				Cars.Pop();
			}
		}
	});
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic physics cars t=%.0fs: %d under MassTraffic's physics, %d stopped, %d with a wheel off the road, %d lying over (roof over 45 deg from the vertical)"),
		World.GetTimeSeconds(), Total, Stopped, WheelsUp, LyingOver);
	for (const FCar& Car : Cars)
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic physics car: %s"), *Car.Line);
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
	LoopExitLanes.Reset();
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
			LoopExitLanes.Add(&Lane);
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

void UGTPMassTrafficSubsystem::ShowNearCarsAsInstances(const TArray<AMassSpawner*>& InSpawners)
{
	UWorld* World = GetWorld();
	if (!World || FParse::Param(FCommandLine::Get(), TEXT("TrafficActorLODs")))
	{
		return;
	}
	// Protected on AMassSpawner: reached through reflection (as in UGTPMassSpawnerSetup).
	const FArrayProperty* Property = CastField<FArrayProperty>(AMassSpawner::StaticClass()->FindPropertyByName(TEXT("EntityTypes")));
	const FStructProperty* Inner = Property ? CastField<FStructProperty>(Property->Inner) : nullptr;
	if (!Inner || Inner->Struct != FMassSpawnedEntityType::StaticStruct())
	{
		UE_LOG(LogGTPMassTraffic, Warning, TEXT("MassTraffic visuals: AMassSpawner has no EntityTypes array of FMassSpawnedEntityType, left as they are"));
		return;
	}
	int32 Traits = 0;
	int32 Changed = 0;
	int32 LODsChanged = 0;
	int32 WithoutMeshes = 0;
	// Near cars with a body: our car of the config, moved by Mass (see the header).
	float BodyMetres = 35.f;
	FParse::Value(FCommandLine::Get(), TEXT("MassTrafficBodies="), BodyMetres);
	int32 BodyMax = 12;
	FParse::Value(FCommandLine::Get(), TEXT("MassTrafficBodiesMax="), BodyMax);
	const float BodyCm = FMath::Max(BodyMetres, 0.f) * 100.f;
	BodyMax = FMath::Max(BodyMax, 0);
	int32 WithBodies = 0;
	int32 BodyClassChanged = 0;
	int32 HighResCleared = 0;
	FString BodyClasses;
	FString BodyReport;
	TSet<const UMassEntityConfigAsset*> Visited;
	TSet<const UMassEntityConfigAsset*> ChangedConfigs;
	TArray<const UMassEntityConfigAsset*> Leaves;
	for (AMassSpawner* Spawner : InSpawners)
	{
		if (!Spawner)
		{
			continue;
		}
		FScriptArrayHelper Helper(Property, Property->ContainerPtrToValuePtr<void>(Spawner));
		for (int32 Index = 0; Index < Helper.Num(); ++Index)
		{
			FMassSpawnedEntityType* Type = reinterpret_cast<FMassSpawnedEntityType*>(Helper.GetRawPtr(Index));
			const UMassEntityConfigAsset* Leaf = Type ? Type->GetEntityConfig() : nullptr;
			if (Leaf)
			{
				Leaves.AddUnique(Leaf);
			}
			// A driving car's own car class (the parked cars have none): its physics template, BP_GTP_* by
			// place_masstraffic.py, looked for in the config and its parents.
			UClass* BodyClass = nullptr;
			int32 Depth = 0;
			for (const UMassEntityConfigAsset* Config = Leaf; Config && !BodyClass && BodyCm > 0.f && BodyMax > 0 && Depth < 16; Config = Config->GetConfig().GetParent(), ++Depth)
			{
				for (UMassEntityTraitBase* Trait : Config->GetConfig().GetTraits())
				{
					if (const UMassTrafficVehicleSimulationTrait* Simulation = Cast<UMassTrafficVehicleSimulationTrait>(Trait))
					{
						BodyClass = Simulation->Params.PhysicsVehicleTemplateActor.Get();
						break;
					}
				}
			}
			// The config and its parents: a trait may come from a parent config. A config already seen
			// had its parents seen with it.
			for (const UMassEntityConfigAsset* Config = Leaf; Config; Config = Config->GetConfig().GetParent())
			{
				if (Visited.Contains(Config))
				{
					break;
				}
				Visited.Add(Config);
				for (UMassEntityTraitBase* Trait : Config->GetConfig().GetTraits())
				{
					UMassVisualizationTrait* Visual = Cast<UMassVisualizationTrait>(Trait);
					if (!Visual)
					{
						continue;
					}
					++Traits;
					WithoutMeshes += Visual->StaticMeshInstanceDesc.Meshes.IsEmpty() ? 1 : 0;
					if (BodyClass)
					{
						// Epic's LowResSpawnedActor up to BodyCm (its trait's default: High and Medium are actors up
						// to 45 m, at most 10 + 40), instances beyond. Mass moves that actor to the entity every frame.
						BodyClassChanged += Visual->LowResTemplateActor && Visual->LowResTemplateActor.Get() != BodyClass ? 1 : 0;
						Visual->LowResTemplateActor = BodyClass;
						// MassTraffic's actor management takes an actor of the high-res class for its physics car
						// (InitHighResActor: snapshot, PID inputs): ours is the low-res one only.
						if (Visual->HighResTemplateActor.Get() == BodyClass)
						{
							Visual->HighResTemplateActor = TSubclassOf<AActor>();
							++HighResCleared;
						}
						Visual->Params.LODRepresentation[EMassLOD::High] = EMassRepresentationType::LowResSpawnedActor;
						Visual->Params.LODRepresentation[EMassLOD::Medium] = EMassRepresentationType::LowResSpawnedActor;
						Visual->Params.LODRepresentation[EMassLOD::Low] = EMassRepresentationType::StaticMeshInstance;
						BodyReport = GTPMassTrafficLocal::SetVisualLODs(*Visual, BodyCm, BodyMax);
						if (!BodyClasses.Contains(BodyClass->GetName()))
						{
							BodyClasses += FString::Printf(TEXT(" %s"), *BodyClass->GetName());
						}
						++WithBodies;
						ChangedConfigs.Add(Config);
					}
					int32 Here = 0;
					for (int32 LOD = 0; LOD < EMassLOD::Max; ++LOD)
					{
						EMassRepresentationType& Wanted = Visual->Params.LODRepresentation[LOD];
						if ((Wanted == EMassRepresentationType::HighResSpawnedActor && !Visual->HighResTemplateActor)
							|| (Wanted == EMassRepresentationType::LowResSpawnedActor && !Visual->LowResTemplateActor))
						{
							Wanted = EMassRepresentationType::StaticMeshInstance;
							++Here;
						}
					}
					LODsChanged += Here;
					Changed += Here > 0 ? 1 : 0;
					if (Here > 0)
					{
						ChangedConfigs.Add(Config);
					}
				}
			}
		}
	}
	// The spawners built their templates when registered (AMassSpawner::RegisterEntityTemplates):
	// those whose config or a parent of it changed are dropped, DoSpawning builds them again.
	int32 Rebuilt = 0;
	for (const UMassEntityConfigAsset* Leaf : Leaves)
	{
		bool bChanged = false;
		int32 Depth = 0;
		for (const UMassEntityConfigAsset* Config = Leaf; Config && !bChanged && Depth < 16; Config = Config->GetConfig().GetParent(), ++Depth)
		{
			bChanged = ChangedConfigs.Contains(Config);
		}
		if (bChanged)
		{
			Leaf->GetConfig().DestroyEntityTemplate(*World);
			++Rebuilt;
		}
	}
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic visuals: %d of %d visualization trait(s) asked for a display actor they do not have, %d LOD(s) shown as instances instead (%d trait(s) without instance meshes), %d entity template(s) rebuilt at spawning; -TrafficActorLODs leaves them"),
		Changed, Traits, LODsChanged, WithoutMeshes, Rebuilt);
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic near cars with a body: %s; %d visualization trait(s) given our car as a Mass-moved actor (LowResSpawnedActor) within %.0f m, %d at most (-MassTrafficBodies=<m>, 0 = instances only; -MassTrafficBodiesMax),%s; %d trait(s) reached with two car classes, %d high-res class(es) cleared (the same as ours); LODs:%s"),
		WithBodies > 0 ? TEXT("on") : TEXT("OFF"), WithBodies, BodyCm / 100.f, BodyMax, BodyClasses.IsEmpty() ? TEXT(" no car class") : *BodyClasses,
		BodyClassChanged, HighResCleared, BodyReport.IsEmpty() ? TEXT(" none") : *BodyReport);
}

void UGTPMassTrafficSubsystem::LightJunctions()
{
	UWorld* World = GetWorld();
	UMassEntitySubsystem* Mass = World ? World->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!World || !Mass || bJunctionsLit)
	{
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("TrafficStopSigns")))
	{
		bJunctionsLit = true;
		World->GetTimerManager().ClearTimer(LightTimer);
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic junctions: left as stop signs (-TrafficStopSigns)"));
		return;
	}
	float GoSeconds = 10.f;
	FParse::Value(FCommandLine::Get(), TEXT("TrafficJunctionGoSeconds="), GoSeconds);
	GoSeconds = FMath::Clamp(GoSeconds, 3.f, 60.f);
	int32 Junctions = 0;
	int32 AlreadyLit = 0;
	int32 Lit = 0;
	int32 SidePeriods = 0;
	FMassEntityManager& Manager = Mass->GetMutableEntityManager();
	FMassEntityQuery Query;
	Query.Initialize(Manager.AsShared());
	Query.AddRequirement<FMassTrafficIntersectionFragment>(EMassFragmentAccess::ReadWrite);
	FMassExecutionContext Context(Manager, 0.f);
	Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const TArrayView<FMassTrafficIntersectionFragment> Fragments = Chunk.GetMutableFragmentView<FMassTrafficIntersectionFragment>();
		for (FMassTrafficIntersectionFragment& Junction : Fragments)
		{
			++Junctions;
			if (Junction.bHasTrafficLights)
			{
				++AlreadyLit;
				continue;
			}
			if (Junction.Periods.IsEmpty())
			{
				continue;
			}
			// Built as a stop sign: one period per side (Epic's StandardMinimumTrafficGoSeconds, 5 s),
			// then one for the crossings. A side's period now lasts GoSeconds; MassTraffic's own
			// traffic-light rule ends it sooner once the junction is clear and no car waits.
			Junction.bHasTrafficLights = true;
			++Lit;
			for (FMassTrafficPeriod& Period : Junction.Periods)
			{
				if (!Period.VehicleLanes.IsEmpty() && static_cast<float>(Period.Duration) < GoSeconds)
				{
					Period.Duration = GoSeconds;
					++SidePeriods;
				}
			}
		}
	});
	++LightTries;
	if (Junctions == 0 && LightTries < 30)
	{
		return;
	}
	bJunctionsLit = true;
	World->GetTimerManager().ClearTimer(LightTimer);
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic junctions: %d of %d junction(s) built as stop signs given traffic-light turns (%d side period(s) of %.0f s, ended early once the junction is clear with no car waiting; no light drawn), %d already lit; -TrafficStopSigns keeps the stop signs"),
		Lit, Junctions, SidePeriods, GoSeconds, AlreadyLit);
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
		SourcesOf.Add(Key(Loop.DataIndex, Loop.ToLane), Loop.FromLane);
	}
	// The cars that looped since the last check: now on an entry, coming from one of its dead ends.
	// And the lanes of the cars MassTraffic simulates with its simple physics (FMassTrafficVehiclePhysicsFragment,
	// within its High/Medium LOD distance of the player, 200 m by default).
	FMassEntityManager& Manager = Mass->GetMutableEntityManager();
	FMassEntityQuery Query;
	Query.Initialize(Manager.AsShared());
	Query.AddTagRequirement<FMassTrafficVehicleTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FMassTrafficVehicleControlFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassZoneGraphLaneLocationFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassTrafficVehiclePhysicsFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	TMap<uint64, int32> LaneNow;
	TSet<int64> PhysicsLanes;
	FMassExecutionContext Context(Manager, 0.f);
	Query.ForEachEntityChunk(Context, [&](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FMassTrafficVehicleControlFragment> Controls = Chunk.GetFragmentView<FMassTrafficVehicleControlFragment>();
		const TConstArrayView<FMassZoneGraphLaneLocationFragment> Locations = Chunk.GetFragmentView<FMassZoneGraphLaneLocationFragment>();
		const bool bPhysics = Chunk.GetFragmentView<FMassTrafficVehiclePhysicsFragment>().Num() > 0;
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			const uint64 Car = Chunk.GetEntity(Index).AsNumber();
			const FZoneGraphLaneHandle& Lane = Locations[Index].LaneHandle;
			LaneNow.Add(Car, Lane.Index);
			if (bPhysics)
			{
				PhysicsLanes.Add(Key(Lane.DataHandle.Index, Lane.Index));
				// Coming up to a dead end (its next lane chosen): held before it gets there, as a check
				// every 0.25 s can miss a car that crosses a short dead end in between.
				const FZoneGraphTrafficLaneData* Next = Controls[Index].NextLane;
				if (Next && LoopExitLanes.Contains(Next))
				{
					PhysicsLanes.Add(Key(Next->LaneHandle.DataHandle.Index, Next->LaneHandle.Index));
				}
			}
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
				LoopJumpsInPhysics += bPhysics ? 1 : 0;
			}
		}
	});
	// A car under MassTraffic's physics must not jump: its body would be pulled onto the entry in one
	// frame, and the speed taken from that move (UpdateCoMVelocity) sends it off at thousands of km/h.
	// Its entry stays closed while such a car is on one of the dead ends or heading into one; once
	// stopped there out of sight, RecycleAtDeadEnds hands it to MassTraffic's recycling, which moves
	// it without physics.
	TSet<int64> HeldForPhysics;
	for (const FLoop& Loop : Loops)
	{
		if (PhysicsLanes.Contains(Key(Loop.DataIndex, Loop.FromLane)))
		{
			HeldForPhysics.Add(Key(Loop.DataIndex, Loop.ToLane));
		}
	}
	for (const FLoop& Loop : Loops)
	{
		FMassTrafficZoneGraphData* Data = AllData.IsValidIndex(Loop.Slot) ? AllData[Loop.Slot] : nullptr;
		if (Data && Data->TrafficLaneDataArray.IsValidIndex(Loop.To))
		{
			const int64 Entry = Key(Loop.DataIndex, Loop.ToLane);
			Data->TrafficLaneDataArray[Loop.To].bIsOpen = !InSightNow.Contains(Entry) && !HeldForPhysics.Contains(Entry);
		}
	}
	LaneOfCar = MoveTemp(LaneNow);
	EntriesInSight = MoveTemp(InSightNow);
	MostEntriesHeld = FMath::Max(MostEntriesHeld, EntriesInSight.Num());
	EntriesHeldForPhysics = HeldForPhysics.Num();
	MostHeldForPhysics = FMath::Max(MostHeldForPhysics, EntriesHeldForPhysics);
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
	{
		// A car handed over here that turns out to be committed to its next lane (bCantStopAtLaneExit,
		// set by MassTraffic between this check and the tag change) is never moved by the overseer: it
		// goes back to the traffic, where MassTraffic takes it on or releases it.
		TArray<FMassEntityHandle> ToGiveBack;
		FMassEntityQuery WaitingQuery;
		WaitingQuery.Initialize(Manager.AsShared());
		WaitingQuery.AddTagRequirement<FMassTrafficRecyclableVehicleTag>(EMassFragmentPresence::All);
		WaitingQuery.AddRequirement<FMassTrafficVehicleControlFragment>(EMassFragmentAccess::ReadOnly);
		FMassExecutionContext WaitingContext(Manager, 0.f);
		WaitingQuery.ForEachEntityChunk(WaitingContext, [&](FMassExecutionContext& Chunk)
		{
			const TConstArrayView<FMassTrafficVehicleControlFragment> Controls = Chunk.GetFragmentView<FMassTrafficVehicleControlFragment>();
			for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
			{
				if (Controls[Index].bCantStopAtLaneExit && RecycleHandedAt.Contains(Chunk.GetEntity(Index).AsNumber()))
				{
					ToGiveBack.Add(Chunk.GetEntity(Index));
				}
			}
		});
		for (const FMassEntityHandle& Entity : ToGiveBack)
		{
			Manager.Defer().SwapTags<FMassTrafficRecyclableVehicleTag, FMassTrafficVehicleTag>(Entity);
			RecycleHandedAt.Remove(Entity.AsNumber());
		}
		GivenBack += ToGiveBack.Num();
		if (!ToGiveBack.IsEmpty())
		{
			UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic dead ends: %d car(s) given back to the traffic, committed to their next lane (MassTraffic's teleport refuses them; %d in all)"),
				ToGiveBack.Num(), GivenBack);
		}
	}
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
			// A dead end linked to an entry held closed for a car under physics (GuardLoops) counts as one
			// with no lane after it; one held because the player may see it is left to wait.
			const FZoneGraphTrafficLaneData* Held = LoopExitLanes.Contains(Lane) && Lane->NextLanes.Num() == 1 ? Lane->NextLanes[0] : nullptr;
			const bool bHeldExit = Held && !Held->bIsOpen
				&& !EntriesInSight.Contains((static_cast<int64>(Held->LaneHandle.DataHandle.Index) << 32) | static_cast<uint32>(Held->LaneHandle.Index));
			const bool bAtDeadEnd = bStopped && (Lane->NextLanes.IsEmpty() || bHeldExit)
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
			// A car committed to its next lane (it could not stop before the lane exit, and reserved a
			// place on that lane) is never moved by MassTraffic's teleport
			// (TeleportVehicleToAnotherLane returns at once): handed over, it would wait for good. It is
			// left to MassTraffic, which takes it on or releases it once it stands still.
			if (Controls[Index].bCantStopAtLaneExit)
			{
				CommittedLeft.Add(Chunk.GetEntity(Index).AsNumber());
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
		RecycleHandedAt.Add(Entity.AsNumber(), World->GetTimeSeconds());
	}
	Recycled += ToRecycle.Num();
	if (ToRecycle.Num() > 0 && (Recycled <= 20 || Recycled % 50 < ToRecycle.Num()))
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic dead ends: %d car(s) handed to MassTraffic's recycling (%d in all, %d given Epic's damage fragment; %d committed to their next lane left to the traffic so far)"),
			ToRecycle.Num(), Recycled, DamageAdded, CommittedLeft.Num());
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

void UGTPMassTrafficSubsystem::LightJunctions()
{
}

void UGTPMassTrafficSubsystem::ShowNearCarsAsInstances(const TArray<AMassSpawner*>& InSpawners)
{
}

void UGTPMassTrafficSubsystem::RecycleAtDeadEnds()
{
}
#endif

// ---------------------------------------------------------------- near cars with a body

UGTPMassTrafficBodyProcessor::UGTPMassTrafficBodyProcessor()
	: Query(*this)
	, PlayerQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	// It changes actors (physics, flags): on the game thread.
	bRequiresGameThreadExecution = true;
#if !WITH_GTP_MASSTRAFFIC
	bAutoRegisterWithProcessingPhases = false;
#endif
}

void UGTPMassTrafficBodyProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
#if WITH_GTP_MASSTRAFFIC
	Query.AddTagRequirement<FMassTrafficVehicleTag>(EMassFragmentPresence::All);
	Query.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	Query.RegisterWithProcessor(*this);
	PlayerQuery.AddTagRequirement<FGTPPlayerObstacleTag>(EMassFragmentPresence::All);
	PlayerQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	PlayerQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite);
	PlayerQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	PlayerQuery.RegisterWithProcessor(*this);
#endif
}

void UGTPMassTrafficBodyProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
#if WITH_GTP_MASSTRAFFIC
	// Collected first, changed after the query: an actor's physics is no business of the chunk loop. Every shown car each
	// frame (12 at most by default): Mass turns its components' tick back on when it takes it out of its pool.
	TArray<AGTPVehicle*, TInlineAllocator<16>> Shown;
	TArray<AGTPVehicle*, TInlineAllocator<16>> Fresh;
	Query.ForEachEntityChunk(Context, [&Shown, &Fresh](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FMassRepresentationFragment> Representations = Chunk.GetFragmentView<FMassRepresentationFragment>();
		const TArrayView<FMassActorFragment> Actors = Chunk.GetMutableFragmentView<FMassActorFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			if (Representations[Index].CurrentRepresentation != EMassRepresentationType::LowResSpawnedActor)
			{
				continue;
			}
			if (AGTPVehicle* Car = Cast<AGTPVehicle>(Actors[Index].GetMutable()))
			{
				(Car->IsShownByMassTraffic() ? Shown : Fresh).Add(Car);
			}
		}
	});
	for (AGTPVehicle* Car : Shown)
	{
		Car->SetShownByMassTraffic();
	}
	for (AGTPVehicle* Car : Fresh)
	{
		Car->SetShownByMassTraffic();
		++Made;
	}
	if (!Fresh.IsEmpty() && (Made <= 3 || Made % 50 == 0))
	{
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic near cars with a body: %d of our car actor(s) made MassTraffic's so far (last %s)"), Made, *Fresh.Last()->GetName());
	}
	// The player's obstacle of the moment (on foot or at the wheel: told by its radius) where he is, the other one out of
	// the way, both out of the way with no player.
	const UWorld* World = Context.GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const bool bAtWheel = Pawn && Pawn->IsA<AGTPVehicle>();
	const FTransform Where = Pawn ? FTransform(Pawn->GetActorQuat(), Pawn->GetActorLocation()) : FTransform::Identity;
	const FVector Velocity = Pawn ? Pawn->GetVelocity() : FVector::ZeroVector;
	PlayerQuery.ForEachEntityChunk(Context, [&Where, &Velocity, Pawn, bAtWheel](FMassExecutionContext& Chunk)
	{
		const TArrayView<FTransformFragment> Transforms = Chunk.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassVelocityFragment> Velocities = Chunk.GetMutableFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FAgentRadiusFragment> Radii = Chunk.GetFragmentView<FAgentRadiusFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			const bool bInUse = Pawn && (Radii[Index].Radius > 100.f) == bAtWheel;
			Transforms[Index].SetTransform(bInUse ? Where : FTransform(FVector(0.0, 0.0, -100000.0)));
			Velocities[Index].Value = bInUse ? Velocity : FVector::ZeroVector;
		}
	});
#endif
}
