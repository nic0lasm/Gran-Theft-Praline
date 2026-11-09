#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GTPHUD.generated.h"

class UFont;

/**
 * Minimal on-foot HUD, drawn on the canvas (no widget asset):
 * - a small centre dot, faint in third person, clearer in first person;
 * - contextual help at the bottom right, one line per action: the key in a
 *   light box, then the verb ("E  Entrer", "F  Monter"), fading in and out;
 * - a subtitle line at the bottom centre for short messages.
 * Hidden while a menu is open. Sizes are given for 1080 lines and scaled.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UPROPERTY(Config, EditAnywhere, Category = "Reticle")
	bool bShowReticle = true;

	/** Dot radius in pixels at 1080 lines. */
	UPROPERTY(Config, EditAnywhere, Category = "Reticle")
	float ReticleRadius = 2.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Reticle")
	float ReticleOpacityThirdPerson = 0.45f;

	UPROPERTY(Config, EditAnywhere, Category = "Reticle")
	float ReticleOpacityFirstPerson = 0.8f;

	/** Fade speed of the prompts, 1/s. */
	UPROPERTY(Config, EditAnywhere, Category = "Prompts")
	float PromptFadeSpeed = 8.f;

	/** Text height in pixels at 1080 lines. */
	UPROPERTY(Config, EditAnywhere, Category = "Prompts")
	float PromptTextHeight = 22.f;

private:
	void DrawReticle(float Scale, float Opacity);
	void DrawPrompts(float Scale, float DeltaTime);
	void DrawMessage(float Scale);
	float TextScaleFor(const UFont* Font, float Scale) const;
	FVector2D MeasureText(const UFont* Font, const FText& Text, float TextScale) const;

	float PromptAlpha = 0.f;
	/** Last lines shown, kept while they fade out. */
	TArray<TPair<FText, FText>> ShownPrompts;
};
