#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPAudioTypes.h"
#include "GTPSurfaceLogic.h"
#include "GTPAudioMix.generated.h"

class AActor;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;
class USoundClass;
class USoundConcurrency;
class USoundMix;
struct FHitResult;

/**
 * Mix of the game, built in code once per game instance (ADR-0009):
 * - one sound class per EGTPAudioCategory, children of Master and Effects;
 * - a user mix holding the category volumes (options menu, later);
 * - a driving mix (ambience lower and duller while the player drives);
 * - concurrency groups and attenuation presets per category;
 * - sound banks found by folder under UGTPAudioSettings::SoundRoot;
 * - the surface grid of the focus street.
 * Gameplay code asks it for sounds and settings; it plays nothing itself.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPAudioMix : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UGTPAudioMix* Get(const UObject* WorldContext);

	/**
	 * False when the game runs with -Audio=0 (or -NoGameAudio): nothing of
	 * ADR-0009 is added to the world and SetSiren does nothing, to measure or
	 * test a system without it. The engine, the ambience and GASP sounds stay.
	 */
	static bool IsGameAudioEnabled();

	/**
	 * False with -SynthAudio=0: no procedural sound wave is created at all (car engines,
	 * sirens, skids), even with -Audio=0, which keeps the engines. Crash hunt A/B switch:
	 * with both -Audio=0 and -SynthAudio=0, no game sound object is created or destroyed during play.
	 */
	static bool IsSynthAudioEnabled();

	/** Crowd pedestrian (ADR-0005), recognised by class name to stay independent of that code. */
	static bool IsPedestrian(const AActor* Actor);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	USoundClass* GetClass(EGTPAudioCategory Category) const;
	USoundConcurrency* GetConcurrency(EGTPAudioCategory Category) const;
	USoundAttenuation* GetAttenuation(EGTPAudioCategory Category) const;
	USoundMix* GetUserMix() const { return UserMix; }
	USoundMix* GetDrivingMix() const { return DrivingMix; }
	USoundMix* GetGunshotMix() const { return GunshotMix; }

	/** Sets the volume of a category (0..1) in the user mix; the world subsystem applies it. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetCategoryVolume(EGTPAudioCategory Category, float Volume);

	float GetCategoryVolume(EGTPAudioCategory Category) const;

	/** Random step or landing sound for a surface, never twice the same in a row; null when the bank is empty. */
	USoundBase* PickFootstep(EGTPSurface Surface, bool bLanding);
	USoundBase* PickImpact(EGTPImpactKind Kind, int32 Tier);
	USoundBase* PickGlass();
	USoundBase* PickVoice(EGTPVoiceKind Kind);
	USoundBase* PickRadio(EGTPRadioCall Call);
	USoundBase* PickUI(EGTPUISound Sound);
	USoundBase* PickWeapon(EGTPWeaponSound Sound);

	/**
	 * Sound of a weapon (UGTPAudioSettings::WeaponVoices): its recorded Shot or DryFire when set and
	 * loadable, else the synthesised bank; OutPitch and OutVolume carry the weapon's own setting.
	 */
	USoundBase* PickWeaponFor(FName Weapon, EGTPWeaponSound Sound, float& OutPitch, float& OutVolume);

	/** False when Shooter fired less than Gap seconds ago (automatic fire: one tail per burst); records the time. */
	bool ShooterReady(const AActor* Shooter, float Gap, double Now);
	USoundBase* PickBulletImpact(EGTPBulletImpact Kind);
	USoundBase* PickMelee(EGTPMeleeSound Sound);

	/**
	 * Plays an interface sound (menus, map, HUD): 2D, Interface class, heard over
	 * the pause menu. Does nothing when the UI/<Name> folder is empty or with -Audio=0.
	 */
	UFUNCTION(BlueprintCallable, Category = "Audio", meta = (WorldContext = "WorldContext"))
	static void PlayUI(const UObject* WorldContext, EGTPUISound Sound);

	/** Plays a non-spatial one-shot through a category. */
	UAudioComponent* Play2D(const UObject* WorldContext, USoundBase* Sound, EGTPAudioCategory Category, float Volume, float Pitch);

	/** Surface under a trace hit: physical material, then material name, then the street grid. */
	EGTPSurface ResolveSurface(const FHitResult& Hit) const;

	/** Ambience zone at a world point (Street outside the grid). */
	EGTPAmbienceZone SampleZone(const FVector& Location) const;

	/**
	 * Uses the surface and zone grids made for a map when they exist: L_City_Quarter reads
	 * Sound/SurfaceMap/quarter.gtpsurf and zones-quarter.gtpsurf (tools/audio/mac_audio_pass.sh
	 * FOCUS=quarter). Otherwise the grids of UGTPAudioSettings stay. Called by the audio world.
	 */
	void UseGridsForMap(const FString& MapName);

	/** "rue-de-la-republique" for L_City_RueDeLaRepublique (also without the PIE prefix). */
	static FString FocusOfMap(const FString& MapName);

	/** Plays a one-shot at a point through a category (class, attenuation, concurrency). */
	UAudioComponent* PlayAt(const UObject* WorldContext, USoundBase* Sound, EGTPAudioCategory Category, const FVector& Location, float Volume, float Pitch);

	/** Counters for the soak test log line. */
	int32 GetBankSize() const;
	int32 FootstepsPlayed = 0;
	int32 ImpactsPlayed = 0;
	int32 VoicesPlayed = 0;
	int32 RadioPlayed = 0;

