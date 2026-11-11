#include "GTPHudMoney.h"

#include "GTPHudMath.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#if __has_include("GTPWallet.h")
#include "GTPWallet.h"

namespace
{
	int32 LastBalance = MIN_int32;
	int32 LastDelta = 0;
	double LastChangeTime = -100.0;

	FString Euros(int32 Value)
	{
		return FString(UTF8_TO_TCHAR(GTPUI::FormatThousands(Value).c_str())) + TEXT(" €");
	}

	void Put(UCanvas* Canvas, const FString& Text, const FVector2D& RightTop, float Scale, const FLinearColor& Color)
	{
		const UFont* Font = GEngine ? GEngine->GetLargeFont() : nullptr;
		if (!Font)
		{
			return;
		}
		float W = 0.f, H = 0.f;
		Canvas->TextSize(Font, Text, W, H, Scale, Scale);
		FCanvasTextItem Item(FVector2D(RightTop.X - W, RightTop.Y), FText::FromString(Text), Font, Color);
		Item.Scale = FVector2D(Scale);
		Item.BlendMode = SE_BLEND_Translucent;
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.9f), FVector2D(1.5f, 1.5f));
		Canvas->DrawItem(Item);
	}
}

void GTPHudMoney::Draw(UCanvas* Canvas, const APlayerController* Controller, float Scale)
{
	const UGTPWalletSubsystem* Wallet = Canvas && Controller ? UGTPWalletSubsystem::Get(Controller) : nullptr;
	if (!Wallet)
	{
		return;
	}
	const int32 Balance = Wallet->GetMoney();
	const double Now = FPlatformTime::Seconds();
	if (LastBalance != MIN_int32 && Balance != LastBalance)
	{
		LastDelta = Balance - LastBalance;
		LastChangeTime = Now;
	}
	LastBalance = Balance;

	const FVector2D RightTop(Canvas->ClipX - 40.f * Scale, 120.f * Scale);
	const float TextScale = 0.9f * Scale;
	Put(Canvas, Euros(Balance), RightTop, TextScale, FLinearColor(0.8f, 0.95f, 0.75f));
	const float Alpha = GTPUI::MoneyFlashAlpha(static_cast<float>(Now - LastChangeTime));
	if (Alpha > 0.f && LastDelta != 0)
	{
		const FString Text = (LastDelta > 0 ? TEXT("+") : TEXT("")) + Euros(LastDelta);
		const FLinearColor Color = LastDelta > 0 ? FLinearColor(0.45f, 0.95f, 0.45f, Alpha) : FLinearColor(0.95f, 0.4f, 0.35f, Alpha);
		Put(Canvas, Text, RightTop + FVector2D(0.f, 34.f * Scale), 0.7f * Scale, Color);
	}
}

#else

void GTPHudMoney::Draw(UCanvas*, const APlayerController*, float) {}

#endif
