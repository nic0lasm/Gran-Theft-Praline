#include "GTPVehicle.h"

#include "Kismet/GameplayStatics.h"

#include "GTPCameraModeComponent.h"
#include "GTPCarjackVictim.h"
#include "GTPLog.h"
#include "GTPPlayerReaction.h"
#include "GTPEngineSoundComponent.h"
#include "GTPHornComponent.h"
#include "GTPTraffic.h"
#include "GTPPlayerController.h"
#include "GTPVehicleInput.h"
#include "GTPVehicleWheels.h"
#include "GTPWanted.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "AIController.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "ChaosVehicleWheel.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "ReferenceSkeleton.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "CollisionQueryParams.h"
#include "Engine/DamageEvents.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// Crimes of the player at the wheel go to the wanted level (GTPWanted.h).
namespace
{
	void ReportPlayerCrime(const AGTPVehicle* Vehicle, EGTPCrimeKind Kind, bool bVictimInvolved = false)
	{
		if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(Vehicle))
		{
			Wanted->ReportCrime(Kind, Vehicle->GetActorLocation(), bVictimInvolved, Vehicle);
		}
	}
}


DEFINE_LOG_CATEGORY_STATIC(LogGTPVehicle, Log, All);

namespace
{
	constexpr float CmPerSecToKmh = 0.036f;
	constexpr float ExitPollInterval = 0.1f;
	constexpr float MaxExitWait = 3.f;
	constexpr float MinTimeBetweenImpacts = 0.15f;

	EGTPDamageZone ZoneOfSide(EGTPVehicleSide Side)
	{
		return Side == EGTPVehicleSide::Left ? EGTPDamageZone::Left : EGTPDamageZone::Right;
	}

	FName ZoneTag(EGTPDamageZone Zone)
	{
		switch (Zone)
		{
		case EGTPDamageZone::Front: return TEXT("Front");
		case EGTPDamageZone::Rear: return TEXT("Rear");
		case EGTPDamageZone::Left: return TEXT("Left");
		default: return TEXT("Right");
		}
	}
}

AGTPVehicle::AGTPVehicle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	// The car turns with its wheels, never with the view.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// AWheeledVehiclePawn leaves the body kinematic (Epic's template turns
	// physics on in its Blueprint): without it Chaos never moves the car.
	GetMesh()->SetSimulatePhysics(true);

	ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
	ChaseArm->SetupAttachment(GetMesh());
	ChaseArm->bUsePawnControlRotation = true;
	ChaseArm->bInheritRoll = false;
	ChaseArm->bDoCollisionTest = true;
	ChaseArm->bEnableCameraLag = true;
	// Tight lag, capped: the car stays the same size on screen at speed.
	ChaseArm->CameraLagSpeed = 14.f;
	ChaseArm->CameraLagMaxDistance = 150.f;
	ChaseArm->bEnableCameraRotationLag = true;
	ChaseArm->CameraRotationLagSpeed = 10.f;

	ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
	ChaseCamera->SetupAttachment(ChaseArm, USpringArmComponent::SocketName);
	ChaseCamera->bUsePawnControlRotation = false;

	InteriorCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InteriorCamera"));
	InteriorCamera->SetupAttachment(GetMesh());
	InteriorCamera->bUsePawnControlRotation = true;
	InteriorCamera->bAutoActivate = false;

	EngineSound = CreateDefaultSubobject<UGTPEngineSoundComponent>(TEXT("EngineSound"));
	EngineSound->SetupAttachment(GetMesh());
	// Engine bay, front of the car.
	EngineSound->SetRelativeLocation(FVector(150.f, 0.f, 60.f));

	DentMorphTargets.Add(EGTPDamageZone::Front, TEXT("Dent_Front"));
	DentMorphTargets.Add(EGTPDamageZone::Rear, TEXT("Dent_Rear"));
	DentMorphTargets.Add(EGTPDamageZone::Left, TEXT("Dent_Left"));
	DentMorphTargets.Add(EGTPDamageZone::Right, TEXT("Dent_Right"));

	ApplyCityCarTuning();
}

UChaosWheeledVehicleMovementComponent* AGTPVehicle::GetWheeledMovement() const
{
	return Cast<UChaosWheeledVehicleMovementComponent>(GetVehicleMovementComponent());
}

void AGTPVehicle::ApplyCityCarTuning()
{
	// A current French city car: about 1,150 kg, 100 hp class, front-wheel
	// drive, 5-speed automatic for the player. Each model's Blueprint may
	// override any of this (docs/gameplay/vehicles.md, section 4).
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move)
	{
		return;
	}
	Move->Mass = 1150.f;
	Move->ChassisWidth = 175.f;
	Move->ChassisHeight = 145.f;
	Move->DragCoefficient = 0.32f;
	Move->DownforceCoefficient = 0.1f;
	// Brake held while stopped reverses, as in most open-world games.
	Move->bReverseAsBrake = true;
	// Traffic and police cars are never possessed: their driver is a seated
	// character. With the engine default (inputs need a local controller) Chaos
	// replaces the pedals of an unpossessed car with its replicated state, zero,
	// so the car idles in neutral at 850 rpm whatever SetThrottleInput asks.
	Move->SetRequiresControllerForInputs(false);

	Move->WheelSetups.SetNum(4);
	Move->WheelSetups[0].WheelClass = UGTPWheelFront::StaticClass();
	Move->WheelSetups[0].BoneName = TEXT("Phys_Wheel_FL");
	Move->WheelSetups[1].WheelClass = UGTPWheelFront::StaticClass();
	Move->WheelSetups[1].BoneName = TEXT("Phys_Wheel_FR");
	Move->WheelSetups[2].WheelClass = UGTPWheelRear::StaticClass();
	Move->WheelSetups[2].BoneName = TEXT("Phys_Wheel_BL");
	Move->WheelSetups[3].WheelClass = UGTPWheelRear::StaticClass();
	Move->WheelSetups[3].BoneName = TEXT("Phys_Wheel_BR");

	// Chaos has no drivetrain losses: about 130 Nm at the wheels' input gives
	// a real 100 hp city car's pace (0-50 km/h in about 4 s, 0-100 in about 11 s).
	Move->EngineSetup.MaxTorque = 130.f;
	Move->EngineSetup.MaxRPM = 6000.f;
	Move->EngineSetup.EngineIdleRPM = 850.f;
	Move->EngineSetup.EngineBrakeEffect = 0.2f;
	if (FRichCurve* Torque = Move->EngineSetup.TorqueCurve.GetRichCurve())
	{
		Torque->Reset();
		Torque->AddKey(0.f, 0.55f);
		Torque->AddKey(1800.f, 1.f);
		Torque->AddKey(4500.f, 0.95f);
		Torque->AddKey(6000.f, 0.7f);
	}

	Move->DifferentialSetup.DifferentialType = EVehicleDifferential::FrontWheelDrive;

	Move->TransmissionSetup.bUseAutomaticGears = true;
	Move->TransmissionSetup.bUseAutoReverse = true;
	Move->TransmissionSetup.FinalRatio = 4.1f;
	Move->TransmissionSetup.ForwardGearRatios = {3.73f, 2.05f, 1.39f, 1.03f, 0.82f};
	Move->TransmissionSetup.ReverseGearRatios = {3.55f};
	Move->TransmissionSetup.ChangeUpRPM = 4800.f;
	Move->TransmissionSetup.ChangeDownRPM = 2000.f;
	Move->TransmissionSetup.GearChangeTime = 0.25f;

	// Less steering at speed so the car stays drivable on a keyboard.
	if (FRichCurve* Steering = Move->SteeringSetup.SteeringCurve.GetRichCurve())
	{
		Steering->Reset();
		Steering->AddKey(0.f, 1.f);
		Steering->AddKey(30.f, 0.75f);
		Steering->AddKey(80.f, 0.3f);
		Steering->AddKey(120.f, 0.22f);
	}
}

namespace GTPWheelBones
{
	struct FCandidate
	{
		FName Name;
		FVector Position;
		int32 Depth = 0;
	};

	/**
	 * The four wheel bones of a car mesh, in the order of AGTPVehicle's wheel setups (front left,
	 * front right, rear left, rear right), from the bones named like a wheel (wheel, whl, tire, tyre;
	 * not steer or spare) at the four corners of their reference pose (+X forward, +Y right). In a
	 * corner: a "phys" bone first, then the deepest in the hierarchy (the one that spins), then the
	 * shortest name. Empty when a corner has none; OutReport says what was seen.
	 */
	TArray<FName> Pick(USkinnedAsset& Mesh, FString& OutReport)
	{
		const FReferenceSkeleton& Skeleton = Mesh.GetRefSkeleton();
		TArray<FCandidate> Candidates;
		FBox Extent(ForceInit);
		for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetNum(); ++BoneIndex)
		{
			const FName BoneName = Skeleton.GetBoneName(BoneIndex);
			const FString Lower = BoneName.ToString().ToLower();
			const bool bWheelName = Lower.Contains(TEXT("wheel")) || Lower.Contains(TEXT("whl")) || Lower.Contains(TEXT("tire")) || Lower.Contains(TEXT("tyre"));
			if (!bWheelName || Lower.Contains(TEXT("steer")) || Lower.Contains(TEXT("spare")))
			{
				continue;
			}
			FCandidate Candidate;
			Candidate.Name = BoneName;
			Candidate.Position = Mesh.GetComposedRefPoseMatrix(BoneName).GetOrigin();
			for (int32 Parent = Skeleton.GetParentIndex(BoneIndex); Parent != INDEX_NONE; Parent = Skeleton.GetParentIndex(Parent))
			{
				++Candidate.Depth;
			}
			Candidates.Add(Candidate);
			Extent += Candidate.Position;
			OutReport += FString::Printf(TEXT(" %s (%.0f %.0f %.0f)"), *BoneName.ToString(), Candidate.Position.X, Candidate.Position.Y, Candidate.Position.Z);
		}
		TArray<FName> Picked;
		if (Candidates.Num() < 4)
		{
			OutReport = FString::Printf(TEXT("%d bone(s) named like a wheel:%s; the mesh's first bones:"), Candidates.Num(), *OutReport);
			for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetNum() && BoneIndex < 80; ++BoneIndex)
			{
				OutReport += TEXT(" ") + Skeleton.GetBoneName(BoneIndex).ToString();
			}
			return Picked;
		}
		const FVector Centre = Extent.GetCenter();
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			const bool bFront = Corner < 2;
			const bool bLeft = Corner % 2 == 0;
			const FCandidate* Best = nullptr;
			for (const FCandidate& Candidate : Candidates)
			{
				const FVector Offset = Candidate.Position - Centre;
				if ((Offset.X > 0.0) != bFront || (Offset.Y < 0.0) != bLeft || FMath::Abs(Offset.X) < 50.0 || FMath::Abs(Offset.Y) < 30.0)
				{
					continue;
				}
				auto IsBetter = [](const FCandidate& A, const FCandidate& B)
				{
					const bool bPhysA = A.Name.ToString().ToLower().Contains(TEXT("phys"));
					const bool bPhysB = B.Name.ToString().ToLower().Contains(TEXT("phys"));
					if (bPhysA != bPhysB)
					{
						return bPhysA;
					}
					if (A.Depth != B.Depth)
					{
						return A.Depth > B.Depth;
					}
					return A.Name.ToString().Len() < B.Name.ToString().Len();
				};
				if (!Best || IsBetter(Candidate, *Best))
				{
					Best = &Candidate;
				}
			}
			if (!Best)
			{
				OutReport = FString::Printf(TEXT("no bone at corner %d among:%s"), Corner, *OutReport);
				Picked.Reset();
				return Picked;
			}
			Picked.Add(Best->Name);
		}
		OutReport = FString::Printf(TEXT("FL %s, FR %s, BL %s, BR %s (bones named like a wheel:%s)"), *Picked[0].ToString(), *Picked[1].ToString(),
			*Picked[2].ToString(), *Picked[3].ToString(), *OutReport);
		return Picked;
	}

	/**
	 * Centre of the root body's shapes (the body Chaos drives: the first bone with a body), weighted by
	 * their volume as Chaos weighs them at one density, in that body's space; before any physics exists.
	 * Convex shapes count by their bounds. False without a physics asset or shapes.
	 */
	// City Sample's own centre of mass for its cars: CenterOfMassOverride of BP_veh*_vehicleNN_Sandbox,
	// whose COMNudge is zero (read on 19/11 by tools/vehicles/read_uasset_props.py, the Blueprints do not
	// load in our project, so make_epic_vehicles.py could not copy it). In the root body's frame, cm.
	bool CitySampleCentreOfMass(const FString& ClassName, FVector& OutCentre)
	{
		struct FEntry
		{
			const TCHAR* Prefix;
			FVector Centre;
		};
		static const FEntry Entries[] = {
			{ TEXT("BP_GTP_Van01"), FVector(33.0, 0.0, 40.0) },
			{ TEXT("BP_GTP_Car02"), FVector(0.0, 0.0, 54.0) },
			{ TEXT("BP_GTP_Car03"), FVector(26.0, 0.0, 56.0) },
			{ TEXT("BP_GTP_Car05"), FVector(38.0, 0.0, 60.0) },
			{ TEXT("BP_GTP_Car07"), FVector(30.0, 0.0, 50.0) },
			{ TEXT("BP_GTP_Car12"), FVector(26.0, 0.0, 56.0) },
			{ TEXT("BP_GTP_Police13"), FVector(26.0, 0.0, 56.0) },
		};
		for (const FEntry& Entry : Entries)
		{
			if (ClassName.StartsWith(Entry.Prefix))
			{
				OutCentre = Entry.Centre;
				return true;
			}
		}
		return false;
	}

	bool RootBodyCentre(USkeletalMeshComponent& Body, USkinnedAsset& Mesh, FVector& OutCentre)
	{
		const UPhysicsAsset* Physics = Body.GetPhysicsAsset();
		if (!Physics)
		{
			return false;
		}
		const FReferenceSkeleton& Skeleton = Mesh.GetRefSkeleton();
		const UBodySetup* Root = nullptr;
		for (int32 BoneIndex = 0; BoneIndex < Skeleton.GetNum() && !Root; ++BoneIndex)
		{
			const int32 BodyIndex = Physics->FindBodyIndex(Skeleton.GetBoneName(BoneIndex));
			if (BodyIndex != INDEX_NONE && Physics->SkeletalBodySetups.IsValidIndex(BodyIndex))
			{
				Root = Physics->SkeletalBodySetups[BodyIndex].Get();
			}
		}
		if (!Root)
		{
			return false;
		}
		double Volume = 0.0;
		FVector Sum = FVector::ZeroVector;
		auto Add = [&Volume, &Sum](const FVector& Centre, double ElemVolume)
		{
			if (ElemVolume > 0.0)
			{
				Sum += Centre * ElemVolume;
				Volume += ElemVolume;
			}
		};
		const FKAggregateGeom& Geom = Root->AggGeom;
		for (const FKBoxElem& Elem : Geom.BoxElems)
		{
			Add(Elem.Center, static_cast<double>(Elem.X) * Elem.Y * Elem.Z);
		}
		for (const FKSphereElem& Elem : Geom.SphereElems)
		{
			Add(Elem.Center, 4.0 / 3.0 * UE_DOUBLE_PI * FMath::Cube(static_cast<double>(Elem.Radius)));
		}
		for (const FKSphylElem& Elem : Geom.SphylElems)
		{
			const double R = Elem.Radius;
			Add(Elem.Center, UE_DOUBLE_PI * R * R * Elem.Length + 4.0 / 3.0 * UE_DOUBLE_PI * R * R * R);
		}
		for (const FKTaperedCapsuleElem& Elem : Geom.TaperedCapsuleElems)
		{
			const double R = 0.5 * (Elem.Radius0 + Elem.Radius1);
			Add(Elem.Center, UE_DOUBLE_PI * R * R * Elem.Length + 4.0 / 3.0 * UE_DOUBLE_PI * R * R * R);
		}
		for (const FKConvexElem& Elem : Geom.ConvexElems)
		{
			Add(Elem.GetTransform().TransformPosition(Elem.ElemBox.GetCenter()), Elem.ElemBox.GetVolume());
		}
		if (Volume <= 0.0)
		{
			return false;
		}
		OutCentre = Sum / Volume;
		return true;
	}
}

