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
 *
 * The order differs from the first request (parked car first) because the demo route's carjack and chase
 * are one script that starts the run. Every step has a time limit: a step that fails or times out is
 * logged and the next one starts.
 *
 * Two last lines: "GTARun police: N/4 ok" for the steps of the wanted level (witness, pursuit, outcome, wasted:
 * what the first milestone asks to prove in the packaged game) and "GTARun done: N/7 ok" for the seven steps
 * the gate has counted since candidate 41, so that its rule reads the same.
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
	EResult CheckParked(FString& Detail);
	EResult CheckWalkArmed(FString& Detail);
	EResult CheckAim(FString& Detail, bool bClose);
	EResult CheckShoot(FString& Detail);

	FTSTicker::FDelegateHandle TickHandle;
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
	bool bDone = false;

	// What happened to the player, by its real-time second (-1 = never): read by the outcome and wasted steps.
	TWeakObjectPtr<class UGTPWantedComponent> BoundWanted;
	TWeakObjectPtr<class UGTPCombatSubsystem> BoundCombat;
	double ArrestedAt = -1.0;
	double DiedAt = -1.0;
	double RespawnedAt = -1.0;
	double LostAt = -1.0;
	FVector ArrestedWhere = FVector::ZeroVector;
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
	double AimFirstReaction = -1.0;
	double LastAimMoveTime = -1.0;
	TWeakObjectPtr<AActor> AimedBefore;
	int32 ShotsFired = 0;
	double NextShot = 0.0;
	int32 ShotAttempts = 0;
};
