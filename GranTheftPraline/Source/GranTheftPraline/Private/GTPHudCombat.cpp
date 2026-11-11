#include "GTPHudCombat.h"

#include "GTPCombat.h"
#include "GTPHealth.h"
#include "GTPHudMath.h"
#include "GTPLocalization.h"
#include "GTPMinimap.h"
#include "GTPWeapon.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace
{
	void Line(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness)
	{
		FCanvasLineItem Item(A, B);
		Item.SetColor(Color);
		Item.LineThickness = Thickness;
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}

	/** Arc of a circle, angles in degrees in canvas space (0 = right, -90 = up). */
	void Arc(UCanvas* Canvas, const FVector2D& Centre, float Radius, float FromDeg, float ToDeg, const FLinearColor& Color, float Thickness)
	{
		const int32 Steps = FMath::Max(2, FMath::CeilToInt(FMath::Abs(ToDeg - FromDeg) / 6.f));
		FVector2D Prev = Centre + FVector2D(FMath::Cos(FMath::DegreesToRadians(FromDeg)), FMath::Sin(FMath::DegreesToRadians(FromDeg))) * Radius;
		for (int32 i = 1; i <= Steps; ++i)
		{
			const float A = FMath::DegreesToRadians(FMath::Lerp(FromDeg, ToDeg, static_cast<float>(i) / Steps));
			const FVector2D Next = Centre + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius;
			Line(Canvas, Prev, Next, Color, Thickness);
			Prev = Next;
		}
	}

	void Text(UCanvas* Canvas, const FText& Value, const FVector2D& At, float Scale, const FLinearColor& Color)
	{
		const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
		if (!Font)
		{
			return;
		}
		FCanvasTextItem Item(At, Value, Font, Color);
		Item.Scale = FVector2D(Scale);
		Item.BlendMode = SE_BLEND_Translucent;
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f), FVector2D(1.f, 1.f));
		Canvas->DrawItem(Item);
	}
}

bool GTPHudCombat::Draw(UCanvas* Canvas, const APlayerController* Controller, float Scale)
{
	if (!Canvas || !Controller || !UGTPCombatSubsystem::IsEnabled())
	{
		return false;
	}
	const APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		return false;
	}
	bool bDrewReticle = false;
	const FVector2D Centre(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// Weapon: reticle and line.
	const UGTPWeaponComponent* Weapon = Pawn->FindComponentByClass<UGTPWeaponComponent>();
	if (Weapon && Weapon->IsDrawn())
	{
		const float Fov = Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetFOVAngle() : 90.f;
		const float Radius = GTPUI::ReticleRadiusPixels(Weapon->GetSpreadDegrees(), Fov, Canvas->ClipX);
		const FLinearColor Color(0.95f, 0.95f, 0.93f, Weapon->IsAiming() ? 0.95f : 0.6f);
		Arc(Canvas, Centre, Radius, 0.f, 360.f, Color, FMath::Max(1.f, 1.5f * Scale));
		Line(Canvas, Centre + FVector2D(-1.5f, 0.f) * Scale, Centre + FVector2D(1.5f, 0.f) * Scale, Color, FMath::Max(2.f, 3.f * Scale));
		bDrewReticle = true;

		// "Pistolet 9 mm   12 / 48", beside the mini-map.
		FVector2D DiscCentre;
		float DiscRadius = 0.f;
		UGTPMinimapSubsystem::GetLayout(Canvas, Scale, DiscCentre, DiscRadius);
		const FString Key = Weapon->GetWeaponKey().ToString();
		const FTCHARToUTF8 KeyUtf8(*Key);
		FString Line1 = GTPLocString(KeyUtf8.Get());
		FString Line2 = Weapon->IsReloading()
			? GTPLocString("hud.reloading")
			: FString::Printf(TEXT("%d / %d"), Weapon->GetMagazine(), Weapon->GetReserve());
		const FVector2D At(DiscCentre.X + DiscRadius + 28.f * Scale, DiscCentre.Y + DiscRadius - 56.f * Scale);
		Text(Canvas, FText::FromString(Line1), At, 0.8f * Scale, FLinearColor(0.85f, 0.85f, 0.82f));
		Text(Canvas, FText::FromString(Line2), At + FVector2D(0.f, 24.f * Scale), 1.1f * Scale,
			(!Weapon->IsReloading() && Weapon->GetMagazine() == 0) ? FLinearColor(0.95f, 0.3f, 0.25f) : FLinearColor::White);
	}

	// Hit marker.
	if (const UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(Pawn))
	{
		bool bKilled = false;
		const float Alpha = GTPUI::HitMarkerAlpha(Combat->GetSecondsSincePlayerHit(bKilled));
		if (Alpha > 0.f)
		{
			const FLinearColor Color = bKilled ? FLinearColor(0.95f, 0.15f, 0.12f, Alpha) : FLinearColor(1.f, 1.f, 1.f, Alpha);
			const float Near = 7.f * Scale, Far = 15.f * Scale;
			for (const FVector2D& D : {FVector2D(1.f, 1.f), FVector2D(-1.f, 1.f), FVector2D(1.f, -1.f), FVector2D(-1.f, -1.f)})
			{
				Line(Canvas, Centre + D * Near, Centre + D * Far, Color, FMath::Max(1.5f, 2.f * Scale));
			}
		}
	}

	// Health and armour: two arcs over the top of the mini-map, filled left to right.
	if (const UGTPHealthComponent* Health = Pawn->FindComponentByClass<UGTPHealthComponent>())
	{
		FVector2D DiscCentre;
		float DiscRadius = 0.f;
		UGTPMinimapSubsystem::GetLayout(Canvas, Scale, DiscCentre, DiscRadius);
		const float From = -170.f, To = -10.f;
		const float Fill = FMath::Clamp(Health->GetHealth01(), 0.f, 1.f);
		const GTPUI::FRgb C = GTPUI::HealthColor(Fill);
		const float R1 = DiscRadius + 9.f * Scale;
		Arc(Canvas, DiscCentre, R1, From, To, FLinearColor(0.f, 0.f, 0.f, 0.5f), FMath::Max(3.f, 5.f * Scale));
		if (Fill > 0.f)
		{
			Arc(Canvas, DiscCentre, R1, From, FMath::Lerp(From, To, Fill), FLinearColor(C.R, C.G, C.B, 0.95f), FMath::Max(3.f, 5.f * Scale));
		}
		const float Armor = FMath::Clamp(Health->GetArmor01(), 0.f, 1.f);
		if (Armor > 0.f)
		{
			const float R2 = DiscRadius + 16.f * Scale;
			Arc(Canvas, DiscCentre, R2, From, To, FLinearColor(0.f, 0.f, 0.f, 0.5f), FMath::Max(2.f, 3.f * Scale));
			Arc(Canvas, DiscCentre, R2, From, FMath::Lerp(From, To, Armor), FLinearColor(0.35f, 0.65f, 0.95f, 0.95f), FMath::Max(2.f, 3.f * Scale));
		}
	}
	return bDrewReticle;
}
