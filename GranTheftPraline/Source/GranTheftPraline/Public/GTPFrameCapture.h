#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include <atomic>
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
 * Frames are read from the window back buffer just before it is presented,
 * at its real size, so menus and HUD are included. Reading back frames costs
 * GPU time: do not combine with a frame time measurement run.
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
	/** Render thread: reads the back buffer when a frame was requested. */
	void HandleBackBufferReady(class SWindow& Window, class ISlateViewportProvider& ViewportProvider);
	void Finish();

	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle BackBufferHandle;
	FString OutputDir;
	double StartSeconds = 0.0;
	double DelaySeconds = 3.0;
	double DurationSeconds = 0.0;
	double FramesPerSecond = 10.0;
	double NextCaptureSeconds = 0.0;
	std::atomic<int32> FrameIndex{0};
	std::atomic<bool> bRequestPending{false};
	bool bFinished = false;
};
