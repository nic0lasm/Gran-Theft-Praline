#include "GTPWeapon.h"

#include "GTPCombat.h"
#include "GTPPedestrians.h"
#include "GTPVehicle.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendProfile.h"
#include "Animation/Skeleton.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"
#include "GTPLog.h"

namespace
{
	/** Loads an asset once per run; a missing one is reported once and never tried again. */
	UObject* WeaponLoadOnce(const FSoftObjectPath& Path)
	{
		static TSet<FSoftObjectPath> Missing;
		if (Path.IsNull() || Missing.Contains(Path))
		{
			return nullptr;
		}
		UObject* Object = Path.ResolveObject();
		if (!Object)
		{
			Object = Path.TryLoad();
		}
		if (!Object)
		{
			Missing.Add(Path);
			UE_LOG(LogGTPCombat, Log, TEXT("weapon: %s not in Content, played without it"), *Path.ToString());
		}
		return Object;
	}

	FGTPWeaponLook MakeWeaponLook(const TCHAR* Prefix, const TCHAR* Flash)
	{
		FGTPWeaponLook Look;
		Look.ClipPrefix = Prefix;
		Look.MuzzleFlash = FSoftObjectPath(Flash);
		return Look;
	}
}

UGTPWeaponComponent::UGTPWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Lyra's rifle flash serves both until a pistol flash exists (assets out of Git: missing = no flash).
	const TCHAR* Flash = TEXT("/Game/Effects/Particles/Weapons/NS_WeaponFire_MuzzleFlash_Rifle.NS_WeaponFire_MuzzleFlash_Rifle");
	Looks = {MakeWeaponLook(TEXT("AM_Pistol"), Flash), MakeWeaponLook(TEXT("AM_Carbine"), Flash)};
	// A pistol until BeginPlay builds the real loadout, so every getter is safe before it.
	Loadout = GTPArsenal::FLoadout::Make({GTPArsenal::EKind::Pistol}, {0});
}

UGTPWeaponComponent* UGTPWeaponComponent::FindHeld(const APawn* Pawn)
{
	if (const AGTPVehicle* Car = Cast<AGTPVehicle>(Pawn))
	{
		const ACharacter* Driver = Car->GetDriver();
		return Driver ? Driver->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
	}
	return Pawn ? Pawn->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
}

void UGTPWeaponComponent::BuildLoadout()
{
	if (bLoadoutBuilt)
	{
		return;
	}
	bLoadoutBuilt = true;
	std::vector<GTPArsenal::EKind> Kinds = {GTPArsenal::EKind::Pistol};
	std::vector<int> Reserves = {bPlayerControlled ? PlayerPistolReserve : NpcPistolReserve};
	if (bPlayerControlled && PlayerCarbineReserve > 0)
	{
		Kinds.push_back(GTPArsenal::EKind::Carbine);
		Reserves.push_back(PlayerCarbineReserve);
	}
	Loadout = GTPArsenal::FLoadout::Make(Kinds, Reserves);
	// Seeded from the owner's name so a deterministic demo replays the same shots.
	Rng = GTPWeapon::FRandom(static_cast<uint32>(GetTypeHash(GetOwner() ? GetOwner()->GetName() : FString())) | 1u);
}

void UGTPWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	BuildLoadout();
	// Loaded here, not at the first shot: no hitch when the fight starts.
	LoadLooks();
}

void UGTPWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GunMesh)
	{
		GunMesh->DestroyComponent();
		GunMesh = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UGTPWeaponComponent::LoadLooks()
{
	if (bLooksLoaded)
	{
		return;
	}
	bLooksLoaded = true;
	const int32 Count = static_cast<int32>(GTPArsenal::EKind::Count);
	LoadedMeshes.SetNum(Count);
	LoadedFlashes.SetNum(Count);
	for (int32 K = 0; K < Count && K < Looks.Num(); ++K)
	{
		if (!Loadout.Has(static_cast<GTPArsenal::EKind>(K)))
		{
			continue;
		}
		LoadedMeshes[K] = Cast<USkeletalMesh>(WeaponLoadOnce(Looks[K].Mesh));
		LoadedFlashes[K] = Cast<UNiagaraSystem>(WeaponLoadOnce(Looks[K].MuzzleFlash));
		if (bPlayerControlled && LoadedFlashes[K] && GetWorld())
		{
			// One hidden spawn far below the street: shaders and pipeline states ready before the first shot.
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedFlashes[K], FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator,
				FVector::OneVector, true, true, ENCPoolMethod::AutoRelease, false);
		}
	}
	for (const FGTPWeaponClip& Clip : Clips)
	{
		if (UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(WeaponLoadOnce(Clip.Asset)))
		{
			LoadedClips.Add(Clip.Name, Anim);
		}
	}
}

