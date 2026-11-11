#include "GTPPlaceAudio.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "GTPAudioWorld.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundWave.h"

namespace
{
	constexpr float SenseInterval = 0.25f;
	/** Two emitters of the same loop closer than this are one (four fryers in a row sound as two). */
	constexpr float MergeDistance = 150.f;
	/** Rooms are slightly smaller than their walls, so standing in the doorway is still outside. */
	constexpr float RoomShrink = 30.f;
	/** Pieces that cannot be part of a room (a whole street mesh in the interior level). */
	constexpr float MaxPieceSize = 6000.f;

	template <typename EnumType>
	int32 EnumIndex(const FString& Name)
	{
		const int64 Value = StaticEnum<EnumType>()->GetValueByNameString(Name);
		return Value >= 0 && Value < static_cast<int64>(EnumType::Count) ? static_cast<int32>(Value) : INDEX_NONE;
	}

	/** Explicitly no loop (GTP_AudioLoop=None on instanced groups): names are not read either. */
	constexpr int32 NoLoop = -2;

	/**
	 * Loop of a component from its tag, else from its names. Actor tags count for the root
	 * component only (bActorTags), so a tagged TargetPoint is one emitter, not one per component.
	 */
	int32 LoopOf(const USceneComponent* Component, bool bActorTags)
	{
		const AActor* Owner = Component->GetOwner();
		for (const TArray<FName>* Tags : {&Component->ComponentTags, Owner && bActorTags ? &Owner->Tags : nullptr})
		{
			if (!Tags)
			{
				continue;
			}
			for (const FName& Tag : *Tags)
			{
				FString Left, Right;
				if (Tag.ToString().Split(TEXT("="), &Left, &Right) && Left == TEXT("GTP_AudioLoop"))
				{
					const int32 Index = EnumIndex<EGTPRoomLoop>(Right);
					return Index != INDEX_NONE ? Index : NoLoop;
				}
			}
		}
		FString Names = Component->GetName() + TEXT(" ") + (Owner ? Owner->GetName() : FString());
		if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component))
		{
			Names += Mesh->GetStaticMesh() ? TEXT(" ") + Mesh->GetStaticMesh()->GetName() : FString();
		}
		Names.ToLowerInline();
		struct FWord
		{
			const TCHAR* Word;
			EGTPRoomLoop Loop;
		};
		static const FWord Words[] = {
			{TEXT("fryer"), EGTPRoomLoop::Fryer}, {TEXT("friteuse"), EGTPRoomLoop::Fryer},
			{TEXT("grill"), EGTPRoomLoop::Grill}, {TEXT("griddle"), EGTPRoomLoop::Grill}, {TEXT("plancha"), EGTPRoomLoop::Grill},
			{TEXT("hood"), EGTPRoomLoop::Hood}, {TEXT("hotte"), EGTPRoomLoop::Hood},
			{TEXT("fridge"), EGTPRoomLoop::Cold}, {TEXT("freezer"), EGTPRoomLoop::Cold}, {TEXT("walkin"), EGTPRoomLoop::Cold},
			{TEXT("coldrack"), EGTPRoomLoop::Cold}, {TEXT("cold_rack"), EGTPRoomLoop::Cold},
			{TEXT("shake"), EGTPRoomLoop::Shake}, {TEXT("icecream"), EGTPRoomLoop::Shake},
			{TEXT("drinktower"), EGTPRoomLoop::Drinks}, {TEXT("drink_tower"), EGTPRoomLoop::Drinks}, {TEXT("soda"), EGTPRoomLoop::Drinks},
		};
		for (const FWord& Entry : Words)
		{
			if (Names.Contains(Entry.Word))
			{
				return static_cast<int32>(Entry.Loop);
			}
		}
		return INDEX_NONE;
	}

	bool HasTag(const USceneComponent* Component, const TCHAR* Tag, bool bActorTags)
	{
		return Component->ComponentHasTag(Tag) || (bActorTags && Component->GetOwner() && Component->GetOwner()->ActorHasTag(Tag));
	}

	/** A till: tag GTP_AudioTill (cooked builds: actor names are not the editor labels), else its names. */
	bool IsTill(const USceneComponent* Component, bool bActorTags)
	{
		if (HasTag(Component, TEXT("GTP_AudioTill"), bActorTags))
		{
			return true;
		}
		FString Names = Component->GetName() + TEXT(" ") + (Component->GetOwner() ? Component->GetOwner()->GetName() : FString());
		Names.ToLowerInline();
		return Names.Contains(TEXT("till")) || Names.Contains(TEXT("register")) || Names.Contains(TEXT("kiosk")) || Names.Contains(TEXT("_pos"));
	}

	/** Adds a spot unless one is already there (the same till seen through two components). */
	template <typename SpotType>
	void AddSpot(TArray<SpotType>& Spots, const FVector& Location, ULevel* Level)
	{
		if (!Spots.ContainsByPredicate([&Location](const SpotType& Spot) { return FVector::DistSquared(Spot.Location, Location) < 50.f * 50.f; }))
		{
			Spots.Add({Location, Level});
		}
	}
}

