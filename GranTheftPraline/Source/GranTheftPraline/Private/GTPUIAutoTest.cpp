#include "GTPUIAutoTest.h"

#include "GTPPlayerController.h"
#include "GTPSettingsCore.h"
#include "GTPUserSettings.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace
{
	GTPUI::FUserSettings NonDefaultSettings()
	{
		GTPUI::FUserSettings S;
		S.Language = GTPUI::ELanguage::English;
		S.Graphics = "fidelity";
		S.VolumePercent = 40;
		S.SensitivityPercent = 150;
		S.bInvertY = true;
		S.Keyboard = GTPUI::EKeyboardLayout::Qwerty;
		S.bMinimap = true;
		S.MinimapZoom = GTPUI::EMinimapZoom::Far;
		return S;
	}

	FString Describe(const GTPUI::FUserSettings& S)
	{
		return FString(UTF8_TO_TCHAR(GTPUI::Serialize(S).c_str())).Replace(TEXT("\n"), TEXT(" "));
	}
}

bool UGTPUIAutoTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	FString Value;
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && FParse::Value(FCommandLine::Get(), TEXT("UITest="), Value)
		&& Super::ShouldCreateSubsystem(Outer);
}

void UGTPUIAutoTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FParse::Value(FCommandLine::Get(), TEXT("UITest="), Step);
	Step = Step.ToLower();
	Report(FString::Printf(TEXT("UITEST %s start"), *Step));
	// Let the player controller finish its start (title skipped) before opening menus.
	InWorld.GetTimerManager().SetTimer(Timer, [this]()
	{
		if (Step == TEXT("write")) { RunMenus(); }
		else if (Step == TEXT("verify")) { RunVerify(); }
		else if (Step == TEXT("reset")) { RunReset(); }
		else { Finish(false, TEXT("unknown step")); }
	}, 3.f, false);
}

void UGTPUIAutoTestSubsystem::Report(const FString& Line) const
{
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("ui_test.txt");
	FFileHelper::SaveStringToFile(Line + LINE_TERMINATOR, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_Append);
}

void UGTPUIAutoTestSubsystem::Finish(bool bPass, const FString& Detail) const
{
	Report(FString::Printf(TEXT("UITEST %s %s %s"), *Step, bPass ? TEXT("PASS") : TEXT("FAIL"), *Detail));
}

void UGTPUIAutoTestSubsystem::RunMenus()
{
	AGTPPlayerController* Controller = Cast<AGTPPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	if (!Controller)
	{
		Finish(false, TEXT("no player controller"));
		return;
	}
	// Pause menu -> settings -> back -> resume: the screens build and close without a crash.
	Controller->OpenPauseMenu();
	Controller->OpenSettingsMenu();
	const bool bSettingsOpen = Controller->IsMenuOpen();
	Controller->CloseSettingsMenu();
	Controller->ClosePauseMenu();
	Report(FString::Printf(TEXT("UITEST menus open=%d closed=%d"), bSettingsOpen ? 1 : 0, Controller->IsMenuOpen() ? 0 : 1));
	RunWrite();
}

void UGTPUIAutoTestSubsystem::RunWrite()
{
	UGTPUserSettings* User = UGTPUserSettings::Find(this);
	if (!User)
	{
		Finish(false, TEXT("no settings subsystem"));
		return;
	}
	const GTPUI::FUserSettings Wanted = NonDefaultSettings();
	User->Modify([&Wanted](GTPUI::FUserSettings& S) { S = Wanted; });
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *UGTPUserSettings::GetSavePath()))
	{
		Finish(false, TEXT("settings file not written"));
		return;
	}
	const FTCHARToUTF8 Utf8(*Text);
	const GTPUI::FUserSettings Read = GTPUI::Parse(std::string_view(Utf8.Get(), static_cast<size_t>(Utf8.Length())));
	Finish(Read == Wanted && User->Get() == Wanted, Describe(Read));
}

void UGTPUIAutoTestSubsystem::RunVerify()
{
	const UGTPUserSettings* User = UGTPUserSettings::Find(this);
	const bool bSame = User && User->HasSavedFile() && User->Get() == NonDefaultSettings();
	Finish(bSame, User ? Describe(User->Get()) : FString(TEXT("no settings subsystem")));
}

void UGTPUIAutoTestSubsystem::RunReset()
{
	UGTPUserSettings* User = UGTPUserSettings::Find(this);
	if (!User)
	{
		Finish(false, TEXT("no settings subsystem"));
		return;
	}
	User->Modify([](GTPUI::FUserSettings& S) { S = GTPUI::FUserSettings(); });
	Finish(User->Get() == GTPUI::FUserSettings(), Describe(User->Get()));
}
