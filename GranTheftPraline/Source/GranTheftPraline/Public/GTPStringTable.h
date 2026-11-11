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
	// Missions (converted from Content/Data/Missions/strings.tsv)
	{"mission.start.prompt", "Commencer : {0}", "Start: {0}"},
	{"mission.holdup.title", "Braquer la boutique", "Rob the shop"},
	{"mission.holdup.brief", "Entrez dans le fast-food, faites vider la caisse, puis semez la police.", "Walk into the fast-food restaurant, have the till emptied, then lose the police."},
	{"mission.order.title", "Voler une voiture sur commande", "Steal a car to order"},
	{"mission.order.brief", "Un client veut une berline bleue. Volez-la et livrez-la au garage sans l'abîmer.", "A client wants a blue sedan. Steal it and deliver it to the garage without wrecking it."},
	{"mission.flee.title", "Semer la police", "Lose the police"},
	{"mission.flee.brief", "Les policiers vous ont repéré. Échappez-leur, puis rejoignez le refuge sans être vu.", "The police have spotted you. Escape them, then reach the hideout unseen."},
	{"objective.holdup.enter", "Entrez dans le restaurant", "Go into the restaurant"},
	{"objective.holdup.take_cash", "Faites vider la caisse", "Get the till emptied"},
	{"objective.holdup.escape", "Semez la police", "Lose the police"},
	{"objective.order.steal", "Volez la berline bleue", "Steal the blue sedan"},
	{"objective.order.deliver", "Livrez la voiture au garage", "Deliver the car to the garage"},
	{"objective.flee.lose", "Échappez aux policiers", "Get away from the police"},
	{"objective.flee.reach", "Rejoignez le refuge sans être vu", "Reach the hideout unseen"},
	{"mission.result.success", "Mission accomplie : +{0} €", "Mission complete: +{0} €"},
	{"mission.result.failed", "Mission échouée", "Mission failed"},
	{"mission.fail.player.arrested", "Vous avez été arrêté", "You were arrested"},
	{"mission.fail.player.died", "Vous êtes mort", "You died"},
	{"mission.fail.vehicle.destroyed", "La voiture est détruite", "The car is destroyed"},
	{"mission.fail.vehicle_abandoned", "Vous avez abandonné la voiture", "You abandoned the car"},
	{"mission.fail.time", "Temps écoulé", "Time is up"},
	{"mission.retry", "Réessayer", "Retry"},
	{"mission.abandon", "Abandonner", "Give up"},
	{"mission.keys.start", "E / croix droite : commencer", "E / D-pad right: start"},
	{"mission.keys.retry", "Entrée / A : réessayer · Retour arrière / B : abandonner", "Enter / A: retry · Backspace / B: give up"},
	{"mission.keys.close", "Entrée / A : continuer", "Enter / A: continue"},
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
	{"fullmap.hint",
		"Flèches / stick : déplacer · Entrée / A : destination · Suppr / X : effacer · M / B : fermer",
		"Arrows / stick: move · Enter / A: destination · Del / X: clear · M / B: close"},
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
