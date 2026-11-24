#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPTraffic.h"
#include "GTPDemoRoute.generated.h"

class AGTPVehicle;
class APlayerController;

/**
 * Plays a fixed route by itself, so a packaged build can film and measure
 * walking and driving with nobody at the keyboard.
 *
 * Command line: -DemoRoute (with -SkipTitle; add -TestVehicle on a map with
 * no parked car). The route:
 *   1. looks around the street,
 *   2. walks down the street, turns back and walks to the nearest free car,
 *   3. gets in with the normal enter action,
 *   4. drives down the street at city speed, keeping the street heading,
 *   5. stops, gets out and stays on foot.
 *
 * Everything goes through the normal gameplay paths: movement input on foot,
 * control rotation for the view, the vehicle interaction component to get
 * in and out, and the vehicle movement inputs to drive. Phase changes are
 * logged (LogGTPDemo) with their time, to match them with the frame times.
 *
 * -DemoDeterministic=1 makes the route repeatable (see GTPDeterminism.h): decisions are
 * taken in fixed steps of 1/30 s of simulated game time (not real time, not frame
 * rate), the walk ends at a waypoint (-DemoWalkCm=<cm> from the start, default 4500)
 * instead of after a duration, junction choices use a fixed seed, and the position
 * and heading are logged at fixed instants ("DemoPos t=... x=... y=... yaw=... kmh=...")
 * so two runs can be compared with tools/perf/compare_demo_positions.py.
 *
 * -DemoDriveTime=<s> sets how long step 4 drives (default 45 s).
 * -DemoCarjack replaces steps 2 and 3: after looking around, the player walks
 * to the nearest car with a non-player driver, steps in front of it so that
 * it stops, presses the enter action at the driver door (the driver is pulled
 * out) and drives on. Each step is logged.
 * -PoliceChaseDemo = -DemoCarjack plus a scripted chase (default 150 s of driving): the stolen
 * car runs at 40 km/h, the level is raised to 2 stars 8 s after the wheel is taken and to 3 after 30 s,
 * from 12 s on the player flees at 45 to 70 km/h by the width of the street (GTPRoadWidth.h), through the corners at 5 m/s2, keeping to the lanes
 * that never end and to the main axes (arrest possible from 100 s; 40 km/h before) and the level is left to fall by itself. Held up
 * on its lane against a fixed obstacle of the map, the car marks that lane blocked for the flights and goes on from another lane.
 * When the map has a flight circuit (Content/Test/Circuits/<Map>.circuit.csv, GTPFlightCircuit.h, off with -FlightCircuit=0) the flight
 * keeps to it instead: its approach from the theft, then its lap again and again, on the circuit's driving line, from 4 s on at 100 km/h
 * on a quay and 75 on another main axis; the car is then only ever put back on a lane of the lap, and each time counts in "relocations=".
 * Logged as "DemoChase t=... stars=... phase=..." every 5 s.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPDemoRoute : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** True while the chase script drives the stolen car. */
	bool IsChaseDriving() const { return bChase && Phase == EPhase::Drive; }
	/**
	 * Chase script, for -GTARun: from now the flight goes on with nothing scripted about the level: no more forced
	 * stars, and the police may arrest the player (the script alone forces 3 stars until 60 s and allows the
	 * arrest from 100 s).
	 */
	void FreeChaseLevel();
	/** Chase script, for -GTARun: the player gives up: the car brakes to a stop and stays there, the player at the wheel. */
	void GiveUpChase();
	bool HasGivenUp() const { return bGaveUp; }
	/**
	 * Chase script, for -GTARun: the flight circuit of the map names the lanes the filmed flight is to end on (the
	 * place Bellecour), and the car is on one of them. -GTARun holds the end of its pursuit until then, 20 s at most.
	 */
	bool HasFlightEndPlace() const;
	bool IsOnFlightEndPlace() const;
	/**
	 * -GTARunEncore (the gate's filmed run only): once -GTARun is over, the chase script plays again from its start, so the
	 * filmed minutes show play to the end instead of a still street. The player gets out if at the wheel, is put back on
	 * the quai Jean Moulin, takes the nearest free car and flees, with the same script as the first chase.
	 */
	void StartEncore();
	/** Times the chase script put its car back on a lane (held up for good, off the road, end of the lanes). */
	int32 GetChaseRelocations() const { return ChaseRelocations; }
	/** Seconds (real time) since the chase script last put its car on another lane; a large number when it never did. */
	double GetSecondsSinceRelocation() const;
	/** After a relocation nobody can arrest the player for this long (s): the script put the car there, not the player. */
	static constexpr double RelocationGraceSeconds = 8.0;

