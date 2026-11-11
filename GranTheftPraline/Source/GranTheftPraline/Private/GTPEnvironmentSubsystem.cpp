#include "GTPEnvironmentSubsystem.h"

#include "GTPDeterminism.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GTPGraphicsProfile.h"
#include "GTPLog.h"
#include "GTPVehicle.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"


DEFINE_LOG_CATEGORY_STATIC(LogGTPEnvironment, Log, All);

// Overrides of the light function cloud shadow for calibration sweeps (-GfxCVars=...).
// Negative = the weather preset value.
static TAutoConsoleVariable<float> CVarCloudShadowMean(TEXT("gtp.CloudShadowLF.Mean"), -1.f,
	TEXT("Multiplier of the light function cloud shadow (negative: weather preset CloudShadowMean)"));
static TAutoConsoleVariable<float> CVarCloudShadowStrength(TEXT("gtp.CloudShadowLF.Strength"), -1.f,
	TEXT("Share of the sun removed under a cloud (negative: weather preset CloudShadowStrength)"));
static TAutoConsoleVariable<float> CVarCloudShadowCoverage(TEXT("gtp.CloudShadowLF.Coverage"), -1.f,
	TEXT("Share of the ground under cloud (negative: weather preset CloudShadowCoverage)"));
static TAutoConsoleVariable<float> CVarCloudShadowContrast(TEXT("gtp.CloudShadowLF.Contrast"), 3.f,
	TEXT("Sharpness of the cloud shadow edges"));

namespace
{
	const FName StreetLightTag(TEXT("GTP_StreetLight"));
	const FName ShopLightTag(TEXT("GTP_ShopLight"));

