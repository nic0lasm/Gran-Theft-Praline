#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/SoftObjectPath.h"
#include "GTPDriveLogic.h"
#include "GTPFlightCircuit.h"
#include "GTPTrafficReactLogic.h"
#include "GTPTrafficLights.h"
#include "GTPTraffic.generated.h"

class ACharacter;
class APawn;
class AGTPVehicle;
class AGTPTrafficNetwork;
class AGTPFarTraffic;
class USkeletalMeshComponent;

/** What held a traffic car back at its last decision: the nearest of its stops ahead (the "traffic cars" line). */
namespace GTPTrafficStop
{
	enum class ELimit : uint8
	{
		None,
		CarAhead,
		PersonAhead,
		OtherAhead,
		Queue,
		RedLight,
		Junction,
		Held,
		Wedged,
		Num
	};
}

/** One driving lane, in world space (cm), from tools/city/traffic.py. */
USTRUCT(BlueprintType)
struct FGTPTrafficLane
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	TArray<FVector> Points;

	/** Lanes this one leads to; empty = dead end (the car is taken away out of sight). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	TArray<int32> Next;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float SpeedKmh = 30.f;

	/** Junction at the end of the lane (one car at a time crosses it), -1 for a plain join. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	int32 ToJunction = -1;

	/** This lane's share of its road's carriageway (m), from the lanes file; 0 when unknown. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	float WidthM = 0.f;

	/**
	 * Width of the carriageway around the lane (m, GTPRoadWidth::Carriageways: the lanes of its road near its middle,
	 * both directions); 0 when unknown. Under 5 m a narrow street, 7 m and over a main axis.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	float CarriagewayM = 0.f;

	/** Some way goes on for ever from this lane (GTPRoadWidth::NeverEnding): a flight that takes it meets no dead end. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	bool bNeverEnds = false;

	/** A fixed obstacle of the map stands on this lane (AGTPTrafficNetwork::MarkLaneBlocked): flights keep off it. */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Traffic")
	bool bBlocked = false;

	float LengthCm() const;
};

/**
 * Follows lanes of a network for a driver that is not an AI car of that
 * network (the demo route driving the player's car): same decisions as the
 * AI drivers, no junction claims. Plain C++, owned by its user.
 */
struct GRANTHEFTPRALINE_API FGTPLaneFollower
{
	/** Starts on the lane closest to Location heading like Yaw (degrees); false if none within MaxDistanceCm. */
	bool StartNear(AGTPTrafficNetwork* InNetwork, const FVector& Location, float YawDegrees, float MaxDistanceCm);

	/** One decision; false when the route ended (dead end) or there is none. LeaderSpeedMps: the speed of what stands
	 *  ObstacleMetres ahead when it drives the same way and is to be followed at a time gap (GTPDrive::Decide);
	 *  negative: the car stops short of it. */
	bool Step(const FVector& Location, float YawDegrees, float SpeedKmh, float MaxSpeedKmh, float ObstacleMetres, FGTPDriveCommand& Out,
		float LeaderSpeedMps = -1.f);

	/** Chases and demos: the speed given to Step is the limit, not the lane's posted speed. */
	bool bIgnoreLaneLimit = false;

	/** Chases and demos: at a junction never an exit a fixed obstacle blocks (FGTPTrafficLane::bBlocked), an exit that
	 *  never ends (FGTPTrafficLane::bNeverEnds) first, then the one with the best way on within 500 m, its metres weighted
	 *  by width and halved on lanes driven lately (a flight keeps to the main axes, not to the narrow streets, and does
	 *  not circle one block); then the one that turns the least (GTPRoadWidth::ExitScore). */
	bool bPreferStraight = false;

	/** Chases and demos: the flight keeps to the circuit of the map when it has one (AGTPTrafficNetwork::GetFlightCircuit,
	 *  GTPFlightCircuit.h): on the lap the next lane of the lap, on the approach its next lane, elsewhere the exit that
	 *  leads to the lap by the shortest way; the lanes of the circuit are driven on their lines, not on their own
	 *  points, and StartNear takes a lane of the circuit when one is near. From a lane no way leads from to the lap,
	 *  bPreferStraight chooses as before. Set before StartNear. */
	bool bFollowCircuit = false;

	/** Pursuers of a flight: the lanes of the map's flight circuit are driven on their lines (looked at beforehand,
	 *  without the jogs of the lanes) whatever chooses the way; the other lanes on their own points. Where the way
	 *  leaves the circuit the line ends up to 4.5 m beside the next lane, a jog as the lanes have many. Set before
	 *  StartNear. */
	bool bDriveCircuitLines = false;

	/** The lane being followed is of the map's flight circuit, lap or approach. */
	bool IsOnCircuit() const;

	/** The speed the flight circuit asks on the lane being followed (km/h); 0 when it asks none or the lane is not of it. */
	float GetCircuitKmh() const;

	/** The map's flight circuit names lanes for the flight to end on, and the lane being followed is one of them. */
	bool HasCircuitEndPlace() const;
	bool IsOnCircuitEndPlace() const;

	/** Metres of road open after entering Lane (GTPDrive::OpenRoad over the network's lanes, 6 junctions deep, capped at OpenRoadWanted). */
	static float OpenRoadMetres(const TArray<FGTPTrafficLane>& Lanes, int32 Lane);
	static constexpr float OpenRoadWanted = 250.f;

	/** Chases: with a goal, the way taken is the shortest one on the lanes (GTPLaneRoute.h) and not, junction after junction,
	 *  the exit that points at the goal (that one can be a dead end, or the wrong way round a one-way block). The goal
	 *  moves: the way is looked for again every 2 s, keeping the lanes the car can no longer avoid at its speed. */
	bool bRouteToGoal = false;

	/** A lane the way must not end on (the dead end its driver has just turned round in); INDEX_NONE for none. */
	int32 AvoidLane = INDEX_NONE;