UGTPPlaceAudio* UGTPPlaceAudio::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGTPAudioWorld* AudioWorld = World ? World->GetSubsystem<UGTPAudioWorld>() : nullptr;
	return AudioWorld ? AudioWorld->GetPlaceAudio() : nullptr;
}

void UGTPPlaceAudio::PlayShop(const UObject* WorldContext, FVector Location, EGTPShopSound Sound)
{
	UGTPPlaceAudio* Places = Get(WorldContext);
	UGTPAudioMix* Mix = UGTPAudioMix::Get(WorldContext);
	if (!Places || !Mix)
	{
		return;
	}
	if (USoundBase* Wave = Places->PickShop(Sound))
	{
		++Places->ShopPlayed;
		Mix->PlayAt(WorldContext, Wave, EGTPAudioCategory::Impacts, Location, Places->ShopVolume, FMath::FRandRange(0.97f, 1.03f));
	}
}

void UGTPPlaceAudio::PlayMission(const UObject* WorldContext, EGTPMissionSound Sound)
{
	UGTPPlaceAudio* Places = Get(WorldContext);
	UGTPAudioMix* Mix = UGTPAudioMix::Get(WorldContext);
	if (!Places || !Mix)
	{
		return;
	}
	if (USoundBase* Wave = Places->PickMission(Sound))
	{
		++Places->MissionPlayed;
		Mix->Play2D(WorldContext, Wave, EGTPAudioCategory::Interface, Places->MissionVolume, 1.f);
	}
}

void UGTPPlaceAudio::Start(UWorld* World)
{
	WorldPtr = World;
	UGTPAudioMix* Mix = UGTPAudioMix::Get(World);
	if (!World || !Mix)
	{
		return;
	}
	int32 LifeSwitch = 1;
	FParse::Value(FCommandLine::Get(), TEXT("ShopLife="), LifeSwitch);
	bShopLife = LifeSwitch != 0 && ShopLifeGap.Y > 0.f;
	LifeTimer = FMath::FRandRange(ShopLifeGap.X, FMath::Max(ShopLifeGap.X, ShopLifeGap.Y));

	LoadBanks();

	LoopAttenuation = NewObject<USoundAttenuation>(this, TEXT("SA_GTP_RoomLoop"));
	FSoundAttenuationSettings& Att = LoopAttenuation->Attenuation;
	Att.bAttenuate = true;
	Att.bSpatialize = true;
	Att.AttenuationShape = EAttenuationShape::Sphere;
	Att.AttenuationShapeExtents = FVector(150.f, 0.f, 0.f);
	Att.FalloffDistance = FMath::Max(300.f, EmitterRange - 150.f);
	Att.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;

	// Indoors: the street is still there, lower and muffled behind the glass.
	IndoorMix = NewObject<USoundMix>(this, TEXT("SM_GTP_Indoor"));
	IndoorMix->Duration = -1.f;
	IndoorMix->FadeInTime = 0.6f;
	IndoorMix->FadeOutTime = 0.6f;
	auto Adjust = [this, Mix](EGTPAudioCategory Category, float Volume)
	{
		FSoundClassAdjuster Adjuster;
		Adjuster.SoundClassObject = Mix->GetClass(Category);
		Adjuster.VolumeAdjuster = Volume;
		Adjuster.LowPassFilterFrequency = IndoorLowPassHz;
		Adjuster.bApplyToChildren = true;
		IndoorMix->SoundClassEffects.Add(Adjuster);
	};
	Adjust(EGTPAudioCategory::Ambience, IndoorStreetVolume);
	Adjust(EGTPAudioCategory::Vehicles, IndoorTrafficVolume);
	Adjust(EGTPAudioCategory::Sirens, FMath::Min(1.f, IndoorTrafficVolume * 2.f));
	Adjust(EGTPAudioCategory::Weather, IndoorStreetVolume);

	if (USoundBase* Tone = PickLoop(EGTPRoomLoop::RoomTone))
	{
		RoomTone = UGameplayStatics::CreateSound2D(World, Tone, 1.f, 1.f, 0.f, nullptr, false, false);
		if (RoomTone)
		{
			RoomTone->SoundClassOverride = Mix->GetClass(EGTPAudioCategory::Effects);
		}
	}

	AddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UGTPPlaceAudio::OnLevelAdded);
	RemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UGTPPlaceAudio::OnLevelRemoved);
	for (ULevel* Level : World->GetLevels())
	{
		if (Level && Level != World->PersistentLevel && Level->bIsVisible)
		{
			ScanLevel(Level);
		}
	}
}

