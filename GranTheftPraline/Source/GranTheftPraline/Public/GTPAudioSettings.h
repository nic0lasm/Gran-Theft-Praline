#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GTPAudioTypes.h"
#include "GTPAudioSettings.generated.h"

class USoundBase;

/**
 * Volume of one mixing category. A list of structs (+CategoryVolumes=(Category=...,Volume=...)
 * lines), not a TMap: the 0.3.0 cook rejected the last "+Key=(Name, Value)" line of a map.
 */
USTRUCT()
struct FGTPCategoryVolume
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mix")
	EGTPAudioCategory Category = EGTPAudioCategory::Master;

	UPROPERTY(EditAnywhere, Category = "Mix", meta = (ClampMin = "0", ClampMax = "1"))
	float Volume = 1.f;
};

/**
 * Engine sound of a car model (GTPEngineSynth.h), chosen by a word of the car
 * class name (BP_Renault_ClioV -> "ClioV"). The Chaos rpm range of the car is
 * mapped onto IdleRpm..MaxRpm, so a diesel sounds like one even when its
 * physics share the petrol curve.
 */
USTRUCT()
struct FGTPEngineProfile
{
	GENERATED_BODY()

	/** Word looked for in the car class name; "" = every other car. */
	UPROPERTY(EditAnywhere, Category = "Engine")
	FString Match;

	/** For the log and the docs, e.g. "Renault TCe 90, 3 cylinders". */
	UPROPERTY(EditAnywhere, Category = "Engine")
	FString Label;

	UPROPERTY(EditAnywhere, Category = "Engine")
	int32 Cylinders = 3;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float IdleRpm = 850.f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float MaxRpm = 6000.f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float IdleGain = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float FullLoadGain = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float NoiseAmount = 0.18f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float Jitter = 0.12f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float RingHarmonic = 4.f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float Diesel = 0.f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float Turbo = 0.f;

	UPROPERTY(EditAnywhere, Category = "Engine")
	float TurboPitchHz = 5200.f;