const FGTPWeaponLook* UGTPWeaponComponent::GetLook() const
{
	const int32 K = static_cast<int32>(Loadout.CurrentKind());
	return Looks.IsValidIndex(K) ? &Looks[K] : nullptr;
}

FName UGTPWeaponComponent::GetWeaponKey() const
{
	const int32 K = static_cast<int32>(Loadout.CurrentKind());
	return WeaponKeys.IsValidIndex(K) ? WeaponKeys[K] : NAME_None;
}

APlayerController* UGTPWeaponComponent::GetPlayerController() const
{
	// In a car the driver is not possessed: the car's controller aims for it.
	const APawn* Pawn = Cast<APawn>(DriveByCar.IsValid() ? DriveByCar.Get() : GetOwner());
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

FRotator UGTPWeaponComponent::GetAimRotation() const
{
	if (const APlayerController* PC = GetPlayerController())
	{
		return PC->GetControlRotation();
	}
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn ? Pawn->GetControlRotation() : FRotator::ZeroRotator;
}

void UGTPWeaponComponent::UpdateMesh()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	const int32 K = static_cast<int32>(Loadout.CurrentKind());
	USkeletalMesh* Mesh = LoadedMeshes.IsValidIndex(K) ? LoadedMeshes[K].Get() : nullptr;
	if (!Character || !Character->GetMesh() || (!Mesh && !GunMesh))
	{
		return;
	}
	if (!GunMesh)
	{
		GunMesh = NewObject<USkeletalMeshComponent>(Character, TEXT("GTPGunMesh"));
		GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GunMesh->SetGenerateOverlapEvents(false);
		GunMesh->bReceivesDecals = false;
		GunMesh->RegisterComponent();
	}
	PlaceGun();
	if (Mesh && GunMesh->GetSkeletalMeshAsset() != Mesh)
	{
		GunMesh->SetSkeletalMeshAsset(Mesh);
	}
	// GTA-like: the gun is in the hand only while drawn.
	GunMesh->SetVisibility(bDrawn && Mesh != nullptr && !Loadout.IsSwitching());
}

void UGTPWeaponComponent::SetDrawn(bool bDraw)
{
	BuildLoadout();
	if (bDraw == bDrawn)
	{
		return;
	}
	bDrawn = bDraw;
	SetComponentTickEnabled(bDrawn);
	if (!bDrawn)
	{
		SetAiming(false);
		GTPWeapon::ReleaseTrigger(Loadout.State());
		CancelReload();
	}
	UpdateMesh();
	PlayClip(bDrawn ? TEXT("_Draw") : TEXT("_Holster"));
	Broadcast(bDrawn ? EGTPWeaponEvent::Drawn : EGTPWeaponEvent::Holstered);
}

void UGTPWeaponComponent::SetAiming(bool bAim)
{
	const bool bWas = IsAiming();
	bAiming = bAim;
	if (bWas != IsAiming())
	{
		SetAimLoop(IsAiming());
	}
	if (bWas && !IsAiming())
	{
		if (UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this))
		{
			Combat->NotifyAim(GetShooter(), FVector::ZeroVector, FVector::ForwardVector, false);
		}
	}
	// Facing the aim is done in Tick (the owner turns to the control yaw).
}

bool UGTPWeaponComponent::SelectKind(GTPArsenal::EKind Kind)
{
	const bool bWasAiming = IsAiming();
	if (!Loadout.Select(Kind, bDrawn ? SwitchSeconds : 0.f))
	{
		return false;
	}
	if (bWasAiming)
	{
		SetAimLoop(false); // the new weapon's aim pose
		SetAimLoop(true);
	}
	UpdateMesh();
	Broadcast(EGTPWeaponEvent::Switched);
	return true;
}

