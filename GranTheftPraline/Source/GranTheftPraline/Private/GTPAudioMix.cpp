#include "GTPAudioMix.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AudioDeviceManager.h"
#include "Components/AudioComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GTPAudioSettings.h"
#include "GTPLog.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundMix.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPAudio, Log, All);

static_assert(static_cast<uint8>(EGTPSurface::Asphalt) == GTPSurfaceId::Asphalt, "surface ids");
static_assert(static_cast<uint8>(EGTPSurface::Water) == GTPSurfaceId::Water, "surface ids");
static_assert(static_cast<uint8>(EGTPSurface::Count) == GTPSurfaceId::Count, "surface ids");

namespace
{
	constexpr int32 NumCategories = static_cast<int32>(EGTPAudioCategory::Count);
	constexpr int32 NumSurfaces = static_cast<int32>(EGTPSurface::Count);
	constexpr int32 NumImpactKinds = static_cast<int32>(EGTPImpactKind::Count);
	const TCHAR* TierNames[3] = {TEXT("Light"), TEXT("Medium"), TEXT("Heavy")};

	EGTPAudioCategory ParentOf(EGTPAudioCategory Category)
	{
		switch (Category)
		{
		case EGTPAudioCategory::Footsteps:
		case EGTPAudioCategory::Vehicles:
		case EGTPAudioCategory::Impacts:
		case EGTPAudioCategory::Sirens:
		case EGTPAudioCategory::Voice:
			return EGTPAudioCategory::Effects;
		case EGTPAudioCategory::Zones:
			// Zone layers of the bed follow the ambience (driving mix: lower and duller in a car).
			return EGTPAudioCategory::Ambience;
		default:
			return EGTPAudioCategory::Master;
		}
	}

	template <typename EnumType>
	FString EnumName(EnumType Value)
	{
		return StaticEnum<EnumType>()->GetNameStringByValue(static_cast<int64>(Value));
	}
}

UGTPAudioMix* UGTPAudioMix::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGTPAudioMix>() : nullptr;
}

bool UGTPAudioMix::IsGameAudioEnabled()
{
	static const bool bEnabled = []()
	{
		int32 Value = 1;
		FParse::Value(FCommandLine::Get(), TEXT("Audio="), Value);
		return Value != 0 && !FParse::Param(FCommandLine::Get(), TEXT("NoGameAudio"));
	}();
	return bEnabled;
}

bool UGTPAudioMix::IsPedestrian(const AActor* Actor)
{
	for (const UClass* Class = Actor ? Actor->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
	{
		if (Class->GetFName() == TEXT("GTPPedestrian"))
		{
			return true;
		}
	}
	return false;
}

void UGTPAudioMix::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!IsGameAudioEnabled())
	{
		GTP_TRACE(LogGTPAudio, TEXT("[Audio] game audio off (-Audio=0): no footsteps, impacts, squeal or sirens"));
		return;
	}
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	Volumes = Settings->CategoryVolumes;
	GroundKeyword = Settings->GroundMaterialKeyword;
	BuildClasses();
	BuildMixes();
	BuildConcurrencyAndAttenuation();
	LoadBanks();
	MuteFolders();
	LoadSurfaceMap();
}

void UGTPAudioMix::MuteFolders()
{
	const TArray<FString>& Folders = GetDefault<UGTPAudioSettings>()->MutedSoundFolders;
	if (Folders.Num() == 0)
	{
		return;
	}
	MutedClass = NewObject<USoundClass>(this, TEXT("SC_GTP_Muted"));
	MutedClass->Properties.Volume = 0.f;
	if (FAudioDeviceManager* Manager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr)
	{
		Manager->RegisterSoundClass(MutedClass);
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	int32 Muted = 0;
	for (const FString& Folder : Folders)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(*Folder), Assets, /*bRecursive*/ true);
		for (const FAssetData& Asset : Assets)
		{
			if (USoundBase* Sound = Cast<USoundBase>(Asset.GetAsset()))
			{
				Sound->SoundClassObject = MutedClass;
				AllSounds.Add(Sound);
				++Muted;
			}
		}
	}
	GTP_TRACE(LogGTPAudio, TEXT("[Audio] muted %d sounds under %s"), Muted, *FString::Join(Folders, TEXT(", ")));
}

