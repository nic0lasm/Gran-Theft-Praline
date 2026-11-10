#include "GTPMenuWidgets.h"

#include "GTPAudioMix.h"

#include "GTPControls.h"
#include "GTPGameInstance.h"
#include "GTPLocalization.h"
#include "GTPUserSettings.h"
#include "GTPPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"

namespace GTPMenuStyle
{
	constexpr float HeadingSize = 56.f;
	constexpr float SubtitleSize = 18.f;
	constexpr float ButtonTextSize = 26.f;
	constexpr float ButtonWidth = 380.f;

	static void SetFontSize(UTextBlock* Text, float Size)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
	}
}

// ---------------------------------------------------------------------------
// UGTPMenuWidget

TSharedRef<SWidget> UGTPMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UGTPMenuWidget::BuildTree()
{
	SetIsFocusable(true);

	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, GetBackgroundOpacity()));
	Background->SetHorizontalAlignment(HAlign_Center);
	Background->SetVerticalAlignment(VAlign_Center);
	WidgetTree->RootWidget = Background;

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Background->SetContent(Column);

	const FText Heading = GetHeading();
	if (!Heading.IsEmpty())
	{
		UTextBlock* HeadingText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Heading"));
		HeadingBlock = HeadingText;
		HeadingText->SetText(Heading);
		HeadingText->SetJustification(ETextJustify::Center);
		GTPMenuStyle::SetFontSize(HeadingText, GTPMenuStyle::HeadingSize);
		UVerticalBoxSlot* HeadingSlot = Column->AddChildToVerticalBox(HeadingText);
		HeadingSlot->SetHorizontalAlignment(HAlign_Center);
		HeadingSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	const FText Subtitle = GetSubtitle();
	if (!Subtitle.IsEmpty())
	{
		UTextBlock* SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Subtitle"));
		SubtitleBlock = SubtitleText;
		SubtitleText->SetText(Subtitle);
		SubtitleText->SetJustification(ETextJustify::Center);
		SubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f)));
		GTPMenuStyle::SetFontSize(SubtitleText, GTPMenuStyle::SubtitleSize);
		UVerticalBoxSlot* SubtitleSlot = Column->AddChildToVerticalBox(SubtitleText);
		SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ButtonColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Buttons"));
	UVerticalBoxSlot* ButtonsSlot = Column->AddChildToVerticalBox(ButtonColumn);
	ButtonsSlot->SetHorizontalAlignment(HAlign_Center);
	ButtonsSlot->SetPadding(FMargin(0.f, 40.f, 0.f, 0.f));

	BuildButtons();
}

void UGTPMenuWidget::RefreshHeadings()
{
	if (HeadingBlock)
	{
		HeadingBlock->SetText(GetHeading());
	}
	if (SubtitleBlock)
	{
		SubtitleBlock->SetText(GetSubtitle());
	}
}

void UGTPMenuWidget::HandleButtonHovered()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Navigate);
}

UButton* UGTPMenuWidget::AddButton(const FText& Label, TObjectPtr<UTextBlock>* OutLabel)
{
	check(ButtonColumn);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(GTPMenuStyle::ButtonWidth);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Size->SetContent(Button);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(Label);
	Text->SetJustification(ETextJustify::Center);
	Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f)));
	GTPMenuStyle::SetFontSize(Text, GTPMenuStyle::ButtonTextSize);
	Button->SetContent(Text);
	if (OutLabel)
	{
		*OutLabel = Text;
	}

	Button->OnHovered.AddDynamic(this, &UGTPMenuWidget::HandleButtonHovered);

	UVerticalBoxSlot* ButtonSlot = ButtonColumn->AddChildToVerticalBox(Size);
	ButtonSlot->SetPadding(FMargin(0.f, 8.f));

	if (!DefaultFocusTarget)
	{
		DefaultFocusTarget = Button;
	}
	return Button;
}

AGTPPlayerController* UGTPMenuWidget::GetGTPController() const
{
	return Cast<AGTPPlayerController>(GetOwningPlayer());
}

// ---------------------------------------------------------------------------
// UGTPTitleMenuWidget

