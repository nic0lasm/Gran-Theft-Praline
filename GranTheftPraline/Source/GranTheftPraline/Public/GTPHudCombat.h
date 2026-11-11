#pragma once

#include "CoreMinimal.h"

class UCanvas;
class APlayerController;

/**
 * Combat part of the HUD (spec: docs/gameplay/combat.md, "HUD"). Draws only; it
 * reads what the combat systems expose (UGTPHealthComponent, UGTPWeaponComponent,
 * UGTPCombatSubsystem) and holds no game logic. Nothing is drawn with -Combat=0.
 *
 *  - reticle: a ring whose radius is the weapon's spread cone, with a centre dot;
 *  - hit marker: a cross at the centre for 0.15 s, red on a kill;
 *  - health and armour: two thin arcs around the mini-map, health colour from green to red;
 *  - weapon line beside the mini-map: name, magazine / reserve, "reloading".
 * Returns true when it drew the reticle (the plain dot of AGTPHUD is then skipped).
 */
namespace GTPHudCombat
{
	bool Draw(UCanvas* Canvas, const APlayerController* Controller, float Scale);
}