void UGTPAudioMix::Deinitialize()
{
	if (FAudioDeviceManager* Manager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr)
	{
		for (USoundClass* Class : Classes)
		{
			Manager->UnregisterSoundClass(Class);
		}
	}
	Super::Deinitialize();
}

void UGTPAudioMix::BuildClasses()
{
	Classes.SetNum(NumCategories);
	for (int32 Index = 0; Index < NumCategories; ++Index)
	{
		const EGTPAudioCategory Category = static_cast<EGTPAudioCategory>(Index);
		Classes[Index] = NewObject<USoundClass>(this, FName(*(TEXT("SC_GTP_") + EnumName(Category))));
	}
	for (int32 Index = 0; Index < NumCategories; ++Index)
	{
		const EGTPAudioCategory Category = static_cast<EGTPAudioCategory>(Index);
		if (Category == EGTPAudioCategory::Master)
		{
			continue;
		}
		USoundClass* Parent = Classes[static_cast<int32>(ParentOf(Category))];
		Classes[Index]->ParentClass = Parent;
		Parent->ChildClasses.AddUnique(Classes[Index]);
	}
	// Ambience and the interface keep playing over the pause menu.
	Classes[static_cast<int32>(EGTPAudioCategory::Ambience)]->Properties.bIsUISound = true;
	Classes[static_cast<int32>(EGTPAudioCategory::Interface)]->Properties.bIsUISound = true;
	Classes[static_cast<int32>(EGTPAudioCategory::Zones)]->Properties.bIsUISound = true;
	Classes[static_cast<int32>(EGTPAudioCategory::Weather)]->Properties.bIsUISound = true;
	if (FAudioDeviceManager* Manager = GEngine ? GEngine->GetAudioDeviceManager() : nullptr)
	{
		for (USoundClass* Class : Classes)
		{
			Manager->RegisterSoundClass(Class);
		}
	}
}

void UGTPAudioMix::BuildMixes()
{
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();

	UserMix = NewObject<USoundMix>(this, TEXT("SM_GTP_User"));
	UserMix->Duration = -1.f;
	UserMix->FadeInTime = 0.f;
	UserMix->FadeOutTime = 0.f;
	for (int32 Index = 0; Index < NumCategories; ++Index)
	{
		FSoundClassAdjuster Adjuster;
		Adjuster.SoundClassObject = Classes[Index];
		Adjuster.VolumeAdjuster = GetCategoryVolume(static_cast<EGTPAudioCategory>(Index));
		Adjuster.bApplyToChildren = false;
		UserMix->SoundClassEffects.Add(Adjuster);
	}

	DrivingMix = NewObject<USoundMix>(this, TEXT("SM_GTP_Driving"));
	DrivingMix->Duration = -1.f;
	DrivingMix->FadeInTime = Settings->MixFadeTime;
	DrivingMix->FadeOutTime = Settings->MixFadeTime;
	FSoundClassAdjuster Ambience;
	Ambience.SoundClassObject = Classes[static_cast<int32>(EGTPAudioCategory::Ambience)];
	Ambience.VolumeAdjuster = Settings->DrivingAmbienceVolume;
	Ambience.LowPassFilterFrequency = Settings->DrivingAmbienceLowPass;
	// Reaches the zone layers too; the street-bed mix and the user mix stay on the Ambience class alone.
	Ambience.bApplyToChildren = true;
	DrivingMix->SoundClassEffects.Add(Ambience);
}

