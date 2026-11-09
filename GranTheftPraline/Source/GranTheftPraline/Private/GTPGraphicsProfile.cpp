#include "GTPGraphicsProfile.h"

#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPGraphics, Log, All);

namespace
{
	void SetCVar(const FString& Name, const FString& Value)
	{
		IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
		if (!CVar)
		{
			UE_LOG(LogGTPGraphics, Warning, TEXT("Unknown console variable %s, skipped"), *Name);
			return;
		}
		CVar->Set(*Value, ECVF_SetByCode);
	}
}

void UGTPGraphicsProfile::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FString Requested;
	if (!FParse::Value(FCommandLine::Get(), TEXT("GfxProfile="), Requested))
	{
		Requested = DefaultProfile;
	}
	Requested = Requested.ToLower();

	if (Requested == TEXT("fidelity"))
	{
		ApplyCVars(FidelityCVars);
	}
	else if (Requested == TEXT("m1"))
	{
		ApplyCVars(FidelityCVars);
		ApplyCVars(M1CVars);
		ResizeHandle = FViewport::ViewportResizedEvent.AddUObject(this, &UGTPGraphicsProfile::HandleViewportResized);
		UpdateScreenPercentage(static_cast<int32>(M1ReferenceHeight));
	}
	else if (Requested != TEXT("ultra"))
	{
		UE_LOG(LogGTPGraphics, Warning, TEXT("Unknown graphics profile '%s', using ultra (project settings)"), *Requested);
		Requested = TEXT("ultra");
	}

	ActiveProfile = Requested;
	UE_LOG(LogGTPGraphics, Log, TEXT("Graphics profile: %s"), *ActiveProfile);
}

void UGTPGraphicsProfile::Deinitialize()
{
	if (ResizeHandle.IsValid())
	{
		FViewport::ViewportResizedEvent.Remove(ResizeHandle);
		ResizeHandle.Reset();
	}
	Super::Deinitialize();
}

void UGTPGraphicsProfile::ApplyCVars(const TArray<FString>& Pairs)
{
	for (const FString& Pair : Pairs)
	{
		FString Name, Value;
		if (!Pair.Split(TEXT("="), &Name, &Value))
		{
			UE_LOG(LogGTPGraphics, Warning, TEXT("Malformed entry '%s' (expected cvar=value)"), *Pair);
			continue;
		}
		Name.TrimStartAndEndInline();
		Value.TrimStartAndEndInline();
		if (Name == TEXT("r.ScreenPercentage"))
		{
			AppliedScreenPercentage = FCString::Atof(*Value);
		}
		SetCVar(Name, Value);
	}
}

void UGTPGraphicsProfile::HandleViewportResized(FViewport* Viewport, uint32 /*Unused*/)
{
	// Only the game viewport matters; editor viewports do not exist in a packaged build.
	if (Viewport)
	{
		const FIntPoint Size = Viewport->GetSizeXY();
		if (Size.Y > 0 && Size != ViewportSize)
		{
			ViewportSize = Size;
			UpdateScreenPercentage(Size.Y);
		}
	}
}

void UGTPGraphicsProfile::UpdateScreenPercentage(int32 ViewportHeight)
{
	if (ViewportHeight <= 0)
	{
		return;
	}
	// Keep the internal resolution of the measured setup (50 % of 1080p) on any
	// display: a Retina panel at full size would otherwise render 3x the pixels.
	const float InternalHeight = M1ReferenceHeight * M1ScreenPercentage / 100.f;
	const float Percentage = FMath::Clamp(100.f * InternalHeight / ViewportHeight, 25.f, 100.f);
	if (!FMath::IsNearlyEqual(Percentage, AppliedScreenPercentage, 0.5f))
	{
		AppliedScreenPercentage = Percentage;
		SetCVar(TEXT("r.ScreenPercentage"), FString::SanitizeFloat(Percentage));
		UE_LOG(LogGTPGraphics, Log, TEXT("m1: viewport height %d px, screen percentage %.1f %%"), ViewportHeight, Percentage);
	}
}
