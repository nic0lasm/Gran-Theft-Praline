#include "GTPHudMissions.h"

#include "GTPLocalization.h"
#include "GTPMinimap.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

#if __has_include("GTPMissionSubsystem.h")
#define GTP_HAS_MISSIONS 1
#include "GTPMissionSubsystem.h"
#else
#define GTP_HAS_MISSIONS 0
#endif

#if GTP_HAS_MISSIONS

namespace
{
	TArray<UGTPMissionSubsystem::FStartMarker> CachedMarkers;
	double LastRefresh = -1.0;
	FName Offered = NAME_None;
	FVector LastWaypoint = FVector(TNumericLimits<double>::Max());
	bool bGpsFromMission = false;

	FString Loc(const FName& Key)
	{
		const FTCHARToUTF8 Utf8(*Key.ToString());
		return GTPLocString(Utf8.Get());
	}

	void PutText(UCanvas* Canvas, const FString& Value, const FVector2D& At, float Scale, const FLinearColor& Color, bool bCentre)
	{
		const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
		if (!Font)
		{
			return;
		}
		FCanvasTextItem Item(At, FText::FromString(Value), Font, Color);
		Item.Scale = FVector2D(Scale);
		Item.bCentreX = bCentre;
		Item.BlendMode = SE_BLEND_Translucent;
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.9f), FVector2D(1.f, 1.f));
		Canvas->DrawItem(Item);
	}

	void DrawPanel(UCanvas* Canvas, const FVector2D& Min, const FVector2D& Size)
	{
		FCanvasTileItem Tile(Min, Size, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	}
}

void GTPHudMissions::Update(const UWorld* World, const APlayerController* Controller)
{
	UGTPMissionSubsystem* Missions = UGTPMissionSubsystem::Get(World);
	UGTPMinimapSubsystem* Minimap = World ? World->GetSubsystem<UGTPMinimapSubsystem>() : nullptr;
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Missions || !Minimap || !Pawn)
	{
		return;
	}
	const FVector Player = Pawn->GetActorLocation();

	// Start markers: refreshed twice a second (GetStartMarkers allocates).
	const double Now = FPlatformTime::Seconds();
	if (Now - LastRefresh >= 0.5)
	{
		LastRefresh = Now;
		CachedMarkers = Missions->GetStartMarkers();
		TArray<UGTPMinimapSubsystem::FMapMarker> Markers;
		Offered = NAME_None;
		const bool bFree = !Missions->HasActiveMission() && !Missions->HasResult();
		for (const UGTPMissionSubsystem::FStartMarker& M : CachedMarkers)
		{
			UGTPMinimapSubsystem::FMapMarker Marker;
			Marker.WorldCm = FVector2D(M.LocationCm.X, M.LocationCm.Y);
			Marker.Color = M.bAvailable ? FLinearColor(1.f, 0.85f, 0.1f) : FLinearColor(0.5f, 0.5f, 0.5f);
			Marker.bClampToEdge = false;
			Markers.Add(Marker);
			if (bFree && M.bAvailable && FVector::Dist2D(Player, M.LocationCm) <= M.RadiusCm)
			{
				Offered = M.MissionId;
			}
		}
		Minimap->SetMarkers(TEXT("Missions.Start"), Markers);

		// Waypoint: a marker on the maps and the GPS destination (the player's own destination is left alone).
		FVector Waypoint;
		float Radius = 0.f;
		if (Missions->HasActiveMission() && Missions->GetWaypoint(Waypoint, Radius))
		{
			TArray<UGTPMinimapSubsystem::FMapMarker> Mark;
			UGTPMinimapSubsystem::FMapMarker Marker;
			Marker.WorldCm = FVector2D(Waypoint.X, Waypoint.Y);
			Marker.Color = FLinearColor(0.3f, 0.85f, 1.f);
			Marker.bClampToEdge = true;
			Mark.Add(Marker);
			Minimap->SetMarkers(TEXT("Missions.Waypoint"), Mark);
			if (FVector::Dist2D(Waypoint, LastWaypoint) > 100.0)
			{
				LastWaypoint = Waypoint;
				bGpsFromMission = true;
				Minimap->SetDestination(FVector2D(Waypoint.X, Waypoint.Y), Player);
			}
		}
		else
		{
			Minimap->SetMarkers(TEXT("Missions.Waypoint"), TArray<UGTPMinimapSubsystem::FMapMarker>());
			LastWaypoint = FVector(TNumericLimits<double>::Max());
			if (bGpsFromMission)
			{
				bGpsFromMission = false;
				Minimap->ClearDestination();
			}
		}
	}
}

