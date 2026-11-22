#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPGtaRun.generated.h"

/**
 * -GTARun: one launch option that chains the existing demos into a 3 to 5 minute tour with one log line
 * per step, so the night review can read the result from the log:
 *
 *   GTARun step N: <name> start | ok <detail> | fail <reason>
 *
 * It adds no gameplay logic. The steps use the normal paths (the vehicle's BeginEnter, the weapon's FireAt,
 * the wanted component) and the existing demo route does the rest: -GTARun adds -PoliceChaseDemo to the
 * command line (module startup), and UGTPDemoRoute steals a car from its driver and runs the chase.
 *
 *    1 carjack   the demo route takes a car in traffic (the driver gets out)       ok: the player is in a car
 *    2 witness   the theft is phoned in by somebody who saw it                      ok: a call reached the police and gave a star
 *    3 chase     stars and police units build up, roadblock cars included          ok: 3 stars, 3 units or more, and 150 m driven
 *    4 pursuit   the flight goes on 40 s with nothing scripted about the level;    ok: the police follow (GTPChaseTally.h: the
 *                the police cars are measured since the level rose                  units' average speed and how near they stay)
 *    5 outcome   the player gives up: the car stops and waits                      ok: arrested and released on foot, or killed
 *                                                                                   and back, or the level was lost
 *    6 parked    the player, back on foot, breaks into a car with an alarm (the traffic thread's
 *                UGTPTheftDemoLibrary: window, wiring, alarm)                          ok: the player is at the wheel
 *    7 walkarmed the player walks 4 s with the gun out, then 4 s aiming        ok: 3 m or more each (else: what holds him)
 *    8 aim       holds up a calm person from 4.5 m                                ok: a compliant person has his hands up
 *    9 aimclose  the same from 1.2 m, another person
 *   10 shoot     the player gets out and fires at the nearest person                 ok: a shot left the gun
 *   11 wasted    the player takes a lethal hit                                       ok: dead, back on his feet at the
 *                                                                                   hospital, the level gone
 *   12 crash     at the wheel of a parked car, the player is put 18 m behind a      ok: his car is held behind it (stopped,
 *                MassTraffic car (a stopped one first) and drives into it at full     pushing it or pushed aside); fail: it
 *                throttle                                                             goes through the car the player sees
 *
 * The order differs from the first request (parked car first) because the demo route's carjack and chase
 * are one script that starts the run. Every step has a time limit: a step that fails or times out is
 * logged and the next one starts.
 *
 * Three last lines: "GTARun police: N/4 ok" for the steps of the wanted level (witness, pursuit, outcome, wasted:
 * what the first milestone asks to prove in the packaged game), "GTARun informative: N/1 ok (crash)" and
 * "GTARun done: N/7 ok" for the seven steps the gate has counted since candidate 41, so that its rule reads the same.
 * The crash step is informative (in neither count) until the near traffic cars have a body: MassTraffic's cars are
 * instances with no collision, so it proves the traversal on what the player sees (docs/gameplay/traffic-mass.md).
 *
 * After the outcome the run clears what is left of the level and the police cannot arrest the player until the
 * end (as in every run so far, where the chase script had left the arrest off): the steps on foot test the theft,
 * the walk and the hold-up, not the police.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPGtaRun : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** True when -GTARun is on the command line. */
	static bool IsRequested();

