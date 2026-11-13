#include "GTPGameMode.h"

#include "GTPHUD.h"
#include "GTPPlayerController.h"
#include "GTPTraffic.h"
#include "GTPWanted.h"
#include "GTPVehicle.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "UObject/UnrealType.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPGameMode, Log, All);

AGTPGameMode::AGTPGameMode()
{
	PlayerControllerClass = AGTPPlayerController::StaticClass();
	HUDClass = AGTPHUD::StaticClass();
}

void AGTPGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// -PlayerPawn=<class path> tries another player character (the MetaHuman, an A/B) without touching the config.
	FString PawnOverride;
	if (FParse::Value(FCommandLine::Get(), TEXT("PlayerPawn="), PawnOverride) && !PawnOverride.IsEmpty())
	{
		PlayerPawnClass = FSoftClassPath(PawnOverride);
	}
	if (PlayerPawnClass.IsNull())
	{
		return;
	}

	if (UClass* PawnClass = PlayerPawnClass.TryLoadClass<APawn>())
	{
		DefaultPawnClass = PawnClass;
	}
	else
	{
		UE_LOG(LogGTPGameMode, Warning, TEXT("PlayerPawnClass '%s' could not be loaded, using the default pawn."),
			*PlayerPawnClass.ToString());
	}
}

void AGTPGameMode::StartPlay()
{
	Super::StartPlay();
	int32 Traffic = -1;
	FParse::Value(FCommandLine::Get(), TEXT("Traffic="), Traffic);
	if (Traffic == 0)
	{
		return;
	}
	if (!TActorIterator<AGTPTrafficNetwork>(GetWorld()))
	{
		const FString File = AGTPTrafficNetwork::LanesFileFor(UGameplayStatics::GetCurrentLevelName(this, true));
		if (!FPaths::FileExists(File))
		{
			UE_LOG(LogGTPGameMode, Log, TEXT("No traffic: %s not found"), *File);
			return;
		}
		GetWorld()->SpawnActor<AGTPTrafficNetwork>();
	}
	// Police ride the traffic lanes: no lanes, no police. -Police=0 turns them off.
	if (UGTPWantedComponent::IsPoliceEnabled() && !TActorIterator<AGTPPoliceDirector>(GetWorld()))
	{
		GetWorld()->SpawnActor<AGTPPoliceDirector>();
	}
}

void AGTPGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	ApplyPlayerVisual(NewPlayer ? NewPlayer->GetPawn() : nullptr);

	if (bTestVehicleSpawned || !FParse::Param(FCommandLine::Get(), TEXT("TestVehicle")))
	{
		return;
	}
	const APawn* Pawn = NewPlayer ? NewPlayer->GetPawn() : nullptr;
	// -TestVehicleClass=/Game/... overrides the config (one car per playtest run).
	FString ClassOverride;
	if (FParse::Value(FCommandLine::Get(), TEXT("TestVehicleClass="), ClassOverride))
	{
		TestVehicleClass = FSoftClassPath(ClassOverride);
	}
	UClass* VehicleClass = TestVehicleClass.TryLoadClass<AGTPVehicle>();
	if (!Pawn || !VehicleClass)
	{
		UE_LOG(LogGTPGameMode, Warning, TEXT("-TestVehicle: no pawn yet or TestVehicleClass '%s' not loadable"), *TestVehicleClass.ToString());
		return;
	}
	bTestVehicleSpawned = true;
	const FRotator Yaw(0.f, Pawn->GetActorRotation().Yaw, 0.f);
	const FVector Location = Pawn->GetActorLocation() + Yaw.RotateVector(TestVehicleOffset);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	// Parked parallel to the street, as the player faces down it.
	const AActor* Car = GetWorld()->SpawnActor<AGTPVehicle>(VehicleClass, Location, Yaw, Params);
	UE_LOG(LogGTPGameMode, Log, TEXT("-TestVehicle: %s parked at %s"), Car ? *Car->GetName() : TEXT("nothing"), *Location.ToCompactString());
}