void UGTPWeaponComponent::CycleWeapon(int32 Step)
{
	BuildLoadout();
	const GTPArsenal::EKind Next = Loadout.Next(Step < 0 ? -1 : 1);
	if (IsDriveBy() && !Loadout.Specs[static_cast<size_t>(Next)].bOneHanded)
	{
		return; // two hands on a long weapon and one on the wheel: no
	}
	const bool bWasDrawn = bDrawn;
	SelectKind(Next);
	if (!bWasDrawn)
	{
		SetDrawn(true);
	}
	else
	{
		SetComponentTickEnabled(true);
		PlayClip(TEXT("_Draw"));
	}
}

void UGTPWeaponComponent::GiveWeapon(GTPArsenal::EKind Kind, int32 Reserve)
{
	BuildLoadout();
	if (Loadout.Give(Kind, Reserve))
	{
		bLooksLoaded = false;
		LoadLooks();
	}
}

USkeletalMesh* UGTPWeaponComponent::GetMeshFor(GTPArsenal::EKind Kind) const
{
	const int32 K = static_cast<int32>(Kind);
	return LoadedMeshes.IsValidIndex(K) ? LoadedMeshes[K].Get() : nullptr;
}

int32 UGTPWeaponComponent::CollectAmmo(GTPArsenal::EKind Kind, int32 Rounds)
{
	BuildLoadout();
	const bool bNew = !Loadout.Has(Kind);
	const int32 Taken = Loadout.Collect(Kind, Rounds);
	if (bNew && Taken > 0)
	{
		bLooksLoaded = false;
		LoadLooks(); // the new gun's mesh, flash and clips
	}
	return Taken;
}

void UGTPWeaponComponent::SetDriveBy(AActor* Car)
{
	BuildLoadout();
	if (Car == DriveByCar.Get())
	{
		return;
	}
	SetDrawn(false);
	if (Car)
	{
		OnFootKind = Loadout.CurrentKind();
		const GTPArsenal::EKind OneHanded = Loadout.FirstOneHanded();
		if (OneHanded != GTPArsenal::EKind::Count)
		{
			Loadout.Select(OneHanded, 0.f);
		}
		DriveByCar = Car;
	}
	else
	{
		DriveByCar.Reset();
		Loadout.Select(OnFootKind, 0.f);
	}
	UpdateMesh();
}

bool UGTPWeaponComponent::TryFireFromView()
{
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = GetPlayerController();
	if (!Controller && Pawn)
	{
		Controller = Pawn->GetController();
	}
	if (Controller)
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
	return Fire(Origin, Direction, 1.f);
}

void UGTPWeaponComponent::PullTrigger()
{
	BuildLoadout();
	if (!bDrawn)
	{
		return;
	}
	TryFireFromView();
}

void UGTPWeaponComponent::ReleaseTrigger()
{
	GTPWeapon::ReleaseTrigger(Loadout.State());
}

bool UGTPWeaponComponent::FireAt(const FVector& Target, float SpreadFactor)
{
	BuildLoadout();
	if (!bDrawn)
	{
		SetDrawn(true);
	}
	GTPWeapon::ReleaseTrigger(Loadout.State()); // AI: each call is a new press
	const FVector Origin = GetMuzzle();
	const bool bFired = Fire(Origin, (Target - Origin).GetSafeNormal(), FMath::Max(0.1f, SpreadFactor));
	GTPWeapon::ReleaseTrigger(Loadout.State());
	return bFired;
}

void UGTPWeaponComponent::Reload()
{
	BuildLoadout();
	if (bDrawn && !Loadout.IsSwitching() && GTPWeapon::StartReload(Loadout.State(), Loadout.Spec()))
	{
		PlayClip(TEXT("_Reload"));
		Broadcast(EGTPWeaponEvent::ReloadStarted);
		bWasReloading = true;
	}
}

void UGTPWeaponComponent::CancelReload()
{
	if (Loadout.State().IsReloading())
	{
		GTPWeapon::CancelReload(Loadout.State());
		bWasReloading = false;
		Broadcast(EGTPWeaponEvent::ReloadCancelled);
	}
}

float UGTPWeaponComponent::GetSpeedShare() const
{
	const AActor* Owner = GetOwner();
	if (IsDriveBy())
	{
		return 0.f; // the car's speed counts through GTPDriveBy instead
	}
	return Owner && FullSpeedCmS > 0.f ? FMath::Clamp(static_cast<float>(Owner->GetVelocity().Size2D()) / FullSpeedCmS, 0.f, 1.f) : 0.f;
}

