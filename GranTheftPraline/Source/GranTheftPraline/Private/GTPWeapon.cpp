#include "GTPWeapon.h"

#include "GTPCombat.h"
#include "GTPPedestrians.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GTPLog.h"

UGTPWeaponComponent::UGTPWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGTPWeaponComponent::ApplySpec()
{
	if (bSpecApplied)
	{
		return;
	}
	bSpecApplied = true;
	Spec.MagazineSize = FMath::Max(1, MagazineSize);
	Spec.MaxReserve = FMath::Max(0, MaxReserve);
	Spec.RoundsPerMinute = RoundsPerMinute;
	Spec.ReloadSeconds = ReloadSeconds;
	Spec.RangeM = RangeM;
	Spec.HipSpreadDeg = HipSpreadDeg;
	Spec.AimSpreadDeg = AimSpreadDeg;
	Spec.BloomPerShotDeg = BloomPerShotDeg;
	Spec.MaxBloomDeg = MaxBloomDeg;
	Spec.MoveSpreadDeg = MoveSpreadDeg;
	Spec.RecoilPitchDeg = RecoilPitchDeg;
	Spec.Damage.Base = BaseDamage;
	Spec.Damage.HeadFactor = HeadFactor;
	State = GTPWeapon::MakeState(Spec, StartReserve);
	// Seeded from the owner's name so a deterministic demo replays the same shots.
	Rng = GTPWeapon::FRandom(static_cast<uint32>(GetTypeHash(GetOwner() ? GetOwner()->GetName() : FString())) | 1u);
}

void UGTPWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplySpec();
}

void UGTPWeaponComponent::SetDrawn(bool bDraw)
{
	ApplySpec();
	if (bDraw == bDrawn)
	{
		return;
	}
	bDrawn = bDraw;
	SetComponentTickEnabled(bDrawn);
	if (!bDrawn)
	{
		SetAiming(false);
		GTPWeapon::ReleaseTrigger(State);
		CancelReload();
	}
	PlayClip(bDrawn ? TEXT("AM_Pistol_Draw") : TEXT("AM_Pistol_Holster"));
	Broadcast(bDrawn ? EGTPWeaponEvent::Drawn : EGTPWeaponEvent::Holstered);
}

void UGTPWeaponComponent::SetAiming(bool bAim)
{
	const bool bWas = IsAiming();
	bAiming = bAim;
	if (bWas && !IsAiming())
	{
		if (UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this))
		{
			Combat->NotifyAim(GetOwner(), FVector::ZeroVector, FVector::ForwardVector, false);
		}
	}
	// Facing the aim is done in Tick (the owner turns to the control yaw).
}

void UGTPWeaponComponent::PullTrigger()
{
	ApplySpec();
	if (!bDrawn)
	{
		return;
	}
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (const APlayerController* PC = Cast<APlayerController>(Controller))
	{
		FRotator ViewRotation;
		PC->GetPlayerViewPoint(Origin, ViewRotation);
		Direction = ViewRotation.Vector();
	}
	else if (Controller)
	{
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(Origin, ViewRotation);
		Direction = ViewRotation.Vector();
	}
	else
	{
		Origin = GetMuzzle();
		Direction = GetOwner()->GetActorForwardVector();
	}
	Fire(Origin, Direction, 1.f);
}

void UGTPWeaponComponent::ReleaseTrigger()
{
	GTPWeapon::ReleaseTrigger(State);
}

bool UGTPWeaponComponent::FireAt(const FVector& Target, float SpreadFactor)
{
	ApplySpec();
	if (!bDrawn)
	{
		SetDrawn(true);
	}
	GTPWeapon::ReleaseTrigger(State); // AI: each call is a new press
	const FVector Origin = GetMuzzle();
	return Fire(Origin, (Target - Origin).GetSafeNormal(), FMath::Max(0.1f, SpreadFactor));
}

void UGTPWeaponComponent::Reload()
{
	ApplySpec();
	if (bDrawn && GTPWeapon::StartReload(State, Spec))
	{
		PlayClip(TEXT("AM_Pistol_Reload"));
		Broadcast(EGTPWeaponEvent::ReloadStarted);
		bWasReloading = true;
	}
}

void UGTPWeaponComponent::CancelReload()
{
	if (State.IsReloading())
	{
		GTPWeapon::CancelReload(State);
		bWasReloading = false;
		Broadcast(EGTPWeaponEvent::ReloadCancelled);
	}
}

float UGTPWeaponComponent::GetSpeedShare() const
{
	const AActor* Owner = GetOwner();
	return Owner && FullSpeedCmS > 0.f ? FMath::Clamp(static_cast<float>(Owner->GetVelocity().Size2D()) / FullSpeedCmS, 0.f, 1.f) : 0.f;
}

float UGTPWeaponComponent::GetSpreadDegrees() const
{
	return bDrawn ? GTPWeapon::SpreadDeg(State, Spec, bAiming, GetSpeedShare()) : 0.f;
}

FVector UGTPWeaponComponent::GetMuzzle() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		const USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (Mesh && Mesh->DoesSocketExist(MuzzleSocket))
		{
			return Mesh->GetSocketLocation(MuzzleSocket);
		}
		return Character->GetActorLocation() + FVector(0.f, 0.f, Character->BaseEyeHeight * 0.8f);
	}
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