FText UGTPTitleMenuWidget::GetHeading() const
{
	return GTPLoc("menu.title.heading");
}

FText UGTPTitleMenuWidget::GetSubtitle() const
{
	const UGTPGameInstance* GameInstance = Cast<UGTPGameInstance>(GetGameInstance());
	const FString Version = GameInstance ? GameInstance->GetDisplayVersion() : FString();
	if (Version.IsEmpty())
	{
		return GTPLoc("menu.title.subtitle_no_version");
	}
	return GTPLocFormat("menu.title.subtitle", {FText::FromString(Version)});
}

void UGTPTitleMenuWidget::BuildButtons()
{
	AddButton(GTPLoc("menu.play"))->OnClicked.AddDynamic(this, &UGTPTitleMenuWidget::HandlePlay);
	AddButton(GTPLoc("menu.settings"))->OnClicked.AddDynamic(this, &UGTPTitleMenuWidget::HandleSettings);
	AddButton(GTPLoc("menu.controls"))->OnClicked.AddDynamic(this, &UGTPTitleMenuWidget::HandleControls);
	AddButton(GTPLoc("menu.quit"))->OnClicked.AddDynamic(this, &UGTPTitleMenuWidget::HandleQuit);
}

void UGTPTitleMenuWidget::HandlePlay()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Confirm);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->StartGameFromTitle();
	}
}

void UGTPTitleMenuWidget::HandleSettings()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Confirm);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->OpenSettingsMenu();
	}
}

void UGTPTitleMenuWidget::HandleControls()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Confirm);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->OpenSettingsMenu(true);
	}
}

void UGTPTitleMenuWidget::HandleQuit()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Confirm);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->QuitToDesktop();
	}
}

// ---------------------------------------------------------------------------
// UGTPPauseMenuWidget

FText UGTPPauseMenuWidget::GetHeading() const
{
	return GTPLoc("menu.pause.heading");
}

void UGTPPauseMenuWidget::BuildButtons()
{
	AddButton(GTPLoc("menu.pause.resume"))->OnClicked.AddDynamic(this, &UGTPPauseMenuWidget::HandleResume);
	AddButton(GTPLoc("menu.settings"))->OnClicked.AddDynamic(this, &UGTPPauseMenuWidget::HandleSettings);
	AddButton(GTPLoc("menu.controls"))->OnClicked.AddDynamic(this, &UGTPPauseMenuWidget::HandleControls);
	AddButton(GTPLoc("menu.pause.quit"))->OnClicked.AddDynamic(this, &UGTPPauseMenuWidget::HandleQuit);
}

FReply UGTPPauseMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_FaceButton_Right)
	{
		HandleResume();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UGTPPauseMenuWidget::HandleResume()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Back);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->ClosePauseMenu();
	}
}

void UGTPPauseMenuWidget::HandleSettings()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Confirm);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->OpenSettingsMenu();
	}
}

void UGTPPauseMenuWidget::HandleControls()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Confirm);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->OpenSettingsMenu(true);
	}
}

void UGTPPauseMenuWidget::HandleQuit()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Confirm);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->QuitToDesktop();
	}
}

// ---------------------------------------------------------------------------
// UGTPControlsMenuWidget

FText UGTPControlsMenuWidget::GetHeading() const
{
	return GTPLoc("controls.heading");
}

FText UGTPControlsMenuWidget::GetSubtitle() const
{
	const AGTPPlayerController* Controller = GetGTPController();
	const bool bQwerty = Controller && Controller->IsUsingQwerty();
	FString Keyboard = GTPLocFormat("controls.keyboard_header", {FText::FromString(bQwerty ? TEXT("QWERTY") : TEXT("AZERTY"))}).ToString() + TEXT("\n");
	FString Gamepad = GTPLoc("controls.gamepad_header").ToString() + TEXT("\n");
	for (size_t i = 0; i < GTPUI::ControlRowCount; ++i)
	{
		const GTPUI::FControlRow& Row = GTPUI::ControlRows[i];
		const FString Action = GTPLocString(Row.ActionKey);
		const FString KeyboardKeys = FString(UTF8_TO_TCHAR(bQwerty ? Row.Qwerty : Row.Azerty));
		const FString GamepadKeys = FString(UTF8_TO_TCHAR(Row.Gamepad));
		if (KeyboardKeys != TEXT("-"))
		{
			Keyboard += FString::Printf(TEXT("%s : %s\n"), *Action, *KeyboardKeys);
		}
		if (GamepadKeys != TEXT("-"))
		{
			Gamepad += FString::Printf(TEXT("%s : %s\n"), *Action, *GamepadKeys);
		}
	}
	return FText::FromString(Keyboard + TEXT("\n") + Gamepad);
}

