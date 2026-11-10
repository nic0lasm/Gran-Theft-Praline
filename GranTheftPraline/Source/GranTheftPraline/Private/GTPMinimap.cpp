#include "GTPMinimap.h"

#include "GTPLocalization.h"
#include "GTPLog.h"
#include "GTPPlayerController.h"
#include "GTPUserSettings.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPMinimap, Log, All);

namespace
{
	constexpr int32 DiscSegments = 48;

	FVector2D ToCanvas(const FVector2D& Centre, const GTPUI::FVec2& P)
	{
		return Centre + FVector2D(P.X, P.Y);
	}

	void DrawLine(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness)
	{
		FCanvasLineItem Line(A, B);
		Line.SetColor(Color);
		Line.LineThickness = Thickness;
		Line.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Line);
	}

	void DrawDisc(UCanvas* Canvas, const FVector2D& Centre, float Radius, const FLinearColor& Color)
	{
		for (int32 i = 0; i < DiscSegments; ++i)
		{
			const float A0 = 2.f * PI * i / DiscSegments;
			const float A1 = 2.f * PI * (i + 1) / DiscSegments;
			FCanvasTriangleItem Triangle(
				Centre,
				Centre + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius,
				Centre + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius,
				GWhiteTexture);
			Triangle.SetColor(Color);
			Triangle.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Triangle);
		}
	}

	void DrawRing(UCanvas* Canvas, const FVector2D& Centre, float Radius, const FLinearColor& Color, float Thickness)
	{
		for (int32 i = 0; i < DiscSegments; ++i)
		{
			const float A0 = 2.f * PI * i / DiscSegments;
			const float A1 = 2.f * PI * (i + 1) / DiscSegments;
			DrawLine(Canvas, Centre + FVector2D(FMath::Cos(A0), FMath::Sin(A0)) * Radius,
				Centre + FVector2D(FMath::Cos(A1), FMath::Sin(A1)) * Radius, Color, Thickness);
		}
	}

	struct FStyle
	{
		FLinearColor Color;
		float Thickness; // pixels at 1080 lines
	};

	FStyle StyleFor(GTPUI::EMapKind Kind, float WidthM)
	{
		switch (Kind)
		{
		case GTPUI::EMapKind::RoadMajor: return {FLinearColor(0.88f, 0.88f, 0.85f, 0.95f), FMath::Clamp(WidthM * 0.25f, 2.5f, 5.f)};
		case GTPUI::EMapKind::RoadMinor: return {FLinearColor(0.72f, 0.72f, 0.7f, 0.92f), FMath::Clamp(WidthM * 0.25f, 1.5f, 3.f)};
		case GTPUI::EMapKind::Path: return {FLinearColor(0.42f, 0.5f, 0.42f, 0.8f), 1.f};
		case GTPUI::EMapKind::Tunnel: return {FLinearColor(0.35f, 0.35f, 0.35f, 0.5f), 1.5f};
		case GTPUI::EMapKind::Water: return {FLinearColor(0.25f, 0.45f, 0.7f, 0.9f), 3.f};
		default: return {FLinearColor(0.23f, 0.24f, 0.27f, 0.5f), 1.f};
		}
	}
}

bool UGTPMinimapSubsystem::IsDisabledOnCommandLine()
{
	// -Minimap=0 or -NoMinimap: not created, not drawn (isolates its cost in measurements).
	static const bool bDisabled = []()
	{
		int32 Value = 1;
		return FParse::Param(FCommandLine::Get(), TEXT("NoMinimap")) || (FParse::Value(FCommandLine::Get(), TEXT("Minimap="), Value) && Value == 0);
	}();
	return bDisabled;
}

bool UGTPMinimapSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsDisabledOnCommandLine() && Cast<UWorld>(Outer) && Cast<UWorld>(Outer)->IsGameWorld())
	{
		GTP_TRACE(LogGTPMinimap, TEXT("minimap: off (-Minimap=0)"));
	}
	// Game worlds only: no mini-map in editor preview worlds or on a dedicated server.
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && !IsRunningDedicatedServer() && !IsDisabledOnCommandLine() && Super::ShouldCreateSubsystem(Outer);
}

void UGTPMinimapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadData();
}