void AGTPVehicle::PreRegisterAllComponents()
{
	Super::PreRegisterAllComponents();
	FindWheelBones();
}

void AGTPVehicle::FindWheelBones()
{
	// Chaos puts a wheel at its bone's reference pose position (LocateBoneOffset); a bone the mesh
	// does not have reads as the origin, so all four springs sat at one point under the City Sample
	// cars (probe of candidate 46: wheelbase 0 cm, springs at 0-14 % of the weight, no wheel down).
	const UWorld* World = GetWorld();
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Body = GetMesh();
	USkinnedAsset* Asset = Body ? Body->GetSkinnedAsset() : nullptr;
	if (!World || !World->IsGameWorld() || !Move || !Asset || Move->WheelSetups.Num() != 4)
	{
		return;
	}
	bool bMissing = false;
	for (const FChaosWheelSetup& WheelSetup : Move->WheelSetups)
	{
		bMissing |= Asset->GetRefSkeleton().FindBoneIndex(WheelSetup.BoneName) == INDEX_NONE;
	}
	if (!bMissing)
	{
		return;
	}
	// MassTraffic's simple physics for the cars near the player is mined from this car. Left as at
	// candidate 46 (candidates 48 to 51), its four wheels sat at one point under a high centre of mass
	// and those cars fell over (5 of the 6 probed at the candidate 49 gate lay on their side, still
	// counted as moving). It now gets the real wheels and City Sample's centre of mass, as the cars do;
	// -MassTemplate46 gives back the stopgap.
	static const bool bTemplate46 = FParse::Param(FCommandLine::Get(), TEXT("MassTemplate46"));
	if (!bTemplate46 && IsPhysicsTemplateSpawn())
	{
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle physics template: %s (%s) taken for MassTraffic's physics template, given the real wheels and City Sample's centre of mass"),
			*GetName(), *GetClass()->GetName());
	}
	else if (IsPhysicsTemplateSpawn())
	{
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle physics template: %s (%s) spawned at %s with no owner and no instigator, taken for MassTraffic's physics template (PhysicsVehicleTemplateActor); left as at candidate 46 (-MassTemplate46): wheel bones %s %s %s %s kept (missing from mesh %s), centre of mass override %s"),
			*GetName(), *GetClass()->GetName(), *GetRootComponent()->GetRelativeLocation().ToCompactString(),
			*Move->WheelSetups[0].BoneName.ToString(), *Move->WheelSetups[1].BoneName.ToString(),
			*Move->WheelSetups[2].BoneName.ToString(), *Move->WheelSetups[3].BoneName.ToString(),
			*Asset->GetName(), Move->bEnableCenterOfMassOverride ? TEXT("ON (set in the Blueprint)") : TEXT("off"));
		return;
	}
	static TMap<FName, TArray<FName>> ByClass;
	const FName ClassKey = GetClass()->GetFName();
	if (!ByClass.Contains(ClassKey))
	{
		FString Report;
		ByClass.Add(ClassKey, GTPWheelBones::Pick(*Asset, Report));
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle wheels: %s (mesh %s) has no bone %s: %s%s"), *GetClass()->GetName(), *Asset->GetName(),
			*Move->WheelSetups[0].BoneName.ToString(), ByClass[ClassKey].Num() == 4 ? TEXT("wheel bones found by name and position, ") : TEXT("NO WHEEL BONES, the wheels stay at the origin; "),
			*Report);
	}
	const TArray<FName>& Picked = ByClass[ClassKey];
	if (Picked.Num() == 4)
	{
		for (int32 Index = 0; Index < 4; ++Index)
		{
			Move->WheelSetups[Index].BoneName = Picked[Index];
		}
		bWheelBonesFromMesh = true;
		// The centre of mass too, before the vehicle exists: Chaos applies the override when it creates
		// it (SetupVehicleMass), and MassTraffic's simple physics, which it extracts from a car of this
		// class spawned for that (PhysicsVehicleTemplateActor), gets it as well; set later, from the
		// first tick, it reached the actors only (candidate 48).
		FVector Centre;
		static TSet<FName> CitySampleLogged;
		if (!Move->bEnableCenterOfMassOverride && GTPWheelBones::CitySampleCentreOfMass(GetClass()->GetName(), Centre))
		{
			// City Sample's own value (candidate 49): 26 cm ahead of the origin and 56 cm up for the
			// saloons, where the fallback below puts it over the origin at the chassis shapes' height
			// (83 cm for Police13), which lifts the nose under throttle and rolls the car in turns.
			Move->bEnableCenterOfMassOverride = true;
			Move->CenterOfMassOverride = Centre;
			if (!CitySampleLogged.Contains(ClassKey))
			{
				CitySampleLogged.Add(ClassKey);
				GTP_TRACE(LogGTPVehicle, TEXT("vehicle balance: %s centre of mass at City Sample's own override %s (its Sandbox Blueprint), set before the vehicle is created"),
					*GetClass()->GetName(), *Centre.ToCompactString());
			}
		}
		else if (!Move->bEnableCenterOfMassOverride && GTPWheelBones::RootBodyCentre(*Body, *Asset, Centre))
		{
			Move->bEnableCenterOfMassOverride = true;
			Move->CenterOfMassOverride = FVector(0.f, 0.f, Centre.Z);
			static TSet<FName> BalanceLogged;
			if (!BalanceLogged.Contains(ClassKey))
			{
				BalanceLogged.Add(ClassKey);
				GTP_TRACE(LogGTPVehicle, TEXT("vehicle balance: %s centre of mass moved from X %.0f Y %.0f (its chassis body's shapes) to over the origin, where Chaos shares the weight between the springs; height %.0f cm; set before the vehicle is created"),
					*GetClass()->GetName(), Centre.X, Centre.Y, Centre.Z);
			}
		}
	}
}

bool AGTPVehicle::IsPhysicsTemplateSpawn() const
{
	// UMassTrafficSubsystem::GetOrExtractVehiclePhysicsTemplate spawns the PhysicsVehicleTemplateActor
	// class with default spawn parameters (identity transform, no owner, no instigator), reads its
	// vehicle and destroys it. Our own spawns (player test car, police, parked cars, legacy traffic)
	// are all placed in the street.
	const USceneComponent* Root = GetRootComponent();
	if (!Root || Root->GetAttachParent() || GetOwner() || GetInstigator())
	{
		return false;
	}
	const AActor* Default = GetClass()->GetDefaultObject<AActor>();
	const USceneComponent* DefaultRoot = Default ? Default->GetRootComponent() : nullptr;
	const FVector Expected = DefaultRoot ? DefaultRoot->GetRelativeLocation() : FVector::ZeroVector;
	return Root->GetRelativeLocation().Equals(Expected, 1.0);
}

void AGTPVehicle::CentreMassOverOrigin()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Body = GetMesh();
	FBodyInstance* Chassis = Body ? Body->GetBodyInstance() : nullptr;
	if (!bWheelBonesFromMesh || !Move || Move->bEnableCenterOfMassOverride || !Chassis || !Chassis->IsValidBodyInstance())
	{
		return;
	}
	// The body's own centre of mass, in its local space, without the physics asset's nudge (Chaos adds
	// COMNudge to the override, UChaosVehicleMovementComponent::UpdateMassProperties).
	const FVector Own = Chassis->GetMassSpaceLocal().GetTranslation() - Chassis->COMNudge;
	Move->bEnableCenterOfMassOverride = true;
	Move->CenterOfMassOverride = FVector(0.f, 0.f, Own.Z);
	// Recalculated mass properties call Chaos back, which applies the override.
	Chassis->UpdateMassProperties();
	static TSet<FName> Logged;
	if (!Logged.Contains(GetClass()->GetFName()))
	{
		Logged.Add(GetClass()->GetFName());
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle balance: %s centre of mass moved from X %.0f Y %.0f to over the origin, where Chaos shares the weight between the springs; height %.0f cm"),
			*GetClass()->GetName(), Own.X, Own.Y, Own.Z);
	}
}

void AGTPVehicle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	for (UStaticMeshComponent* Old : GlassComponents)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	GlassComponents.Reset();

	USkeletalMeshComponent* Body = GetMesh();
	for (const TPair<EGTPDamageZone, TObjectPtr<UStaticMesh>>& Pair : GlassMeshes)
	{
		if (!Pair.Value || !Body)
		{
			continue;
		}
		UStaticMeshComponent* Glass = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		Glass->SetStaticMesh(Pair.Value);
		Glass->SetupAttachment(Body);
		Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Glass->SetCastShadow(false);
		Glass->ComponentTags = {TEXT("Glass"), ZoneTag(Pair.Key)};
		Glass->RegisterComponent();
		GlassComponents.Add(Glass);
	}
}

void AGTPVehicle::BeginPlay()
{
	Super::BeginPlay();
	Health = MaxHealth;

	ChaseArm->SetRelativeLocation(FVector(0.f, 0.f, ChasePivotHeight));
	ChaseArm->TargetArmLength = ChaseDistance;
	ChaseArm->SocketOffset = FVector(0.f, 0.f, ChaseHeight);
	ChaseCamera->SetFieldOfView(BaseFOV);
	InteriorCamera->SetRelativeLocation(InteriorEyeOffset);
	InteriorCamera->SetFieldOfView(InteriorFOV);

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetNotifyRigidBodyCollision(true);
		Body->OnComponentHit.AddDynamic(this, &AGTPVehicle::OnBodyHit);
	}
	KeepChassisBodyOnly();
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		// Only the wheels are animated, and Chaos does not read them back: no pose off screen,
		// fewer updates when small on screen (89 skeletal mesh ticks a frame on candidate 31).
		Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		Body->bEnableUpdateRateOptimizations = true;
	}
	// The chase camera's arm only works for the player's car (PossessedBy turns it on).
	ChaseArm->SetComponentTickEnabled(Cast<APlayerController>(GetController()) != nullptr);

	// Parked cars do not roll away.
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		// Again here in case a Blueprint turned it back on (see ApplyCityCarTuning).
		Move->SetRequiresControllerForInputs(false);
		Move->SetHandbrakeInput(true);
	}
	ApplyModelProfile();
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		BaseTorque = Move->EngineSetup.MaxTorque;
		if (Move->Wheels.Num() > 2 && Move->Wheels[2])
		{
			RearGripBase = Move->Wheels[2]->FrictionForceMultiplier;
			RearGripNow = RearGripBase;
		}
	}
	// The arcade boost is the city car's (PlayerTorque for 1 150 kg, the class default mass) per
	// kilogram: a heavier car gets as much per kilogram, a car stronger than that keeps its own torque.
	// Scaled by torque instead (until 16/11), Car05 got 785 N.m once its wheels held the road:
	// 0-50 km/h in 1.4 s, 0-100 in 4.8 s (candidate 47).
	const UChaosWheeledVehicleMovementComponent* Movement = GetWheeledMovement();
	const float MassKg = Movement && Movement->Mass > 0.f ? Movement->Mass : 1150.f;
	Assist.PlayerTorque = FMath::Max(BaseTorque, PlayerTorque * FMath::Max(1.f, MassKg / 1150.f));
	Assist.DriftRearGrip = DriftRearGrip;
	Assist.DriftYawAccel = DriftYawAccel;
	Assist.StabilityDamping = StabilityDamping;
}

void AGTPVehicle::ApplyModelProfile()
{
	// Driving profiles of docs/gameplay/vehicles.md section 4, by model, for the models
	// whose Blueprint (tools/vehicles/unreal/import_vehicle.py) keeps the city car tuning.
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Body = GetMesh();
	if (!Move || !Body)
	{
		return;
	}
	const FString Model = GetClass()->GetName();
	float Torque = 0.f;
	float MassKg = 0.f;
	if (Model.Contains(TEXT("Kangoo")) || Model.Contains(TEXT("Berlingo")) || Model.Contains(TEXT("Trafic")))
	{
		// Van: heavier, slower, long braking.
		Torque = 200.f;
		MassKg = 1500.f;
	}
	else if (Model.Contains(TEXT("208")))
	{
		Torque = 140.f;
		MassKg = 1160.f;
	}
	if (Torque > 0.f)
	{
		Move->EngineSetup.MaxTorque = Torque;
		Move->SetMaxEngineTorque(Torque);
	}
	if (MassKg > 0.f)
	{
		Body->SetMassOverrideInKg(NAME_None, MassKg, true);
	}
}

bool AGTPVehicle::IsEngineRunning() const
{
	const UWorld* World = GetWorld();
	return Driver != nullptr && !bWrecked && (!World || World->GetTimeSeconds() >= EngineStartUntil);
}

