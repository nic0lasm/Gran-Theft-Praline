#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GTPDamageLogic.h"
#include "GTPHealth.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPCombat.generated.h"

GRANTHEFTPRALINE_API DECLARE_LOG_CATEGORY_EXTERN(LogGTPCombat, Log, All);

class UNiagaraSystem;
class UGTPWeaponComponent;
class UGTPCombatInput;
class UMaterialInterface;

/** One hit, as the weapon or the fist that caused it knows it. */
struct FGTPCombatHit
{
	TWeakObjectPtr<AActor> Instigator;
	TWeakObjectPtr<AActor> Victim;
	/** Bone that was hit (NAME_None = torso). */
	FName Bone = NAME_None;
	FVector Location = FVector::ZeroVector;
	/** Direction the hit travels, attacker toward victim. */
	FVector Direction = FVector::ForwardVector;
	/** Damage table of the weapon or the blow (the subsystem applies zone, distance, difficulty). */
	GTPDamage::FWeaponDamage Damage;
	float DistanceM = 0.f;
	bool bMelee = false;
	/** A heavy blow (third of a combo): knocks down. */
	bool bHeavy = false;
	/** Damage already decided by the caller (melee blow after guard); < 0 = compute from Damage. */
	float FixedDamage = -1.f;
};

/** Hit is valid only when bHit (the audio thread plays the bullet impact from it). */
DECLARE_MULTICAST_DELEGATE_SixParams(FGTPShotFired, AActor* /*Shooter*/, const FVector& /*Origin*/, const FVector& /*Direction*/, FName /*Weapon*/, const FHitResult& /*Hit*/, bool /*bHit*/);
DECLARE_MULTICAST_DELEGATE_SixParams(FGTPDamageDealt, AActor* /*Instigator*/, AActor* /*Victim*/, float /*Damage*/, GTPDamage::EZone /*Zone*/, bool /*bMelee*/, float /*HealthAfter*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPPawnKilled, AActor* /*Killer*/, AActor* /*Victim*/, bool /*bMelee*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FGTPPlayerDied, const FVector& /*Where*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPPlayerRespawned, const FVector& /*Where*/, FName /*Hospital*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPArmedThreat, AActor* /*Source*/, const FVector& /*Origin*/, const FVector& /*Aim*/);

/**
 * Combat for one world (ADR-0011): the single entry point for every hit (ApplyHit),
 * what other systems listen to (police, pedestrians, HUD, audio, missions), impact
 * effects from a capped pool, the 10 Hz step of every body's health, and the player's
 * death and respawn at the nearest fictional hospital (ATargetPoint tagged GTPHospital).
 * It also gives the player's pawn its health, hit reaction and weapon, and binds the
 * combat inputs. -Combat=0 turns the whole system off (the subsystem is not created).
 * -CombatBench=1 is the firefight scene of the M1 gate: the player fires the pistol
 * every 0.45 s along a fixed sweep and every tenth shot kills the nearest passer-by
 * (ragdoll, blood pool), with no input and the same shots every run.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPCombatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static bool IsEnabled();
	static UGTPCombatSubsystem* Get(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Resolves a hit: damage, health, reaction, threat, effect, delegates. Returns the outcome on the victim. */
	FGTPDamageOutcome ApplyHit(const FGTPCombatHit& Hit);

	/** A shot left a gun (called by UGTPWeaponComponent once per shot, before its hit if any). */
	void NotifyShot(AActor* Shooter, const FVector& Origin, const FVector& Direction, FName Weapon, const FHitResult& Hit, bool bHit);

	/** An armed body aims (every step while it does); bAiming false once when it stops. */
	void NotifyAim(AActor* Source, const FVector& Origin, const FVector& Aim, bool bAiming);

	/** Takes the gun out of Body's hands and leaves it on the ground with some ammunition (AGTPWeaponPickup). */
	void DropWeapon(AActor* Body, UGTPWeaponComponent* Gun);

	/** A bullet hit something that is not a body: effect only. */
	void SpawnImpact(const FHitResult& Hit, const FVector& Direction);

	void RegisterHealth(UGTPHealthComponent* Health);
	void UnregisterHealth(UGTPHealthComponent* Health);

	/** The player's pawn (the car while driving). */
	APawn* GetPlayerPawn() const;

	/** Gamepad aim: turns the player's view onto the best body near the reticle (GTPAimAssist). */
	void SnapPlayerAim(UGTPWeaponComponent* Weapon);

	/** Best living body near the line Origin + Direction (cone in degrees), nullptr when none. */
	AActor* FindAimTarget(const FVector& Origin, const FVector& Direction, float ConeDeg, float MaxDistanceM, const AActor* Ignore) const;

	/** The -CombatBench firefight runs: listeners that call the police skip it (same scene every pass). */
	bool IsBenchRunning() const { return BenchTimer.IsValid(); }

	/** The player, the player's body seated in the car the player drives, or that car. */
	bool IsPlayerActor(const AActor* Actor) const { return IsPlayer(Actor); }

	/** HUD hit marker: seconds since the player last hit someone, and whether that hit killed. */
	float GetSecondsSincePlayerHit(bool& bKilled) const;

	FGTPShotFired OnShotFired;
	FGTPDamageDealt OnDamageDealt;
	FGTPPawnKilled OnPawnKilled;
	FGTPPlayerDied OnPlayerDied;
	FGTPPlayerRespawned OnPlayerRespawned;
	FGTPArmedThreat OnArmedThreat;

