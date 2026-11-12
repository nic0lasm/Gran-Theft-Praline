#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPSolarPosition.h"
#include "GTPCarSignalLogic.h"
#include "GTPEnvironmentSubsystem.generated.h"

class AActor;
class UActorComponent;
class UDirectionalLightComponent;
class UExponentialHeightFogComponent;
class ULocalLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMaterialParameterCollection;
class UPostProcessComponent;
class USkyLightComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UVolumetricCloudComponent;

/** One weather state of docs/rendering/day-night-weather.md; states blend linearly. */
USTRUCT()
struct FGTPWeatherPreset
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	/** 0 clear sky, 1 fully overcast (cloud material, sun and sky scale follow). */
	UPROPERTY()
	float CloudCoverage = 0.2f;

	/** Multiplier of the clear-sky sun illuminance. */
	UPROPERTY()
	float SunScale = 1.f;

	/** Multiplier of the sky light. */
	UPROPERTY()
	float SkyScale = 1.f;

	/** Sun disc angle in degrees: larger gives softer shadows under thin cloud. */
	UPROPERTY()
	float SunSourceAngle = 0.5357f;

	/** Exponential height fog density and height falloff. */
	UPROPERTY()
	float FogDensity = 0.01f;

	UPROPERTY()
	float FogHeightFalloff = 0.2f;

	/** 0..1 scale of the Saone fog volumes (the morning mist adds to it). */
	UPROPERTY()
	float RiverFog = 0.f;

	/** 0..1 rain intensity (rain layer, wetting speed). */
	UPROPERTY()
	float Rain = 0.f;

	/** EV added to the daylight exposure target (negative under cloud). */
	UPROPERTY()
	float ExposureOffset = 0.f;

	/** Post-process colour saturation. */
	UPROPERTY()
	float Saturation = 1.f;

	/** Relative chance to be drawn by the automatic weather. */
	UPROPERTY()
	float Weight = 1.f;

	/** Multiplier of the profile's cloud ray samples in this weather (thick cover can take fewer). */
	UPROPERTY()
	float CloudSampleScale = 1.f;

	/** A uniform cover that the 2D dome renders well: used instead of the volumetric
	 *  clouds when the profile sets bCloudDomeWhenOvercast. */
	UPROPERTY()
	bool bPreferCloudDome = false;

	/** Light function cloud shadow (M_CloudShadowLF): share of the ground under cloud,
	 *  0..1 (negative = CloudCoverage). The volumetric clouds of the map are not driven by
	 *  the weather in m1, so this is set per weather to match their shadow. */
	UPROPERTY()
	float CloudShadowCoverage = -1.f;

	/** Share of the sun removed under a cloud by the light function, 0..1. */
	UPROPERTY()
	float CloudShadowStrength = 0.75f;

	/** Multiplier of the whole light function: the average sun loss of the volumetric
	 *  cloud shadow, matched by image luminance (tools/environment/calibrate-cloud-shadow-mac.sh). */
	UPROPERTY()
	float CloudShadowMean = 1.f;
};

/** A named, frozen test ambience (-Ambience=<Name>): date, time and weather. */
USTRUCT()
struct FGTPAmbience
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	/** Local date, YYYY-MM-DD. */
	UPROPERTY()
	FString Date;

	/** Local hour, 0..24. */
	UPROPERTY()
	float Hour = 12.f;

	UPROPERTY()
	FName Weather;
};

/** Light budget of one graphics profile (ADR-0002, art bible section 5.3). */
USTRUCT()
struct FGTPLightBudget
{
	GENERATED_BODY()

	UPROPERTY()
	FString Profile;

	/** Local night lights rendered at once (nearest to the camera). */
	UPROPERTY()
	int32 MaxLights = 64;

	/** Of those, how many cast shadows. */
	UPROPERTY()
	int32 MaxShadowedLights = 4;

	/** Lights farther than this are off (cm). */
	UPROPERTY()
	float MaxDistance = 9000.f;

	/** Volumetric fog allowed with this profile. */
	UPROPERTY()
	bool bVolumetricFog = false;

	/** Cars (traffic, police, the player's) with headlights and tail lights on at night, nearest first. */
	UPROPERTY()
	int32 MaxLitCars = 4;

	/** Volumetric clouds drawn at all (sky atmosphere and fog only when false). */
	UPROPERTY()
	bool bVolumetricClouds = true;

