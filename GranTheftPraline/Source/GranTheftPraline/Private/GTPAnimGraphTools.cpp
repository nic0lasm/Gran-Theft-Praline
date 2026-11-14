#include "GTPAnimGraphTools.h"

#include "Animation/AnimBlueprint.h"

#if WITH_EDITOR
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/Skeleton.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/UnrealType.h"

// Named, not anonymous: unity builds merge the anonymous namespaces of several files.
namespace GTPAnimGraphToolsPrivate
{
	const TCHAR* const LayerCacheName = TEXT("GTP_UpperBodyBase");

	/** Sets a property from text, by path ("Node.SlotName"), through reflection: no access to private members needed. */
	bool SetByText(UObject* Object, const TCHAR* Path, const FString& Text, FString& Error)
	{
		TArray<FString> Parts;
		FString(Path).ParseIntoArray(Parts, TEXT("."));
		const UStruct* Owner = Object->GetClass();
		void* Container = Object;
		for (int32 Index = 0; Index < Parts.Num(); ++Index)
		{
			FProperty* Prop = FindFProperty<FProperty>(Owner, *Parts[Index]);
			if (!Prop)
			{
				Error = FString::Printf(TEXT("no property %s on %s"), *Parts[Index], *Owner->GetName());
				return false;
			}
			void* Value = Prop->ContainerPtrToValuePtr<void>(Container);
			if (Index + 1 < Parts.Num())
			{
				const FStructProperty* AsStruct = CastField<FStructProperty>(Prop);
				if (!AsStruct)
				{
					Error = FString::Printf(TEXT("%s is not a struct"), *Parts[Index]);
					return false;
				}
				Owner = AsStruct->Struct.Get();
				Container = Value;
				continue;
			}
			if (!Prop->ImportText_Direct(*Text, Value, Object, PPF_None))
			{
				Error = FString::Printf(TEXT("could not set %s to %s"), Path, *Text);
				return false;
			}
		}
		return true;
	}

	FString GetString(const UObject* Object, const TCHAR* Name)
	{
		const FStrProperty* Prop = FindFProperty<FStrProperty>(Object->GetClass(), Name);
		return Prop ? Prop->GetPropertyValue_InContainer(Object) : FString();
	}

	bool IsPosePin(const UEdGraphPin* Pin)
	{
		const UObject* Type = Pin ? Pin->PinType.PinSubCategoryObject.Get() : nullptr;
		return Type == FPoseLink::StaticStruct() || Type == FComponentSpacePoseLink::StaticStruct();
	}

	/** The pose pin of that name and direction, else the Fallback-th pose pin in that direction. */
	UEdGraphPin* PosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const TCHAR* Name, int32 Fallback = 0)
	{
		if (!Node)
		{
			return nullptr;
		}
		TArray<UEdGraphPin*> Found;
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin && Pin->Direction == Direction && IsPosePin(Pin))
			{
				if (Pin->PinName == FName(Name))
				{
					return Pin;
				}
				Found.Add(Pin);
			}
		}
		return Found.IsValidIndex(Fallback) ? Found[Fallback] : nullptr;
	}

	/** A new node in the graph, its properties still to set before Place. */
	template <typename TNode>
	TNode* MakeNode(UEdGraph& Graph, int32 X, int32 Y)
	{
		TNode* Node = NewObject<TNode>(&Graph, NAME_None, RF_Transactional);
		Node->NodePosX = X;
		Node->NodePosY = Y;
		Graph.AddNode(Node, false, false);
		return Node;
	}

	/** What the editor does once a node is placed: its id, its own setup, its pins (from the properties set). */
	void Place(UEdGraphNode* Node)
	{
		Node->CreateNewGuid();
		Node->PostPlacedNewNode();
		if (Node->Pins.Num() == 0)
		{
			Node->AllocateDefaultPins();
		}
	}
}
#endif