void AGTPVehicle::RequestAIStop(float Seconds)
{
	if (const UWorld* World = GetWorld())
	{
		AIStopUntil = FMath::Max(AIStopUntil, World->GetTimeSeconds() + Seconds);
	}
}

bool AGTPVehicle::IsAIStopRequested() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < AIStopUntil;
}

float AGTPVehicle::GetSpeedKmh() const
{
	const UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	return Move ? Move->GetForwardSpeed() * CmPerSecToKmh : 0.f;
}

float AGTPVehicle::GetEngineRpm() const
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	return Move ? Move->GetEngineRotationSpeed() : 0.f;
}

int32 AGTPVehicle::GetGear() const
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	return Move ? Move->GetCurrentGear() : 0;
}

void AGTPVehicle::SetAIDriveInput(float Throttle, float Brake, float Steering, bool bHandbrake, bool bReverse)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || !HasAIDriver() || bWrecked)
	{
		return;
	}
	// The brake reverses a stopped car only when the AI asks for it: an AI car held on
	// the brake at a light or in a queue otherwise backed away (front-wheel drive, the
	// handbrake holds only the rear wheels). The player's pedals keep reverse-as-brake.
	Move->bReverseAsBrake = bReverse;
	// Out of reverse when the AI no longer asks for it: the automatic gearbox keeps gear -1
	// after backing out, and the throttle then drove the car backwards (-45 km/h measured).
	if (!bReverse && Move->GetCurrentGear() < 0)
	{
		Move->SetTargetGear(Throttle > 0.05f ? 1 : 0, true);
	}
	ThrottleAmount = FMath::Clamp(Throttle, 0.f, 1.f);
	Move->SetThrottleInput(ThrottleAmount);
	Move->SetBrakeInput(FMath::Clamp(Brake, 0.f, 1.f));
	Move->SetSteeringInput(FMath::Clamp(Steering, -1.f, 1.f));
	Move->SetHandbrakeInput(bHandbrake);

	// A car pulling away from a stop: a sleeping body ignores the pedals, and a
	// gearbox left in neutral by the ghost mode or a parked state stays there.
	if (ThrottleAmount > 0.05f && Brake < 0.05f && FMath::Abs(GetSpeedKmh()) < 1.f)
	{
		if (USkeletalMeshComponent* Body = GetMesh())
		{
			if (Body->IsSimulatingPhysics() && !Body->IsAnyRigidBodyAwake())
			{
				Body->WakeAllRigidBodies();
			}
		}
		if (Move->GetCurrentGear() == 0)
		{
			Move->SetTargetGear(1, true);
		}
	}
	TraceDriveDiag(Brake, bHandbrake);
}

bool AGTPVehicle::IsDriveDiagWanted()
{
	static const bool bWanted = []
	{
		int32 Switch = 0;
		FParse::Value(FCommandLine::Get(), TEXT("DriveDiag="), Switch);
		return Switch != 0;
	}();
	return bWanted;
}

void AGTPVehicle::TraceDriveDiag(float Brake, bool bHandbrake)
{
	// -DriveDiag=1: every 5 s, the drivetrain of each AI-driven car within 60 m
	// of the player (gear, rpm, speed, pedals, controller, physics state).
	if (!IsDriveDiagWanted())
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() - LastDriveDiagTime < 5.f)
	{
		return;
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player || FVector::DistSquared(Player->GetActorLocation(), GetActorLocation()) > FMath::Square(6000.f))
	{
		return;
	}
	LastDriveDiagTime = World->GetTimeSeconds();
	const UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Body = GetMesh();
	UE_LOG(LogGTPVehicle, Log, TEXT("drive diag %s: gear=%d rpm=%.0f kmh=%.1f throttle=%.2f brake=%.2f handbrake=%d controller=%d simulating=%d awake=%d movetick=%d"),
		*GetName(), Move ? Move->GetCurrentGear() : -99, GetEngineRpm(), GetSpeedKmh(), ThrottleAmount, Brake, bHandbrake ? 1 : 0,
		GetController() ? 1 : 0, Body && Body->IsSimulatingPhysics() ? 1 : 0, Body && Body->IsAnyRigidBodyAwake() ? 1 : 0,
		Move && Move->IsComponentTickEnabled() ? 1 : 0);
}

