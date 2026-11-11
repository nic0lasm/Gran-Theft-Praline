#include "GTPFrameCapture.h"

#include "Async/Async.h"
#include "Framework/Application/SlateApplication.h"
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
#include "RHICommandList.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "Slate/SlateViewportProvider.h"

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
	// Below 1 fps is allowed: -CaptureFps=0.1 saves one still every 10 s (a 1 fps floor made
	// the demo-route stills cost a >100 ms frame every second, 11/11).
	FramesPerSecond = FMath::Clamp(FramesPerSecond, 0.01, 60.0);

	if (!FParse::Value(CommandLine, TEXT("CaptureDir="), OutputDir) || OutputDir.IsEmpty())
	{
		OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Capture"),
			FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	}
	OutputDir = FPaths::ConvertRelativePathToFull(OutputDir);
	IFileManager::Get().MakeDirectory(*OutputDir, true);

	// Load on the game thread; compression runs on worker threads.
	FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
	{
		UE_LOG(LogGTPFrameCapture, Warning, TEXT("No Slate renderer: capture disabled"));
		return;
	}
	// Removed in Finish(), which flushes the render thread before this object goes away.
	BackBufferHandle = FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent()
		.AddLambda([this](SWindow& Window, ISlateViewportProvider& ViewportProvider)
		{
			HandleBackBufferReady(Window, ViewportProvider);
		});
	StartSeconds = FPlatformTime::Seconds();
	NextCaptureSeconds = DelaySeconds;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPFrameCapture::Tick));

	UE_LOG(LogGTPFrameCapture, Log, TEXT("Capturing %.0f s at %.2f fps to %s"), DurationSeconds, FramesPerSecond, *OutputDir);
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
		NextCaptureSeconds += 1.0 / FramesPerSecond;
		if (NextCaptureSeconds < Elapsed)
		{
			// Slow frames: keep the schedule instead of bursting to catch up.
			NextCaptureSeconds = Elapsed + 1.0 / FramesPerSecond;
		}
		bRequestPending = true;
	}
	return true;
}

void UGTPFrameCapture::HandleBackBufferReady(SWindow& Window, ISlateViewportProvider& ViewportProvider)
{
	if (bFinished || !bRequestPending.exchange(false))
	{
		return;
	}
	FRHITexture* BackBuffer = ViewportProvider.GetBackBufferResource();
	if (!BackBuffer)
	{
		return;
	}

	// Read the whole texture at its real size: the viewport size can differ from
	// the back buffer size on Metal, and reading past it returns black frames.
	const FIntPoint Size = BackBuffer->GetSizeXY();
	TArray<FColor> Pixels;
	FRHICommandListImmediate::Get().ReadSurfaceData(BackBuffer, FIntRect(0, 0, Size.X, Size.Y), Pixels, FReadSurfaceDataFlags());
	if (Pixels.Num() != Size.X * Size.Y)
	{
		return;
	}
	for (FColor& Pixel : Pixels)
	{
		Pixel.A = 255;
	}

	const FString Path = FPaths::Combine(OutputDir, FString::Printf(TEXT("frame_%05d.jpg"), FrameIndex++));
	const int32 Width = Size.X;
	const int32 Height = Size.Y;
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
	if (BackBufferHandle.IsValid())
	{
		if (FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
		{
			FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Remove(BackBufferHandle);
		}
		BackBufferHandle.Reset();
		FlushRenderingCommands();
	}

	const double Captured = FMath::Max(0.0, FPlatformTime::Seconds() - StartSeconds - DelaySeconds);
	const int32 Frames = FrameIndex.load();
	const double EffectiveFps = Captured > 0.0 ? Frames / FMath::Min(Captured, DurationSeconds) : 0.0;
	const FString Info = FString::Printf(TEXT("frames=%d\nrequested_fps=%.2f\neffective_fps=%.2f\n"),
		Frames, FramesPerSecond, EffectiveFps);
	FFileHelper::SaveStringToFile(Info, *FPaths::Combine(OutputDir, TEXT("capture.txt")));
	UE_LOG(LogGTPFrameCapture, Log, TEXT("Capture finished: %d frames, %.2f fps effective"), Frames, EffectiveFps);
}