void UGTPAudioMix::BuildConcurrencyAndAttenuation()
{
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	Concurrency.SetNum(NumCategories);
	Attenuation.SetNum(NumCategories);

	auto MakeConcurrency = [this](EGTPAudioCategory Category, int32 MaxCount)
	{
		USoundConcurrency* Group = NewObject<USoundConcurrency>(this, FName(*(TEXT("SCon_GTP_") + EnumName(Category))));
		Group->Concurrency.MaxCount = FMath::Max(1, MaxCount);
		Group->Concurrency.bLimitToOwner = false;
		Group->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopFarthestThenOldest;
		Concurrency[static_cast<int32>(Category)] = Group;
	};
	MakeConcurrency(EGTPAudioCategory::Footsteps, Settings->MaxFootstepVoices);
	MakeConcurrency(EGTPAudioCategory::Impacts, Settings->MaxImpactVoices);
	MakeConcurrency(EGTPAudioCategory::Sirens, Settings->MaxSirenVoices);
	// Tyre squeal loops share the vehicle category budget with nothing else.
	MakeConcurrency(EGTPAudioCategory::Vehicles, Settings->MaxSkidVoices);
	MakeConcurrency(EGTPAudioCategory::Voice, Settings->MaxVoiceVoices);
	MakeConcurrency(EGTPAudioCategory::Radio, 1);
	MakeConcurrency(EGTPAudioCategory::Interface, 3);

	auto MakeAttenuation = [this](EGTPAudioCategory Category, const FVector2D& Range, float FarLowPass)
	{
		USoundAttenuation* Preset = NewObject<USoundAttenuation>(this, FName(*(TEXT("SA_GTP_") + EnumName(Category))));
		FSoundAttenuationSettings& A = Preset->Attenuation;
		A.bAttenuate = true;
		A.bSpatialize = true;
		A.AttenuationShape = EAttenuationShape::Sphere;
		A.AttenuationShapeExtents = FVector(FMath::Max(0.f, Range.X), 0.f, 0.f);
		A.FalloffDistance = FMath::Max(100.f, Range.Y - Range.X);
		A.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		// Far sounds lose their highs, like in a street.
		A.bAttenuateWithLPF = true;
		A.LPFRadiusMin = Range.X;
		A.LPFRadiusMax = Range.Y;
		A.LPFFrequencyAtMin = 20000.f;
		A.LPFFrequencyAtMax = FarLowPass;
		Attenuation[static_cast<int32>(Category)] = Preset;
	};
	MakeAttenuation(EGTPAudioCategory::Footsteps, Settings->FootstepRange, 4000.f);
	MakeAttenuation(EGTPAudioCategory::Impacts, Settings->ImpactRange, 2500.f);
	MakeAttenuation(EGTPAudioCategory::Sirens, Settings->SirenRange, 2000.f);
	MakeAttenuation(EGTPAudioCategory::Vehicles, Settings->SkidRange, 3000.f);
	MakeAttenuation(EGTPAudioCategory::Voice, Settings->VoiceRange, 3000.f);
}

