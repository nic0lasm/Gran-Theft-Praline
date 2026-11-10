#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPAudioWorld.generated.h"

class AActor;

/**
 * Audio of a game world (ADR-0006). Without touching the classes of the
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

private:
	void OnActorSpawned(AActor* Actor);
	void LogStats();

	FDelegateHandle SpawnHandle;
	bool bDrivingMixOn = false;
	bool bLogStats = false;
	float StatsTimer = 0.f;
	int32 Equipped = 0;
};
