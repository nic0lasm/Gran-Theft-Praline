#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPTestExport.generated.h"

/**
 * Test-build export key. F9 writes a small text report that a tester can
 * attach to a bug: version, system, graphics card, player position, map,
 * and the frame times of the last minute. Nothing is sent anywhere.
 *
 * File: <Saved>/Exports/export-<date>.txt
 * Command line: -ExportKey=0 turns the key off, -ExportSeconds=<s> changes
 * the window of frame times (default 60).
 * Docs: docs/qa/test-build-spec.md
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPTestExport : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Writes the report now; returns the file path, or empty on failure. */
	FString ExportNow();

	/** p50, p95 and worst of the values, and how many exceed the limit. Static so it can be unit-tested. */
	struct FFrameStats
	{
		float P50 = 0.f;
		float P95 = 0.f;
		float Worst = 0.f;
		int32 Count = 0;
		int32 OverLimit = 0;
	};
	static FFrameStats ComputeStats(const TArray<float>& FrameMs, float LimitMs);

private:
	bool Tick(float DeltaTime);

	FTSTicker::FDelegateHandle TickHandle;
	TArray<float> FrameMs;
	TArray<double> FrameTimes;
	double WindowSeconds = 60.0;
	bool bKeyDownLastTick = false;
};
