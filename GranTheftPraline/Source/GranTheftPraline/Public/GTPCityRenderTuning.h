#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPCityRenderTuning.generated.h"

class ULevel;

/**
 * Budget levers of the generated city, tried on a package without regenerating the map
 * (ADR-0010, worst 2 x 2 block of cells). The generation tags its instanced components:
 *   facades  GTP_Cull_<family>      family = ground, upper, eave, court, sign (place_facades.py)
 *   props    GTP_Cull_<set>_<size>  set = props or details, size = small, medium, large,
 *            GTP_SizeCm=<largest side>                                          (place_props.py)
 * Command line (nothing given: the subsystem does nothing):
 *   -CityCull=court=60,upper=180,props_small=20   end cull distance in metres per tag suffix
 *                                                 (culling starts at 80 % of it, as generated)
 *   -CityPropShadowBelow=1.0                      props smaller than this (m) cast no shadow
 * Applied to the persistent level at begin play and to every streamed cell as it is added.
 * One log line per application: "CityRenderTuning: <level> <components> components".
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPCityRenderTuning : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Parses "a=1,b=2" into Out (metres); false when empty or malformed. */
	static bool ParseCullList(const FString& List, TMap<FString, float>& Out);

private:
	void OnLevelAdded(ULevel* Level, UWorld* World);
	void Apply(ULevel* Level);

	TMap<FString, float> CullM;
	float ShadowBelowM = -1.f;
	FDelegateHandle LevelAddedHandle;
};
