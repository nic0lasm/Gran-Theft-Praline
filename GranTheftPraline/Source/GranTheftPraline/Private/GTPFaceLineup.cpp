#include "GTPFaceLineup.h"

#include "GTPFollowCameraComponent.h"
#include "GTPPedestrians.h"
#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPFaceLineup, Log, All);

namespace GTPFaceLineupPrivate
{
	const TCHAR* const FaceSuffix = TEXT("_nrw_FaceMesh");

	/** Component-space transform of a bone in the mesh's reference pose (INDEX_NONE: not found). */
	bool RefPoseBone(const USkeletalMesh* Mesh, FName Bone, FTransform& Out)
	{
		const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
		int32 Index = Skeleton.FindBoneIndex(Bone);
		if (Index == INDEX_NONE)
		{
			return false;
		}
		Out = FTransform::Identity;
		const TArray<FTransform>& Pose = Skeleton.GetRefBonePose();
		while (Index != INDEX_NONE)
		{
			Out = Out * Pose[Index];
			Index = Skeleton.GetParentIndex(Index);
		}
		return true;
	}
}

bool UGTPFaceLineupSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const bool bWanted = FParse::Param(FCommandLine::Get(), TEXT("NicolitoFaces")) || FParse::Param(FCommandLine::Get(), TEXT("NicolitoPortrait"));
	if (!bWanted || !Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld();
}

void UGTPFaceLineupSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	using namespace GTPFaceLineupPrivate;
	if (FParse::Param(FCommandLine::Get(), TEXT("NicolitoPortrait")))
	{
		// The player as he is in the game: two shots, no crowd head.
		bPlayerPortrait = true;
		Labels = {TEXT("player_face"), TEXT("player_body")};
		Heads = Labels;
		UE_LOG(LogGTPFaceLineup, Log, TEXT("face lineup: player portrait (face, then full length)"));
		InWorld.GetTimerManager().SetTimer(Timer, this, &UGTPFaceLineupSubsystem::ShowNext, 10.f, false);
		return;
	}
	// Every normal-weight male head of the pack, whatever was copied (the migrated subset had 001 to 003).
	const FString Male = CrowdCharacterRoot + TEXT("/Male");
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({Male}, false);
	TArray<FAssetData> Found;
	Registry.GetAssetsByPath(FName(*Male), Found, true);
	Found.Sort([](const FAssetData& A, const FAssetData& B) { return A.AssetName.LexicalLess(B.AssetName); });
	for (const FAssetData& Data : Found)
	{
		const FString Name = Data.AssetName.ToString();
		if (Heads.Num() < MaxHeads && Name.EndsWith(FaceSuffix) && Data.AssetClassPath == USkeletalMesh::StaticClass()->GetClassPathName())
		{
			Heads.Add(Data.GetObjectPathString());
			Labels.Add(Name.LeftChop(FCString::Strlen(FaceSuffix)));
		}
	}
	UE_LOG(LogGTPFaceLineup, Log, TEXT("face lineup: %d male head(s) under %s (%d asset(s) there): %s"), Heads.Num(), *Male, Found.Num(),
		*FString::Join(Labels, TEXT(", ")));
	// Late enough for the world, the player and the textures around to be there.
	InWorld.GetTimerManager().SetTimer(Timer, this, &UGTPFaceLineupSubsystem::ShowNext, 10.f, false);
}

