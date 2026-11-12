#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "GTPHornSynth.h"
#include "GTPHornComponent.generated.h"

class UGTPSynthSoundWave;

/**
 * Horn of a car, synthesised (GTPHornSynth.h). Added on demand to the car that
 * honks, silent and not ticking while off. Traffic code calls only:
 *     UGTPHornComponent::SetHorn(Car, true);
 * Each car gets its own pitch (plus or minus 8 %), so two horns never sound the same.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPHornComponent : public UAudioComponent
{
	GENERATED_BODY()

public:
	UGTPHornComponent(const FObjectInitializer& ObjectInitializer);

	/** Finds or adds the horn of Vehicle and sounds or stops it. Null when game audio is off. */
	static UGTPHornComponent* SetHorn(AActor* Vehicle, bool bOn);

	void SetOn(bool bOn);
	bool IsOn() const { return bHornOn; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void EnsureWave();

	UPROPERTY(Transient)
	TObjectPtr<UGTPSynthSoundWave> HornWave;

	TSharedPtr<FGTPHornSynth, ESPMode::ThreadSafe> Synth;
	bool bHornOn = false;
	float Pitch = 1.f;
	/** Time the synth needs to fade out after off, before the voice is released (s). */
	float StopCountdown = 0.f;
};