float UGTPWeaponComponent::GetSpreadDegrees() const
{
	if (!bDrawn)
	{
		return 0.f;
	}
	const float Base = GTPWeapon::SpreadDeg(Loadout.State(), Loadout.Spec(), bAiming, GetSpeedShare());
	if (const AActor* Car = DriveByCar.Get())
	{
		const GTPDriveBy::FSettings DriveBy;
		return Base * DriveBy.SpreadFactor + GTPDriveBy::SpeedSpreadDeg(static_cast<float>(Car->GetVelocity().Size()) * 0.01f, DriveBy);
	}
	return Base;
}

USkeletalMeshComponent* UGTPWeaponComponent::FindVisibleBody() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const USkeletalMeshComponent* Own = Character ? Character->GetMesh() : nullptr;
	if (!Own || Own->IsVisible())
	{
		return nullptr; // the pawn's own mesh shows: the gun stays on it
	}
	// The player visual's body hangs from the pawn's hidden mesh (GTPGameMode::ApplyPlayerVisual).
	static const FName Hand(TEXT("hand_r"));
	TArray<USceneComponent*> Children;
	Own->GetChildrenComponents(false, Children);
	for (USceneComponent* Child : Children)
	{
		USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Child);
		if (Skel && Skel != GunMesh && Skel->IsVisible() && !Skel->LeaderPoseComponent.IsValid() && Skel->GetBoneIndex(Hand) != INDEX_NONE)
		{
			return Skel;
		}
	}
	return nullptr;
}

void UGTPWeaponComponent::PlaceGun()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Own = Character ? Character->GetMesh() : nullptr;
	if (!GunMesh || !Own)
	{
		return;
	}
	const FName Socket = Own->DoesSocketExist(HandSocket) ? HandSocket : HandFallbackSocket;
	const FGTPWeaponLook* Look = GetLook();
	const FTransform Offset = Look ? Look->AttachOffset : FTransform::Identity;
	static const FName Hand(TEXT("hand_r"));
	USkeletalMeshComponent* Visible = FindVisibleBody();
	if (!Visible || Own->GetBoneIndex(Hand) == INDEX_NONE)
	{
		if (GunMesh->GetAttachParent() != Own || GunMesh->GetAttachSocketName() != Socket)
		{
			GunMesh->AttachToComponent(Own, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		}
		GunMesh->SetRelativeTransform(Offset);
		return;
	}
	// In the visible body's hand: the grip of the pawn's weapon_r (right for its hand whatever the body's bone
	// axes), moved by the gap between the two hands (the body is retargeted, its proportions differ).
	FTransform World = Offset * Own->GetSocketTransform(Socket);
	World.AddToTranslation(Visible->GetSocketLocation(Hand) - Own->GetSocketLocation(Hand));
	World.SetScale3D(FVector::OneVector);
	if (GunMesh->GetAttachParent() != Visible || GunMesh->GetAttachSocketName() != Hand)
	{
		GunMesh->AttachToComponent(Visible, FAttachmentTransformRules::KeepWorldTransform, Hand);
	}
	GunMesh->SetWorldTransform(World);
}

FVector UGTPWeaponComponent::GetMuzzle() const
{
	if (const FGTPWeaponLook* Look = GetLook())
	{
		if (GunMesh && GunMesh->IsVisible() && GunMesh->DoesSocketExist(Look->MuzzleSocket))
		{
			return GunMesh->GetSocketLocation(Look->MuzzleSocket);
		}
	}
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		const USkeletalMeshComponent* Mesh = Character->GetMesh();
		if (Mesh && Mesh->DoesSocketExist(HandFallbackSocket))
		{
			return Mesh->GetSocketLocation(HandFallbackSocket);
		}
		return Character->GetActorLocation() + FVector(0.f, 0.f, Character->BaseEyeHeight * 0.8f);
	}
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UGTPWeaponComponent::SpawnFlash()
{
	const int32 K = static_cast<int32>(Loadout.CurrentKind());
	UNiagaraSystem* Flash = LoadedFlashes.IsValidIndex(K) ? LoadedFlashes[K].Get() : nullptr;
	if (!Flash)
	{
		return;
	}
	const FGTPWeaponLook* Look = GetLook();
	if (GunMesh && GunMesh->IsVisible() && Look && GunMesh->DoesSocketExist(Look->MuzzleSocket))
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(Flash, GunMesh, Look->MuzzleSocket, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true, true, ENCPoolMethod::AutoRelease);
		return;
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Flash, GetMuzzle(), GetAimRotation(), FVector::OneVector, true, true,
		ENCPoolMethod::AutoRelease);
}