private:
	enum class EPhase : uint8
	{
		WaitForPawn,
		LookAround,
		WalkStreet,
		TurnBack,
		WalkToCar,
		Enter,
		Drive,
		Stop,
		Exit,
		Done,
		Carjack
	};

	bool Tick(float DeltaTime);
	/** One decision step (the whole route logic); DeltaTime is real in a normal run, a fixed step when deterministic. */
	bool RunStep(float DeltaTime);
	/** Seconds on the route's clock: real time normally, simulated time (sum of the fixed steps) when deterministic. */
	double Clock() const;
	void SetPhase(EPhase NewPhase);
	AGTPVehicle* FindCar() const;
	AGTPVehicle* FindTrafficCar() const;
	/** bReverse: the brake pedal backs the car (chase script backing off); otherwise it only brakes. */
	void DriveInputs(AGTPVehicle* Car, float Throttle, float Brake, float Steering, bool bHandbrake, bool bReverse = false) const;
	APlayerController* GetController() const;
	/** Chase demo: the car goes on from a lane 80 to 150 m away (end of the lanes, or held up for good); false when none fits. */
	bool RelocateChase(AGTPVehicle* Driven, APlayerController* PC, const TCHAR* Why);

	FTSTicker::FDelegateHandle TickHandle;
	EPhase Phase = EPhase::WaitForPawn;
	double PhaseStart = 0.0;
	bool bDeterministic = false;
	/** Simulated seconds (deterministic runs only). */
	double SimTime = 0.0;
	double Accumulator = 0.0;
	double LastWorldTime = -1.0;
	int32 LastLoggedSlot = -1;
	int32 LastLoggedLane = INDEX_NONE;
	/** Movement input held between fixed steps and applied every frame. */
	FVector HeldMove = FVector::ZeroVector;
	FVector WalkOrigin = FVector::ZeroVector;
	float WalkStreetCm = 4500.f;
	double RouteStart = 0.0;
	float StreetYaw = 0.f;
	float StartControlYaw = 0.f;
	TWeakObjectPtr<AGTPVehicle> Car;
	double StuckSince = 0.0;
	double UnstickUntil = 0.0;
	float UnstickSteer = 0.f;
	double DriveTime = 45.0;
	bool bCarjack = false;
	bool bChase = false;
	bool bLevelFree = false;
	bool bGaveUp = false;
	int32 ChaseRelocations = 0;
	/** FPlatformTime second of the last relocation (-1 = never). */
	double LastRelocationAt = -1.0;
	double RamUntil = 0.0;
	double SlowSince = 0.0;
	int32 ChaseStuckCount = 0;
	/** Where the chase car was last held up, and how many times in a row within 15 m of there (twice: put on another lane). */
	FVector HeldUpAt = FVector::ZeroVector;
	int32 HeldUpHere = 0;
	/** RelocateChase: where the car was put; held there with the physics off until RelocateHoldUntil. */
	FTransform RelocatedTo = FTransform::Identity;
	bool bRelocateHeld = false;
	/** Since when the chase car has been more than 6 m off its lane (0 = on it). */
	double OffLaneSince = 0.0;
	/** The chase car has been within 3 m of its lane since the lanes were last joined (the off-lane rule waits for it). */
	bool bOnLaneOnce = false;
	TWeakObjectPtr<AGTPVehicle> RelocatedCar;
	double RelocateHoldUntil = 0.0;
	double NextDriveDiag = 0.0;
	/** Highest speed of the driven car in the chase (km/h), logged with DemoChase. */
	float PeakKmh = 0.f;
	double NextRelocate = 0.0;
	double NextClear = 0.0;
	double NextChaseLog = 0.0;
	/** Seconds well under the cruise speed by cause (GTPDrive::ELimit), logged every 10 s. */
	float LimitSeconds[5] = {0.f, 0.f, 0.f, 0.f, 0.f};
	double NextLimitLog = 0.0;
	double LastLimitTime = 0.0;
	double CarjackRetarget = 0.0;
	/** Drives along the traffic lanes once the car is on one (else holds the street heading). */
	FGTPLaneFollower Follower;
	double NextLaneSearch = 0.0;
	/** -DemoToRiver: drive to the east end of the lanes (quai du Rhône, pont Lafayette), then roam. */
	bool bToRiver = false;
	bool bGoalSet = false;
	FVector RiverGoal = FVector::ZeroVector;
};
