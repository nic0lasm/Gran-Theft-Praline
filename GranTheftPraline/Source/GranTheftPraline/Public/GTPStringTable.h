#pragma once

// Text tables of the game, French and English side by side. No Unreal types in
// this header, so tools/ui/tests/cpp compiles and checks it with a plain C++
// compiler (same keys in both languages, same {n} placeholders, no duplicates).
//
// Adding a string: add one row to GTPUI::Entries with a dotted key, both
// translations, and read it in game with GTPLoc(TEXT("key")) or
// GTPLocFormat(TEXT("key"), Args) (GTPLocalization.h). Texts are UTF-8.
//
// Conventions: keys are "area.name"; {0}, {1} are placeholders filled by
// FText::Format; the French text is the reference, the English one is
// checked against it by the test (same placeholders).

#include <cstddef>
#include <cstring>

namespace GTPUI
{
enum class ELanguage
{
	French = 0,
	English = 1,
};

struct FStringEntry
{
	const char* Key;
	const char* French;
	const char* English;
};

inline constexpr FStringEntry Entries[] = {
	// Title screen and menus
	{"menu.title.heading", "GRAN THEFT PRALINE", "GRAN THEFT PRALINE"},
	{"menu.title.subtitle", "Lyon, Presqu'île · version {0}", "Lyon, Presqu'île · version {0}"},
	{"menu.title.subtitle_no_version", "Lyon, Presqu'île", "Lyon, Presqu'île"},
	{"menu.play", "Jouer", "Play"},
	{"menu.quit", "Quitter", "Quit"},
	{"menu.settings", "Réglages", "Settings"},
	{"menu.controls", "Commandes", "Controls"},
	{"menu.back", "Retour", "Back"},
	{"menu.pause.heading", "Pause", "Paused"},
	{"menu.pause.resume", "Reprendre", "Resume"},
	{"menu.pause.quit", "Quitter le jeu", "Quit game"},

	// Settings screen (each line is "label : value", the value is chosen by clicking)
	{"settings.heading", "Réglages", "Settings"},
	{"settings.language", "Langue : {0}", "Language: {0}"},
	{"settings.graphics", "Graphismes : {0}", "Graphics: {0}"},
	{"settings.volume", "Volume : {0} %", "Volume: {0}%"},
	{"settings.sensitivity", "Sensibilité de la vue : {0} %", "Look sensitivity: {0}%"},
	{"settings.invert_y", "Vue verticale inversée : {0}", "Invert vertical look: {0}"},
	{"settings.keyboard", "Clavier : {0}", "Keyboard: {0}"},
	{"settings.minimap", "Mini-carte : {0}", "Minimap: {0}"},
	{"settings.minimap_zoom", "Zoom de la mini-carte : {0}", "Minimap zoom: {0}"},
	{"settings.hint", "Cliquer sur une ligne change sa valeur. Les réglages sont enregistrés.",
		"Click a line to change its value. Settings are saved."},

	// Values
	{"value.on", "Oui", "On"},
	{"value.off", "Non", "Off"},
	{"value.lang.fr", "Français", "Français"},
	{"value.lang.en", "English", "English"},
	{"value.gfx.m1", "Fluide (M1)", "Smooth (M1)"},
	{"value.gfx.fidelity", "Fidélité", "Fidelity"},
	{"value.gfx.ultra", "Ultra", "Ultra"},
	{"value.kb.auto", "Automatique", "Automatic"},
	{"value.kb.azerty", "AZERTY", "AZERTY"},
	{"value.kb.qwerty", "QWERTY", "QWERTY"},
	{"value.zoom.near", "Proche", "Near"},
	{"value.zoom.medium", "Moyen", "Medium"},
	{"value.zoom.far", "Loin", "Far"},

	// HUD, shared with the other HUD elements
	{"prompt.enter_vehicle", "Monter", "Get in"},
	{"prompt.exit_vehicle", "Descendre", "Get out"},
	{"hud.reloading", "Rechargement…", "Reloading…"},
	{"Weapon.Pistol9", "Pistolet 9 mm", "9 mm pistol"},
	{"Weapon.Fist", "Poings", "Fists"},
	{"minimap.north", "N", "N"},
	{"controls.heading", "Commandes", "Controls"},
	{"controls.interact", "Interagir", "Interact"},
	{"controls.enter_exit", "Monter / descendre", "Get in / out"},
	{"controls.camera", "Changer de vue", "Change view"},
	{"controls.recenter", "Recentrer la caméra", "Recentre the camera"},
	{"controls.throttle", "Accélérer", "Accelerate"},
	{"controls.brake", "Freiner / marche arrière", "Brake / reverse"},
	{"controls.steer", "Tourner", "Steer"},
	{"controls.handbrake", "Frein à main", "Handbrake"},
	{"controls.look_behind", "Regarder derrière", "Look behind"},
	{"controls.map", "Carte", "Map"},
	{"controls.pause", "Pause", "Pause"},
	{"controls.keyboard_header", "Clavier ({0})", "Keyboard ({0})"},
	{"controls.gamepad_header", "Manette", "Gamepad"},
	{"fullmap.title", "Carte de Lyon", "Map of Lyon"},
	{"fullmap.hint", "M ou croix haut : fermer la carte", "M or D-pad up: close the map"},
};

inline constexpr std::size_t EntryCount = sizeof(Entries) / sizeof(Entries[0]);

inline const FStringEntry* FindEntry(const char* Key)
{
	for (const FStringEntry& Entry : Entries)
	{
		if (std::strcmp(Entry.Key, Key) == 0)
		{
			return &Entry;
		}
	}
	return nullptr;
}

/** Text of a key in a language; the key itself when it is unknown, so a typo is visible on screen. */
inline const char* Find(const char* Key, ELanguage Language)
{
	const FStringEntry* Entry = FindEntry(Key);
	if (!Entry)
	{
		return Key;
	}
	const char* Text = Language == ELanguage::English ? Entry->English : Entry->French;
	if (Text && Text[0] != '\0')
	{
		return Text;
	}
	// Missing translation: fall back to the other language.
	const char* Other = Language == ELanguage::English ? Entry->French : Entry->English;
	return (Other && Other[0] != '\0') ? Other : Key;
}

/** Language from a code like "fr", "fr-FR", "en", "en_GB"; French when unknown. */
inline ELanguage LanguageFromCode(const char* Code)
{
	if (Code && (Code[0] == 'e' || Code[0] == 'E') && (Code[1] == 'n' || Code[1] == 'N'))
	{
		return ELanguage::English;
	}
	return ELanguage::French;
}

inline const char* LanguageCode(ELanguage Language)
{
	return Language == ELanguage::English ? "en" : "fr";
}
} // namespace GTPUI
