#pragma once

// What the controls screen lists: one row per action with the keys of each
// device. No Unreal types; the test checks every action name is in the string
// table. Only bindings that exist in code are listed (GTPVehicleInput.cpp,
// the on-foot contexts); the table is updated with them.

#include <cstddef>

namespace GTPUI
{
struct FControlRow
{
	const char* ActionKey; // string table key
	const char* Azerty;
	const char* Qwerty;
	const char* Gamepad;
};

inline constexpr FControlRow ControlRows[] = {
	{"controls.interact", "E", "E", "D-pad \xE2\x86\x92"},
	{"controls.enter_exit", "F", "F", "Y"},
	{"controls.camera", "V", "V", "View"},
	{"controls.recenter", "Clic molette / Middle click", "Clic molette / Middle click", "-"},
	{"controls.throttle", "Z / \xE2\x86\x91", "W / \xE2\x86\x91", "RT"},
	{"controls.brake", "S / \xE2\x86\x93", "S / \xE2\x86\x93", "LT"},
	{"controls.steer", "Q D / \xE2\x86\x90 \xE2\x86\x92", "A D / \xE2\x86\x90 \xE2\x86\x92", "L"},
	{"controls.handbrake", "Espace / Space", "Espace / Space", "RB"},
	{"controls.look_behind", "-", "-", "R3"},
	{"controls.map", "M", "M", "D-pad \xE2\x86\x91"},
	{"controls.pause", "Echap / Esc", "Echap / Esc", "Menu"},
};

inline constexpr std::size_t ControlRowCount = sizeof(ControlRows) / sizeof(ControlRows[0]);
} // namespace GTPUI
