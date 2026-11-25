#include "GTPPoliceOfficer.h"
#include "GTPOnFoot.h"

#include "Animation/AnimSequenceBase.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GTPLog.h"
#include "GTPVehicle.h"
#include "GTPWanted.h"
#include "GTPWeapon.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	Tags.AddUnique(GTPPolice::PoliceTag);
}

void AGTPPoliceOfficer::BeginPlay()
{
	Super::BeginPlay();
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
		Move->SetComponentTickEnabled(true);
	}
	// He runs to the player's car and along it: he must not throw it (gate 99, GTPOnFoot.h).
	GTPOnFoot::NoShoving(this);
	static bool bShovingTold = false;
	if (!bShovingTold)
	{
		bShovingTold = true;
		const UCharacterMovementComponent* Legs = GetCharacterMovement();
		GTP_TRACE(LogTemp, TEXT("police: officers on foot %s the cars they run into (-PeopleShoveCars to have them push)"),
			Legs && Legs->bEnablePhysicsInteraction ? TEXT("PUSH") : TEXT("do not push"));
	}
	SetActorTickEnabled(true);
	SpawnTime = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	// In uniform the body is another skeleton than the mannequin's: its own run loop, or none.
	const bool bUniform = WearUniform();
	FString AnimOverride;
	FSoftObjectPath Path = bUniform ? UniformRunAnimation : RunAnimation;
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
		else if (bUniform)
		{
			GTP_TRACE(LogTemp, TEXT("police: officer in uniform without a run loop: %s does not load (it slides in its reference pose)"), *Path.ToString());
		}
	}
}

