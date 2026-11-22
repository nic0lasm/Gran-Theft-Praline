#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Engine/TimerHandle.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "MassEntityTypes.h"
#include "GTPMassTraffic.generated.h"

struct FZoneGraphTrafficLaneData;
class AMassSpawner;
class AActor;

/**
 * Switch to the MassTraffic plugin of City Sample (ADR-0015, docs/gameplay/traffic-mass.md).
 *
 * -Traffic=mass: the legacy network keeps its lights, street parked cars and police, but drives
 * no car of its own (AGTPTrafficNetwork::IsMassTrafficWanted, which also asks for spawners in the map); the Mass spawners placed by
 * tools/city/unreal/place_masstraffic.py (tag GTPMassTraffic, no spawn on begin play) spawn
 * the traffic and the parked cars on our ZoneGraph lanes. Without the plugin (not installed,
 * WITH_GTP_MASSTRAFFIC=0) nothing happens here and the legacy traffic drives as before.
 *
 * Proof in the log, every 5 s for the first minute then every 30 s ("MassTraffic count"): ZoneGraph
 * data in the map (lanes, zones: 0 lanes means the ZoneGraph was never built, so no car can
 * spawn), cars drawn by Mass as instances (MassVisualizer) and as actors (City Sample's
 * CitySampleMassVehicleBase, or our cars shown near the player: -MassTrafficBodies), and the nearest
 * one to the player.
 * -MassTrafficView: the player is put 12 m from the first Mass car seen (once), for a capture.
 */
UCLASS()
class GTPMASS_API UGTPMassTrafficSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static const FName SpawnerTag;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Spawners started at begin play (0 when MassTraffic is off or absent). */
	int32 GetStartedSpawners() const { return StartedSpawners; }

