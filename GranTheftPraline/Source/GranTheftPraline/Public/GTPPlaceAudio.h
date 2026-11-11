#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GTPPlaceAudio.generated.h"

class UAudioComponent;
class ULevel;
class USoundAttenuation;
class USoundBase;
class USoundMix;
class UWorld;

/** Seamless loops of an interior, folder /Game/Sound/SFX/Interior/<Name>/ (tools/audio/make_places.py). */
UENUM(BlueprintType)
enum class EGTPRoomLoop : uint8
{
	/** Ventilation air of the room, 2D while the listener is inside. */
	RoomTone,
	Fryer,
	Grill,
	/** Extraction hood fan. */
	Hood,
	/** Cold cabinets, fridges, walk-in. */
	Cold,
	/** Shake and ice-cream machine. */
	Shake,
	/** Drink tower: fizz, pour, drip. */
	Drinks,
	Count UMETA(Hidden)
};

/** One-shots of a shop, folder /Game/Sound/SFX/Shop/<Name>/. */
UENUM(BlueprintType)
enum class EGTPShopSound : uint8
{
	TillOpen,
	TillClose,
	Coins,
	Notes,
	Receipt,
	CardBeep,
	FryerTimer,
	BasketDrop,
	OrderChime,
	DoorChime,
	/** Back-door alarm (a staff member slipping out during a hold-up). The counter alarm is silent. */
	AlarmBuzzer,
	Safe,
	Count UMETA(Hidden)
};

/** Mission stings (2D, Interface class), folder /Game/Sound/SFX/Mission/<Name>/. */
UENUM(BlueprintType)
enum class EGTPMissionSound : uint8
{
	Available,
	Start,
	Objective,
	Checkpoint,
	Success,
	Fail,
	Reward,
	Countdown,
	Count UMETA(Hidden)
};

/**
 * Sound of places (ADR-0009, docs/audio/audio-design.md "Interiors and missions"), owned by
 * UGTPAudioWorld. The interior and mission code knows nothing of it beyond the two calls below.
 *
 * Interiors (ADR-0012): when a streamed level whose name contains "Interior" becomes visible,
 * its components are scanned once:
 * - loop emitters: a tag GTP_AudioLoop=<EGTPRoomLoop name>, or else a component, actor or mesh
 *   name with fryer, grill, hood, fridge/freezer/cold, shake or drink in it; same loops closer
 *   than 1.5 m merge;
 * - rooms: components tagged GTP_AudioRoom (their bounds); without any, the level bounds;
 * - till points (till, register, pos, kiosk in the name) for the shop's own life.
 * A small pool of looping voices (MaxRoomVoices) follows the nearest emitters, so streaming
 * creates and destroys no audio object. Inside a room, an indoor mix lowers and muffles the
 * street (traffic, sirens, rain, the day loop) and the room tone fades in; walking in plays
 * the door chime. Until the staff loops play their own sounds (PlayShop), a light "shop life"
 * plays card beeps, receipts, order chimes and fryer timers at the tills and fryers (-ShopLife=0 turns it off).
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPPlaceAudio : public UObject
{
	GENERATED_BODY()

public:
	static UGTPPlaceAudio* Get(const UObject* WorldContext);

	/** A shop one-shot at a point (Impacts class: attenuation and voice limit). */
	UFUNCTION(BlueprintCallable, Category = "Audio|Places", meta = (WorldContext = "WorldContext"))
	static void PlayShop(const UObject* WorldContext, FVector Location, EGTPShopSound Sound);

	/** A mission sting, heard in 2D through the Interface class. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Places", meta = (WorldContext = "WorldContext"))
	static void PlayMission(const UObject* WorldContext, EGTPMissionSound Sound);

	void Start(UWorld* World);
	void Stop();
	void Tick(float DeltaTime);

	/** 0 outdoors, 1 inside a room, gliding. */
	float GetIndoor() const { return Indoor; }

	/** "places emitters 9 rooms 1 tills 4 voices 3/6 indoor 1.00 life 5" for the stats line. */
	FString Describe() const;

protected:
	/** Looping emitter voices at once (each a few tenths of a percent of a core). */
	UPROPERTY(Config)
	int32 MaxRoomVoices = 6;

	/** Emitters further than this are not played (cm). */
	UPROPERTY(Config)
	float EmitterRange = 2500.f;

	UPROPERTY(Config)
	float LoopVolume = 0.8f;

	UPROPERTY(Config)
	float RoomToneVolume = 0.5f;

	UPROPERTY(Config)
	float ShopVolume = 0.9f;

	UPROPERTY(Config)
	float MissionVolume = 0.8f;

	/** Indoor mix: what is left of the street outside (volume) and its muffling (low-pass, Hz). */
	UPROPERTY(Config)
	float IndoorStreetVolume = 0.3f;

	UPROPERTY(Config)
	float IndoorTrafficVolume = 0.35f;

	UPROPERTY(Config)
	float IndoorLowPassHz = 1500.f;

	/** Seconds between two shop life sounds (random in the range); 0 turns it off. */
	UPROPERTY(Config)
	FVector2D ShopLifeGap = FVector2D(3.f, 9.f);

private:
	struct FEmitter
	{
		EGTPRoomLoop Loop = EGTPRoomLoop::RoomTone;
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<ULevel> Level;
	};
	struct FSpot
	{
		FVector Location = FVector::ZeroVector;
		TWeakObjectPtr<ULevel> Level;
	};
	struct FRoom
	{
		FBox Box;
		TWeakObjectPtr<ULevel> Level;
	};

	void LoadBanks();
	USoundBase* PickLoop(EGTPRoomLoop Loop) const;
	USoundBase* PickShop(EGTPShopSound Sound);
	USoundBase* PickMission(EGTPMissionSound Sound);
	void OnLevelAdded(ULevel* Level, UWorld* World);
	void OnLevelRemoved(ULevel* Level, UWorld* World);
	void ScanLevel(ULevel* Level);
	void ForgetLevel(const ULevel* Level);
	void UpdateVoices(const FVector& Listener);
	void UpdateIndoor(const FVector& Listener, float DeltaTime);
	void UpdateShopLife(const FVector& Listener, float DeltaTime);

	TWeakObjectPtr<UWorld> WorldPtr;
	FDelegateHandle AddedHandle;
	FDelegateHandle RemovedHandle;

	/** Keeps every loaded sound alive; the banks below point into it. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> AllSounds;
	TArray<USoundBase*> LoopBank[static_cast<int32>(EGTPRoomLoop::Count)];
	TArray<USoundBase*> ShopBank[static_cast<int32>(EGTPShopSound::Count)];
	TArray<USoundBase*> MissionBank[static_cast<int32>(EGTPMissionSound::Count)];
	int32 LastShop[static_cast<int32>(EGTPShopSound::Count)] = {};

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> Voices;
	/** Emitter index each voice plays, -1 when free. */
	TArray<int32> VoiceEmitter;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> RoomTone;

	UPROPERTY(Transient)
	TObjectPtr<USoundAttenuation> LoopAttenuation;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> IndoorMix;

	TArray<FEmitter> Emitters;
	TArray<FRoom> Rooms;
	TArray<FSpot> Tills;
	TArray<FSpot> Fryers;

	float SenseTimer = 0.f;
	float Indoor = 0.f;
	bool bInside = false;
	bool bIndoorMixOn = false;
	bool bShopLife = true;
	float LifeTimer = 0.f;
	int32 LifePlayed = 0;
	int32 ShopPlayed = 0;
	int32 MissionPlayed = 0;
};