private:
	void BuildClasses();
	void BuildMixes();
	void BuildConcurrencyAndAttenuation();
	void LoadBanks();
	void LoadSurfaceMap();
	bool LoadGrids(const FString& SurfaceFile, const FString& ZoneFile, bool bRequired);
	FString GridsFocus;
	USoundBase* PickFrom(TArray<TObjectPtr<USoundBase>>& Bank, int32& LastIndex);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundClass>> Classes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundConcurrency>> Concurrency;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundAttenuation>> Attenuation;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> UserMix;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> DrivingMix;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> GunshotMix;

	/** Bank arrays, flattened: Steps[surface], Lands[surface], Impacts[kind * 3 + tier - 1]. */
	struct FBank
	{
		TArray<TObjectPtr<USoundBase>> Sounds;
		int32 Last = INDEX_NONE;
	};
	FBank Steps[static_cast<int32>(EGTPSurface::Count)];
	FBank Lands[static_cast<int32>(EGTPSurface::Count)];
	FBank Impacts[static_cast<int32>(EGTPImpactKind::Count) * 3];
	FBank Glass;
	/** Voices/<Kind>, Radio/<Call>, UI/<Sound>: one bank per folder name. */
	FBank Voices[static_cast<int32>(EGTPVoiceKind::Count)];
	FBank Radio[static_cast<int32>(EGTPRadioCall::Count)];
	FBank Interface[static_cast<int32>(EGTPUISound::Count)];
	FBank WeaponBanks[static_cast<int32>(EGTPWeaponSound::Count)];
	FBank BulletBanks[static_cast<int32>(EGTPBulletImpact::Count)];
	FBank MeleeBanks[static_cast<int32>(EGTPMeleeSound::Count)];
	int32 UIPlayed = 0;

	/** Keeps the bank sounds loaded (the FBank arrays are not seen by the garbage collector). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> AllSounds;

	/** Recorded weapon sounds already loaded, by soft path ("" when it failed: not tried again). */
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<USoundBase>> WeaponOverrides;

	TMap<TWeakObjectPtr<const AActor>, double> LastBurst;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MutedClass;

	void MuteFolders();

	TMap<EGTPAudioCategory, float> Volumes;
	FGTPSurfaceMap SurfaceMap;
	FGTPSurfaceMap ZoneMap;
	FString GroundKeyword;
};
