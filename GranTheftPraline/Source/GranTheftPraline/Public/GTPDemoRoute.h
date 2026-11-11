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
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPDemoRoute : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

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
	void DriveInputs(AGTPVehicle* Car, float Throttle, float Brake, float Steering, bool bHandbrake) const;
	APlayerController* GetController() const;

	FTSTicker::FDelegateHandle TickHandle;
	EPhase Phase = EPhase::WaitForPawn;
	double PhaseStart = 0.0;
	bool bDeterministic = false;
	/** Simulated seconds (deterministic runs only). */
	double SimTime = 0.0;
	double Accumulator = 0.0;
	double LastWorldTime = -1.0;
	int32 LastLoggedSlot = -1;
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
	double CarjackRetarget = 0.0;
	/** Drives along the traffic lanes once the car is on one (else holds the street heading). */
	FGTPLaneFollower Follower;
	double NextLaneSearch = 0.0;
	/** -DemoToRiver: drive to the east end of the lanes (quai du Rhône, pont Lafayette), then roam. */
	bool bToRiver = false;
	bool bGoalSet = false;
	FVector RiverGoal = FVector::ZeroVector;
};
