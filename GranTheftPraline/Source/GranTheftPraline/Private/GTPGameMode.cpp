#include "GTPGameMode.h"

#include "GTPHUD.h"
#include "GTPPlayerController.h"
#include "GTPTraffic.h"
#include "GTPWanted.h"
#include "GTPWeapon.h"
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
	// -Traffic=mass reads as 0 as a number: the network still runs (lights, parked cars, police).
	if (!AGTPTrafficNetwork::IsMassTrafficRequested())
	{
		FParse::Value(FCommandLine::Get(), TEXT("Traffic="), Traffic);
	}
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

/**
 * Points the body blueprint's "Retarget Pose From Mesh" nodes, and the variables bound to their pins, at the
 * pawn's mesh and the retargeter. Returns how many settings it had to change; Seen, when given, gets what the
 * nodes and variables held before.
 */
static int32 PointBodyRetarget(UAnimInstance* Anim, USkeletalMeshComponent* Own, UObject* Retargeter, FString* Seen)
{
	if (!Anim || !Retargeter)
	{
		return 0;
	}
	int32 Changed = 0;
	auto NameOf = [](const UObject* Object) { return Object ? Object->GetName() : FString(TEXT("none")); };
	auto Point = [&Changed, Seen, &NameOf](FObjectPropertyBase* Prop, void* Container, UObject* Wanted, const TCHAR* Label)
	{
		UObject* Held = Prop->GetObjectPropertyValue_InContainer(Container);
		if (Seen)
		{
			Seen->Appendf(TEXT("%s %s; "), Label, *NameOf(Held));
		}
		if (Held != Wanted)
		{
			Prop->SetObjectPropertyValue_InContainer(Container, Wanted);
			++Changed;
		}
	};
	for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
	{
		if (!It->Struct || !It->Struct->GetName().Contains(TEXT("RetargetPoseFromMesh")))
		{
			continue;
		}
		void* Node = It->ContainerPtrToValuePtr<void>(Anim);
		if (FObjectPropertyBase* Asset = FindFProperty<FObjectPropertyBase>(It->Struct, TEXT("IKRetargeterAsset")))
		{
			Point(Asset, Node, Retargeter, TEXT("node asset"));
		}
		// Source: the pawn's mesh, named explicitly instead of the attach parent resolved at init.
		if (FObjectPropertyBase* Source = FindFProperty<FObjectPropertyBase>(It->Struct, TEXT("SourceMeshComponent")))
		{
			Point(Source, Node, Own, TEXT("node source"));
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
		// A node scale of 0 collapses the output: a retarget scale must be at least 1.
		if (FFloatProperty* Scale = FindFProperty<FFloatProperty>(It->Struct, TEXT("TargetActorScale")))
		{
			if (Scale->GetPropertyValue_InContainer(Node) <= 0.f)
			{
				Scale->SetPropertyValue_InContainer(Node, 1.f);
				++Changed;
			}
		}
	}
	// The sample's retarget blueprint feeds the node's pins from these variables.
	for (TFieldIterator<FObjectProperty> It(Anim->GetClass()); It; ++It)
	{
		if (It->GetOwnerClass() == UAnimInstance::StaticClass() || It->GetOwnerClass() == UObject::StaticClass())
		{
			continue;
		}
		if (It->GetName() == TEXT("IKRetargeter"))
		{
			Point(*It, Anim, Retargeter, TEXT("var IKRetargeter"));
		}
		else if (It->GetName() == TEXT("ParentSkeletalMeshComponent"))
		{
			Point(*It, Anim, Own, TEXT("var ParentSkeletalMeshComponent"));
		}
	}
	// The visual's own blueprint may hand its retargeter to the anim blueprint at every update: any
	// retargeter-typed variable of the actor holds ours too.
	if (AActor* Visual = Anim->GetOwningActor())
	{
		for (TFieldIterator<FObjectProperty> It(Visual->GetClass()); It; ++It)
		{
			if (It->PropertyClass && Retargeter->GetClass()->IsChildOf(It->PropertyClass) && It->PropertyClass != UObject::StaticClass())
			{
				Point(*It, Visual, Retargeter, *FString::Printf(TEXT("actor var %s"), *It->GetName()));
			}
		}
	}
	return Changed;
}

/**
 * Points the MetaHuman body's "Retarget Pose From Mesh" at the pawn's own (hidden) mesh and the
 * UEFN-to-MetaHuman retargeter, in memory. Run at spawn, and again whenever the body goes back to its
 * animation blueprint (the baked mode returns to it while a weapon is drawn).
 */
// A UFUNCTION with one int32 parameter (Set) or one int32 result (Get), called by reflection: the grooms' class
// (HairStrands plugin) is not a dependency of this module.
static bool GTPCallIntSetter(UObject* Object, const TCHAR* Name, int32 Value)
{
	UFunction* Fn = Object ? Object->FindFunction(FName(Name)) : nullptr;
	const FIntProperty* Arg = Fn ? CastField<FIntProperty>(Fn->PropertyLink) : nullptr;
	if (!Fn || !Arg || Fn->NumParms != 1 || Fn->GetReturnProperty())
	{
		return false;
	}
	TArray<uint8> Buffer;
	Buffer.SetNumZeroed(Fn->ParmsSize);
	Arg->SetPropertyValue_InContainer(Buffer.GetData(), Value);
	Object->ProcessEvent(Fn, Buffer.GetData());
	return true;
}

static FString GTPReadGroomLOD(UObject* Groom)
{
	UFunction* Fn = Groom ? Groom->FindFunction(FName(TEXT("GetForcedLOD"))) : nullptr;
	const FIntProperty* Result = Fn ? CastField<FIntProperty>(Fn->GetReturnProperty()) : nullptr;
	if (Fn && Result && Fn->NumParms == 1)
	{
		TArray<uint8> Buffer;
		Buffer.SetNumZeroed(Fn->ParmsSize);
		Groom->ProcessEvent(Fn, Buffer.GetData());
		return FString::FromInt(Result->GetPropertyValue_InContainer(Buffer.GetData()));
	}
	for (const TCHAR* Name : {TEXT("LODForcedIndex"), TEXT("ForcedLOD")})
	{
		if (const FIntProperty* Prop = Groom ? FindFProperty<FIntProperty>(Groom->GetClass(), Name) : nullptr)
		{
			return FString::FromInt(Prop->GetPropertyValue_InContainer(Groom));
		}
	}
	return TEXT("?");
}

/** Index of the first LOD of the first hair group whose geometry is cards, from the exported HairGroupsLOD text. */
static int32 GTPFirstCardsLOD(const FString& Text, int32& OutCount)
{
	OutCount = 0;
	const int32 Start = Text.Find(TEXT("Lods=("));
	if (Start == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	int32 Depth = 0;
	int32 Index = 0;
	int32 EntryStart = Start + 6;
	int32 Found = INDEX_NONE;
	for (int32 At = EntryStart; At < Text.Len(); ++At)
	{
		const TCHAR C = Text[At];
		if (C == TEXT('('))
		{
			++Depth;
		}
		else if (C == TEXT(')'))
		{
			if (Depth == 0)
			{
				OutCount = Index + 1;
				break;
			}
			if (--Depth == 0 && Found == INDEX_NONE && Text.Mid(EntryStart, At - EntryStart + 1).Contains(TEXT("GeometryType=Cards")))
			{
				Found = Index;
			}
		}
		else if (C == TEXT(',') && Depth == 0)
		{
			++Index;
			EntryStart = At + 1;
		}
	}
	return Found;
}

/**
 * The player visual's grooms. A groom without an asset, or whose name has one of HiddenWords, is hidden in game and
 * stops ticking (its tick is the game-thread cost); the others are asked for ForcedLOD (GPU cost only). Logged with
 * the real state (visible flag, hidden in game, tick), so a blueprint that changes one of them shows.
 */
static void GTPThinPlayerGrooms(AActor* Visual, const TArray<FString>& HiddenWords, int32 ForcedLOD, bool bDescribe)
{
	TArray<UPrimitiveComponent*> Parts;
	if (Visual)
	{
		Visual->GetComponents<UPrimitiveComponent>(Parts);
	}
	for (UPrimitiveComponent* Part : Parts)
	{
		if (!Part || !Part->GetClass()->GetName().Contains(TEXT("GroomComponent")))
		{
			continue;
		}
		const FString PartName = Part->GetName();
		const FObjectPropertyBase* AssetProp = FindFProperty<FObjectPropertyBase>(Part->GetClass(), TEXT("GroomAsset"));
		UObject* Asset = AssetProp ? AssetProp->GetObjectPropertyValue_InContainer(Part) : nullptr;
		const bool bNamed = HiddenWords.ContainsByPredicate([&PartName](const FString& Word) { return !Word.IsEmpty() && PartName.Contains(Word); });
		if (!Asset || bNamed)
		{
			const bool bChanged = !Part->bHiddenInGame || Part->IsComponentTickEnabled();
			Part->SetVisibility(false, false);
			Part->SetHiddenInGame(true, false);
			Part->SetComponentTickEnabled(false);
			if (bDescribe || bChanged)
			{
				UE_LOG(LogGTPGameMode, Log, TEXT("  groom %s (%s): hidden in game, no tick%s"), *PartName,
					Asset ? TEXT("not seen from the camera") : TEXT("no groom asset"),
					bChanged && !bDescribe ? TEXT(" (put back by the blueprint, set again)") : TEXT(""));
			}
			continue;
		}
		const FString Was = GTPReadGroomLOD(Part);
		const bool bSet = ForcedLOD >= 0 && GTPCallIntSetter(Part, TEXT("SetForcedLOD"), ForcedLOD);
		FString LodText;
		if (bDescribe)
		{
			// Which LODs of the asset are strands, cards or meshes: the export leaves defaults out (strands).
			if (const FProperty* LodProp = FindFProperty<FProperty>(Asset->GetClass(), TEXT("HairGroupsLOD")))
			{
				FString Lods;
				LodProp->ExportTextItem_Direct(Lods, LodProp->ContainerPtrToValuePtr<void>(Asset), nullptr, Asset, PPF_None);
				int32 Count = 0;
				const int32 Cards = GTPFirstCardsLOD(Lods, Count);
				LodText = FString::Printf(TEXT("; asset %s: %d LODs, first cards LOD %d"), *Asset->GetName(), Count, Cards);
			}
		}
		UE_LOG(LogGTPGameMode, Log, TEXT("  groom %s: visible %d, hidden in game %d, ticking %d; forced LOD was %s, %s, reads %s%s"), *PartName,
			Part->IsVisible() ? 1 : 0, Part->bHiddenInGame ? 1 : 0, Part->IsComponentTickEnabled() ? 1 : 0, *Was,
			bSet ? *FString::Printf(TEXT("set to %d"), ForcedLOD) : TEXT("not set (SetForcedLOD not reachable by reflection)"),
			*GTPReadGroomLOD(Part), *LodText);
	}
}

static void ConfigureBodyRetarget(USkeletalMeshComponent* Body, USkeletalMeshComponent* Own, const FString& RetargeterPath)
{
	if (!Body || RetargeterPath.IsEmpty())
	{
		return;
	}
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
		// What the blueprint holds before we touch it (its retarget nodes and its variables), for the log.
		for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
		{
			if (It->Struct && It->Struct->GetName().Contains(TEXT("RetargetPoseFromMesh")))
			{
				void* Node = It->ContainerPtrToValuePtr<void>(Anim);
				for (TFieldIterator<FProperty> Pr(It->Struct); Pr; ++Pr)
				{
					FString NodeText;
					Pr->ExportText_InContainer(0, NodeText, Node, Node, nullptr, PPF_None);
					UE_LOG(LogGTPGameMode, Log, TEXT("  node %s (%s) = %s"), *Pr->GetName(), *Pr->GetClass()->GetName(), *NodeText.Left(80));
				}
			}
		}
		for (TFieldIterator<FProperty> It(Anim->GetClass()); It; ++It)
		{
			if (It->GetOwnerClass() == UAnimInstance::StaticClass() || It->GetOwnerClass() == UObject::StaticClass())
			{
				continue;
			}
			FString Value;
			It->ExportText_InContainer(0, Value, Anim, Anim, nullptr, PPF_None);
			UE_LOG(LogGTPGameMode, Log, TEXT("  anim var %s (%s) = %s"), *It->GetName(), *It->GetClass()->GetName(), *Value.Left(120));
		}
		Changed += PointBodyRetarget(Anim, Own, Retargeter, nullptr);
	}
	int32 Undone = 0;
	if (Changed > 0)
	{
		// Re-initialised once, then pointed again without another re-initialisation: the blueprint's own
		// initialisation may put back the sample's retargeter or source, and must not have the last word.
		Body->InitAnim(true);
		Anim = Body->GetAnimInstance();
		Undone = PointBodyRetarget(Anim, Own, Retargeter, nullptr);
	}
	UE_LOG(LogGTPGameMode, Log, TEXT("  body anim %s: retargeter %s, %d setting(s) changed, %d undone by the blueprint's initialisation and set again"),
		Anim ? *Anim->GetClass()->GetName() : TEXT("none yet"), Retargeter ? *Retargeter->GetName() : TEXT("not loaded"), Changed, Undone);
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
	FString GroomMode;
	const bool bGroomsFull = FParse::Value(FCommandLine::Get(), TEXT("PlayerGrooms="), GroomMode) && GroomMode == TEXT("full");
	const TArray<FString> HiddenGrooms = PlayerGroomsHidden;
	const int32 GroomLOD = PlayerGroomForcedLOD;
	auto Enforce = [WeakVisual, WeakPawn, BodyName, RetargeterPath, bGroomsFull, HiddenGrooms, GroomLOD](const TCHAR* When)
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
					// A face without its own animation (a crowd head whose skeleton the MetaHuman face blueprint does
					// not take, Nicolito): it follows the body by bone name (head, neck), or it stays in its
					// reference pose while the body moves.
					if (BodyComp && Clothes->GetName() == TEXT("Face") && !Clothes->GetAnimInstance() && !Clothes->LeaderPoseComponent.IsValid())
					{
						Clothes->SetLeaderPoseComponent(BodyComp);
						UE_LOG(LogGTPGameMode, Log, TEXT("  Face has no animation of its own: it now follows %s's pose"), *BodyComp->GetName());
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
					ConfigureBodyRetarget(Body, Own, RetargeterPath);
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
		if (!bGroomsFull)
		{
			GTPThinPlayerGrooms(V, HiddenGrooms, GroomLOD, FCString::Strcmp(When, TEXT("spawn")) == 0);
		}
	};
	Enforce(TEXT("spawn"));
	FTimerHandle Later;
	GetWorldTimerManager().SetTimer(Later, FTimerDelegate::CreateLambda([Enforce]() { Enforce(TEXT("after 1.5 s")); }), 1.5f, false);
	FTimerHandle Latest;
	GetWorldTimerManager().SetTimer(Latest, FTimerDelegate::CreateLambda([Enforce]() { Enforce(TEXT("after 4 s")); }), 4.f, false);
	// Baked animations (retarget_player_animations.py, "MH_" prefix): the body plays them itself, no runtime retargeting.
	if (bPlayerVisualBaked || FParse::Param(FCommandLine::Get(), TEXT("PlayerVisualBaked")))
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
			GetWorldTimerManager().SetTimer(Baked, FTimerDelegate::CreateLambda([WeakVisual, WeakPawn, BodyName, RetargeterPath, IdleAnim, WalkAnim, RunAnim, Current]()
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
					// A weapon out: the body follows the pawn's pose again (runtime retarget), so drawing, aiming
					// and reloading show; back to the baked clips once it is holstered.
					const ACharacter* PawnCharacter = Cast<ACharacter>(P);
					const UGTPWeaponComponent* Gun = PawnCharacter ? PawnCharacter->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
					if (Gun && Gun->IsDrawn())
					{
						if (Skel->GetAnimationMode() != EAnimationMode::AnimationBlueprint)
						{
							Skel->SetAnimationMode(EAnimationMode::AnimationBlueprint);
							ConfigureBodyRetarget(Skel, PawnCharacter->GetMesh(), RetargeterPath);
							*Current = nullptr;
							UE_LOG(LogGTPGameMode, Log, TEXT("baked body: weapon drawn, runtime retarget (%s)"),
								Skel->GetAnimInstance() ? *Skel->GetAnimInstance()->GetClass()->GetName() : TEXT("no anim"));
						}
						continue;
					}
					const float Speed = P->GetVelocity().Size2D();
					UAnimSequence* Want = Speed < 20.f && IdleAnim.IsValid() ? IdleAnim.Get() : (Speed > 380.f && RunAnim.IsValid() ? RunAnim.Get() : WalkAnim.Get());
					if (Current->Get() != Want)
					{
						*Current = Want;
						// No displacement of its own: the pawn moves the body.
						for (const TCHAR* Name : {TEXT("bForceRootLock"), TEXT("ForceRootLock")})
						{
							if (FBoolProperty* Lock = FindFProperty<FBoolProperty>(Want->GetClass(), Name))
							{
								Lock->SetPropertyValue_InContainer(Want, true);
							}
						}
						Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
						Skel->SetAnimation(Want);
						Skel->Play(true);
						Skel->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					}
				}
			}), 0.15f, true, 2.f);
		}
	}
	// Four times a second: the body's blueprint (or the visual's) puts the sample's retargeter back now and then
	// (twice in 349 s on candidate 31: RTG_UEFN_to_Metahuman_nrw, which has no source rig, so the body falls
	// back to its A pose); point it at ours again at once. Silent unless something had changed.
	GetWorldTimerManager().SetTimer(PlayerBodyKeepTimer, FTimerDelegate::CreateLambda([WeakPawn, RetargeterPath]()
	{
		const ACharacter* C = Cast<ACharacter>(WeakPawn.Get());
		USkeletalMeshComponent* M = C ? C->GetMesh() : nullptr;
		if (!M || RetargeterPath.IsEmpty())
		{
			return;
		}
		UObject* Retargeter = nullptr;
		for (USceneComponent* Child : M->GetAttachChildren())
		{
			USkeletalMeshComponent* B = Cast<USkeletalMeshComponent>(Child);
			if (!B || B->GetAnimationMode() != EAnimationMode::AnimationBlueprint || !B->GetAnimInstance())
			{
				continue;
			}
			Retargeter = Retargeter ? Retargeter : StaticLoadObject(UObject::StaticClass(), nullptr, *RetargeterPath);
			FString Seen;
			if (const int32 Drift = PointBodyRetarget(B->GetAnimInstance(), M, Retargeter, &Seen))
			{
				UE_LOG(LogGTPGameMode, Log, TEXT("body retarget: %d setting(s) put back by a blueprint (%s), set again"), Drift, *Seen);
			}
		}
	}), 0.25f, true, 0.25f);
	// Every 2 s: does the pawn's own pose move while the pawn moves? (class of its anim, speed, a foot bone)
	FTimerHandle Probe;
	GetWorldTimerManager().SetTimer(Probe, FTimerDelegate::CreateLambda([WeakPawn, RetargeterPath]()
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
			if (USkeletalMeshComponent* B = Cast<USkeletalMeshComponent>(Child))
			{
				// The retarget settings the body's blueprint holds now: anything its graph put back is set again.
				if (B->GetAnimationMode() == EAnimationMode::AnimationBlueprint && B->GetAnimInstance() && !RetargeterPath.IsEmpty())
				{
					FString Seen;
					UObject* Retargeter = StaticLoadObject(UObject::StaticClass(), nullptr, *RetargeterPath);
					const int32 Drift = PointBodyRetarget(B->GetAnimInstance(), M, Retargeter, &Seen);
					UE_LOG(LogGTPGameMode, Log, TEXT("body retarget probe: %s%s"), *Seen,
						Drift > 0 ? *FString::Printf(TEXT("%d setting(s) had drifted, set again"), Drift) : TEXT("as set"));
				}
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
