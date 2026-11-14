#include "GTPFaceLineup.h"

#include "GTPPedestrians.h"
#include "Animation/AnimInstance.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
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
	if (!FParse::Param(FCommandLine::Get(), TEXT("NicolitoFaces")) || !Super::ShouldCreateSubsystem(Outer))
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
		if (Pawn)
		{
			PC->SetViewTargetWithBlend(Pawn, 0.f);
		}
		if (Camera)
		{
			Camera->Destroy();
			Camera = nullptr;
		}
		UE_LOG(LogGTPFaceLineup, Log, TEXT("face lineup: done, %d head(s) shown%s"), FMath::Min(Current, Heads.Num()), Pawn ? TEXT("") : TEXT(" (no player pawn)"));
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
		// The head bone sits at the base of the skull: aim a little higher, at the eyes.
		const FVector Look = HeadAt + FVector(0.0, 0.0, 6.0);
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
	UE_LOG(LogGTPFaceLineup, Log, TEXT("face lineup: %s portrait requested: %s"), *Labels[Current], *File);
	World->GetTimerManager().SetTimer(Timer, this, &UGTPFaceLineupSubsystem::ShowNext, 1.f, false);
}