	/** Ray sample count scale of the clouds (view, reflections, shadows); 1 = map setting. */
	UPROPERTY()
	float CloudSampleScale = 1.f;

	/** Sun shadows cast by the clouds. */
	UPROPERTY()
	bool bCloudShadows = true;

	/** EV added to the exposure target with this profile (compensates a flatter, brighter scene,
	 *  e.g. without cloud shadows). Applied only when the cloud shadows are off. */
	UPROPERTY()
	float NoCloudShadowExposureOffset = 0.f;

	/** The weather may change the cloud coverage (denser clouds cost more). */
	UPROPERTY()
	bool bDriveCloudCoverage = true;

	/** Cheap fallback: a 2D cloud dome around the camera, when the volumetric clouds are off. */
	UPROPERTY()
	bool bCloudDome = false;

	/** The 2D dome replaces the volumetric clouds in the weathers that prefer it (overcast, rain...). */
	UPROPERTY()
	bool bCloudDomeWhenOvercast = false;

	/** Cheap cloud shadows: a scrolling light function on the sun instead of the volumetric
	 *  cloud shadow map (about 8.6 ms in m1). Turns the volumetric cloud shadows off. */
	UPROPERTY()
	bool bCloudShadowLightFunction = false;

	/** Night clouds (sun below NightCloudSunElevation, the moon lights them): the volumetric
	 *  clouds cost more at night and the eye sees little of them. bCloudDomeAtNight shows the
	 *  2D dome instead; NightCloudSampleScale multiplies the cloud ray samples. */
	UPROPERTY()
	bool bCloudDomeAtNight = false;

	UPROPERTY()
	float NightCloudSampleScale = 1.f;

	UPROPERTY()
	float NightCloudSunElevation = -4.f;

	/** The sample scale ramps from 1 at this sun elevation down to NightCloudSampleScale at
	 *  NightCloudSunElevation (dusk costs as much as the night: +16 ms p95 measured). */
	UPROPERTY()
	float NightCloudRampStart = 5.f;
};

