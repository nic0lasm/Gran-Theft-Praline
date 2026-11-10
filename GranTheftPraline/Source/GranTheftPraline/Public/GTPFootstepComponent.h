#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPAudioTypes.h"
#include "GTPSurfaceLogic.h"
#include "GTPFootstepComponent.generated.h"

class USkeletalMeshComponent;

/**
 * Footsteps of anyone with a mannequin skeleton: the player (GASP character)
 * and the pedestrians (AGTPPedestrian), without animation notifies, so it
 * works with motion matching and with any walk cycle.
 *
 * Each frame the two foot bones are compared with the ground height (one
 * trace below the actor every GroundProbeInterval); a foot that comes down
 * (FGTPFootPlantDetector) traces the ground under itself, resolves the surface
 * (UGTPAudioMix::ResolveSurface) and plays a step whose loudness follows the
 * ground speed. A fall that ends plays a landing. Far from the listener, or
 * inside a vehicle (owner hidden or attached), it does nothing.
 * Added at run time by UGTPAudioWorld to every ACharacter and AGTPPedestrian.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPFootstepComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Last surface stepped on (debug, tests). */
	UFUNCTION(BlueprintPure, Category = "Audio|Footsteps")
	EGTPSurface GetLastSurface() const { return LastSurface; }

	UFUNCTION(BlueprintPure, Category = "Audio|Footsteps")
	int32 GetStepCount() const { return StepCount; }

protected:
	virtual void BeginPlay() override;

private:
	USkeletalMeshComponent* FindBody() const;
	bool ShouldListen() const;
	void UpdateGround(float DeltaTime);
	void Step(const FVector& FootLocation, bool bLanding, float Gain);

	TWeakObjectPtr<USkeletalMeshComponent> Body;
	FGTPFootPlantSettings PlantSettings;
	FGTPFootPlantDetector Feet[2];
	FName FootBones[2];

	float GroundZ = 0.f;
	bool bHasGround = false;
	float GroundProbeTimer = 0.f;
	FVector LastLocation = FVector::ZeroVector;
	bool bHasLastLocation = false;
	float GroundSpeed = 0.f;
	float FallSpeed = 0.f;
	bool bWasFalling = false;
	EGTPSurface LastSurface = EGTPSurface::Stone;
	int32 StepCount = 0;
	/** Seconds between two ground probes (cheap: about four traces per second). */
	float GroundProbeInterval = 0.25f;
	bool bWarnedNoBones = false;
};