bool UGTPWeaponComponent::Fire(const FVector& ViewOrigin, const FVector& ViewDirection, float SpreadFactor)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner || Loadout.IsSwitching())
	{
		return false;
	}
	AActor* Car = DriveByCar.Get();
	GTPWeapon::FSpec Effective = Loadout.Spec();
	Effective.HipSpreadDeg *= SpreadFactor;
	Effective.AimSpreadDeg *= SpreadFactor;
	if (Car)
	{
		// Out of a window only, wider, and wider still at speed.
		const GTPDriveBy::FSettings DriveBy;
		const FRotator Relative = (ViewDirection.Rotation() - Car->GetActorRotation()).GetNormalized();
		if (!GTPDriveBy::CanShoot(static_cast<float>(Relative.Yaw), static_cast<float>(Relative.Pitch), DriveBy))
		{
			return false;
		}
		const float Extra = GTPDriveBy::SpeedSpreadDeg(static_cast<float>(Car->GetVelocity().Size()) * 0.01f, DriveBy);
		Effective.HipSpreadDeg = Effective.HipSpreadDeg * DriveBy.SpreadFactor + Extra;
		Effective.AimSpreadDeg = Effective.AimSpreadDeg * DriveBy.SpreadFactor + Extra;
	}
	GTPWeapon::FShot Shot;
	GTPWeapon::FState& State = Loadout.State();
	const bool bWasReloadingBefore = State.IsReloading();
	const GTPWeapon::EFire Result = GTPWeapon::PullTrigger(State, Effective, bAiming, GetSpeedShare(), false, Rng, Shot);
	if (Result == GTPWeapon::EFire::Empty)
	{
		PlayClip(TEXT("_DryFire"));
		Broadcast(EGTPWeaponEvent::DryFire);
		return false;
	}
	if (Result == GTPWeapon::EFire::Reloading && !bWasReloadingBefore)
	{
		// The empty magazine started a reload by itself.
		PlayClip(TEXT("_Reload"));
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
	const float RangeCm = Loadout.Spec().RangeM * 100.f;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPWeaponShot), false, Owner);
	Params.bReturnPhysicalMaterial = true;
	if (Car)
	{
		Params.AddIgnoredActor(Car); // never the own car, glass included
	}
	// 1. From the view: what the player points at (skip the first half metre: the own shoulder).
	FHitResult ViewHit;
	const FVector ViewStart = ViewOrigin + Dir * 50.f;
	FVector Target = ViewOrigin + Dir * RangeCm;
	if (World->LineTraceSingleByChannel(ViewHit, ViewStart, Target, ECC_Visibility, Params))
	{
		Target = ViewHit.ImpactPoint;
	}
	// 2. From the muzzle to that point: walls between the gun and the target stop the bullet.
	const FVector Muzzle = GetMuzzle();
	const FVector BulletDir = (Target - Muzzle).GetSafeNormal();
	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Muzzle, Muzzle + BulletDir * RangeCm, ECC_Visibility, Params);

	UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this);
	if (Combat)
	{
		Combat->NotifyShot(GetShooter(), Muzzle, BulletDir, GetWeaponKey(), Hit, bHit);
	}
	SpawnFlash();
	PlayClip(TEXT("_Fire"));
	Broadcast(EGTPWeaponEvent::Fired);

	// Recoil on the player's view.
	if (bPlayerControlled)
	{
		if (APlayerController* PC = GetPlayerController())
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
			CombatHit.Instigator = GetShooter();
			CombatHit.Victim = HitActor;
			CombatHit.Bone = Hit.BoneName;
			CombatHit.Location = Hit.ImpactPoint;
			CombatHit.Direction = BulletDir;
			CombatHit.Damage = Loadout.Spec().Damage;
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
	if (Loadout.TickSwitch(DeltaTime))
	{
		UpdateMesh(); // the new gun appears in the hand
	}
	if (bDrawn && GunMesh && GunMesh->IsVisible())
	{
		PlaceGun(); // follows the visible body's hand (the gap between the two hands moves with the pose)
	}
	const float Back = GTPWeapon::Tick(Loadout.State(), Loadout.Spec(), DeltaTime);
	if (bWasReloading && !Loadout.State().IsReloading())
	{
		bWasReloading = false;
		Broadcast(EGTPWeaponEvent::ReloadFinished);
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* PC = bPlayerControlled ? GetPlayerController() : nullptr;
	if (PC && Back > 0.f)
	{
		FRotator Control = PC->GetControlRotation();
		Control.Pitch -= Back;
		PC->SetControlRotation(Control);
	}

	// Automatic weapon: fires for as long as the player holds the trigger.
	if (bPlayerControlled && bDrawn && Loadout.Spec().bAutomatic && Loadout.State().bTriggerHeld)
	{
		TryFireFromView();
	}

	if (IsAiming() && Pawn)
	{
		const FRotator Aim = GetAimRotation();
		if (!IsDriveBy())
		{
			// Face where the view looks while aiming (seated: the body stays in the seat).
			FRotator Facing = Pawn->GetActorRotation();
			Facing.Yaw = FMath::FixedTurn(static_cast<float>(Facing.Yaw), static_cast<float>(Aim.Yaw), 720.f * DeltaTime);
			Pawn->SetActorRotation(Facing);
		}
		if (UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this))
		{
			Combat->NotifyAim(GetShooter(), GetMuzzle(), Aim.Vector(), true);
		}
	}
}

