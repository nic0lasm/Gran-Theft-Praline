#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/SoftObjectPath.h"
#include "GTPWeaponLogic.h"
#include "GTPWeapon.generated.h"

class UAnimSequenceBase;
class UNiagaraSystem;
class USkeletalMesh;
class USkeletalMeshComponent;
class APawn;
class APlayerController;

UENUM(BlueprintType)
enum class EGTPWeaponEvent : uint8
{
	Drawn,
	Holstered,
	Fired,
	DryFire,
	ReloadStarted,
	ReloadFinished,
	ReloadCancelled,
	/** Another weapon taken in hand (GetWeaponKey is the new one). */
	Switched
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPWeaponEventSignature, class UGTPWeaponComponent* /*Weapon*/, EGTPWeaponEvent /*Event*/);

/** How one weapon of the arsenal looks and moves (assets out of Git; any of them may be missing). */
USTRUCT()
struct FGTPWeaponLook
{
	GENERATED_BODY()

	/** Mesh in the hand (Lyra SK_Pistol / SK_Rifle once migrated). Empty = no visible gun. */
	UPROPERTY()
	FSoftObjectPath Mesh;

	/** Socket of the mesh the bullets and the flash leave from. */
	UPROPERTY()
	FName MuzzleSocket = TEXT("Muzzle");

	/** Clip name prefix: <Prefix>_Draw, _Holster, _Fire, _Reload, _DryFire, _Aim (see Clips). */
	UPROPERTY()
	FString ClipPrefix;

	UPROPERTY()
	FSoftObjectPath MuzzleFlash;

	/** Gun placement relative to the hand socket (Lyra attaches to weapon_r; on hand_r it needs an offset). */
	UPROPERTY()
	FTransform AttachOffset;
};

/**
 * One animation by name: a montage (played in its own slot, or its first sequence in Slot
 * when Slot is set: the player's animation blueprint may lack Lyra's UpperBody slots), or a
 * sequence played in Slot (AnimSlot when empty). An additive sequence only plays in a slot
 * whose name says Additive, never as a full pose.
 */
USTRUCT()
struct FGTPWeaponClip
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FSoftObjectPath Asset;

	UPROPERTY()
	FName Slot;
};

/**
 * The firearms in a body's hands: the fictional 9 mm pistol and, for the player, the
 * fictional carbine (GTPArsenal in GTPWeaponLogic.h), used the same way by the player
 * (UGTPCombatInput) and by police AI (FireAt, pistol only). It ticks only while a weapon
 * is drawn. A shot traces from the camera (player) or the eyes (AI) along the spread,
 * then from the muzzle to that point, and hands the hit to UGTPCombatSubsystem::ApplyHit.
 * The carbine fires while the trigger is held. While an armed body aims, the subsystem
 * tells the threat system (people put their hands up, flee) and the police.
 *
 * Drive-by: while the player drives, SetDriveBy(Car) puts a one-handed weapon in the
 * hand; aiming and firing go out of the side windows or the windscreen (GTPDriveBy),
 * the car itself is never hit by its driver's shots.
 */
UCLASS(Config = Game, ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPWeaponComponent();

	/** The gun a pawn holds: its own, or at the wheel of a car the seated driver's (HUD, sounds, inputs). */
	static UGTPWeaponComponent* FindHeld(const APawn* Pawn);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetDrawn(bool bDraw);
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ToggleDrawn() { SetDrawn(!bDrawn); }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetAiming(bool bAim);

	/** Trigger down (the player's fire input, or AI). The carbine keeps firing while it stays down. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void PullTrigger();
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ReleaseTrigger();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Reload();

	/** Sprint, dodge, entering a car: stops the reload (nothing is lost). */
	void CancelReload();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SwapShoulder() { Loadout.State().bRightShoulder = !Loadout.State().bRightShoulder; }

	/** Next (Step 1) or previous (-1) carried weapon; draws it. Refused in a car for two-handed ones. */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void CycleWeapon(int32 Step = 1);

	/** Gives the carbine (pickup, mission); with ammunition. */
	void GiveWeapon(GTPArsenal::EKind Kind, int32 Reserve);

	/**
	 * The player sits in Car (nullptr: back on foot). Holsters; in the car only one-handed
	 * weapons come out, and the one held on foot comes back when the player gets out.
	 */
	void SetDriveBy(AActor* Car);
	bool IsDriveBy() const { return DriveByCar.IsValid(); }

	/**
	 * Who the world sees shooting: the owner, or at the wheel the car the player drives (the
	 * player's pawn), so the police crime table, the threat system and the HUD treat a
	 * drive-by like a shot on foot.
	 */
	AActor* GetShooter() const { return DriveByCar.IsValid() ? DriveByCar.Get() : GetOwner(); }

	/** AI: one shot toward Target with its own spread factor (2.5 = police at two stars). */
	bool FireAt(const FVector& Target, float SpreadFactor);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	int32 AddAmmo(int32 Rounds) { return GTPWeapon::AddAmmo(Loadout.State(), Loadout.Spec(), Rounds); }

	/** Arrest: the police take the guns and the ammunition (every weapon carried). */
	void StripAmmo() { Loadout.StripAll(); }

	// The HUD reads these every frame (interface thread).
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsDrawn() const { return bDrawn; }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsAiming() const { return bDrawn && bAiming; }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return Loadout.State().IsReloading(); }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetMagazine() const { return Loadout.State().Magazine; }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetReserve() const { return Loadout.State().Reserve; }
	/** Key of the weapon's name in the string table (GTPStringTable). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FName GetWeaponKey() const;
	/** Current half-angle of the cone (degrees): the reticle radius. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetSpreadDegrees() const;
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsRightShoulder() const { return Loadout.State().bRightShoulder; }
	/** Whether the weapon in hand is automatic (the HUD shows it). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsAutomatic() const { return Loadout.Spec().bAutomatic; }

	GTPArsenal::EKind GetKind() const { return Loadout.CurrentKind(); }
	const GTPWeapon::FSpec& GetSpec() const { return Loadout.Spec(); }

	/** Draw, fire, reload...: animation, sound and HUD listen here. */
	FGTPWeaponEventSignature OnWeaponEvent;

	/** Whether this body is the player (camera trace, recoil on the view, the long weapon). */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	bool bPlayerControlled = false;

