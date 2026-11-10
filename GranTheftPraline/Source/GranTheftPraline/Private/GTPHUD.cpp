#include "GTPHUD.h"

#include "GTPCameraModeComponent.h"
#include "GTPInteraction.h"
#include "GTPMinimap.h"
#include "GTPPlayerController.h"
#include "GTPVehicle.h"
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

	void GTPDrawRect(UCanvas* Canvas, const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(Position, Size, Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}

	void GTPDrawText(UCanvas* Canvas, const UFont* Font, const FText& Text, const FVector2D& Position, float TextScale, const FLinearColor& Color, bool bShadow)
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

}

FVector2D AGTPHUD::MeasureText(const UFont* Font, const FText& Text, float TextScale) const
{
	// Measured through the canvas: the engine fonts are runtime-cached, their legacy metrics are empty.
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text.ToString(), Width, Height, const_cast<UFont*>(Font), TextScale);
	return FVector2D(Width, Height);
}

float AGTPHUD::TextScaleFor(const UFont* Font, float Scale) const
{
	const float FontHeight = Font ? FMath::Max(1.f, static_cast<float>(MeasureText(Font, FText::FromString(TEXT("Ag")), 1.f).Y)) : 1.f;
	return PromptTextHeight * Scale / FontHeight;
}

void AGTPHUD::DrawHUD()
{
	Super::DrawHUD();
	const AGTPPlayerController* PC = Cast<AGTPPlayerController>(GetOwningPlayerController());
	if (Canvas && PC && PC->IsFullMapOpen())
	{
		// The full-screen map replaces the HUD while the game is paused on it.
		if (UGTPMinimapSubsystem* Minimap = GetWorld() ? GetWorld()->GetSubsystem<UGTPMinimapSubsystem>() : nullptr)
		{
			Minimap->DrawFullMap(Canvas, PC);
		}
		return;
	}
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
	const float DrivingHeight = DrawDriving(Scale);
	DrawPrompts(Scale, DeltaTime, DrivingHeight);
	DrawMessage(Scale);
	// Bottom left; the driving panel and the prompts sit on the right (UGTPMinimapSubsystem).
	if (UGTPMinimapSubsystem* Minimap = GetWorld() ? GetWorld()->GetSubsystem<UGTPMinimapSubsystem>() : nullptr)
	{
		Minimap->Draw(Canvas, PC, Scale);
	}
}