	/** Metres of lane planned ahead (0 = the default 50). A flight at 90 km/h needs more, or the end of the path brakes it:
	 *  with bIgnoreLaneLimit, Step() raises it to the braking distance of the speed asked (GTPDrive::PathNeeded). */
	float AheadMeters = 0.f;

	/** Chases and demos: corners taken at 6 m/s2 instead of the calm 2.2, a firmer throttle, a brake that holds the car
	 *  to the speed of the corner ahead (GTPDrive::SetChaseDriving: a flight, not a drive). */
	void SetHardDriving() { GTPDrive::SetChaseDriving(Settings); }

	/** Metres between Location and the path being followed (the segment after the closest point); 0 when idle. */
	float OffPathMetres(const FVector& Location) const;

	/** Chases: the caller's own driving settings (the police director tunes its units). */
	void SetSettings(const FGTPDriveSettings& InSettings) { Settings = InSettings; }

	bool IsActive() const { return Network.IsValid() && Route.Num() > 0; }

	/** Index of the lane being followed, INDEX_NONE when idle (logged by the deterministic demo to compare two runs). */
	int32 GetLane() const { return Route.Num() > 0 ? Route[0] : INDEX_NONE; }

	/** Carriageway around the lane being followed (m, FGTPTrafficLane::CarriagewayM); 0 when idle or unknown. */
	float GetCarriagewayM() const;

	/** The lane driven lately (the last RecentCount left) or planned ahead: a flight's way counts it for half. */
	bool IsRecent(int32 Lane) const { return RecentLanes.Contains(Lane) || Route.Contains(Lane); }
	static constexpr int32 RecentCount = 25;
	/** The lanes left lately, oldest first: kept by a flight put on another lane (GTPDemoRoute::RelocateChase). */
	const TArray<int32>& GetRecentLanes() const { return RecentLanes; }
	void SetRecentLanes(const TArray<int32>& InLanes) { RecentLanes = InLanes; }

	/** Of the lane being followed and the next one, the one nearest Point (world cm, 2D), and how far (m); INDEX_NONE when idle. */
	int32 LaneNear(const FVector& Point, float& OutMetres) const;

	/** From now on, junction choices lead towards Goal (world cm) instead of being random. */
	void SetGoal(const FVector& InGoal) { Goal = InGoal; bHasGoal = true; }
	void ClearGoal() { bHasGoal = false; }

private:
	void Plan();
	/** The lanes to take after the last one of the route to reach the goal (bRouteToGoal). */
	TArray<int32> WayToGoal(const TArray<FGTPTrafficLane>& Lanes) const;
	/** Past the end of that way: the exit that ends nearest the goal among those with open road behind them. */
	int32 PickOpenToward(const TArray<FGTPTrafficLane>& Lanes, int32 Lane) const;
	double NextReplanTime = 0.0;

	FVector Goal = FVector::ZeroVector;
	bool bHasGoal = false;

	TWeakObjectPtr<AGTPTrafficNetwork> Network;
	/** Own stream for deterministic runs: the junction choices do not depend on what other cars drew. */
	FRandomStream OwnRandom{0x44454D4F};
	TArray<int32> Route;
	TArray<int32> RecentLanes;
	std::vector<FGTPDrivePoint> Path;
	/** The end of the lane left last, kept at the head of Path (GTPDrive::KeptBehind): a corner the lane change falls in stays one. */
	std::vector<FGTPDrivePoint> LaneBehind;
	/** Points of Path up to the end of the first lane of the route, LaneBehind included. */
	int32 CurrentLaneLength = 0;
	int Closest = 0;
	FGTPDriveSettings Settings;
};

/**
 * Drives a car along the lanes of the traffic network (GTPDriveLogic.h, the
 * same decisions as tools/vehicles/traffic_sim). Stops for anything ahead,
 * the player on foot included. Switches itself off when the AI driver
 * leaves the seat (car stolen by the player).
 */
