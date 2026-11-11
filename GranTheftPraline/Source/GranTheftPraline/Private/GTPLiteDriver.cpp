#include "GTPLiteDriver.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AGTPLiteDriver::AGTPLiteDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Nothing here ticks by itself: the vehicle drives the occupant.
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->PrimaryComponentTick.bStartWithTickEnabled = false;
		Move->SetComponentTickEnabled(false);
		Move->DefaultLandMovementMode = MOVE_None;
		Move->bRunPhysicsWithNoController = false;
	}
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetRelativeRotation(FRotator(0.f, MeshYawOffset, 0.f));
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
		Body->SetCastShadow(false);
	}
}

void AGTPLiteDriver::BeginPlay()
{
	Super::BeginPlay();
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body)
	{
		return;
	}
	FString MeshOverride;
	FSoftObjectPath Path = MeshPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("LiteDriverMesh="), MeshOverride) && !MeshOverride.IsEmpty())
	{
		Path = FSoftObjectPath(MeshOverride);
	}
	USkeletalMesh* DriverMesh = Path.IsValid() ? Cast<USkeletalMesh>(Path.TryLoad()) : nullptr;
	if (!DriverMesh)
	{
		// The player's own mesh: already loaded, so no synchronous load at spawn.
		if (const ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (Player->GetMesh())
			{
				DriverMesh = Player->GetMesh()->GetSkeletalMeshAsset();
			}
		}
	}
	if (!DriverMesh)
	{
		// The player is in a car (the pawn is the vehicle): the player character class's own mesh.
		const AGameModeBase* Mode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
		const UClass* PawnClass = Mode ? Mode->GetDefaultPawnClassForController(nullptr) : nullptr;
		const ACharacter* Template = PawnClass ? Cast<ACharacter>(PawnClass->GetDefaultObject()) : nullptr;
		if (Template && Template->GetMesh())
		{
			DriverMesh = Template->GetMesh()->GetSkeletalMeshAsset();
		}
	}
	if (DriverMesh)
	{
		Body->SetSkeletalMesh(DriverMesh);
	}
	FString AnimOverride;
	FSoftObjectPath AnimPath = SeatedAnimation;
	if (FParse::Value(FCommandLine::Get(), TEXT("LiteDriverAnim="), AnimOverride) && !AnimOverride.IsEmpty())
	{
		AnimPath = FSoftObjectPath(AnimOverride);
	}
	if (AnimPath.IsValid())
	{
		if (UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(AnimPath.TryLoad()))
		{
			Body->SetAnimation(Anim);
			Body->Play(true);
		}
	}
	ApplyMovementTick();
}

void AGTPLiteDriver::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
	ApplyMovementTick();
}

void AGTPLiteDriver::ApplyMovementTick()
{
	// Seated (MOVE_None) the movement component sleeps; any other mode (the vehicle's release
	// sets MOVE_Walking) wakes it, so a pulled-out driver can run away.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		const bool bAwake = Move->MovementMode != MOVE_None;
		if (Move->IsComponentTickEnabled() != bAwake)
		{
			Move->SetComponentTickEnabled(bAwake);
		}
		SetActorTickEnabled(false);
	}
}