/**
 * Day-night cycle and weather of the street maps, driven at run time so no
 * map has to be edited (the map's own sun, sky, fog and clouds are reused;
 * see docs/rendering/day-night-weather.md and ADR-0008).
 *
 * Every frame or a few times per second it sets:
 *   - sun and moon from the date, the time and Lyon's position (45.76 N, 4.83 E);
 *   - sun and sky intensity, clouds and fog from the blended weather state;
 *   - the exposure range (EV100) from the sun elevation and the weather, on an
 *     unbound post-process component of priority 1000 (the map's own lock
 *     stays underneath as the fallback);
 *   - the night lights (street lamps, shop windows): spawned from
 *     Content/<NightLightsFile> plus any actor tagged GTP_StreetLight or
 *     GTP_ShopLight, only the nearest MaxLights on, MaxShadowedLights with shadows;
 *   - car headlights and tail lights on the nearest MaxLitCars vehicles (no shadows);
 *   - the parameter collection MPC_Environment read by the materials
 *     (Wetness, Puddles, RainIntensity, CityLights, EmissiveScale, Cloudiness).
 *
 * Command line: -TimeOfDay=<hours> -Date=YYYY-MM-DD -TimeScale=<x> (0 freezes)
 * -Weather=<preset> -Ambience=<noon|golden|dusk|night|rain|night-rain|fog>
 * (frozen test ambience, time stopped unless -TimeScale is given)
 * -Sky=volumetric|lightfunction|cheap (A/B test of the sky: volumetric clouds and their
 *  shadow map; volumetric clouds with light function shadows; 2D dome with light function shadows)
 * -CloudShadowLF=0|1 (cheap light function cloud shadows)
 * -CloudShadows=0 (no cloud shadows, exposure compensated by NoCloudShadowExposureOffset)
 * -CloudDome=1 (2D cloud dome instead of the volumetric clouds, 0 = never)
 * -DayNight=0 or -NoEnvironment (leave the map lighting untouched).
 * Start line on stdout, Shipping included: "daynight: on | ..." or "daynight: off".
 * Console: gtp.Time, gtp.Date, gtp.TimeScale, gtp.Weather, gtp.EnvStatus.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPEnvironmentSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Writes Describe() and the sky state to Saved/Environment/status.txt (or -EnvReport=<path>):
	 *  Shipping writes no log, this is how a test run checks what was loaded. */
	void WriteStatusFile() const;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Local Lyon time of day in hours, 0..24. */
	void SetTimeOfDay(double Hours);
	double GetTimeOfDay() const { return LocalHours; }

	/** Local date (time of day is kept). */
	void SetDate(const FDateTime& Date);

	/** Game seconds per real second (30: one day lasts 48 minutes). */
	void SetTimeScale(float Scale) { TimeScale = FMath::Max(0.f, Scale); }

	/** Blend to a weather preset; false if no preset has this name. */
	bool SetWeather(FName Preset, float TransitionSeconds);

	/** One line for logs and the console. */
	FString Describe() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FManagedLight
	{
		TWeakObjectPtr<ULocalLightComponent> Light;
		FVector Location = FVector::ZeroVector;
		float FullIntensity = 0.f;
		float AppliedScale = -1.f;
		bool bCanShadow = true;
		bool bOn = false;
		bool bShadow = false;
	};

	void FindSceneComponents(UWorld& World);
	void CreateOwnComponents(UWorld& World);
	void SpawnRiverFog(UWorld& World);
	void LoadNightLights(UWorld& World);
	void FindNewCars();
	void UpdateCarLights(const FVector& CameraLocation);
	/** Every frame, lit cars only: brake, reverse and indicator lights. */
	void UpdateCarSignals();
	void AdoptTaggedLights(UWorld& World);
	void ReadCommandLine();
	void ApplyBudgetForProfile(UWorld& World);
	void ApplyCloudQuality();

	/** Dynamic instance of the clouds' material for the coverage; retried while the clouds or
	 *  their material are not there yet (streamed level, component without material). */
	void SetupCloudMaterial();
	void UpdateClouds(const FGTPWeatherPreset& Weather);

	void AdvanceTime(float DeltaTime);
	void UpdateWeather(float DeltaTime);
	void ApplySunAndMoon(bool bForce);
	void ApplyAtmosphere();
	void ApplyExposure();
	void ApplyParameters();
	void UpdateNightLights();
	void UpdateRain();

	const FGTPWeatherPreset* FindPreset(FName Name) const;
	FGTPWeatherPreset BlendedWeather() const;
	FDateTime LocalDateTime() const;
	float ExposureForElevation(float ElevationDeg) const;
	float MorningMist() const;

	// ---- configuration (DefaultGame.ini) ----

	UPROPERTY(Config)
	bool bEnabled = true;

	/** Observer position (degrees, east positive). */
	UPROPERTY(Config)
	double Latitude = 45.7597;

	UPROPERTY(Config)
	double Longitude = 4.8320;

	/** Local date and hour at start (autumn by default, art bible section 3.3). */
	UPROPERTY(Config)
	FString StartDate = TEXT("2026-10-15");

	UPROPERTY(Config)
	float StartHour = 14.f;

	UPROPERTY(Config)
	float DefaultTimeScale = 30.f;

	/** Clear-sky sun illuminance at the top of the atmosphere (lux) and full-moon illuminance (lux). */
	UPROPERTY(Config)
	float SunLux = 100000.f;

	UPROPERTY(Config)
	float MoonLux = 0.4f;

	/** The sun rotation is only updated after this angle (virtual shadow map cache). */
	UPROPERTY(Config)
	float SunStepDegrees = 0.1f;

	/** Exposure target in EV100 against the sun elevation (X degrees, Y EV100), clear sky. */
	UPROPERTY(Config)
	TArray<FVector2D> ExposureCurve;

	/** Exposure range around the target (EV), adaptation speeds (EV per second). */
	UPROPERTY(Config)
	float ExposureHalfRange = 0.5f;

	UPROPERTY(Config)
	float ExposureSpeedUp = 2.f;

	UPROPERTY(Config)
	float ExposureSpeedDown = 1.f;

	/** White balance by day and at night (K). Night stays a little warm, never sepia. */
	UPROPERTY(Config)
	float DayWhiteTemp = 6500.f;

	UPROPERTY(Config)
	float NightWhiteTemp = 5400.f;

	/** Sun elevations (degrees) where the city lights start and finish switching on. */
	UPROPERTY(Config)
	float LightsOnStartElevation = 2.f;

	UPROPERTY(Config)
	float LightsOnFullElevation = -4.f;

	/** EmissiveScale of MPC_Environment at full night (1 by day). */
	UPROPERTY(Config)
	float NightEmissiveScale = 20.f;

	/** Height fog inscattering at night (city glow), linear colour. */
	UPROPERTY(Config)
	FLinearColor NightFogInscattering = FLinearColor(0.02f, 0.014f, 0.008f);

	/** Weather states and automatic weather. */
	UPROPERTY(Config)
	TArray<FGTPWeatherPreset> WeatherPresets;

	UPROPERTY(Config)
	FName StartWeather = TEXT("Clear");

	/** Test ambiences for -Ambience= (tools/environment/measure-ambiences-mac.sh). */
	UPROPERTY(Config)
	TArray<FGTPAmbience> Ambiences;

	UPROPERTY(Config)
	bool bAutoWeather = true;

	/** Game hours between two automatic weather draws, real seconds of a transition. */
	UPROPERTY(Config)
	float WeatherChangeHours = 3.f;

	UPROPERTY(Config)
	float WeatherTransitionSeconds = 60.f;

	/** Ground wetting and drying speeds (per real second, at rain 1 and rain 0). */
	UPROPERTY(Config)
	float WettingRate = 0.025f;

	UPROPERTY(Config)
	float DryingRate = 0.0025f;

	/** Cloud material scalar parameters that receive CloudCoverage (unknown names are ignored). */
	UPROPERTY(Config)
	TArray<FName> CloudCoverageParameters;

	/** Material given to the clouds when their component has none (the engine's default clouds). */
	UPROPERTY(Config)
	TSoftObjectPtr<UMaterialInterface> CloudFallbackMaterial;

	/** Value given to the coverage parameters = Offset + Scale x weather CloudCoverage (0-1). */
	UPROPERTY(Config)
	float CloudCoverageScale = 1.f;
	UPROPERTY(Config)
	float CloudCoverageOffset = 0.f;

	/** Parameter collection read by the materials (tools/environment/unreal/setup_environment.py). */
	UPROPERTY(Config)
	TSoftObjectPtr<UMaterialParameterCollection> ParameterCollection;

	/** Rain layer around the camera: mesh, material, scale. */
	UPROPERTY(Config)
	TSoftObjectPtr<UStaticMesh> RainMesh;

	UPROPERTY(Config)
	TSoftObjectPtr<UMaterialInterface> RainMaterial;

	UPROPERTY(Config)
	FVector RainScale = FVector(24.0, 24.0, 16.0);

	/** 2D cloud dome (fallback of the volumetric clouds): mesh, unlit material, scale, brightness. */
	UPROPERTY(Config)
	TSoftObjectPtr<UStaticMesh> CloudDomeMesh;

	UPROPERTY(Config)
	TSoftObjectPtr<UMaterialInterface> CloudDomeMaterial;

	UPROPERTY(Config)
	FVector CloudDomeScale = FVector(4000.0, 4000.0, 1200.0);

	/** Cloud luminance relative to the middle grey of the current exposure. */
	UPROPERTY(Config)
	float CloudDomeBrightness = 1.6f;

	/** Light function material of the cheap cloud shadows (reads Cloudiness from MPC_Environment). */
	UPROPERTY(Config)
	TSoftObjectPtr<UMaterialInterface> CloudShadowMaterial;

	/** Saone mist: local fog volumes (centres in cm, scale of the unit sphere), extinction at 1. */
	UPROPERTY(Config)
	TArray<FVector> RiverFogCentres;

	UPROPERTY(Config)
	FVector RiverFogScale = FVector(45.0, 160.0, 18.0);

	UPROPERTY(Config)
	float RiverFogExtinction = 0.6f;

	/** Morning mist on the river (added to the weather's RiverFog), October to March. */
	UPROPERTY(Config)
	float MorningRiverFog = 0.8f;

	/** Night lights file under Content/ (tools/environment/build_night_lights.py). */
	UPROPERTY(Config)
	FString NightLightsFile = TEXT("Environment/Data/NightLights.csv");

	/** Light budgets; the active graphics profile picks one (m1 when unknown). */
	UPROPERTY(Config)
	TArray<FGTPLightBudget> LightBudgets;

	/** Car lights: headlight (candela, K, cm, outer cone degrees) and tail light (candela, cm). */
	UPROPERTY(Config)
	float HeadlightCandela = 12000.f;

	UPROPERTY(Config)
	float HeadlightKelvin = 5000.f;

	UPROPERTY(Config)
	float HeadlightRadius = 3500.f;

	UPROPERTY(Config)
	float HeadlightOuterCone = 32.f;

	UPROPERTY(Config)
	float TailLightCandela = 40.f;

	UPROPERTY(Config)
	float TailLightRadius = 350.f;

	/** Car signals on the lit cars (GTPCarSignalLogic.h): brake lights (tail light times
	 *  BrakeLightBoost), reverse light and turn indicators on one extra small light per car.
	 *  -CarSignals=0 switches them off. */
	UPROPERTY(Config)
	bool bCarSignals = true;

	UPROPERTY(Config)
	float BrakeLightBoost = 5.f;

	UPROPERTY(Config)
	float SignalCandela = 30.f;

	UPROPERTY(Config)
	float SignalRadius = 250.f;

	/** Seconds between two night light updates. */
	UPROPERTY(Config)
	float LightUpdateInterval = 0.2f;

	// ---- state ----

	bool bActive = false;
	FDateTime CurrentDate;
	double LocalHours = 12.0;
	float TimeScale = 30.f;
	float LightTimer = 0.f;
	double HoursToNextWeather = 3.0;
	FRandomStream WeatherRandom;

	FName FromWeather;
	FName ToWeather;
	float WeatherAlpha = 1.f;
	float WeatherBlendSpeed = 1.f;
	float Wetness = 0.f;

	FGTPSkyPosition SunPosition;
	FGTPSkyPosition MoonPosition;
	double MoonIllumination = 0.0;
	FRotator AppliedSunRotation = FRotator(1000.0, 0.0, 0.0);
	float NightFactor = 0.f;
	float ExposureTarget = 14.f;
	FGTPLightBudget Budget;

	TWeakObjectPtr<UDirectionalLightComponent> Sun;
	TWeakObjectPtr<UDirectionalLightComponent> MoonLight;
	TWeakObjectPtr<USkyLightComponent> SkyLight;
	TWeakObjectPtr<UExponentialHeightFogComponent> HeightFog;
	TWeakObjectPtr<UVolumetricCloudComponent> Clouds;
	float SkyLightBaseIntensity = 1.f;
	FLinearColor DayFogInscattering = FLinearColor::Black;
	FLinearColor AppliedFogInscattering = FLinearColor(-1.f, -1.f, -1.f);
	float AppliedRiverFog = -1.f;
	float AppliedCloudSampleScale = 1.f;
	int32 AppliedCloudMode = -1;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Holder;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RainLayer;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CloudDome;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CloudMaterial;

	/** Instance of CloudShadowMaterial on the sun, null when the light function is off. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CloudShadowInstance;

	/** off, loaded or missing: reported in the status file. */
	FString CloudShadowState = TEXT("off");

	/** CloudCoverageParameters found in the map's cloud material (status file), "none" when
	 *  the weather cannot change the volumetric clouds. */
	FString CloudCoverageFound = TEXT("not set up yet");

	/** Seconds left to retry SetupCloudMaterial (once per second). */
	float CloudSetupRetryTime = 30.f;
	float CloudSetupTimer = 1.f;

	/** The status file is written again once the night lights and cars have settled. */
	float StatusRewriteTimer = 12.f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> LoadedCollection;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> RiverFogVolumes;

	TArray<FManagedLight> NightLights;

	/** Headlights and tail lights added to each car (no shadows, ADR-0002). */
	struct FCarLights
	{
		TWeakObjectPtr<AActor> Car;
		TArray<TWeakObjectPtr<ULocalLightComponent>> Lights;
		bool bOn = false;
		TWeakObjectPtr<ULocalLightComponent> Tail;
		TWeakObjectPtr<ULocalLightComponent> Signal;
		TWeakObjectPtr<UActorComponent> Movement;
		FVector RearLeft = FVector::ZeroVector;
		FVector RearCentre = FVector::ZeroVector;
		FGTPCarSignalState SignalState;
		bool bBrakeShown = false;
		int32 SignalShown = 0;  // 0 off, 1 reverse, 2 indicator lit
		int32 SignalSide = 0;
	};
	TArray<FCarLights> CarLights;
	float CarScanTimer = 0.f;
};