void UGTPFaceLineupSubsystem::ShowNext()
{
	using namespace GTPFaceLineupPrivate;
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (Model)
	{
		Model->Destroy();
		Model = nullptr;
	}
	++Current;
	if (!World || !Pawn || !Heads.IsValidIndex(Current))
	{
		if (Camera)
		{
			Camera->Destroy();
			Camera = nullptr;
		}
		if (PC)
		{
			HoldStage(PC, false);
		}
		if (Pawn && PC->GetViewTarget() != Pawn)
		{
			// The follow camera, ticking again, takes the view back from the pawn by itself.
			PC->SetViewTargetWithBlend(Pawn, 0.f);
		}
		UE_LOG(LogGTPFaceLineup, Log, TEXT("face lineup: done, %d head(s) shown%s"), FMath::Min(Current, Heads.Num()), Pawn ? TEXT("") : TEXT(" (no player pawn)"));
		return;
	}
	if (bPlayerPortrait)
	{
		if (!FramePlayer(PC, Current == 0))
		{
			UE_LOG(LogGTPFaceLineup, Warning, TEXT("face lineup: %s skipped (no player visual with a head bone)"), *Labels[Current]);
			World->GetTimerManager().SetTimer(Timer, this, &UGTPFaceLineupSubsystem::ShowNext, 0.1f, false);
			return;
		}
		World->GetTimerManager().SetTimer(Timer, this, &UGTPFaceLineupSubsystem::Shoot, 4.f, false);
		return;
	}
	const FString& Label = Labels[Current];
	const FString Male = CrowdCharacterRoot + TEXT("/Male");
	auto Part = [&Male](const TCHAR* Name) -> USkeletalMesh*
	{
		const FString Mesh = FString::Printf(TEXT("m_tal_nrw_%s"), Name);
		return LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/NormalWeight/Meshes/%s.%s"), *Male, *Mesh, *Mesh), nullptr, LOAD_NoWarn | LOAD_Quiet);
	};
	USkeletalMesh* BodyMesh = Part(TEXT("body"));
	TArray<USkeletalMesh*> Clothes;
	for (const TCHAR* Garment : {TEXT("crewneck"), TEXT("jeans"), TEXT("loafers")})
	{
		if (USkeletalMesh* Garb = Part(Garment))
		{
			Clothes.Add(Garb);
		}
	}
	USkeletalMesh* Face = LoadObject<USkeletalMesh>(nullptr, *Heads[Current]);
	TSubclassOf<UAnimInstance> FaceAnim = FSoftClassPath(FaceAnimClass).TryLoadClass<UAnimInstance>();

	// On the ground 4 m in front of the player.
	FVector Feet = Pawn->GetActorLocation() + Pawn->GetActorForwardVector() * 400.0;
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPFaceLineup), false, Pawn);
	if (World->LineTraceSingleByChannel(Ground, Feet + FVector(0.0, 0.0, 300.0), Feet - FVector(0.0, 0.0, 1000.0), ECC_Visibility, Params))
	{
		Feet = Ground.ImpactPoint;
	}
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// The actor's +X towards the player; the body is turned below so that the face looks that way too.
	Model = World->SpawnActor<AGTPPedestrian>(AGTPPedestrian::StaticClass(), Feet, FRotator(0.f, Pawn->GetActorRotation().Yaw + 180.f, 0.f), Spawn);
	if (!Model || !BodyMesh || !Face)
	{
		UE_LOG(LogGTPFaceLineup, Warning, TEXT("face lineup: %s skipped (model %d, body %d, face %d)"), *Label, Model ? 1 : 0, BodyMesh ? 1 : 0, Face ? 1 : 0);
		World->GetTimerManager().SetTimer(Timer, this, &UGTPFaceLineupSubsystem::ShowNext, 0.1f, false);
		return;
	}
	Model->Dress(BodyMesh, Clothes, Face, FaceAnim);

	// Which way the body looks in its own space: right = left clavicle to right clavicle, forward = right ^ up
	// (X forward, Y right, Z up). The crowd's note says the pack's bodies face +X; this measures it.
	FVector MeshForward = FVector::ForwardVector;
	FTransform Left, Right, HeadBone;
	const bool bClavicles = RefPoseBone(BodyMesh, TEXT("clavicle_l"), Left) && RefPoseBone(BodyMesh, TEXT("clavicle_r"), Right);
	if (bClavicles)
	{
		const FVector Side = (Right.GetLocation() - Left.GetLocation()).GetSafeNormal2D();
		if (!Side.IsNearlyZero())
		{
			MeshForward = FVector::CrossProduct(Side, FVector::UpVector);
		}
	}
	const double MeshYaw = MeshForward.Rotation().Yaw;
	USkeletalMeshComponent* Body = Model->GetBody();
	Body->SetRelativeRotation(FRotator(0.0, -MeshYaw, 0.0));
	const FVector Facing = Body->GetComponentTransform().TransformVectorNoScale(MeshForward).GetSafeNormal2D();
	FVector HeadAt = Feet + FVector(0.0, 0.0, 163.0);
	const bool bHead = RefPoseBone(BodyMesh, TEXT("head"), HeadBone);
	if (bHead)
	{
		HeadAt = Body->GetComponentTransform().TransformPosition(HeadBone.GetLocation());
	}
	TArray<USkeletalMeshComponent*> Meshes;
	Model->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetSkeletalMeshAsset() == Face)
		{
			Mesh->SetForcedLOD(1); // its finest level: the crowd forces a coarse one
		}
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	if (!Camera)
	{
		Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Spawn);
	}
	if (Camera)
	{
		HoldStage(PC, true);
		// The head bone sits at the base of the skull: aim a little higher, at the eyes.
		const FVector Look = HeadAt + FVector(0.0, 0.0, 8.0);
		const FVector Eye = Look + Facing * CameraDistanceCm;
		Camera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
		Camera->GetCameraComponent()->SetFieldOfView(CameraFOV);
		Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
		PC->SetViewTargetWithBlend(Camera, 0.f);
	}
	UE_LOG(LogGTPFaceLineup, Log,
		TEXT("face lineup: %s at %s, body faces %s in its mesh (%s), head %.0f cm above the feet (%s), face anim %s, %d garment(s)"), *Label,
		*Feet.ToCompactString(), *MeshForward.ToCompactString(), bClavicles ? TEXT("clavicles") : TEXT("assumed"), (HeadAt - Feet).Z,
		bHead ? TEXT("head bone") : TEXT("assumed"), FaceAnim ? *FaceAnim->GetName() : TEXT("none, follows the body"), Clothes.Num());
	// Shot once the face's textures had time to stream in, next head a second later.
	World->GetTimerManager().SetTimer(Timer, this, &UGTPFaceLineupSubsystem::Shoot, 4.f, false);
}