void UGTPAudioMix::LoadBanks()
{
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	const FString Root = Settings->SoundRoot;
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(*Root), Assets, /*bRecursive*/ true);

	// Folder names -> enum values.
	TMap<FString, int32> SurfaceByName;
	for (int32 Index = 0; Index < NumSurfaces; ++Index)
	{
		SurfaceByName.Add(EnumName(static_cast<EGTPSurface>(Index)), Index);
	}
	TMap<FString, int32> KindByName;
	for (int32 Index = 0; Index < NumImpactKinds; ++Index)
	{
		KindByName.Add(EnumName(static_cast<EGTPImpactKind>(Index)), Index);
	}

	// Folders of one bank per enum value: Voices/Scream, Radio/Chatter, UI/Confirm...
	auto FindNamed = [](const FString& Name, FBank* Banks, const UEnum* Enum, int32 Count) -> FBank*
	{
		const int64 Value = Enum->GetValueByNameString(Name);
		return Value >= 0 && Value < Count ? &Banks[Value] : nullptr;
	};

	USoundClass* FootstepClass = GetClass(EGTPAudioCategory::Footsteps);
	USoundClass* ImpactClass = GetClass(EGTPAudioCategory::Impacts);
	int32 Unsorted = 0;
	for (const FAssetData& Asset : Assets)
	{
		FString Relative = Asset.PackagePath.ToString();
		if (!Relative.RemoveFromStart(Root))
		{
			continue;
		}
		TArray<FString> Parts;
		Relative.ParseIntoArray(Parts, TEXT("/"), true);
		if (Parts.Num() < 1)
		{
			continue;
		}
		FBank* Bank = nullptr;
		USoundClass* Class = nullptr;
		if ((Parts[0] == TEXT("Footsteps") || Parts[0] == TEXT("Land")) && Parts.Num() >= 2)
		{
			if (const int32* Surface = SurfaceByName.Find(Parts[1]))
			{
				Bank = Parts[0] == TEXT("Land") ? &Lands[*Surface] : &Steps[*Surface];
				Class = FootstepClass;
			}
		}
		else if (Parts[0] == TEXT("Impacts") && Parts.Num() >= 2)
		{
			if (Parts[1] == TEXT("Glass"))
			{
				Bank = &Glass;
			}
			else if (const int32* Kind = KindByName.Find(Parts[1]); Kind && Parts.Num() >= 3)
			{
				for (int32 Tier = 0; Tier < 3; ++Tier)
				{
					if (Parts[2] == TierNames[Tier])
					{
						Bank = &Impacts[*Kind * 3 + Tier];
					}
				}
			}
			Class = ImpactClass;
		}
		else if (Parts.Num() >= 2 && Parts[0] == TEXT("Voices"))
		{
			Bank = FindNamed(Parts[1], Voices, StaticEnum<EGTPVoiceKind>(), UE_ARRAY_COUNT(Voices));
			Class = GetClass(EGTPAudioCategory::Voice);
		}
		else if (Parts.Num() >= 2 && Parts[0] == TEXT("Radio"))
		{
			Bank = FindNamed(Parts[1], Radio, StaticEnum<EGTPRadioCall>(), UE_ARRAY_COUNT(Radio));
			Class = GetClass(EGTPAudioCategory::Radio);
		}
		else if (Parts.Num() >= 2 && Parts[0] == TEXT("UI"))
		{
			Bank = FindNamed(Parts[1], Interface, StaticEnum<EGTPUISound>(), UE_ARRAY_COUNT(Interface));
			Class = GetClass(EGTPAudioCategory::Interface);
		}
		if (!Bank)
		{
			++Unsorted;
			continue;
		}
		USoundBase* Sound = Cast<USoundBase>(Asset.GetAsset());
		if (!Sound)
		{
			continue;
		}
		// In memory only: the assets keep their saved class, the game plays them through ours.
		Sound->SoundClassObject = Class;
		Bank->Sounds.Add(Sound);
		AllSounds.Add(Sound);
	}

	if (!Settings->AmbienceSound.IsNull())
	{
		if (USoundBase* Ambience = Settings->AmbienceSound.LoadSynchronous())
		{
			Ambience->SoundClassObject = GetClass(EGTPAudioCategory::Ambience);
			AllSounds.Add(Ambience);
		}
	}

	FString Summary;
	for (int32 Index = 0; Index < NumSurfaces; ++Index)
	{
		Summary += FString::Printf(TEXT(" %s=%d/%d"), *EnumName(static_cast<EGTPSurface>(Index)), Steps[Index].Sounds.Num(), Lands[Index].Sounds.Num());
	}
	int32 ImpactCount = Glass.Sounds.Num();
	for (const FBank& Bank : Impacts)
	{
		ImpactCount += Bank.Sounds.Num();
	}
	auto CountOf = [](const FBank* Banks, int32 Count)
	{
		int32 Total = 0;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Total += Banks[Index].Sounds.Num();
		}
		return Total;
	};
	GTP_TRACE(LogGTPAudio, TEXT("[Audio] game audio on (ADR-0009, -Audio=0 turns it off); banks under %s: steps/landings%s; impacts %d (glass %d); voices %d, radio %d, ui %d; %d unsorted"),
		*Root, *Summary, ImpactCount, Glass.Sounds.Num(), CountOf(Voices, UE_ARRAY_COUNT(Voices)), CountOf(Radio, UE_ARRAY_COUNT(Radio)),
		CountOf(Interface, UE_ARRAY_COUNT(Interface)), Unsorted);
}

