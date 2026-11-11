#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPAudioWorld.generated.h"

class AActor;
class UGTPAmbienceBed;
class UGTPStreetReactions;

/** A loud sound at Location that frightens people within Radius (cm); Cause may be null. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPLoudSoundEvent, const FVector& /*Location*/, float /*Radius*/, AActor* /*Cause*/);

/**
 * Audio of a game world (ADR-0009). Without touching the classes of the
 * other systems, it gives sounds to what appears in the world:
 * - footsteps (UGTPFootstepComponent) to every ACharacter and pedestrian;
 * - impacts and tyre squeal (UGTPVehicleAudioComponent) to every car, and
 *   the Vehicles class to its engine sound;
 * - a siren (UGTPSirenComponent, Off) to every actor tagged GTPPolice.
 * It pushes the user mix, the driving mix while the player is in a car, and
 * writes the soak test line "[Audio] ..." once per second when
 * UGTPAudioSettings::bLogStats is set (or -AudioStats on the command line).
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPAudioWorld : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Gives sounds to an actor if it needs some (called for every spawned actor). */
	void Equip(AActor* Actor);

	/**
	 * Broadcast for each loud sound of the game (crashes now; horns, shots and
	 * explosions later). Pedestrians bind UGTPPedestrianSubsystem::ScareAround
	 * here through a lambda (it returns the count of people scared).
	 */
	FGTPLoudSoundEvent OnLoudSound;

	/** Ducks the street ambience and the traffic for GunshotDuckSeconds (each shot restarts it). */
	void DuckForGunshot();

private:
	void OnActorSpawned(AActor* Actor);
	void LogStats();
	void UpdateRecording();
	void UpdateTestSiren(float DeltaTime);
	/** Keeps only the nearest engines and walkers audible (UGTPAudioSettings Budget). */
	void UpdateBudget(float DeltaTime);

	TArray<TWeakObjectPtr<class UGTPFootstepComponent>> Walkers;
	TArray<TWeakObjectPtr<class UGTPEngineSoundComponent>> Engines;
	float BudgetTimer = 0.f;
	float DuckLeft = 0.f;
	int32 EnginesAudible = 0;
	int32 WalkersHeard = 0;

	/** -TestSiren: the nearest AI car runs a siren (two-tone, then yelp), to hear it pass without the police. */
	bool bTestSiren = false;
	float TestSirenTimer = 0.f;
	TWeakObjectPtr<AActor> TestSirenCar;

	/** -RecordAudio[=seconds]: records the game output to a WAV next to the video capture. */
	float RecordDelay = -1.f;
	float RecordSeconds = 0.f;
	float RecordStartedAt = -1.f;
	FString RecordDir;
	bool bRecording = false;

	UPROPERTY(Transient)
	TObjectPtr<UGTPAmbienceBed> Bed;

	UPROPERTY(Transient)
	TObjectPtr<UGTPStreetReactions> Reactions;

	/** Crowd pedestrians seen so far, for the crowd layer of the bed. */
	TArray<TWeakObjectPtr<AActor>> Pedestrians;

	FDelegateHandle SpawnHandle;

	/** Actors spawned since the last tick: equipped once they have begun play, never mid-spawn. */
	TArray<TWeakObjectPtr<AActor>> PendingEquip;
	bool bDrivingMixOn = false;
	bool bLogStats = false;
	float StatsTimer = 0.f;
	int32 Equipped = 0;
};
