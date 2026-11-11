#pragma once

#include "CoreMinimal.h"

class UCanvas;
class APlayerController;
struct FKey;

/**
 * Missions on the interface (ADR-0014; the mission systems are in GTPMissionSubsystem.h).
 * Draws only what the mission subsystem exposes, holds no mission logic:
 *  - start markers on the mini-map and the full map (yellow when available, grey when not),
 *    refreshed twice a second; the current waypoint (light blue), and the GPS route to it;
 *  - the current objective hint at the top centre;
 *  - near a start marker: "E Commencer : <title>" at the bottom centre; E / D-pad right starts;
 *  - the result card: success with the reward, or failure with the reason and
 *    Retry / Give up (Enter or A / Backspace or B).
 * Compiles to nothing when GTPMissionSubsystem.h is absent, and does nothing with -Missions=0.
 */
namespace GTPHudMissions
{
	/** Once per HUD frame, before the maps draw: refreshes markers, waypoint and GPS. */
	void Update(const UWorld* World, const APlayerController* Controller);

	void Draw(UCanvas* Canvas, const APlayerController* Controller, float Scale);

	/** Key pressed: true when the missions took it (start, retry, give up, continue). */
	bool HandleKey(APlayerController* Controller, const FKey& Key);
}
