#pragma once

// Player settings without any Unreal type: values, limits, the save format and
// the step logic of the settings screen. Checked by tools/ui/tests/cpp.
// UGTPUserSettings (GTPUserSettings.h) stores and applies them in the game.

#include "GTPStringTable.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <string_view>

namespace GTPUI
{
enum class EKeyboardLayout
{
	Auto = 0, // DefaultGame.ini (AZERTY) or -Qwerty on the command line
	Azerty = 1,
	Qwerty = 2,
};

enum class EMinimapZoom
{
	Near = 0,
	Medium = 1,
	Far = 2,
};

/** Rendering profiles of ADR-0002, in the order the settings screen offers them. */
inline constexpr const char* GraphicsProfiles[] = {"m1", "fidelity", "ultra"};
inline constexpr int GraphicsProfileCount = 3;

struct FUserSettings
{
	ELanguage Language = ELanguage::French;
	std::string Graphics = "m1";
	int VolumePercent = 100;      // 0..100, steps of 10
	int SensitivityPercent = 100; // 25..200, steps of 25
	bool bInvertY = false;
	EKeyboardLayout Keyboard = EKeyboardLayout::Auto;
	bool bMinimap = true;
	EMinimapZoom MinimapZoom = EMinimapZoom::Medium;

	static constexpr int VolumeStep = 10;
	static constexpr int SensitivityMin = 25;
	static constexpr int SensitivityMax = 200;
	static constexpr int SensitivityStep = 25;

	/** Brings every value back into its range (a hand-edited file must not break the game). */
	void Clamp()
	{
		VolumePercent = std::clamp(VolumePercent, 0, 100);
		SensitivityPercent = std::clamp(SensitivityPercent, SensitivityMin, SensitivityMax);
		bool bKnown = false;
		for (const char* Profile : GraphicsProfiles)
		{
			bKnown = bKnown || Graphics == Profile;
		}
		if (!bKnown)
		{
			Graphics = "m1";
		}
	}

	float VolumeMultiplier() const { return static_cast<float>(VolumePercent) / 100.f; }
	float SensitivityMultiplier() const { return static_cast<float>(SensitivityPercent) / 100.f; }

	// Steps used by the settings screen: a click goes to the next value and wraps around.
	void CycleLanguage() { Language = Language == ELanguage::French ? ELanguage::English : ELanguage::French; }

	void CycleGraphics()
	{
		int Index = 0;
		for (int i = 0; i < GraphicsProfileCount; ++i)
		{
			if (Graphics == GraphicsProfiles[i])
			{
				Index = i;
			}
		}
		Graphics = GraphicsProfiles[(Index + 1) % GraphicsProfileCount];
	}

	void CycleVolume() { VolumePercent = VolumePercent >= 100 ? 0 : std::min(100, VolumePercent + VolumeStep); }

	void CycleSensitivity()
	{
		SensitivityPercent = SensitivityPercent >= SensitivityMax ? SensitivityMin : SensitivityPercent + SensitivityStep;
	}

	void CycleKeyboard() { Keyboard = static_cast<EKeyboardLayout>((static_cast<int>(Keyboard) + 1) % 3); }
	void CycleMinimapZoom() { MinimapZoom = static_cast<EMinimapZoom>((static_cast<int>(MinimapZoom) + 1) % 3); }

	bool operator==(const FUserSettings& Other) const
	{
		return Language == Other.Language && Graphics == Other.Graphics && VolumePercent == Other.VolumePercent
			&& SensitivityPercent == Other.SensitivityPercent && bInvertY == Other.bInvertY
			&& Keyboard == Other.Keyboard && bMinimap == Other.bMinimap && MinimapZoom == Other.MinimapZoom;
	}
};

/** Save format: one "key=value" per line, '#' starts a comment. */
inline std::string Serialize(const FUserSettings& S)
{
	std::string Out = "# Gran Theft Praline settings\n";
	Out += std::string("language=") + LanguageCode(S.Language) + "\n";
	Out += "graphics=" + S.Graphics + "\n";
	Out += "volume=" + std::to_string(S.VolumePercent) + "\n";
	Out += "sensitivity=" + std::to_string(S.SensitivityPercent) + "\n";
	Out += std::string("invert_y=") + (S.bInvertY ? "1" : "0") + "\n";
	Out += "keyboard=" + std::to_string(static_cast<int>(S.Keyboard)) + "\n";
	Out += std::string("minimap=") + (S.bMinimap ? "1" : "0") + "\n";
	Out += "minimap_zoom=" + std::to_string(static_cast<int>(S.MinimapZoom)) + "\n";
	return Out;
}

namespace Detail
{
inline std::string_view Trim(std::string_view S)
{
	while (!S.empty() && (S.front() == ' ' || S.front() == '\t' || S.front() == '\r'))
	{
		S.remove_prefix(1);
	}
	while (!S.empty() && (S.back() == ' ' || S.back() == '\t' || S.back() == '\r'))
	{
		S.remove_suffix(1);
	}
	return S;
}

inline bool ParseInt(std::string_view S, int& Out)
{
	if (S.empty())
	{
		return false;
	}
	const std::string Text(S);
	char* End = nullptr;
	const long Value = std::strtol(Text.c_str(), &End, 10);
	if (End == Text.c_str() || *End != '\0')
	{
		return false;
	}
	Out = static_cast<int>(std::clamp<long>(Value, -100000, 100000));
	return true;
}
} // namespace Detail

/** Reads a saved file on top of Defaults: unknown keys and bad values are ignored, the result is clamped. */
inline FUserSettings Parse(std::string_view Text, FUserSettings Defaults = FUserSettings())
{
	FUserSettings S = Defaults;
	while (!Text.empty())
	{
		const std::size_t Newline = Text.find('\n');
		std::string_view Line = Detail::Trim(Text.substr(0, Newline));
		Text = Newline == std::string_view::npos ? std::string_view() : Text.substr(Newline + 1);
		if (Line.empty() || Line.front() == '#')
		{
			continue;
		}
		const std::size_t Equals = Line.find('=');
		if (Equals == std::string_view::npos)
		{
			continue;
		}
		const std::string_view Key = Detail::Trim(Line.substr(0, Equals));
		const std::string_view Value = Detail::Trim(Line.substr(Equals + 1));
		int Number = 0;
		if (Key == "language")
		{
			S.Language = LanguageFromCode(std::string(Value).c_str());
		}
		else if (Key == "graphics")
		{
			S.Graphics = std::string(Value);
		}
		else if (Key == "volume" && Detail::ParseInt(Value, Number))
		{
			S.VolumePercent = Number;
		}
		else if (Key == "sensitivity" && Detail::ParseInt(Value, Number))
		{
			S.SensitivityPercent = Number;
		}
		else if (Key == "invert_y" && Detail::ParseInt(Value, Number))
		{
			S.bInvertY = Number != 0;
		}
		else if (Key == "keyboard" && Detail::ParseInt(Value, Number) && Number >= 0 && Number <= 2)
		{
			S.Keyboard = static_cast<EKeyboardLayout>(Number);
		}
		else if (Key == "minimap" && Detail::ParseInt(Value, Number))
		{
			S.bMinimap = Number != 0;
		}
		else if (Key == "minimap_zoom" && Detail::ParseInt(Value, Number) && Number >= 0 && Number <= 2)
		{
			S.MinimapZoom = static_cast<EMinimapZoom>(Number);
		}
	}
	S.Clamp();
	return S;
}
} // namespace GTPUI
