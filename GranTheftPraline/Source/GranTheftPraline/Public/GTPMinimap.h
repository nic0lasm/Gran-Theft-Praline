#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPGpsCore.h"
#include "GTPMinimapCore.h"
#include "GTPMinimap.generated.h"

class AGTPPlayerController;
class UCanvas;

/**
 * Round mini-map at the bottom left of the screen (docs/ui/minimap.md): roads,
 * paths, water and building outlines of the street around the player, turning
 * with the camera so "up" is where the player looks, a north mark on the ring,
 * the player's arrow in the middle, and blips for what other systems register.
 *
 * Data: Content/City/Minimap/<Map>.minimap.csv, written by tools/ui/minimap.py
 * from the same BD TOPO tables as the city (derived data, not committed). No
 * file, no mini-map. Staged with the game (DefaultGame.ini).
 *
 * Drawn on the canvas from AGTPHUD::DrawHUD with one call, Draw(); cost is one
 * line item per visible segment (a few hundred), measured in docs/ui/minimap.md.
 * -Minimap=0 (or -NoMinimap) on the command line switches it off (A/B measurements).
 * The player can switch it off and pick its zoom in the settings screen.
 *
 * Blips (police cars, pedestrians, targets): AddBlip(Actor, Colour) from any
 * system, RemoveBlip when done. Blips are drawn as dots; with bClampToEdge
 * they stay on the ring when out of range (for a destination).
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPMinimapSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Draws the mini-map; does nothing when disabled in the settings or when the map has no data. */
	void Draw(UCanvas* Canvas, const AGTPPlayerController* Controller, float Scale);

	/**
	 * Full-screen map: the same data north up, centred on the player, a few
	 * hundred metres tall, with the street names. Opened by M (gamepad View
	 * button) through AGTPPlayerController::ToggleFullMap; the game is paused.
	 */
	void DrawFullMap(UCanvas* Canvas, const AGTPPlayerController* Controller);

	/**
	 * GPS (roadmap 0.9): a destination set on the full-screen map (cursor at the
	 * centre of the screen, moved with the arrow keys or the left stick, Enter or
	 * gamepad A to set, Delete or X to clear). The route follows the road lines
	 * of the map data (two-way for now), is drawn on both maps and is computed
	 * again when the player leaves it by more than 40 m. Reaching the destination
	 * (25 m) clears it. -Gps=0 on the command line turns it off.
	 */
	void SetDestination(const FVector2D& WorldCm, const FVector& PlayerLocation);
	void SetDestinationAtFullMapCursor(const FVector& PlayerLocation);
	void ClearDestination();
	bool HasDestination() const { return bHasDestination; }
	static bool IsGpsDisabledOnCommandLine();

	/** Centres the full-screen map on the player again (called when it opens). */
	void ResetFullMapView() { FullMapOffset = FVector2D::ZeroVector; }

	void AddBlip(AActor* Actor, const FLinearColor& Color, bool bClampToEdge = false);
	void RemoveBlip(AActor* Actor);

	/** True with -Minimap=0 or -NoMinimap: the subsystem is not created. */
	static bool IsDisabledOnCommandLine();

	/**
	 * Fixed markers from other systems (mission starts, activities): replace all the
	 * markers of one source. Drawn as diamonds on the mini-map (on the ring when out
	 * of range) and on the full map. Cheap: the caller refreshes them a few times a second.
	 */
	struct FMapMarker
	{
		FVector2D WorldCm = FVector2D::ZeroVector;
		FLinearColor Color = FLinearColor::White;
		bool bClampToEdge = false;
	};
	void SetMarkers(FName Source, const TArray<FMapMarker>& Markers);

	/** Where the mini-map disc sits on a canvas (centre, radius in pixels): the combat HUD draws around it. */
	static void GetLayout(const UCanvas* Canvas, float Scale, FVector2D& OutCentre, float& OutRadius);

	bool HasData() const { return Data.Lines.size() > 0; }

private:
	void LoadData();
	/** Keeps the route current: arrival, leaving the route, first computation. */
	void UpdateGps(const FVector& PlayerLocation);
	void ComputeRoute(const FVector& From);
	void DrawRouteInDisc(UCanvas* Canvas, const FVector2D& Centre, const GTPUI::FMapView& View, float Scale) const;

	TMap<FName, TArray<FMapMarker>> MarkersBySource;
	void DrawMarkers(UCanvas* Canvas, const FVector2D& Centre, const GTPUI::FMapView& View, float Radius, float Scale, bool bFullMap) const;

	GTPUI::FGpsGraph Gps;
	TArray<GTPUI::FVec2> Route;
	FVector2D Destination = FVector2D::ZeroVector;
	bool bHasDestination = false;
	double LastRouteCheckTime = 0.0;
	FVector2D FullMapOffset = FVector2D::ZeroVector; // world cm, from the player

	struct FBlip
	{
		TWeakObjectPtr<AActor> Actor;
		FLinearColor Color;
		bool bClampToEdge = false;
	};

	GTPUI::FMapData Data;
	TArray<GTPUI::FMapLabel> Labels;
	TArray<FBlip> Blips;
	bool bLoaded = false;

	/** Disc radius in pixels at 1080 lines, and its distance to the screen corner. */
	/** Height of the full-screen map in world cm (700 m). */
	static constexpr float FullMapHeightCm = 70000.f;

	static constexpr float RadiusAt1080 = 120.f;
	static constexpr float MarginAt1080 = 36.f;
};