private:
	enum class EResult : uint8 { Pending, Ok, Fail };

	bool Tick(float DeltaTime);
	void BeginStep(int32 Index);
	void EndStep(EResult Result, const FString& Detail);

	EResult CheckCarjack(FString& Detail);
	EResult CheckWitness(FString& Detail);
	EResult CheckChase(FString& Detail);
	EResult CheckPursuit(FString& Detail);
	EResult CheckOutcome(FString& Detail);
	EResult CheckWasted(FString& Detail);
	/** The metres and the best speed of the player's car since the wheel was taken (read by the chase step). */
	void TrackFlight();
	/** Binds the arrest, death and respawn events once, and notes when the level is lost by itself. */
	void TrackWanted();
	/**
	 * The picture at the end of the chase step (Saved/Screenshots/.../gtarun-chase.png): the player's car and the police
	 * behind it, seen from in front of the car, by day, motion blur off. The player's own camera looks ahead and never
	 * shows the cars that follow. One tick with the view on a camera that rides with the car, the picture, the view back.
	 */
	void StartChasePicture();
	void TickChasePicture();
	int32 ChasePicturePhase = 0;
	TWeakObjectPtr<class ACameraActor> ChasePictureCamera;
	EResult CheckParked(FString& Detail);
	EResult CheckWalkArmed(FString& Detail);
	EResult CheckAim(FString& Detail, bool bClose);
	EResult CheckShoot(FString& Detail);
	EResult CheckCrash(FString& Detail);
	/**
	 * Crash step: a MassTraffic car with room behind it and the ground there, for the player's car. 1 chosen, 0 none for now,
	 * -1 never (no MassTraffic); Why says why not.
	 */
	int32 ChooseCrashTarget(class UWorld& World, const class AGTPVehicle& Car, FString& Why);
	/** Crash step, every frame: holds the player's car behind its target, then drives it in and measures (Crash.Verdict). */
	bool TickCrash(float DeltaTime);
	/** Crash step: the pedals released and the frame ticker gone. */
	void EndCrashDrive();

	FTSTicker::FDelegateHandle TickHandle;
	FTSTicker::FDelegateHandle CrashHandle;
	FTSTicker::FDelegateHandle WalkHandle;
	FVector WalkDirection = FVector::ZeroVector;
	int32 WalkInputs = 0;
	int32 WalkPendingBefore = 0;
	int32 WalkRejected = 0;
	int32 WalkArmedPhase = 0;
	double WalkArmedPhaseStart = -1.0;
	bool bWalkArmedExit = false;
	FVector WalkArmedFrom = FVector::ZeroVector;
	double WalkArmedYaw = 0.0;
	int32 Step = -1;
	double StepStart = 0.0;
	double RunStart = -1.0;
	int32 OkCount = 0;
	int32 PoliceOkCount = 0;
	int32 InfoOkCount = 0;
	bool bDone = false;

	// What happened to the player, by its real-time second (-1 = never): read by the outcome and wasted steps.
	TWeakObjectPtr<class UGTPWantedComponent> BoundWanted;
	TWeakObjectPtr<class UGTPCombatSubsystem> BoundCombat;
	double ArrestedAt = -1.0;
	double DiedAt = -1.0;
	double RespawnedAt = -1.0;
	double LostAt = -1.0;
	FVector ArrestedWhere = FVector::ZeroVector;
	/** At the arrest: the nearest chasing car, roadblock car and officer (m, -1 = none), the player's speed, and how long
	 * before the chase script had last moved the player's car (s). */
	float ArrestChasing = -1.f;
	float ArrestBlock = -1.f;
	float ArrestOfficer = -1.f;
	float ArrestKmh = 0.f;
	double ArrestSinceRelocation = 1.0e9;
	FVector DiedWhere = FVector::ZeroVector;
	FName RespawnHospital;
	int32 LastStars = 0;
	double LastTrackTime = 0.0;
	/** The run itself cleared the level (after the outcome): not a level lost by the player. */
	bool bRunCleared = false;
	double ChaseBegan = -1.0;
	int32 OutcomePhase = 0;
	int32 OutcomeStars = 0;
	double GaveUpAt = -1.0;
	float GaveUpNearest = -1.f;
	bool bWastedExit = false;
	int32 WastedPhase = 0;
	int32 WastedTries = 0;
	int32 WastedStars = 0;
	double WastedHitAt = -1.0;
	FVector WastedFrom = FVector::ZeroVector;

	// Step data.
	int32 MaxUnits = 0;
	int32 MaxStars = 0;
	/** Chase step: what the player's car really drove, and its best speed. */
	TWeakObjectPtr<const class AGTPVehicle> ChaseCar;
	FVector ChaseLastAt = FVector::ZeroVector;
	double ChaseDrivenCm = 0.0;
	float ChasePeakKmh = 0.f;
	static constexpr double ChaseDrivenWantedCm = 15000.0;
	TWeakObjectPtr<class AGTPVehicle> ParkedTarget;
	TWeakObjectPtr<class ACharacter> ParkedCharacter;
	bool bParkedStarted = false;
	bool bExitRequested = false;
	bool bParkedExitRequested = false;
	bool bAimExitRequested = false;
	/** The hold-up steps are pictured by day, motion blur off (set once). */
	bool bHoldUpLit = false;
	double AimFirstReaction = -1.0;
	double LastAimMoveTime = -1.0;
	TWeakObjectPtr<AActor> AimedBefore;
	/** The person held up at the intended range, and since when (the hold-up steps want his body to show it). */
	TWeakObjectPtr<AActor> AimedHeld;
	double AimedHeldSince = 0.0;
	/** Since when the pose of the hold-up is proved and the picture waits for the view to come onto the person (-1 = not). */
	double HoldUpProvedSince = -1.0;
	int32 ShotsFired = 0;
	double NextShot = 0.0;
	int32 ShotAttempts = 0;

	/** Crash step: its car, its target and what it measured (reset at each step). Distances in cm, speeds in km/h. */
	struct FCrashRun
	{
		TWeakObjectPtr<class AGTPVehicle> Car;
		TWeakObjectPtr<class ACharacter> Character;
		/** 0 get a car, 1 entering, 2 choose a target, 3 held behind it, 4 driving into it. */
		int32 Phase = 0;
		int32 Attempts = 0;
		TArray<uint64> Tried;
		uint64 TargetId = 0;
		FVector TargetAt = FVector::ZeroVector;
		FVector TargetForward = FVector::ForwardVector;
		float TargetRadius = 0.f;
		float TargetKmh = 0.f;
		float OwnHalfLength = 230.f;
		double PhaseStart = 0.0;
		double HoldUntil = 0.0;
		double DriveStart = 0.0;
		double LastSeen = 0.0;
		double ContactAt = -1.0;
		double SlowSince = -1.0;
		float ContactKmh = 0.f;
		float MinKmhAfter = 0.f;
		float PeakKmh = 0.f;
		float ClosestCm = 1.0e9f;
		float MaxAsideCm = 0.f;
		float PrevAlong = -1.0e9f;
		/** Where the player's car stood in the target's frame when the verdict came (along its axis, aside), and its speed. */
		float EndAlong = 0.f;
		float EndAside = 0.f;
		float EndKmh = 0.f;
		/** 0 pending, 1 held behind the car, 2 went through it, 3 never reached it (next attempt). */
		int32 Verdict = 0;
		FString Note;
	};
	FCrashRun Crash;
};
