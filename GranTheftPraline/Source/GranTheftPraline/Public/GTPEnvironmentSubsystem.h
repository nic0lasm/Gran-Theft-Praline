#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPSolarPosition.h"
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
};

/**
 * Day-night cycle and weather of the street maps, driven at run time so no
 * map has to be edited (the map's own sun, sky, fog and clouds are reused;
 * see docs/rendering/day-night-weather.md and ADR-0004).
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
 *   - the parameter collection MPC_Environment read by the materials
 *     (Wetness, Puddles, RainIntensity, CityLights, EmissiveScale, Cloudiness).
 *
 * Command line: -TimeOfDay=<hours> -Date=YYYY-MM-DD -TimeScale=<x> (0 freezes)
 * -Weather=<preset> -NoEnvironment (leave the map lighting untouched).
 * Console: gtp.Time, gtp.Date, gtp.TimeScale, gtp.Weather, gtp.EnvStatus.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPEnvironmentSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
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
	void AdoptTaggedLights(UWorld& World);
	void ReadCommandLine();
	void ApplyBudgetForProfile(UWorld& World);

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

	UPROPERTY(Transient)
	TObjectPtr<AActor> Holder;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RainLayer;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CloudMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> LoadedCollection;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UActorComponent>> RiverFogVolumes;

	TArray<FManagedLight> NightLights;
};
