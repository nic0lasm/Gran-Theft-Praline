#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "GTPAudioSynths.h"
#include "GTPAudioTypes.h"
#include "GTPVehicleAudioComponent.generated.h"

class UGTPSynthSoundWave;
class UPrimitiveComponent;

/**
 * Body sounds of a car (ADR-0006): impacts and tyre squeal. The engine
 * stays in UGTPEngineSoundComponent (ADR-0003).
 * - Impacts: bound to the body's hit event; the change of speed sets the
 *   tier (light, medium, heavy), what was hit sets the kind (car, metal,
 *   wall, body, wood); a heavy hit may add breaking glass.
 * - Squeal: the sideways slide of the body and the Chaos wheel skid drive a
 *   synthesised squeal (GTPAudioSynths.h) played by this component.
 * Added at run time by UGTPAudioWorld to every AWheeledVehiclePawn: nothing
 * to change in the vehicle classes.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPVehicleAudioComponent : public UAudioComponent
{
	GENERATED_BODY()

public:
	UGTPVehicleAudioComponent(const FObjectInitializer& ObjectInitializer);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Plays an impact as if the body had been hit (tests, scripted crashes). */
	void PlayImpact(EGTPImpactKind Kind, int32 Tier, const FVector& Location);

	UFUNCTION(BlueprintPure, Category = "Audio|Vehicle")
	float GetSkidAmount() const { return SkidAmount; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	EGTPImpactKind ClassifyHit(const AActor* OtherActor, const FHitResult& Hit) const;
	float ComputeSkid() const;

	UPROPERTY(Transient)
	TObjectPtr<UGTPSynthSoundWave> SkidWave;

	TWeakObjectPtr<UPrimitiveComponent> BoundBody;
	TSharedPtr<FGTPSkidSynth, ESPMode::ThreadSafe> Skid;
	float SkidAmount = 0.f;
	float SilentTime = 0.f;
	float LastImpactTime = -100.f;
	float LastGlassTime = -100.f;
};