protected:
	/** Niagara system per impact kind, in GTPDamage::EImpact order (Dust, Sparks, CarBody, Glass, Wood, Dirt, Water, Flesh). */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> ImpactSystems;

	/** Impact effects alive at once (M1 budget, docs/gameplay/combat.md). */
	UPROPERTY(Config)
	int32 MaxLiveImpacts = 12;

	UPROPERTY(Config)
	float ImpactLifeSeconds = 0.6f;

	/** NPC weapons: share of the damage the player takes. */
	UPROPERTY(Config)
	float PlayerDamageTaken = 0.45f;

	/** Blood pool decal under a dead body ("GTA-like" blood, chosen by Nico on 2026-11-11), made by tools/combat/unreal/make_blood_material.py. Missing = none. */
	UPROPERTY(Config)
	FSoftObjectPath BloodPoolMaterial = FSoftObjectPath(TEXT("/Game/Combat/Materials/M_GTP_BloodPool.M_GTP_BloodPool"));

	/** Bullet hole decal on walls, ground, furniture (tools/combat/unreal/make_blood_material.py builds it). Missing = none. */
	UPROPERTY(Config)
	FSoftObjectPath BulletHoleMaterial = FSoftObjectPath(TEXT("/Game/Combat/Materials/M_GTP_BulletHole.M_GTP_BulletHole"));

	/** On glass (Lyra's M_GlassBulletHoles_Decal once migrated); missing = the plain hole. */
	UPROPERTY(Config)
	FSoftObjectPath GlassHoleMaterial;

	/** Guns on the ground at once (the oldest goes first) and how long each stays. */
	UPROPERTY(Config)
	int32 MaxPickups = 6;
	UPROPERTY(Config)
	float PickupLifeSeconds = 60.f;

	/** Bullet holes in the world at once (the oldest goes first), their life and size. */
	UPROPERTY(Config)
	int32 MaxBulletHoles = 24;
	UPROPERTY(Config)
	float BulletHoleLifeSeconds = 30.f;
	UPROPERTY(Config)
	float BulletHoleSizeCm = 5.f;

	/** Blood pools in the world at once; the oldest goes first. */
	UPROPERTY(Config)
	int32 MaxBloodPools = 8;

	UPROPERTY(Config)
	float BloodPoolLifeSeconds = 90.f;

	/** Each bullet in a body adds this to the scalar "GTPBlood" of its materials (stains, 0..1). */
	UPROPERTY(Config)
	float BloodStainPerHit = 0.25f;

	/** Gamepad aim snaps to a body this close to the reticle (degrees) and distance (m). */
	UPROPERTY(Config)
	bool bAimAssist = true;
	UPROPERTY(Config)
	float AimSnapConeDeg = 9.f;
	UPROPERTY(Config)
	float AimSnapDistanceM = 40.f;

	/** OnArmedThreat (the police crime table) fires when a body is this close to the player's aim. */
	UPROPERTY(Config)
	float ThreatConeDeg = 12.f;
	UPROPERTY(Config)
	float ThreatDistanceM = 25.f;

	/** Grace without damage after a respawn (s). */
	UPROPERTY(Config)
	float RespawnGraceSeconds = 3.f;

private:
	void StepHealth();
	void EquipPlayer();
	void OnHealthDied(UGTPHealthComponent* Health, EGTPDamageCause Cause, AActor* Killer);
	void RespawnPlayer();
	void WarmUpEffects();
	void BenchShot();
	void SpawnBloodPool(AActor* Body);
	void SpawnBulletHole(const FHitResult& Hit, bool bGlass);
	void AddBloodStain(AActor* Body);
	bool IsPlayer(const AActor* Actor) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGTPHealthComponent>> Bodies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraSystem>> LoadedImpacts;

	UPROPERTY(Transient)
	TObjectPtr<UGTPCombatInput> Input;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedBloodPool;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedBulletHole;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedGlassHole;

	TWeakObjectPtr<APawn> EquippedPawn;
	TWeakObjectPtr<APawn> DeadPawn;
	FVector DeathLocation = FVector::ZeroVector;
	TArray<double> LiveImpactEnds;
	TArray<TWeakObjectPtr<class UDecalComponent>> BloodPools;
	TArray<TWeakObjectPtr<class UDecalComponent>> BulletHoles;
	TArray<TWeakObjectPtr<class AGTPWeaponPickup>> Pickups;
	TMap<TWeakObjectPtr<AActor>, float> BloodStains;
	double LastPlayerHitTime = -1000.0;
	bool bLastPlayerHitKilled = false;
	bool bThreatActive = false;
	bool bThreatBroadcast = false;
	double NextThreatCheckTime = 0.0;
	FTimerHandle HealthTimer;
	FTimerHandle EquipTimer;
	FTimerHandle RespawnTimer;
	FTimerHandle BenchTimer;
	int32 BenchShots = 0;
};