float AGTPVehicle::ChassisClearanceCm(USkeletalMeshComponent& Body) const
{
	const FBodyInstance* Chassis = Body.GetBodyInstance();
	UWorld* World = GetWorld();
	if (!Chassis || !Chassis->IsValidBodyInstance() || !World)
	{
		return -9999.f;
	}
	const FBox Box = Chassis->GetBodyBounds();
	// Static geometry only, without the car's seated driver: a trace on the visibility channel from
	// the box's centre can stop on the driver and read the box below the ground (smoke of bac7e06).
	FCollisionQueryParams Params(TEXT("GTPDriveProbe"), false, this);
	if (Driver)
	{
		Params.AddIgnoredActor(Driver.Get());
	}
	FHitResult Ground;
	const FVector Center = Box.GetCenter();
	if (!World->LineTraceSingleByObjectType(Ground, Center, Center - FVector(0.f, 0.f, 500.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return -9999.f;
	}
	return static_cast<float>(Box.Min.Z - Ground.ImpactPoint.Z);
}

FBox AGTPVehicle::CollisionBoxInComponent(USkeletalMeshComponent& Body, const FBodyInstance& Part) const
{
	const UBodySetup* Setup = Part.GetBodySetup();
	if (!Setup)
	{
		return FBox(ForceInit);
	}
	const int32 BoneIndex = Body.GetBoneIndex(Setup->BoneName);
	const FTransform BoneInComponent = BoneIndex != INDEX_NONE ? Body.GetBoneTransform(BoneIndex, FTransform::Identity) : FTransform::Identity;
	return Setup->AggGeom.CalcAABB(BoneInComponent);
}

float AGTPVehicle::WheelContactZ(const UChaosWheeledVehicleMovementComponent& Move, USkeletalMeshComponent& Body, float& OutMaxRaise) const
{
	OutMaxRaise = 0.f;
	float Lowest = TNumericLimits<float>::Max();
	for (const FChaosWheelSetup& WheelSetup : Move.WheelSetups)
	{
		const UChaosVehicleWheel* Wheel = WheelSetup.WheelClass ? WheelSetup.WheelClass->GetDefaultObject<UChaosVehicleWheel>() : nullptr;
		const int32 BoneIndex = Body.GetBoneIndex(WheelSetup.BoneName);
		if (!Wheel || BoneIndex == INDEX_NONE)
		{
			continue;
		}
		const float CentreZ = static_cast<float>(Body.GetBoneTransform(BoneIndex, FTransform::Identity).GetLocation().Z + WheelSetup.AdditionalOffset.Z);
		Lowest = FMath::Min(Lowest, CentreZ - Wheel->WheelRadius);
		OutMaxRaise = FMath::Max(OutMaxRaise, Wheel->SuspensionMaxRaise);
	}
	return Lowest == TNumericLimits<float>::Max() ? 0.f : Lowest;
}

void AGTPVehicle::LogCollisionBodies(USkeletalMeshComponent& Body) const
{
	// A body that reaches below the wheels' contact plane plus the suspension's raise rests on the
	// road as soon as the springs compress (probe of candidate 45: chassis box under the ground).
	const UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	float MaxRaise = 0.f;
	const float ContactZ = Move ? WheelContactZ(*Move, Body, MaxRaise) : 0.f;
	const FBodyInstance* Chassis = Body.GetBodyInstance();
	for (int32 Index = 0; Index < Body.Bodies.Num(); ++Index)
	{
		const FBodyInstance* Part = Body.Bodies[Index];
		const UBodySetup* Setup = Part ? Part->GetBodySetup() : nullptr;
		if (!Setup)
		{
			continue;
		}
		const FBox Box = CollisionBoxInComponent(Body, *Part);
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle collision: %s body %d/%d bone %s%s: %d box, %d sphere, %d capsule, %d convex, from %.0f to %.0f cm above the wheels' contact plane (Z %.0f, max raise %.0f cm), %.0f x %.0f cm"),
			*GetClass()->GetName(), Index + 1, Body.Bodies.Num(), *Setup->BoneName.ToString(), Part == Chassis ? TEXT(" (chassis)") : TEXT(""),
			Setup->AggGeom.BoxElems.Num(), Setup->AggGeom.SphereElems.Num(), Setup->AggGeom.SphylElems.Num(), Setup->AggGeom.ConvexElems.Num(),
			Box.IsValid ? Box.Min.Z - ContactZ : 0.0, Box.IsValid ? Box.Max.Z - ContactZ : 0.0, ContactZ, MaxRaise, Box.GetSize().X, Box.GetSize().Y);
	}
}

void AGTPVehicle::LogDriveConfig(UChaosWheeledVehicleMovementComponent& Move, USkeletalMeshComponent& Body) const
{
	// Which wheels the engine drives: Chaos decides it from the differential for a wheel with an axle
	// (UChaosWheeledVehicleSimulation::SetupVehicle), from bAffectedByEngine otherwise.
	const EVehicleDifferential Differential = Move.DifferentialSetup.DifferentialType;
	const TCHAR* DifferentialName = Differential == EVehicleDifferential::AllWheelDrive ? TEXT("all wheel drive")
		: Differential == EVehicleDifferential::FrontWheelDrive ? TEXT("front wheel drive")
		: Differential == EVehicleDifferential::RearWheelDrive ? TEXT("rear wheel drive") : TEXT("undefined");
	FString Wheels;
	double FrontX = 0.0;
	double RearX = 0.0;
	int32 Fronts = 0;
	int32 Rears = 0;
	for (int32 Index = 0; Index < Move.Wheels.Num() && Index < Move.WheelSetups.Num(); ++Index)
	{
		const UChaosVehicleWheel* Wheel = Move.Wheels[Index];
		if (!Wheel)
		{
			continue;
		}
		const bool bFront = Wheel->AxleType == EAxleType::Front;
		const bool bRear = Wheel->AxleType == EAxleType::Rear;
		const bool bPowered = bFront ? (Differential == EVehicleDifferential::AllWheelDrive || Differential == EVehicleDifferential::FrontWheelDrive)
			: bRear ? (Differential == EVehicleDifferential::AllWheelDrive || Differential == EVehicleDifferential::RearWheelDrive)
			: Wheel->bAffectedByEngine;
		const FVector Local = Body.GetBoneLocation(Move.WheelSetups[Index].BoneName, EBoneSpaces::ComponentSpace);
		if (bFront)
		{
			FrontX += Local.X;
			++Fronts;
		}
		else if (bRear)
		{
			RearX += Local.X;
			++Rears;
		}
		Wheels += FString::Printf(TEXT(" | %s %s%s %s radius %.1f friction %.2f cornering %.0f spring %.0f preload %.0f raise %.0f drop %.0f load ratio %.2f traction control %d at X %.0f Z %.0f"),
			*Move.WheelSetups[Index].BoneName.ToString(), bFront ? TEXT("front") : bRear ? TEXT("rear") : TEXT("no axle"), bPowered ? TEXT(" powered") : TEXT(""),
			*Wheel->GetClass()->GetName(),
			Wheel->WheelRadius, Wheel->FrictionForceMultiplier, Wheel->CorneringStiffness, Wheel->SpringRate, Wheel->SpringPreload,
			Wheel->SuspensionMaxRaise, Wheel->SuspensionMaxDrop, Wheel->WheelLoadRatio, Wheel->bTractionControlEnabled ? 1 : 0, Local.X, Local.Z);
	}
	FrontX = Fronts > 0 ? FrontX / Fronts : 0.0;
	RearX = Rears > 0 ? RearX / Rears : 0.0;
	const double Wheelbase = FrontX - RearX;
	const FBodyInstance* Chassis = Body.GetBodyInstance();
	const FVector Com = Chassis && Chassis->IsValidBodyInstance()
		? Body.GetComponentTransform().InverseTransformPosition(Chassis->GetCOMPosition()) : FVector::ZeroVector;
	float MaxRaise = 0.f;
	const float ContactZ = WheelContactZ(Move, Body, MaxRaise);
	const FBox ChassisBox = Chassis ? CollisionBoxInComponent(Body, *Chassis) : FBox(ForceInit);
	// Chaos shares the car's weight between the springs from their positions around the component's
	// origin (FSuspensionUtility::ComputeSprungMasses), not around the centre of mass.
	GTP_TRACE(LogGTPVehicle, TEXT("drive config: %s mass %.0f kg (body %.0f kg), %s (front share %.2f), torque %.0f N.m (player %.0f), max %.0f rpm, final ratio %.2f, %d forward gears, first %.2f; origin at %.2f and centre of mass at %.2f of the way from the rear axle to the front one (wheelbase %.0f cm), centre of mass %.0f cm above the origin, chassis box %.0f cm above the ground, chassis collision %.0f cm above the wheels' contact plane (max raise %.0f cm)%s"),
		*GetClass()->GetName(), Move.Mass, Body.GetMass(), DifferentialName, Move.DifferentialSetup.FrontRearSplit, BaseTorque, Assist.PlayerTorque,
		Move.EngineSetup.MaxRPM, Move.TransmissionSetup.FinalRatio, Move.TransmissionSetup.ForwardGearRatios.Num(),
		Move.TransmissionSetup.ForwardGearRatios.Num() > 0 ? Move.TransmissionSetup.ForwardGearRatios[0] : 0.f,
		Wheelbase != 0.0 ? -RearX / Wheelbase : -1.0, Wheelbase != 0.0 ? (Com.X - RearX) / Wheelbase : -1.0, Wheelbase, Com.Z,
		ChassisClearanceCm(Body), ChassisBox.IsValid ? ChassisBox.Min.Z - ContactZ : -9999.0, MaxRaise, *Wheels);
}

void AGTPVehicle::ProbeDrive()
{
	static const bool bOff = FParse::Param(FCommandLine::Get(), TEXT("NoDriveProbe"));
	if (bOff || (State != EGTPVehicleState::Driven && !HasAIDriver()))
	{
		return;
	}
	UWorld* World = GetWorld();
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Body = GetMesh();
	if (bOff || !World || !Move || !Body || bWrecked || !Body->IsSimulatingPhysics() || !Move->PhysicsVehicleOutput().IsValid())
	{
		return;
	}
	const bool bPlayer = Cast<APlayerController>(GetController()) != nullptr;
	const bool bPolice = !bPlayer && HasAIDriver() && ActorHasTag(GTPPolice::PoliceTag);
	if (!bPlayer && !bPolice)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float Speed = GetSpeedKmh();
	const float Throttle = Move->GetThrottleInput();
	if (bPlayer)
	{
		// 0-50 and 0-100 km/h from a standstill with the throttle held down.
		TopSpeedKmh = FMath::Max(TopSpeedKmh, FMath::Abs(Speed));
		if (FMath::Abs(Speed) < 2.f)
		{
			if (Throttle < 0.9f)
			{
				LaunchTime = -1.f;
			}
			else if (LaunchTime < 0.f)
			{
				LaunchTime = Now;
				bLaunch50Logged = false;
			}
		}
		if (LaunchTime >= 0.f)
		{
			if (Throttle < 0.5f)
			{
				LaunchTime = -1.f;
			}
			else if (Speed >= 50.f && !bLaunch50Logged)
			{
				bLaunch50Logged = true;
				GTP_TRACE(LogGTPVehicle, TEXT("drive probe: %s 0-50 km/h in %.1f s"), *GetClass()->GetName(), Now - LaunchTime);
			}
			else if (Speed >= 100.f)
			{
				GTP_TRACE(LogGTPVehicle, TEXT("drive probe: %s 0-100 km/h in %.1f s"), *GetClass()->GetName(), Now - LaunchTime);
				LaunchTime = -1.f;
			}
			else if (Now - LaunchTime > 40.f)
			{
				GTP_TRACE(LogGTPVehicle, TEXT("drive probe: %s 0-100 km/h not reached in 40 s (%s 50 km/h, top %.0f km/h)"), *GetClass()->GetName(),
					bLaunch50Logged ? TEXT("past") : TEXT("below"), TopSpeedKmh);
				LaunchTime = -1.f;
			}
		}
	}
	if (Now < NextDriveProbeTime || (bPlayer && FMath::Abs(Speed) < 1.f && Throttle < 0.1f) || (bPolice && PoliceProbeLines >= 40))
	{
		return;
	}
	NextDriveProbeTime = Now + (bPlayer ? 1.f : 3.f);
	PoliceProbeLines += bPolice ? 1 : 0;
	static TSet<FName> ConfigLogged;
	if (!ConfigLogged.Contains(GetClass()->GetFName()))
	{
		ConfigLogged.Add(GetClass()->GetFName());
		LogDriveConfig(*Move, *Body);
	}
	// The weight the springs carry: about 1 when the car stands on its wheels, well below when the
	// chassis rests on something (Chaos works in cm: the spring forces are in kg.cm/s2, as M * g).
	const float Weight = Move->Mass * FMath::Abs(World->GetGravityZ());
	float Springs = 0.f;
	FString Wheels;
	static const TCHAR* Names[] = {TEXT("FL"), TEXT("FR"), TEXT("BL"), TEXT("BR")};
	for (int32 Index = 0; Index < Move->GetNumWheels() && Index < Move->Wheels.Num(); ++Index)
	{
		const FWheelStatus& WheelState = Move->GetWheelState(Index);
		const UChaosVehicleWheel* Wheel = Move->Wheels[Index];
		Springs += WheelState.bInContact ? WheelState.SpringForce : 0.f;
		const float WheelKmh = Wheel ? FMath::Abs(FMath::DegreesToRadians(Wheel->GetRotationAngularVelocity()) * Wheel->GetWheelRadius()) * 0.036f : 0.f;
		const UPhysicalMaterial* Ground = WheelState.PhysMaterial.Get();
		Wheels += FString::Printf(TEXT(" | %s contact=%d spring=%.0f%% susp=%.2f drive=%.0f N.m wheel=%.0f km/h slip=%.0f %s %.2f"),
			Index < 4 ? Names[Index] : TEXT("W"), WheelState.bInContact ? 1 : 0, Weight > 0.f ? 100.f * WheelState.SpringForce / Weight : 0.f,
			WheelState.NormalizedSuspensionLength, WheelState.DriveTorque, WheelKmh, WheelState.SlipMagnitude,
			Ground ? *Ground->GetName() : TEXT("no material"), Ground ? Ground->Friction : -1.f);
	}
	GTP_TRACE(LogGTPVehicle, TEXT("drive probe: %s%s t=%.1f kmh=%.1f top=%.0f gear=%d rpm=%.0f throttle=%.2f brake=%.2f handbrake=%d torque=%.0f N.m | springs carry %.0f%% of the weight | chassis touched the ground %d time(s) since the last line, box %.0f cm above the ground%s"),
		*GetName(), bPolice ? TEXT(" (police)") : TEXT(""), Now, Speed, TopSpeedKmh, Move->GetCurrentGear(), Move->GetEngineRotationSpeed(), Throttle,
		Move->GetBrakeInput(), Move->GetHandbrakeInput() ? 1 : 0, bPlayerAssist ? Assist.PlayerTorque * FMath::Max(AppliedPower, 0.f) : BaseTorque,
		Weight > 0.f ? 100.f * Springs / Weight : 0.f, GroundHitsSinceProbe, ChassisClearanceCm(*Body), *Wheels);
	GroundHitsSinceProbe = 0;
}

namespace GTPLaunchStraight
{
	// A road line for the launch test: from Start along Dir, LengthCm of lanes that keep within
	// SideCm of the line and keep going forward (lane points are 2D, Z = 0).
	struct FLine
	{
		FVector2D Start = FVector2D::ZeroVector;
		FVector2D Dir = FVector2D::ZeroVector;
		float LengthCm = 0.f;
		int32 FirstLane = INDEX_NONE;
		int32 LastLane = INDEX_NONE;
	};
	constexpr float SideCm = 150.f;
	constexpr float MaxCm = 150000.f;

	FLine Walk(const TArray<FGTPTrafficLane>& Lanes, int32 Lane, int32 Point)
	{
		FLine Line;
		const TArray<FVector>& First = Lanes[Lane].Points;
		const FVector2D Start(First[Point].X, First[Point].Y);
		const FVector2D Dir = (FVector2D(First[Point + 1].X, First[Point + 1].Y) - Start).GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			return Line;
		}
		Line.Start = Start;
		Line.Dir = Dir;
		Line.FirstLane = Lane;
		Line.LastLane = Lane;
		int32 Current = Lane;
		int32 Index = Point + 1;
		double Along = 0.0;
		for (int32 Step = 0; Step < 4000 && Along < MaxCm; ++Step)
		{
			const TArray<FVector>& Points = Lanes[Current].Points;
			if (Index >= Points.Num())
			{
				// On to the next lane whose chord keeps the heading best (within about 8 degrees).
				int32 Best = INDEX_NONE;
				double BestDot = 0.99;
				for (const int32 Next : Lanes[Current].Next)
				{
					const TArray<FVector>& NextPoints = Lanes[Next].Points;
					if (NextPoints.Num() < 2)
					{
						continue;
					}
					const FVector2D Chord = (FVector2D(NextPoints.Last().X, NextPoints.Last().Y) - FVector2D(NextPoints[0].X, NextPoints[0].Y)).GetSafeNormal();
					const double Dot = FVector2D::DotProduct(Chord, Dir);
					if (Dot > BestDot)
					{
						BestDot = Dot;
						Best = Next;
					}
				}
				if (Best == INDEX_NONE)
				{
					break;
				}
				Current = Best;
				Index = 0;
				continue;
			}
			const FVector2D Rel = FVector2D(Points[Index].X, Points[Index].Y) - Start;
			const double Forward = FVector2D::DotProduct(Rel, Dir);
			if (FMath::Abs(FVector2D::CrossProduct(Dir, Rel)) > SideCm || Forward < Along - 1.0)
			{
				break;
			}
			Along = FMath::Max(Along, Forward);
			Line.LastLane = Current;
			++Index;
		}
		Line.LengthCm = static_cast<float>(Along);
		return Line;
	}

	// The road under At, searched from 3 m above RefZ (under the trees and lamps over the road) to 20 m below.
	bool GroundZ(const UWorld& World, const FVector2D& At, double RefZ, const FCollisionQueryParams& Params, double& OutZ)
	{
		FHitResult Hit;
		if (!World.LineTraceSingleByObjectType(Hit, FVector(At.X, At.Y, RefZ + 300.0), FVector(At.X, At.Y, RefZ - 2000.0),
			FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			return false;
		}
		OutZ = Hit.ImpactPoint.Z;
		return true;
	}

	// The longest stretch of Line a car-wide box, 0.45 to 1.55 m above the road, crosses without
	// meeting the world (bollards, poles, shelters, walls, props; not pawns nor vehicles), in 25 m
	// sweeps that follow the road; OutFromCm is where it starts on the line, OutEnd what ends it.
	float ClearCm(const UWorld& World, const FLine& Line, double RefZ, const FCollisionQueryParams& Params, float& OutFromCm, double& OutFromZ, FString& OutEnd)
	{
		const FQuat Facing(FVector::UpVector, FMath::Atan2(Line.Dir.Y, Line.Dir.X));
		const FCollisionShape Box = FCollisionShape::MakeBox(FVector(50.f, 100.f, 55.f));
		// Street furniture: static, movable (props, details) and loose physics bodies (breakables).
		FCollisionObjectQueryParams Static(ECC_WorldStatic);
		Static.AddObjectTypesToQuery(ECC_WorldDynamic);
		Static.AddObjectTypesToQuery(ECC_PhysicsBody);
		constexpr float StepCm = 2500.f;
		constexpr float PastObstacleCm = 500.f;
		float BestCm = 0.f;
		double Z = 0.0;
		if (!GroundZ(World, Line.Start, RefZ, Params, Z))
		{
			OutEnd = TEXT("no road under the start");
			return 0.f;
		}
		float GapFrom = 0.f;
		double GapZ = Z;
		float Done = 0.f;
		auto CloseGap = [&](float At, const FString& Why)
		{
			if (At - GapFrom > BestCm)
			{
				BestCm = At - GapFrom;
				OutFromCm = GapFrom;
				OutFromZ = GapZ;
				OutEnd = Why;
			}
		};
		for (int32 Guard = 0; Done < Line.LengthCm && Guard < 1000; ++Guard)
		{
			const float Next = FMath::Min(Done + StepCm, Line.LengthCm);
			const FVector2D A = Line.Start + Line.Dir * Done;
			const FVector2D B = Line.Start + Line.Dir * Next;
			double NextZ = Z;
			if (!GroundZ(World, B, Z, Params, NextZ))
			{
				CloseGap(Done, TEXT("no road found under the line"));
				return BestCm;
			}
			FHitResult Hit;
			if (!World.SweepSingleByObjectType(Hit, FVector(A.X, A.Y, Z + 100.0), FVector(B.X, B.Y, NextZ + 100.0), Facing, Static, Box, Params))
			{
				Done = Next;
				Z = NextZ;
				continue;
			}
			const float HitAt = Done + (Hit.bStartPenetrating ? 0.f : static_cast<float>(Hit.Distance));
			const AActor* Blocker = Hit.GetActor();
			CloseGap(HitAt, FString::Printf(TEXT("%s at %.0f m"), Blocker ? *Blocker->GetName() : TEXT("static geometry"), HitAt / 100.f));
			// Past the obstacle: a new stretch may start there.
			Done = HitAt + PastObstacleCm;
			GapFrom = Done;
			const FVector2D After = Line.Start + Line.Dir * Done;
			if (Done >= Line.LengthCm || !GroundZ(World, After, Z, Params, Z))
			{
				return BestCm;
			}
			GapZ = Z;
		}
		CloseGap(FMath::Min(Done, Line.LengthCm), TEXT("the road turns"));
		return BestCm;
	}
}

void AGTPVehicle::PlaceForLaunchTest()
{
	UWorld* World = GetWorld();
	USkeletalMeshComponent* Body = GetMesh();
	if (!World || !Body)
	{
		return;
	}
	// The lanes: the traffic network's, else the map's lanes file (-Traffic=0 spawns no network).
	TArray<FGTPTrafficLane> FileLanes;
	const TArray<FGTPTrafficLane>* Lanes = nullptr;
	TActorIterator<AGTPTrafficNetwork> Network(World);
	if (Network)
	{
		Lanes = &Network->GetLanes();
	}
	else if (AGTPTrafficNetwork::ReadLanesCsv(AGTPTrafficNetwork::LanesFileFor(UGameplayStatics::GetCurrentLevelName(this, true)), FileLanes))
	{
		Lanes = &FileLanes;
	}
	if (!Lanes || Lanes->Num() == 0)
	{
		GTP_TRACE(LogGTPVehicle, TEXT("drive launch test: no lanes, launched where the car stands"));
		return;
	}
	using namespace GTPLaunchStraight;
	TArray<FLine> Candidates;
	for (int32 Lane = 0; Lane < Lanes->Num(); ++Lane)
	{
		for (int32 Point = 0; Point + 1 < (*Lanes)[Lane].Points.Num(); ++Point)
		{
			const FLine Line = Walk(*Lanes, Lane, Point);
			if (Line.LengthCm > 15000.f)
			{
				Candidates.Add(Line);
			}
		}
	}
	Candidates.Sort([](const FLine& A, const FLine& B) { return A.LengthCm > B.LengthCm; });
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPLaunchStraight), false, this);
	Params.AddIgnoredActor(Driver);
	TArray<FLine> Tested;
	FLine Chosen;
	float ChosenCm = 0.f;
	float ChosenFrom = 0.f;
	double ChosenZ = GetActorLocation().Z;
	FString ChosenEnd;
	for (const FLine& Line : Candidates)
	{
		if (Line.LengthCm <= ChosenCm || Tested.Num() >= 24)
		{
			break;
		}
		// One test per road and direction: a start on a line already tested is the same straight.
		const bool bSeen = Tested.ContainsByPredicate([&Line](const FLine& Other)
		{
			return FVector2D::DotProduct(Other.Dir, Line.Dir) > 0.99 && FMath::Abs(FVector2D::CrossProduct(Other.Dir, Line.Start - Other.Start)) < 400.0;
		});
		if (bSeen)
		{
			continue;
		}
		Tested.Add(Line);
		float From = 0.f;
		double FromZ = 0.0;
		FString End;
		const float Clear = ClearCm(*World, Line, GetActorLocation().Z, Params, From, FromZ, End);
		if (Clear > ChosenCm)
		{
			Chosen = Line;
			ChosenCm = Clear;
			ChosenFrom = From;
			ChosenZ = FromZ;
			ChosenEnd = End;
		}
	}
	if (ChosenCm < 10000.f)
	{
		GTP_TRACE(LogGTPVehicle, TEXT("drive launch test: no clear straight over 100 m among %d line(s) (%d tested), launched where the car stands"),
			Candidates.Num(), Tested.Num());
		return;
	}
	// The car 3 m into the stretch, along the line, its wheels just above the road.
	constexpr float Setback = 300.f;
	LaunchLineDir = Chosen.Dir;
	LaunchLineStart = Chosen.Start + Chosen.Dir * ChosenFrom;
	LaunchLineCm = ChosenCm;
	const FVector2D Spot = LaunchLineStart + LaunchLineDir * Setback;
	const float Yaw = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(LaunchLineDir.Y, LaunchLineDir.X)));
	// Physics off while the car is moved and for half a second after (RunLaunchTest holds it there,
	// then gives it back to the solver): the solver undoes the teleport of a simulated car. Seen by
	// GTPDemoRoute's chase relocation on 16/11, and here at candidate 49: put 441 m away, the car was
	// back at the player start when the launch began, three times out of three.
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Body->SetSimulatePhysics(false);
	LaunchHoldAt = FTransform(FRotator(0.f, Yaw, 0.f), FVector(Spot.X, Spot.Y, ChosenZ + 30.0));
	LaunchHoldUntil = static_cast<float>(World->GetTimeSeconds()) + 0.5f;
	SetActorLocationAndRotation(LaunchHoldAt.GetLocation(), LaunchHoldAt.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	NotePlaced(TEXT("launch test"));
	GTP_TRACE(LogGTPVehicle, TEXT("drive launch test: straight of %.0f m clear (line of %.0f m on lanes %d to %d, %d line(s), %d road(s) tested; ends: %s), car put at %s heading %.0f"),
		ChosenCm / 100.f, Chosen.LengthCm / 100.f, Chosen.FirstLane, Chosen.LastLane, Candidates.Num(), Tested.Num(), *ChosenEnd,
		*FVector(Spot.X, Spot.Y, ChosenZ).ToCompactString(), Yaw);
}

