#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GTPCombat.h"
#include "GTPWeapon.h"
#include "GTPCombatSounds.generated.h"

class UWorld;

/**
 * Combat sounds played by listening to the combat system (ADR-0009, docs/audio/audio-design.md,
 * "Combat"), owned by UGTPAudioWorld. The combat code knows nothing of it:
 * - UGTPCombatSubsystem::OnShotFired: the shot (near or far, street echo, whizz, ducking)
 *   and, when it hit, the bullet impact chosen from the hit (UGTPCombatAudio::ImpactKindOf);
 * - OnDamageDealt with bMelee: a light or heavy blow (bullet hits already sound as Flesh impacts);
 * - UGTPWeaponComponent::OnWeaponEvent of the player's weapon: dry fire, magazine out then in
 *   during a reload, slide when it ends.
 * Nothing happens under -Combat=0 (no subsystem). The player's weapon is looked up twice a second
 * because the combat system gives it to the pawn after begin play and again after a respawn.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPCombatSounds : public UObject
{
	GENERATED_BODY()

public:
	void Start(UWorld* World);
	void Stop();
	void Tick(float DeltaTime);

	/** "combat shots 3 impacts 2 blows 1 handling 4" for the stats line. */
	FString Describe() const;

private:
	void OnShotFired(AActor* Shooter, const FVector& Origin, const FVector& Direction, FName Weapon, const FHitResult& Hit, bool bHit);
	void OnDamageDealt(AActor* Instigator, AActor* Victim, float Damage, GTPDamage::EZone Zone, bool bMelee, float HealthAfter);
	void OnWeaponEvent(UGTPWeaponComponent* Weapon, EGTPWeaponEvent Event);
	void BindPlayerWeapon();

	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<UGTPCombatSubsystem> CombatPtr;
	TWeakObjectPtr<UGTPWeaponComponent> WeaponPtr;
	FDelegateHandle ShotHandle;
	FDelegateHandle DamageHandle;
	FDelegateHandle WeaponHandle;
	float LookupTimer = 0.f;
	/** Seconds before the magazine goes back in during a reload, -1 when none is due. */
	float MagInLeft = -1.f;
	int32 Shots = 0;
	int32 Impacts = 0;
	int32 Blows = 0;
	int32 Handling = 0;
};