float AGTPHUD::DrawDriving(float Scale)
{
	const AGTPPlayerController* PC = Cast<AGTPPlayerController>(GetOwningPlayerController());
	const AGTPVehicle* Vehicle = PC ? Cast<AGTPVehicle>(PC->GetPawn()) : nullptr;
	const UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	const UFont* SmallFont = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (!Vehicle || !Font || !SmallFont)
	{
		return 0.f;
	}

	const float Margin = 48.f * Scale;
	const float Pad = 14.f * Scale;
	const float SpeedHeight = FMath::Max(1.f, static_cast<float>(MeasureText(Font, FText::FromString(TEXT("0")), 1.f).Y));
	const float SpeedScale = SpeedTextHeight * Scale / SpeedHeight;
	const float SmallScale = TextScaleFor(SmallFont, Scale);

	const int32 Speed = FMath::RoundToInt(FMath::Abs(Vehicle->GetSpeedKmh()));
	const int32 Gear = Vehicle->GetGear();
	const FText SpeedText = FText::AsNumber(Speed);
	const FText UnitText = FText::FromString(TEXT("km/h"));
	const FText GearText = FText::FromString(Gear < 0 ? TEXT("R") : (Gear == 0 ? TEXT("N") : FString::FromInt(Gear)));

	const FVector2D SpeedSize = MeasureText(Font, SpeedText, SpeedScale);
	const FVector2D UnitSize = MeasureText(SmallFont, UnitText, SmallScale);
	const FVector2D GearSize = MeasureText(SmallFont, GearText, SmallScale);
	// Width fixed for three digits, so the panel does not jump with the speed.
	const float DigitsWidth = MeasureText(Font, FText::FromString(TEXT("888")), SpeedScale).X;
	const float BarHeight = 5.f * Scale;
	const float Width = DigitsWidth + Pad + FMath::Max(UnitSize.X, GearSize.X) + 2.f * Pad;
	const float Height = SpeedSize.Y + BarHeight + 3.f * Pad;
	const FVector2D TopLeft(Canvas->ClipX - Margin - Width, Canvas->ClipY - Margin - Height);

	GTPDrawRect(Canvas, TopLeft, FVector2D(Width, Height), PanelColor);
	const float DigitsRight = TopLeft.X + Pad + DigitsWidth;
	GTPDrawText(Canvas, Font, SpeedText, FVector2D(DigitsRight - SpeedSize.X, TopLeft.Y + Pad), SpeedScale, TextColor, true);
	GTPDrawText(Canvas, SmallFont, GearText, FVector2D(DigitsRight + Pad, TopLeft.Y + Pad), SmallScale, TextColor, true);
	GTPDrawText(Canvas, SmallFont, UnitText, FVector2D(DigitsRight + Pad, TopLeft.Y + Pad + SpeedSize.Y - UnitSize.Y), SmallScale,
		FLinearColor(TextColor.R, TextColor.G, TextColor.B, 0.7f), true);

	// State of the car: white, then amber, then red as it gets damaged.
	const float Health = FMath::Clamp(Vehicle->GetHealthRatio(), 0.f, 1.f);
	const FLinearColor BarColor = Health > 0.6f ? FLinearColor(0.9f, 0.9f, 0.88f, 0.85f)
		: (Health > 0.35f ? FLinearColor(1.f, 0.65f, 0.15f, 0.9f) : FLinearColor(0.95f, 0.2f, 0.15f, 0.95f));
	const FVector2D BarPos(TopLeft.X + Pad, TopLeft.Y + Height - Pad - BarHeight);
	const float BarWidth = Width - 2.f * Pad;
	GTPDrawRect(Canvas, BarPos, FVector2D(BarWidth, BarHeight), FLinearColor(1.f, 1.f, 1.f, 0.15f));
	GTPDrawRect(Canvas, BarPos, FVector2D(BarWidth * Health, BarHeight), BarColor);
	return Height + 12.f * Scale;
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

void AGTPHUD::DrawPrompts(float Scale, float DeltaTime, float BottomOffset)
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
	float Bottom = Canvas->ClipY - Margin - BottomOffset;
	for (int32 Index = ShownPrompts.Num() - 1; Index >= 0; --Index)
	{
		const FText& Key = ShownPrompts[Index].Key;
		const FText& Action = ShownPrompts[Index].Value;
		const FVector2D ActionSize = MeasureText(Font, Action, TextScale);
		const FVector2D KeySize = MeasureText(Font, Key, TextScale);
		const float BoxWidth = FMath::Max(KeyBox, KeySize.X + 14.f * Scale);
		const float RowWidth = BoxWidth + Gap + ActionSize.X;
		const float Left = Canvas->ClipX - Margin - RowWidth + Slide;
		const float Top = Bottom - KeyBox;

		GTPDrawRect(Canvas, FVector2D(Left - Gap, Top - 6.f * Scale), FVector2D(RowWidth + 2.f * Gap, KeyBox + 12.f * Scale),
			PanelColor * FLinearColor(1.f, 1.f, 1.f, PromptAlpha));
		GTPDrawRect(Canvas, FVector2D(Left, Top), FVector2D(BoxWidth, KeyBox), KeyBoxColor * FLinearColor(1.f, 1.f, 1.f, PromptAlpha));
		GTPDrawText(Canvas, Font, Key, FVector2D(Left + (BoxWidth - KeySize.X) * 0.5f, Top + (KeyBox - KeySize.Y) * 0.5f), TextScale,
			FLinearColor(KeyTextColor.R, KeyTextColor.G, KeyTextColor.B, PromptAlpha), false);
		GTPDrawText(Canvas, Font, Action, FVector2D(Left + BoxWidth + Gap, Top + (KeyBox - ActionSize.Y) * 0.5f), TextScale,
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
	const FVector2D Size = MeasureText(Font, Message, TextScale);
	const FVector2D Position((Canvas->ClipX - Size.X) * 0.5f, Canvas->ClipY - 140.f * Scale);
	const float Pad = 10.f * Scale;
	GTPDrawRect(Canvas, Position - FVector2D(Pad * 1.5f, Pad), Size + FVector2D(Pad * 3.f, Pad * 2.f), PanelColor);
	GTPDrawText(Canvas, Font, Message, Position, TextScale, TextColor, true);
}
