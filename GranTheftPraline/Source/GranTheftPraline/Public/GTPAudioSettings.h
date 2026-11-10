#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GTPAudioTypes.h"
#include "GTPAudioSettings.generated.h"

class USoundBase;

/**
 * Audio mix of the game (ADR-0009), section [/Script/GranTheftPraline.GTPAudioSettings]
 * of Config/DefaultGame.ini. Sound classes, mixes, concurrency and
 * attenuation are built in code at start-up from these values, so the mix
 * lives in Git and needs no binary asset. Sounds are found by folder:
 *   /Game/Sound/SFX/Footsteps/<Surface>/   steps (any name)
 *   /Game/Sound/SFX/Land/<Surface>/        landings after a jump or a fall
 *   /Game/Sound/SFX/Impacts/<Kind>/<Tier>/ vehicle hits, Tier = Light, Medium, Heavy
 *   /Game/Sound/SFX/Impacts/Glass/         glass breaking on heavy hits
 * (tools/audio/unreal/import_sounds.py fills them from the generated or recorded WAVs).
 */
UCLASS(Config = Game, defaultconfig, meta = (DisplayName = "GTP Audio"))
class GRANTHEFTPRALINE_API UGTPAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Root of the sound folders above. */
	UPROPERTY(Config, EditAnywhere, Category = "Banks")
	FString SoundRoot = TEXT("/Game/Sound/SFX");

	/** Ambience loop played by AGTPPlayerController: put in the Ambience class. */
	UPROPERTY(Config, EditAnywhere, Category = "Banks")
	TSoftObjectPtr<USoundBase> AmbienceSound;

	/**
	 * Folders whose sounds are silenced at start-up (in memory only), e.g. the
	 * Game Animation Sample foley if it doubles our footsteps (/Game/Audio/Foley).
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Banks")
	TArray<FString> MutedSoundFolders;

	/** Volume of each category, 0..1 (future options menu writes these). */
	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TMap<EGTPAudioCategory, float> CategoryVolumes;

	/** While the player drives: ambience level and its low-pass (Hz), fade (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	float DrivingAmbienceVolume = 0.6f;

	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	float DrivingAmbienceLowPass = 6000.f;

	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	float MixFadeTime = 0.8f;

	/** Most voices of each category at once; the farthest, then the oldest, is stopped. */
	UPROPERTY(Config, EditAnywhere, Category = "Budget")
	int32 MaxFootstepVoices = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Budget")
	int32 MaxImpactVoices = 6;

	UPROPERTY(Config, EditAnywhere, Category = "Budget")
	int32 MaxSirenVoices = 3;

	UPROPERTY(Config, EditAnywhere, Category = "Budget")
	int32 MaxSkidVoices = 4;

	/** Characters farther than this from the listener do not look for steps (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Budget")
	float FootstepMaxDistance = 2500.f;

	/** Attenuation: full level up to Inner, silent at Outer (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Attenuation")
	FVector2D FootstepRange = FVector2D(150.f, 2500.f);

	UPROPERTY(Config, EditAnywhere, Category = "Attenuation")
	FVector2D ImpactRange = FVector2D(400.f, 6000.f);

	UPROPERTY(Config, EditAnywhere, Category = "Attenuation")
	FVector2D SirenRange = FVector2D(1500.f, 40000.f);

	UPROPERTY(Config, EditAnywhere, Category = "Attenuation")
	FVector2D SkidRange = FVector2D(500.f, 5000.f);

	/** Surface grid of the focus street (tools/audio/surface_map.py), relative to Content/. */
	UPROPERTY(Config, EditAnywhere, Category = "Surfaces")
	FString SurfaceMapFile = TEXT("Sound/SurfaceMap/rue-de-la-republique.gtpsurf");

	/** A hit on a material whose name contains this reads the surface grid. */
	UPROPERTY(Config, EditAnywhere, Category = "Surfaces")
	FString GroundMaterialKeyword = TEXT("City_Ground");

	/** Surface when nothing else tells. */
	UPROPERTY(Config, EditAnywhere, Category = "Surfaces")
	EGTPSurface DefaultSurface = EGTPSurface::Stone;

	/** Physical surface names (Project Settings > Physics) mapped to our surfaces. */
	UPROPERTY(Config, EditAnywhere, Category = "Surfaces")
	TMap<FName, EGTPSurface> PhysicalSurfaceNames;

	/** Footstep detection: foot bones of the mannequin, plant height (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Footsteps")
	FName LeftFootBone = TEXT("foot_l");

	UPROPERTY(Config, EditAnywhere, Category = "Footsteps")
	FName RightFootBone = TEXT("foot_r");

	UPROPERTY(Config, EditAnywhere, Category = "Footsteps")
	float FootPlantHeight = 13.f;

	UPROPERTY(Config, EditAnywhere, Category = "Footsteps")
	float FootLiftHysteresis = 5.f;

	/** Step volume and pitch spread (± fraction). */
	UPROPERTY(Config, EditAnywhere, Category = "Footsteps")
	float FootstepVolume = 0.7f;

	UPROPERTY(Config, EditAnywhere, Category = "Footsteps")
	float FootstepPitchSpread = 0.05f;

	/** Falls slower than this make no landing sound (cm/s). */
	UPROPERTY(Config, EditAnywhere, Category = "Footsteps")
	float LandMinSpeed = 350.f;

	/** Impact thresholds: change of speed of the hit (cm/s), and gap between two hits (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicles")
	FVector ImpactTierSpeeds = FVector(120.f, 450.f, 1000.f);

	UPROPERTY(Config, EditAnywhere, Category = "Vehicles")
	float ImpactCooldown = 0.12f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicles")
	float ImpactVolume = 1.f;

	/** Tyre squeal: lateral slide speed for a full squeal (cm/s), level. */
	UPROPERTY(Config, EditAnywhere, Category = "Vehicles")
	float SkidFullSlipSpeed = 900.f;

	UPROPERTY(Config, EditAnywhere, Category = "Vehicles")
	float SkidVolume = 0.8f;

	UPROPERTY(Config, EditAnywhere, Category = "Sirens")
	float SirenVolume = 1.f;

	/** Actors with this tag get a siren component (police cars, ADR-0004). */
	UPROPERTY(Config, EditAnywhere, Category = "Sirens")
	FName SirenActorTag = TEXT("GTPPolice");

	/** Writes one line per second with the audio counters (soak tests). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bLogStats = false;
};
