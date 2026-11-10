#include "GTPRagdoll.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GTPGraphicsProfile.h"
#include "TimerManager.h"

namespace
{
GTPImpact::ESeverity ToLogic(EGTPImpactSeverity S)
{
	return static_cast<GTPImpact::ESeverity>(static_cast<int32>(S));
}
}

UGTPRagdollComponent::UGTPRagdollComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

USkeletalMeshComponent* UGTPRagdollComponent::FindMesh() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->GetMesh();
	}
	return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

bool UGTPRagdollComponent::BeginRagdoll(FVector LaunchVelocity, EGTPImpactSeverity Severity, float HealthAfterHit, float SpinRadS)
{
	if (bActive)
	{
		return false;
	}
	Mesh = FindMesh();
	if (!Mesh || !Mesh->GetPhysicsAsset())
	{
		return false;
	}
	if (const UWorld* World = GetWorld())
	{
		const UGTPRagdollSubsystem* Subsystem = World->GetSubsystem<UGTPRagdollSubsystem>();
		if (Subsystem && !Subsystem->HasRoom())
		{
			return false;
		}
	}

	State = GTPImpact::StartRagdollState(ToLogic(Severity), HealthAfterHit);
	bActive = true;

	MeshParent = Mesh->GetAttachParent();
	MeshParentSocket = Mesh->GetAttachSocketName();
	MeshRelative = Mesh->GetRelativeTransform();
	MeshCollision = Mesh->GetCollisionEnabled();
	MeshProfile = Mesh->GetCollisionProfileName();

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		CapsuleHalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->GetCharacterMovement()->DisableMovement();
		Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(RagdollProfile);
	Mesh->SetAllBodiesSimulatePhysics(true);
	Mesh->SetSimulatePhysics(true);
	Mesh->WakeAllRigidBodies();
	Mesh->AddImpulse(LaunchVelocity, PelvisBone, true);
	if (SpinRadS > 0.f)
	{
		// Spin around the axis perpendicular to the launch, so the body tumbles forward.
		const FVector Axis = FVector::CrossProduct(FVector::UpVector, LaunchVelocity.GetSafeNormal());
		Mesh->SetPhysicsAngularVelocityInRadians(Axis * SpinRadS, false, PelvisBone);
	}

	SetComponentTickEnabled(true);
	if (UWorld* World = GetWorld())
	{
		if (UGTPRagdollSubsystem* Subsystem = World->GetSubsystem<UGTPRagdollSubsystem>())
		{
			Subsystem->Register(this);
		}
	}
	return true;
}

void UGTPRagdollComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bActive || !Mesh)
	{
		return;
	}

	const float Speed = Mesh->GetPhysicsLinearVelocity(PelvisBone).Size();
	const FTransform Pelvis = Mesh->GetSocketTransform(PelvisBone);
	const float UpDot = static_cast<float>(FVector::DotProduct(Pelvis.TransformVectorNoScale(PelvisFrontAxis), FVector::UpVector));

	// The capsule follows the body so the camera and the world logic see it.
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCapsuleComponent()->SetWorldLocation(Pelvis.GetLocation() + FVector(0.f, 0.f, CapsuleHalfHeight * 0.3f), false, nullptr, ETeleportType::TeleportPhysics);
	}

	const GTPImpact::EOutcome Outcome = GTPImpact::StepRagdoll(State, Speed, UpDot, DeltaTime, Timings);
	if (Outcome == GTPImpact::EOutcome::StayDown)
	{
		Finish(false);
	}
	else if (Outcome != GTPImpact::EOutcome::Simulating)
	{
		const bool bFaceUp = Outcome == GTPImpact::EOutcome::GetUpFaceUp;
		RestoreCharacter();
		UAnimMontage* Montage = (bFaceUp ? GetUpFaceUp : GetUpFaceDown).LoadSynchronous();
		if (Montage && Mesh->GetAnimInstance())
		{
			Mesh->GetAnimInstance()->Montage_Play(Montage);
		}
		Finish(true);
	}
}