	/** Throttle cut while the gearbox changes gear (s): the drop you hear between gears. */
	UPROPERTY(EditAnywhere, Category = "Engine")
	float ShiftCutSeconds = 0.2f;
};

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

	/** Volume of each category, 0..1; a category left out plays at 1 (future options menu writes these). */
	UPROPERTY(Config, EditAnywhere, Category = "Mix")
	TArray<FGTPCategoryVolume> CategoryVolumes;

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

	/**
	 * Crowd budget, re-ranked every BudgetInterval seconds by distance to the listener:
	 * engines rendered at once (the player's car always counts), and walkers whose
	 * steps are heard (the player always). 25 traffic engines would take about 11 %
	 * of a core on the audio thread; 6 take under 3 %.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Budget")
	int32 MaxEngineVoices = 6;

	UPROPERTY(Config, EditAnywhere, Category = "Budget")
	int32 MaxStepWalkers = 8;

	UPROPERTY(Config, EditAnywhere, Category = "Budget")
	float BudgetInterval = 0.25f;

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

	/** Engine sound per car model; the first whose Match is in the class name wins. */
	UPROPERTY(Config, EditAnywhere, Category = "Engines")
	TArray<FGTPEngineProfile> EngineProfiles;

	/** Profile for a car class name (the last entry with an empty Match, else null). */
	const FGTPEngineProfile* FindEngineProfile(const FString& ClassName) const
	{
		const FGTPEngineProfile* Fallback = nullptr;
		for (const FGTPEngineProfile& Profile : EngineProfiles)
		{
			if (Profile.Match.IsEmpty())
			{
				Fallback = &Profile;
			}
			else if (ClassName.Contains(Profile.Match))
			{
				return &Profile;
			}
		}
		return Fallback;
	}

	/** Street voices: most at once, level, range (cm), gap between two screams (s), share of scared people who scream. */
	UPROPERTY(Config, EditAnywhere, Category = "Voices")
	int32 MaxVoiceVoices = 4;

	UPROPERTY(Config, EditAnywhere, Category = "Voices")
	float VoiceVolume = 0.9f;

	UPROPERTY(Config, EditAnywhere, Category = "Voices")
	FVector2D VoiceRange = FVector2D(200.f, 3500.f);

	UPROPERTY(Config, EditAnywhere, Category = "Voices")
	float ScreamCooldown = 0.35f;

	UPROPERTY(Config, EditAnywhere, Category = "Voices")
	float ScreamChance = 0.5f;

	/** Loud sounds frighten people around (UGTPAudioWorld::OnLoudSound): radius of a heavy and a medium crash (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Voices")
	FVector2D CrashScareRadius = FVector2D(800.f, 2000.f);

	/**
	 * Combat (docs/gameplay/combat.md): a shot within CloseShotDistance (cm) sounds
	 * dry and close, beyond it far (crack and tail) and late by the speed of sound.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	float CloseShotDistance = 2500.f;

	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	float ShotVolume = 1.f;

	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	FVector2D ShotRange = FVector2D(500.f, 60000.f);

	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	int32 MaxWeaponVoices = 6;

	/** A bullet passing within this of the listener whizzes (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	float WhizzRadius = 250.f;

	/** Ducking after a shot: time (s), level of the ambience and of the traffic. */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	float GunshotDuckSeconds = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	float GunshotDuckAmbience = 0.4f;

	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	float GunshotDuckTraffic = 0.6f;

	/** People within this of a shot run away (OnLoudSound, cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Combat")
	float ShotScareRadius = 3000.f;

	/** Police radio while wanted: level, random gap between two chatters (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Radio")
	float RadioVolume = 0.45f;

	UPROPERTY(Config, EditAnywhere, Category = "Radio")
	FVector2D RadioChatterGap = FVector2D(7.f, 16.f);

	/** Level of the interface sounds. */
	UPROPERTY(Config, EditAnywhere, Category = "Interface")
	float InterfaceVolume = 0.6f;

	/** Zone grid of the map (tools/audio/zone_map.py), relative to Content/. */
	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	FString ZoneMapFile = TEXT("Sound/SurfaceMap/zones-rue-de-la-republique.gtpsurf");

	/** Folder of the bed loops SW_Amb_<Layer> (tools/audio/make_ambiences.py). */
	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	FString AmbienceRoot = TEXT("/Game/Sound/Ambience");

	/** Parameter collection of the day-night and weather system (ADR-0008): rain and night are read from it. */
	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	FSoftObjectPath EnvironmentCollection = FSoftObjectPath(TEXT("/Game/Environment/Weather/MPC_Environment.MPC_Environment"));

	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	FName RainParameter = TEXT("RainIntensity");

	/** 0 by day, 1 at night (city lights on). */
	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	FName NightParameter = TEXT("CityLights");

	/** Level of the day street loop in each zone, and at night (multiplied). */
	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	float StreetLevelPedestrian = 0.45f;

	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	float StreetLevelQuay = 0.35f;

	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	float StreetLevelBridge = 0.2f;

	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	float StreetLevelNight = 0.3f;

	/** Level of the zone, night and crowd layers at full weight. */
	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	float ZoneLayerVolume = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	float RainVolume = 0.6f;

	/** Street rain heard from inside a car (the roof drumming takes over). */
	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	float RainInCarLevel = 0.3f;

	/** Crowd layer: full with this many visible pedestrians within CrowdRadius (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	int32 CrowdFullCount = 10;

	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	float CrowdRadius = 2500.f;

	/** Crossfade time between layers (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Ambience bed")
	float BedFadeSeconds = 2.f;

	/** Writes one line per second with the audio counters (soak tests). */
	UPROPERTY(Config, EditAnywhere, Category = "Debug")
	bool bLogStats = false;
};