void UGTPControlsMenuWidget::BuildButtons()
{
	AddButton(GTPLoc("menu.back"))->OnClicked.AddDynamic(this, &UGTPControlsMenuWidget::HandleBack);
}

FReply UGTPControlsMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_FaceButton_Right)
	{
		HandleBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UGTPControlsMenuWidget::HandleBack()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Back);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->CloseSettingsMenu();
	}
}

// ---------------------------------------------------------------------------
// UGTPSettingsMenuWidget

namespace
{
	FText OnOff(bool bOn)
	{
		return GTPLoc(bOn ? "value.on" : "value.off");
	}

	FText GraphicsName(const std::string& Profile)
	{
		if (Profile == "fidelity")
		{
			return GTPLoc("value.gfx.fidelity");
		}
		return GTPLoc(Profile == "ultra" ? "value.gfx.ultra" : "value.gfx.m1");
	}

	FText KeyboardName(GTPUI::EKeyboardLayout Layout)
	{
		switch (Layout)
		{
		case GTPUI::EKeyboardLayout::Azerty: return GTPLoc("value.kb.azerty");
		case GTPUI::EKeyboardLayout::Qwerty: return GTPLoc("value.kb.qwerty");
		default: return GTPLoc("value.kb.auto");
		}
	}

	FText ZoomName(GTPUI::EMinimapZoom Zoom)
	{
		switch (Zoom)
		{
		case GTPUI::EMinimapZoom::Near: return GTPLoc("value.zoom.near");
		case GTPUI::EMinimapZoom::Far: return GTPLoc("value.zoom.far");
		default: return GTPLoc("value.zoom.medium");
		}
	}

	/** Applies one change to the saved settings (saved and applied by the subsystem). */
	void ChangeSetting(const UObject* Context, TFunctionRef<void(GTPUI::FUserSettings&)> Change)
	{
		if (UGTPUserSettings* Settings = UGTPUserSettings::Find(Context))
		{
			Settings->ChangeSettings(Change);
		}
	}
}

FText UGTPSettingsMenuWidget::GetHeading() const
{
	return GTPLoc("settings.heading");
}

FText UGTPSettingsMenuWidget::GetSubtitle() const
{
	return GTPLoc("settings.hint");
}

void UGTPSettingsMenuWidget::BuildButtons()
{
	AddButton(FText::GetEmpty(), &LanguageLabel)->OnClicked.AddDynamic(this, &UGTPSettingsMenuWidget::HandleLanguage);
	AddButton(FText::GetEmpty(), &GraphicsLabel)->OnClicked.AddDynamic(this, &UGTPSettingsMenuWidget::HandleGraphics);
	AddButton(FText::GetEmpty(), &VolumeLabel)->OnClicked.AddDynamic(this, &UGTPSettingsMenuWidget::HandleVolume);
	AddButton(FText::GetEmpty(), &SensitivityLabel)->OnClicked.AddDynamic(this, &UGTPSettingsMenuWidget::HandleSensitivity);
	AddButton(FText::GetEmpty(), &InvertYLabel)->OnClicked.AddDynamic(this, &UGTPSettingsMenuWidget::HandleInvertY);
	AddButton(FText::GetEmpty(), &KeyboardLabel)->OnClicked.AddDynamic(this, &UGTPSettingsMenuWidget::HandleKeyboard);
	AddButton(FText::GetEmpty(), &MinimapLabel)->OnClicked.AddDynamic(this, &UGTPSettingsMenuWidget::HandleMinimap);
	AddButton(FText::GetEmpty(), &MinimapZoomLabel)->OnClicked.AddDynamic(this, &UGTPSettingsMenuWidget::HandleMinimapZoom);
	AddButton(GTPLoc("menu.back"))->OnClicked.AddDynamic(this, &UGTPSettingsMenuWidget::HandleBack);
	RefreshRows();
}

