#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GTPSettingsCore.h"
#include "GTPMenuWidgets.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class AGTPPlayerController;

/**
 * Full-screen menu built in code (no widget asset needed): a dimmed
 * background, a heading, an optional subtitle and a column of buttons.
 * Works with mouse, keyboard and gamepad (buttons take focus).
 */
UCLASS(Abstract)
class GRANTHEFTPRALINE_API UGTPMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Widget that receives keyboard / gamepad focus when the menu opens. */
	UWidget* GetDefaultFocusTarget() const { return DefaultFocusTarget; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Subclasses add their buttons here. */
	virtual void BuildButtons() {}

	virtual FText GetHeading() const { return FText::GetEmpty(); }
	virtual FText GetSubtitle() const { return FText::GetEmpty(); }
	virtual float GetBackgroundOpacity() const { return 0.55f; }

	/** Adds a button; OutLabel receives its text block when the label changes later. */
	UButton* AddButton(const FText& Label, TObjectPtr<UTextBlock>* OutLabel = nullptr);
	AGTPPlayerController* GetGTPController() const;

	/** Re-reads GetHeading() and GetSubtitle() (language change). */
	void RefreshHeadings();

	/** Focus sound when the pointer moves onto a button (ADR-0009). */
	UFUNCTION()
	void HandleButtonHovered();

private:
	void BuildTree();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ButtonColumn;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadingBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SubtitleBlock;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> DefaultFocusTarget;
};

/** Title screen: Play, Settings, Quit. */
UCLASS()
class GRANTHEFTPRALINE_API UGTPTitleMenuWidget : public UGTPMenuWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildButtons() override;
	virtual FText GetHeading() const override;
	virtual FText GetSubtitle() const override;
	virtual float GetBackgroundOpacity() const override { return 0.7f; }

private:
	UFUNCTION()
	void HandlePlay();

	UFUNCTION()
	void HandleSettings();

	UFUNCTION()
	void HandleControls();

	UFUNCTION()
	void HandleCredits();

	UFUNCTION()
	void HandleQuit();
};

/** Pause menu: Resume, Settings, Quit to desktop. Escape or gamepad B/Start resumes. */
UCLASS()
class GRANTHEFTPRALINE_API UGTPPauseMenuWidget : public UGTPMenuWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildButtons() override;
	virtual FText GetHeading() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleResume();

	UFUNCTION()
	void HandleSettings();

	UFUNCTION()
	void HandleControls();

	UFUNCTION()
	void HandleCredits();

	UFUNCTION()
	void HandleQuit();
};

/** Credits: the project licence, the data sources and the tools it builds on (texts in the string table). */
UCLASS()
class GRANTHEFTPRALINE_API UGTPCreditsMenuWidget : public UGTPMenuWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildButtons() override;
	virtual FText GetHeading() const override;
	virtual FText GetSubtitle() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleBack();
};

/**
 * Controls screen: the keyboard keys (AZERTY or QWERTY, as chosen) and the
 * gamepad buttons of each action, from GTPControls.h. Back, Escape or gamepad
 * B returns to the menu it was opened from (same route as the settings).
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPControlsMenuWidget : public UGTPMenuWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildButtons() override;
	virtual FText GetHeading() const override;
	virtual FText GetSubtitle() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	UFUNCTION()
	void HandleBack();
};

/**
 * Settings screen (docs/ui/settings.md), reached from the title screen and the
 * pause menu. One button per setting, "label : value"; a click moves to the
 * next value, saves and applies it at once. Back, Escape or gamepad B returns
 * to the menu it was opened from.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPSettingsMenuWidget : public UGTPMenuWidget
{
	GENERATED_BODY()

protected:
	virtual void BuildButtons() override;
	virtual FText GetHeading() const override;
	virtual FText GetSubtitle() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	/** One label per setting, rebuilt from the saved values after every click. */
	void RefreshRows();

	UFUNCTION()
	void HandleLanguage();
	UFUNCTION()
	void HandleGraphics();
	UFUNCTION()
	void HandleVolume();
	UFUNCTION()
	void HandleSensitivity();
	UFUNCTION()
	void HandleInvertY();
	UFUNCTION()
	void HandleKeyboard();
	UFUNCTION()
	void HandleMinimap();
	UFUNCTION()
	void HandleMinimapZoom();
	UFUNCTION()
	void HandleBack();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LanguageLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GraphicsLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VolumeLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SensitivityLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InvertYLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> KeyboardLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MinimapLabel;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MinimapZoomLabel;
};