void UGTPRagdollComponent::RestoreCharacter()
{
	if (!Mesh)
	{
		return;
	}
	const FTransform Pelvis = Mesh->GetSocketTransform(PelvisBone);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(MeshCollision);
	Mesh->SetCollisionProfileName(MeshProfile);

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		// Stand where the pelvis lies, facing where the belly or back looked, on the floor below.
		FVector Location = Pelvis.GetLocation();
		FHitResult Floor;
		const FVector Start = Location + FVector(0.f, 0.f, 50.f);
		if (GetWorld()->LineTraceSingleByChannel(Floor, Start, Start - FVector(0.f, 0.f, 400.f), ECC_Visibility))
		{
			Location.Z = Floor.ImpactPoint.Z + CapsuleHalfHeight;
		}
		const FVector Forward = Pelvis.TransformVectorNoScale(PelvisFrontAxis).GetSafeNormal2D();
		const FRotator Yaw(0.f, Forward.IsNearlyZero() ? Character->GetActorRotation().Yaw : Forward.Rotation().Yaw, 0.f);
		Character->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Character->SetActorLocationAndRotation(Location, Yaw, false, nullptr, ETeleportType::TeleportPhysics);
		if (MeshParent.IsValid())
		{
			Mesh->AttachToComponent(MeshParent.Get(), FAttachmentTransformRules::KeepWorldTransform, MeshParentSocket);
			Mesh->SetRelativeTransform(MeshRelative, false, nullptr, ETeleportType::TeleportPhysics);
		}
		Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void UGTPRagdollComponent::Freeze()
{
	if (bActive)
	{
		Finish(false);
	}
}

void UGTPRagdollComponent::Finish(bool bGotUp)
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	SetComponentTickEnabled(false);
	if (!bGotUp && Mesh)
	{
		// Keep the pose; no more simulation cost.
		Mesh->SetSimulatePhysics(false);
	}
	if (UWorld* World = GetWorld())
	{
		if (UGTPRagdollSubsystem* Subsystem = World->GetSubsystem<UGTPRagdollSubsystem>())
		{
			Subsystem->Unregister(this);
		}
	}
	OnRagdollEnded.Broadcast(this, bGotUp);
}

void UGTPRagdollComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bActive)
	{
		bActive = false;
		if (UWorld* World = GetWorld())
		{
			if (UGTPRagdollSubsystem* Subsystem = World->GetSubsystem<UGTPRagdollSubsystem>())
			{
				Subsystem->Unregister(this);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------- subsystem

void UGTPRagdollSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InWorld.GetTimerManager().SetTimer(EnforceTimer, this, &UGTPRagdollSubsystem::Enforce, 0.5f, true);
}

void UGTPRagdollSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EnforceTimer);
	}
	Active.Reset();
	Super::Deinitialize();
}

void UGTPRagdollSubsystem::Register(UGTPRagdollComponent* Ragdoll)
{
	Active.AddUnique(Ragdoll);
}

bool UGTPRagdollSubsystem::HasRoom() const
{
	int32 Running = 0;
	for (const TWeakObjectPtr<UGTPRagdollComponent>& P : Active)
	{
		Running += P.IsValid() ? 1 : 0;
	}
	return Running < ChooseCap();
}

void UGTPRagdollSubsystem::Unregister(UGTPRagdollComponent* Ragdoll)
{
	Active.Remove(Ragdoll);
}

int32 UGTPRagdollSubsystem::ChooseCap() const
{
	int32 Override = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("-MaxRagdolls="), Override))
	{
		return FMath::Max(0, Override);
	}
	FString Profile = TEXT("m1");
	if (const UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (const UGTPGraphicsProfile* Gfx = GI->GetSubsystem<UGTPGraphicsProfile>())
		{
			Profile = Gfx->GetActiveProfile();
		}
	}
	if (Profile == TEXT("ultra"))
	{
		return MaxRagdollsUltra;
	}
	return Profile == TEXT("fidelity") ? MaxRagdollsFidelity : MaxRagdollsM1;
}

void UGTPRagdollSubsystem::Enforce()
{
	Active.RemoveAll([](const TWeakObjectPtr<UGTPRagdollComponent>& P) { return !P.IsValid(); });
	const int32 Cap = ChooseCap();
	if (Active.Num() <= Cap)
	{
		return;
	}
	FVector Viewer = FVector::ZeroVector;
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		FRotator Rotation;
		PC->GetPlayerViewPoint(Viewer, Rotation);
	}
	std::vector<GTPImpact::FRagdollEntry> Entries;
	for (const TWeakObjectPtr<UGTPRagdollComponent>& P : Active)
	{
		Entries.push_back({static_cast<float>(FVector::Dist(Viewer, P->GetOwner()->GetActorLocation())), P->GetAge()});
	}
	TArray<TWeakObjectPtr<UGTPRagdollComponent>> ToFreeze;
	for (int32 Index : GTPImpact::ChooseToFreeze(Entries, Cap))
	{
		ToFreeze.Add(Active[Index]);
	}
	for (const TWeakObjectPtr<UGTPRagdollComponent>& P : ToFreeze)
	{
		if (P.IsValid())
		{
			P->Freeze();
		}
	}
}
