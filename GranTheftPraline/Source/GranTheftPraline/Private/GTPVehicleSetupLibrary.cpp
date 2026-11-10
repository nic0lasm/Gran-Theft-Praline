#include "GTPVehicleSetupLibrary.h"

#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"

namespace
{
	FTransform ComponentSpaceRefPose(const FReferenceSkeleton& Skeleton, int32 BoneIndex)
	{
		FTransform Result = FTransform::Identity;
		const TArray<FTransform>& Pose = Skeleton.GetRefBonePose();
		while (BoneIndex != INDEX_NONE)
		{
			Result = Result * Pose[BoneIndex];
			BoneIndex = Skeleton.GetParentIndex(BoneIndex);
		}
		return Result;
	}

	FKBoxElem MakeBox(const FVector& Min, const FVector& Max)
	{
		const FVector Size = Max - Min;
		FKBoxElem Box(Size.X, Size.Y, Size.Z);
		Box.Center = (Min + Max) * 0.5f;
		return Box;
	}
}

FString UGTPVehicleSetupLibrary::BuildChassisPhysicsAsset(USkeletalMesh* Mesh, UPhysicsAsset* PhysicsAsset,
	FName RootBone, FString WheelBonePrefix)
{
	if (!Mesh || !PhysicsAsset)
	{
		return FString();
	}
	const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
	const int32 RootIndex = Skeleton.FindBoneIndex(RootBone);
	if (RootIndex == INDEX_NONE)
	{
		return FString();
	}

	// Wheel centres in component space.
	TArray<FVector> Wheels;
	for (int32 Index = 0; Index < Skeleton.GetNum(); ++Index)
	{
		if (Skeleton.GetBoneName(Index).ToString().StartsWith(WheelBonePrefix))
		{
			Wheels.Add(ComponentSpaceRefPose(Skeleton, Index).GetLocation());
		}
	}
	if (Wheels.Num() < 4)
	{
		return FString();
	}

	const FBox Bounds = Mesh->GetImportedBounds().GetBox();
	float WheelZ = 0.f, MaxWheelY = 0.f, FrontX = -BIG_NUMBER, RearX = BIG_NUMBER;
	for (const FVector& W : Wheels)
	{
		WheelZ += W.Z / Wheels.Num();
		MaxWheelY = FMath::Max(MaxWheelY, FMath::Abs(W.Y));
		FrontX = FMath::Max(FrontX, W.X);
		RearX = FMath::Min(RearX, W.X);
	}
	const float Ground = Bounds.Min.Z;
	const float Height = Bounds.Max.Z - Ground;
	const float Radius = WheelZ - Ground;
	// Body side = outer face of the tyres; the mirrors stay outside the box.
	const float HalfWidth = MaxWheelY + 0.3f * Radius;
	const float Belt = Ground + 0.62f * Height;
	const float Length = Bounds.Max.X - Bounds.Min.X;

	// Lower body: bumper to bumper, above the ground clearance.
	const FKBoxElem Lower = MakeBox(
		FVector(Bounds.Min.X + 2.f, -HalfWidth, Ground + 0.5f * Radius),
		FVector(Bounds.Max.X - 2.f, HalfWidth, Belt));
	// Cabin: from the rear window to the windscreen base, a little narrower.
	const FKBoxElem Cabin = MakeBox(
		FVector(Bounds.Min.X + 0.12f * Length, -0.86f * HalfWidth, Belt),
		FVector(FrontX - 0.15f * (FrontX - RearX), 0.86f * HalfWidth, Bounds.Max.Z - 1.f));

	// The root bone can sit and turn anywhere (Blender bones point up, so the
	// imported root is usually rotated): boxes are expressed in its space.
	// It can also be scaled (a Blender armature exported with FBX unit scale
	// carries x100 or x0.01): the body is scaled by it, so sizes are divided too.
	const FTransform RootPose = ComponentSpaceRefPose(Skeleton, RootIndex);
	const float RootScale = FMath::Max(RootPose.GetMaximumAxisScale(), UE_KINDA_SMALL_NUMBER);
	auto ToRoot = [&RootPose, RootScale](FKBoxElem Box)
	{
		Box.Center = RootPose.InverseTransformPosition(Box.Center);
		Box.Rotation = RootPose.GetRotation().Inverse().Rotator();
		Box.X /= RootScale;
		Box.Y /= RootScale;
		Box.Z /= RootScale;
		return Box;
	};

	PhysicsAsset->Modify();
	PhysicsAsset->SkeletalBodySetups.Empty();
	PhysicsAsset->ConstraintSetup.Empty();

	USkeletalBodySetup* Body = NewObject<USkeletalBodySetup>(PhysicsAsset, NAME_None, RF_Transactional);
	Body->BoneName = RootBone;
	Body->PhysicsType = EPhysicsType::PhysType_Default;
	Body->CollisionTraceFlag = ECollisionTraceFlag::CTF_UseSimpleAsComplex;
	Body->AggGeom.BoxElems.Add(ToRoot(Lower));
	Body->AggGeom.BoxElems.Add(ToRoot(Cabin));
	PhysicsAsset->SkeletalBodySetups.Add(Body);

	PhysicsAsset->UpdateBodySetupIndexMap();
	PhysicsAsset->UpdateBoundsBodiesArray();
	Body->InvalidatePhysicsData();
	Body->CreatePhysicsMeshes();
#if WITH_EDITOR
	PhysicsAsset->SetPreviewMesh(Mesh);
#endif
	Mesh->SetPhysicsAsset(PhysicsAsset);
	PhysicsAsset->MarkPackageDirty();
	Mesh->MarkPackageDirty();

	return FString::Printf(TEXT("wheels %d, radius %.1f cm, track %.1f cm, wheelbase %.1f cm, root scale %s, root rotation %s, lower box %s, cabin box %s"),
		Wheels.Num(), Radius, 2.f * MaxWheelY, FrontX - RearX, *RootPose.GetScale3D().ToString(), *RootPose.Rotator().ToString(),
		*FVector(Lower.X, Lower.Y, Lower.Z).ToString(), *FVector(Cabin.X, Cabin.Y, Cabin.Z).ToString());
}
