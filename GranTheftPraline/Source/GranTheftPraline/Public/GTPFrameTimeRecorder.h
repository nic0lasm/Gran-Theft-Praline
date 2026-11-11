#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPFrameTimeRecorder.generated.h"

/**
 * Records per-frame timings to a CSV file. Works in every build
 * configuration, Shipping included, so the packaged game can be measured.
 *
 * Command line:
 *   -FrameLog                write <Saved>/Profiling/frametimes-<date>.csv
 *   -FrameLogFile=<path>     write to this file instead
 *   -FrameLogWarmup=<s>      ignore the first seconds (default 5)
 *   -ExitAfter=<s>           quit cleanly after this many seconds (soak tests)
 *
 * Columns: frame, time_s, frame_ms, game_ms, render_ms, gpu_ms.
 * A summary (p50, p95, p99, max, share of frames <= 33.3 ms) is written to
 * the log and next to the CSV as <name>.summary.txt.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPFrameTimeRecorder : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	struct FFrameSample
	{
		double TimeSeconds;
		float FrameMs;
		float GameMs;
		float RenderMs;
		float GpuMs;
	};

	bool Tick(float DeltaTime);
	void WriteResults();

	FTSTicker::FDelegateHandle TickHandle;
	TArray<FFrameSample> Samples;
	FString OutputPath;
	double StartSeconds = 0.0;
	double WarmupSeconds = 5.0;
	double ExitAfterSeconds = 0.0;
	double FirstTickSinceLaunch = -1.0;
	bool bRecording = false;
	bool bExitRequested = false;
	double CsvStopSeconds = -1.0;
	FString CsvCopyTo;
	void CopyCsvProfile() const;
	bool bWritten = false;
};