void UGTPPlaceAudio::Stop()
{
	FWorldDelegates::LevelAddedToWorld.Remove(AddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(RemovedHandle);
	for (UAudioComponent* Voice : Voices)
	{
		if (Voice)
		{
			Voice->Stop();
		}
	}
	if (RoomTone)
	{
		RoomTone->Stop();
	}
	if (UWorld* World = WorldPtr.Get())
	{
		if (bIndoorMixOn && IndoorMix)
		{
			UGameplayStatics::PopSoundMixModifier(World, IndoorMix);
		}
	}
	bIndoorMixOn = false;
	Voices.Reset();
	VoiceEmitter.Reset();
	Emitters.Reset();
	Rooms.Reset();
	Tills.Reset();
	Fryers.Reset();
	WorldPtr.Reset();
}

void UGTPPlaceAudio::LoadBanks()
{
	const UGTPAudioMix* Mix = UGTPAudioMix::Get(WorldPtr.Get());
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	const FString Root = GetDefault<UGTPAudioSettings>()->SoundRoot;
	struct FGroup
	{
		const TCHAR* Folder;
		EGTPAudioCategory Category;
	};
	for (const FGroup& Group : {FGroup{TEXT("Interior"), EGTPAudioCategory::Effects}, FGroup{TEXT("Shop"), EGTPAudioCategory::Impacts},
			 FGroup{TEXT("Mission"), EGTPAudioCategory::Interface}})
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(*(Root + TEXT("/") + Group.Folder)), Assets, /*bRecursive*/ true);
		for (const FAssetData& Asset : Assets)
		{
			const FString Name = FPaths::GetCleanFilename(Asset.PackagePath.ToString());
			TArray<USoundBase*>* Bank = nullptr;
			if (FCString::Strcmp(Group.Folder, TEXT("Interior")) == 0)
			{
				const int32 Index = EnumIndex<EGTPRoomLoop>(Name);
				Bank = Index != INDEX_NONE ? &LoopBank[Index] : nullptr;
			}
			else if (FCString::Strcmp(Group.Folder, TEXT("Shop")) == 0)
			{
				const int32 Index = EnumIndex<EGTPShopSound>(Name);
				Bank = Index != INDEX_NONE ? &ShopBank[Index] : nullptr;
			}
			else
			{
				const int32 Index = EnumIndex<EGTPMissionSound>(Name);
				Bank = Index != INDEX_NONE ? &MissionBank[Index] : nullptr;
			}
			USoundBase* Sound = Bank ? Cast<USoundBase>(Asset.GetAsset()) : nullptr;
			if (!Sound)
			{
				continue;
			}
			// In memory only, like UGTPAudioMix: the game plays them through its classes.
			Sound->SoundClassObject = Mix ? Mix->GetClass(Group.Category) : nullptr;
			if (USoundWave* Wave = Cast<USoundWave>(Sound); Wave && Group.Category == EGTPAudioCategory::Effects)
			{
				Wave->bLooping = true;
			}
			Bank->Add(Sound);
			AllSounds.Add(Sound);
		}
	}
}

USoundBase* UGTPPlaceAudio::PickLoop(EGTPRoomLoop Loop) const
{
	const TArray<USoundBase*>& Bank = LoopBank[static_cast<int32>(Loop)];
	return Bank.Num() > 0 ? Bank[FMath::RandRange(0, Bank.Num() - 1)] : nullptr;
}