const FGTPWeaponClip* UGTPWeaponComponent::FindClip(const TCHAR* Suffix) const
{
	const FGTPWeaponLook* Look = GetLook();
	if (!Look || Look->ClipPrefix.IsEmpty())
	{
		return nullptr;
	}
	const FName Name(*(Look->ClipPrefix + Suffix));
	return Clips.FindByPredicate([Name](const FGTPWeaponClip& Clip) { return Clip.Name == Name; });
}

namespace
{
	/** The sequence a clip plays in an explicit slot: the clip itself, or a montage's first segment. */
	UAnimSequenceBase* WeaponClipSequence(UAnimSequenceBase* Clip)
	{
		if (UAnimMontage* Montage = Cast<UAnimMontage>(Clip))
		{
			if (Montage->SlotAnimTracks.Num() > 0 && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0)
			{
				return Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference().Get();
			}
			return nullptr;
		}
		return Clip;
	}
}

void UGTPWeaponComponent::PlayClip(const TCHAR* Suffix)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const FGTPWeaponClip* Entry = FindClip(Suffix);
	if (!Entry || !Character || !Character->GetMesh())
	{
		return;
	}
	const TObjectPtr<UAnimSequenceBase>* Found = LoadedClips.Find(Entry->Name);
	UAnimSequenceBase* Clip = Found ? Found->Get() : nullptr;
	UAnimInstance* Anim = Character->GetMesh()->GetAnimInstance();
	if (!Clip || !Anim)
	{
		return;
	}
	UAnimMontage* Montage = Cast<UAnimMontage>(Clip);
	if (Montage && Entry->Slot.IsNone())
	{
		Anim->Montage_Play(Montage);
		return;
	}
	const FName Slot = Entry->Slot.IsNone() ? AnimSlot : Entry->Slot;
	UAnimSequenceBase* Sequence = WeaponClipSequence(Clip);
	if (!Sequence || (Sequence->IsValidAdditive() && !Slot.ToString().Contains(TEXT("Additive"))))
	{
		return; // an additive pose in a full-pose slot would twist the body
	}
	PlayInSlot(Sequence, Slot, 0.08f, 0.12f, 1);
}

