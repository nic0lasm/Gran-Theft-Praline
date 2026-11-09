#include "GTPFrameCapture.h"

#include "Async/Async.h"
#include "Engine/GameViewportClient.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPFrameCapture, Log, All);

namespace
{
	constexpr int32 JpegQuality = 85;
}

void UGTPFrameCapture::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const TCHAR* CommandLine = FCommandLine::Get();
	if (!FParse::Value(CommandLine, TEXT("CaptureVideo="), DurationSeconds) || DurationSeconds <= 0.0)
	{
		return;
	}
	FParse::Value(CommandLine, TEXT("CaptureFps="), FramesPerSecond);
	FParse::Value(CommandLine, TEXT("CaptureDelay="), DelaySeconds);
	FramesPerSecond = FMath::Clamp(FramesPerSecond, 1.0, 60.0);

	if (!FParse::Value(CommandLine, TEXT("CaptureDir="), OutputDir) || OutputDir.IsEmpty())
	{
		OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Capture"),
			FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	}
	OutputDir = FPaths::ConvertRelativePathToFull(OutputDir);
	IFileManager::Get().MakeDirectory(*OutputDir, true);

	// Load on the game thread; compression runs on worker threads.
	FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	ScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UGTPFrameCapture::HandleScreenshot);
	StartSeconds = FPlatformTime::Seconds();
	NextCaptureSeconds = DelaySeconds;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPFrameCapture::Tick));

	UE_LOG(LogGTPFrameCapture, Log, TEXT("Capturing %.0f s at %.0f fps to %s"), DurationSeconds, FramesPerSecond, *OutputDir);
}

void UGTPFrameCapture::Deinitialize()
{
	Finish();
	Super::Deinitialize();
}

bool UGTPFrameCapture::Tick(float DeltaTime)
{
	const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
	if (Elapsed >= DelaySeconds + DurationSeconds)
	{
		Finish();
		return false;
	}
	if (!bRequestPending && Elapsed >= NextCaptureSeconds)
	{
		bRequestPending = true;
		NextCaptureSeconds += 1.0 / FramesPerSecond;
		if (NextCaptureSeconds < Elapsed)
		{
			// Slow frames: keep the schedule instead of bursting to catch up.
			NextCaptureSeconds = Elapsed + 1.0 / FramesPerSecond;
		}
		// Capture the whole window through Slate: it includes menus and HUD, and the
		// scene-only readback returns black frames on Metal when the backbuffer size
		// differs from the viewport size.
		FScreenshotRequest::RequestScreenshot(true, false);
	}
	return true;
}

void UGTPFrameCapture::HandleScreenshot(int32 Width, int32 Height, const TArray<FColor>& Colors)
{
	bRequestPending = false;
	if (bFinished || Colors.Num() != Width * Height)
	{
		return;
	}

	const FString Path = FPaths::Combine(OutputDir, FString::Printf(TEXT("frame_%05d.jpg"), FrameIndex++));
	TArray<FColor> Pixels = Colors;
	Async(EAsyncExecution::ThreadPool, [Pixels = MoveTemp(Pixels), Width, Height, Path]()
	{
		IImageWrapperModule& Module = FModuleManager::GetModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Wrapper = Module.CreateImageWrapper(EImageFormat::JPEG);
		if (Wrapper.IsValid() && Wrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			const TArray64<uint8> Compressed = Wrapper->GetCompressed(JpegQuality);
			FFileHelper::SaveArrayToFile(Compressed, *Path);
		}
	});
}

void UGTPFrameCapture::Finish()
{
	if (bFinished || OutputDir.IsEmpty())
	{
		return;
	}
	bFinished = true;

	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	if (ScreenshotHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
		ScreenshotHandle.Reset();
	}

	const double Captured = FMath::Max(0.0, FPlatformTime::Seconds() - StartSeconds - DelaySeconds);
	const double EffectiveFps = Captured > 0.0 ? FrameIndex / FMath::Min(Captured, DurationSeconds) : 0.0;
	const FString Info = FString::Printf(TEXT("frames=%d\nrequested_fps=%.2f\neffective_fps=%.2f\n"),
		FrameIndex, FramesPerSecond, EffectiveFps);
	FFileHelper::SaveStringToFile(Info, *FPaths::Combine(OutputDir, TEXT("capture.txt")));
	UE_LOG(LogGTPFrameCapture, Log, TEXT("Capture finished: %d frames, %.2f fps effective"), FrameIndex, EffectiveFps);
}