USoundBase* UGTPPlaceAudio::PickShop(EGTPShopSound Sound)
{
	const int32 Index = static_cast<int32>(Sound);
	const TArray<USoundBase*>& Bank = ShopBank[Index];
	if (Bank.Num() == 0)
	{
		return nullptr;
	}
	// Never twice the same in a row when there is a choice.
	int32 Pick = FMath::RandRange(0, Bank.Num() - 1);
	if (Bank.Num() > 1 && Pick == LastShop[Index])
	{
		Pick = (Pick + 1) % Bank.Num();
	}
	LastShop[Index] = Pick;
	return Bank[Pick];
}

USoundBase* UGTPPlaceAudio::PickMission(EGTPMissionSound Sound)
{
	const TArray<USoundBase*>& Bank = MissionBank[static_cast<int32>(Sound)];
	return Bank.Num() > 0 ? Bank[FMath::RandRange(0, Bank.Num() - 1)] : nullptr;
}

void UGTPPlaceAudio::OnLevelAdded(ULevel* Level, UWorld* World)
{
	if (World && World == WorldPtr.Get() && Level && Level != World->PersistentLevel)
	{
		ScanLevel(Level);
	}
}

void UGTPPlaceAudio::OnLevelRemoved(ULevel* Level, UWorld* World)
{
	if (World && World == WorldPtr.Get())
	{
		ForgetLevel(Level);
	}
}

void UGTPPlaceAudio::ScanLevel(ULevel* Level)
{
	const FString Package = Level->GetPackage()->GetName();
	if (!Package.Contains(TEXT("Interior")))
	{
		return;
	}
	ForgetLevel(Level);
	int32 TaggedRooms = 0;
	int32 NewEmitters = 0;
	FBox Bounds(ForceInit);
	for (AActor* Actor : Level->Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		TInlineComponentArray<USceneComponent*> Components(Actor);
		for (const USceneComponent* Component : Components)
		{
			const bool bRoot = Component == Actor->GetRootComponent();
			const FBoxSphereBounds ComponentBounds = Component->Bounds;
			const FVector Location = ComponentBounds.Origin;
			if (HasTag(Component, TEXT("GTP_AudioRoom"), bRoot))
			{
				Rooms.Add({ComponentBounds.GetBox().ExpandBy(-RoomShrink), Level});
				++TaggedRooms;
				continue;
			}
			if (Component->IsA<UPrimitiveComponent>() && ComponentBounds.BoxExtent.GetMax() * 2.f < MaxPieceSize)
			{
				Bounds += ComponentBounds.GetBox();
			}
			const int32 Loop = LoopOf(Component, bRoot);
			if (Loop >= 0 && Loop != static_cast<int32>(EGTPRoomLoop::RoomTone))
			{
				const bool bNear = Emitters.ContainsByPredicate([&](const FEmitter& Other)
				{
					return static_cast<int32>(Other.Loop) == Loop && FVector::DistSquared(Other.Location, Location) < FMath::Square(MergeDistance);
				});
				if (!bNear)
				{
					Emitters.Add({static_cast<EGTPRoomLoop>(Loop), Location, Level});
					++NewEmitters;
				}
				if (Loop == static_cast<int32>(EGTPRoomLoop::Fryer))
				{
					AddSpot(Fryers, Location, Level);
				}
			}
			else if (IsTill(Component, bRoot))
			{
				AddSpot(Tills, Location, Level);
			}
		}
	}
	if (TaggedRooms == 0 && Bounds.IsValid)
	{
		Rooms.Add({Bounds.ExpandBy(-RoomShrink), Level});
	}
	UE_LOG(LogTemp, Log, TEXT("[Audio] places: %s scanned, %d loop emitters, %d tills, %d rooms (%s)"), *Package, NewEmitters,
		Tills.FilterByPredicate([Level](const FSpot& Spot) { return Spot.Level == Level; }).Num(),
		TaggedRooms > 0 ? TaggedRooms : 1, TaggedRooms > 0 ? TEXT("tagged") : TEXT("level bounds"));
}