UBlendProfile* UGTPWeaponComponent::GetUpperBodyMask()
{
	if (bUpperBodyMaskTried)
	{
		return UpperBodyMask;
	}
	bUpperBodyMaskTried = true;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	USkeletalMesh* BodyMesh = Character && Character->GetMesh() ? Character->GetMesh()->GetSkeletalMeshAsset() : nullptr;
	USkeleton* Skeleton = BodyMesh ? BodyMesh->GetSkeleton() : nullptr;
	if (UpperBodyRoot.IsNone() || !Skeleton || Skeleton->GetReferenceSkeleton().FindBoneIndex(UpperBodyRoot) == INDEX_NONE)
	{
		GTP_TRACE(LogGTPCombat, TEXT("weapon: no upper-body mask (root %s), clips play on the full body"), *UpperBodyRoot.ToString());
		return nullptr;
	}
	// A blend mask: per-bone weight = scale x montage weight; the bones left out weigh 0 (the legs).
	// OwningSkeleton and Mode are set through reflection: the editor normally sets them.
	UBlendProfile* Mask = NewObject<UBlendProfile>(Skeleton, NAME_None, RF_Transient);
	bool bSkeletonSet = false;
	bool bModeSet = false;
	if (FObjectPropertyBase* SkeletonProp = FindFProperty<FObjectPropertyBase>(UBlendProfile::StaticClass(), TEXT("OwningSkeleton")))
	{
		SkeletonProp->SetObjectPropertyValue_InContainer(Mask, Skeleton);
		bSkeletonSet = true;
	}
	const int64 MaskMode = static_cast<int64>(EBlendProfileMode::BlendMask);
	if (FEnumProperty* ModeEnum = FindFProperty<FEnumProperty>(UBlendProfile::StaticClass(), TEXT("Mode")))
	{
		ModeEnum->GetUnderlyingProperty()->SetIntPropertyValue(ModeEnum->ContainerPtrToValuePtr<void>(Mask), MaskMode);
		bModeSet = true;
	}
	else if (FByteProperty* ModeByte = FindFProperty<FByteProperty>(UBlendProfile::StaticClass(), TEXT("Mode")))
	{
		ModeByte->SetIntPropertyValue(ModeByte->ContainerPtrToValuePtr<void>(Mask), MaskMode);
		bModeSet = true;
	}
	if (!bSkeletonSet || !bModeSet)
	{
		GTP_TRACE(LogGTPCombat, TEXT("weapon: blend mask not settable (skeleton %d, mode %d), clips play on the full body"), bSkeletonSet ? 1 : 0, bModeSet ? 1 : 0);
		return nullptr;
	}
	Mask->SetBoneBlendScale(UpperBodyRoot, 1.f, true, true);
	UpperBodyMask = Mask;
	GTP_TRACE(LogGTPCombat, TEXT("weapon: upper-body mask from %s (%d bones)"), *UpperBodyRoot.ToString(), Mask->GetNumBlendEntries());
	return Mask;
}

void UGTPWeaponComponent::PlayInSlot(UAnimSequenceBase* Sequence, FName Slot, float BlendIn, float BlendOut, int32 LoopCount)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim || !Sequence)
	{
		return;
	}
	UBlendProfile* Mask = GetUpperBodyMask();
	if (!Mask)
	{
		Anim->PlaySlotAnimationAsDynamicMontage(Sequence, Slot, BlendIn, BlendOut, 1.f, LoopCount);
		return;
	}
	FMontageBlendSettings In;
	In.Blend.BlendTime = BlendIn;
	In.BlendProfile = Mask;
	FMontageBlendSettings Out;
	Out.Blend.BlendTime = BlendOut;
	Out.BlendProfile = Mask;
	Anim->PlaySlotAnimationAsDynamicMontage_WithBlendSettings(Sequence, Slot, In, Out, 1.f, LoopCount);
}

void UGTPWeaponComponent::SetAimLoop(bool bOn)
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Anim = Character && Character->GetMesh() ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Anim)
	{
		return;
	}
	if (!bOn)
	{
		if (!AimLoopSlot.IsNone())
		{
			Anim->StopSlotAnimation(0.15f, AimLoopSlot);
			AimLoopSlot = NAME_None;
		}
		return;
	}
	const FGTPWeaponClip* Entry = FindClip(TEXT("_AimLoop"));
	const TObjectPtr<UAnimSequenceBase>* Found = Entry ? LoadedClips.Find(Entry->Name) : nullptr;
	UAnimSequenceBase* Sequence = Found ? WeaponClipSequence(Found->Get()) : nullptr;
	const FName Slot = Entry && !Entry->Slot.IsNone() ? Entry->Slot : AnimSlot;
	if (!Sequence || IsDriveBy() || (Sequence->IsValidAdditive() && !Slot.ToString().Contains(TEXT("Additive"))))
	{
		return;
	}
	// Held as long as the aim lasts (a large loop count), stopped by SetAimLoop(false).
	PlayInSlot(Sequence, Slot, 0.12f, 0.15f, 100000);
	AimLoopSlot = Slot;
}