void AGTPVehicle::RunLaunchTest()
{
	static const bool bOn = FParse::Param(FCommandLine::Get(), TEXT("DriveLaunchTest"));
	if (!bOn || bLaunchTestDone || !bPlayerAssist || State != EGTPVehicleState::Driven || bWrecked)
	{
		return;
	}
	UWorld* World = GetWorld();
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!World || !Move)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (LaunchTestStart < 0.f)
	{
		// Put on the longest clear straight of the map first, then 3 s to settle.
		PlaceForLaunchTest();
		LaunchTestStart = Now + 3.f;
		return;
	}
	if (LaunchHoldUntil >= 0.f)
	{
		// Just put on the straight: held there with the physics off, then given back to the solver.
		SetActorLocationAndRotation(LaunchHoldAt.GetLocation(), LaunchHoldAt.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
		if (Now < LaunchHoldUntil)
		{
			return;
		}
		LaunchHoldUntil = -1.f;
		if (USkeletalMeshComponent* Held = GetMesh())
		{
			Held->SetSimulatePhysics(true);
			Held->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Held->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			Held->WakeAllRigidBodies();
		}
		GTP_TRACE(LogGTPVehicle, TEXT("drive launch test: physics back on at %s"), *GetActorLocation().ToCompactString());
	}
	if (Now < LaunchTestStart)
	{
		return;
	}
	if (LaunchTestFrom.IsZero())
	{
		// The PC's first run (candidate 49) ended at once, "braking before the end of the straight" at
		// 0 km/h: the car was no longer where it was put. Checked here, put back at most twice, else the
		// straight is dropped and the car launched where it stands.
		if (LaunchLineCm > 0.f)
		{
			const FVector2D At = FVector2D(GetActorLocation().X, GetActorLocation().Y) - LaunchLineStart;
			const float AlongCm = static_cast<float>(FVector2D::DotProduct(At, LaunchLineDir));
			const float OffCm = static_cast<float>(FVector2D::CrossProduct(LaunchLineDir, At));
			const USkeletalMeshComponent* Body = GetMesh();
			GTP_TRACE(LogGTPVehicle, TEXT("drive launch test: car at %s, %.0f m along the straight of %.0f m, %.1f m beside it, %.0f km/h, %s, last placed by %s %.1f s ago"),
				*GetActorLocation().ToCompactString(), AlongCm / 100.f, LaunchLineCm / 100.f, OffCm / 100.f, GetSpeedKmh(),
				Body && Body->IsSimulatingPhysics() ? TEXT("simulated") : TEXT("kinematic"), LastPlacedHow, Now - LastPlacedTime);
			if (AlongCm < 0.f || AlongCm > LaunchLineCm - 5000.f || FMath::Abs(OffCm) > 500.f)
			{
				if (LaunchPlaceTries < 2)
				{
					++LaunchPlaceTries;
					GTP_TRACE(LogGTPVehicle, TEXT("drive launch test: the car left the straight it was put on, put on it again (%d of 2)"), LaunchPlaceTries);
					PlaceForLaunchTest();
					LaunchTestStart = Now + 3.f;
					return;
				}
				GTP_TRACE(LogGTPVehicle, TEXT("drive launch test: the car left the straight twice, straight dropped, launched where it stands"));
				LaunchLineCm = 0.f;
			}
		}
		LaunchTestFrom = GetActorLocation();
		TopSpeedKmh = 0.f;
		LaunchTestMarks.Reset();
		LaunchTest130 = -1.f;
	}
	const float Speed = GetSpeedKmh();
	const float Metres = static_cast<float>(FVector::Dist2D(LaunchTestFrom, GetActorLocation()) / 100.0);
	if (LaunchTest130 < 0.f && Speed >= 130.f)
	{
		LaunchTest130 = Now - LaunchTestStart;
	}
	// The speed every 100 m.
	while (Metres >= 100.f * (LaunchTestMarks.Num() + 1) && LaunchTestMarks.Num() < 20)
	{
		LaunchTestMarks.Add(Speed);
	}
	const bool bBlocked = TopSpeedKmh > 30.f && Speed < TopSpeedKmh - 30.f;
	// On a straight: the run is measured up to half a second and 15 m before its end, then the car
	// brakes. The longest clear straight of the map is 181 m (PC, candidate 49): braking to a stop
	// before its end (6 m/s2) left too little of it to reach 100 km/h; what follows is not measured.
	const FVector2D Rel = FVector2D(GetActorLocation().X, GetActorLocation().Y) - LaunchLineStart;
	const float Along = LaunchLineCm > 0.f ? static_cast<float>(FVector2D::DotProduct(Rel, LaunchLineDir)) : 0.f;
	const float SpeedMs = FMath::Max(Speed, 0.f) / 3.6f;
	const bool bStraightEnds = LaunchLineCm > 0.f && (LaunchLineCm - Along) / 100.f < SpeedMs * 0.5f + 15.f;
	if (bBlocked || bStraightEnds || Now - LaunchTestStart > 40.f)
	{
		bLaunchTestDone = true;
		ThrottleAmount = 0.f;
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(1.f);
		FString Marks;
		for (int32 Index = 0; Index < LaunchTestMarks.Num(); ++Index)
		{
			Marks += FString::Printf(TEXT("%s%d m %.0f"), Index ? TEXT(", ") : TEXT(""), 100 * (Index + 1), LaunchTestMarks[Index]);
		}
		const FBodyInstance* Chassis = GetMesh() ? GetMesh()->GetBodyInstance() : nullptr;
		const FVector CentreOfMass = Chassis && Chassis->IsValidBodyInstance() ? Chassis->GetMassSpaceLocal().GetTranslation() : FVector::ZeroVector;
		GTP_TRACE(LogGTPVehicle, TEXT("drive launch test: %s top %.0f km/h, %.0f m in %.1f s, %s at %.0f m along the straight of %.0f m; 0-130 km/h %s; km/h every 100 m: %s | torque %.0f N.m (player), mass %.0f kg, centre of mass X %.0f Z %.0f, %d gears, max %.0f rpm"),
			*GetClass()->GetName(), TopSpeedKmh, Metres, Now - LaunchTestStart,
			bBlocked ? TEXT("stopped by an obstacle") : bStraightEnds ? TEXT("15 m before the end of the straight") : TEXT("40 s at full throttle"),
			Along / 100.f, LaunchLineCm / 100.f, LaunchTest130 >= 0.f ? *FString::Printf(TEXT("in %.1f s"), LaunchTest130) : TEXT("not reached"), Marks.IsEmpty() ? TEXT("none") : *Marks,
			Assist.PlayerTorque * FMath::Max(AppliedPower, 0.f), Move->Mass, CentreOfMass.X, CentreOfMass.Z,
			Move->TransmissionSetup.ForwardGearRatios.Num(), Move->EngineSetup.MaxRPM);
		return;
	}
	if (Move->GetCurrentGear() == 0)
	{
		Move->SetTargetGear(1, true);
	}
	// Held on the line: back towards it when off by a few metres or turned from it.
	float Steer = 0.f;
	if (LaunchLineCm > 0.f)
	{
		const float Side = static_cast<float>(FVector2D::CrossProduct(LaunchLineDir, Rel));
		const float LineYaw = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(LaunchLineDir.Y, LaunchLineDir.X)));
		const float YawError = static_cast<float>(FRotator::NormalizeAxis(GetActorRotation().Yaw - LineYaw));
		const float Limit = Speed > 60.f ? 0.15f : 0.3f;
		Steer = FMath::Clamp(-YawError / 8.f - Side / 400.f, -Limit, Limit);
	}
	ThrottleAmount = 1.f;
	Move->SetHandbrakeInput(false);
	Move->SetBrakeInput(0.f);
	Move->SetSteeringInput(Steer);
	Move->SetThrottleInput(1.f);
}

void AGTPVehicle::SetScriptedDriveInput(float Throttle, float Brake, float Steering, bool bHandbrake)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || bWrecked)
	{
		return;
	}
	ThrottleAmount = FMath::Clamp(Throttle, 0.f, 1.f);
	Move->SetThrottleInput(ThrottleAmount);
	Move->SetBrakeInput(FMath::Clamp(Brake, 0.f, 1.f));
	Move->SetSteeringInput(FMath::Clamp(Steering, -1.f, 1.f));
	Move->SetHandbrakeInput(bHandbrake);
}

// --- Input -----------------------------------------------------------------

void AGTPVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	AGTPPlayerController* PC = Cast<AGTPPlayerController>(GetController());
	UGTPVehicleInput* Input = PC ? PC->GetVehicleInput() : nullptr;
	if (!Enhanced || !Input)
	{
		UE_LOG(LogGTPVehicle, Warning, TEXT("%s: no enhanced input or vehicle input set, driving disabled"), *GetName());
		return;
	}
	Enhanced->BindAction(Input->Throttle, ETriggerEvent::Triggered, this, &AGTPVehicle::OnThrottle);
	Enhanced->BindAction(Input->Throttle, ETriggerEvent::Completed, this, &AGTPVehicle::OnThrottleReleased);
	Enhanced->BindAction(Input->Brake, ETriggerEvent::Triggered, this, &AGTPVehicle::OnBrake);
	Enhanced->BindAction(Input->Brake, ETriggerEvent::Completed, this, &AGTPVehicle::OnBrakeReleased);
	Enhanced->BindAction(Input->Steer, ETriggerEvent::Triggered, this, &AGTPVehicle::OnSteer);
	Enhanced->BindAction(Input->Steer, ETriggerEvent::Completed, this, &AGTPVehicle::OnSteerReleased);
	Enhanced->BindAction(Input->Handbrake, ETriggerEvent::Started, this, &AGTPVehicle::OnHandbrakeStarted);
	Enhanced->BindAction(Input->Handbrake, ETriggerEvent::Completed, this, &AGTPVehicle::OnHandbrakeCompleted);
	Enhanced->BindAction(Input->Look, ETriggerEvent::Triggered, this, &AGTPVehicle::OnLook);
	Enhanced->BindAction(Input->LookBehind, ETriggerEvent::Started, this, &AGTPVehicle::OnLookBehindStarted);
	Enhanced->BindAction(Input->LookBehind, ETriggerEvent::Completed, this, &AGTPVehicle::OnLookBehindCompleted);
	// Enter / exit and the view toggle are bound by the player controller.
}

void AGTPVehicle::OnThrottle(const FInputActionValue& Value)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (Move && State == EGTPVehicleState::Driven && !bWrecked)
	{
		// The engine is still starting (or being hot-wired): nothing yet.
		ThrottleAmount = IsEngineRunning() ? FMath::Clamp(Value.Get<float>(), 0.f, 1.f) : 0.f;
		Move->SetThrottleInput(ThrottleAmount);
	}
}

void AGTPVehicle::OnThrottleReleased()
{
	ThrottleAmount = 0.f;
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetThrottleInput(0.f);
	}
}

void AGTPVehicle::OnBrake(const FInputActionValue& Value)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (Move && State == EGTPVehicleState::Driven)
	{
		Move->SetBrakeInput(FMath::Clamp(Value.Get<float>(), 0.f, 1.f));
	}
}

void AGTPVehicle::OnBrakeReleased()
{
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetBrakeInput(0.f);
	}
}

void AGTPVehicle::OnSteer(const FInputActionValue& Value)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (Move && State == EGTPVehicleState::Driven)
	{
		SteerInput = FMath::Clamp(Value.Get<float>(), -1.f, 1.f);
		Move->SetSteeringInput(SteerInput);
	}
}

void AGTPVehicle::OnSteerReleased()
{
	SteerInput = 0.f;
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetSteeringInput(0.f);
	}
}

void AGTPVehicle::OnHandbrakeStarted()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (Move && State == EGTPVehicleState::Driven)
	{
		bHandbrakeHeld = true;
		Move->SetHandbrakeInput(true);
	}
}

void AGTPVehicle::OnHandbrakeCompleted()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	bHandbrakeHeld = false;
	if (Move && State == EGTPVehicleState::Driven)
	{
		Move->SetHandbrakeInput(false);
	}
}

void AGTPVehicle::OnLook(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	if (!Look.IsNearlyZero())
	{
		AddControllerYawInput(Look.X);
		AddControllerPitchInput(Look.Y);
		LastLookInputTime = GetWorld()->GetTimeSeconds();
	}
}

void AGTPVehicle::OnLookBehindStarted()
{
	bLookingBehind = true;
	ChaseArm->bUsePawnControlRotation = false;
	ChaseArm->SetRelativeRotation(FRotator(ChasePitch, 180.f, 0.f));
	InteriorCamera->bUsePawnControlRotation = false;
	InteriorCamera->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
}