bool AGTPPoliceOfficer::WearUniform()
{
	USkeletalMeshComponent* Body = GetMesh();
	int32 Switch = 1;
	FParse::Value(FCommandLine::Get(), TEXT("PoliceUniform="), Switch);
	if (!Body || Switch == 0 || !UniformBody.IsValid())
	{
		return false;
	}
	USkeletalMesh* BodyMesh = Cast<USkeletalMesh>(UniformBody.TryLoad());
	if (!BodyMesh)
	{
		GTP_TRACE(LogTemp, TEXT("police: officer without uniform: the body %s does not load"), *UniformBody.ToString());
		return false;
	}
	// The crowd's bodies have no head of their own: without the face asked for, the old body is better than a headless one.
	if (UniformFace.IsValid() && !Cast<USkeletalMesh>(UniformFace.TryLoad()))
	{
		GTP_TRACE(LogTemp, TEXT("police: officer without uniform: the face %s does not load"), *UniformFace.ToString());
		return false;
	}
	Body->SetSkeletalMesh(BodyMesh);
	Body->SetRelativeRotation(FRotator(0.f, UniformYawOffset, 0.f));

	// Garments and face: skeletal meshes on the body's skeleton that take its pose (as AGTPPedestrian::Dress does).
	FString Missing;
	int32 Garments = 0;
	auto AddFollower = [this, Body, &Missing](const FSoftObjectPath& PartPath, const FString& PartName) -> USkeletalMeshComponent*
	{
		USkeletalMesh* PartMesh = PartPath.IsValid() ? Cast<USkeletalMesh>(PartPath.TryLoad()) : nullptr;
		if (!PartMesh)
		{
			if (PartPath.IsValid())
			{
				Missing += FString::Printf(TEXT(" %s"), *PartPath.GetAssetName());
			}
			return nullptr;
		}
		USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(this, *PartName);
		Part->SetSkeletalMesh(PartMesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->bReceivesDecals = false;
		Part->bUseAttachParentBound = true;
		Part->bSyncAttachParentLOD = true;
		Part->PrimaryComponentTick.bCanEverTick = false;
		Part->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Part->RegisterComponent();
		Part->SetLeaderPoseComponent(Body);
		UniformParts.Add(Part);
		return Part;
	};
	int32 Coloured = 0;
	int32 OwnMaterials = 0;
	for (int32 Index = 0; Index < UniformClothes.Num(); ++Index)
	{
		USkeletalMeshComponent* Part = AddFollower(UniformClothes[Index], FString::Printf(TEXT("UniformGarment%d"), Index));
		if (!Part)
		{
			continue;
		}
		++Garments;
		// The uniform's own materials, slot by slot, where the editor script made them.
		const FString GarmentName = UniformClothes[Index].GetAssetName();
		for (int32 Slot = 0; !UniformMaterialFolder.IsEmpty() && Slot < Part->GetNumMaterials(); ++Slot)
		{
			const FString MaterialName = FString::Printf(TEXT("MI_GTP_Uniform_%s_%d"), *GarmentName, Slot);
			if (UMaterialInterface* Own = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *UniformMaterialFolder, *MaterialName, *MaterialName),
				nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				Part->SetMaterial(Slot, Own);
				++OwnMaterials;
			}
		}
		// The colour goes to the garments only, never to the skin.
		if (UniformColourDataIndex >= 0)
		{
			Part->SetCustomPrimitiveDataVector4(UniformColourDataIndex, FVector4(UniformColour.R, UniformColour.G, UniformColour.B, UniformColour.A));
			++Coloured;
		}
		if (UniformColourParameters.Num() > 0)
		{
			for (int32 Slot = 0; Slot < Part->GetNumMaterials(); ++Slot)
			{
				if (UMaterialInstanceDynamic* Material = Part->CreateAndSetMaterialInstanceDynamic(Slot))
				{
					// A parameter the material does not have is ignored.
					for (const FName& Parameter : UniformColourParameters)
					{
						Material->SetVectorParameterValue(Parameter, UniformColour);
					}
				}
			}
			++Coloured;
		}
	}
	const bool bFace = AddFollower(UniformFace, TEXT("UniformFace")) != nullptr;

	// Rigid pieces on the bones.
	FString Worn;
	for (int32 Index = 0; Index < UniformAccessories.Num(); ++Index)
	{
		const FGTPOfficerAccessory& Piece = UniformAccessories[Index];
		UStaticMesh* PieceMesh = Piece.Mesh.IsValid() ? Cast<UStaticMesh>(Piece.Mesh.TryLoad()) : nullptr;
		if (!PieceMesh || Body->GetBoneIndex(Piece.Bone) == INDEX_NONE)
		{
			const FString Why = PieceMesh ? FString::Printf(TEXT("no bone %s"), *Piece.Bone.ToString()) : FString(TEXT("does not load"));
			Missing += FString::Printf(TEXT(" %s (%s)"), *Piece.Mesh.GetAssetName(), *Why);
			continue;
		}
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("UniformPiece%d"), Index));
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetStaticMesh(PieceMesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->bReceivesDecals = false;
		Part->PrimaryComponentTick.bCanEverTick = false;
		Part->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Piece.Bone);
		Part->SetRelativeTransform(FTransform(Piece.Rotation, Piece.Location, FVector(Piece.Scale)));
		Part->RegisterComponent();
		UniformParts.Add(Part);
		Worn += FString::Printf(TEXT(" %s on %s"), *Piece.Mesh.GetAssetName(), *Piece.Bone.ToString());
	}
	const FString FaceName = bFace ? UniformFace.GetAssetName() : FString(TEXT("none"));
	const FString LoopName = UniformRunAnimation.IsValid() ? UniformRunAnimation.GetAssetName() : FString(TEXT("none"));
	GTP_TRACE(LogTemp, TEXT("police: officer in uniform: body %s, %d garment(s) of %d (%d given the colour, %d slot(s) given the uniform's own material), face %s, run loop %s; pieces:%s; left out:%s"),
		*BodyMesh->GetName(), Garments, UniformClothes.Num(), Coloured, OwnMaterials, *FaceName, *LoopName, Worn.IsEmpty() ? TEXT(" none") : *Worn,
		Missing.IsEmpty() ? TEXT(" nothing") : *Missing);
	return true;
}

void AGTPPoliceOfficer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GTP_TRACE(LogTemp, TEXT("police: officer ends (reason %d) at %s after %.1f s"), static_cast<int32>(EndPlayReason), *GetActorLocation().ToCompactString(),
		GetWorld() ? GetWorld()->GetTimeSeconds() - SpawnTime : 0.0);
	Super::EndPlay(EndPlayReason);
}

bool AGTPPoliceOfficer::HasLineOfSight(const FVector& From, const AActor& Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPOfficerSight), false, this);
	Params.AddIgnoredActor(&Target);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, From, Target.GetActorLocation() + FVector(0.f, 0.f, 60.f), ECC_Visibility, Params);
}