void UGTPAudioMix::LoadSurfaceMap()
{
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	{
		const FString ZonePath = FPaths::ProjectContentDir() / Settings->ZoneMapFile;
		TArray<uint8> ZoneBytes;
		if (FFileHelper::LoadFileToArray(ZoneBytes, *ZonePath, FILEREAD_Silent) && ZoneMap.Load(ZoneBytes.GetData(), ZoneBytes.Num()))
		{
			GTP_TRACE(LogGTPAudio, TEXT("[Audio] zone map %s: %d x %d cells"), *Settings->ZoneMapFile, ZoneMap.GetWidth(), ZoneMap.GetHeight());
		}
		else
		{
			GTP_TRACE(LogGTPAudio, TEXT("[Audio] no zone map at %s: street ambience everywhere"), *ZonePath);
		}
	}
	const FString Path = FPaths::ProjectContentDir() / Settings->SurfaceMapFile;
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent) || !SurfaceMap.Load(Bytes.GetData(), Bytes.Num()))
	{
		GTP_TRACE(LogGTPAudio, TEXT("[Audio] no surface map at %s: the city ground counts as %s"), *Path, *EnumName(Settings->DefaultSurface));
		return;
	}
	GTP_TRACE(LogGTPAudio, TEXT("[Audio] surface map %s: %d x %d cells of %.0f cm"), *Settings->SurfaceMapFile,
		SurfaceMap.GetWidth(), SurfaceMap.GetHeight(), SurfaceMap.GetCellCm());
}

EGTPAmbienceZone UGTPAudioMix::SampleZone(const FVector& Location) const
{
	const uint8 Id = ZoneMap.Sample(Location.X, Location.Y);
	return Id < static_cast<uint8>(EGTPAmbienceZone::Count) ? static_cast<EGTPAmbienceZone>(Id) : EGTPAmbienceZone::Street;
}

USoundClass* UGTPAudioMix::GetClass(EGTPAudioCategory Category) const
{
	const int32 Index = static_cast<int32>(Category);
	return Classes.IsValidIndex(Index) ? Classes[Index].Get() : nullptr;
}

USoundConcurrency* UGTPAudioMix::GetConcurrency(EGTPAudioCategory Category) const
{
	const int32 Index = static_cast<int32>(Category);
	return Concurrency.IsValidIndex(Index) ? Concurrency[Index].Get() : nullptr;
}

USoundAttenuation* UGTPAudioMix::GetAttenuation(EGTPAudioCategory Category) const
{
	const int32 Index = static_cast<int32>(Category);
	return Attenuation.IsValidIndex(Index) ? Attenuation[Index].Get() : nullptr;
}

float UGTPAudioMix::GetCategoryVolume(EGTPAudioCategory Category) const
{
	const float* Volume = Volumes.Find(Category);
	return Volume ? FMath::Clamp(*Volume, 0.f, 1.f) : 1.f;
}

void UGTPAudioMix::SetCategoryVolume(EGTPAudioCategory Category, float Volume)
{
	Volumes.Add(Category, FMath::Clamp(Volume, 0.f, 1.f));
	const int32 Index = static_cast<int32>(Category);
	if (UserMix && UserMix->SoundClassEffects.IsValidIndex(Index))
	{
		UserMix->SoundClassEffects[Index].VolumeAdjuster = GetCategoryVolume(Category);
	}
	if (const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		UGameplayStatics::SetSoundMixClassOverride(World, UserMix, GetClass(Category), GetCategoryVolume(Category), 1.f, 0.2f, false);
	}
}

