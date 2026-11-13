#include "GTPWeaponPickup.h"

#include "GTPCombat.h"
#include "GTPLog.h"
#include "GTPWeapon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"

AGTPWeaponPickup::AGTPWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(90.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	Gun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Gun"));
	Gun->SetupAttachment(Trigger);
	Gun->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Gun->SetGenerateOverlapEvents(false);
	// Lying on its side a little above the ground.
	Gun->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -70.f), FRotator(0.f, 0.f, 90.f));

	Box = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Box"));
	Box->SetupAttachment(Trigger);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);
	Box->SetRelativeLocation(FVector(0.f, 0.f, -80.f));
	Box->SetRelativeScale3D(FVector(0.24f, 0.07f, 0.12f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Box->SetStaticMesh(Cube.Object);
	}
}

void AGTPWeaponPickup::Setup(GTPArsenal::EKind InKind, int32 InRounds, USkeletalMesh* GunMesh, float LifeSeconds)
{
	Kind = InKind;
	Rounds = InRounds;
	if (GunMesh)
	{
		Gun->SetSkeletalMeshAsset(GunMesh);
		Box->SetVisibility(false);
	}
	else
	{
		Gun->SetVisibility(false);
	}
	SetLifeSpan(FMath::Max(1.f, LifeSeconds));
}

void AGTPWeaponPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// A slow turn catches the eye on the pavement.
	AddActorLocalRotation(FRotator(0.f, 70.f * DeltaSeconds, 0.f));
}

void AGTPWeaponPickup::NotifyActorBeginOverlap(AActor* Other)
{
	Super::NotifyActorBeginOverlap(Other);
	const APawn* Pawn = Cast<APawn>(Other);
	if (!Pawn || !Pawn->IsPlayerControlled() || Rounds <= 0)
	{
		return;
	}
	// On foot, or driving over it (the driver's weapon: GTA lets you pick up from the car).
	UGTPWeaponComponent* Weapon = UGTPWeaponComponent::FindHeld(Pawn);
	const int32 Taken = Weapon ? Weapon->CollectAmmo(Kind, Rounds) : 0;
	if (Taken <= 0)
	{
		return; // full: it stays for later
	}
	GTP_TRACE(LogGTPCombat, TEXT("combat: picked up weapon %d, %d rounds"), static_cast<int32>(Kind), Taken);
	Destroy();
}