private:
	void LogCounts();
	/** "MassTraffic cars": the simulated cars (Mass entities), moving or not, near the player or not. */
	void LogSimulatedCars(UWorld& World, const FVector& PlayerLocation);
	/**
	 * "MassTraffic physics template" (once per extracted template: mass, centre of mass, wheels) and
	 * "MassTraffic physics car" (the 6 cars under MassTraffic's simple physics nearest the player:
	 * speed, inputs, gear, wheels on the road, place against the lane), with each count log.
	 */
	void LogPhysicsCars(UWorld& World, const FVector& PlayerLocation);
	TSet<const void*> LoggedTemplates;
	/**
	 * -MassTrafficPicture[=150,290]: every 0.25 s; at each time (s), a camera at 50 to 70 m from a
	 * traffic car 50 to 70 m from the player (the one lying over the most, else the one nearest 60 m),
	 * where the player stands if it sees the car, else 60 m behind or ahead of it on its heading; half a
	 * second later the picture (Saved/Screenshots/<platform>/mass-traffic-<n>.png) and a line
	 * "MassTraffic picture" with the car, its distances, roll and pitch; then the view goes back.
	 */
	void TickTrafficPicture();
	FTimerHandle PictureTimer;
	TArray<float> PictureTimes;
	int32 PicturesTaken = 0;
	int32 PictureRetries = 0;
	int32 PicturePhase = 0;
	double PictureAskedAt = 0.0;
	TWeakObjectPtr<AActor> PictureCamera;
	int32 PictureEntityIndex = INDEX_NONE;
	int32 PictureEntitySerial = 0;
	FString PictureEye;
	FString PictureBlocked;
	/**
	 * With -TrafficDeadEnds=cut, once before the spawners: the car lanes from which every way ends at
	 * a dead end (the edge of the focus area) are taken out of the cars' choice. MassTraffic's lane choice does not look
	 * ahead: a car takes a lane that leads to a dead end, then stops for good before its last lane
	 * (ShouldStopAtLaneExit, "NONEXT"), inside the junction when that last lane leaves one, and the
	 * junction never opens again for anyone. The links to those lanes are removed from MassTraffic's
	 * own lane data (FZoneGraphTrafficLaneData::NextLanes): every other lane keeps a way on. A junction
	 * lane into a lane too short for a car and its gap counts as a dead end too: MassTraffic never lets
	 * a car into it (no room after the junction) and the car waits at the line for good. Cars spawned
	 * inside such a branch are recycled (RecycleAtDeadEnds). On the PC (f8fc873) this took 307 of 577
	 * car lanes out of the traffic, most of the map away from the player.
	 */
	void CutDeadEndBranches();
	/**
	 * Once, before the spawners (default, -TrafficDeadEnds=link): the network is closed instead of
	 * cut, as a GTA city has no end. Each lane with no lane after it (a road leaving the map) leads
	 * on, in MassTraffic's own lane data (FZoneGraphTrafficLaneData::NextLanes), to an entry: a lane
	 * no lane leads into (a road coming into the map), the least used then the nearest. MassTraffic
	 * moves a car onto its next lane by lane and distance only (MoveVehicleToNextLane): the car leaves
	 * the map at one end and comes back at another, and no car stops for good before a dead end
	 * (ShouldStopAtLaneExit, "NONEXT"). A junction lane into a lane too short for a car and its gap
	 * (MassTraffic never lets a car into it) is taken out of the cars' choice first. Every lane keeps
	 * its traffic: nothing is closed. -TrafficDeadEnds=cut cuts the branches as before
	 * (CutDeadEndBranches), -TrafficDeadEnds=none (or -NoDeadEndCut) leaves the lanes as built.
	 */
	void LinkDeadEndsToEntries();
	/** The dead-end handling of -TrafficDeadEnds (link, cut or none), once the lane data is there. */
	void HandleDeadEnds();
	/**
	 * Every 0.25 s while dead ends are linked: no car leaves or comes into the map in the player's
	 * sight, as in GTA. An entry lane is closed (bIsOpen) while the player may see one of its two ends
	 * (the dead end a car leaves from, the entry's own start): within 300 m (-TrafficLoopSight=<m>),
	 * in the camera's field (plus 15 degrees) or within 30 m, and not hidden by a building (a
	 * visibility line trace). MassTraffic stops a car before a closed lane (ShouldStopAtLaneExit), so
	 * the cars wait at the dead end and go on once out of sight. Counts the cars that looped, and those
	 * that did it in sight (a car that could no longer stop): "MassTraffic loops". An entry is also
	 * held while a car MassTraffic simulates with its simple physics (its High/Medium LOD, 200 m by default) is
	 * on one of its dead ends: such a car would be pulled onto the entry in one frame and take that
	 * move as its speed. RecycleAtDeadEnds hands it to MassTraffic's recycling once stopped there out
	 * of sight. -NoLoopGuard leaves the entries open.
	 */
	void GuardLoops();

	/** A dead end linked to an entry: indices in GetMutableTrafficZoneGraphData() and its lane array. */
	struct FLoop
	{
		int32 Slot = INDEX_NONE;
		int32 DataIndex = INDEX_NONE;
		int32 From = INDEX_NONE;
		int32 To = INDEX_NONE;
		int32 FromLane = INDEX_NONE;
		int32 ToLane = INDEX_NONE;
		FVector Exit = FVector::ZeroVector;
		FVector Entry = FVector::ZeroVector;
	};
	TArray<FLoop> Loops;
	int32 LoopEntries = 0;
	/** Entries held closed at the last check (ZoneGraph data index << 32 | lane index). */
	TSet<int64> EntriesInSight;
	/** ZoneGraph lane of each car at the last check, to see it loop. */
	TMap<uint64, int32> LaneOfCar;
	int32 LoopJumps = 0;
	int32 LoopJumpsInSight = 0;
	/** Loops made by a car under MassTraffic's simple physics (should stay 0, see GuardLoops). */
	int32 LoopJumpsInPhysics = 0;
	int32 MostEntriesHeld = 0;
	/** Entries held closed because a car under MassTraffic's physics is on one of their dead ends. */
	int32 EntriesHeldForPhysics = 0;
	int32 MostHeldForPhysics = 0;
	/** The dead ends LinkDeadEndsToEntries linked (lane data in MassTraffic's arrays, never resized). */
	TSet<const FZoneGraphTrafficLaneData*> LoopExitLanes;
	FTimerHandle LoopTimer;
	/**
	 * Every second until MassTraffic's junction entities are there: our junctions have no traffic
	 * lights, so MassTraffic runs them as stop signs, one car per side per turn (the lane closes as the
	 * first car enters it) and each turn waits for the junction to be clear. They are given Epic's
	 * traffic-light behaviour instead (bHasTrafficLights, as at 106 of Small City's 172 junctions):
	 * a side stays open for 10 s (-TrafficJunctionGoSeconds=<s>) and lets every car through, and the
	 * turn ends early once the junction is clear with no car waiting. No light is drawn: the periods
	 * are those MassTraffic built, one per side. -TrafficStopSigns keeps the stop signs.
	 */
	void LightJunctions();
	/**
	 * Before the spawners: our copies of City Sample's car configs have no display actor (its car
	 * Blueprints break the cook), yet their visualization trait asks for one up close
	 * (HighResSpawnedActor). MassRepresentation then logs "Template actor type -1 is not referring to
	 * a valid type" on every update of such a car and falls back to its instance. Those LODs are
	 * given the instance straight away (StaticMeshInstance), and the entity templates already built
	 * by the spawners are rebuilt at their spawning. Looks the same, without the error.
	 *
	 * Near cars with a body (-MassTrafficBodies=<m>, default 35; 0 = instances only, as up to
	 * candidate 56): the driving cars within that distance of the player, 12 at most
	 * (-MassTrafficBodiesMax), are shown as our car of the config (its PhysicsVehicleTemplateActor,
	 * BP_GTP_*), as Epic's LowResSpawnedActor: MassTraffic moves the actor to its entity every frame
	 * (UMassTrafficVehicleUpdateCustomVisualizationProcessor) and UGTPMassTrafficBodyProcessor makes
	 * it kinematic (AGTPVehicle::SetShownByMassTraffic). An instance has no body: the player's car went
	 * through them (the crash step of -GTARun). Parked cars stay instances. The player is an obstacle
	 * for the cars (FGTPPlayerObstacleFragment), else they would push his stopped car along.
	 */
	void ShowNearCarsAsInstances(const TArray<AMassSpawner*>& InSpawners);
	bool bJunctionsLit = false;
	int32 LightTries = 0;
	FTimerHandle LightTimer;
	/**
	 * Every second: a car in a branch that only leads to dead ends, or standing at the end of a lane
	 * with no lane after it, out of the player's sight, is handed to MassTraffic's own recycling
	 * (FMassTrafficRecyclableVehicleTag, the tag a repaired wreck gets), which moves it to a free
	 * space on one of the least busy lanes away from the player (UMassTrafficOverseerProcessor).
	 * MassTraffic itself never takes a car off a dead end.
	 */
	void RecycleAtDeadEnds();
	/** True for a ZoneGraph lane of a branch cut off by CutDeadEndBranches. */
	bool IsInDeadEndBranch(int32 DataIndex, int32 LaneIndex) const { return DataIndex == DeadEndBranchData && DeadEndBranch.Contains(LaneIndex); }

	int32 StartedSpawners = 0;
	int32 CountLogs = 0;
	/** Counts (every 5 s) with a car within 50 m of the watch point (-TrafficWatchPoint, LogSimulatedCars). */
	int32 WatchCounts = 0;
	int32 WatchCountsWithCar = 0;
	bool bViewPlaced = false;
	bool bNetworkLogged = false;
	/** HandleDeadEnds found the lane data and did its work (or none was asked). */
	bool bDeadEndsHandled = false;
	int32 Recycled = 0;
	/** Recycled cars to which Epic's damage fragment had to be added (its recycling needs one). */
	int32 DamageAdded = 0;
	/** World time at which each car still waiting for recycling was handed over (entity as number). */
	TMap<uint64, double> RecycleHandedAt;
	/** The stopped MassTraffic cars (entity serial and index) and since when, as of the last count (queue heads). */
	TMap<uint64, double> StoppedSince;
	/** Cars waiting for recycling given back to the traffic: committed to their next lane (bCantStopAtLaneExit). */
	int32 GivenBack = 0;
	/** Cars at a dead end not handed over because committed to their next lane (entity as number). */
	TSet<uint64> CommittedLeft;
	/** ZoneGraph lanes (indices in the data of index DeadEndBranchData) of the cut branches. */
	TSet<int32> DeadEndBranch;
	int32 DeadEndBranchData = INDEX_NONE;
	FTimerHandle RecycleTimer;
	FTimerHandle CountTimer;
};

