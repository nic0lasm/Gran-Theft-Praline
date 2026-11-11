#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GTPMissionSounds.generated.h"

class UGTPMissionSubsystem;
class UWorld;

/**
 * Mission stings played by listening to UGTPMissionSubsystem (ADR-0009, docs/audio/audio-design.md,
 * "Interiors and missions"), owned by UGTPAudioWorld. The mission code knows nothing of it:
 * - OnStartAvailable: "available" each time the player enters a start zone;
 * - OnObjective: "start" for the first objective of a run, then "objective";
 * - OnSucceeded: "success", then "reward" a second later when it pays;
 * - OnFailed: "fail".
 * Nothing is bound under -Missions=0 (no subsystem). The subsystem is looked up until found,
 * since the two subsystems do not start in a fixed order.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPMissionSounds : public UObject
{
	GENERATED_BODY()

public:
	void Start(UWorld* World);
	void Stop();
	void Tick(float DeltaTime);

	/** "missions start 1 objective 4 success 1 fail 0" for the stats line. */
	FString Describe() const;

private:
	void Bind();
	void OnStartAvailable(FName MissionId);
	void OnObjective(FName MissionId, FName HintKey);
	void OnFailed(FName Reason);
	void OnSucceeded(FName MissionId, int32 Reward);

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<UGTPMissionSubsystem> MissionsPtr;
	FDelegateHandle AvailableHandle;
	FDelegateHandle ObjectiveHandle;
	FDelegateHandle FailedHandle;
	FDelegateHandle SucceededHandle;
	float LookupLeft = 0.f;
	int32 Lookups = 0;
	FName RunMission;
	FName LastHint;
	/** Seconds before the reward sting, -1 when none is due. */
	float RewardLeft = -1.f;
	int32 Starts = 0;
	int32 Objectives = 0;
	int32 Successes = 0;
	int32 Fails = 0;
};