void UGTPPlaceAudio::ForgetLevel(const ULevel* Level)
{
	auto OfLevel = [Level](const auto& Item) { return !Item.Level.IsValid() || Item.Level.Get() == Level; };
	for (int32 Voice = 0; Voice < VoiceEmitter.Num(); ++Voice)
	{
		const int32 Emitter = VoiceEmitter[Voice];
		if (Emitter != INDEX_NONE && Emitters.IsValidIndex(Emitter) && OfLevel(Emitters[Emitter]))
		{
			if (Voices[Voice])
			{
				Voices[Voice]->FadeOut(0.3f, 0.f);
			}
			VoiceEmitter[Voice] = INDEX_NONE;
		}
	}
	// Voices refer to emitters by index: remap the ones that stay.
	TArray<int32> NewIndex;
	NewIndex.Init(INDEX_NONE, Emitters.Num());
	int32 Kept = 0;
	for (int32 Index = 0; Index < Emitters.Num(); ++Index)
	{
		NewIndex[Index] = OfLevel(Emitters[Index]) ? INDEX_NONE : Kept++;
	}
	for (int32& Emitter : VoiceEmitter)
	{
		Emitter = Emitter != INDEX_NONE ? NewIndex[Emitter] : INDEX_NONE;
	}
	Emitters.RemoveAll(OfLevel);
	Rooms.RemoveAll(OfLevel);
	Tills.RemoveAll(OfLevel);
	Fryers.RemoveAll(OfLevel);
}

void UGTPPlaceAudio::Tick(float DeltaTime)
{
	UWorld* World = WorldPtr.Get();
	const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	if (!Player)
	{
		return;
	}
	FVector Listener, Front, Right;
	Player->GetAudioListenerPosition(Listener, Front, Right);
	UpdateIndoor(Listener, DeltaTime);
	SenseTimer -= DeltaTime;
	if (SenseTimer <= 0.f)
	{
		SenseTimer = SenseInterval;
		UpdateVoices(Listener);
	}
	UpdateShopLife(Listener, DeltaTime);
}

void UGTPPlaceAudio::UpdateIndoor(const FVector& Listener, float DeltaTime)
{
	UWorld* World = WorldPtr.Get();
	const bool bNowInside = Rooms.ContainsByPredicate([&](const FRoom& Room) { return Room.Box.IsInsideOrOn(Listener); });
	if (bNowInside && !bInside)
	{
		PlayShop(World, Listener, EGTPShopSound::DoorChime);
	}
	bInside = bNowInside;
	Indoor = FMath::FInterpConstantTo(Indoor, bInside ? 1.f : 0.f, DeltaTime, 1.6f);
	if (bInside != bIndoorMixOn && IndoorMix)
	{
		if (bInside)
		{
			UGameplayStatics::PushSoundMixModifier(World, IndoorMix);
		}
		else
		{
			UGameplayStatics::PopSoundMixModifier(World, IndoorMix);
		}
		bIndoorMixOn = bInside;
	}
	if (RoomTone)
	{
		const float Volume = Indoor * RoomToneVolume;
		if (Volume > 0.001f)
		{
			if (!RoomTone->IsPlaying())
			{
				RoomTone->Play(FMath::FRandRange(0.f, 8.f));
			}
			RoomTone->SetVolumeMultiplier(Volume);
		}
		else if (RoomTone->IsPlaying())
		{
			RoomTone->Stop();
		}
	}
}

