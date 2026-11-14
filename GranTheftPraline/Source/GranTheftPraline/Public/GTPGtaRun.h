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
 *   1 carjack   the demo route takes a car in traffic (the driver gets out)       ok: the player is in a car
 *   2 chase     stars and police units build up, roadblock cars included          ok: 3 stars and 3 units or more
 *   3 parked    the player, back on foot, breaks into a car with an alarm (the traffic thread's
 *               UGTPTheftDemoLibrary: window, wiring, alarm)                          ok: the player is at the wheel
 *   4 walkarmed the player walks 4 s with the gun out, then 4 s aiming        ok: 3 m or more each (else: what holds him)
 *   5 aim       holds up a calm person from 4.5 m                                ok: a compliant person has his hands up
 *   6 aimclose  the same from 1.2 m, another person
 *   7 shoot     the player gets out and fires at the nearest person                 ok: a shot left the gun
 *
 * The order differs from the first request (parked car first) because the demo route's carjack and chase
 * are one script that starts the run. Every step has a time limit: a step that fails or times out is
 * logged and the next one starts. A last line "GTARun done: N/4 ok" ends the run.
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
	EResult CheckChase(FString& Detail);
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
	bool bDone = false;

	// Step data.
	int32 MaxUnits = 0;
	int32 MaxStars = 0;
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