bool AGTPPoliceOfficer::IsWayBlocked(const FVector& Direction, const AActor* Ignored, FVector& OutNormal) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPOfficerWay), false, this);
	if (Ignored)
	{
		Params.AddIgnoredActor(Ignored);
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	// A ball at his waist, a step and a half ahead: the ground and the kerbs pass under it.
	const FVector Start = GetActorLocation();
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByObjectType(Hit, Start, Start + Direction * 150.f, FQuat::Identity, Objects, FCollisionShape::MakeSphere(30.f), Params))
	{
		return false;
	}
	// The ground he walks up is not in his way.
	if (Hit.ImpactNormal.Z > 0.6f)
	{
		return false;
	}
	FVector Face(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f);
	if (Hit.bStartPenetrating || Face.IsNearlyZero())
	{
		Face = -Direction;
	}
	OutNormal = Face.GetSafeNormal2D();
	return true;
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
		if (SearchSeconds <= 0.f)
		{
			GTP_TRACE(LogTemp, TEXT("police: officer lost sight, searching (slot %d/%d)"), SlotIndex, SlotCount);
		}
		SearchSeconds += DeltaSeconds;
		float Ox = 0.f, Oy = 0.f;
		GTPPoliceTactics::SearchOffset(Tactics, SlotIndex, SlotCount, SearchSeconds, Ox, Oy);
		const FVector Last = Wanted->GetLastKnownLocation();
		// Go to the last known position first, then sweep the circle around it.
		const bool bAtLast = FVector::Dist2D(Here, Last) < Tactics.ArriveRadius * 3.f;
		FVector Sweep = Last + FVector(Ox, Oy, 0.f);
		if (SearchPoints.Num() > 0)
		{
			// EQS points: each officer starts on a different one and moves to the next every 8 s.
			const int32 Pick = (SlotIndex + static_cast<int32>(SearchSeconds / 8.f)) % SearchPoints.Num();
			Sweep = FVector(SearchPoints[Pick].X, SearchPoints[Pick].Y, Here.Z);
		}
		Goal = SearchSeconds > 4.f && bAtLast ? Sweep : Last;
	}

	// Close in until about two metres, then hold (the arrest is the wanted component's, by proximity).
	if (FVector::Dist2D(Here, Goal) > 200.f)
	{
		FVector Direction = (Goal - Here).GetSafeNormal2D();
		// Something in the way (a wall, a car, his own, the street's furniture): along it, the same hand on it,
		// until the way is free. At gate 75 the one officer stayed 36 m from the stopped player for 25 s: he
		// walked straight at his goal, whatever stood between.
		FVector Wall = FVector::ZeroVector;
		FVector NextWall = FVector::ZeroVector;
		const bool bBlocked = IsWayBlocked(Direction, Pawn, Wall);
		bool bAlongBlocked = false;
		if (bBlocked)
		{
			float AlongX = 0.f;
			float AlongY = 0.f;
			Detour.Along(static_cast<float>(Direction.X), static_cast<float>(Direction.Y), static_cast<float>(Wall.X), static_cast<float>(Wall.Y), AlongX, AlongY);
			bAlongBlocked = IsWayBlocked(FVector(AlongX, AlongY, 0.f), Pawn, NextWall);
		}
		float WalkX = 0.f;
		float WalkY = 0.f;
		const int32 TurnsBefore = Detour.Detours + Detour.Flips;
		Detour.Step(DeltaSeconds, static_cast<float>(Here.X) * 0.01f, static_cast<float>(Here.Y) * 0.01f, static_cast<float>(Direction.X),
			static_cast<float>(Direction.Y), bBlocked, static_cast<float>(Wall.X), static_cast<float>(Wall.Y), bAlongBlocked,
			static_cast<float>(NextWall.X), static_cast<float>(NextWall.Y), WalkX, WalkY);
		if (Detour.Detours + Detour.Flips != TurnsBefore)
		{
			GTP_TRACE(LogTemp, TEXT("police: officer goes round something (hand %d, %d detour(s), %d change(s) of hand), %.0f m from his goal"), Detour.Hand,
				Detour.Detours, Detour.Flips, FVector::Dist2D(Here, Goal) * 0.01);
		}
		Direction = FVector(WalkX, WalkY, 0.f);
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
		const bool bFired = Weapon->FireAt(Player + FVector(0.f, 0.f, 40.f), GTPPoliceTactics::SpreadFactor(Tactics, Stars));
		GTP_TRACE(LogTemp, TEXT("police: officer %s at %.0f m stars=%d"), bFired ? TEXT("fires") : TEXT("dry"), Distance * 0.01f, Stars);
		FireCooldown = GTPPoliceTactics::FireInterval(Tactics, Stars);
	}
}
