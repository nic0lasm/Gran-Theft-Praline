#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GTPMenuWidgets.generated.h"

class UButton;
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

	UButton* AddButton(const FText& Label);
	AGTPPlayerController* GetGTPController() const;

private:
	void BuildTree();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ButtonColumn;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> DefaultFocusTarget;
};

/** Title screen: Play, Quit. */
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
	void HandleQuit();
};

/** Pause menu: Resume, Quit to desktop. Escape or gamepad B/Start resumes. */
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
	void HandleQuit();
};
