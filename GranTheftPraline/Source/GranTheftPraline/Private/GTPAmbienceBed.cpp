#include "GTPAmbienceBed.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"
#include "WheeledVehiclePawn.h"

namespace
{
	const TCHAR* LayerNames[] = {TEXT("Pedestrian"), TEXT("Quay"), TEXT("Bridge"), TEXT("Night"), TEXT("Rain"), TEXT("RainCar"), TEXT("Crowd"), TEXT("Square")};
	/** World state is sensed a few times per second; volumes glide every frame. */
	constexpr float SenseInterval = 0.25f;
}

void UGTPAmbienceBed::Start(UWorld* World)
{
	WorldRef = World;
	UGTPAudioMix* Mix = UGTPAudioMix::Get(World);
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	if (!World || !Mix)
	{
		return;
	}
	Layers.SetNum(NumLayers);
	for (int32 Index = 0; Index < NumLayers; ++Index)
	{
		const FString Name = FString(TEXT("SW_Amb_")) + LayerNames[Index];
		const FString Path = Settings->AmbienceRoot + TEXT("/") + Name + TEXT(".") + Name;
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Sound)
		{
			continue;
		}
		if (USoundWave* Wave = Cast<USoundWave>(Sound))
		{
			Wave->bLooping = true;
		}
		const bool bWeather = Index == Rain || Index == RainCar;
		UAudioComponent* Layer = UGameplayStatics::CreateSound2D(World, Sound, 1.f, 1.f, 0.f, nullptr, false, false);
		if (!Layer)
		{
			continue;
		}
		Layer->bIsUISound = true;
		Layer->SoundClassOverride = Mix->GetClass(bWeather ? EGTPAudioCategory::Weather : EGTPAudioCategory::Zones);
		Layer->SetVolumeMultiplier(0.001f);
		// Not played yet: Tick starts a layer when it is needed, at a random point of
		// the loop (the wave's own length, GetDuration() says "forever" once bLooping
		// is set) so two layers never pulse together.
		Layers[Index] = Layer;
	}

	// The day street loop (Ambience class) is lowered by this mix, never stopped.
	StreetMix = NewObject<USoundMix>(this, TEXT("SM_GTP_StreetBed"));
	StreetMix->Duration = -1.f;
	FSoundClassAdjuster Street;
	Street.SoundClassObject = Mix->GetClass(EGTPAudioCategory::Ambience);
	Street.VolumeAdjuster = 1.f;
	StreetMix->SoundClassEffects.Add(Street);
	UGameplayStatics::PushSoundMixModifier(World, StreetMix);

	Collection = Cast<UMaterialParameterCollection>(Settings->EnvironmentCollection.TryLoad());
}

void UGTPAmbienceBed::Stop()
{
	for (UAudioComponent* Layer : Layers)
	{
		if (Layer)
		{
			Layer->Stop();
		}
	}
	Layers.Reset();
	if (UWorld* World = WorldRef.Get())
	{
		if (StreetMix)
		{
			UGameplayStatics::PopSoundMixModifier(World, StreetMix);
		}
	}
}

float UGTPAmbienceBed::ReadParameter(FName Name) const
{
	UWorld* World = WorldRef.Get();
	if (!World || !Collection || Name.IsNone())
	{
		return 0.f;
	}
	float Value = 0.f;
	if (UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(Collection))
	{
		Instance->GetScalarParameterValue(Name, Value);
	}
	return FMath::Clamp(Value, 0.f, 1.f);
}

