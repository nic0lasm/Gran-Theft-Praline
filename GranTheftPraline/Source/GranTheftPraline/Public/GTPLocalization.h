#pragma once

#include "CoreMinimal.h"
#include "GTPStringTable.h"

/**
 * Text lookup in the game's string tables (GTPStringTable.h, French and English).
 * The language is the player's setting (UGTPUserSettings); French until it is loaded.
 *
 *   GTPLoc("menu.play")                              -> "Jouer" / "Play"
 *   GTPLocFormat("settings.volume", {FText::AsNumber(40)})
 *
 * An unknown key comes back as the key itself, so a typo shows on screen.
 */
GRANTHEFTPRALINE_API FText GTPLoc(const char* Key);
GRANTHEFTPRALINE_API FText GTPLocFormat(const char* Key, const FFormatOrderedArguments& Args);
GRANTHEFTPRALINE_API FString GTPLocString(const char* Key);

GRANTHEFTPRALINE_API GTPUI::ELanguage GTPGetLanguage();
GRANTHEFTPRALINE_API void GTPSetLanguage(GTPUI::ELanguage Language);
