#include "GTPPoliceOfficer.h"

#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GTPVehicle.h"
#include "GTPWanted.h"
#include "GTPWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AGTPPoliceOfficer::AGTPPoliceOfficer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// The lite driver sleeps seated; an officer walks.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->DefaultLandMovementMode = MOVE_Walking;
		Move->MaxWalkSpeed = Tactics.FootSpeedCmS;
		Move->bOrientRotationToMovement = false;
		Move->bRunPhysicsWithNoController = true;
		Move->SetComponentTickEnabled(true);
	}
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetCastShadow(true);
	}
	Weapon = CreateDefaultSubobject<UGTPWeaponComponent>(TEXT("Weapon"));
}

void AGTPPoliceOfficer::BeginPlay()
{
	Super::BeginPlay();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
		Move->SetComponentTickEnabled(true);
	}
	SetActorTickEnabled(true);
	FString AnimOverride;
	FSoftObjectPath Path = RunAnimation;
	if (FParse::Value(FCommandLine::Get(), TEXT("PoliceRunAnim="), AnimOverride) && !AnimOverride.IsEmpty())
	{
		Path = FSoftObjectPath(AnimOverride);
	}
	if (Path.IsValid() && GetMesh())
	{
		if (UAnimSequenceBase* Anim = Cast<UAnimSequenceBase>(Path.TryLoad()))
		{
			GetMesh()->SetAnimation(Anim);
			GetMesh()->Play(true);
		}
	}
}

bool AGTPPoliceOfficer::HasLineOfSight(const FVector& From, const AActor& Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPOfficerSight), false, this);
	Params.AddIgnoredActor(&Target);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, Target.GetActorLocation() + FVector(0.f, 0.f, 60.f), ECC_Visibility, Params);
}

void AGTPPoliceOfficer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	UGTPWantedComponent* Wanted = PC ? PC->FindComponentByClass<UGTPWantedComponent>() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Wanted || !Pawn || Wanted->GetStars() <= 0)
	{
		return;
	}
	const int32 Stars = Wanted->GetStars();
	const FVector Here = GetActorLocation();
	const FVector Player = Pawn->GetActorLocation();
	const float Distance = static_cast<float>(FVector::Dist2D(Here, Player));

	// Line of sight is traced at 5 Hz.
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	if (Now >= NextLosTime)
	{
		NextLosTime = Now + 0.2f;
		bLosCached = Distance <= Wanted->NoticeRange || HasLineOfSight(Here + FVector(0.f, 0.f, 80.f), *Pawn);
	}
	const bool bSees = bLosCached;

	FVector Goal = Player;
	if (bSees)
	{
		SearchSeconds = 0.f;
	}
	else
	{
		SearchSeconds += DeltaSeconds;
		float Ox = 0.f, Oy = 0.f;
		GTPPoliceTactics::SearchOffset(Tactics, SlotIndex, SlotCount, SearchSeconds, Ox, Oy);
		const FVector Last = Wanted->GetLastKnownLocation();
		// Go to the last known position first, then sweep the circle around it.
		const bool bAtLast = FVector::Dist2D(Here, Last) < Tactics.ArriveRadius * 3.f;
		Goal = SearchSeconds > 4.f && bAtLast ? Last + FVector(Ox, Oy, 0.f) : Last;
	}

	// Close in until about two metres, then hold (the arrest is the wanted component's, by proximity).
	if (FVector::Dist2D(Here, Goal) > 200.f)
	{
		const FVector Direction = (Goal - Here).GetSafeNormal2D();
		AddMovementInput(Direction, 1.f);
		const FRotator Face = Direction.Rotation();
		SetActorRotation(FRotator(0.f, Face.Yaw, 0.f));
	}
	else if (bSees)
	{
		SetActorRotation(FRotator(0.f, (Player - Here).Rotation().Yaw, 0.f));
	}

	FireCooldown = FMath::Max(0.f, FireCooldown - DeltaSeconds);
	if (Weapon && bSees && GTPPoliceTactics::MayFire(Tactics, Stars, Wanted->IsPlayerAggressive(), Distance, true, FireCooldown))
	{
		Weapon->FireAt(Player + FVector(0.f, 0.f, 40.f), GTPPoliceTactics::SpreadFactor(Tactics, Stars));
		FireCooldown = GTPPoliceTactics::FireInterval(Tactics, Stars);
	}
}