	bool ParseDate(const FString& Text, FDateTime& Out)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT("-"));
		if (Parts.Num() != 3)
		{
			return false;
		}
		const int32 Year = FCString::Atoi(*Parts[0]);
		const int32 Month = FCString::Atoi(*Parts[1]);
		const int32 Day = FCString::Atoi(*Parts[2]);
		if (!FDateTime::Validate(Year, Month, Day, 0, 0, 0, 0))
		{
			return false;
		}
		Out = FDateTime(Year, Month, Day);
		return true;
	}

	/** Writes an FLinearColor property found by one of several names (they changed across engine versions). */
	/** Like FParse::Value, but the LAST occurrence wins: test scripts append a variant's
	 *  -Sky= after the base arguments, and FParse::Value would keep the first one. */
	bool ParseLastValue(const TCHAR* Stream, const TCHAR* Key, FString& Out)
	{
		bool bFound = false;
		const int32 KeyLength = FCString::Strlen(Key);
		const TCHAR* Cursor = Stream;
		while (const TCHAR* Hit = FCString::Stristr(Cursor, Key))
		{
			// Only a whole option: start of the line, or after a space, '-' or '"'.
			const bool bStart = Hit == Stream || Hit[-1] == TEXT('-') || Hit[-1] == TEXT(' ') || Hit[-1] == TEXT('"');
			FString Value;
			if (bStart && FParse::Value(Hit, Key, Value))
			{
				Out = Value;
				bFound = true;
			}
			Cursor = Hit + KeyLength;
		}
		return bFound;
	}

	bool SetColorProperty(UObject* Object, std::initializer_list<const TCHAR*> Names, const FLinearColor& Value)
	{
		for (const TCHAR* Name : Names)
		{
			FStructProperty* Prop = FindFProperty<FStructProperty>(Object->GetClass(), Name);
			if (Prop && Prop->Struct == TBaseStructure<FLinearColor>::Get())
			{
				*Prop->ContainerPtrToValuePtr<FLinearColor>(Object) = Value;
				return true;
			}
		}
		return false;
	}

	bool GetColorProperty(const UObject* Object, std::initializer_list<const TCHAR*> Names, FLinearColor& Out)
	{
		for (const TCHAR* Name : Names)
		{
			const FStructProperty* Prop = FindFProperty<FStructProperty>(Object->GetClass(), Name);
			if (Prop && Prop->Struct == TBaseStructure<FLinearColor>::Get())
			{
				Out = *Prop->ContainerPtrToValuePtr<FLinearColor>(Object);
				return true;
			}
		}
		return false;
	}

	/** Calls a Blueprint setter taking one float (used for classes we only know by name). */
	bool CallFloatSetter(UObject* Object, const TCHAR* FunctionName, float Value)
	{
		UFunction* Function = Object ? Object->FindFunction(FName(FunctionName)) : nullptr;
		if (!Function || Function->ParmsSize != sizeof(float))
		{
			return false;
		}
		Object->ProcessEvent(Function, &Value);
		return true;
	}

	float Saturate(float Value)
	{
		return FMath::Clamp(Value, 0.f, 1.f);
	}

	UGTPEnvironmentSubsystem* EnvironmentOf(UWorld* World)
	{
		UGTPEnvironmentSubsystem* Env = World ? World->GetSubsystem<UGTPEnvironmentSubsystem>() : nullptr;
		if (!Env)
		{
			UE_LOG(LogGTPEnvironment, Warning, TEXT("No environment in this world"));
		}
		return Env;
	}

	FAutoConsoleCommandWithWorldAndArgs TimeCommand(
		TEXT("gtp.Time"), TEXT("gtp.Time <hours>: set the Lyon local time of day (e.g. 18.25)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGTPEnvironmentSubsystem* Env = EnvironmentOf(World); Env && Args.Num() > 0)
			{
				Env->SetTimeOfDay(FCString::Atod(*Args[0]));
				UE_LOG(LogGTPEnvironment, Display, TEXT("%s"), *Env->Describe());
				Env->WriteStatusFile();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs DateCommand(
		TEXT("gtp.Date"), TEXT("gtp.Date <YYYY-MM-DD>: set the local date"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FDateTime Date;
			if (UGTPEnvironmentSubsystem* Env = EnvironmentOf(World); Env && Args.Num() > 0 && ParseDate(Args[0], Date))
			{
				Env->SetDate(Date);
				UE_LOG(LogGTPEnvironment, Display, TEXT("%s"), *Env->Describe());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs TimeScaleCommand(
		TEXT("gtp.TimeScale"), TEXT("gtp.TimeScale <x>: game seconds per real second (0 freezes, 30 = 48 min days)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGTPEnvironmentSubsystem* Env = EnvironmentOf(World); Env && Args.Num() > 0)
			{
				Env->SetTimeScale(FCString::Atof(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs WeatherCommand(
		TEXT("gtp.Weather"), TEXT("gtp.Weather <Clear|Cloudy|Overcast|Rain|Storm|Fog> [transition seconds, default 0]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGTPEnvironmentSubsystem* Env = EnvironmentOf(World); Env && Args.Num() > 0)
			{
				const float Seconds = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.f;
				if (!Env->SetWeather(FName(*Args[0]), Seconds))
				{
					UE_LOG(LogGTPEnvironment, Warning, TEXT("Unknown weather '%s'"), *Args[0]);
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(
		TEXT("gtp.EnvStatus"), TEXT("Print the time, sun, weather, exposure and night light state"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UGTPEnvironmentSubsystem* Env = EnvironmentOf(World))
			{
				UE_LOG(LogGTPEnvironment, Display, TEXT("%s"), *Env->Describe());
			}
		}));
}

bool UGTPEnvironmentSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UGTPEnvironmentSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPEnvironmentSubsystem, STATGROUP_Tickables);
}

void UGTPEnvironmentSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	int32 DayNight = 1;
	FParse::Value(FCommandLine::Get(), TEXT("DayNight="), DayNight);
	if (!bEnabled || DayNight == 0 || FParse::Param(FCommandLine::Get(), TEXT("NoEnvironment")))
	{
		GTP_TRACE(LogGTPEnvironment, TEXT("daynight: off (map lighting left as authored)"));
		return;
	}

	FindSceneComponents(InWorld);
	if (!Sun.IsValid())
	{
		GTP_TRACE(LogGTPEnvironment, TEXT("daynight: off (no directional light in %s)"), *InWorld.GetMapName());
		return;
	}

	if (WeatherPresets.IsEmpty())
	{
		FGTPWeatherPreset Clear;
		Clear.Name = TEXT("Clear");
		WeatherPresets.Add(Clear);
	}
	if (ExposureCurve.IsEmpty())
	{
		ExposureCurve = { FVector2D(-18.0, 3.5), FVector2D(0.0, 10.0), FVector2D(70.0, 14.5) };
	}
	ExposureCurve.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });

	if (!ParseDate(StartDate, CurrentDate))
	{
		CurrentDate = FDateTime(2026, 10, 15);
	}
	LocalHours = StartHour;
	TimeScale = DefaultTimeScale;
	FromWeather = ToWeather = FindPreset(StartWeather) ? StartWeather : WeatherPresets[0].Name;
	WeatherRandom.Initialize(GTPDeterminism::IsOn() ? 0x57454154 : static_cast<int32>(FDateTime::Now().GetTicks() & 0x7fffffff));
	HoursToNextWeather = WeatherChangeHours;
	ReadCommandLine();
	Wetness = BlendedWeather().Rain > 0.05f ? 1.f : 0.f;

	CreateOwnComponents(InWorld);
	ApplyBudgetForProfile(InWorld);
	ApplyCloudQuality();
	SpawnRiverFog(InWorld);
	LoadNightLights(InWorld);
	AdoptTaggedLights(InWorld);

	bActive = true;
	ApplySunAndMoon(true);
	ApplyAtmosphere();
	ApplyExposure();
	ApplyParameters();
	FindNewCars();
	UpdateNightLights();
	GTP_TRACE(LogGTPEnvironment, TEXT("daynight: on | %s"), *Describe());
	WriteStatusFile();
}

void UGTPEnvironmentSubsystem::Deinitialize()
{
	bActive = false;
	NightLights.Reset();
	RiverFogVolumes.Reset();
	Super::Deinitialize();
}

// --------------------------------------------------------------------------- setup

void UGTPEnvironmentSubsystem::FindSceneComponents(UWorld& World)
{
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		TInlineComponentArray<UDirectionalLightComponent*> Directionals(*It);
		for (UDirectionalLightComponent* Light : Directionals)
		{
			// Atmosphere light 0 is the sun, 1 the moon (tools/rendering/reference_scene).
			if (Light->GetAtmosphereSunLightIndex() == 1 && Light->IsUsedAsAtmosphereSunLight())
			{
				MoonLight = Light;
			}
			else if (!Sun.IsValid())
			{
				Sun = Light;
			}
		}
		if (!SkyLight.IsValid())
		{
			SkyLight = It->FindComponentByClass<USkyLightComponent>();
		}
		if (!HeightFog.IsValid())
		{
			HeightFog = It->FindComponentByClass<UExponentialHeightFogComponent>();
		}
		if (!Clouds.IsValid())
		{
			Clouds = It->FindComponentByClass<UVolumetricCloudComponent>();
		}
	}

	if (Sun.IsValid())
	{
		Sun->SetMobility(EComponentMobility::Movable);
		Sun->SetAtmosphereSunLight(true);
		Sun->SetAtmosphereSunLightIndex(0);
	}
	if (SkyLight.IsValid())
	{
		SkyLight->SetMobility(EComponentMobility::Movable);
		SkyLightBaseIntensity = SkyLight->Intensity;
	}
	if (HeightFog.IsValid())
	{
		GetColorProperty(HeightFog.Get(), { TEXT("FogInscatteringLuminance"), TEXT("FogInscatteringColor") }, DayFogInscattering);
	}
}

void UGTPEnvironmentSubsystem::CreateOwnComponents(UWorld& World)
{
	FActorSpawnParameters Params;
	Params.Name = TEXT("GTP_Environment");
	Params.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Params.ObjectFlags |= RF_Transient;
	Holder = World.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!Holder)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	Holder->SetRootComponent(Root);
	Root->RegisterComponent();

	// Exposure, white balance and saturation, above the map's post-process volume.
	PostProcess = NewObject<UPostProcessComponent>(Holder, TEXT("EnvironmentPostProcess"));
	PostProcess->bUnbound = true;
	PostProcess->Priority = 1000.f;
	PostProcess->BlendWeight = 1.f;
	FPostProcessSettings& S = PostProcess->Settings;
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = 0.f;
	S.bOverride_AutoExposureMinBrightness = true;
	S.bOverride_AutoExposureMaxBrightness = true;
	S.bOverride_AutoExposureSpeedUp = true;
	S.AutoExposureSpeedUp = ExposureSpeedUp;
	S.bOverride_AutoExposureSpeedDown = true;
	S.AutoExposureSpeedDown = ExposureSpeedDown;
	S.bOverride_WhiteTemp = true;
	S.bOverride_ColorSaturation = true;
	PostProcess->SetupAttachment(Root);
	PostProcess->RegisterComponent();

	// Moon: the map has none at day, add one (no shadows, atmosphere light 1).
	if (!MoonLight.IsValid())
	{
		UDirectionalLightComponent* Moon = NewObject<UDirectionalLightComponent>(Holder, TEXT("Moon"));
		Moon->SetMobility(EComponentMobility::Movable);
		Moon->CastShadows = false;
		Moon->bUseTemperature = true;
		Moon->Temperature = 7500.f;
		Moon->SetVisibility(false);
		Moon->SetupAttachment(Root);
		Moon->RegisterComponent();
		Moon->SetAtmosphereSunLight(true);
		Moon->SetAtmosphereSunLightIndex(1);
		MoonLight = Moon;
	}

	// Cloud coverage goes through a dynamic instance of the map's cloud material.
	if (Clouds.IsValid())
	{
		const FObjectProperty* MaterialProp = FindFProperty<FObjectProperty>(UVolumetricCloudComponent::StaticClass(), TEXT("Material"));
		UMaterialInterface* Base = MaterialProp ? Cast<UMaterialInterface>(MaterialProp->GetObjectPropertyValue_InContainer(Clouds.Get())) : nullptr;
		if (Base)
		{
			CloudMaterial = UMaterialInstanceDynamic::Create(Base, Holder);
			Clouds->SetMaterial(CloudMaterial);
		}
	}

	LoadedCollection = ParameterCollection.LoadSynchronous();
	if (!LoadedCollection)
	{
		UE_LOG(LogGTPEnvironment, Warning, TEXT("Parameter collection %s missing: wet and night materials stay as authored"),
			*ParameterCollection.ToString());
	}

	// Rain layer: a two-sided translucent cylinder that follows the camera.
	UStaticMesh* Mesh = RainMesh.LoadSynchronous();
	UMaterialInterface* Material = RainMaterial.LoadSynchronous();
	if (Mesh && Material)
	{
		RainLayer = NewObject<UStaticMeshComponent>(Holder, TEXT("RainLayer"));
		RainLayer->SetMobility(EComponentMobility::Movable);
		RainLayer->SetStaticMesh(Mesh);
		RainLayer->SetMaterial(0, Material);
		RainLayer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RainLayer->SetCastShadow(false);
		RainLayer->bReceivesDecals = false;
		RainLayer->SetWorldScale3D(RainScale);
		RainLayer->SetVisibility(false);
		RainLayer->SetupAttachment(Root);
		RainLayer->RegisterComponent();
	}
	else
	{
		UE_LOG(LogGTPEnvironment, Warning, TEXT("Rain mesh or material missing (%s, %s): no rain layer"),
			*RainMesh.ToString(), *RainMaterial.ToString());
	}
}

void UGTPEnvironmentSubsystem::SpawnRiverFog(UWorld& World)
{
	if (RiverFogCentres.IsEmpty())
	{
		return;
	}
	// Local fog volumes (UE 5.3+) are only known by name here, set through their Blueprint setters.
	UClass* FogClass = LoadClass<AActor>(nullptr, TEXT("/Script/Engine.LocalFogVolume"));
	if (!FogClass)
	{
		UE_LOG(LogGTPEnvironment, Warning, TEXT("LocalFogVolume class not found: no river mist"));
		return;
	}
	for (const FVector& Centre : RiverFogCentres)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		AActor* Volume = World.SpawnActor<AActor>(FogClass, FTransform(FRotator::ZeroRotator, Centre, RiverFogScale), Params);
		if (!Volume)
		{
			continue;
		}
		TInlineComponentArray<UActorComponent*> Components(Volume);
		for (UActorComponent* Component : Components)
		{
			if (Component->GetClass()->GetName().Contains(TEXT("LocalFogVolume")))
			{
				CallFloatSetter(Component, TEXT("SetHeightFogExtinction"), 0.f);
				RiverFogVolumes.Add(Component);
			}
		}
	}
	UE_LOG(LogGTPEnvironment, Log, TEXT("River mist: %d fog volumes"), RiverFogVolumes.Num());
}

void UGTPEnvironmentSubsystem::LoadNightLights(UWorld& World)
{
	const FString Path = FPaths::ProjectContentDir() / NightLightsFile;
	TArray<FString> Lines;
	if (!Holder || !FFileHelper::LoadFileToStringArray(Lines, *Path) || Lines.Num() < 2)
	{
		UE_LOG(LogGTPEnvironment, Log, TEXT("No night lights file at %s"), *Path);
		return;
	}

	// kind,x,y,z,yaw,pitch,width_cm,height_cm,lumens,kelvin,radius_cm,shadow,map
	const FString MapName = UWorld::RemovePIEPrefix(World.GetMapName());
	int32 Streets = 0, Shops = 0;
	for (int32 i = 1; i < Lines.Num(); ++i)
	{
		TArray<FString> C;
		Lines[i].ParseIntoArray(C, TEXT(","), false);
		if (C.Num() < 13)
		{
			continue;
		}
		const FString& RowMap = C[12].TrimStartAndEnd();
		if (!RowMap.IsEmpty() && RowMap != TEXT("*") && RowMap != MapName)
		{
			continue;
		}
		const bool bShop = C[0] == TEXT("shop");
		const FVector Location(FCString::Atod(*C[1]), FCString::Atod(*C[2]), FCString::Atod(*C[3]));
		const FRotator Rotation(FCString::Atod(*C[5]), FCString::Atod(*C[4]), 0.0);

		ULocalLightComponent* Light = nullptr;
		if (bShop)
		{
			URectLightComponent* Rect = NewObject<URectLightComponent>(Holder);
			Rect->SourceWidth = FCString::Atof(*C[6]);
			Rect->SourceHeight = FCString::Atof(*C[7]);
			Rect->BarnDoorAngle = 70.f;
			Rect->BarnDoorLength = 30.f;
			Light = Rect;
			++Shops;
		}
		else
		{
			UPointLightComponent* Point = NewObject<UPointLightComponent>(Holder);
			Point->SourceRadius = 12.f;
			Light = Point;
			++Streets;
		}
		Light->SetMobility(EComponentMobility::Movable);
		Light->IntensityUnits = ELightUnits::Lumens;
		Light->Intensity = FCString::Atof(*C[8]);
		Light->bUseTemperature = true;
		Light->Temperature = FCString::Atof(*C[9]);
		Light->AttenuationRadius = FCString::Atof(*C[10]);
		Light->CastShadows = false;
		Light->SetVisibility(false);
		Light->SetupAttachment(Holder->GetRootComponent());
		Light->SetRelativeLocationAndRotation(Location, Rotation);
		Light->RegisterComponent();

		FManagedLight& Entry = NightLights.AddDefaulted_GetRef();
		Entry.Light = Light;
		Entry.Location = Location;
		Entry.FullIntensity = Light->Intensity;
		Entry.bCanShadow = C[11].TrimStartAndEnd() == TEXT("1");
	}
	UE_LOG(LogGTPEnvironment, Log, TEXT("Night lights for %s: %d street lamps, %d shop windows"), *MapName, Streets, Shops);
}

void UGTPEnvironmentSubsystem::AdoptTaggedLights(UWorld& World)
{
	int32 Adopted = 0;
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		if (!It->Tags.Contains(StreetLightTag) && !It->Tags.Contains(ShopLightTag))
		{
			continue;
		}
		TInlineComponentArray<ULocalLightComponent*> Lights(*It);
		for (ULocalLightComponent* Light : Lights)
		{
			Light->SetMobility(EComponentMobility::Movable);
			FManagedLight& Entry = NightLights.AddDefaulted_GetRef();
			Entry.Light = Light;
			Entry.Location = Light->GetComponentLocation();
			Entry.FullIntensity = Light->Intensity;
			Entry.bCanShadow = It->Tags.Contains(StreetLightTag);
			Light->SetCastShadows(false);
			Light->SetVisibility(false);
			++Adopted;
		}
	}
	if (Adopted > 0)
	{
		UE_LOG(LogGTPEnvironment, Log, TEXT("Night lights: %d tagged lights from the map"), Adopted);
	}
}

void UGTPEnvironmentSubsystem::ApplyBudgetForProfile(UWorld& World)
{
	FString Profile = TEXT("m1");
	if (const UGameInstance* GameInstance = World.GetGameInstance())
	{
		if (const UGTPGraphicsProfile* Graphics = GameInstance->GetSubsystem<UGTPGraphicsProfile>())
		{
			Profile = Graphics->GetActiveProfile();
		}
	}
	const FGTPLightBudget* Found = LightBudgets.FindByPredicate([&](const FGTPLightBudget& B) { return B.Profile == Profile; });
	if (!Found)
	{
		Found = LightBudgets.FindByPredicate([](const FGTPLightBudget& B) { return B.Profile == TEXT("m1"); });
	}
	Budget = Found ? *Found : FGTPLightBudget();
	Budget.Profile = Profile;

	// -CloudDome=1 forces the 2D cloud fallback (measurements), -CloudDome=0 forbids it.
	int32 CloudDomeFlag = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("CloudDome="), CloudDomeFlag) && CloudDomeFlag >= 0)
	{
		Budget.bCloudDome = CloudDomeFlag != 0;
	}
	// -Sky= sets the three sky switches at once for A/B measurements.
	FString Sky;
	if (ParseLastValue(FCommandLine::Get(), TEXT("Sky="), Sky))
	{
		Sky = Sky.ToLower();
		const bool bVolumetric = Sky == TEXT("volumetric");
		const bool bLightFunction = Sky == TEXT("lightfunction");
		const bool bCheap = Sky == TEXT("cheap");
		if (bVolumetric || bLightFunction || bCheap)
		{
			Budget.bVolumetricClouds = true;
			Budget.bCloudDome = bCheap;
			Budget.bCloudDomeWhenOvercast = false;
			// Kept on: a loaded light function turns it off in ApplyCloudQuality, a missing one
			// falls back to the volumetric shadow instead of no cloud shadow at all.
			Budget.bCloudShadows = true;
			Budget.bCloudShadowLightFunction = !bVolumetric;
		}
		else
		{
			UE_LOG(LogGTPEnvironment, Warning, TEXT("Unknown -Sky=%s (volumetric, lightfunction, cheap)"), *Sky);
		}
	}
	// -NightClouds=volumetric|dome|half for night A/B measurements.
	FString NightClouds;
	if (ParseLastValue(FCommandLine::Get(), TEXT("NightClouds="), NightClouds))
	{
		NightClouds = NightClouds.ToLower();
		if (NightClouds == TEXT("volumetric"))
		{
			Budget.bCloudDomeAtNight = false;
		}
		else if (NightClouds == TEXT("dome"))
		{
			Budget.bCloudDomeAtNight = true;
		}
		else if (NightClouds == TEXT("half"))
		{
			Budget.bCloudDomeAtNight = false;
			Budget.NightCloudSampleScale = 0.5f;
		}
		else if (NightClouds == TEXT("full"))
		{
			// The 0.3 behaviour: full samples day and night.
			Budget.bCloudDomeAtNight = false;
			Budget.NightCloudSampleScale = 1.f;
		}
		else
		{
			UE_LOG(LogGTPEnvironment, Warning, TEXT("Unknown -NightClouds=%s (volumetric, dome, half, full)"), *NightClouds);
		}
	}
	int32 LightFunctionFlag = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("CloudShadowLF="), LightFunctionFlag) && LightFunctionFlag >= 0)
	{
		Budget.bCloudShadowLightFunction = LightFunctionFlag != 0;
	}
	// Night light budget overrides for measurements (-NightLights=0 switches them all off).
	int32 IntFlag = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("NightLights="), IntFlag) && IntFlag >= 0)
	{
		Budget.MaxLights = IntFlag;
	}
	IntFlag = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("NightShadows="), IntFlag) && IntFlag >= 0)
	{
		Budget.MaxShadowedLights = IntFlag;
	}
	IntFlag = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("LitCars="), IntFlag) && IntFlag >= 0)
	{
		Budget.MaxLitCars = IntFlag;
	}
	float DistanceFlag = -1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("NightDistance="), DistanceFlag) && DistanceFlag > 0.f)
	{
		Budget.MaxDistance = DistanceFlag;
	}
	int32 CloudShadowsFlag = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("CloudShadows="), CloudShadowsFlag) && CloudShadowsFlag >= 0)
	{
		Budget.bCloudShadows = CloudShadowsFlag != 0;
	}
}