bool UGTPWeaponComponent::Fire(const FVector& ViewOrigin, const FVector& ViewDirection, float SpreadFactor)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner)
	{
		return false;
	}
	GTPWeapon::FShot Shot;
	GTPWeapon::FSpec Effective = Spec;
	Effective.HipSpreadDeg *= SpreadFactor;
	Effective.AimSpreadDeg *= SpreadFactor;
	const bool bWasReloadingBefore = State.IsReloading();
	const GTPWeapon::EFire Result = GTPWeapon::PullTrigger(State, Effective, bAiming, GetSpeedShare(), false, Rng, Shot);
	if (Result == GTPWeapon::EFire::Empty)
	{
		Broadcast(EGTPWeaponEvent::DryFire);
		return false;
	}
	if (Result == GTPWeapon::EFire::Reloading && !bWasReloadingBefore)
	{
		// The empty magazine started a reload by itself.
		PlayClip(TEXT("AM_Pistol_Reload"));
		Broadcast(EGTPWeaponEvent::ReloadStarted);
		bWasReloading = true;
	}
	if (Result != GTPWeapon::EFire::Fired)
	{
		return false;
	}

	// Deviate the view direction inside the cone.
	const FRotator Base = ViewDirection.Rotation();
	const FVector Dir = (Base + FRotator(Shot.PitchDeg, Shot.YawDeg, 0.f)).Vector();
	const float RangeCm = Spec.RangeM * 100.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPWeaponShot), false, Owner);
	Params.bReturnPhysicalMaterial = true;
	// 1. From the view: what the player points at (skip the first metre: the own shoulder).
	FHitResult ViewHit;
	const FVector ViewStart = ViewOrigin + Dir * 50.f;
	FVector Target = ViewOrigin + Dir * RangeCm;
	if (World->LineTraceSingleByChannel(ViewHit, ViewStart, Target, ECC_Visibility, Params))
	{
		Target = ViewHit.ImpactPoint;
	}
	// 2. From the hand to that point: walls between the gun and the target stop the bullet.
	const FVector Muzzle = GetMuzzle();
	const FVector BulletDir = (Target - Muzzle).GetSafeNormal();
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Muzzle, Muzzle + BulletDir * RangeCm, ECC_Visibility, Params);

	UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this);
	if (Combat)
	{
		Combat->NotifyShot(Owner, Muzzle, BulletDir, WeaponKey, Hit, bHit);
	}
	PlayClip(TEXT("AM_Pistol_Fire"));
	Broadcast(EGTPWeaponEvent::Fired);

	// Recoil on the player's view.
	if (bPlayerControlled)
	{
		if (APlayerController* PC = Cast<APlayerController>(Cast<APawn>(Owner) ? Cast<APawn>(Owner)->GetController() : nullptr))
		{
			FRotator Control = PC->GetControlRotation();
			Control.Pitch += Shot.KickPitchDeg;
			Control.Yaw += Shot.KickYawDeg;
			PC->SetControlRotation(Control);
		}
	}

	if (bHit && Combat)
	{
		AActor* HitActor = Hit.GetActor();
		// A body: a character (player, police, staff) or a pooled pedestrian.
		const bool bBody = Cast<ACharacter>(HitActor) != nullptr || Cast<AGTPPedestrian>(HitActor) != nullptr;
		if (bBody)
		{
			FGTPCombatHit CombatHit;
			CombatHit.Instigator = Owner;
			CombatHit.Victim = HitActor;
			CombatHit.Bone = Hit.BoneName;
			CombatHit.Location = Hit.ImpactPoint;
			CombatHit.Direction = BulletDir;
			CombatHit.Damage = Spec.Damage;
			CombatHit.DistanceM = static_cast<float>(FVector::Dist(Muzzle, Hit.ImpactPoint)) * 0.01f;
			Combat->ApplyHit(CombatHit);
		}
		else
		{
			Combat->SpawnImpact(Hit, BulletDir);
		}
	}
	return true;
}

void UGTPWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const float Back = GTPWeapon::Tick(State, Spec, DeltaTime);
	if (bWasReloading && !State.IsReloading())
	{
		bWasReloading = false;
		Broadcast(EGTPWeaponEvent::ReloadFinished);
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (bPlayerControlled && PC && Back > 0.f)
	{
		FRotator Control = PC->GetControlRotation();
		Control.Pitch -= Back;
		PC->SetControlRotation(Control);
	}

	if (IsAiming() && Pawn)
	{
		// Face where the view looks while aiming.
		const float ControlYaw = static_cast<float>(Pawn->GetControlRotation().Yaw);
		FRotator Facing = Pawn->GetActorRotation();
		Facing.Yaw = FMath::FixedTurn(static_cast<float>(Facing.Yaw), ControlYaw, 720.f * DeltaTime);
		Pawn->SetActorRotation(Facing);
		if (UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this))
		{
			Combat->NotifyAim(Pawn, GetMuzzle(), Pawn->GetControlRotation().Vector(), true);
		}
	}
}

void UGTPWeaponComponent::PlayClip(FName Name)
{
	const TSoftObjectPtr<UAnimMontage>* Clip = Clips.Find(Name);
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Clip || Clip->IsNull() || !Character || !Character->GetMesh())
	{
		return;
	}
	if (UAnimMontage* Montage = Clip->LoadSynchronous())
	{
		if (UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance())
		{
			Anim->Montage_Play(Montage);
		}
	}
}