void UGTPFaceLineupSubsystem::Shoot()
{
	UWorld* World = GetWorld();
	if (!World || !Labels.IsValidIndex(Current))
	{
		return;
	}
	const FString File = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ScreenShotDir(), FString::Printf(TEXT("Nicolito_%s.png"), *Labels[Current])));
	FScreenshotRequest::RequestScreenshot(File, false, false);
	// The proof that the image is the portrait and not the game's view.
	const APlayerController* PC = World->GetFirstPlayerController();
	const AActor* Viewing = PC ? PC->GetViewTarget() : nullptr;
	const FVector Eye = PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	UE_LOG(LogGTPFaceLineup, Log, TEXT("face lineup: %s portrait requested: %s; view target %s (%s), view %.0f cm from the portrait camera, FOV %.0f"),
		*Labels[Current], *File, *GetNameSafe(Viewing), Viewing && Viewing == Camera ? TEXT("ours") : TEXT("NOT OURS"),
		Camera ? FVector::Dist(Eye, Camera->GetActorLocation()) : -1.0, PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetFOVAngle() : -1.f);
	World->GetTimerManager().SetTimer(Timer, this, &UGTPFaceLineupSubsystem::ShowNext, 1.f, false);
}

bool UGTPFaceLineupSubsystem::FramePlayer(APlayerController* PC, bool bFace)
{
	UWorld* World = GetWorld();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!World || !Pawn)
	{
		return false;
	}
	// The visible mesh with a head bone: the player's visual attached to the pawn (its body), else the pawn's own.
	TArray<AActor*> Actors;
	Pawn->GetAttachedActors(Actors, true, true);
	Actors.Add(Pawn);
	USkeletalMeshComponent* Shown = nullptr;
	for (AActor* Actor : Actors)
	{
		TArray<USkeletalMeshComponent*> Meshes;
		Actor->GetComponents<USkeletalMeshComponent>(Meshes);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!Shown && Mesh->IsVisible() && Mesh->GetSkeletalMeshAsset() && Mesh->GetBoneIndex(TEXT("head")) != INDEX_NONE)
			{
				Shown = Mesh;
			}
		}
	}
	if (!Shown)
	{
		return false;
	}
	if (!Camera)
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Spawn);
	}
	if (!Camera)
	{
		return false;
	}
	HoldStage(PC, true, false);
	const FVector Head = Shown->GetSocketLocation(TEXT("head"));
	const FVector Forward = Pawn->GetActorForwardVector().GetSafeNormal2D();
	const FVector Feet = Pawn->GetActorLocation() - FVector(0.0, 0.0, Pawn->GetSimpleCollisionHalfHeight());
	// Face: from the chin to the top of the skull (the head bone is at the base of the skull, the top about 22 cm
	// above it) from CameraDistanceCm; full length: from the feet to the top of the skull, from PlayerBodyDistanceCm.
	// First capture (14/11): aimed at the eyes the chin was cut, aimed at the middle of feet and head bone the head was.
	const FVector Top = Head + FVector(0.0, 0.0, 22.0);
	const FVector Look = bFace ? Head + FVector(0.0, 0.0, 4.0) : FVector(Head.X, Head.Y, (Feet.Z + Top.Z) * 0.5);
	const FVector Eye = Look + Forward * (bFace ? CameraDistanceCm : PlayerBodyDistanceCm);
	Camera->SetActorLocationAndRotation(Eye, (Look - Eye).Rotation());
	Camera->GetCameraComponent()->SetFieldOfView(bFace ? CameraFOV : PlayerBodyFOV);
	Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
	PC->SetViewTargetWithBlend(Camera, 0.f);
	UE_LOG(LogGTPFaceLineup, Log, TEXT("face lineup: %s, on %s of %s (mesh %s), head %.0f cm above the feet"), bFace ? TEXT("player face") : TEXT("player full length"),
		*Shown->GetName(), *GetNameSafe(Shown->GetOwner()), *GetNameSafe(Shown->GetSkeletalMeshAsset()), (Head - Feet).Z);
	return true;
}

