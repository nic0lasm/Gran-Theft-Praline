#include "GTPTestExport.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GeneralProjectSettings.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RHI.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPExport, Log, All);

void UGTPTestExport::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	int32 KeyOn = 1;
	FParse::Value(FCommandLine::Get(), TEXT("ExportKey="), KeyOn);
	FParse::Value(FCommandLine::Get(), TEXT("ExportSeconds="), WindowSeconds);
	if (KeyOn == 0 || FParse::Param(FCommandLine::Get(), TEXT("NullRHI")))
	{
		return;
	}
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPTestExport::Tick));
}

void UGTPTestExport::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	Super::Deinitialize();
}

UGTPTestExport::FFrameStats UGTPTestExport::ComputeStats(const TArray<float>& Values, float LimitMs)
{
	FFrameStats Stats;
	Stats.Count = Values.Num();
	if (Values.Num() == 0)
	{
		return Stats;
	}
	TArray<float> Sorted = Values;
	Sorted.Sort();
	auto Percentile = [&Sorted](double Fraction)
	{
		const int32 Index = FMath::Clamp(FMath::CeilToInt(Fraction * Sorted.Num()) - 1, 0, Sorted.Num() - 1);
		return Sorted[Index];
	};
	Stats.P50 = Percentile(0.50);
	Stats.P95 = Percentile(0.95);
	Stats.Worst = Sorted.Last();
	for (float Value : Sorted)
	{
		Stats.OverLimit += Value > LimitMs ? 1 : 0;
	}
	return Stats;
}

bool UGTPTestExport::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	FrameMs.Add(DeltaTime * 1000.f);
	FrameTimes.Add(Now);
	// Drop what is older than the window (removing from the front stays cheap: ~2400 entries).
	int32 Stale = 0;
	while (Stale < FrameTimes.Num() && Now - FrameTimes[Stale] > WindowSeconds)
	{
		++Stale;
	}
	if (Stale > 0)
	{
		FrameMs.RemoveAt(0, Stale, EAllowShrinking::No);
		FrameTimes.RemoveAt(0, Stale, EAllowShrinking::No);
	}

	bool bDown = false;
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			bDown = Controller->IsInputKeyDown(EKeys::F9);
		}
	}
	if (bDown && !bKeyDownLastTick)
	{
		const FString Path = ExportNow();
#if !UE_BUILD_SHIPPING
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.f, Path.IsEmpty() ? FColor::Red : FColor::Green,
				Path.IsEmpty() ? TEXT("Export failed") : FString::Printf(TEXT("Exported to %s"), *Path));
		}
#endif
	}
	bKeyDownLastTick = bDown;
	return true;
}

FString UGTPTestExport::ExportNow()
{
	const FString Version = GetDefault<UGeneralProjectSettings>()->ProjectVersion;
	const FFrameStats Stats = ComputeStats(FrameMs, 100.f);

	FString Location = TEXT("n/a");
	FString MapName = TEXT("n/a");
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		MapName = World->GetMapName();
		if (APlayerController* Controller = World->GetFirstPlayerController())
		{
			const FVector Position = Controller->GetPawn() ? Controller->GetPawn()->GetActorLocation() : FVector::ZeroVector;
			const FRotator Rotation = Controller->GetControlRotation();
			Location = FString::Printf(TEXT("x=%.0f y=%.0f z=%.0f yaw=%.0f (cm)"), Position.X, Position.Y, Position.Z, Rotation.Yaw);
		}
	}

	FString Text;
	Text += FString::Printf(TEXT("Gran Theft Praline test export\n"));
	Text += FString::Printf(TEXT("version=%s\n"), *Version);
	Text += FString::Printf(TEXT("time=%s\n"), *FDateTime::Now().ToIso8601());
	Text += FString::Printf(TEXT("os=%s\n"), *FPlatformMisc::GetOSVersion());
	Text += FString::Printf(TEXT("gpu=%s\n"), *FPlatformMisc::GetPrimaryGPUBrand());
	Text += FString::Printf(TEXT("cpu=%s (%d cores)\n"), *FPlatformMisc::GetCPUBrand(), FPlatformMisc::NumberOfCores());
	Text += FString::Printf(TEXT("map=%s\n"), *MapName);
	Text += FString::Printf(TEXT("player=%s\n"), *Location);
	Text += FString::Printf(TEXT("uptime_s=%.0f\n"), FPlatformTime::Seconds() - GStartTime);
	Text += FString::Printf(TEXT("command_line=%s\n"), FCommandLine::Get());
	Text += FString::Printf(TEXT("frames_last_%.0fs=%d p50_ms=%.1f p95_ms=%.1f worst_ms=%.1f over_100ms=%d\n"),
		WindowSeconds, Stats.Count, Stats.P50, Stats.P95, Stats.Worst, Stats.OverLimit);

	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Exports"),
		FString::Printf(TEXT("export-%s.txt"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")))));
	if (!FFileHelper::SaveStringToFile(Text, *Path))
	{
		UE_LOG(LogGTPExport, Warning, TEXT("Export failed: cannot write %s"), *Path);
		return FString();
	}
	UE_LOG(LogGTPExport, Log, TEXT("Exported to %s"), *Path);
	return Path;
}