void UGTPEnvironmentSubsystem::ApplyCloudQuality()
{
	// Volumetric clouds are the most expensive part of the sky on the M1 (about 12 ms
	// measured): quality per profile and per weather, values owned by the optimisation work.
	// The 2D dome is created whenever the profile may use it, and shown by UpdateClouds.
	if ((Budget.bCloudDome || Budget.bCloudDomeWhenOvercast || Budget.bCloudDomeAtNight) && Holder)
	{
		UStaticMesh* Mesh = CloudDomeMesh.LoadSynchronous();
		UMaterialInterface* Material = CloudDomeMaterial.LoadSynchronous();
		if (Mesh && Material)
		{
			CloudDome = NewObject<UStaticMeshComponent>(Holder, TEXT("CloudDome"));
			CloudDome->SetMobility(EComponentMobility::Movable);
			CloudDome->SetStaticMesh(Mesh);
			CloudDome->SetMaterial(0, Material);
			CloudDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			CloudDome->SetCastShadow(false);
			CloudDome->bReceivesDecals = false;
			CloudDome->SetWorldScale3D(CloudDomeScale);
			CloudDome->SetVisibility(false);
			CloudDome->SetupAttachment(Holder->GetRootComponent());
			CloudDome->RegisterComponent();
		}
		else
		{
			UE_LOG(LogGTPEnvironment, Warning, TEXT("Cloud dome mesh or material missing (%s, %s)"),
				*CloudDomeMesh.ToString(), *CloudDomeMaterial.ToString());
		}
	}
	if (Sun.IsValid() && Budget.bCloudShadowLightFunction)
	{
		if (UMaterialInterface* Material = CloudShadowMaterial.LoadSynchronous())
		{
			CloudShadowInstance = UMaterialInstanceDynamic::Create(Material, this);
			Sun->SetLightFunctionMaterial(CloudShadowInstance);
			// Beyond this distance a light function falls back to DisabledBrightness (0.5 by
			// default): keep the whole city inside it and neutral outside.
			Sun->LightFunctionFadeDistance = 1000000.f;
			Sun->DisabledBrightness = 1.f;
			Sun->MarkRenderStateDirty();
			Budget.bCloudShadows = false;
			CloudShadowState = TEXT("loaded");
		}
		else
		{
			CloudShadowState = TEXT("missing");
			UE_LOG(LogGTPEnvironment, Warning, TEXT("Cloud shadow light function %s missing"), *CloudShadowMaterial.ToString());
		}
	}
	if (Sun.IsValid() && !Budget.bCloudShadows)
	{
		if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(UDirectionalLightComponent::StaticClass(), TEXT("bCastCloudShadows")))
		{
			Prop->SetPropertyValue_InContainer(Sun.Get(), false);
			Sun->MarkRenderStateDirty();
		}
	}
}