void UGTPPlaceAudio::UpdateVoices(const FVector& Listener)
{
	UWorld* World = WorldPtr.Get();
	UGTPAudioMix* Mix = UGTPAudioMix::Get(World);
	if (!World || !Mix)
	{
		return;
	}
	// The nearest emitters in range, one per loop and place.
	TArray<TPair<float, int32>> Order;
	for (int32 Index = 0; Index < Emitters.Num(); ++Index)
	{
		const float Distance = FVector::Dist(Emitters[Index].Location, Listener);
		if (Distance < EmitterRange)
		{
			Order.Emplace(Distance, Index);
		}
	}
	Order.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	TArray<int32> Wanted;
	for (const TPair<float, int32>& Entry : Order)
	{
		if (Wanted.Num() >= MaxRoomVoices)
		{
			break;
		}
		Wanted.Add(Entry.Value);
	}
	// Voices that play an emitter no longer wanted fade out and become free.
	for (int32 Voice = 0; Voice < VoiceEmitter.Num(); ++Voice)
	{
		if (VoiceEmitter[Voice] != INDEX_NONE && !Wanted.Contains(VoiceEmitter[Voice]))
		{
			if (Voices[Voice])
			{
				Voices[Voice]->FadeOut(0.5f, 0.f);
			}
			VoiceEmitter[Voice] = INDEX_NONE;
		}
	}
	for (const int32 Emitter : Wanted)
	{
		if (VoiceEmitter.Contains(Emitter))
		{
			continue;
		}
		USoundBase* Sound = PickLoop(Emitters[Emitter].Loop);
		if (!Sound)
		{
			continue;
		}
		int32 Free = VoiceEmitter.IndexOfByKey(INDEX_NONE);
		if (Free == INDEX_NONE && Voices.Num() < MaxRoomVoices)
		{
			// Created once, at most MaxRoomVoices: interiors streaming in and out never create audio objects.
			UAudioComponent* Voice = UGameplayStatics::SpawnSoundAtLocation(World, Sound, Emitters[Emitter].Location, FRotator::ZeroRotator,
				1.f, 1.f, 0.f, LoopAttenuation, nullptr, /*bAutoDestroy*/ false);
			if (!Voice)
			{
				continue;
			}
			Voice->Stop();
			Voice->SoundClassOverride = Mix->GetClass(EGTPAudioCategory::Effects);
			Free = Voices.Add(Voice);
			VoiceEmitter.Add(INDEX_NONE);
		}
		if (Free == INDEX_NONE || !Voices[Free])
		{
			continue;
		}
		UAudioComponent* Voice = Voices[Free];
		Voice->Stop();
		Voice->SetSound(Sound);
		Voice->SetWorldLocation(Emitters[Emitter].Location);
		Voice->SetPitchMultiplier(FMath::FRandRange(0.96f, 1.04f));
		Voice->FadeIn(0.5f, LoopVolume, FMath::FRandRange(0.f, 10.f));
		VoiceEmitter[Free] = Emitter;
	}
}

void UGTPPlaceAudio::UpdateShopLife(const FVector& Listener, float DeltaTime)
{
	if (!bShopLife || Indoor < 0.5f || (Tills.Num() == 0 && Fryers.Num() == 0))
	{
		return;
	}
	LifeTimer -= DeltaTime;
	if (LifeTimer > 0.f)
	{
		return;
	}
	LifeTimer = FMath::FRandRange(ShopLifeGap.X, FMath::Max(ShopLifeGap.X, ShopLifeGap.Y));
	UWorld* World = WorldPtr.Get();
	auto Nearest = [&Listener](const TArray<FSpot>& Spots) -> const FSpot*
	{
		const FSpot* Best = nullptr;
		for (const FSpot& Spot : Spots)
		{
			// One of the closer ones, not always the very nearest.
			if (!Best || FVector::DistSquared(Spot.Location, Listener) * FMath::FRandRange(0.5f, 1.5f) < FVector::DistSquared(Best->Location, Listener))
			{
				Best = &Spot;
			}
		}
		return Best;
	};
	const float Roll = FMath::FRand();
	++LifePlayed;
	if (Fryers.Num() > 0 && (Tills.Num() == 0 || Roll < 0.25f))
	{
		const FSpot* Fryer = Nearest(Fryers);
		PlayShop(World, Fryer->Location, Roll < 0.12f ? EGTPShopSound::BasketDrop : EGTPShopSound::FryerTimer);
		return;
	}
	const FSpot* Till = Nearest(Tills);
	const EGTPShopSound Sound = Roll < 0.55f ? EGTPShopSound::CardBeep : Roll < 0.75f ? EGTPShopSound::Receipt : Roll < 0.9f ? EGTPShopSound::OrderChime : EGTPShopSound::Coins;
	PlayShop(World, Till->Location, Sound);
}

FString UGTPPlaceAudio::Describe() const
{
	int32 Busy = 0;
	for (const int32 Emitter : VoiceEmitter)
	{
		Busy += Emitter != INDEX_NONE ? 1 : 0;
	}
	return FString::Printf(TEXT("places emitters %d rooms %d tills %d voices %d/%d indoor %.2f life %d shop %d mission %d"),
		Emitters.Num(), Rooms.Num(), Tills.Num(), Busy, MaxRoomVoices, Indoor, LifePlayed, ShopPlayed, MissionPlayed);
}