void UGTPSettingsMenuWidget::RefreshRows()
{
	const UGTPUserSettings* Settings = UGTPUserSettings::Find(this);
	const GTPUI::FUserSettings Values = Settings ? Settings->Get() : GTPUI::FUserSettings();
	const FText Language = GTPLoc(Values.Language == GTPUI::ELanguage::English ? "value.lang.en" : "value.lang.fr");
	if (LanguageLabel) LanguageLabel->SetText(GTPLocFormat("settings.language", {Language}));
	if (GraphicsLabel) GraphicsLabel->SetText(GTPLocFormat("settings.graphics", {GraphicsName(Values.Graphics)}));
	if (VolumeLabel) VolumeLabel->SetText(GTPLocFormat("settings.volume", {FText::AsNumber(Values.VolumePercent)}));
	if (SensitivityLabel) SensitivityLabel->SetText(GTPLocFormat("settings.sensitivity", {FText::AsNumber(Values.SensitivityPercent)}));
	if (InvertYLabel) InvertYLabel->SetText(GTPLocFormat("settings.invert_y", {OnOff(Values.bInvertY)}));
	if (KeyboardLabel) KeyboardLabel->SetText(GTPLocFormat("settings.keyboard", {KeyboardName(Values.Keyboard)}));
	if (MinimapLabel) MinimapLabel->SetText(GTPLocFormat("settings.minimap", {OnOff(Values.bMinimap)}));
	if (MinimapZoomLabel) MinimapZoomLabel->SetText(GTPLocFormat("settings.minimap_zoom", {ZoomName(Values.MinimapZoom)}));
	RefreshHeadings();
}

void UGTPSettingsMenuWidget::HandleLanguage()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Toggle);
	ChangeSetting(this, [](GTPUI::FUserSettings& S) { S.CycleLanguage(); });
	RefreshRows();
}

void UGTPSettingsMenuWidget::HandleGraphics()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Toggle);
	ChangeSetting(this, [](GTPUI::FUserSettings& S) { S.CycleGraphics(); });
	RefreshRows();
}

void UGTPSettingsMenuWidget::HandleVolume()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Toggle);
	ChangeSetting(this, [](GTPUI::FUserSettings& S) { S.CycleVolume(); });
	RefreshRows();
}

void UGTPSettingsMenuWidget::HandleSensitivity()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Toggle);
	ChangeSetting(this, [](GTPUI::FUserSettings& S) { S.CycleSensitivity(); });
	RefreshRows();
}

void UGTPSettingsMenuWidget::HandleInvertY()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Toggle);
	ChangeSetting(this, [](GTPUI::FUserSettings& S) { S.bInvertY = !S.bInvertY; });
	RefreshRows();
}

void UGTPSettingsMenuWidget::HandleKeyboard()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Toggle);
	ChangeSetting(this, [](GTPUI::FUserSettings& S) { S.CycleKeyboard(); });
	RefreshRows();
}

void UGTPSettingsMenuWidget::HandleMinimap()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Toggle);
	ChangeSetting(this, [](GTPUI::FUserSettings& S) { S.bMinimap = !S.bMinimap; });
	RefreshRows();
}

void UGTPSettingsMenuWidget::HandleMinimapZoom()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Toggle);
	ChangeSetting(this, [](GTPUI::FUserSettings& S) { S.CycleMinimapZoom(); });
	RefreshRows();
}

FReply UGTPSettingsMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right || Key == EKeys::Gamepad_FaceButton_Right)
	{
		HandleBack();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UGTPSettingsMenuWidget::HandleBack()
{
	UGTPAudioMix::PlayUI(this, EGTPUISound::Back);
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->CloseSettingsMenu();
	}
}