/**
 * The player as MassTraffic obstacles (created with the cars; -MassTrafficPlayerObstacle=0 leaves them out): the cars
 * on the lanes near him brake for him as for a person on a crossing (UMassTrafficFindObstaclesProcessor,
 * UpdateDistanceToNearestObstacle). Now that the near cars have a body, a car that does not brake pushes the player's
 * stopped car along, as a wall moving. Four points of radius 0, as Epic's "Traffic Obstacle" trait gives (transform,
 * velocity, radius, its tag): on foot, the player, only while he stands in the path of a car (as our people wear the
 * tag on a crossing only, GTPCrowd); at the wheel, the rear bumper, the middle and the front bumper of his car. A
 * point not in use waits out of the way. Radius 0: MassTraffic brakes a car for an obstacle closer to its lane line
 * than its own radius (about half its length), so a bigger one would brake the cars of the next lane.
 */
USTRUCT()
struct GTPMASS_API FGTPPlayerObstacleFragment : public FMassFragment
{
	GENERATED_BODY()

	/** 0 on foot; at the wheel 1 rear bumper, 2 middle, 3 front bumper. */
	UPROPERTY()
	int32 Point = 0;
};

/**
 * The first frame MassTraffic shows a car as one of our car actors (LowResSpawnedActor, -MassTrafficBodies): the actor
 * becomes MassTraffic's (AGTPVehicle::SetShownByMassTraffic: kinematic, its body follows the entity, nobody gets in).
 * Nothing to do when no car is shown as an actor. Also puts the player's obstacle points in use on him
 * (FGTPPlayerObstacleFragment).
 */
UCLASS()
class GTPMASS_API UGTPMassTrafficBodyProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGTPMassTrafficBodyProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
	/** The player's obstacle points (FGTPPlayerObstacleFragment): those in use put on the player, every frame. */
	FMassEntityQuery PlayerQuery;
	int32 Made = 0;
	/** The player's car whose bumpers RearCm and FrontCm are (its local bounds along X), measured once a car. */
	TWeakObjectPtr<const AActor> SizedCar;
	float RearCm = -230.f;
	float FrontCm = 230.f;
	/** On foot in the path of a car last frame, and how many times he stepped into one (logged). */
	bool bWasInPath = false;
	int32 InPathTimes = 0;
	/** -MassTrafficBodies in cm (read on the first frame), and when the cars near the player were last said (game time). */
	float BodyRadiusCm = -1.f;
	double NextNearLog = 0.0;
	int32 NearLogs = 0;
};
