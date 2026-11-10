#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
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

	void AddBlip(AActor* Actor, const FLinearColor& Color, bool bClampToEdge = false);
	void RemoveBlip(AActor* Actor);

	bool HasData() const { return Data.Lines.size() > 0; }

private:
	void LoadData();

	struct FBlip
	{
		TWeakObjectPtr<AActor> Actor;
		FLinearColor Color;
		bool bClampToEdge = false;
	};

	GTPUI::FMapData Data;
	TArray<FBlip> Blips;
	bool bLoaded = false;

	/** Disc radius in pixels at 1080 lines, and its distance to the screen corner. */
	static constexpr float RadiusAt1080 = 120.f;
	static constexpr float MarginAt1080 = 36.f;
};
