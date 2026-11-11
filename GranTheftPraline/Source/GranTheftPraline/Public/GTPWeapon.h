#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPWeaponLogic.h"
#include "GTPWeapon.generated.h"

class UAnimMontage;
class USoundBase;

UENUM(BlueprintType)
enum class EGTPWeaponEvent : uint8
{
	Drawn,
	Holstered,
	Fired,
	DryFire,
	ReloadStarted,
	ReloadFinished,
	ReloadCancelled
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPWeaponEventSignature, class UGTPWeaponComponent* /*Weapon*/, EGTPWeaponEvent /*Event*/);

/**
 * A firearm in a body's hands (version 0.7: the fictional 9 mm pistol), used the same way
 * by the player (inputs from UGTPCombatInput) and by police AI (FireAt). Rules in
 * GTPWeaponLogic.h. It ticks only while the weapon is drawn. A shot traces from the
 * camera (player) or the eyes (AI) along the spread, then from the hand to that point,
 * and hands the hit to UGTPCombatSubsystem::ApplyHit. While an armed body aims, the
 * subsystem tells the threat system (people put their hands up, flee).
 */
UCLASS(Config = Game, ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPWeaponComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetDrawn(bool bDraw);
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ToggleDrawn() { SetDrawn(!bDrawn); }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetAiming(bool bAim);

	/** Trigger down (the player's fire input, or AI). */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void PullTrigger();
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void ReleaseTrigger();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Reload();

	/** Sprint, dodge, entering a car: stops the reload (nothing is lost). */
	void CancelReload();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SwapShoulder() { State.bRightShoulder = !State.bRightShoulder; }

	/** AI: one shot toward Target with its own spread factor (2.5 = police at two stars). */
	bool FireAt(const FVector& Target, float SpreadFactor);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	int32 AddAmmo(int32 Rounds) { return GTPWeapon::AddAmmo(State, Spec, Rounds); }

	// The HUD reads these every frame (interface thread).
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsDrawn() const { return bDrawn; }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsAiming() const { return bDrawn && bAiming; }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsReloading() const { return State.IsReloading(); }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetMagazine() const { return State.Magazine; }
	UFUNCTION(BlueprintPure, Category = "Weapon")
	int32 GetReserve() const { return State.Reserve; }
	/** Key of the weapon's name in the string table (GTPStringTable). */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	FName GetWeaponKey() const { return WeaponKey; }
	/** Current half-angle of the cone (degrees): the reticle radius. */
	UFUNCTION(BlueprintPure, Category = "Weapon")
	float GetSpreadDegrees() const;
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool IsRightShoulder() const { return State.bRightShoulder; }

	const GTPWeapon::FSpec& GetSpec() const { return Spec; }

	/** Draw, fire, reload...: animation, sound and HUD listen here. */
	FGTPWeaponEventSignature OnWeaponEvent;

	/** Montages by name: AM_Pistol_Draw, AM_Pistol_Holster, AM_Pistol_Fire, AM_Pistol_Reload. */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TMap<FName, TSoftObjectPtr<UAnimMontage>> Clips;

	/** Whether this body is the player (camera trace, aim assist, recoil on the view). */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	bool bPlayerControlled = false;

protected:
	UPROPERTY(Config)
	FName WeaponKey = TEXT("Weapon.Pistol9");
	UPROPERTY(Config)
	int32 MagazineSize = 12;
	UPROPERTY(Config)
	int32 MaxReserve = 96;
	UPROPERTY(Config)
	int32 StartReserve = 36;
	UPROPERTY(Config)
	float RoundsPerMinute = 330.f;
	UPROPERTY(Config)
	float ReloadSeconds = 1.5f;
	UPROPERTY(Config)
	float RangeM = 60.f;
	UPROPERTY(Config)
	float HipSpreadDeg = 3.0f;
	UPROPERTY(Config)
	float AimSpreadDeg = 0.5f;
	UPROPERTY(Config)
	float BloomPerShotDeg = 0.7f;
	UPROPERTY(Config)
	float MaxBloomDeg = 4.0f;
	UPROPERTY(Config)
	float MoveSpreadDeg = 2.0f;
	UPROPERTY(Config)
	float RecoilPitchDeg = 1.4f;
	UPROPERTY(Config)
	float BaseDamage = 20.f;
	UPROPERTY(Config)
	float HeadFactor = 2.5f;
	/** Running speed that counts as "full movement" for the spread (cm/s). */
	UPROPERTY(Config)
	float FullSpeedCmS = 500.f;
	/** Muzzle socket on the character mesh (falls back to the right hand). */
	UPROPERTY(Config)
	FName MuzzleSocket = TEXT("hand_r");

private:
	void ApplySpec();
	bool Fire(const FVector& ViewOrigin, const FVector& ViewDirection, float SpreadFactor);
	FVector GetMuzzle() const;
	float GetSpeedShare() const;
	void PlayClip(FName Name);
	void Broadcast(EGTPWeaponEvent Event) { OnWeaponEvent.Broadcast(this, Event); }

	GTPWeapon::FSpec Spec;
	GTPWeapon::FState State;
	GTPWeapon::FRandom Rng;
	bool bDrawn = false;
	bool bAiming = false;
	bool bWasReloading = false;
	bool bSpecApplied = false;
};
