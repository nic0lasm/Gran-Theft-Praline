#include "GTPGraphicsProfile.h"

#include "GTPUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPGraphics, Log, All);

void UGTPGraphicsProfile::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FString Requested;
	if (!FParse::Value(FCommandLine::Get(), TEXT("GfxProfile="), Requested))
	{
		Requested = DefaultProfile;
		// The player's saved choice (settings screen) replaces the project default.
		const UGTPUserSettings* User = Collection.InitializeDependency<UGTPUserSettings>();
		if (User && User->HasSavedFile())
		{
			Requested = FString(UTF8_TO_TCHAR(User->Get().Graphics.c_str()));
		}
	}
	ApplyProfile(Requested);
}

void UGTPGraphicsProfile::SetProfile(const FString& Name)
{
	// Back to the values found at start, then the new profile on top (ultra = nothing on top).
	if (ResizeHandle.IsValid())
	{
		FViewport::ViewportResizedEvent.Remove(ResizeHandle);
		ResizeHandle.Reset();
	}
	for (const TPair<FString, FString>& Original : OriginalValues)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Original.Key))
		{
			CVar->Set(*Original.Value, ECVF_SetByCode);
		}
	}
	OriginalValues.Reset();
	AppliedScreenPercentage = 0.f;
	ApplyProfile(Name);
}

void UGTPGraphicsProfile::ApplyProfile(FString Requested)
{
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
		UpdateScreenPercentage(ViewportSize.Y > 0 ? ViewportSize.Y : static_cast<int32>(M1ReferenceHeight));
	}
	else if (Requested != TEXT("ultra"))
	{
		UE_LOG(LogGTPGraphics, Warning, TEXT("Unknown graphics profile '%s', using ultra (project settings)"), *Requested);
		Requested = TEXT("ultra");
	}

	// -GfxCVars=a=1,b=2 applies extra cvars on top of the profile. Shipping
	// builds ignore -ExecCmds and -DPCVars, so this is how perf sweeps A/B a
	// single setting on a packaged build.
	FString Extra;
	if (FParse::Value(FCommandLine::Get(), TEXT("GfxCVars="), Extra, false))
	{
		TArray<FString> Pairs;
		Extra.ParseIntoArray(Pairs, TEXT(","), true);
		ApplyCVars(Pairs);
		UE_LOG(LogGTPGraphics, Log, TEXT("Extra cvars from the command line: %s"), *Extra);
	}

	ActiveProfile = Requested;
	UE_LOG(LogGTPGraphics, Log, TEXT("Graphics profile: %s"), *ActiveProfile);
}

void UGTPGraphicsProfile::SetCVar(const FString& Name, const FString& Value)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CVar)
	{
		UE_LOG(LogGTPGraphics, Warning, TEXT("Unknown console variable %s, skipped"), *Name);
		return;
	}
	// Remember the value found before the first change, to come back to it on a profile switch.
	if (!OriginalValues.Contains(Name))
	{
		OriginalValues.Add(Name, CVar->GetString());
	}
	CVar->Set(*Value, ECVF_SetByCode);
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
