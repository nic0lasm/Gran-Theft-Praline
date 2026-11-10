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

	/** Crowd pedestrian (ADR-0005), recognised by class name to stay independent of that code. */
	static bool IsPedestrian(const AActor* Actor);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	USoundClass* GetClass(EGTPAudioCategory Category) const;
	USoundConcurrency* GetConcurrency(EGTPAudioCategory Category) const;
	USoundAttenuation* GetAttenuation(EGTPAudioCategory Category) const;
	USoundMix* GetUserMix() const { return UserMix; }
	USoundMix* GetDrivingMix() const { return DrivingMix; }

	/** Sets the volume of a category (0..1) in the user mix; the world subsystem applies it. */
	UFUNCTION(BlueprintCallable, Category = "Audio")
	void SetCategoryVolume(EGTPAudioCategory Category, float Volume);

	float GetCategoryVolume(EGTPAudioCategory Category) const;

	/** Random step or landing sound for a surface, never twice the same in a row; null when the bank is empty. */
	USoundBase* PickFootstep(EGTPSurface Surface, bool bLanding);
	USoundBase* PickImpact(EGTPImpactKind Kind, int32 Tier);
	USoundBase* PickGlass();

	/** Surface under a trace hit: physical material, then material name, then the street grid. */
	EGTPSurface ResolveSurface(const FHitResult& Hit) const;

	/** Plays a one-shot at a point through a category (class, attenuation, concurrency). */
	UAudioComponent* PlayAt(const UObject* WorldContext, USoundBase* Sound, EGTPAudioCategory Category, const FVector& Location, float Volume, float Pitch);

	/** Counters for the soak test log line. */
	int32 GetBankSize() const;
	int32 FootstepsPlayed = 0;
	int32 ImpactsPlayed = 0;

private:
	void BuildClasses();
	void BuildMixes();
	void BuildConcurrencyAndAttenuation();
	void LoadBanks();
	void LoadSurfaceMap();
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

	/** Keeps the bank sounds loaded (the FBank arrays are not seen by the garbage collector). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> AllSounds;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> MutedClass;

	void MuteFolders();

	TMap<EGTPAudioCategory, float> Volumes;
	FGTPSurfaceMap SurfaceMap;
	FString GroundKeyword;
};