void UGTPEnvironmentSubsystem::UpdateClouds(const FGTPWeatherPreset& Weather)
{
	// 0 none, 1 volumetric, 2 dome. Without the dome assets the volumetric clouds stay.
	int32 Mode = Budget.bVolumetricClouds ? 1 : 0;
	const bool bNightClouds = SunPosition.ElevationDeg < Budget.NightCloudSunElevation;
	if (CloudDome && (Budget.bCloudDome || (Budget.bCloudDomeWhenOvercast && Weather.bPreferCloudDome)
		|| (Budget.bCloudDomeAtNight && bNightClouds)))
	{
		Mode = 2;
	}
	else if (Budget.bCloudDomeAtNight && bNightClouds)
	{
		// No dome assets: at night the volumetric clouds are close to invisible (measured
		// 2026-11-11) and cost up to 24 ms at p95 on the M1, so none rather than volumetric.
		Mode = 0;
	}
	if (Mode != AppliedCloudMode)
	{
		AppliedCloudMode = Mode;
		if (Clouds.IsValid())
		{
			Clouds->SetVisibility(Mode == 1);
		}
		if (CloudDome)
		{
			CloudDome->SetVisibility(Mode == 2);
		}
	}
	const float RampHigh = FMath::Max(Budget.NightCloudRampStart, Budget.NightCloudSunElevation + 0.1f);
	const float NightRamp = 1.f - FMath::SmoothStep(Budget.NightCloudSunElevation, RampHigh, static_cast<float>(SunPosition.ElevationDeg));
	const float SampleScale = Budget.CloudSampleScale * Weather.CloudSampleScale
		* FMath::Lerp(1.f, Budget.NightCloudSampleScale, NightRamp);
	if (Mode == 1 && Clouds.IsValid() && FMath::Abs(SampleScale - AppliedCloudSampleScale) > 0.05f)
	{
		AppliedCloudSampleScale = SampleScale;
		for (const TCHAR* Setter : { TEXT("SetViewSampleCountScale"), TEXT("SetReflectionViewSampleCountScale"), TEXT("SetShadowViewSampleCountScale") })
		{
			if (!CallFloatSetter(Clouds.Get(), Setter, SampleScale))
			{
				UE_LOG(LogGTPEnvironment, Warning, TEXT("Cloud setter %s not found"), Setter);
			}
		}
	}
}

