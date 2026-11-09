#include "GTPHUD.h"

#include "GTPCameraModeComponent.h"
#include "GTPInteraction.h"
#include "GTPPlayerController.h"
#include "CanvasItem.h"
#include "RenderUtils.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

namespace
{
	const FLinearColor TextColor(0.95f, 0.95f, 0.93f);
	const FLinearColor KeyBoxColor(0.92f, 0.92f, 0.9f);
	const FLinearColor KeyTextColor(0.06f, 0.06f, 0.06f);
	const FLinearColor PanelColor(0.f, 0.f, 0.f, 0.45f);

	void DrawRect(UCanvas* Canvas, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(Position, Size, Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}

	void DrawText(UCanvas* Canvas, const UFont* Font, const FText& Text, const FVector2D& Position, float TextScale, const FLinearColor& Color, bool bShadow)
	{
		FCanvasTextItem Item(Position, Text, Font, Color);
		Item.Scale = FVector2D(TextScale);
		Item.BlendMode = SE_BLEND_Translucent;
		if (bShadow)
		{
			Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), FVector2D(1.f, 1.f));
		}
		Canvas->DrawItem(Item);
	}

	FVector2D TextSize(const UFont* Font, const FText& Text, float TextScale)
	{
		const FString String = Text.ToString();
		return FVector2D(Font->GetStringSize(*String), Font->GetStringHeightSize(*String)) * TextScale;
	}
}

float AGTPHUD::TextScaleFor(const UFont* Font, float Scale) const
{
	const float FontHeight = Font ? FMath::Max(1.f, Font->GetMaxCharHeight()) : 1.f;
	return PromptTextHeight * Scale / FontHeight;
}

void AGTPHUD::DrawHUD()
{
	Super::DrawHUD();
	const AGTPPlayerController* PC = Cast<AGTPPlayerController>(GetOwningPlayerController());
	if (!Canvas || !PC || PC->IsPaused() || PC->IsMenuOpen())
	{
		return;
	}
	const float Scale = Canvas->ClipY / 1080.f;
	const float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;

	if (bShowReticle && Cast<ACharacter>(PC->GetPawn()))
	{
		const bool bFirstPerson = PC->GetCameraMode() && PC->GetCameraMode()->IsFirstPerson();
		DrawReticle(Scale, bFirstPerson ? ReticleOpacityFirstPerson : ReticleOpacityThirdPerson);
	}
	DrawPrompts(Scale, DeltaTime);
	DrawMessage(Scale);
}

void AGTPHUD::DrawReticle(float Scale, float Opacity)
{
	const FVector2D Centre(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const float Radius = FMath::Max(1.5f, ReticleRadius * Scale);
	// Dark ring under a light dot: readable on a bright facade and on a dark street.
	FCanvasNGonItem Ring(Centre, FVector2D(Radius + 1.f), 16, GWhiteTexture, FLinearColor(0.f, 0.f, 0.f, Opacity * 0.6f));
	Ring.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Ring);
	FCanvasNGonItem Dot(Centre, FVector2D(Radius), 16, GWhiteTexture, FLinearColor(1.f, 1.f, 1.f, Opacity));
	Dot.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Dot);
}

void AGTPHUD::DrawPrompts(float Scale, float DeltaTime)
{
	const AGTPPlayerController* PC = Cast<AGTPPlayerController>(GetOwningPlayerController());
	const UGTPInteractionComponent* Interaction = PC ? PC->GetInteraction() : nullptr;
	const TArray<FGTPInteractionPrompt> Empty;
	const TArray<FGTPInteractionPrompt>& Current = Interaction ? Interaction->GetPrompts() : Empty;

	if (Current.Num() > 0)
	{
		ShownPrompts.Reset();
		for (const FGTPInteractionPrompt& Prompt : Current)
		{
			ShownPrompts.Emplace(Prompt.Key, Prompt.Action);
		}
	}
	const float Target = Current.Num() > 0 ? 1.f : 0.f;
	PromptAlpha = FMath::FInterpConstantTo(PromptAlpha, Target, DeltaTime, PromptFadeSpeed);
	if (PromptAlpha <= 0.f || ShownPrompts.Num() == 0)
	{
		return;
	}

	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Font)
	{
		return;
	}
	const float TextScale = TextScaleFor(Font, Scale);
	const float Margin = 48.f * Scale;
	const float LineHeight = PromptTextHeight * Scale * 1.9f;
	const float KeyBox = PromptTextHeight * Scale * 1.45f;
	const float Gap = 12.f * Scale;

	// Bottom right, stacked upwards, slightly sliding in with the fade.
	const float Slide = (1.f - PromptAlpha) * 14.f * Scale;
	float Bottom = Canvas->ClipY - Margin;
	for (int32 Index = ShownPrompts.Num() - 1; Index >= 0; --Index)
	{
		const FText& Key = ShownPrompts[Index].Key;
		const FText& Action = ShownPrompts[Index].Value;
		const FVector2D ActionSize = TextSize(Font, Action, TextScale);
		const FVector2D KeySize = TextSize(Font, Key, TextScale);
		const float BoxWidth = FMath::Max(KeyBox, KeySize.X + 14.f * Scale);
		const float RowWidth = BoxWidth + Gap + ActionSize.X;
		const float Left = Canvas->ClipX - Margin - RowWidth + Slide;
		const float Top = Bottom - KeyBox;

		DrawRect(Canvas, FVector2D(Left - Gap, Top - 6.f * Scale), FVector2D(RowWidth + 2.f * Gap, KeyBox + 12.f * Scale),
			PanelColor * FLinearColor(1.f, 1.f, 1.f, PromptAlpha));
		DrawRect(Canvas, FVector2D(Left, Top), FVector2D(BoxWidth, KeyBox), KeyBoxColor * FLinearColor(1.f, 1.f, 1.f, PromptAlpha));
		DrawText(Canvas, Font, Key, FVector2D(Left + (BoxWidth - KeySize.X) * 0.5f, Top + (KeyBox - KeySize.Y) * 0.5f), TextScale,
			FLinearColor(KeyTextColor.R, KeyTextColor.G, KeyTextColor.B, PromptAlpha), false);
		DrawText(Canvas, Font, Action, FVector2D(Left + BoxWidth + Gap, Top + (KeyBox - ActionSize.Y) * 0.5f), TextScale,
			FLinearColor(TextColor.R, TextColor.G, TextColor.B, PromptAlpha), true);

		Bottom = Top - (LineHeight - KeyBox);
	}
}

void AGTPHUD::DrawMessage(float Scale)
{
	const AGTPPlayerController* PC = Cast<AGTPPlayerController>(GetOwningPlayerController());
	const UGTPInteractionComponent* Interaction = PC ? PC->GetInteraction() : nullptr;
	const FText Message = Interaction ? Interaction->GetMessage() : FText::GetEmpty();
	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (Message.IsEmpty() || !Font)
	{
		return;
	}
	const float TextScale = TextScaleFor(Font, Scale);
	const FVector2D Size = TextSize(Font, Message, TextScale);
	const FVector2D Position((Canvas->ClipX - Size.X) * 0.5f, Canvas->ClipY - 140.f * Scale);
	const float Pad = 10.f * Scale;
	DrawRect(Canvas, Position - FVector2D(Pad * 1.5f, Pad), Size + FVector2D(Pad * 3.f, Pad * 2.f), PanelColor);
	DrawText(Canvas, Font, Message, Position, TextScale, TextColor, true);
}