void GTPHudMissions::Draw(UCanvas* Canvas, const APlayerController* Controller, float Scale)
{
	const UGTPMissionSubsystem* Missions = Canvas && Controller ? UGTPMissionSubsystem::Get(Controller) : nullptr;
	if (!Missions)
	{
		return;
	}
	const float CentreX = Canvas->ClipX * 0.5f;

	if (Missions->HasResult())
	{
		// Result card in the middle of the screen.
		const bool bSuccess = Missions->ResultIsSuccess();
		const FVector2D Size(560.f * Scale, 190.f * Scale);
		const FVector2D Min(CentreX - Size.X * 0.5f, Canvas->ClipY * 0.5f - Size.Y * 0.5f - 60.f * Scale);
		DrawPanel(Canvas, Min, Size);
		FString Title;
		FString Detail;
		if (bSuccess)
		{
			Title = GTPLocFormat("mission.result.success", {FText::AsNumber(Missions->GetResultReward())}).ToString();
		}
		else
		{
			Title = GTPLocString("mission.result.failed");
			Detail = Loc(FName(*(FString(TEXT("mission.fail.")) + Missions->GetResultReason().ToString())));
		}
		PutText(Canvas, Title, FVector2D(CentreX, Min.Y + 22.f * Scale), 1.5f * Scale,
			bSuccess ? FLinearColor(0.45f, 0.9f, 0.45f) : FLinearColor(0.95f, 0.35f, 0.3f), true);
		if (!Detail.IsEmpty())
		{
			PutText(Canvas, Detail, FVector2D(CentreX, Min.Y + 78.f * Scale), 1.f * Scale, FLinearColor::White, true);
		}
		PutText(Canvas, GTPLocString(bSuccess ? "mission.keys.close" : "mission.keys.retry"),
			FVector2D(CentreX, Min.Y + 138.f * Scale), 0.8f * Scale, FLinearColor(0.85f, 0.85f, 0.8f), true);
		return;
	}

	if (Missions->HasActiveMission())
	{
		const FName Hint = Missions->GetCurrentHintKey();
		if (!Hint.IsNone())
		{
			const FString Text = Loc(Hint);
			const FVector2D Size(FMath::Max(360.f, Text.Len() * 15.f) * Scale, 40.f * Scale);
			DrawPanel(Canvas, FVector2D(CentreX - Size.X * 0.5f, 24.f * Scale), Size);
			PutText(Canvas, Text, FVector2D(CentreX, 31.f * Scale), 1.f * Scale, FLinearColor(1.f, 0.92f, 0.5f), true);
		}
		return;
	}

	if (!Offered.IsNone())
	{
		FString Title;
		for (const UGTPMissionSubsystem::FStartMarker& M : CachedMarkers)
		{
			if (M.MissionId == Offered)
			{
				Title = Loc(M.TitleKey);
			}
		}
		const FString Line = GTPLocFormat("mission.start.prompt", {FText::FromString(Title)}).ToString();
		const FVector2D Size(FMath::Max(420.f, Line.Len() * 15.f) * Scale, 64.f * Scale);
		const FVector2D Min(CentreX - Size.X * 0.5f, Canvas->ClipY - 190.f * Scale);
		DrawPanel(Canvas, Min, Size);
		PutText(Canvas, Line, FVector2D(CentreX, Min.Y + 8.f * Scale), 1.f * Scale, FLinearColor::White, true);
		PutText(Canvas, GTPLocString("mission.keys.start"), FVector2D(CentreX, Min.Y + 36.f * Scale), 0.7f * Scale, FLinearColor(0.85f, 0.85f, 0.8f), true);
	}
}

bool GTPHudMissions::HandleKey(APlayerController* Controller, const FKey& Key)
{
	UGTPMissionSubsystem* Missions = Controller ? UGTPMissionSubsystem::Get(Controller) : nullptr;
	if (!Missions)
	{
		return false;
	}
	const bool bConfirm = Key == EKeys::Enter || Key == EKeys::Gamepad_FaceButton_Bottom;
	const bool bCancel = Key == EKeys::BackSpace || Key == EKeys::Gamepad_FaceButton_Right;
	if (Missions->HasResult())
	{
		if (Missions->ResultIsSuccess())
		{
			if (bConfirm || bCancel)
			{
				Missions->AbortMission(); // clears the result
				return true;
			}
		}
		else if (bConfirm)
		{
			Missions->RetryMission();
			return true;
		}
		else if (bCancel)
		{
			Missions->AbortMission();
			return true;
		}
		return false;
	}
	if (!Offered.IsNone() && !Missions->HasActiveMission() && (Key == EKeys::E || Key == EKeys::Gamepad_DPad_Right))
	{
		const FName Mission = Offered;
		Offered = NAME_None;
		return Missions->StartMission(Mission);
	}
	return false;
}

#else // no mission subsystem in this build

void GTPHudMissions::Update(const UWorld*, const APlayerController*) {}
void GTPHudMissions::Draw(UCanvas*, const APlayerController*, float) {}
bool GTPHudMissions::HandleKey(APlayerController*, const FKey&) { return false; }

#endif
