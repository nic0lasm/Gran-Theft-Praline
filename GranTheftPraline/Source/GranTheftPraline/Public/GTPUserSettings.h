#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPSettingsCore.h"
#include "GTPUserSettings.generated.h"

/**
 * The player's settings, saved between runs (docs/ui/settings.md):
 * language, graphics profile, volume, look sensitivity, vertical inversion,
 * keyboard layout, mini-map. The values and the file format live in
 * GTPSettingsCore.h (tested outside Unreal); this subsystem loads and saves
 * Saved/Config/GTPSettings.ini and applies each value:
 *
 *   language    GTPSetLanguage (every text read through GTPLoc)
 *   graphics    UGTPGraphicsProfile::SetProfile (the profiles of ADR-0002,
 *               owned by the optimisation work: this only picks one by name)
 *   volume      FApp::SetVolumeMultiplier (all game audio)
 *   sensitivity, inversion, keyboard   read by AGTPPlayerController
 *   mini-map    read by UGTPMinimapSubsystem
 *
 * -GfxProfile on the command line still wins at start-up.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPUserSettings : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const GTPUI::FUserSettings& Get() const { return Settings; }

	/** True when a saved file was read at start (else the project defaults apply). */
	bool HasSavedFile() const { return bLoadedFromFile; }

	/** Changes the settings, applies what changed and saves. */
	void Modify(TFunctionRef<void(GTPUI::FUserSettings&)> Change);

	/** Called after every change (controller re-applies the keyboard, HUD redraws). */
	FSimpleMulticastDelegate OnChanged;

	/** Settings of the local game instance; nullptr outside a game. */
	static UGTPUserSettings* Find(const UObject* WorldContext);

	static FString GetSavePath();

private:
	void Load();
	void Save() const;
	void ApplyLanguageAndVolume() const;

	GTPUI::FUserSettings Settings;
	bool bLoadedFromFile = false;
};