USoundBase* UGTPAudioMix::PickFrom(TArray<TObjectPtr<USoundBase>>& Bank, int32& LastIndex)
{
	if (Bank.Num() == 0)
	{
		return nullptr;
	}
	int32 Index = FMath::RandHelper(Bank.Num());
	if (Bank.Num() > 1 && Index == LastIndex)
	{
		Index = (Index + 1 + FMath::RandHelper(Bank.Num() - 1)) % Bank.Num();
	}
	LastIndex = Index;
	return Bank[Index];
}

USoundBase* UGTPAudioMix::PickFootstep(EGTPSurface Surface, bool bLanding)
{
	const int32 Index = FMath::Clamp(static_cast<int32>(Surface), 0, NumSurfaces - 1);
	FBank* Bank = bLanding ? Lands : Steps;
	if (USoundBase* Sound = PickFrom(Bank[Index].Sounds, Bank[Index].Last))
	{
		return Sound;
	}
	// Missing bank: stone for any hard ground, then plain steps for a landing.
	const int32 Stone = static_cast<int32>(EGTPSurface::Stone);
	if (USoundBase* Sound = PickFrom(Bank[Stone].Sounds, Bank[Stone].Last))
	{
		return Sound;
	}
	return bLanding ? PickFootstep(Surface, false) : nullptr;
}

USoundBase* UGTPAudioMix::PickImpact(EGTPImpactKind Kind, int32 Tier)
{
	const int32 KindIndex = FMath::Clamp(static_cast<int32>(Kind), 0, NumImpactKinds - 1);
	const int32 TierIndex = FMath::Clamp(Tier, 1, 3) - 1;
	// The asked tier first, then the nearest ones, then the car crash bank.
	const int32 Order[3] = {TierIndex, FMath::Max(0, TierIndex - 1), FMath::Min(2, TierIndex + 1)};
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		const int32 K = Pass == 0 ? KindIndex : static_cast<int32>(EGTPImpactKind::Car);
		for (int32 T : Order)
		{
			FBank& Bank = Impacts[K * 3 + T];
			if (USoundBase* Sound = PickFrom(Bank.Sounds, Bank.Last))
			{
				return Sound;
			}
		}
	}
	return nullptr;
}

USoundBase* UGTPAudioMix::PickGlass()
{
	return PickFrom(Glass.Sounds, Glass.Last);
}

USoundBase* UGTPAudioMix::PickVoice(EGTPVoiceKind Kind)
{
	FBank& Bank = Voices[FMath::Clamp(static_cast<int32>(Kind), 0, UE_ARRAY_COUNT(Voices) - 1)];
	return PickFrom(Bank.Sounds, Bank.Last);
}

USoundBase* UGTPAudioMix::PickRadio(EGTPRadioCall Call)
{
	FBank& Bank = Radio[FMath::Clamp(static_cast<int32>(Call), 0, UE_ARRAY_COUNT(Radio) - 1)];
	return PickFrom(Bank.Sounds, Bank.Last);
}

USoundBase* UGTPAudioMix::PickUI(EGTPUISound Sound)
{
	FBank& Bank = Interface[FMath::Clamp(static_cast<int32>(Sound), 0, UE_ARRAY_COUNT(Interface) - 1)];
	return PickFrom(Bank.Sounds, Bank.Last);
}

void UGTPAudioMix::PlayUI(const UObject* WorldContext, EGTPUISound Sound)
{
	if (!IsGameAudioEnabled())
	{
		return;
	}
	if (UGTPAudioMix* Mix = Get(WorldContext))
	{
		if (Mix->Play2D(WorldContext, Mix->PickUI(Sound), EGTPAudioCategory::Interface, GetDefault<UGTPAudioSettings>()->InterfaceVolume, 1.f))
		{
			++Mix->UIPlayed;
		}
	}
}