void UGTPEnvironmentSubsystem::ReadCommandLine()
{
	const TCHAR* Cmd = FCommandLine::Get();
	float Value = 0.f;
	FString Text;
	FDateTime Date;

	// A test ambience first, so the single options below can still override it.
	if (FParse::Value(Cmd, TEXT("Ambience="), Text))
	{
		const FGTPAmbience* Ambience = Ambiences.FindByPredicate([&Text](const FGTPAmbience& A) { return A.Name == FName(*Text); });
		if (Ambience && ParseDate(Ambience->Date, Date) && FindPreset(Ambience->Weather))
		{
			CurrentDate = Date;
			LocalHours = FMath::Clamp(static_cast<double>(Ambience->Hour), 0.0, 23.999);
			FromWeather = ToWeather = Ambience->Weather;
			bAutoWeather = false;
			TimeScale = 0.f;
			UE_LOG(LogGTPEnvironment, Log, TEXT("Test ambience %s"), *Text);
		}
		else
		{
			UE_LOG(LogGTPEnvironment, Warning, TEXT("Unknown or invalid ambience '%s'"), *Text);
		}
	}
	if (FParse::Value(Cmd, TEXT("TimeOfDay="), Value))
	{
		LocalHours = FMath::Clamp(static_cast<double>(Value), 0.0, 23.999);
	}
	if (FParse::Value(Cmd, TEXT("TimeScale="), Value))
	{
		TimeScale = FMath::Max(0.f, Value);
	}
	if (FParse::Value(Cmd, TEXT("Date="), Text) && ParseDate(Text, Date))
	{
		CurrentDate = Date;
	}
	if (FParse::Value(Cmd, TEXT("Weather="), Text))
	{
		if (FindPreset(FName(*Text)))
		{
			FromWeather = ToWeather = FName(*Text);
			bAutoWeather = false;
		}
		else
		{
			UE_LOG(LogGTPEnvironment, Warning, TEXT("Unknown weather '%s' on the command line"), *Text);
		}
	}
}

// --------------------------------------------------------------------------- control

void UGTPEnvironmentSubsystem::SetTimeOfDay(double Hours)
{
	LocalHours = FMath::Fmod(FMath::Max(0.0, Hours), 24.0);
	if (bActive)
	{
		ApplySunAndMoon(true);
	}
}

void UGTPEnvironmentSubsystem::SetDate(const FDateTime& Date)
{
	CurrentDate = FDateTime(Date.GetYear(), Date.GetMonth(), Date.GetDay());
	if (bActive)
	{
		ApplySunAndMoon(true);
	}
}

bool UGTPEnvironmentSubsystem::SetWeather(FName Preset, float TransitionSeconds)
{
	if (!FindPreset(Preset))
	{
		return false;
	}
	if (TransitionSeconds <= 0.f)
	{
		FromWeather = ToWeather = Preset;
		WeatherAlpha = 1.f;
	}
	else
	{
		// Start from the current blend's target so a change mid-transition does not jump.
		FromWeather = WeatherAlpha < 0.5f ? FromWeather : ToWeather;
		ToWeather = Preset;
		WeatherAlpha = 0.f;
		WeatherBlendSpeed = 1.f / TransitionSeconds;
	}
	HoursToNextWeather = WeatherChangeHours;
	return true;
}

const FGTPWeatherPreset* UGTPEnvironmentSubsystem::FindPreset(FName Name) const
{
	return WeatherPresets.FindByPredicate([Name](const FGTPWeatherPreset& P) { return P.Name == Name; });
}

FGTPWeatherPreset UGTPEnvironmentSubsystem::BlendedWeather() const
{
	const FGTPWeatherPreset* A = FindPreset(FromWeather);
	const FGTPWeatherPreset* B = FindPreset(ToWeather);
	if (!A || !B)
	{
		return A ? *A : (B ? *B : FGTPWeatherPreset());
	}
	const float T = FMath::SmoothStep(0.f, 1.f, WeatherAlpha);
	FGTPWeatherPreset Out = T < 0.5f ? *A : *B;
	Out.CloudCoverage = FMath::Lerp(A->CloudCoverage, B->CloudCoverage, T);
	Out.SunScale = FMath::Lerp(A->SunScale, B->SunScale, T);
	Out.SkyScale = FMath::Lerp(A->SkyScale, B->SkyScale, T);
	Out.SunSourceAngle = FMath::Lerp(A->SunSourceAngle, B->SunSourceAngle, T);
	Out.FogDensity = FMath::Lerp(A->FogDensity, B->FogDensity, T);
	Out.FogHeightFalloff = FMath::Lerp(A->FogHeightFalloff, B->FogHeightFalloff, T);
	Out.RiverFog = FMath::Lerp(A->RiverFog, B->RiverFog, T);
	// Rain starts late and stops early in a transition: clouds come first.
	Out.Rain = FMath::Lerp(A->Rain, B->Rain, FMath::Clamp(T * 1.6f - 0.6f * (B->Rain > A->Rain ? 1.f : 0.f), 0.f, 1.f));
	Out.ExposureOffset = FMath::Lerp(A->ExposureOffset, B->ExposureOffset, T);
	Out.Saturation = FMath::Lerp(A->Saturation, B->Saturation, T);
	Out.CloudSampleScale = FMath::Lerp(A->CloudSampleScale, B->CloudSampleScale, T);
	const float CoverA = A->CloudShadowCoverage >= 0.f ? A->CloudShadowCoverage : A->CloudCoverage;
	const float CoverB = B->CloudShadowCoverage >= 0.f ? B->CloudShadowCoverage : B->CloudCoverage;
	Out.CloudShadowCoverage = FMath::Lerp(CoverA, CoverB, T);
	Out.CloudShadowStrength = FMath::Lerp(A->CloudShadowStrength, B->CloudShadowStrength, T);
	Out.CloudShadowMean = FMath::Lerp(A->CloudShadowMean, B->CloudShadowMean, T);
	return Out;
}

FDateTime UGTPEnvironmentSubsystem::LocalDateTime() const
{
	return CurrentDate + FTimespan::FromHours(LocalHours);
}

FString UGTPEnvironmentSubsystem::Describe() const
{
	const FDateTime Local = LocalDateTime();
	const FDateTime Utc = GTPSolar::LocalToUtc(Local);
	int32 On = 0, Shadowed = 0, LitCars = 0;
	for (const FCarLights& C : CarLights)
	{
		LitCars += C.bOn ? 1 : 0;
	}
	for (const FManagedLight& L : NightLights)
	{
		On += L.bOn ? 1 : 0;
		Shadowed += L.bShadow ? 1 : 0;
	}
	return FString::Printf(
		TEXT("Lyon %s (UTC+%d) x%.0f | sun %.1f deg az %.0f | moon %.1f deg %.0f %% | weather %s%s%s %.0f %% | wet %.2f | EV100 %.1f | night %.2f | lights %d/%d on, %d shadowed (budget %s %d/%d) | cars lit %d/%d"),
		*Local.ToString(TEXT("%Y-%m-%d %H:%M")), GTPSolar::ParisUtcOffsetHours(Utc), TimeScale,
		SunPosition.ElevationDeg, SunPosition.AzimuthDeg, MoonPosition.ElevationDeg, MoonIllumination * 100.0,
		*FromWeather.ToString(), FromWeather == ToWeather ? TEXT("") : TEXT(" -> "), FromWeather == ToWeather ? TEXT("") : *ToWeather.ToString(),
		WeatherAlpha * 100.f, Wetness, ExposureTarget, NightFactor, On, NightLights.Num(), Shadowed,
		*Budget.Profile, Budget.MaxLights, Budget.MaxShadowedLights, LitCars, CarLights.Num());
}