void UGTPMinimapSubsystem::LoadData()
{
	if (bLoaded)
	{
		return;
	}
	bLoaded = true;
	const FString MapName = UGameplayStatics::GetCurrentLevelName(this, true);
	const FString Path = FPaths::ProjectContentDir() / TEXT("City/Minimap") / (MapName + TEXT(".minimap.csv"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		GTP_TRACE(LogGTPMinimap, TEXT("minimap: on but no data for %s (%s): the mini-map stays hidden. Run tools/ui/minimap.py --map %s"),
			*MapName, *Path, *MapName);
		return;
	}
	const FTCHARToUTF8 Utf8(*Text);
	const int32 Kept = Data.ParseCsv(std::string_view(Utf8.Get(), static_cast<size_t>(Utf8.Length())));
	for (const GTPUI::FMapLabel& Label : Data.BuildStreetLabels())
	{
		Labels.Add(Label);
	}
	GTP_TRACE(LogGTPMinimap, TEXT("minimap: on (%d polylines from %s)"), Kept, *Path);
}

void UGTPMinimapSubsystem::AddBlip(AActor* Actor, const FLinearColor& Color, bool bClampToEdge)
{
	if (!Actor)
	{
		return;
	}
	for (FBlip& Blip : Blips)
	{
		if (Blip.Actor.Get() == Actor)
		{
			Blip.Color = Color;
			Blip.bClampToEdge = bClampToEdge;
			return;
		}
	}
	Blips.Add({Actor, Color, bClampToEdge});
}

void UGTPMinimapSubsystem::RemoveBlip(AActor* Actor)
{
	Blips.RemoveAll([Actor](const FBlip& Blip) { return !Blip.Actor.IsValid() || Blip.Actor.Get() == Actor; });
}

void UGTPMinimapSubsystem::Draw(UCanvas* Canvas, const AGTPPlayerController* Controller, float Scale)
{
	const UGTPUserSettings* User = UGTPUserSettings::Find(this);
	if (IsDisabledOnCommandLine() || !Canvas || !Controller || !HasData() || (User && !User->Get().bMinimap))
	{
		return;
	}
	const APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		return;
	}

	const float Radius = RadiusAt1080 * Scale;
	const FVector2D Centre(Radius + MarginAt1080 * Scale, Canvas->ClipY - Radius - MarginAt1080 * Scale);

	GTPUI::FMapView View;
	const FVector Location = Pawn->GetActorLocation();
	View.Center = {static_cast<float>(Location.X), static_cast<float>(Location.Y)};
	View.HeadingRadians = FMath::DegreesToRadians(Controller->GetControlRotation().Yaw);
	View.RadiusPixels = Radius;
	View.WorldRadiusCm = GTPUI::WorldRadiusForZoom(User ? static_cast<int>(User->Get().MinimapZoom) : 1);

	DrawDisc(Canvas, Centre, Radius, FLinearColor(0.05f, 0.06f, 0.07f, 0.72f));

	// Far zoom: roads and water only, buildings and paths would be a few thousand lines of noise.
	const bool bFar = User && User->Get().MinimapZoom == GTPUI::EMinimapZoom::Far;
	GTPUI::ForEachVisibleSegment(Data, View, [&](GTPUI::EMapKind Kind, float WidthM, const GTPUI::FVec2& A, const GTPUI::FVec2& B) {
		if (bFar && (Kind == GTPUI::EMapKind::Building || Kind == GTPUI::EMapKind::Path))
		{
			return;
		}
		const FStyle Style = StyleFor(Kind, WidthM);
		DrawLine(Canvas, ToCanvas(Centre, A), ToCanvas(Centre, B), Style.Color, FMath::Max(1.f, Style.Thickness * Scale));
	});

	for (const FBlip& Blip : Blips)
	{
		const AActor* Actor = Blip.Actor.Get();
		if (!Actor)
		{
			continue;
		}
		const FVector P = Actor->GetActorLocation();
		GTPUI::FVec2 Screen = View.ToScreen({static_cast<float>(P.X), static_cast<float>(P.Y)});
		const float Distance = FMath::Sqrt(Screen.X * Screen.X + Screen.Y * Screen.Y);
		if (Distance > Radius)
		{
			if (!Blip.bClampToEdge)
			{
				continue;
			}
			Screen.X *= Radius / Distance;
			Screen.Y *= Radius / Distance;
		}
		DrawDisc(Canvas, ToCanvas(Centre, Screen), 4.f * Scale, Blip.Color);
	}

	// Ring, north mark, and the player's arrow (always up).
	DrawRing(Canvas, Centre, Radius, FLinearColor(0.9f, 0.9f, 0.88f, 0.8f), FMath::Max(1.f, 2.f * Scale));

	// North on the map is -Y in world space (Y points south).
	const GTPUI::FVec2 North = View.ToScreen({View.Center.X, View.Center.Y - View.WorldRadiusCm});
	const float NorthLength = FMath::Sqrt(North.X * North.X + North.Y * North.Y);
	if (NorthLength > 1.f)
	{
		const UFont* Font = GEngine ? GEngine->GetSmallFont() : nullptr;
		if (Font)
		{
			const FVector2D Mark = Centre + FVector2D(North.X, North.Y) * ((Radius - 11.f * Scale) / NorthLength);
			FCanvasTextItem Text(Mark - FVector2D(4.f * Scale, 6.f * Scale), GTPLoc("minimap.north"), Font, FLinearColor(0.95f, 0.45f, 0.35f));
			Text.Scale = FVector2D(Scale);
			Text.BlendMode = SE_BLEND_Translucent;
			Text.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.8f));
			Canvas->DrawItem(Text);
		}
	}

	const float Arrow = 7.f * Scale;
	FCanvasTriangleItem Pointer(
		Centre + FVector2D(0.f, -Arrow * 1.3f),
		Centre + FVector2D(-Arrow * 0.8f, Arrow),
		Centre + FVector2D(Arrow * 0.8f, Arrow),
		GWhiteTexture);
	Pointer.SetColor(FLinearColor(1.f, 0.62f, 0.1f, 1.f));
	Pointer.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Pointer);
}