UAudioComponent* UGTPAudioMix::Play2D(const UObject* WorldContext, USoundBase* Sound, EGTPAudioCategory Category, float Volume, float Pitch)
{
	if (!Sound || !WorldContext)
	{
		return nullptr;
	}
	RadioPlayed += Category == EGTPAudioCategory::Radio ? 1 : 0;
	return UGameplayStatics::SpawnSound2D(WorldContext, Sound, Volume, Pitch, 0.f, GetConcurrency(Category), /*bPersistAcrossLevelTransition*/ false, /*bAutoDestroy*/ true);
}

EGTPSurface UGTPAudioMix::ResolveSurface(const FHitResult& Hit) const
{
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();

	// 1. A physical material set on purpose (not the engine default).
	if (const UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get())
	{
		if (!GEngine || PhysMat != GEngine->DefaultPhysMaterial)
		{
			const EPhysicalSurface SurfaceType = UPhysicalMaterial::DetermineSurfaceType(PhysMat);
			for (const FPhysicalSurfaceName& Named : UPhysicsSettings::Get()->PhysicalSurfaces)
			{
				if (Named.Type == SurfaceType)
				{
					if (const EGTPSurface* Mapped = Settings->PhysicalSurfaceNames.Find(Named.Name))
					{
						return *Mapped;
					}
				}
			}
			const uint8 ByName = GTPSurfaceFromName(TCHAR_TO_UTF8(*PhysMat->GetName()));
			if (ByName != GTPSurfaceId::Unknown)
			{
				return static_cast<EGTPSurface>(ByName);
			}
		}
	}

	// 2. The material of the hit component: the city ground reads the grid, others their name.
	if (const UPrimitiveComponent* Component = Hit.GetComponent())
	{
		const int32 NumMaterials = Component->GetNumMaterials();
		for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
		{
			const UMaterialInterface* Material = Component->GetMaterial(Slot);
			if (!Material)
			{
				continue;
			}
			const FString Name = Material->GetName();
			const FString BaseName = Material->GetMaterial() ? Material->GetMaterial()->GetName() : FString();
			if (!GroundKeyword.IsEmpty() && (Name.Contains(GroundKeyword) || BaseName.Contains(GroundKeyword)))
			{
				const uint8 Id = SurfaceMap.Sample(Hit.ImpactPoint.X, Hit.ImpactPoint.Y);
				return Id != GTPSurfaceId::Unknown ? static_cast<EGTPSurface>(Id) : Settings->DefaultSurface;
			}
			uint8 Id = GTPSurfaceFromName(TCHAR_TO_UTF8(*Name));
			if (Id == GTPSurfaceId::Unknown && !BaseName.IsEmpty())
			{
				Id = GTPSurfaceFromName(TCHAR_TO_UTF8(*BaseName));
			}
			if (Id != GTPSurfaceId::Unknown)
			{
				return static_cast<EGTPSurface>(Id);
			}
		}
	}

	// 3. A prop without a telling name (steps, kerb pieces, plinths): stone by default.
	return Settings->DefaultSurface;
}

UAudioComponent* UGTPAudioMix::PlayAt(const UObject* WorldContext, USoundBase* Sound, EGTPAudioCategory Category, const FVector& Location, float Volume, float Pitch)
{
	if (!Sound || !WorldContext)
	{
		return nullptr;
	}
	if (Category == EGTPAudioCategory::Footsteps)
	{
		++FootstepsPlayed;
	}
	else if (Category == EGTPAudioCategory::Impacts)
	{
		++ImpactsPlayed;
	}
	else if (Category == EGTPAudioCategory::Voice)
	{
		++VoicesPlayed;
	}
	return UGameplayStatics::SpawnSoundAtLocation(WorldContext, Sound, Location, FRotator::ZeroRotator, Volume, Pitch, 0.f,
		GetAttenuation(Category), GetConcurrency(Category), /*bAutoDestroy*/ true);
}

int32 UGTPAudioMix::GetBankSize() const
{
	return AllSounds.Num();
}
