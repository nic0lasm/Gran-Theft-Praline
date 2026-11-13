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
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
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
				// Clothes without a leader (the sample's construction script is bypassed): follow the body's pose.
				if (USkeletalMeshComponent* Clothes = Cast<USkeletalMeshComponent>(Part))
				{
					USkeletalMeshComponent* BodyComp = nullptr;
					for (UPrimitiveComponent* Other : Parts)
					{
						if (Other && Other->GetName() == BodyName)
						{
							BodyComp = Cast<USkeletalMeshComponent>(Other);
						}
					}
					if (BodyComp && Clothes != BodyComp && Clothes->GetName() != TEXT("Face") && !Clothes->LeaderPoseComponent.IsValid())
					{
						Clothes->SetLeaderPoseComponent(BodyComp);
						UE_LOG(LogGTPGameMode, Log, TEXT("  %s now follows %s's pose"), *Clothes->GetName(), *BodyComp->GetName());
					}
				}
				if (Own && Body && Part->GetName() == BodyName)
				{
					// Test: the post-process of the body runs after the retarget and may overwrite it.
					if (!FParse::Param(FCommandLine::Get(), TEXT("PlayerVisualKeepPostProcess")))
					{
						Body->SetDisablePostProcessBlueprint(true);
					}
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
					if (Own && Retargeter)
					{
						UE_LOG(LogGTPGameMode, Log, TEXT("  pawn mesh asset %s, skeleton %s"),
							Own->GetSkeletalMeshAsset() ? *Own->GetSkeletalMeshAsset()->GetPathName() : TEXT("none"),
							Own->GetSkeletalMeshAsset() && Own->GetSkeletalMeshAsset()->GetSkeleton() ? *Own->GetSkeletalMeshAsset()->GetSkeleton()->GetPathName() : TEXT("none"));
						for (TFieldIterator<FProperty> Pr(Retargeter->GetClass()); Pr; ++Pr)
						{
							if (Pr->GetName().Contains(TEXT("IKRig")) || Pr->GetName().Contains(TEXT("Source")) || Pr->GetName().Contains(TEXT("Target")))
							{
								FString RetText;
								Pr->ExportText_InContainer(0, RetText, Retargeter, Retargeter, nullptr, PPF_None);
								UE_LOG(LogGTPGameMode, Log, TEXT("  retargeter %s = %s"), *Pr->GetName(), *RetText.Left(160));
							}
						}
					}
					int32 Changed = 0;
					// The retargeter asset ships without a source IK rig: give it the pawn skeleton's rig (found by name), in memory only.
					if (Retargeter && Own)
					{
						if (FObjectProperty* SrcRig = FindFProperty<FObjectProperty>(Retargeter->GetClass(), TEXT("SourceIKRigAsset")))
						{
							if (!SrcRig->GetObjectPropertyValue_InContainer(Retargeter))
							{
								FAssetRegistryModule& Reg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
								TArray<FAssetData> Rigs;
								Reg.Get().GetAssetsByClass(FTopLevelAssetPath(TEXT("/Script/IKRig"), TEXT("IKRigDefinition")), Rigs, true);
								const FString MeshName = Own->GetSkeletalMeshAsset() ? Own->GetSkeletalMeshAsset()->GetName() : FString();
								for (const FAssetData& Rig : Rigs)
								{
									UE_LOG(LogGTPGameMode, Log, TEXT("  ik rig candidate %s"), *Rig.GetObjectPathString());
								}
								for (const FAssetData& Rig : Rigs)
								{
									const FString Name = Rig.AssetName.ToString();
									if (Name.Contains(TEXT("UEFN")) && !Name.Contains(TEXT("Metahuman")) && !Name.Contains(TEXT("UE4")))
									{
										SrcRig->SetObjectPropertyValue_InContainer(Retargeter, Rig.GetAsset());
										UE_LOG(LogGTPGameMode, Log, TEXT("  retargeter source rig set to %s"), *Rig.GetObjectPathString());
										++Changed;
										break;
									}
								}
							}
						}
					}
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
							if (FBoolProperty* Attached = FindFProperty<FBoolProperty>(It->Struct, TEXT("bUseAttachedParent")))
							{
								if (Attached->GetPropertyValue_InContainer(Node))
								{
									Attached->SetPropertyValue_InContainer(Node, false);
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
					bool bVariableChanged = false;
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
								// The node's pin is bound to this variable: it must hold the retargeter we want.
								if (It->GetName() == TEXT("IKRetargeter") && Retargeter && ObjProp->GetObjectPropertyValue_InContainer(Anim) != Retargeter)
								{
									ObjProp->SetObjectPropertyValue_InContainer(Anim, Retargeter);
									UE_LOG(LogGTPGameMode, Log, TEXT("  anim variable IKRetargeter set to %s"), *Retargeter->GetName());
									bVariableChanged = true;
								}
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
					// A node scale of 0 collapses the output: a retarget scale must be at least 1 cm.
					if (Anim)
					{
						for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
						{
							if (!It->Struct || !It->Struct->GetName().Contains(TEXT("RetargetPoseFromMesh")))
							{
								continue;
							}
							void* Node = It->ContainerPtrToValuePtr<void>(Anim);
							if (FFloatProperty* Scale = FindFProperty<FFloatProperty>(It->Struct, TEXT("TargetActorScale")))
							{
								if (Scale->GetPropertyValue_InContainer(Node) <= 0.f)
								{
									Scale->SetPropertyValue_InContainer(Node, 1.f);
									bVariableChanged = true;
								}
							}
						}
					}
					if (bVariableChanged)
					{
						Body->InitAnim(true);
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
			if (const USkeletalMeshComponent* SkelPart = Cast<USkeletalMeshComponent>(Part))
			{
				UE_LOG(LogGTPGameMode, Log, TEXT("  skel part %s: anim mode %d, anim class %s, post-process %s, leader %s, anim tick %d, parent %s, ticking %d"),
					*SkelPart->GetName(), static_cast<int32>(SkelPart->GetAnimationMode()),
					SkelPart->GetAnimInstance() ? *SkelPart->GetAnimInstance()->GetClass()->GetName() : TEXT("none"),
					SkelPart->GetPostProcessInstance() ? *SkelPart->GetPostProcessInstance()->GetClass()->GetName() : TEXT("none"),
					SkelPart->LeaderPoseComponent.IsValid() ? *SkelPart->LeaderPoseComponent->GetName() : TEXT("none"),
					static_cast<int32>(SkelPart->VisibilityBasedAnimTickOption),
					SkelPart->GetAttachParent() ? *SkelPart->GetAttachParent()->GetName() : TEXT("none"), SkelPart->IsComponentTickEnabled() ? 1 : 0);
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
	// Baked animations (retarget_player_animations.py, "MH_" prefix): the body plays them itself, no runtime retargeting.
	if (FParse::Param(FCommandLine::Get(), TEXT("PlayerVisualBaked")))
	{
		FAssetRegistryModule& Reg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> Anims;
		Reg.Get().GetAssetsByClass(UAnimSequence::StaticClass()->GetClassPathName(), Anims, true);
		TWeakObjectPtr<UAnimSequence> IdleAnim, WalkAnim, RunAnim;
		for (const FAssetData& A : Anims)
		{
			const FString N = A.AssetName.ToString();
			if (!N.StartsWith(TEXT("MH_")))
			{
				continue;
			}
			UAnimSequence* Seq = Cast<UAnimSequence>(A.GetAsset());
			if (N.Contains(TEXT("Idle"))) { IdleAnim = Seq; }
			else if (N.Contains(TEXT("Walk"))) { WalkAnim = Seq; }
			else if (N.Contains(TEXT("Run"))) { RunAnim = Seq; }
		}
		UE_LOG(LogGTPGameMode, Log, TEXT("baked player animations: idle %s walk %s run %s"),
			IdleAnim.IsValid() ? *IdleAnim->GetName() : TEXT("none"), WalkAnim.IsValid() ? *WalkAnim->GetName() : TEXT("none"), RunAnim.IsValid() ? *RunAnim->GetName() : TEXT("none"));
		if (WalkAnim.IsValid())
		{
			FTimerHandle Baked;
			TSharedRef<TWeakObjectPtr<UAnimSequence>> Current = MakeShared<TWeakObjectPtr<UAnimSequence>>();
			GetWorldTimerManager().SetTimer(Baked, FTimerDelegate::CreateLambda([WeakVisual, WeakPawn, BodyName, IdleAnim, WalkAnim, RunAnim, Current]()
			{
				AActor* V = WeakVisual.Get();
				const APawn* P = WeakPawn.Get();
				if (!V || !P)
				{
					return;
				}
				TArray<USkeletalMeshComponent*> Skels;
				V->GetComponents(Skels);
				for (USkeletalMeshComponent* Skel : Skels)
				{
					if (Skel->GetName() != BodyName)
					{
						continue;
					}
					const float Speed = P->GetVelocity().Size2D();
					UAnimSequence* Want = Speed < 20.f && IdleAnim.IsValid() ? IdleAnim.Get() : (Speed > 380.f && RunAnim.IsValid() ? RunAnim.Get() : WalkAnim.Get());
					if (Current->Get() != Want)
					{
						*Current = Want;
						Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						Skel->SetAnimation(Want);
						Skel->Play(true);
						Skel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					}
				}
			}), 0.15f, true, 2.f);
		}
	}
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
