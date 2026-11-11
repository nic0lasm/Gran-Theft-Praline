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
 *   quarter  GTP_Cull_proxy, GTP_Cull_terrain on the persistent level's static mesh actors
 *            (import_city.py); maps built before that tag are recognised by mesh folder
 *            (/CellProxies/, /Terrain/).
 * Command line (nothing given: the subsystem does nothing):
 *   -CityCull=court=60,upper=180,props_small=20   end cull distance in metres per tag suffix
 *                                                 (culling starts at 80 % of it, as generated)
 *   -CityPropShadowBelow=1.0                      props smaller than this (m) cast no shadow
 *   -CityCull=proxy=400,terrain=600               also works on the quarter's plain components
 *   -CityProxies=0                                far proxies never drawn (cost of the proxies)
 *   -CityTuneReport=1                             only the log line, nothing changed
 *   A distance of 0 hides the family (-CityCull=details_small=0,props_small=0: A/B on one package).
 * Streamed cells (L_City_Cell*) get defaults, budget compensation of their furniture and details:
 *   court=60, sign=45, no shadow under 1 m, props and details distances at -CityCellDetailPct=
 *   percent (60, bounded 10..100, GTPCityTuningCore.h). -CityCull and -CityPropShadowBelow win
 *   over them; -CityCellDefaults=0 turns them off.
 * Applied to the persistent level at begin play and to every streamed cell as it is added.
 * One log line per application:
 *   "CityRenderTuning: <level> <n> components changed; <family>=<changed>/<found> ..."
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

	/** Family of an untagged quarter component from its mesh path ("proxy", "terrain" or empty). */
	static FString ImpliedFamily(const FString& MeshPath);

	/** True for a streamed cell level of the quarter (L_City_Cell<X><Y>). */
	static bool IsCellLevel(const FString& LevelPackageName);

	/** Default cull distances (m) of the cell levels, per family. */
	static void CellDefaultCull(TMap<FString, float>& Out);

private:
	void OnLevelAdded(ULevel* Level, UWorld* World);
	void Apply(ULevel* Level);

	TMap<FString, float> CullM;
	float ShadowBelowM = -1.f;
	bool bHideProxies = false;
	bool bShadowBelowGiven = false;
	bool bCellDefaults = true;
	int32 CellDetailPct = 60;
	FDelegateHandle LevelAddedHandle;
};