protected:
	/** String table keys, in GTPArsenal::EKind order. */
	UPROPERTY(Config)
	TArray<FName> WeaponKeys = {TEXT("Weapon.Pistol9"), TEXT("Weapon.Carbine")};

	/** Looks, in GTPArsenal::EKind order. */
	UPROPERTY(Config)
	TArray<FGTPWeaponLook> Looks;

	/**
	 * Animations by name: <ClipPrefix>_Draw, _Holster, _Fire, _Reload, _DryFire, and _AimLoop
	 * (held while aiming, e.g. Lyra's MM_Pistol_Idle_ADS retargeted). Missing = none.
	 */
	UPROPERTY(Config)
	TArray<FGTPWeaponClip> Clips;

	/** Slot that plays a clip given as a plain sequence (a montage keeps its own). */
	UPROPERTY(Config)
	FName AnimSlot = TEXT("DefaultSlot");

	/** Ammunition the player starts with in reserve: pistol, carbine (0 = not carried). */
	UPROPERTY(Config)
	int32 PlayerPistolReserve = 36;
	UPROPERTY(Config)
	int32 PlayerCarbineReserve = 90;
	UPROPERTY(Config)
	int32 NpcPistolReserve = 36;

	UPROPERTY(Config)
	float SwitchSeconds = 0.45f;

	/** Running speed that counts as "full movement" for the spread (cm/s). */
	UPROPERTY(Config)
	float FullSpeedCmS = 500.f;

	/** Hand socket the gun mesh sits in (falls back to HandFallbackSocket). */
	UPROPERTY(Config)
	FName HandSocket = TEXT("weapon_r");
	UPROPERTY(Config)
	FName HandFallbackSocket = TEXT("hand_r");

private:
	void BuildLoadout();
	bool Fire(const FVector& ViewOrigin, const FVector& ViewDirection, float SpreadFactor);
	bool TryFireFromView();
	FVector GetMuzzle() const;
	float GetSpeedShare() const;
	void PlayClip(const TCHAR* Suffix);
	void SetAimLoop(bool bOn);
	const FGTPWeaponClip* FindClip(const TCHAR* Suffix) const;
	void Broadcast(EGTPWeaponEvent Event) { OnWeaponEvent.Broadcast(this, Event); }
	void LoadLooks();
	void UpdateMesh();
	APlayerController* GetPlayerController() const;
	FRotator GetAimRotation() const;
	void SpawnFlash();
	const FGTPWeaponLook* GetLook() const;
	bool SelectKind(GTPArsenal::EKind Kind);

	GTPArsenal::FLoadout Loadout;
	GTPWeapon::FRandom Rng;
	TWeakObjectPtr<AActor> DriveByCar;
	GTPArsenal::EKind OnFootKind = GTPArsenal::EKind::Pistol;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> GunMesh;

	/** Loaded once per weapon kind (nullptr when the asset is missing). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMesh>> LoadedMeshes;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraSystem>> LoadedFlashes;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UAnimSequenceBase>> LoadedClips;

	bool bDrawn = false;
	bool bAiming = false;
	bool bWasReloading = false;
	bool bLoadoutBuilt = false;
	bool bLooksLoaded = false;
	/** Slot the aim loop plays in while it plays (NAME_None = not playing). */
	FName AimLoopSlot;
};