void UGTPEnvironmentSubsystem::WriteStatusFile() const
{
	FString Path;
	if (!FParse::Value(FCommandLine::Get(), TEXT("EnvReport="), Path))
	{
		Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Environment"), TEXT("status.txt"));
	}
	const FGTPWeatherPreset Weather = BlendedWeather();
	FString Text = FString::Printf(TEXT("daynight=on\nprofile=%s\nvolumetric_clouds=%d\ncloud_dome=%d\ncloud_dome_when_overcast=%d\n")
		TEXT("cloud_shadow_map=%d\ncloud_shadow_light_function=%s\nlf_mean=%.3f\nlf_strength=%.3f\nlf_coverage=%.3f\nlf_contrast=%.2f\n")
		TEXT("cvar_mean=%.3f\ncvar_strength=%.3f\ncvar_coverage=%.3f\nnight_budget=%d lights, %d shadowed, %.0f cm, %d lit cars\nnight_clouds=%s, samples x%.2f below %.1f deg (ramp from %.1f deg)\ncloud_mode_applied=%s\ncloud_dome_component=%s\nstatus=%s\n"),
		*Budget.Profile, Budget.bVolumetricClouds ? 1 : 0, Budget.bCloudDome ? 1 : 0, Budget.bCloudDomeWhenOvercast ? 1 : 0,
		Budget.bCloudShadows ? 1 : 0, *CloudShadowState, Weather.CloudShadowMean, Weather.CloudShadowStrength,
		Weather.CloudShadowCoverage, CVarCloudShadowContrast.GetValueOnGameThread(),
		CVarCloudShadowMean.GetValueOnGameThread(), CVarCloudShadowStrength.GetValueOnGameThread(),
		CVarCloudShadowCoverage.GetValueOnGameThread(), Budget.MaxLights, Budget.MaxShadowedLights,
		Budget.MaxDistance, Budget.MaxLitCars, Budget.bCloudDomeAtNight ? TEXT("dome") : TEXT("volumetric"),
		Budget.NightCloudSampleScale, Budget.NightCloudSunElevation, Budget.NightCloudRampStart,
		AppliedCloudMode == 2 ? TEXT("dome") : (AppliedCloudMode == 1 ? TEXT("volumetric") : (AppliedCloudMode == 0 ? TEXT("none") : TEXT("not yet"))),
		CloudDome ? TEXT("yes") : TEXT("no (dome assets missing: run setup-environment-mac.sh before packaging)"), *Describe());
	if (!FFileHelper::SaveStringToFile(Text, *Path))
	{
		GTP_TRACE(LogGTPEnvironment, TEXT("daynight: cannot write %s"), *Path);
	}
	else
	{
		GTP_TRACE(LogGTPEnvironment, TEXT("daynight: status in %s"), *FPaths::ConvertRelativePathToFull(Path));
	}
}

// --------------------------------------------------------------------------- tick

void UGTPEnvironmentSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bActive || !Sun.IsValid())
	{
		return;
	}
	AdvanceTime(DeltaTime);
	UpdateWeather(DeltaTime);
	ApplySunAndMoon(false);
	ApplyAtmosphere();
	ApplyExposure();
	ApplyParameters();
	UpdateRain();

	LightTimer -= DeltaTime;
	if (LightTimer <= 0.f)
	{
		LightTimer = LightUpdateInterval;
		CarScanTimer -= LightUpdateInterval;
		if (CarScanTimer <= 0.f)
		{
			// Traffic and police spawn and despawn cars: look for new ones every second.
			CarScanTimer = 1.f;
			FindNewCars();
		}
		UpdateNightLights();
	}

	if (StatusRewriteTimer > 0.f)
	{
		StatusRewriteTimer -= DeltaTime;
		if (StatusRewriteTimer <= 0.f)
		{
			WriteStatusFile();
		}
	}
}

void UGTPEnvironmentSubsystem::AdvanceTime(float DeltaTime)
{
	const double GameHours = DeltaTime * TimeScale / 3600.0;
	LocalHours += GameHours;
	while (LocalHours >= 24.0)
	{
		LocalHours -= 24.0;
		CurrentDate += FTimespan::FromDays(1.0);
	}
	if (bAutoWeather && WeatherPresets.Num() > 1 && GameHours > 0.0)
	{
		HoursToNextWeather -= GameHours;
		if (HoursToNextWeather <= 0.0)
		{
			float Total = 0.f;
			for (const FGTPWeatherPreset& P : WeatherPresets)
			{
				Total += FMath::Max(0.f, P.Weight);
			}
			float Pick = WeatherRandom.FRandRange(0.f, Total);
			for (const FGTPWeatherPreset& P : WeatherPresets)
			{
				Pick -= FMath::Max(0.f, P.Weight);
				if (Pick <= 0.f)
				{
					if (P.Name != ToWeather)
					{
						SetWeather(P.Name, WeatherTransitionSeconds);
					}
					break;
				}
			}
			HoursToNextWeather = WeatherChangeHours;
		}
	}
}

void UGTPEnvironmentSubsystem::UpdateWeather(float DeltaTime)
{
	if (WeatherAlpha < 1.f)
	{
		WeatherAlpha = FMath::Min(1.f, WeatherAlpha + DeltaTime * WeatherBlendSpeed);
		if (WeatherAlpha >= 1.f)
		{
			FromWeather = ToWeather;
		}
	}
	const float Rain = BlendedWeather().Rain;
	Wetness = Rain > 0.05f
		? FMath::Min(1.f, Wetness + WettingRate * Rain * DeltaTime)
		: FMath::Max(0.f, Wetness - DryingRate * DeltaTime);
}

void UGTPEnvironmentSubsystem::ApplySunAndMoon(bool bForce)
{
	const FDateTime Utc = GTPSolar::LocalToUtc(LocalDateTime());
	SunPosition = GTPSolar::Sun(Latitude, Longitude, Utc);
	MoonPosition = GTPSolar::Moon(Latitude, Longitude, Utc, MoonIllumination);
	const FGTPWeatherPreset Weather = BlendedWeather();

	// Moving the sun invalidates the virtual shadow map cache: move it in steps.
	const FRotator SunRotation = GTPSolar::LightRotation(SunPosition);
	const double StepRadians = FMath::DegreesToRadians(SunStepDegrees);
	if (bForce || AppliedSunRotation.Quaternion().AngularDistance(SunRotation.Quaternion()) >= StepRadians)
	{
		AppliedSunRotation = SunRotation;
		Sun->SetWorldRotation(SunRotation);
		if (MoonLight.IsValid())
		{
			MoonLight->SetWorldRotation(GTPSolar::LightRotation(MoonPosition));
		}
		if (SkyLight.IsValid() && !SkyLight->bRealTimeCapture)
		{
			SkyLight->RecaptureSky();
		}
	}

	// Below the horizon the atmosphere already blackens the sun; it is hidden
	// once the twilight is over and stops casting shadows at sunset.
	const float Elevation = static_cast<float>(SunPosition.ElevationDeg);
	const bool bSunVisible = Elevation > -10.f;
	if (Sun->IsVisible() != bSunVisible)
	{
		Sun->SetVisibility(bSunVisible);
	}
	const bool bSunShadows = Elevation > -1.f;
	if (static_cast<bool>(Sun->CastShadows) != bSunShadows)
	{
		Sun->SetCastShadows(bSunShadows);
	}
	Sun->SetIntensity(SunLux * Weather.SunScale);
	Sun->SetLightSourceAngle(Weather.SunSourceAngle);

	if (MoonLight.IsValid())
	{
		const bool bMoonVisible = Elevation < -4.f && MoonPosition.ElevationDeg > -2.0;
		if (MoonLight->IsVisible() != bMoonVisible)
		{
			MoonLight->SetVisibility(bMoonVisible);
		}
		MoonLight->SetIntensity(MoonLux * static_cast<float>(MoonIllumination) * (1.f - 0.85f * Weather.CloudCoverage));
	}

	// City lights come on earlier under a dark sky.
	const float EffectiveElevation = Elevation - 3.f * Weather.CloudCoverage;
	NightFactor = Saturate((LightsOnStartElevation - EffectiveElevation) / FMath::Max(0.1f, LightsOnStartElevation - LightsOnFullElevation));
}