void AGTPGameMode::ApplyPlayerVisual(APawn* Pawn)
{
	FString Override;
	if (FParse::Value(FCommandLine::Get(), TEXT("PlayerVisual="), Override))
	{
		PlayerVisualClass = FSoftClassPath(Override);
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PlayerVisualRetargeter="), Override))
	{
		PlayerVisualRetargeter = Override;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PlayerVisualBody="), Override))
	{
		PlayerVisualBodyName = Override;
	}
	static const FName VisualTag(TEXT("GTPPlayerVisual"));
	if (!Pawn || PlayerVisualClass.IsNull() || Pawn->ActorHasTag(VisualTag))
	{
		return;
	}
	UClass* VisualClass = PlayerVisualClass.TryLoadClass<AActor>();
	if (!VisualClass)
	{
		UE_LOG(LogGTPGameMode, Warning, TEXT("PlayerVisualClass '%s' could not be loaded, the pawn keeps its own mesh."), *PlayerVisualClass.ToString());
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Visual = GetWorld()->SpawnActor<AActor>(VisualClass, Pawn->GetActorTransform(), Params);
	if (!Visual)
	{
		return;
	}
	Visual->AttachToActor(Pawn, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	Visual->SetActorRelativeLocation(PlayerVisualOffset);
	Visual->SetActorRelativeRotation(FRotator(0.f, PlayerVisualYaw, 0.f));
	Visual->SetActorEnableCollision(false);
	Pawn->Tags.AddUnique(VisualTag);
	// Shown and hidden through flags that other code (camera mode, the sample's blueprints) does not toggle,
	// enforced again 1.5 s later, with a trace of every component for the log.
	TWeakObjectPtr<AActor> WeakVisual(Visual);
	TWeakObjectPtr<APawn> WeakPawn(Pawn);
	const FString BodyName = PlayerVisualBodyName;
	const FString RetargeterPath = PlayerVisualRetargeter;
	auto Enforce = [WeakVisual, WeakPawn, BodyName, RetargeterPath](const TCHAR* When)
	{
		AActor* V = WeakVisual.Get();
		APawn* P = WeakPawn.Get();
		if (!V || !P)
		{
			return;
		}
		if (const ACharacter* Character = Cast<ACharacter>(P))
		{
			if (USkeletalMeshComponent* Own = Character->GetMesh())
			{
				// Hidden but still posed: the visual copies or retargets this pose.
				Own->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
				// Not HiddenInGame (it can stop the pose evaluation the visual copies): drawn nowhere instead.
				Own->SetVisibility(false, false);
				Own->SetRenderInMainPass(false);
				Own->SetRenderInDepthPass(false);
				Own->SetCastShadow(false);
				Own->bNoSkeletonUpdate = false;
				UE_LOG(LogGTPGameMode, Log, TEXT("  pawn mesh: anim tick option %d, noSkeletonUpdate %d, visible %d, hiddenInGame %d"),
					static_cast<int32>(Own->VisibilityBasedAnimTickOption), Own->bNoSkeletonUpdate ? 1 : 0, Own->IsVisible() ? 1 : 0, Own->bHiddenInGame ? 1 : 0);
			}
		}
		UE_LOG(LogGTPGameMode, Log, TEXT("Player visual (%s): actor at %s, hidden %d, scale %s, attached to %s"), When,
			*V->GetActorLocation().ToCompactString(), V->IsHidden() ? 1 : 0, *V->GetActorScale3D().ToCompactString(),
			V->GetAttachParentActor() ? *V->GetAttachParentActor()->GetName() : TEXT("nothing"));
		TArray<UPrimitiveComponent*> Parts;
		V->GetComponents<UPrimitiveComponent>(Parts);
		// The retarget of the MetaHuman's body reads the mesh it is attached to ("use attached parent"):
		// the body hangs from the pawn's own (hidden) mesh, which keeps posing the skeleton.
		if (const ACharacter* Character = Cast<ACharacter>(P))
		{
			USkeletalMeshComponent* Own = Character->GetMesh();
			for (UPrimitiveComponent* Part : Parts)
			{
				USkeletalMeshComponent* Body = Cast<USkeletalMeshComponent>(Part);
				if (Own && Body && Part->GetName() == BodyName && Body->GetAttachParent() != Own)
				{
					Body->AttachToComponent(Own, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
					Body->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
					UE_LOG(LogGTPGameMode, Log, TEXT("  body %s re-attached to the pawn's mesh %s"), *Body->GetName(), *Own->GetName());
				}
				if (Own && Body && Part->GetName() == BodyName)
				{
					// The body must be evaluated every frame, after the pawn's own pose (no update-rate skipping, no render gate).
					Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					Body->bEnableUpdateRateOptimizations = false;
					Body->bNoSkeletonUpdate = false;
					Body->SetComponentTickEnabled(true);
					Body->AddTickPrerequisiteComponent(Own);
				}
				if (Body && Part->GetName() == BodyName && !RetargeterPath.IsEmpty())
				{
					// The body's "Retarget Pose From Mesh" node: point it at the UEFN-to-MetaHuman retargeter.
					UAnimInstance* Anim = Body->GetAnimInstance();
					UObject* Retargeter = Anim ? StaticLoadObject(UObject::StaticClass(), nullptr, *RetargeterPath) : nullptr;
					int32 Changed = 0;
					if (Anim && Retargeter)
					{
						for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
						{
							if (!It->Struct || !It->Struct->GetName().Contains(TEXT("RetargetPoseFromMesh")))
							{
								continue;
							}
							void* Node = It->ContainerPtrToValuePtr<void>(Anim);
							FObjectProperty* Prop = FindFProperty<FObjectProperty>(It->Struct, TEXT("IKRetargeterAsset"));
							if (Prop && Prop->GetObjectPropertyValue_InContainer(Node) != Retargeter)
							{
								Prop->SetObjectPropertyValue_InContainer(Node, Retargeter);
								++Changed;
							}
						}
						// Source: name the pawn's mesh explicitly instead of relying on the attach parent resolved at init.
						for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
						{
							if (!It->Struct || !It->Struct->GetName().Contains(TEXT("RetargetPoseFromMesh")))
							{
								continue;
							}
							void* Node = It->ContainerPtrToValuePtr<void>(Anim);
							for (TFieldIterator<FProperty> Pr(It->Struct); Pr; ++Pr)
							{
								FString NodeText;
								Pr->ExportText_InContainer(0, NodeText, Node, Node, nullptr, PPF_None);
								UE_LOG(LogGTPGameMode, Log, TEXT("  node %s (%s) = %s"), *Pr->GetName(), *Pr->GetClass()->GetName(), *NodeText.Left(80));
							}
							if (FWeakObjectProperty* Src = FindFProperty<FWeakObjectProperty>(It->Struct, TEXT("SourceMeshComponent")))
							{
								if (Src->GetObjectPropertyValue_InContainer(Node) != Own)
								{
									Src->SetObjectPropertyValue_InContainer(Node, Own);
									++Changed;
								}
							}
							else if (FObjectProperty* SrcObj = FindFProperty<FObjectProperty>(It->Struct, TEXT("SourceMeshComponent")))
							{
								if (SrcObj->GetObjectPropertyValue_InContainer(Node) != Own)
								{
									SrcObj->SetObjectPropertyValue_InContainer(Node, Own);
									++Changed;
								}
							}
							if (FEnumProperty* Mode = FindFProperty<FEnumProperty>(It->Struct, TEXT("RetargetFrom")))
							{
								const int64 Custom = Mode->GetEnum()->GetValueByName(TEXT("CustomSkeletalMeshComponent"));
								if (Custom != INDEX_NONE && Mode->GetUnderlyingProperty()->GetSignedIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(Node)) != Custom)
								{
									Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(Node), Custom);
									++Changed;
								}
							}
						}
						if (Changed > 0)
						{
							Body->InitAnim(true);
						}
					}
					// The sample's retarget blueprint reads its source from this variable: set it to the pawn's mesh.
					if (Anim)
					{
						for (TFieldIterator<FProperty> It(Anim->GetClass()); It; ++It)
						{
							if (It->GetOwnerClass() == UAnimInstance::StaticClass() || It->GetOwnerClass() == UObject::StaticClass())
							{
								continue;
							}
							if (FObjectProperty* ObjProp = CastField<FObjectProperty>(*It))
							{
								if (It->GetName() == TEXT("ParentSkeletalMeshComponent") && ObjProp->GetObjectPropertyValue_InContainer(Anim) != Own)
								{
									ObjProp->SetObjectPropertyValue_InContainer(Anim, Own);
									UE_LOG(LogGTPGameMode, Log, TEXT("  anim variable ParentSkeletalMeshComponent set to %s"), *Own->GetName());
								}
							}
							FString Value;
							It->ExportText_InContainer(0, Value, Anim, Anim, nullptr, PPF_None);
							UE_LOG(LogGTPGameMode, Log, TEXT("  anim var %s (%s) = %s"), *It->GetName(), *It->GetClass()->GetName(), *Value.Left(120));
						}
					}
					UE_LOG(LogGTPGameMode, Log, TEXT("  body anim %s: retargeter %s, %d node(s) changed"),
						Anim ? *Anim->GetClass()->GetName() : TEXT("none yet"), Retargeter ? *Retargeter->GetName() : TEXT("not loaded"), Changed);
				}
			}
		}
		for (UPrimitiveComponent* Part : Parts)
		{
			USkeletalMeshComponent* Skel = Cast<USkeletalMeshComponent>(Part);
			if (Skel)
			{
				Skel->SetHiddenInGame(false, false);
				Skel->SetVisibility(true, false);
			}
			UE_LOG(LogGTPGameMode, Log, TEXT("  part %s (%s) mesh %s visible %d hiddenInGame %d at %s bounds radius %.0f"),
				*Part->GetName(), *Part->GetClass()->GetName(),
				Skel && Skel->GetSkeletalMeshAsset() ? *Skel->GetSkeletalMeshAsset()->GetName() : TEXT("-"),
				Part->IsVisible() ? 1 : 0, Part->bHiddenInGame ? 1 : 0, *Part->GetComponentLocation().ToCompactString(), Part->Bounds.SphereRadius);
		}
	};
	Enforce(TEXT("spawn"));
	FTimerHandle Later;
	GetWorldTimerManager().SetTimer(Later, FTimerDelegate::CreateLambda([Enforce]() { Enforce(TEXT("after 1.5 s")); }), 1.5f, false);
	FTimerHandle Latest;
	GetWorldTimerManager().SetTimer(Latest, FTimerDelegate::CreateLambda([Enforce]() { Enforce(TEXT("after 4 s")); }), 4.f, false);
	// Every 2 s: does the pawn's own pose move while the pawn moves? (class of its anim, speed, a foot bone)
	FTimerHandle Probe;
	GetWorldTimerManager().SetTimer(Probe, FTimerDelegate::CreateLambda([WeakPawn]()
	{
		const ACharacter* C = Cast<ACharacter>(WeakPawn.Get());
		USkeletalMeshComponent* M = C ? C->GetMesh() : nullptr;
		if (!M)
		{
			return;
		}
		const FName Foot(TEXT("foot_l"));
		const FVector FootRel = M->GetBoneIndex(Foot) != INDEX_NONE
			? M->GetSocketTransform(Foot, RTS_Component).GetLocation() : FVector::ZeroVector;
		for (USceneComponent* Child : M->GetAttachChildren())
		{
			if (const USkeletalMeshComponent* B = Cast<USkeletalMeshComponent>(Child))
			{
				const FName Hand(TEXT("hand_l"));
				UE_LOG(LogGTPGameMode, Log, TEXT("body pose probe: %s anim %s, hand_l rel %s, tick %d, update rate opt %d"), *B->GetName(),
					B->GetAnimInstance() ? *B->GetAnimInstance()->GetClass()->GetName() : TEXT("none"),
					B->GetBoneIndex(Hand) != INDEX_NONE ? *B->GetSocketTransform(Hand, RTS_Component).GetLocation().ToCompactString() : TEXT("no bone"),
					B->IsComponentTickEnabled() ? 1 : 0, B->bEnableUpdateRateOptimizations ? 1 : 0);
			}
		}
		UE_LOG(LogGTPGameMode, Log, TEXT("pawn pose probe: anim %s, actor speed %.0f, movement speed %.0f, mode %d, foot_l rel %s, anim tick %d"),
			M->GetAnimInstance() ? *M->GetAnimInstance()->GetClass()->GetName() : TEXT("none"),
			C->GetVelocity().Size(), C->GetCharacterMovement() ? C->GetCharacterMovement()->Velocity.Size() : -1.f,
			C->GetCharacterMovement() ? static_cast<int32>(C->GetCharacterMovement()->MovementMode.GetValue()) : -1,
			*FootRel.ToCompactString(), M->IsComponentTickEnabled() ? 1 : 0);
	}), 2.f, true);
	UE_LOG(LogGTPGameMode, Log, TEXT("Player visual: %s attached to %s"), *Visual->GetName(), *Pawn->GetName());
}
