#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPFrameCapture.generated.h"

/**
 * Films the game from the inside: saves the rendered frames as numbered
 * JPEG files, which tools/build/frames-to-movie.swift turns into a video.
 * Needs no screen-recording permission and works in Shipping builds.
 *
 * Command line:
 *   -CaptureVideo=<s>     capture this many seconds (enables the capture)
 *   -CaptureFps=<n>       frames per second to save (default 10)
 *   -CaptureDelay=<s>     wait before starting (default 3)
 *   -CaptureDir=<path>    output folder (default <Saved>/Capture/<date>)
 *
 * Reading back frames costs GPU time: do not combine with a frame time
 * measurement run.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPFrameCapture : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float DeltaTime);
	void HandleScreenshot(int32 Width, int32 Height, const TArray<FColor>& Colors);
	void Finish();

	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle ScreenshotHandle;
	FString OutputDir;
	double StartSeconds = 0.0;
	double DelaySeconds = 3.0;
	double DurationSeconds = 0.0;
	double FramesPerSecond = 10.0;
	double NextCaptureSeconds = 0.0;
	int32 FrameIndex = 0;
	bool bRequestPending = false;
	bool bFinished = false;
};