UCLASS(ClassGroup = (Traffic), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPAIDriverComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPAIDriverComponent();

	/** Starts on Lane at its point StartPoint. */
	void Start(AGTPTrafficNetwork* InNetwork, int32 Lane, int32 StartPoint);

	/**
	 * The network of a driver made but not started (AGTPTrafficNetwork::CreateCar): a far traffic puppet is made so and
	 * only placed by PlaceGhost at its first handover, which needs the network.
	 */
	void SetNetwork(AGTPTrafficNetwork* InNetwork) { Network = InNetwork; }

	/** False for a driver with no route or no decisions: never started, or switched off (stolen, wrecked, puppet). */
	bool IsDriving() const { return Route.Num() > 0 && IsComponentTickEnabled(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True when the car stands at the end of a dead-end lane. */
	bool IsAtDeadEnd() const { return bAtDeadEnd; }

	int32 GetLane() const { return Route.Num() > 0 ? Route[0] : INDEX_NONE; }

	/** Lane after the current one on the planned route, INDEX_NONE if none. */
	int32 GetNextLane() const { return Route.Num() > 1 ? Route[1] : INDEX_NONE; }

	/** Index of the lane point the car is nearest to, on its current lane. */
	int32 GetLanePoint() const { return Closest; }

	/**
	 * Free distance in front of Vehicle (m) to the nearest car or person, negative if none within LengthCm.
	 * OutHit, when given, receives what was hit.
	 */
	static float ProbeAhead(const AGTPVehicle* Vehicle, float LengthCm, float RadiusCm, AActor** OutHit = nullptr);

	/** Reaction state of this driver (GTPTrafficReactLogic.h): cruise, waiting, bypass, panic, flee, shaken. */
	GTPTrafficReact::EState GetReactState() const { return React.State; }
	/**
	 * The still obstacle this car waits behind (reaction Waiting: a parked car, a person, a car waiting itself),
	 * OutSeconds how long it has been stopped there; null when it is not waiting.
	 */
	const AActor* GetWaitingFor(float& OutSeconds) const
	{
		const AActor* Blocker = React.State == GTPTrafficReact::EState::Waiting ? WaitingFor.Get() : nullptr;
		OutSeconds = Blocker ? React.BlockedTime : 0.f;
		return Blocker;
	}

	/** Demo and tests: the driver behaves as if aimed at, shot near, or rammed by the player (-TrafficReactDemo). */
	void InjectReaction(bool bAimed, bool bShot, bool bRammed);

	AGTPTrafficNetwork* GetNetwork() const { return Network; }
	GTPTrafficReact::ETemperament GetTemperament() const { return Temperament; }

	/** Seconds the car has been standing still. */
	float GetStoppedTime() const { return StoppedTime; }
	int32 GetWedgeCount() const { return WedgeCount; }

	/** What held the car back at its last decision (none when nothing within the probe or the lane's stops). */
	GTPTrafficStop::ELimit GetLimit() const { return Limit; }

	/**
	 * Seconds between two decisions (0 = every frame). The pedals keep their
	 * last value in between, so far cars can decide 4 to 10 times a second
	 * (-VehicleLod=1, docs/performance/vehicle-cost.md).
	 */
	void SetDecisionInterval(float Seconds);

	/** Gives back the junctions this car holds (car taken away or stolen). */
	void ReleaseJunctions();

	/**
	 * Ghost mode (-VehicleGhost=1, docs/performance/vehicle-cost.md): physics
	 * and the Chaos vehicle off, the car slides along its path under the same
	 * stops as a driven car (GTPTrafficGhost.h). Off again: physics back on at
	 * the ghost's speed.
	 */
	void SetGhost(bool bInGhost);
	bool IsGhost() const { return bGhost; }

	/** Ghost speed (m/s). */
	float GetGhostSpeed() const { return GhostSpeed; }
	double GetGhostSince() const { return GhostSince; }

	/**
	 * Moves the car, as a ghost, to point Point of Lane at Location (cm) facing
	 * YawDegrees, rolling at SpeedMs: a far car handed over to this driven car
	 * (-FarTraffic, GTPFarTraffic.h). False if it could not (no network): the car
	 * has not moved.
	 */
	bool PlaceGhost(int32 Lane, int32 Point, const FVector& Location, float YawDegrees, float SpeedMs);

	/** Time spent in the ticks of all AI drivers (seconds) and their number, since the last read. */
	static double TickSeconds;
	static int32 TickCount;

	/** Ghosts found with their physics or Chaos vehicle on, and switched off again (HoldGhostKinematic), since the start. */
	static int32 GhostPhysicsCaught;

	/** Length of the obstacle probe ahead (cm). */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float ProbeLength = 4000.f;

	/** Half width of the obstacle probe (cm): a lane is about 3 m. */
	UPROPERTY(EditAnywhere, Category = "Traffic")
	float ProbeRadius = 90.f;

private:
	void PlanRoute();
	float FindObstacleMetres(const AGTPVehicle* Vehicle) const;
	/** Ground height under Location (cm), false if nothing within ReachCm below. */
	bool FindGround(const AGTPVehicle* Vehicle, const FVector& Location, float& OutZ, float ReachCm = 1000.f) const;
	/**
	 * A ghost rides at the height the car stood above the ground when it became one. Off
	 * its wheels (thrown, lying, on another car), that height is not a ride height: the
	 * car's spawn height instead, so no ghost slides along in the air.
	 */
	void KeepRideHeightOnWheels(const AGTPVehicle* Vehicle, const TCHAR* When);
	/**
	 * A ghost is kinematic: its teleports throw a simulated body (gate 65: a car placed as a ghost, found simulated 2 s
	 * later, lying, at 87 km/h, 83 m from the player). Physics and the Chaos vehicle off again when found on, said in the
	 * log with the car's last placement, so a gate names the path that left them on.
	 */
	void HoldGhostKinematic(AGTPVehicle* Vehicle, const TCHAR* Where);

	UPROPERTY(Transient)
	TObjectPtr<AGTPTrafficNetwork> Network;

	FRandomStream DriverRandom;
	TArray<int32> Route;
	std::vector<FGTPDrivePoint> Path;
	int32 CurrentLaneLength = 0;
	int Closest = 0;
	bool bAtDeadEnd = false;
	float StoppedTime = 0.f;
	FGTPDriveSettings Settings;
	FGTPJunctionClaims Junctions;

	bool bGhost = false;
	/** World time its physics went off (the ghost switch on), for the pool's cooldown (AGTPTrafficNetwork::PutAway). */
	double GhostSince = 0.0;
	float GhostSpeed = 0.f;      // m/s
	float GhostAlong = 0.f;      // m past path point Closest
	float GhostRideHeight = 40.f; // cm from the ground to the actor origin
	float GhostGroundZ = 0.f;    // last ground height found (cm)
	float GhostGroundAge = 1.f;  // seconds since that ground trace
	float PushTime = 0.f;        // seconds pressing the throttle without moving (wedged on something)
	float BackOffTime = 0.f;     // seconds left reversing out of it
	int32 WedgeCount = 0;        // back-offs since it last drove freely
	GTPTrafficStop::ELimit Limit = GTPTrafficStop::ELimit::None;

	/** Reactions (GTPTrafficReactLogic.h) and what feeds them. */
	GTPTrafficReact::FOutput UpdateReactions(AGTPVehicle* Vehicle, float DeltaTime, float Speed, float ProbeM, AActor* ProbeHit);
	bool IsSideClear(const AGTPVehicle* Vehicle, float AheadM) const;
	bool IsStillObstacle(const AActor* Actor) const;
	GTPTrafficReact::FBrain React;
	GTPTrafficReact::FSettings DriverSettings;
	GTPTrafficReact::ETemperament Temperament = GTPTrafficReact::ETemperament::Calm;
	float TemperamentSpeed = 1.f;
	TWeakObjectPtr<AActor> BypassBlocker;
	/** What the last reaction step saw ahead while Waiting (GetWaitingFor). */
	TWeakObjectPtr<AActor> WaitingFor;
	int32 SeenShotSerial = -1;
	bool bHornOn = false;
	float InjectAimLeft = 0.f;
	bool bInjectShot = false;
	bool bInjectRammed = false;
};

/**
 * The driving lanes of a map and the cars that drive on them (version 0.3:
 * a few cars with a non-player driver the player can pull out).
 *
 * Lanes come from Content/City/Traffic/<MapName>.lanes.csv (written by
 * tools/city/traffic.py, staged with the game) unless set on the actor. The
 * game mode spawns one network per map when that file exists. Car count:
 * -Traffic=N on the command line, else NumCars; -Traffic=0 turns it off.
 * Cars are spawned and taken away only out of sight, at least
 * MinSpawnDistance from the player.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPTrafficNetwork : public AActor
{
	GENERATED_BODY()

public:
	AGTPTrafficNetwork();

	virtual void Tick(float DeltaSeconds) override;

	const TArray<FGTPTrafficLane>& GetLanes() const { return Lanes; }

	/**
	 * A car without a driver, parked beside a lane near Near (within RadiusCm, world cm),
	 * out of the player's sight when it can, tagged with Tag (empty: no tag). Class empty: a
	 * random traffic car class. The car is not traffic: never recycled, no AI driver, so the
	 * player can take it. nullptr when no spot was free. Costs nothing unless called (missions).
	 * Spots near a lane's end, on a bend or with the body over another lane are left out; when
	 * bQuietMiss is false (a request that must be met) they are taken if no other spot is free.
	 */
	AGTPVehicle* SpawnParkedVehicle(FName Tag, const FVector& Near, float RadiusCm, TSubclassOf<AGTPVehicle> Class = nullptr, bool bQuietMiss = false);

	/** Distance from the lane's centre line to the kerb spot of SpawnParkedVehicle (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Missions")
	float ParkOffset = 250.f;

	/**
	 * A parked car's centre stays at least this far from the centre line of every lane (cm): two half widths and a
	 * margin. ParkOffset puts it to the right of one lane, which can be on the next one (gate 59: a street parked car
	 * beside lane 96 stood in the chase's way, and the script moved the player's car).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Missions")
	float ParkLaneClearance = 210.f;

	/**
	 * A parked car's spot is left out when the ground found there stands more than this above the road at the lane
	 * beside it (cm): a kerb and a slope, not a bus shelter roof (gate 61's video: a van on one, at first-floor height).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Missions")
	float ParkKerbRise = 60.f;

	/**
	 * No parked car within this length of lane of a lane's start or end (cm): traffic aims a few metres ahead and cuts
	 * the corners there, and the next lane often starts two metres aside (gate 62: eight of the nine parked cars traffic
	 * queued behind stood less than 10 m from a lane's end or start).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Missions")
	float ParkEndMargin = 1200.f;

	/** Nor where the lane, within ParkEndMargin of the spot, comes more than this towards the kerb (cm): a bend that traffic cuts. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Missions")
	float ParkBendAllowance = 40.f;

	/**
	 * A parked car's body (its mesh's bounds) stays at least this far from the centre line of every other lane (cm): a
	 * driven car's half width and a margin. Its centre can be clear while its bonnet stands over a lane that crosses
	 * (gate 62: a Clio parked across a lane, its centre 226 cm from it).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Missions")
	float ParkBodyClearance = 130.f;

	/**
	 * A parked car's centre stays at least this far from the driving line of the map's flight circuit (cm): the two
	 * half widths, and what the filmed flight strays from its line. The line is not the middle of a lane, and
	 * ParkLaneClearance says nothing of it. Gate 91: the flight 1.6 m to the right of its line behind a row of
	 * parked cars 2.4 m from it, stopped at 62 s and arrested there. 0 turns it off.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Missions")
	float ParkFlightClearance = 340.f;

	// --- Real cars parked along the streets around the player, to steal (-StreetParked=N) ---

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Parked")
	int32 NumStreetParked = 10;

	/** Share of them locked (entered by breaking a window), and of the locked ones with an alarm. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Parked")
	float StreetParkedLockedShare = 0.6f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Parked")
	float StreetParkedAlarmShare = 0.4f;

	/** Physics on within this distance of the player, off beyond (cm); taken away out of sight past RemoveDistance. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Parked")
	float StreetParkedWakeDistance = 3500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Parked")
	float StreetParkedRemoveDistance = 15000.f;

	// --- Crashes between traffic cars (GTPTrafficIncident.h) ---

	/** Change of speed of a crash between two traffic cars that makes the drivers react (cm/s). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Incidents")
	float IncidentMinSpeedChange = 450.f;

	/** Only crashes this close to the player play out (cm), at most MaxIncidents at once (cars left in the street). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Incidents")
	float IncidentRange = 8000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Incidents")
	int32 MaxIncidents = 2;

	/**
	 * A street parked car at the kerb within RadiusCm of Near, managed like the others (asleep
	 * far away, taken away out of sight). Out of sight first, in view when nothing else is free.
	 * For scripted runs (UGTPTheftDemoLibrary); null when no kerb spot is free.
	 */
	AGTPVehicle* SpawnStreetParkedNear(const FVector& Near, float RadiusCm, bool bLocked, bool bAlarm);

	/** -Traffic=mass on the command line (MassTraffic of City Sample, module GTPMass). */
	static bool IsMassTrafficRequested();

	/**
	 * Requested, the MassTraffic plugin is loaded and the map has its spawners (tag GTPMassTraffic,
	 * place_masstraffic.py): this network then drives no car of its own. Otherwise the legacy
	 * traffic drives, so -Traffic=mass never leaves the streets empty.
	 */
	static bool IsMassTrafficWanted(const UWorld* World);

	/** Two traffic cars crashed (AGTPVehicle::OnBodyHit): the drivers argue, flee or drive off. */
	void NotifyTrafficCrash(AGTPVehicle* A, AGTPVehicle* B, float SpeedChange);

	/** A traffic driver back at their car after an incident: seated again and driving on from the nearest lane. */
	bool ReclaimCar(AGTPVehicle* Car, ACharacter* Driver);

	/** The player's weapon, read directly when the threat system is off (-ThreatResponse=0, the default). */
	bool GetPlayerAim(FVector& OutOrigin, FVector& OutDirection) const;
	int32 GetPlayerShotSerial() const { return PlayerShotSerial; }
	FVector GetPlayerShotFrom() const { return PlayerShotFrom; }

	// --- Reactions of the drivers (GTPTrafficReactLogic.h, docs/gameplay/traffic-reactions.md) ---

	/** Horn, going round, panic, fleeing, shock (-TrafficReact=0 turns them all off for measurements). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Reactions")
	bool bTrafficReact = true;

	/** Stopped behind something that does not move this long before the first horn (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Reactions")
	float ReactHonkAfter = 2.5f;

	/** Blocked this long before going round when the side is clear (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Reactions")
	float ReactBypassAfter = 5.f;

	/** Flooring it after a threat (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Reactions")
	float ReactPanicSeconds = 8.f;

	/** A gun pointed within this distance (cm) and half angle (degrees) frightens the driver. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Reactions")
	float ReactAimRange = 2000.f;
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Reactions")
	float ReactAimHalfAngle = 10.f;

	/** A shot fired within this distance (cm) starts a panic. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Reactions")
	float ReactShotRange = 3500.f;

	/** Horns farther than this from the camera stay silent (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Reactions")
	float ReactHornDistance = 8000.f;

	/** The rules' settings, from the values above. */
	GTPTrafficReact::FSettings ReactSettings;

	/**
	 * Lanes a car can start on (4 points or more, not a dead end) with a point that may lie
	 * between MinCm and MaxCm of Center (2D, by bounding box): what spawns draw from.
	 */
	void LanesInRing(const FVector& Center, float MinCm, float MaxCm, TArray<int32>& Out) const;

	/** Reads a lanes CSV (traffic.py format); returns the number of lanes. */
	int32 LoadLanesFromCsv(const FString& Path);

	/** Reads a lanes CSV (traffic.py format) without a network (the launch test with -Traffic=0); false when none. */
	static bool ReadLanesCsv(const FString& Path, TArray<FGTPTrafficLane>& OutLanes);

	/** Sets FGTPTrafficLane::bNeverEnds on every lane (GTPRoadWidth::NeverEnding): from those, no way ends at a dead end or on a blocked lane. */
	static void MarkNeverEnding(TArray<FGTPTrafficLane>& InLanes);

	/**
	 * A fixed obstacle of the map (By, at At) stands on Lane: flights keep off it from now on and the lanes that never end
	 * are found again without it. Logged once per lane. False when the lane was already blocked or does not exist.
	 */
	bool MarkLaneBlocked(int32 Lane, const AActor* By, const FVector& At);

	/** Path of the lanes file for a map name, under the project content folder. */
	static FString LanesFileFor(const FString& MapName);

	/** Path of the flight circuit of a map (Content/Test/Circuits/<Map>.circuit.csv: in Git, staged with the game). */
	static FString CircuitFileFor(const FString& MapName);

	/**
	 * The circuit the flights of the chase script keep to (GTPFlightCircuit.h, tools/city/flight_circuit.py). Not valid
	 * when the map has none, when its file does not fit the lanes read, or with -FlightCircuit=0: the flights are then
	 * free on the lanes, as until gate 78.
	 */
	const GTPFlightCircuit::FCircuit& GetFlightCircuit() const { return FlightCircuit; }

	/** Picks a random lane among Lane's next lanes, INDEX_NONE at a dead end. Draws from Stream when given (deterministic runs), else from the network's own stream. */
	int32 PickNext(int32 Lane, FRandomStream* Stream = nullptr);

	/** Next lane of Lane whose end is closest to Target (world cm); INDEX_NONE at a dead end. */
	int32 PickNextToward(int32 Lane, const FVector& Target) const;

	FGTPJunctionTable& GetJunctionTable() { return JunctionTable; }

	/** Traffic lights of the crossings (version 0.4); empty with -TrafficLights=0. */
	const FGTPTrafficLights& GetTrafficLights() const { return Lights; }

	/** Clock of the lights (seconds). */
	float GetLightTime() const;

	/**
	 * Pedestrian crossings: may someone walk across the road at Location now,
	 * heading WalkDirection? True away from any signalled junction (JunctionRadiusCm),
	 * else true only while the roads being crossed have a red light.
	 */
	bool IsWalkAllowed(const FVector& Location, const FVector& WalkDirection, float JunctionRadiusCm = 2500.f) const;

	/**
	 * Free distance (m) along the lanes from Driver to the next AI car on its
	 * current or next lane, negative if none within MaxMetres. Sees queues on
	 * bends that a straight probe misses.
	 */
	float GapAlongLanes(const UGTPAIDriverComponent* Driver, float MaxMetres) const;

	/**
	 * Car is a car of the traffic its driver left in the street (he ran from a threat or from a crash), empty now.
	 * TakeLeftCarAway takes such a car off the road and says so; false, and nothing done, for any other car. The far
	 * ones go by themselves out of the player's sight (GTPLeftCars.h); the chase script hands in the one in its way.
	 */
	bool IsLeftInTheStreet(const AGTPVehicle* Car) const;
	bool TakeLeftCarAway(AGTPVehicle* Car, const TCHAR* Why);

	/**
	 * The car the player is taken out of at his arrest stays in the street like one its driver ran from: taken away
	 * once he is far from it and does not see it (he reappears at the police station). Gate 100's long take: it stayed
	 * on place Bellecour, where the first flight ends, and the second flight stopped against it at 235 s.
	 * -KeepArrestCars leaves it there for good.
	 */
	void NoteLeftAtArrest(AGTPVehicle* Car);

	/** A car left in the street by its driver is taken away beyond this (cm), out of the player's sight. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	float LeftCarRemoveDistance = 8000.f;

	/** Lane and point closest to Location whose direction is within 60 degrees of YawDegrees; bFlightCircuitOnly: among
	 *  the lanes of the flight circuit (lap and approach) only. */
	bool FindNearestLane(const FVector& Location, float YawDegrees, float MaxDistanceCm, int32& OutLane, int32& OutPoint, bool bFlightCircuitOnly = false) const;

	/**
	 * A point to drive to in order to join the nearest lane, whatever its
	 * direction: AheadCm further along that lane, so the car meets it at an
	 * angle and can then follow it. False when no lane is within MaxDistanceCm.
	 */
	bool FindJoinPoint(const FVector& Location, float MaxDistanceCm, float AheadCm, FVector& OutPoint) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Traffic")
	TArray<FGTPTrafficLane> Lanes;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	int32 NumCars = 3;

	/** Cars to pick from (Blueprint children of AGTPVehicle); empty = the game mode's test car. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic", meta = (MetaClass = "/Script/GranTheftPraline.GTPVehicle"))
	TArray<FSoftClassPath> VehicleClasses;

	/** Character put at the wheel; empty = the player's character class. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic", meta = (MetaClass = "/Script/Engine.Character"))
	FSoftClassPath DriverClass;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	float MinSpawnDistance = 4000.f;

	/** Cars farther than this from the player, out of sight, are moved closer. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	float MaxCarDistance = 25000.f;

	/**
	 * Cheaper AI cars (frame budget, ADR-0002): beyond DetailDistance a car casts no
	 * shadow (moving shadow casters redraw virtual shadow map pages every frame),
	 * its lights cast no shadow at all, and it is drawn from FarLOD (0-based).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float DetailDistance = 2500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	int32 FarLOD = 2;

	/**
	 * Draw cost of the driven AI cars (docs/performance/vehicle-cost.md section 8),
	 * all off by default and set in metres on the command line:
	 * - DriverDrawDistance (-CarDriverDraw=M): the AI driver at the wheel is not drawn past it;
	 * - LightDistance (-CarLightDistance=M): the car's lights are off past it;
	 * - StaticRenderDistance (-CarRenderStatic=M): past it the body is drawn as a static
	 *   mesh (no skinning, last pose kept);
	 * - NearLOD (-CarNearLOD=N, 0-based): LOD drawn even under DetailDistance (0 = the full one);
	 * - bCarShadows (-CarShadows=0): no shadow at all from AI cars.
	 * A negative distance means never.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float DriverDrawDistance = -1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float LightDistance = -1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float StaticRenderDistance = -1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	int32 NearLOD = 0;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	bool bCarShadows = true;

	/** Beyond this distance an AI car is not drawn at all (cm, 0 = always drawn). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float DrawDistance = 15000.f;

	/**
	 * Distance tiers for the cost of AI cars, only with -VehicleLod=1 (or
	 * bVehicleLod): the driver decides every frame under LodNearDistance, 10
	 * times a second under LodMidDistance, 4 times a second beyond; the body
	 * pose is only refreshed while the car is drawn.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	bool bVehicleLod = false;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float LodNearDistance = 3500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float LodMidDistance = 9000.f;

	/**
	 * Ghost mode of far AI cars, only with -VehicleGhost=1 (or bVehicleGhost):
	 * a car turns into a ghost past GhostDistance from the player (cm) and is
	 * driven again inside GhostWakeDistance. Ghosts move 20 times a second.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	bool bVehicleGhost = false;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float GhostDistance = 9000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float GhostWakeDistance = 7000.f;

	/** Reuse the cars taken away instead of destroying and spawning new ones (-TrafficPool=1): no spawn hitch. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	bool bTrafficPool = false;

	/**
	 * Seconds a car stays in the pool before it is reused (-PoolCooldown=S): its physics was
	 * switched off when it was put away, and is never switched back on, teleported, in the same
	 * frame or the next ones. The oldest pooled car is reused first.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	float PoolCooldown = 2.f;

	/** Parked cars along the kerbs as static instances (-ParkedCars=1, AGTPParkedCars, tools/city/parking.py). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	bool bParkedCars = false;

	/**
	 * Far traffic (-FarTraffic=N, AGTPFarTraffic): N more cars past the ghost
	 * distance, drawn as moving instances with no actor. 0 = none. Needs the
	 * ghost mode, turned on with it.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Cost")
	int32 NumFarCars = 0;

	/** A car standing still this long (gridlock), out of sight, is taken away (seconds); longer than a red light and its queue. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic")
	float StuckSeconds = 35.f;

	/** Traffic lights at the crossings (off: -TrafficLights=0). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	bool bTrafficLights = true;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float GreenSeconds = 12.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float AmberSeconds = 3.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float AllRedSeconds = 2.f;

	/**
	 * Signal heads at each stop line, no shadow, no collision: the project's vehicle
	 * traffic light (SignalHeadMesh) turned towards the incoming cars, with a coloured
	 * lens disc on its front showing the light. If that mesh does not load (or with
	 * -SignalHeads=2), the stand-in: a coloured sphere on a thin pole.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	bool bShowSignalHeads = true;

	/**
	 * Street-furniture traffic light (wave 8 prop): pivot at the bottom centre, +X its
	 * front, the convention tools/city/details.py places it with. Empty: stand-in spheres.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	FSoftObjectPath SignalHeadMesh = FSoftObjectPath(TEXT("/Game/Environment/Props/Signals/TrafficLight_Vehicle/SM_TrafficLight_Vehicle_A.SM_TrafficLight_Vehicle_A"));

	/** Added to the yaw that turns the mesh's +X towards the incoming cars (degrees; -SignalHeadYaw=D overrides). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float SignalHeadYawOffset = 0.f;

	/** Centre of the red lens below the top of the mesh (cm); amber and green are SignalLensStep lower each. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float SignalLensBelowTop = 25.f;

	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float SignalLensStep = 30.f;

	/** Diameter of the lens disc (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float SignalLensSize = 20.f;

	/** Signal heads farther than this from the camera are hidden (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float SignalDrawDistance = 6000.f;

	/** A signal head is not drawn while the view is this near it on the ground plan (cm): it has no collision, a camera
	 *  can stand in it. */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float SignalCameraClearance = 250.f;

	/**
	 * Least distance from a signal head's foot to the centre line of any lane (cm). Right of its own lane, a head can
	 * stand on another one: a lane of another light group, a bus lane, the next road where two meet at an angle (gate
	 * 65's video: a van drove through one). Moved further right 50 cm at a time, up to 3 m, else left out.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float SignalHeadLaneClearance = 220.f;

	/**
	 * A street light of the same mesh (the details layer puts one at each traffic_signals node of OpenStreetMap) within
	 * this distance of a head (cm) and facing the same cars, within 60 degrees: the head's lenses are drawn on that
	 * light, and its own mesh is not (gate 65's video: three or four lights at a corner). 0: never.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Traffic|Lights")
	float SignalBorrowRadius = 600.f;

private:
	friend class AGTPPoliceDirector;
	friend class AGTPFarTraffic;
	friend class UGTPDemoRoute;
	bool SpawnCar();
	/** A new car with its AI driver (registered, not started) at Transform; nullptr if it could not spawn. */
	AGTPVehicle* CreateCar(const FTransform& Transform, ESpawnActorCollisionHandlingMethod Collision);
	bool FindSpawnPoint(int32& OutLane, int32& OutPoint, FTransform& OutTransform) const;
	bool IsSeenByPlayer(const FVector& Location) const;
	/**
	 * Takes Car off the road: to the pool, or destroyed. Any car with an AI driver can be handed in (the chase script
	 * hands in whatever stands in the player's way, the police its own cars). False, and nothing done, for a car
	 * that is not the network's to take: a far traffic puppet, or a car that is in the pool already.
	 */
	bool Recycle(AGTPVehicle* Car);
	void MakeCheap(AGTPVehicle* Car, ACharacter* Driver) const;
	void UpdateDetail(AGTPVehicle* Car, float DistanceToView) const;
	void UpdateStreetParked(const APawn* Player);
	void UpdatePlayerShots(const APawn* Player);

	struct FStreetParked
	{
		TWeakObjectPtr<AGTPVehicle> Car;
		double Born = 0.0;
		/** Kept asleep because another car stands in it (logged once). */
		bool bWakeBlocked = false;
		/** Placed for a script (SpawnStreetParkedNear): never taken away for blocking traffic. */
		bool bOnRequest = false;
		/** Holds traffic up while in view: logged once. */
		bool bBlockLogged = false;
	};
	/**
	 * A street parked car of the keeper (not one placed on request) that a traffic car has waited behind for
	 * this long is taken away once out of sight, and the keeper puts one elsewhere. Gate 67: a parked car in
	 * the path held 4 cars in turn, each until taken away as stuck (35 s), and a queue of 9 to 16 behind them.
	 */
	float StreetParkedBlockSeconds = 8.f;
	int32 StreetParkedTakenForBlocking = 0;
	/** After a jump of the player (Tick), until then: every cooled pooled car goes back at once, not four a tick. */
	double PoolRefillUntil = 0.0;
	TArray<FStreetParked> StreetParked;
	double NextStreetParkedTime = 0.0;
	int32 StreetParkedWanted = -1;
	int32 StreetParkedMisses = 0;
	TArray<int32> StreetParkedLanes;
	/** Cars left in the street by a crash: forgotten once driven again, removed far out of sight. */
	TArray<TWeakObjectPtr<AGTPVehicle>> IncidentCars;
	void UpdateIncidents(const APawn* Player);
	/** Cars a driver of the traffic left without a crash (AGTPVehicle::PanicExit), and when (world seconds). */
	struct FLeftCar
	{
		TWeakObjectPtr<AGTPVehicle> Car;
		double Since = 0.0;
	};
	TArray<FLeftCar> LeftCars;
	/** Cars the player was arrested in (NoteLeftAtArrest): left cars once he is out of them. */
	TArray<FLeftCar> ArrestCars;
	int32 LeftCarsNoted = 0;
	int32 LeftCarsTaken = 0;
	void UpdateLeftCars(const APawn* Player);
	int32 LastPlayerMagazine = -1;
	int32 PlayerShotSerial = 0;
	FVector PlayerShotFrom = FVector::ZeroVector;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGTPVehicle>> Cars;

	/** Cars taken away and kept hidden for the next spawn (-TrafficPool=1): no spawn, no destroy. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGTPVehicle>> Pool;
	/** Game time each pooled car was put away, same order as Pool. */
	TArray<double> PoolSince;

	/** 2D bounds of each lane (cm), filled when the lanes load, for LanesInRing. */
	TArray<FBox2D> LaneBounds;

	/** The flight circuit of the map, read after the lanes (BeginPlay). */
	GTPFlightCircuit::FCircuit FlightCircuit;
	void LoadFlightCircuit();

	/** Puts Car away into Pool; false if it cannot be reused (wrecked, no driver). */
	bool PutAway(AGTPVehicle* Car);
	/** A pooled car at Transform, ready to drive; nullptr if the pool is empty. */
	AGTPVehicle* TakeFromPool(const FTransform& Transform);
	/** A visible vehicle other than Car inside Car's box placed at Transform, or null (-PoolSpawnCheck=0 skips the test). */
	const AActor* FindCarAt(const AGTPVehicle* Car, const FTransform& Transform) const;
	/** -TrafficReactDemo=S: a reaction every S seconds on a car in view (video and measurements). */
	void UpdateReactDemo();
	float ReactDemoInterval = 0.f;
	double NextReactDemoTime = 0.0;
	int32 ReactDemoStep = 0;
	int32 ReactDemoWaits = 0;
	bool bReactDemoMoved = false;
	bool MovePlayerAhead(const UGTPAIDriverComponent* Driver);
	bool bPoolSpawnCheck = true;

	UPROPERTY(Transient)
	TArray<TSubclassOf<AGTPVehicle>> LoadedVehicleClasses;
	/**
	 * Leaves out of LoadedVehicleClasses (traffic, parked cars, police fallback) each class that draws
	 * nothing (AGTPVehicle::IsClassDrawn: gate 67, six City Sample classes of nine invisible), one line per
	 * class; keeps them all when none is drawn. Once.
	 */
	void KeepDrawnClasses();
	bool bDrawnClassesKept = false;

	UPROPERTY(Transient)
	TSubclassOf<ACharacter> LoadedDriverClass;

	void BuildLights();
	void UpdateSignalHeads();

	struct FSignalHead
	{
		int32 Lane = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		EGTPLight Shown = EGTPLight::None;
		TWeakObjectPtr<UStaticMeshComponent> Lamp;
		TWeakObjectPtr<UStaticMeshComponent> Pole;
		TWeakObjectPtr<UMaterialInstanceDynamic> Material;
		/** From Location: the pole or mesh, and the lens for red (and no light), amber, green. */
		FVector BodyOffset = FVector::ZeroVector;
		FVector LensOffset[3] = {FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector};
		/** Foot found on the ground; false while its streamed cell is not loaded yet (retried when seen). */
		bool bGrounded = false;
		/** Yaw of its mesh's +X, its front (degrees). */
		float FacingYaw = 0.f;
		/** Looked for a street light to draw its lenses on (BorrowStreetLight), once, when first drawn. */
		bool bLookedForStreetLight = false;
	};
	/**
	 * The street light of the details layer nearest to Head, if one of the same mesh stands within SignalBorrowRadius
	 * facing the same cars and no other head took it: Head's lenses go on its front, Pole draws nothing. Lens: the lens
	 * the head shows now (0 red, 1 amber, 2 green).
	 */
	void BorrowStreetLight(FSignalHead& Head, UStaticMeshComponent& Lamp, UStaticMeshComponent& Pole, int32 Lens);
	/** Package folder of the signal head mesh (empty with the stand-in): a street light in it is the same light. */
	FString SignalHeadFolder;
	/** Street lights a head took (component unique id << 32 | instance), and how many heads did, or stood by one facing elsewhere. */
	TSet<uint64> BorrowedStreetLights;
	int32 SignalHeadsBorrowed = 0;
	int32 SignalHeadsBesideOther = 0;

	FGTPJunctionTable JunctionTable;
	FGTPTrafficLights Lights;
	TArray<FSignalHead> SignalHeads;
	/** AI drivers of the cars spawned here (stolen ones stop ticking), for GapAlongLanes. */
	TArray<TWeakObjectPtr<UGTPAIDriverComponent>> AIDrivers;
	/** Centre of each signalled junction (mean of its stop lines), by junction id. */
	TMap<int32, FVector> JunctionCentres;
	/** Heading (radians) of group 0 at each signalled junction. */
	TMap<int32, float> JunctionAxis;
	int32 WantedCars = 0;
	float NextSpawnTime = 0.f;
	/** Far traffic drawn as instances (-FarTraffic=N), spawned by BeginPlay. */
	UPROPERTY(Transient)
	TObjectPtr<AGTPFarTraffic> FarTraffic;
	/** CPU budget log: time spent in Tick (seconds) and ticks since the last line. */
	double NetworkTickSeconds = 0.0;
	int32 BudgetTicks = 0;
	uint64 BudgetFrame = 0;
	void LogBudget();
	/** Every 10 s: the driven cars, how many move, how many stand and what holds them, near the player too. */
	void LogCars(const APawn* Player);
	/** One line on an overturned driven car: which, where, simulated or not, its reaction, its last placement. */
	void LogOverturned(const AGTPVehicle* Car, const APawn* Player, const TCHAR* What) const;
	double NextCarsLogTime = 10.0;
	int32 RecycledStuck = 0;
	int32 RecycledFar = 0;
	/** The cars loop of Tick met an index past the end once (gate 81's crash): logged the first time only. */
	bool bLoggedCarsShrank = false;
	/** Overturned (past 45 degrees) out of the player's sight: taken away at once. */
	int32 RecycledOverturned = 0;
	/** Every car taken off the road (stuck, far, and by the others: the chase script, the police), and cars put back from the pool. */
	int32 RecycledAll = 0;
	/** Far traffic puppets handed to Recycle and left where they were (the chase script meets them on the road). */
	int32 PuppetsLeft = 0;
	int32 ReusedFromPool = 0;
	/** The player's pawn last seen by the tick, and where: a line when it is gone (gate 62's -GTARun pass, no car near
	 *  the player from 236 s: no pawn, so no distance to spawn or take cars away by). */
	TWeakObjectPtr<const APawn> LastPlayerPawn;
	FVector LastPlayerAt = FVector::ZeroVector;
	/** Where the player was at the last tick with a player pawn, never cleared: a jump is measured from there. */
	FVector PlayerJumpCheckAt = FVector::ZeroVector;

public:
	/** Switches the skinned draw of a car body, tracked by a component tag so it is only changed once. */
	static void SetCarRenderStatic(USkeletalMeshComponent* Body, bool bStatic);

private:
	FRandomStream Random;
};