void AGTPVehicle::OnLookBehindCompleted()
{
	bLookingBehind = false;
	ChaseArm->bUsePawnControlRotation = true;
	InteriorCamera->bUsePawnControlRotation = true;
}

void AGTPVehicle::ClearDrivingInput()
{
	ThrottleAmount = 0.f;
	SteerInput = 0.f;
	bHandbrakeHeld = false;
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(0.f);
		Move->SetSteeringInput(0.f);
		Move->SetHandbrakeInput(false);
	}
	if (bLookingBehind)
	{
		OnLookBehindCompleted();
	}
}

void AGTPVehicle::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	State = EGTPVehicleState::Driven;
	ClearDrivingInput();
	LastLookInputTime = -1000.f;
	// A car taken from an AI driver: brake held while stopped reverses again (SetAIDriveInput).
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->bReverseAsBrake = true;
	}
	if (const AGTPPlayerController* PC = Cast<AGTPPlayerController>(NewController))
	{
		SetInteriorView(PC->GetCameraMode() && PC->GetCameraMode()->IsFirstPerson());
	}
	// The player's car gets the arcade torque (GTA pick-up); AI cars keep the real one.
	bPlayerAssist = Cast<APlayerController>(NewController) != nullptr;
	AppliedPower = -1.f;
	ChaseArm->SetComponentTickEnabled(bPlayerAssist);
}

void AGTPVehicle::UnPossessed()
{
	Super::UnPossessed();
	ClearDrivingInput();
	ChaseArm->SetComponentTickEnabled(false);
	if (bPlayerAssist)
	{
		bPlayerAssist = false;
		SetRearGrip(RearGripBase);
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetMaxEngineTorque(BaseTorque);
		}
	}
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		// A car left behind by a bail out keeps rolling; otherwise it parks.
		Move->SetHandbrakeInput(FMath::Abs(GetSpeedKmh()) < BailOutSpeedKmh);
	}
}

// --- Camera ----------------------------------------------------------------

void AGTPVehicle::SetInteriorView(bool bEnable)
{
	bInteriorView = bEnable;
	ChaseCamera->SetActive(!bEnable);
	InteriorCamera->SetActive(bEnable);
}

void AGTPVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (const UWorld* World = GetWorld(); World && World->GetTimeSeconds() >= NextBodyCheckTime)
	{
		NextBodyCheckTime = World->GetTimeSeconds() + 1.f;
		KeepChassisBodyOnly();
		CentreMassOverOrigin();
	}
	RunLaunchTest();
	ProbeDrive();
	if ((State == EGTPVehicleState::Driven && IsLocallyControlled() && bPlayerAssist) || HasAIDriver())
	{
		PutBackOnWheels(DeltaSeconds);
	}
	if (State == EGTPVehicleState::Driven && IsLocallyControlled())
	{
		if (bPlayerAssist)
		{
			UpdateDriveAssist(DeltaSeconds);
		}
		UpdateChaseCamera(DeltaSeconds);
	}
}

void AGTPVehicle::SetRearGrip(float Multiplier)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	if (!Move || FMath::IsNearlyEqual(Multiplier, RearGripNow, 0.01f))
	{
		return;
	}
	RearGripNow = Multiplier;
	for (int32 Wheel = 2; Wheel < Move->Wheels.Num(); ++Wheel)
	{
		Move->SetWheelFrictionMultiplier(Wheel, Multiplier);
	}
}

void AGTPVehicle::UpdateDriveAssist(float DeltaSeconds)
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	USkeletalMeshComponent* Body = GetMesh();
	if (!Move || !Body || !Body->IsSimulatingPhysics() || bWrecked)
	{
		return;
	}
	const float Speed = GetSpeedKmh();

	// Power: arcade torque, less of it when the car is badly damaged.
	const float Power = GTPCarjack::PowerFromHealth(GetHealthRatio(), Assist);
	if (!FMath::IsNearlyEqual(Power, AppliedPower, 0.02f))
	{
		AppliedPower = Power;
		Move->SetMaxEngineTorque(Assist.PlayerTorque * Power);
	}

	// Handbrake turns: less rear grip, and the tail swings with the steering.
	const bool bDrift = bHandbrakeHeld && FMath::Abs(Speed) >= Assist.DriftMinSpeedKmh;
	SetRearGrip(bDrift ? RearGripBase * Assist.DriftRearGrip : RearGripBase);
	const FVector Velocity = Body->GetPhysicsLinearVelocity();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
	float SlipDeg = 0.f;
	if (Velocity.Size2D() > 300.f)
	{
		const FVector Heading = Velocity.GetSafeNormal2D();
		SlipDeg = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Forward, Heading).Z, FVector::DotProduct(Forward, Heading))));
	}
	const float YawRate = static_cast<float>(Body->GetPhysicsAngularVelocityInDegrees().Z);
	const float YawAccel = GTPCarjack::YawAssist(bHandbrakeHeld, Speed, SteerInput, SlipDeg, YawRate, Assist);
	if (YawAccel != 0.f)
	{
		Body->AddTorqueInDegrees(FVector(0.f, 0.f, YawAccel), NAME_None, true);
	}

	// A car beaten on one side pulls to that side.
	const float FullDent = FullDentDamage > 0.f ? FullDentDamage : 1.f;
	const float Pull = GTPCarjack::PullFromDents(FMath::Clamp(ZoneDamage[static_cast<int32>(EGTPDamageZone::Left)] / FullDent, 0.f, 1.f),
		FMath::Clamp(ZoneDamage[static_cast<int32>(EGTPDamageZone::Right)] / FullDent, 0.f, 1.f), Assist);
	if (Pull != 0.f)
	{
		Move->SetSteeringInput(FMath::Clamp(SteerInput + Pull * FMath::Clamp(FMath::Abs(Speed) / 30.f, 0.f, 1.f), -1.f, 1.f));
	}
}

void AGTPVehicle::PutBackOnWheels(float DeltaSeconds)
{
	// On its roof or side and stopped: put back on its wheels after a moment, as in GTA. The player's
	// car and, since candidate 49, the cars driven by the AI (police cars turned over in a chase
	// stayed there: their body asleep, the vehicle's output frozen).
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || bWrecked || !Body->IsSimulatingPhysics())
	{
		UpsideDownTime = 0.f;
		return;
	}
	// A traffic car from 45 degrees, where the traffic line counts it lying (gate 62: one on 22 in the default and
	// in the -GTARun pass); leaning on a car, a kerb or a wall, it stayed there. The player's car from its side.
	const float LyingUpZ = HasAIDriver() ? 0.7071f : 0.25f;
	if (GetActorUpVector().Z < LyingUpZ && FMath::Abs(GetSpeedKmh()) < 5.f)
	{
		UpsideDownTime += DeltaSeconds;
		if (UpsideDownTime > 2.5f)
		{
			UpsideDownTime = 0.f;
			const FVector Was = GetActorLocation();
			const float WasUp = static_cast<float>(GetActorUpVector().Z);
			SetActorLocationAndRotation(Was + FVector(0.f, 0.f, 120.f), FRotator(0.f, GetActorRotation().Yaw, 0.f), false, nullptr,
				ETeleportType::TeleportPhysics);
			Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			Body->WakeAllRigidBodies();
			NotePlaced(TEXT("flipped back"));
			GTP_TRACE(LogGTPVehicle, TEXT("%s%s: put back on its wheels at %s (up.Z %.2f)"), *GetName(), HasAIDriver() ? TEXT(" (AI driver)") : TEXT(""),
				*Was.ToCompactString(), WasUp);
		}
	}
	else
	{
		UpsideDownTime = 0.f;
	}
}

void AGTPVehicle::UpdateChaseCamera(float DeltaSeconds)
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}
	const float Speed = FMath::Abs(GetSpeedKmh());
	const float SpeedAlpha = FOVMaxSpeedKmh > 0.f ? FMath::Clamp(Speed / FOVMaxSpeedKmh, 0.f, 1.f) : 0.f;
	ChaseCamera->SetFieldOfView(FMath::Lerp(BaseFOV, MaxSpeedFOV, SpeedAlpha) + ImpactShake * 4.f);
	// Further back at speed, so the road ahead stays in view.
	ChaseArm->TargetArmLength = FMath::FInterpTo(ChaseArm->TargetArmLength, ChaseDistance + SpeedAlpha * ChaseSpeedExtraDistance, DeltaSeconds, 2.f);
	// Impact jolt, fading in a fraction of a second.
	if (ImpactShake > 0.01f)
	{
		ChaseCamera->SetRelativeLocation(FVector(0.f, FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * ImpactShake * ImpactShakeCm);
		ImpactShake *= FMath::Exp(-8.f * DeltaSeconds);
	}
	else if (ImpactShake > 0.f)
	{
		ImpactShake = 0.f;
		ChaseCamera->SetRelativeLocation(FVector::ZeroVector);
	}

	if (bInteriorView || bLookingBehind)
	{
		return;
	}
	// GTA-style: after a short pause in look input, swing back behind the car.
	const float SinceLook = GetWorld()->GetTimeSeconds() - LastLookInputTime;
	if (SinceLook > RecenterDelay && Speed > RecenterMinSpeedKmh)
	{
		const FRotator Current = PC->GetControlRotation();
		// In a slide the camera looks part of the way towards where the car is going.
		float TargetYaw = static_cast<float>(GetActorRotation().Yaw);
		const FVector Velocity = GetVelocity();
		if (Velocity.Size2D() > 500.f && GetSpeedKmh() > 0.f)
		{
			const float Slide = FRotator::NormalizeAxis(static_cast<float>(Velocity.Rotation().Yaw) - TargetYaw);
			TargetYaw += FMath::Clamp(Slide, -60.f, 60.f) * VelocityLookWeight;
		}
		const FRotator Target(ChasePitch, TargetYaw, 0.f);
		PC->SetControlRotation(FMath::RInterpTo(Current, Target, DeltaSeconds, RecenterSpeed));
	}
}

// --- Enter and exit --------------------------------------------------------

EGTPVehicleSide AGTPVehicle::GetPassengerSide() const
{
	return DriverSide == EGTPVehicleSide::Left ? EGTPVehicleSide::Right : EGTPVehicleSide::Left;
}

FVector AGTPVehicle::GetDoorLocation(EGTPVehicleSide Side) const
{
	FVector Offset = LeftDoorOffset;
	if (Side == EGTPVehicleSide::Right)
	{
		Offset.Y = -Offset.Y;
	}
	return GetActorTransform().TransformPosition(Offset);
}

bool AGTPVehicle::CanEnter(const ACharacter* Character) const
{
	if (!Character || Character == Driver || bWrecked)
	{
		return false;
	}
	// MassTraffic moves it along its lane every frame (SetShownByMassTraffic).
	if (bShownByMassTraffic)
	{
		return false;
	}
	if (State == EGTPVehicleState::Entering || State == EGTPVehicleState::Exiting)
	{
		return false;
	}
	// A moving car cannot be boarded (a driven one stops for a person in front of it).
	if (FMath::Abs(GetSpeedKmh()) > MaxEntrySpeedKmh)
	{
		return false;
	}
	// Never take the car from another player.
	const AController* Current = GetController();
	return !(Current && Current->IsPlayerController());
}

EGTPEntryKind AGTPVehicle::GetEntryKind() const
{
	if (Driver)
	{
		return EGTPEntryKind::Carjack;
	}
	if (bLocked && !bGlassBroken[static_cast<int32>(ZoneOfSide(DriverSide))])
	{
		return EGTPEntryKind::BreakWindow;
	}
	return EGTPEntryKind::OpenDoor;
}

float AGTPVehicle::PlayMontageOr(ACharacter* Character, const TSoftObjectPtr<UAnimMontage>& Montage, float Fallback) const
{
	if (UAnimMontage* Loaded = Montage.LoadSynchronous())
	{
		const float Length = Character->PlayAnimMontage(Loaded);
		if (Length > 0.f)
		{
			return Length;
		}
	}
	return Fallback;
}

bool AGTPVehicle::BeginEnter(ACharacter* Character, AController* DriverController)
{
	if (!CanEnter(Character) || !DriverController)
	{
		return false;
	}
	SetParkedAsleep(false);
	PendingEntry = GetEntryKind();
	EnteringCharacter = Character;
	EnteringController = DriverController;
	State = EGTPVehicleState::Entering;

	// Freeze the character at the driver door, facing the car. A walk to the
	// door with motion warping replaces this snap when the entry animations exist.
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	DriverController->SetIgnoreMoveInput(true);
	Character->MoveIgnoreActorAdd(this);
	const FVector Door = GetDoorLocation(DriverSide);
	const FVector Stand(Door.X, Door.Y, Character->GetActorLocation().Z);
	const FRotator Facing(0.f, (GetActorLocation() - Stand).GetSafeNormal2D().Rotation().Yaw, 0.f);
	Character->SetActorLocationAndRotation(Stand, Facing, false, nullptr, ETeleportType::TeleportPhysics);

	float Duration = EnterDuration;
	switch (PendingEntry)
	{
	case EGTPEntryKind::Carjack:
		if (DriverController && DriverController->IsPlayerController())
		{
			// The driver pulled out of the car calls the police.
			ReportPlayerCrime(this, EGTPCrimeKind::CarTheft, Driver != nullptr);
		}
		// The car stops where it is while its driver is pulled out.
		ClearDrivingInput();
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetBrakeInput(1.f);
			Move->SetHandbrakeInput(true);
		}
		if (ACharacter* Victim = Driver)
		{
			// Pulled out through the driver door, thrown a little further out.
			Driver = nullptr;
			const FVector Out = (Door - GetActorLocation()).GetSafeNormal2D();
			ReleaseCharacter(Victim, Door + Out * 110.f + FVector(0.f, 0.f, Victim->GetSimpleCollisionHalfHeight()), Facing);
			Victim->LaunchCharacter(Out * 250.f, true, false);
			// A non-player driver runs off, comes back at the thief, or cowers (GTPCarjackLogic.h).
			if (Cast<AAIController>(Victim->GetController()))
			{
				UGTPCarjackVictimComponent::Start(Victim, this, Character);
			}
		}
		Duration = PlayMontageOr(Character, CarjackMontage, CarjackDuration);
		break;
	case EGTPEntryKind::BreakWindow:
		if (DriverController && DriverController->IsPlayerController())
		{
			ReportPlayerCrime(this, EGTPCrimeKind::CarTheft);
		}
		BreakGlass(ZoneOfSide(DriverSide));
		TriggerAlarm();
		Duration = PlayMontageOr(Character, BreakWindowMontage, BreakWindowDuration);
		break;
	default:
		Duration = PlayMontageOr(Character, EnterMontage, EnterDuration);
		break;
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: entering (%s, %.1f s)"), *GetName(),
		*StaticEnum<EGTPEntryKind>()->GetNameStringByValue(static_cast<int64>(PendingEntry)), Duration);
	GetWorldTimerManager().SetTimer(SequenceTimer, this, &AGTPVehicle::FinishEnter, FMath::Max(Duration, 0.05f), false);
	return true;
}

