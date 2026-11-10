#include "GTPUserSettings.h"

#include "GTPGraphicsProfile.h"
#include "GTPLocalization.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPSettings, Log, All);

FString UGTPUserSettings::GetSavePath()
{
	return FPaths::ProjectSavedDir() / TEXT("Config") / TEXT("GTPSettings.ini");
}

UGTPUserSettings* UGTPUserSettings::Find(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGTPUserSettings>() : nullptr;
}

void UGTPUserSettings::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Load();
	ApplyLanguageAndVolume();
}

void UGTPUserSettings::Load()
{
	FString Text;
	bLoadedFromFile = FFileHelper::LoadFileToString(Text, *GetSavePath());
	if (bLoadedFromFile)
	{
		const FTCHARToUTF8 Utf8(*Text);
		Settings = GTPUI::Parse(std::string_view(Utf8.Get(), static_cast<size_t>(Utf8.Length())));
		UE_LOG(LogGTPSettings, Log, TEXT("Settings loaded from %s (graphics %hs, language %hs)"), *GetSavePath(),
			Settings.Graphics.c_str(), GTPUI::LanguageCode(Settings.Language));
	}
}

void UGTPUserSettings::Save() const
{
	const FString Text(UTF8_TO_TCHAR(GTPUI::Serialize(Settings).c_str()));
	if (!FFileHelper::SaveStringToFile(Text, *GetSavePath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogGTPSettings, Warning, TEXT("Could not save settings to %s"), *GetSavePath());
	}
}

void UGTPUserSettings::ApplyLanguageAndVolume() const
{
	GTPSetLanguage(Settings.Language);
	FApp::SetVolumeMultiplier(Settings.VolumeMultiplier());
}

void UGTPUserSettings::ChangeSettings(TFunctionRef<void(GTPUI::FUserSettings&)> Change)
{
	const GTPUI::FUserSettings Before = Settings;
	Change(Settings);
	Settings.Clamp();
	if (Settings == Before)
	{
		return;
	}
	ApplyLanguageAndVolume();
	if (Settings.Graphics != Before.Graphics)
	{
		if (UGTPGraphicsProfile* Profile = GetGameInstance()->GetSubsystem<UGTPGraphicsProfile>())
		{
			Profile->SetProfile(FString(UTF8_TO_TCHAR(Settings.Graphics.c_str())));
		}
	}
	bLoadedFromFile = true;
	Save();
	OnChanged.Broadcast();
}
