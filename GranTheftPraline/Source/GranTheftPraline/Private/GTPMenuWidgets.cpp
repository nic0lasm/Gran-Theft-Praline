#include "GTPMenuWidgets.h"

#include "GTPGameInstance.h"
#include "GTPPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "GTPMenus"

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

UButton* UGTPMenuWidget::AddButton(const FText& Label)
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
	return LOCTEXT("TitleHeading", "GRAN THEFT PRALINE");
}

FText UGTPTitleMenuWidget::GetSubtitle() const
{
	const UGTPGameInstance* GameInstance = Cast<UGTPGameInstance>(GetGameInstance());
	const FString Version = GameInstance ? GameInstance->GetDisplayVersion() : FString();
	if (Version.IsEmpty())
	{
		return LOCTEXT("TitleSubtitleNoVersion", "Lyon, Presqu'île");
	}
	return FText::Format(LOCTEXT("TitleSubtitle", "Lyon, Presqu'île · version {0}"), FText::FromString(Version));
}

void UGTPTitleMenuWidget::BuildButtons()
{
	AddButton(LOCTEXT("Play", "Jouer"))->OnClicked.AddDynamic(this, &UGTPTitleMenuWidget::HandlePlay);
	AddButton(LOCTEXT("Quit", "Quitter"))->OnClicked.AddDynamic(this, &UGTPTitleMenuWidget::HandleQuit);
}

void UGTPTitleMenuWidget::HandlePlay()
{
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->StartGameFromTitle();
	}
}

void UGTPTitleMenuWidget::HandleQuit()
{
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->QuitToDesktop();
	}
}

// ---------------------------------------------------------------------------
// UGTPPauseMenuWidget

FText UGTPPauseMenuWidget::GetHeading() const
{
	return LOCTEXT("PauseHeading", "Pause");
}

void UGTPPauseMenuWidget::BuildButtons()
{
	AddButton(LOCTEXT("Resume", "Reprendre"))->OnClicked.AddDynamic(this, &UGTPPauseMenuWidget::HandleResume);
	AddButton(LOCTEXT("QuitToDesktop", "Quitter le jeu"))->OnClicked.AddDynamic(this, &UGTPPauseMenuWidget::HandleQuit);
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
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->ClosePauseMenu();
	}
}

void UGTPPauseMenuWidget::HandleQuit()
{
	if (AGTPPlayerController* Controller = GetGTPController())
	{
		Controller->QuitToDesktop();
	}
}

#undef LOCTEXT_NAMESPACE