float UGTPEnvironmentSubsystem::MorningMist() const
{
	const int32 Month = CurrentDate.GetMonth();
	const float Season = (Month >= 10 || Month <= 3) ? 1.f : 0.25f;
	const float Hour = static_cast<float>(LocalHours);
	return Season * Saturate(1.f - FMath::Abs(Hour - 7.5f) / 2.5f);
}

void UGTPEnvironmentSubsystem::ApplyAtmosphere()
{
	const FGTPWeatherPreset Weather = BlendedWeather();
	const float Mist = MorningMist();

	if (SkyLight.IsValid())
	{
		SkyLight->SetIntensity(SkyLightBaseIntensity * Weather.SkyScale);
	}
	if (HeightFog.IsValid())
	{
		HeightFog->SetFogDensity(Weather.FogDensity * (1.f + 1.5f * Mist));
		HeightFog->SetFogHeightFalloff(Weather.FogHeightFalloff);
		if (HeightFog->bEnableVolumetricFog != Budget.bVolumetricFog)
		{
			HeightFog->SetVolumetricFog(Budget.bVolumetricFog);
		}
		const FLinearColor Inscattering = FMath::Lerp(DayFogInscattering, NightFogInscattering, NightFactor);
		if (!Inscattering.Equals(AppliedFogInscattering, 1e-4f) &&
			SetColorProperty(HeightFog.Get(), { TEXT("FogInscatteringLuminance"), TEXT("FogInscatteringColor") }, Inscattering))
		{
			AppliedFogInscattering = Inscattering;
			HeightFog->MarkRenderStateDirty();
		}
	}
	UpdateClouds(Weather);
	if (CloudMaterial && Budget.bDriveCloudCoverage)
	{
		for (const FName& Parameter : CloudCoverageParameters)
		{
			CloudMaterial->SetScalarParameterValue(Parameter, Weather.CloudCoverage);
		}
	}
	const float River = Saturate(Weather.RiverFog + MorningRiverFog * Mist);
	if (FMath::Abs(River - AppliedRiverFog) < 0.005f)
	{
		return;
	}
	AppliedRiverFog = River;
	for (UActorComponent* Volume : RiverFogVolumes)
	{
		if (USceneComponent* Scene = Cast<USceneComponent>(Volume))
		{
			const bool bVisible = River > 0.01f;
			if (Scene->IsVisible() != bVisible)
			{
				Scene->SetVisibility(bVisible);
			}
		}
		CallFloatSetter(Volume, TEXT("SetRadialFogExtinction"), RiverFogExtinction * River);
	}
}

float UGTPEnvironmentSubsystem::ExposureForElevation(float ElevationDeg) const
{
	if (ElevationDeg <= ExposureCurve[0].X)
	{
		return static_cast<float>(ExposureCurve[0].Y);
	}
	for (int32 i = 1; i < ExposureCurve.Num(); ++i)
	{
		if (ElevationDeg <= ExposureCurve[i].X)
		{
			const FVector2D& A = ExposureCurve[i - 1];
			const FVector2D& B = ExposureCurve[i];
			const double Alpha = (ElevationDeg - A.X) / FMath::Max(1e-3, B.X - A.X);
			return static_cast<float>(FMath::Lerp(A.Y, B.Y, Alpha));
		}
	}
	return static_cast<float>(ExposureCurve.Last().Y);
}

void UGTPEnvironmentSubsystem::ApplyExposure()
{
	if (!PostProcess)
	{
		return;
	}
	const FGTPWeatherPreset Weather = BlendedWeather();
	const float Elevation = static_cast<float>(SunPosition.ElevationDeg);
	// Cloud darkens the day; at night the street lights set the exposure.
	const float Daylight = Saturate((Elevation + 6.f) / 12.f);
	ExposureTarget = ExposureForElevation(Elevation) + Weather.ExposureOffset * Daylight;
	if (!Budget.bCloudShadows)
	{
		ExposureTarget += Budget.NoCloudShadowExposureOffset * Daylight;
	}

	FPostProcessSettings& S = PostProcess->Settings;
	S.AutoExposureMinBrightness = ExposureTarget - ExposureHalfRange;
	S.AutoExposureMaxBrightness = ExposureTarget + ExposureHalfRange;
	S.WhiteTemp = FMath::Lerp(DayWhiteTemp, NightWhiteTemp, NightFactor);
	S.ColorSaturation = FVector4(Weather.Saturation, Weather.Saturation, Weather.Saturation, 1.f);
}

void UGTPEnvironmentSubsystem::ApplyParameters()
{
	UWorld* World = GetWorld();
	UMaterialParameterCollectionInstance* Instance = (World && LoadedCollection) ? World->GetParameterCollectionInstance(LoadedCollection) : nullptr;
	if (!Instance)
	{
		return;
	}
	const FGTPWeatherPreset Weather = BlendedWeather();
	Instance->SetScalarParameterValue(TEXT("Wetness"), Wetness);
	Instance->SetScalarParameterValue(TEXT("Puddles"), FMath::SmoothStep(0.55f, 1.f, Wetness));
	Instance->SetScalarParameterValue(TEXT("RainIntensity"), Weather.Rain);
	Instance->SetScalarParameterValue(TEXT("CityLights"), NightFactor);
	Instance->SetScalarParameterValue(TEXT("EmissiveScale"), FMath::Lerp(1.f, NightEmissiveScale, NightFactor));
	Instance->SetScalarParameterValue(TEXT("Cloudiness"), Weather.CloudCoverage);
	Instance->SetScalarParameterValue(TEXT("TimeOfDay"), static_cast<float>(LocalHours));
	if (CloudShadowInstance)
	{
		auto Pick = [](float Override, float Preset) { return Override >= 0.f ? Override : Preset; };
		const float Coverage = Pick(CVarCloudShadowCoverage.GetValueOnGameThread(),
			Weather.CloudShadowCoverage >= 0.f ? Weather.CloudShadowCoverage : Weather.CloudCoverage);
		CloudShadowInstance->SetScalarParameterValue(TEXT("Coverage"), FMath::Clamp(Coverage, 0.f, 1.f));
		CloudShadowInstance->SetScalarParameterValue(TEXT("Strength"),
			FMath::Clamp(Pick(CVarCloudShadowStrength.GetValueOnGameThread(), Weather.CloudShadowStrength), 0.f, 1.f));
		CloudShadowInstance->SetScalarParameterValue(TEXT("Mean"),
			FMath::Max(0.f, Pick(CVarCloudShadowMean.GetValueOnGameThread(), Weather.CloudShadowMean)));
		CloudShadowInstance->SetScalarParameterValue(TEXT("Contrast"), FMath::Max(0.1f, CVarCloudShadowContrast.GetValueOnGameThread()));
	}
	// Unlit cloud dome: luminance of the exposure's middle grey (0.125 x 2^EV100 nits), darker in the rain.
	Instance->SetScalarParameterValue(TEXT("CloudLuminance"),
		0.125f * FMath::Pow(2.f, ExposureTarget) * CloudDomeBrightness * FMath::Lerp(1.f, 0.5f, Weather.Rain)
		* FMath::Lerp(1.f, 0.45f, NightFactor));  // night clouds sit below middle grey
}