void UGTPMinimapSubsystem::DrawFullMap(UCanvas* Canvas, const AGTPPlayerController* Controller)
{
	if (!Canvas || !Controller || !HasData() || !Controller->GetPawn())
	{
		return;
	}
	const float Scale = Canvas->ClipY / 1080.f;
	const FVector2D Centre(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);

	// North up: the view heading is -90 degrees (forward = -Y on the map data).
	GTPUI::FMapView View;
	const FVector Location = Controller->GetPawn()->GetActorLocation();
	View.Center = {static_cast<float>(Location.X), static_cast<float>(Location.Y)};
	View.HeadingRadians = -PI * 0.5f;
	// The disc only has to cover the screen; the canvas clips what is outside.
	View.RadiusPixels = 0.5f * FMath::Sqrt(Canvas->ClipX * Canvas->ClipX + Canvas->ClipY * Canvas->ClipY);
	View.WorldRadiusCm = View.RadiusPixels * FullMapHeightCm / Canvas->ClipY;

	FCanvasTileItem Backdrop(FVector2D::ZeroVector, FVector2D(Canvas->ClipX, Canvas->ClipY), FLinearColor(0.04f, 0.05f, 0.06f, 0.94f));
	Backdrop.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Backdrop);

	GTPUI::ForEachVisibleSegment(Data, View, [&](GTPUI::EMapKind Kind, float WidthM, const GTPUI::FVec2& A, const GTPUI::FVec2& B) {
		const FStyle Style = StyleFor(Kind, WidthM);
		DrawLine(Canvas, ToCanvas(Centre, A), ToCanvas(Centre, B), Style.Color, FMath::Max(1.f, Style.Thickness * Scale * 1.6f));
	});

	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
	if (Font)
	{
		for (const GTPUI::FMapLabel& Label : Labels)
		{
			const GTPUI::FVec2 P = View.ToScreen(Label.Position);
			const FVector2D At = Centre + FVector2D(P.X, P.Y);
			if (At.X < 40.f || At.Y < 40.f || At.X > Canvas->ClipX - 40.f || At.Y > Canvas->ClipY - 60.f)
			{
				continue;
			}
			FCanvasTextItem Text(At, FText::FromString(FString(UTF8_TO_TCHAR(Label.Text.c_str()))), Font, FLinearColor(0.92f, 0.92f, 0.88f, 0.95f));
			Text.Scale = FVector2D(0.7f * Scale);
			Text.bCentreX = true;
			Text.bCentreY = true;
			Text.BlendMode = SE_BLEND_Translucent;
			Text.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.9f), FVector2D(1.f, 1.f));
			Canvas->DrawItem(Text);
		}
		FCanvasTextItem Title(FVector2D(36.f * Scale, 28.f * Scale), GTPLoc("fullmap.title"), Font, FLinearColor::White);
		Title.Scale = FVector2D(1.2f * Scale);
		Title.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Title);
		FCanvasTextItem Hint(FVector2D(36.f * Scale, Canvas->ClipY - 48.f * Scale), GTPLoc("fullmap.hint"), Font, FLinearColor(0.8f, 0.8f, 0.78f));
		Hint.Scale = FVector2D(0.75f * Scale);
		Hint.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Hint);
	}

	// Player arrow, pointing where the player looks on the north-up map.
	const float Yaw = FMath::DegreesToRadians(Controller->GetControlRotation().Yaw);
	const GTPUI::FVec2 Ahead = View.ToScreen({View.Center.X + FMath::Cos(Yaw) * 1000.f, View.Center.Y + FMath::Sin(Yaw) * 1000.f});
	const FVector2D Dir = FVector2D(Ahead.X, Ahead.Y).GetSafeNormal();
	const FVector2D Side(-Dir.Y, Dir.X);
	const float Size = 11.f * Scale;
	FCanvasTriangleItem Pointer(Centre + Dir * Size * 1.4f, Centre - Dir * Size - Side * Size * 0.8f, Centre - Dir * Size + Side * Size * 0.8f, GWhiteTexture);
	Pointer.SetColor(FLinearColor(1.f, 0.62f, 0.1f, 1.f));
	Pointer.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Pointer);
}