void AGTPVehicle::FinishEnter()
{
	ACharacter* Character = EnteringCharacter;
	AController* DriverController = EnteringController;
	EnteringCharacter = nullptr;
	EnteringController = nullptr;
	if (!IsValid(Character) || !IsValid(DriverController))
	{
		State = EGTPVehicleState::Parked;
		return;
	}
	AttachToSeat(Character);
	Driver = Character;
	bLocked = false;
	DriverController->SetIgnoreMoveInput(false);
	State = EGTPVehicleState::Driven;
	// A parked car starts (or is hot-wired through the broken window); a car taken from its
	// driver is already running.
	if (PendingEntry != EGTPEntryKind::Carjack)
	{
		const float Start = PendingEntry == EGTPEntryKind::BreakWindow ? HotwireSeconds : EngineStartSeconds;
		EngineStartUntil = GetWorld()->GetTimeSeconds() + Start;
		GTP_TRACE(LogGTPVehicle, TEXT("%s: engine starting (%.1f s)"), *GetName(), Start);
	}
	DriverController->Possess(this);
}

void AGTPVehicle::SeatOccupant(ACharacter* Character)
{
	if (!Character || Driver)
	{
		return;
	}
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->DisableMovement();
	}
	AttachToSeat(Character);
	Driver = Character;
	if (IsDriverLiteWanted() && !Character->IsPlayerControlled())
	{
		SetDriverLite(Character, true);
	}
}

bool AGTPVehicle::IsDriverLiteWanted()
{
	static const bool bWanted = []
	{
		int32 Switch = 1;
		FParse::Value(FCommandLine::Get(), TEXT("DriverLite="), Switch);
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle: seated drivers %s (-DriverLite=%d)"), Switch != 0 ? TEXT("lite") : TEXT("whole"), Switch);
		return Switch != 0;
	}();
	return bWanted;
}

void AGTPVehicle::SetDriverLite(ACharacter* Character, bool bLite)
{
	static const FName LiteTag(TEXT("GTPDriverLite"));
	static const FName TickTag(TEXT("GTPLiteTick"));
	static const FName ActiveTag(TEXT("GTPLiteActive"));
	if (!Character || Character->Tags.Contains(LiteTag) == bLite)
	{
		return;
	}
	if (bLite)
	{
		Character->Tags.Add(LiteTag);
	}
	else
	{
		Character->Tags.Remove(LiteTag);
	}
	Character->SetActorTickEnabled(!bLite);
	if (AController* Brain = Character->GetController())
	{
		Brain->SetActorTickEnabled(!bLite);
	}
	if (bLite)
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_None);
		}
	}
	TInlineComponentArray<UActorComponent*> Parts(Character);
	for (UActorComponent* Part : Parts)
	{
		// Meshes keep their pose update: a visible driver stays seated, not frozen.
		if (Part->IsA<USkeletalMeshComponent>())
		{
			continue;
		}
		if (bLite)
		{
			if (Part->IsComponentTickEnabled())
			{
				Part->ComponentTags.AddUnique(TickTag);
				Part->SetComponentTickEnabled(false);
			}
			const bool bView = Part->IsA<UCameraComponent>() || Part->IsA<USpringArmComponent>()
				|| Part->GetClass()->GetName().Contains(TEXT("Camera"));
			if (bView && Part->IsActive())
			{
				Part->ComponentTags.AddUnique(ActiveTag);
				Part->Deactivate();
			}
		}
		else
		{
			if (Part->ComponentTags.Remove(TickTag) > 0)
			{
				Part->SetComponentTickEnabled(true);
			}
			// An AI character pulled out never needs its camera: activating a gameplay camera
			// creates a camera host and can take the player's view. Only a player gets it back.
			if (Part->ComponentTags.Remove(ActiveTag) > 0 && Character->IsPlayerControlled())
			{
				Part->Activate();
			}
		}
	}
}

void AGTPVehicle::AttachToSeat(ACharacter* Character)
{
	Character->SetActorEnableCollision(false);
	USkeletalMeshComponent* Body = GetMesh();
	if (Body && Body->DoesSocketExist(DriverSeatSocket))
	{
		Character->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, DriverSeatSocket);
	}
	else
	{
		Character->AttachToComponent(Body, FAttachmentTransformRules::KeepWorldTransform);
		Character->SetActorRelativeTransform(FTransform(FRotator::ZeroRotator, DriverSeatOffset));
	}
	Character->SetActorHiddenInGame(!bShowDriverInside);
	// A driver hidden in the seat is never posed, not even once (no update, no evaluation): the sample's
	// locomotion blueprint asserts on a NaN pelvis in its Foot Placement node when its graph is evaluated without
	// an update since the last evaluation (speed = distance / a zero delta time). Woken again in ReleaseCharacter.
	USkeletalMeshComponent* DriverMesh = Character->GetMesh();
	if (!bShowDriverInside && DriverMesh && !Character->IsPlayerControlled() && !DriverMesh->bNoSkeletonUpdate)
	{
		DriverMesh->bNoSkeletonUpdate = true;
		DriverMesh->ComponentTags.AddUnique(SeatDormantTag());
	}
}

FName AGTPVehicle::SeatDormantTag()
{
	static const FName Tag(TEXT("GTPSeatDormant"));
	return Tag;
}

void AGTPVehicle::ReleaseCharacter(ACharacter* Character, const FVector& Location, const FRotator& Rotation)
{
	SetDriverLite(Character, false);
	USkeletalMeshComponent* DriverMesh = Character->GetMesh();
	if (DriverMesh && DriverMesh->ComponentHasTag(SeatDormantTag()))
	{
		DriverMesh->ComponentTags.Remove(SeatDormantTag());
		DriverMesh->bNoSkeletonUpdate = false;
	}
	Character->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Character->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Character->SetActorHiddenInGame(false);
	Character->SetActorEnableCollision(true);
	Character->MoveIgnoreActorRemove(this);
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}

void AGTPVehicle::PanicExit(const FVector& ThreatLocation)
{
	if (!Driver || Cast<APlayerController>(GetController()))
	{
		return;
	}
	// Traffic drivers keep the car Parked while driving (HasAIDriver): they had no way out.
	if (HasAIDriver())
	{
		AIGetOut(true, ThreatLocation);
		return;
	}
	if (State != EGTPVehicleState::Driven)
	{
		return;
	}
	bFleeAfterExit = true;
	FleeFrom = ThreatLocation;
	RequestExit();
}

void AGTPVehicle::AIGetOut(bool bFlee, const FVector& From)
{
	if (!HasAIDriver() || bWrecked)
	{
		return;
	}
	bFleeAfterExit = bFlee;
	FleeFrom = From;
	SetAIDriveInput(0.f, 0.f, 0.f, false);
	// Exiting: no longer traffic (HasAIDriver), the AI driver stops ticking and the network
	// forgets the car. Stops first, the driver does not jump out of a moving car.
	State = EGTPVehicleState::Exiting;
	ClearDrivingInput();
	ExitWaitTime = 0.f;
	PollExit();
	if (State == EGTPVehicleState::Exiting)
	{
		GetWorldTimerManager().SetTimer(SequenceTimer, this, &AGTPVehicle::PollExit, ExitPollInterval, true);
	}
}

void AGTPVehicle::TriggerAlarm()
{
	if (!bAlarm || bWrecked || !GetWorld())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	const bool bWasOn = AlarmUntil > Now;
	AlarmUntil = Now + AlarmSeconds;
	if (!bWasOn)
	{
		GTP_TRACE(LogGTPVehicle, TEXT("%s: alarm"), *GetName());
		bAlarmHornOn = false;
		PulseAlarm();
		GetWorldTimerManager().SetTimer(AlarmTimer, this, &AGTPVehicle::PulseAlarm, 0.45f, true);
	}
}

void AGTPVehicle::PulseAlarm()
{
	// The horn on and off, twice a second, until AlarmUntil.
	const bool bOver = !GetWorld() || GetWorld()->GetTimeSeconds() >= AlarmUntil || bWrecked;
	bAlarmHornOn = bOver ? false : !bAlarmHornOn;
	UGTPHornComponent::SetHorn(this, bAlarmHornOn);
	if (bOver)
	{
		GetWorldTimerManager().ClearTimer(AlarmTimer);
	}
}

void AGTPVehicle::SetParkedAsleep(bool bAsleep)
{
	// A car MassTraffic moves never wakes: its body follows Mass, a simulated one would fight the teleports.
	if (bParkedAsleep == bAsleep || (bAsleep && (Driver || State != EGTPVehicleState::Parked)) || (!bAsleep && bShownByMassTraffic))
	{
		return;
	}
	bParkedAsleep = bAsleep;
	USkeletalMeshComponent* Body = GetMesh();
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetComponentTickEnabled(!bAsleep);
	}
	if (Body)
	{
		Body->SetSimulatePhysics(!bAsleep);
		if (!bAsleep)
		{
			// From rest: no speed carried over from the kinematic sleep.
			Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}
	if (!bAsleep)
	{
		NotePlaced(TEXT("parked wake"));
	}
}

void AGTPVehicle::SetShownByMassTraffic()
{
	if (bShownByMassTraffic)
	{
		// Called every frame it is shown: Mass turns every component's tick back on when it takes an actor out of its
		// pool (UMassRepresentationActorManagement::SetActorEnabled), the Chaos vehicle's included.
		UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
		if (Move && Move->IsComponentTickEnabled())
		{
			Move->SetComponentTickEnabled(false);
		}
		if (USkeletalMeshComponent* Body = GetMesh(); Body && Body->IsSimulatingPhysics())
		{
			Body->SetSimulatePhysics(false);
		}
		return;
	}
	// Kinematic as a sleeping parked car (Chaos vehicle and physics off), then held so: SetParkedAsleep(false) and the
	// entry (CanEnter) refuse such a car.
	SetParkedAsleep(true);
	bShownByMassTraffic = true;
	bAlarm = false;
	static int32 Logged = 0;
	if (Logged < 3)
	{
		++Logged;
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle shown by MassTraffic: %s (%s) at %s, kinematic (asleep=%d, simulating=%d), collision %s, nobody gets in"),
			*GetName(), *GetClass()->GetName(), *GetActorLocation().ToCompactString(), bParkedAsleep ? 1 : 0,
			GetMesh() && GetMesh()->IsSimulatingPhysics() ? 1 : 0, GetMesh() && GetMesh()->IsCollisionEnabled() ? TEXT("on") : TEXT("OFF"));
	}
}

void AGTPVehicle::RequestExit()
{
	if (State != EGTPVehicleState::Driven || !Driver)
	{
		return;
	}
	if (FMath::Abs(GetSpeedKmh()) > BailOutSpeedKmh)
	{
		DoExit(true);
		return;
	}
	State = EGTPVehicleState::Exiting;
	ClearDrivingInput();
	ExitWaitTime = 0.f;
	PollExit();
	GetWorldTimerManager().SetTimer(SequenceTimer, this, &AGTPVehicle::PollExit, ExitPollInterval, true);
}

void AGTPVehicle::PollExit()
{
	UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement();
	const float Speed = GetSpeedKmh();
	if (FMath::Abs(Speed) <= StoppedSpeedKmh || ExitWaitTime >= MaxExitWait)
	{
		GetWorldTimerManager().ClearTimer(SequenceTimer);
		DoExit(false);
		return;
	}
	ExitWaitTime += ExitPollInterval;
	if (Move)
	{
		// Brake only while rolling forward: with reverse-as-brake, braking at
		// a standstill would start reversing.
		Move->SetThrottleInput(0.f);
		Move->SetBrakeInput(Speed > StoppedSpeedKmh ? 1.f : 0.f);
		Move->SetHandbrakeInput(true);
	}
}

bool AGTPVehicle::FindExitLocation(const ACharacter* Character, FVector& OutLocation, EGTPVehicleSide& OutSide) const
{
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 35.f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPVehicleExit), false, this);
	Params.AddIgnoredActor(Character);

	// Driver door, then the other door (climbing across), then the roof.
	const EGTPVehicleSide Sides[] = {DriverSide, GetPassengerSide()};
	for (const EGTPVehicleSide Side : Sides)
	{
		const FVector Test = GetDoorLocation(Side) + FVector(0.f, 0.f, HalfHeight + 10.f);
		if (!GetWorld()->OverlapBlockingTestByChannel(Test, FQuat::Identity, ECC_Pawn, Shape, Params))
		{
			OutLocation = Test;
			OutSide = Side;
			return true;
		}
	}
	const FBox Bounds = GetComponentsBoundingBox();
	const FVector Roof(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z + HalfHeight + 5.f);
	if (!GetWorld()->OverlapBlockingTestByChannel(Roof, FQuat::Identity, ECC_Pawn, Shape, Params))
	{
		OutLocation = Roof;
		OutSide = DriverSide;
		return true;
	}
	return false;
}