void UGTPAmbienceBed::Tick(float DeltaTime, const TArray<TWeakObjectPtr<AActor>>& Pedestrians)
{
	UWorld* World = WorldRef.Get();
	if (!World || Layers.Num() != NumLayers)
	{
		return;
	}
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();

	SenseTimer -= DeltaTime;
	if (SenseTimer <= 0.f)
	{
		SenseTimer = SenseInterval;
		const APlayerController* Player = World->GetFirstPlayerController();
		FVector Listener = FVector::ZeroVector, Front, Right;
		if (Player)
		{
			Player->GetAudioListenerPosition(Listener, Front, Right);
		}
		const bool bInCar = Player && Cast<AWheeledVehiclePawn>(Player->GetPawn()) != nullptr;
		const UGTPAudioMix* Mix = UGTPAudioMix::Get(World);
		Zone = Mix ? Mix->SampleZone(Listener) : EGTPAmbienceZone::Street;
		RainAmount = ReadParameter(Settings->RainParameter);
		NightAmount = ReadParameter(Settings->NightParameter);

		CrowdCount = 0;
		const float Radius2 = FMath::Square(Settings->CrowdRadius);
		for (const TWeakObjectPtr<AActor>& Weak : Pedestrians)
		{
			const AActor* Walker = Weak.Get();
			if (Walker && !Walker->IsHidden() && FVector::DistSquared(Walker->GetActorLocation(), Listener) < Radius2)
			{
				++CrowdCount;
			}
		}

		const float ZoneVolume = Settings->ZoneLayerVolume;
		Targets[Pedestrian] = Zone == EGTPAmbienceZone::Pedestrian ? ZoneVolume : 0.f;
		Targets[Quay] = Zone == EGTPAmbienceZone::Quay ? ZoneVolume : 0.f;
		Targets[Bridge] = Zone == EGTPAmbienceZone::Bridge ? ZoneVolume : 0.f;
		Targets[Square] = Zone == EGTPAmbienceZone::Square ? ZoneVolume : 0.f;
		// At night the zone layers keep a third of their level, the night layer takes over.
		for (int32 Index : {static_cast<int32>(Pedestrian), static_cast<int32>(Quay), static_cast<int32>(Bridge), static_cast<int32>(Square)})
		{
			Targets[Index] *= 1.f - 0.66f * NightAmount;
		}
		Targets[Night] = ZoneVolume * NightAmount;
		Targets[Rain] = Settings->RainVolume * RainAmount * (bInCar ? Settings->RainInCarLevel : 1.f);
		Targets[RainCar] = bInCar ? Settings->RainVolume * RainAmount : 0.f;
		const float CrowdShare = Settings->CrowdFullCount > 0 ? FMath::Clamp(static_cast<float>(CrowdCount) / Settings->CrowdFullCount, 0.f, 1.f) : 0.f;
		Targets[Crowd] = ZoneVolume * CrowdShare * (1.f - 0.5f * NightAmount);

		float Street = 1.f;
		switch (Zone)
		{
		case EGTPAmbienceZone::Pedestrian: Street = Settings->StreetLevelPedestrian; break;
		case EGTPAmbienceZone::Quay: Street = Settings->StreetLevelQuay; break;
		case EGTPAmbienceZone::Bridge: Street = Settings->StreetLevelBridge; break;
		// The traffic is all around a square but far: the street loop is lowered like on a quay.
		case EGTPAmbienceZone::Square: Street = Settings->StreetLevelQuay; break;
		default: break;
		}
		StreetTarget = Street * FMath::Lerp(1.f, Settings->StreetLevelNight, NightAmount);
		if (FMath::Abs(StreetTarget - StreetApplied) > 0.02f && StreetMix && Mix)
		{
			StreetApplied = StreetTarget;
			UGameplayStatics::SetSoundMixClassOverride(World, StreetMix, Mix->GetClass(EGTPAudioCategory::Ambience),
				StreetApplied, 1.f, Settings->BedFadeSeconds, false);
		}
	}

	const float Rate = 1.f / FMath::Max(0.1f, Settings->BedFadeSeconds);
	for (int32 Index = 0; Index < NumLayers; ++Index)
	{
		if (UAudioComponent* Layer = Layers[Index])
		{
			const float Before = Current[Index];
			Current[Index] = FMath::FInterpConstantTo(Current[Index], Targets[Index], DeltaTime, Rate);
			// A silent layer is stopped (no decoding, no mixing); it starts again at a random point when needed.
			const bool bSilent = Current[Index] <= 0.002f && Targets[Index] <= 0.002f;
			if (bSilent)
			{
				if (Layer->IsPlaying())
				{
					Layer->Stop();
				}
				continue;
			}
			if (!Layer->IsPlaying())
			{
				const USoundWave* LoopWave = Cast<USoundWave>(Layer->Sound);
				const float LoopLength = LoopWave ? LoopWave->Duration : 0.f;
				Layer->SetVolumeMultiplier(FMath::Max(0.001f, Current[Index]));
				Layer->Play(LoopLength > 1.f && LoopLength < 600.f ? FMath::FRandRange(0.f, LoopLength * 0.9f) : 0.f);
			}
			else if (FMath::Abs(Current[Index] - Before) > 0.f)
			{
				Layer->SetVolumeMultiplier(FMath::Max(0.001f, Current[Index]));
			}
		}
	}
}

FString UGTPAmbienceBed::Describe() const
{
	static const TCHAR* ZoneNames[] = {TEXT("street"), TEXT("pedestrian"), TEXT("quay"), TEXT("bridge"), TEXT("square")};
	int32 Loaded = 0;
	int32 Playing = 0;
	for (const UAudioComponent* Layer : Layers)
	{
		Loaded += Layer ? 1 : 0;
		Playing += Layer && Layer->IsPlaying() ? 1 : 0;
	}
	return FString::Printf(TEXT("zone=%s street=%.2f rain=%.2f night=%.2f crowd=%d layers=%d/%d playing=%d"),
		ZoneNames[FMath::Clamp(static_cast<int32>(Zone), 0, 4)], StreetApplied, RainAmount, NightAmount, CrowdCount, Loaded, static_cast<int32>(NumLayers), Playing);
}