void UGTPEnvironmentSubsystem::UpdateRain()
{
	if (CloudDome)
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
		{
			CloudDome->SetWorldLocation(Camera->GetCameraLocation());
		}
	}
	if (!RainLayer)
	{
		return;
	}
	const bool bRaining = BlendedWeather().Rain > 0.02f;
	if (RainLayer->IsVisible() != bRaining)
	{
		RainLayer->SetVisibility(bRaining);
	}
	if (bRaining)
	{
		if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
		{
			RainLayer->SetWorldLocation(Camera->GetCameraLocation());
		}
	}
}

void UGTPEnvironmentSubsystem::FindNewCars()
{
	CarLights.RemoveAll([](const FCarLights& C) { return !C.Car.IsValid(); });
	for (TActorIterator<AGTPVehicle> It(GetWorld()); It; ++It)
	{
		AGTPVehicle* Car = *It;
		if (CarLights.ContainsByPredicate([Car](const FCarLights& C) { return C.Car.Get() == Car; }))
		{
			continue;
		}
		UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Car->GetRootComponent());
		if (!Body)
		{
			continue;
		}
		// Car frame: +X forward. Lamps at the corners of the body's local bounds.
		const FBox Box = Body->CalcBounds(FTransform::Identity).GetBox();
		const FVector Extent = Box.GetExtent();
		const FVector Centre = Box.GetCenter();
		const float LampZ = Centre.Z - 0.15f * Extent.Z;

		FCarLights& Entry = CarLights.AddDefaulted_GetRef();
		Entry.Car = Car;
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			USpotLightComponent* Head = NewObject<USpotLightComponent>(Car);
			Head->SetMobility(EComponentMobility::Movable);
			Head->IntensityUnits = ELightUnits::Candelas;
			Head->Intensity = HeadlightCandela;
			Head->bUseTemperature = true;
			Head->Temperature = HeadlightKelvin;
			Head->AttenuationRadius = HeadlightRadius;
			Head->InnerConeAngle = 0.4f * HeadlightOuterCone;
			Head->OuterConeAngle = HeadlightOuterCone;
			Head->SourceRadius = 6.f;
			Head->CastShadows = false;
			Head->SetVisibility(false);
			Head->SetupAttachment(Body);
			Head->SetRelativeLocationAndRotation(FVector(Box.Max.X - 5.f, Centre.Y + Side * 0.65f * Extent.Y, LampZ), FRotator(-3.f, 0.f, 0.f));
			Head->RegisterComponent();
			Entry.Lights.Add(Head);
		}
		UPointLightComponent* Tail = NewObject<UPointLightComponent>(Car);
		Tail->SetMobility(EComponentMobility::Movable);
		Tail->IntensityUnits = ELightUnits::Candelas;
		Tail->Intensity = TailLightCandela;
		Tail->AttenuationRadius = TailLightRadius;
		Tail->SetLightColor(FLinearColor(1.f, 0.05f, 0.03f));
		Tail->CastShadows = false;
		Tail->SetVisibility(false);
		Tail->SetupAttachment(Body);
		Tail->SetRelativeLocation(FVector(Box.Min.X - 10.f, Centre.Y, LampZ + 0.1f * Extent.Z));
		Tail->RegisterComponent();
		Entry.Lights.Add(Tail);
	}
}

void UGTPEnvironmentSubsystem::UpdateCarLights(const FVector& CameraLocation)
{
	TArray<TPair<float, int32>> Ranked;
	if (NightFactor > 0.3f)
	{
		for (int32 i = 0; i < CarLights.Num(); ++i)
		{
			if (const AActor* Car = CarLights[i].Car.Get())
			{
				const float Distance = FVector::Dist(Car->GetActorLocation(), CameraLocation);
				if (Distance <= Budget.MaxDistance)
				{
					Ranked.Emplace(Distance, i);
				}
			}
		}
		Ranked.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	}
	TArray<bool> Wanted;
	Wanted.Init(false, CarLights.Num());
	for (int32 k = 0; k < Ranked.Num() && k < Budget.MaxLitCars; ++k)
	{
		Wanted[Ranked[k].Value] = true;
	}
	for (int32 i = 0; i < CarLights.Num(); ++i)
	{
		FCarLights& Entry = CarLights[i];
		if (Entry.bOn == Wanted[i])
		{
			continue;
		}
		Entry.bOn = Wanted[i];
		for (const TWeakObjectPtr<ULocalLightComponent>& Light : Entry.Lights)
		{
			if (Light.IsValid())
			{
				Light->SetVisibility(Entry.bOn);
			}
		}
	}
}

void UGTPEnvironmentSubsystem::UpdateNightLights()
{
	FVector CameraLocation = FVector::ZeroVector;
	FVector CameraForward = FVector::ForwardVector;
	if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
	{
		CameraLocation = Camera->GetCameraLocation();
		CameraForward = Camera->GetCameraRotation().Vector();
	}
	UpdateCarLights(CameraLocation);
	if (NightLights.IsEmpty())
	{
		return;
	}

	// Rank the lights in range: nearest first, lights behind the camera count as farther.
	TArray<TPair<float, int32>> Ranked;
	TArray<float> Distances;
	Distances.SetNumUninitialized(NightLights.Num());
	if (NightFactor > 0.001f)
	{
		for (int32 i = 0; i < NightLights.Num(); ++i)
		{
			const FVector ToLight = NightLights[i].Location - CameraLocation;
			const float Distance = ToLight.Size();
			Distances[i] = Distance;
			if (Distance > Budget.MaxDistance || !NightLights[i].Light.IsValid())
			{
				continue;
			}
			const bool bBehind = Distance > 1500.f && FVector::DotProduct(ToLight, CameraForward) < 0.f;
			Ranked.Emplace(bBehind ? Distance * 2.5f : Distance, i);
		}
		Ranked.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	}

	TArray<uint8> Wanted;
	Wanted.SetNumZeroed(NightLights.Num());
	int32 ShadowsLeft = Budget.MaxShadowedLights;
	for (int32 k = 0; k < Ranked.Num() && k < Budget.MaxLights; ++k)
	{
		const int32 i = Ranked[k].Value;
		Wanted[i] = 1;
		if (ShadowsLeft > 0 && NightLights[i].bCanShadow)
		{
			Wanted[i] = 2;
			--ShadowsLeft;
		}
	}

	for (int32 i = 0; i < NightLights.Num(); ++i)
	{
		FManagedLight& Entry = NightLights[i];
		ULocalLightComponent* Light = Entry.Light.Get();
		if (!Light)
		{
			continue;
		}
		const bool bOn = Wanted[i] > 0;
		if (bOn != Entry.bOn)
		{
			Light->SetVisibility(bOn);
			Entry.bOn = bOn;
		}
		if (!bOn)
		{
			// Fade back in from the right intensity next time.
			Entry.AppliedScale = -1.f;
			continue;
		}
		const bool bShadow = Wanted[i] == 2;
		if (bShadow != Entry.bShadow)
		{
			Light->SetCastShadows(bShadow);
			Entry.bShadow = bShadow;
		}
		// Fade out over the last fifth of the range so lights do not pop.
		const float Fade = 1.f - FMath::SmoothStep(0.8f * Budget.MaxDistance, Budget.MaxDistance, Distances[i]);
		const float Scale = NightFactor * Fade;
		if (FMath::Abs(Scale - Entry.AppliedScale) > 0.02f)
		{
			Light->SetIntensity(Entry.FullIntensity * Scale);
			Entry.AppliedScale = Scale;
		}
	}
}
