#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GTPAudioTypes.h"
#include "GTPAmbienceBed.generated.h"

class AActor;
class UAudioComponent;
class UMaterialParameterCollection;
class USoundMix;
class UWorld;

/**
 * The ambience bed (ADR-0009): 2D loops layered over the day street loop of
 * AGTPPlayerController, crossfaded from where the listener is and what the
 * world does:
 *   zone of the zone grid  -> Pedestrian, Quay or Bridge layer, street loop lowered;
 *   night (MPC CityLights) -> Night layer, street loop lowered;
 *   rain (MPC RainIntensity, ADR-0008) -> Rain, or RainCar from inside a car;
 *   visible pedestrians nearby -> Crowd layer.
 * Loops: <AmbienceRoot>/SW_Amb_<Layer> (tools/audio/make_ambiences.py); a
 * missing loop is skipped. Owned and ticked by UGTPAudioWorld.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPAmbienceBed : public UObject
{
	GENERATED_BODY()

public:
	void Start(UWorld* World);
	void Stop();
	void Tick(float DeltaTime, const TArray<TWeakObjectPtr<AActor>>& Pedestrians);

	/** Counters for the "[Audio] bed ..." line. */
	FString Describe() const;

private:
	enum ELayer : int32
	{
		Pedestrian,
		Quay,
		Bridge,
		Night,
		Rain,
		RainCar,
		Crowd,
		NumLayers
	};

	float ReadParameter(FName Name) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Layers;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> StreetMix;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> Collection;

	TWeakObjectPtr<UWorld> WorldRef;
	float Targets[NumLayers] = {};
	float Current[NumLayers] = {};
	float StreetTarget = 1.f;
	float StreetApplied = 1.f;
	float SenseTimer = 0.f;
	EGTPAmbienceZone Zone = EGTPAmbienceZone::Street;
	float RainAmount = 0.f;
	float NightAmount = 0.f;
	int32 CrowdCount = 0;
};
