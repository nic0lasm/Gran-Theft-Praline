#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GTPWeaponLogic.h"
#include "GTPWeaponPickup.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USphereComponent;

/**
 * A gun on the ground (GTA-like): dropped by a body that died armed or was disarmed
 * (GTPPickup in GTPWeaponLogic.h). It turns slowly so it catches the eye; the player
 * walking over it takes the weapon or its ammunition (UGTPWeaponComponent::CollectAmmo).
 * Made by UGTPCombatSubsystem::DropWeapon, which keeps a handful at most; each one goes
 * after LifeSeconds. Shows the gun's mesh when it is loaded, a small dark box otherwise.
 */
UCLASS(NotBlueprintable)
class GRANTHEFTPRALINE_API AGTPWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	AGTPWeaponPickup();

	void Setup(GTPArsenal::EKind InKind, int32 InRounds, USkeletalMesh* GunMesh, float LifeSeconds);

	GTPArsenal::EKind GetKind() const { return Kind; }
	int32 GetRounds() const { return Rounds; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void NotifyActorBeginOverlap(AActor* Other) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Gun;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Box;

	GTPArsenal::EKind Kind = GTPArsenal::EKind::Pistol;
	int32 Rounds = 0;
};
