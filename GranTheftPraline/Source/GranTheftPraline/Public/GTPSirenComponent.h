#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "GTPAudioSynths.h"
#include "GTPAudioTypes.h"
#include "GTPSirenComponent.generated.h"

class UGTPSynthSoundWave;

/**
 * Siren of a police car (ADR-0009, police in ADR-0004): French « deux tons »
 * or a chase yelp, synthesised (GTPAudioSynths.h), with the Doppler shift of
 * the car passing the listener. Silent and not ticking while Off.
 *
 * Police code turns it on with one call, without knowing the audio code:
 *     UGTPSirenComponent::SetSiren(PoliceCar, EGTPSirenMode::TwoTone);
 * The audio world subsystem also adds one, Off, to every actor tagged
 * GTPPolice (UGTPAudioSettings::SirenActorTag).
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPSirenComponent : public UAudioComponent
{
	GENERATED_BODY()

public:
	UGTPSirenComponent(const FObjectInitializer& ObjectInitializer);

	/** Finds or adds the siren of Vehicle and sets its mode. Returns the siren. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Siren")
	static UGTPSirenComponent* SetSiren(AActor* Vehicle, EGTPSirenMode Mode);

	/** The siren of Vehicle, added (Off) if it has none. */
	static UGTPSirenComponent* FindOrAdd(AActor* Vehicle);

	UFUNCTION(BlueprintCallable, Category = "Audio|Siren")
	void SetMode(EGTPSirenMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Audio|Siren")
	EGTPSirenMode GetMode() const { return Mode; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void EnsureWave();
	void UpdateDoppler(float DeltaTime);

	UPROPERTY(Transient)
	TObjectPtr<UGTPSynthSoundWave> SirenWave;

	TSharedPtr<FGTPSirenSynth, ESPMode::ThreadSafe> Synth;
	EGTPSirenMode Mode = EGTPSirenMode::Off;
	FVector LastLocation = FVector::ZeroVector;
	bool bHasLastLocation = false;
	float SmoothedDoppler = 1.f;
	/** Time the synth needs to fade out after Off, before the voice is released (s). */
	float StopCountdown = 0.f;
};