FString UGTPAnimGraphTools::AddUpperBodyLayer(UAnimBlueprint* AnimBlueprint, FName SlotName, FName RootBone)
{
#if WITH_EDITOR
	using namespace GTPAnimGraphToolsPrivate;
	if (!AnimBlueprint || SlotName.IsNone() || RootBone.IsNone())
	{
		return TEXT("error: no animation blueprint, slot or bone");
	}
	USkeleton* Skeleton = AnimBlueprint->TargetSkeleton;
	if (Skeleton && Skeleton->GetReferenceSkeleton().FindBoneIndex(RootBone) == INDEX_NONE)
	{
		return FString::Printf(TEXT("error: no bone %s on %s"), *RootBone.ToString(), *Skeleton->GetName());
	}
	TArray<UEdGraph*> Graphs;
	AnimBlueprint->GetAllGraphs(Graphs);
	UEdGraph* AnimGraph = nullptr;
	UAnimGraphNode_Root* Root = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph || Graph->GetFName() != FName(TEXT("AnimGraph")))
		{
			continue;
		}
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Cast<UAnimGraphNode_SaveCachedPose>(Node) && GetString(Node, TEXT("CacheName")) == LayerCacheName)
			{
				return FString::Printf(TEXT("kept: %s already has the upper-body layer"), *AnimBlueprint->GetName());
			}
			if (UAnimGraphNode_Root* Found = Cast<UAnimGraphNode_Root>(Node))
			{
				AnimGraph = Graph;
				Root = Found;
			}
		}
	}
	UEdGraphPin* RootIn = PosePin(Root, EGPD_Input, TEXT("Result"));
	if (!AnimGraph || !RootIn || RootIn->LinkedTo.Num() != 1)
	{
		return FString::Printf(TEXT("error: no output pose with one input in the AnimGraph of %s"), *AnimBlueprint->GetName());
	}
	UEdGraphPin* Final = RootIn->LinkedTo[0];
	const UEdGraphSchema* Schema = AnimGraph->GetSchema();
	const int32 X = Root->NodePosX;
	const int32 Y = Root->NodePosY;
	AnimBlueprint->Modify();
	AnimGraph->Modify();
	FString Error;

	// The final pose is cached, then read twice: as the base, and as the slot's source (a pose output feeds one
	// input only). Without a montage in the slot, both branches are the same pose and the blend changes nothing.
	UAnimGraphNode_SaveCachedPose* Save = MakeNode<UAnimGraphNode_SaveCachedPose>(*AnimGraph, X - 900, Y + 300);
	Place(Save);
	bool bSet = SetByText(Save, TEXT("CacheName"), LayerCacheName, Error); // after Place: it names new caches itself

	UAnimGraphNode_UseCachedPose* UseBase = MakeNode<UAnimGraphNode_UseCachedPose>(*AnimGraph, X - 600, Y + 300);
	UAnimGraphNode_UseCachedPose* UseSlot = MakeNode<UAnimGraphNode_UseCachedPose>(*AnimGraph, X - 600, Y + 450);
	for (UAnimGraphNode_UseCachedPose* Use : {UseBase, UseSlot})
	{
		bSet = bSet && SetByText(Use, TEXT("NameOfCache"), LayerCacheName, Error);
		if (FObjectPropertyBase* Link = FindFProperty<FObjectPropertyBase>(Use->GetClass(), TEXT("SaveCachedPoseNode")))
		{
			Link->SetObjectPropertyValue_InContainer(Use, Save);
		}
		Place(Use);
	}

	UAnimGraphNode_Slot* Slot = MakeNode<UAnimGraphNode_Slot>(*AnimGraph, X - 400, Y + 450);
	bSet = bSet && SetByText(Slot, TEXT("Node.SlotName"), SlotName.ToString(), Error);
	Place(Slot);

	// Upper body from RootBone and every bone under it, at full weight from the first bone (depth 0), rotations
	// blended in mesh space so the aim keeps its direction whatever the legs and pelvis do.
	UAnimGraphNode_LayeredBoneBlend* Blend = MakeNode<UAnimGraphNode_LayeredBoneBlend>(*AnimGraph, X - 200, Y + 300);
	bSet = bSet && SetByText(Blend, TEXT("Node.LayerSetup"),
		FString::Printf(TEXT("((BranchFilters=((BoneName=\"%s\",BlendDepth=0))))"), *RootBone.ToString()), Error);
	bSet = bSet && SetByText(Blend, TEXT("Node.bMeshSpaceRotationBlend"), TEXT("True"), Error);
	Place(Blend);
	if (!bSet)
	{
		return TEXT("error: ") + Error + TEXT(" (nothing linked; do not save, the new nodes are unlinked)");
	}

	UEdGraphPin* SaveIn = PosePin(Save, EGPD_Input, TEXT("Pose"));
	UEdGraphPin* BaseOut = PosePin(UseBase, EGPD_Output, TEXT("Pose"));
	UEdGraphPin* SlotSourceOut = PosePin(UseSlot, EGPD_Output, TEXT("Pose"));
	UEdGraphPin* SlotIn = PosePin(Slot, EGPD_Input, TEXT("Source"));
	UEdGraphPin* SlotOut = PosePin(Slot, EGPD_Output, TEXT("Pose"));
	UEdGraphPin* BlendBase = PosePin(Blend, EGPD_Input, TEXT("BasePose"), 0);
	UEdGraphPin* BlendLayer = PosePin(Blend, EGPD_Input, TEXT("BlendPoses_0"), 1);
	UEdGraphPin* BlendOut = PosePin(Blend, EGPD_Output, TEXT("Pose"));
	if (!SaveIn || !BaseOut || !SlotSourceOut || !SlotIn || !SlotOut || !BlendBase || !BlendLayer || !BlendOut || BlendBase == BlendLayer)
	{
		return TEXT("error: a pose pin is missing on the new nodes (nothing linked; do not save)");
	}
	RootIn->BreakAllPinLinks();
	const TCHAR* Refused = nullptr;
	if (!Schema->TryCreateConnection(Final, SaveIn)) { Refused = TEXT("final pose -> cache"); }
	else if (!Schema->TryCreateConnection(BaseOut, BlendBase)) { Refused = TEXT("cache -> base pose"); }
	else if (!Schema->TryCreateConnection(SlotSourceOut, SlotIn)) { Refused = TEXT("cache -> slot"); }
	else if (!Schema->TryCreateConnection(SlotOut, BlendLayer)) { Refused = TEXT("slot -> upper body"); }
	else if (!Schema->TryCreateConnection(BlendOut, RootIn)) { Refused = TEXT("blend -> output pose"); }
	if (Refused)
	{
		return FString::Printf(TEXT("error: link %s refused (do not save)"), Refused);
	}
	if (Skeleton)
	{
		Skeleton->Modify();
		Skeleton->RegisterSlotNode(SlotName);
		Skeleton->MarkPackageDirty();
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(AnimBlueprint);
	FKismetEditorUtilities::CompileBlueprint(AnimBlueprint);
	if (AnimBlueprint->Status == BS_Error)
	{
		return FString::Printf(TEXT("error: %s does not compile with the layer (do not save)"), *AnimBlueprint->GetName());
	}
	AnimBlueprint->MarkPackageDirty();
	return FString::Printf(TEXT("ok: %s has an upper-body layer (slot %s from %s, mesh-space rotation, status %d)"),
		*AnimBlueprint->GetName(), *SlotName.ToString(), *RootBone.ToString(), static_cast<int32>(AnimBlueprint->Status.GetValue()));
#else
	return TEXT("error: editor only");
#endif
}
