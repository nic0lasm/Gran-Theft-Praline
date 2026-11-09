#include "GTPFrameTimeRecorder.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "CoreGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderCore.h"
#include "RHI.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPFrameTime, Log, All);

namespace
{
	constexpr float BudgetMs = 33.3f;

	float Percentile(TArray<float>& Sorted, double Fraction)
	{
		if (Sorted.Num() == 0)
		{
			return 0.f;
		}
		const int32 Index = FMath::Clamp(FMath::CeilToInt(Fraction * Sorted.Num()) - 1, 0, Sorted.Num() - 1);
		return Sorted[Index];
	}

	FString Describe(const TCHAR* Name, TArray<float> Values)
	{
		Values.Sort();
		int32 WithinBudget = 0;
		for (float Value : Values)
		{
			WithinBudget += Value <= BudgetMs ? 1 : 0;
		}
		const double Share = Values.Num() > 0 ? 100.0 * WithinBudget / Values.Num() : 0.0;
		return FString::Printf(TEXT("%-6s p50 %6.2f ms | p95 %6.2f ms | p99 %6.2f ms | max %7.2f ms | <= 33.3 ms: %5.1f %%"),
			Name, Percentile(Values, 0.50), Percentile(Values, 0.95), Percentile(Values, 0.99),
			Values.Num() > 0 ? Values.Last() : 0.f, Share);
	}
}

void UGTPFrameTimeRecorder::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const TCHAR* CommandLine = FCommandLine::Get();
	FString RequestedPath;
	const bool bWantsLog = FParse::Param(CommandLine, TEXT("FrameLog"))
		|| FParse::Value(CommandLine, TEXT("FrameLogFile="), RequestedPath);

	FParse::Value(CommandLine, TEXT("FrameLogWarmup="), WarmupSeconds);
	FParse::Value(CommandLine, TEXT("ExitAfter="), ExitAfterSeconds);

	if (!bWantsLog && ExitAfterSeconds <= 0.0)
	{
		return;
	}

	if (bWantsLog)
	{
		bRecording = true;
		OutputPath = !RequestedPath.IsEmpty()
			? RequestedPath
			: FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Profiling"),
				FString::Printf(TEXT("frametimes-%s.csv"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
		OutputPath = FPaths::ConvertRelativePathToFull(OutputPath);
		Samples.Reserve(60 * 60 * 15);
		UE_LOG(LogGTPFrameTime, Log, TEXT("Frame time recording to %s (warm-up %.0f s)"), *OutputPath, WarmupSeconds);
	}

	StartSeconds = FPlatformTime::Seconds();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPFrameTimeRecorder::Tick));
}

void UGTPFrameTimeRecorder::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	WriteResults();
	Super::Deinitialize();
}

bool UGTPFrameTimeRecorder::Tick(float DeltaTime)
{
	const double Elapsed = FPlatformTime::Seconds() - StartSeconds;

	if (FirstTickSinceLaunch < 0.0)
	{
		FirstTickSinceLaunch = FPlatformTime::Seconds() - GStartTime;
	}

	if (bRecording && Elapsed >= WarmupSeconds)
	{
		FFrameSample& Sample = Samples.AddDefaulted_GetRef();
		Sample.TimeSeconds = Elapsed;
		Sample.FrameMs = DeltaTime * 1000.f;
		Sample.GameMs = FPlatformTime::ToMilliseconds(GGameThreadTime);
		Sample.RenderMs = FPlatformTime::ToMilliseconds(GRenderThreadTime);
		Sample.GpuMs = FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles(0));
	}

	if (ExitAfterSeconds > 0.0 && Elapsed >= ExitAfterSeconds && !bExitRequested)
	{
		bExitRequested = true;
		UE_LOG(LogGTPFrameTime, Log, TEXT("ExitAfter reached (%.0f s), quitting."), ExitAfterSeconds);
		WriteResults();
		FPlatformMisc::RequestExit(false, TEXT("GTPFrameTimeRecorder"));
	}
	return true;
}

void UGTPFrameTimeRecorder::WriteResults()
{
	if (!bRecording || bWritten)
	{
		return;
	}
	bWritten = true;

	TArray<FString> Lines;
	Lines.Reserve(Samples.Num() + 1);
	Lines.Add(TEXT("frame,time_s,frame_ms,game_ms,render_ms,gpu_ms"));

	TArray<float> Frame, Game, Render, Gpu;
	Frame.Reserve(Samples.Num());
	Game.Reserve(Samples.Num());
	Render.Reserve(Samples.Num());
	Gpu.Reserve(Samples.Num());

	for (int32 Index = 0; Index < Samples.Num(); ++Index)
	{
		const FFrameSample& S = Samples[Index];
		Lines.Add(FString::Printf(TEXT("%d,%.3f,%.3f,%.3f,%.3f,%.3f"),
			Index, S.TimeSeconds, S.FrameMs, S.GameMs, S.RenderMs, S.GpuMs));
		Frame.Add(S.FrameMs);
		Game.Add(S.GameMs);
		Render.Add(S.RenderMs);
		Gpu.Add(S.GpuMs);
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true);
	const bool bCsvOk = FFileHelper::SaveStringArrayToFile(Lines, *OutputPath);

	const double Duration = Samples.Num() > 0 ? Samples.Last().TimeSeconds - Samples[0].TimeSeconds : 0.0;
	TArray<FString> Summary;
	Summary.Add(FString::Printf(TEXT("first frame %.1f s after launch"), FirstTickSinceLaunch));
	Summary.Add(FString::Printf(TEXT("frames %d over %.1f s (warm-up %.0f s excluded)"), Samples.Num(), Duration, WarmupSeconds));
	Summary.Add(Describe(TEXT("frame"), Frame));
	Summary.Add(Describe(TEXT("game"), Game));
	Summary.Add(Describe(TEXT("render"), Render));
	Summary.Add(Describe(TEXT("gpu"), Gpu));

	const FString SummaryPath = FPaths::ChangeExtension(OutputPath, TEXT("summary.txt"));
	FFileHelper::SaveStringArrayToFile(Summary, *SummaryPath);

	UE_LOG(LogGTPFrameTime, Log, TEXT("Frame time CSV %s: %s"), bCsvOk ? TEXT("written") : TEXT("FAILED"), *OutputPath);
	for (const FString& Line : Summary)
	{
		UE_LOG(LogGTPFrameTime, Log, TEXT("%s"), *Line);
	}
}