ACharacter* AGTPVehicle::EjectDriverTo(const FVector& Location, const FRotator& Rotation)
{
	GetWorldTimerManager().ClearTimer(SequenceTimer);
	ACharacter* Character = Driver;
	if (!Character)
	{
		return nullptr;
	}
	AController* PawnController = GetController();
	Driver = nullptr;
	State = EGTPVehicleState::Parked;
	ReleaseCharacter(Character, Location, Rotation);
	if (PawnController && PawnController->IsPlayerController())
	{
		PawnController->Possess(Character);
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: driver taken out to %s"), *GetName(), *Location.ToCompactString());
	return Character;
}

void AGTPVehicle::Drown()
{
	if (!bWrecked)
	{
		Health = 0.f;
		SetWrecked(EGTPVehicleLoss::Drowned);
	}
	ClearDrivingInput();
	GTP_TRACE(LogGTPVehicle, TEXT("%s: drowned"), *GetName());
}

void AGTPVehicle::DoExit(bool bBailOut)
{
	GetWorldTimerManager().ClearTimer(SequenceTimer);
	ACharacter* Character = Driver;
	AController* PawnController = GetController();
	if (!Character)
	{
		State = EGTPVehicleState::Parked;
		return;
	}

	FVector Location;
	EGTPVehicleSide Side;
	if (!FindExitLocation(Character, Location, Side))
	{
		// Boxed in: stay inside, the player can drive out.
		GTP_TRACE(LogGTPVehicle, TEXT("%s: no free exit"), *GetName());
		bFleeAfterExit = false;
		// A traffic driver stays seated in a stopped car (it is no longer traffic); the
		// player can drive out.
		State = PawnController && PawnController->IsPlayerController() ? EGTPVehicleState::Driven : EGTPVehicleState::Parked;
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetBrakeInput(0.f);
			Move->SetHandbrakeInput(false);
		}
		return;
	}

	const FVector CarVelocity = GetVelocity();
	Driver = nullptr;
	State = EGTPVehicleState::Parked;
	ReleaseCharacter(Character, Location, FRotator(0.f, GetActorRotation().Yaw, 0.f));
	if (PawnController)
	{
		PawnController->Possess(Character);
	}

	if (bBailOut)
	{
		// Thrown sideways with part of the car's speed; the car rolls on.
		const FVector Out = (Location - GetActorLocation()).GetSafeNormal2D();
		Character->LaunchCharacter(Out * BailOutLaunchSpeed + CarVelocity * 0.6f + FVector(0.f, 0.f, 200.f), true, true);
		// Stumble, fall or be thrown depending on the speed (ADR-0006), when the character reacts.
		if (UGTPPlayerReactionComponent* Reaction = Character->FindComponentByClass<UGTPPlayerReactionComponent>())
		{
			Reaction->ReactToBailOut(CarVelocity);
		}
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetHandbrakeInput(false);
		}
	}
	else
	{
		PlayMontageOr(Character, ExitMontage, ExitDuration);
		if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
		{
			Move->SetBrakeInput(0.f);
			Move->SetHandbrakeInput(true);
		}
	}
	if (bFleeAfterExit)
	{
		// Threatened: runs away from the gun, no navigation mesh needed.
		bFleeAfterExit = false;
		if (AAIController* Brain = Cast<AAIController>(Character->GetController()))
		{
			FVector Away = (Location - FleeFrom).GetSafeNormal2D();
			if (Away.IsNearlyZero())
			{
				Away = -GetActorRightVector();
			}
			Brain->MoveToLocation(Location + Away * VictimFleeDistance * 2.f, 50.f, false, false);
		}
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: driver out (%s)"), *GetName(), bBailOut ? TEXT("bail out") : TEXT("stopped"));
}

// --- Damage ----------------------------------------------------------------

void AGTPVehicle::KeepChassisBodyOnly()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || Body->Bodies.Num() <= 1)
	{
		return;
	}
	static const bool bAllBodies = FParse::Param(FCommandLine::Get(), TEXT("CarAllBodies"));
	if (bAllBodies)
	{
		return;
	}
	static TSet<FName> BodiesLogged;
	if (!BodiesLogged.Contains(GetClass()->GetFName()))
	{
		BodiesLogged.Add(GetClass()->GetFName());
		LogCollisionBodies(*Body);
	}
	// The body Chaos drives: the component's root body.
	const FBodyInstance* Chassis = Body->GetBodyInstance();
	int32 BodiesOff = 0;
	int32 ConstraintsOff = 0;
	for (FConstraintInstance* Constraint : Body->Constraints)
	{
		if (Constraint && Constraint->IsValidConstraintInstance())
		{
			Constraint->TermConstraint();
			++ConstraintsOff;
		}
	}
	for (FBodyInstance* Part : Body->Bodies)
	{
		if (Part && Part != Chassis && Part->IsValidBodyInstance())
		{
			Part->TermBody();
			++BodiesOff;
		}
	}
	if (BodiesOff == 0 && ConstraintsOff == 0)
	{
		return;
	}
	static TSet<FName> Logged;
	if (!Logged.Contains(GetClass()->GetFName()))
	{
		Logged.Add(GetClass()->GetFName());
		const FBox Local = Body->CalcBounds(FTransform::Identity).GetBox();
		GTP_TRACE(LogGTPVehicle, TEXT("vehicle physics: %s keeps its chassis body (%s, bone %s) only: %d other bodies and %d constraints of %s switched off; collision %s, chassis box %.0f x %.0f x %.0f cm, bottom %.0f cm"),
			*GetClass()->GetName(), Chassis && Chassis->BodySetup.IsValid() ? TEXT("found") : TEXT("none"),
			Chassis && Chassis->BodySetup.IsValid() ? *Chassis->BodySetup->BoneName.ToString() : TEXT("-"), BodiesOff, ConstraintsOff,
			Body->GetPhysicsAsset() ? *Body->GetPhysicsAsset()->GetName() : TEXT("no physics asset"), *Body->GetCollisionProfileName().ToString(),
			Local.GetSize().X, Local.GetSize().Y, Local.GetSize().Z, Local.Min.Z);
	}
}

void AGTPVehicle::NoteIgnoredImpact(const AActor* OtherActor, float DeltaV, const TCHAR* Why)
{
	++IgnoredSinceLog;
	IgnoredMaxDeltaV = FMath::Max(IgnoredMaxDeltaV, DeltaV);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - IgnoredLogTime < 2.f)
	{
		return;
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: %d impacts ignored since the last line (%s; last from %s, up to %.0f cm/s)"), *GetName(), IgnoredSinceLog, Why,
		*GetNameSafe(OtherActor), IgnoredMaxDeltaV);
	IgnoredLogTime = Now;
	IgnoredSinceLog = 0;
	IgnoredMaxDeltaV = 0.f;
}

void AGTPVehicle::OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Two bodies of the same car touching each other is no impact.
	if (OtherActor == this)
	{
		return;
	}
	// The chassis resting on or scraping the ground under the car (drive probe).
	if (Hit.ImpactNormal.Z > 0.7f && !Cast<AGTPVehicle>(OtherActor) && !Cast<ACharacter>(OtherActor))
	{
		++GroundHitsSinceProbe;
	}
	// Crimes by the player at the wheel: running into people, cars and police.
	if (OtherActor && State == EGTPVehicleState::Driven && Cast<APlayerController>(GetController()) && FMath::Abs(GetSpeedKmh()) > 12.f)
	{
		if (OtherActor->IsA<ACharacter>())
		{
			ReportPlayerCrime(this, EGTPCrimeKind::HitPedestrian, true);
		}
		else if (OtherActor->IsA<AGTPVehicle>())
		{
			ReportPlayerCrime(this, OtherActor->ActorHasTag(GTPPolice::PoliceTag) ? EGTPCrimeKind::HitPolice : EGTPCrimeKind::HitVehicle);
		}
	}
	// Rammed or touched by the player: the traffic driver reacts (GTPTrafficReactLogic.h).
	if (const AGTPVehicle* Rammer = Cast<AGTPVehicle>(OtherActor))
	{
		bHitByPlayer |= Cast<APlayerController>(Rammer->GetController()) != nullptr;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	const float Mass = HitComponent ? HitComponent->GetMass() : 0.f;
	if (Mass <= 0.f || Now - LastImpactTime < MinTimeBetweenImpacts)
	{
		return;
	}
	// Change of speed caused by the impact, in cm/s.
	const float DeltaV = NormalImpulse.Size() / Mass;
	if (DeltaV < MinImpactSpeed)
	{
		return;
	}
	if (bPlayerAssist)
	{
		ImpactShake = FMath::Max(ImpactShake, FMath::Clamp(DeltaV / 1200.f, 0.2f, 1.f));
	}
	// Traffic does not wreck itself: a hit between two AI cars, or from a moving kinematic body (a
	// ghost car teleported along its lane, a pedestrian capsule), deals no damage unless the player's
	// car is one of the two. Ghost cars carry an infinite mass, so brushing one gave a driven car
	// enough speed change to wreck at once. -TrafficCrashDamage=1 restores the old rule.
	static const bool bTrafficCrashDamage = []
	{
		int32 Switch = 0;
		FParse::Value(FCommandLine::Get(), TEXT("TrafficCrashDamage="), Switch);
		return Switch != 0;
	}();
	const AGTPVehicle* OtherCar = Cast<AGTPVehicle>(OtherActor);
	const bool bPlayerInvolved = Cast<APlayerController>(GetController()) != nullptr
		|| (OtherCar && Cast<APlayerController>(OtherCar->GetController()) != nullptr);
	const bool bKinematicOther = OtherComp && OtherComp->Mobility == EComponentMobility::Movable && !OtherComp->IsSimulatingPhysics();
	if (!bPlayerInvolved && OtherCar && HasAIDriver() && OtherCar->HasAIDriver() && Now - LastPlacedTime > 3.f && Now - OtherCar->LastPlacedTime > 3.f)
	{
		// Two traffic cars crashed for real (not a ghost brushing or a placement): the drivers
		// react (GTPTrafficIncident.h). Reported once, by the car with the lower address.
		if (this < OtherCar)
		{
			if (const UGTPAIDriverComponent* Mine = FindComponentByClass<UGTPAIDriverComponent>())
			{
				if (AGTPTrafficNetwork* Network = Mine->GetNetwork(); Network && !Mine->IsGhost())
				{
					Network->NotifyTrafficCrash(this, const_cast<AGTPVehicle*>(OtherCar), DeltaV);
				}
			}
		}
	}
	if (!bTrafficCrashDamage && !bPlayerInvolved && (OtherCar || bKinematicOther))
	{
		NoteIgnoredImpact(OtherActor, DeltaV, TEXT("between AI cars"));
		return;
	}
	const float SincePlaced = Now - LastPlacedTime;
	if (!bTrafficCrashDamage && !bPlayerInvolved && SincePlaced < 1.5f)
	{
		NoteIgnoredImpact(OtherActor, DeltaV, LastPlacedHow);
		return;
	}
	LastImpactTime = Now;
	const float HealthBefore = Health;
	ApplyZoneDamage(ZoneFromWorldLocation(Hit.ImpactPoint), (DeltaV - MinImpactSpeed) * DamagePerImpactSpeed);
	if (bWrecked && HealthBefore > 0.f)
	{
		const USkeletalMeshComponent* Body = GetMesh();
		GTP_TRACE(LogGTPVehicle, TEXT("%s: wrecked by an impact with %s (%.0f cm/s) | %s%s, %s, %.0f km/h, %.1f s after %s"), *GetName(),
			*GetNameSafe(OtherActor), DeltaV, *GetClass()->GetName(), ActorHasTag(GTPPolice::PoliceTag) ? TEXT(" police") : TEXT(""),
			Body && Body->IsSimulatingPhysics() ? TEXT("simulated") : TEXT("kinematic"), GetSpeedKmh(), SincePlaced, LastPlacedHow);
	}
}

void AGTPVehicle::NotePlaced(const TCHAR* How)
{
	LastPlacedTime = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	LastPlacedHow = How;
}

EGTPDamageZone AGTPVehicle::ZoneFromWorldLocation(const FVector& WorldLocation) const
{
	// Normalised by a typical half length and half width, so corners split fairly.
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const float Along = Local.X / 200.f;
	const float Across = Local.Y / 88.f;
	if (FMath::Abs(Along) >= FMath::Abs(Across))
	{
		return Along >= 0.f ? EGTPDamageZone::Front : EGTPDamageZone::Rear;
	}
	return Across < 0.f ? EGTPDamageZone::Left : EGTPDamageZone::Right;
}

void AGTPVehicle::ApplyZoneDamage(EGTPDamageZone Zone, float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}
	const int32 Index = static_cast<int32>(Zone);
	ZoneDamage[Index] += Amount;
	if (!Driver && State == EGTPVehicleState::Parked)
	{
		SetParkedAsleep(false);
		TriggerAlarm();
	}

	if (const FName* Morph = DentMorphTargets.Find(Zone))
	{
		const float Dent = FullDentDamage > 0.f ? FMath::Clamp(ZoneDamage[Index] / FullDentDamage, 0.f, 1.f) : 1.f;
		GetMesh()->SetMorphTarget(*Morph, Dent);
	}
	if (!bGlassBroken[Index] && ZoneDamage[Index] >= GlassBreakDamage)
	{
		BreakGlass(Zone);
	}

	Health = FMath::Max(0.f, Health - Amount);
	OnDamageChanged(Zone, ZoneDamage[Index], Health);
	if (Health <= 0.f && !bWrecked)
	{
		SetWrecked();
	}
}

float AGTPVehicle::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Actual = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	FVector Where = GetActorLocation();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		Where = static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo.ImpactPoint;
	}
	else if (DamageCauser)
	{
		Where = DamageCauser->GetActorLocation();
	}
	ApplyZoneDamage(ZoneFromWorldLocation(Where), Actual);
	return Actual;
}

void AGTPVehicle::BreakGlass(EGTPDamageZone Zone)
{
	const int32 Index = static_cast<int32>(Zone);
	if (bGlassBroken[Index])
	{
		return;
	}
	bGlassBroken[Index] = true;
	const FName Tag = ZoneTag(Zone);
	TInlineComponentArray<UPrimitiveComponent*> Parts(this);
	for (UPrimitiveComponent* Part : Parts)
	{
		if (Part->ComponentHasTag(TEXT("Glass")) && Part->ComponentHasTag(Tag))
		{
			Part->SetVisibility(false);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	OnGlassBroken(Zone);
}

FGTPVehicleWrecked& AGTPVehicle::OnVehicleWrecked()
{
	static FGTPVehicleWrecked Lost;
	return Lost;
}

void AGTPVehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed)
	{
		OnVehicleWrecked().Broadcast(this, EGTPVehicleLoss::Destroyed);
	}
	Super::EndPlay(EndPlayReason);
}

void AGTPVehicle::SetWrecked(EGTPVehicleLoss Reason)
{
	const bool bFirst = !bWrecked;
	bWrecked = true;
	ThrottleAmount = 0.f;
	if (UChaosWheeledVehicleMovementComponent* Move = GetWheeledMovement())
	{
		Move->SetThrottleInput(0.f);
	}
	GTP_TRACE(LogGTPVehicle, TEXT("%s: wrecked"), *GetName());
	OnWrecked();
	if (bFirst)
	{
		OnVehicleWrecked().Broadcast(this, Reason);
	}
}
