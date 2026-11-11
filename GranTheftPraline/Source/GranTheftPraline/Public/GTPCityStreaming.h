#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPCityStreamingCore.h"
#include "GTPCityStreaming.generated.h"

class AActor;
class ULevelStreamingDynamic;

/**
 * Streams the 250 m cells of the quarter around the player (ADR-0010, option B).
 *
 * The persistent level holds one far proxy per cell (tools/city/cell_proxies.py, placed by
 * import_city.py with GTP_CITY_LEVEL=quarter): a StaticMeshActor tagged
 *   GTP_CellProxy, <level package, e.g. /Game/Maps/L_City_CellP0000P0000>, bounds:<x0>,<y0>,<x1>,<y1>
 * (cell box in Unreal cm). Each update the subsystem plans which detailed levels should be
 * loaded (GTPCityStreamingCore.h), requests them asynchronously with ULevelStreamingDynamic,
 * hides a proxy once its level is visible and shows it again before the level is released.
 *
 * Inert on maps without tagged proxies (the street test map). Command line:
 *   -CityCells=0 keeps every proxy and loads nothing;
 *   -CityCellLoad=<m>, -CityCellUnload=<m>, -CityCellMax=<n> override the rules.
 * Log lines "CityStreaming: load|visible|unload <cell> t=<s>" let the bench line up a hitch
 * with a cell change.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPCityStreaming : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	int32 GetCellCount() const { return Cells.Num(); }
	int32 GetVisibleDetailedCount() const;

protected:
	UPROPERTY(Config)
	float LoadRadiusM = 60.f;

	UPROPERTY(Config)
	float UnloadRadiusM = 90.f;

	UPROPERTY(Config)
	int32 MaxDetailed = 4;

	/** Seconds between two plans; loading itself is asynchronous. */
	UPROPERTY(Config)
	float UpdateSeconds = 0.25f;

private:
	struct FCell
	{
		FString Level;
		FString Name;
		TWeakObjectPtr<AActor> Proxy;
		TWeakObjectPtr<ULevelStreamingDynamic> Streaming;
		bool bRequested = false;
		bool bWasVisible = false;
	};

	void Update();
	void RequestLoad(FCell& Cell);
	void Release(FCell& Cell);

	TArray<FCell> Cells;
	std::vector<GTPCity::FCellBox> Boxes;
	GTPCity::FStreamingRules Rules;
	bool bEnabled = false;
	float SinceUpdate = 0.f;
};