void UGTPFaceLineupSubsystem::HoldStage(APlayerController* PC, bool bHold, bool bHidePlayer)
{
	if (bHold == bStageHeld || !PC)
	{
		return;
	}
	bStageHeld = bHold;
	if (bHold)
	{
		// The follow camera takes the view back every frame when it is not its own: paused while the portraits last.
		TArray<UGTPFollowCameraComponent*> Follows;
		PC->GetComponents<UGTPFollowCameraComponent>(Follows);
		APawn* Pawn = PC->GetPawn();
		if (Pawn && !bHidePlayer)
		{
			TArray<UGTPFollowCameraComponent*> OnPawn;
			Pawn->GetComponents<UGTPFollowCameraComponent>(OnPawn);
			Follows.Append(OnPawn);
		}
		else if (Pawn)
		{
			TArray<UGTPFollowCameraComponent*> OnPawn;
			Pawn->GetComponents<UGTPFollowCameraComponent>(OnPawn);
			Follows.Append(OnPawn);
			// The pawn and what rides on it (the player's visual is an actor attached to the pawn).
			TArray<AActor*> Riders;
			Pawn->GetAttachedActors(Riders, true, true);
			Riders.Add(Pawn);
			for (AActor* Rider : Riders)
			{
				if (Rider && !Rider->IsHidden())
				{
					Rider->SetActorHiddenInGame(true);
					HiddenActors.Add(Rider);
				}
			}
		}
		for (UGTPFollowCameraComponent* Follow : Follows)
		{
			if (Follow->IsComponentTickEnabled())
			{
				Follow->SetComponentTickEnabled(false);
				PausedCameras.Add(Follow);
			}
		}
		AHUD* Hud = PC->GetHUD();
		bHudWasShown = Hud && Hud->bShowHUD;
		if (Hud)
		{
			Hud->bShowHUD = false;
		}
		UE_LOG(LogGTPFaceLineup, Log, TEXT("face lineup: stage held, %d follow camera(s) paused, %d player actor(s) hidden, HUD %s"), PausedCameras.Num(),
			HiddenActors.Num(), Hud ? TEXT("hidden") : TEXT("none"));
		return;
	}
	for (const TWeakObjectPtr<UActorComponent>& Paused : PausedCameras)
	{
		if (UActorComponent* Follow = Paused.Get())
		{
			Follow->SetComponentTickEnabled(true);
		}
	}
	PausedCameras.Reset();
	for (const TWeakObjectPtr<AActor>& Hidden : HiddenActors)
	{
		if (AActor* Rider = Hidden.Get())
		{
			Rider->SetActorHiddenInGame(false);
		}
	}
	HiddenActors.Reset();
	if (AHUD* Hud = PC->GetHUD())
	{
		Hud->bShowHUD = bHudWasShown;
	}
	UE_LOG(LogGTPFaceLineup, Log, TEXT("face lineup: stage given back"));
}
