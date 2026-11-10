#include "GTPStreetTrees.h"

#include "GTPLog.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPStreetTrees, Log, All);

namespace
{
	const FName LeafyTag(TEXT("GTP_LeafyTrees"));
	const FName BareTag(TEXT("GTP_BareTrees"));

	/** Removes every instance whose index is not in Keep (Keep sorted ascending). */
	void KeepOnly(UInstancedStaticMeshComponent* Comp, const TArray<int32>& Keep)
	{
		TArray<int32> Drop;
		for (int32 i = 0, k = 0; i < Comp->GetInstanceCount(); ++i)
		{
			if (k < Keep.Num() && Keep[k] == i)
			{
				++k;
			}
			else
			{
				Drop.Add(i);
			}
		}
		Comp->RemoveInstances(Drop);
	}
}

TArray<int32> UGTPStreetTreesSubsystem::SpreadIndices(int32 Total, int32 Count)
{
	TArray<int32> Out;
	if (Total <= 0 || Count <= 0)
	{
		return Out;
	}
	if (Count >= Total)
	{
		for (int32 i = 0; i < Total; ++i)
		{
			Out.Add(i);
		}
		return Out;
	}
	const double Step = double(Total) / double(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Out.AddUnique(FMath::Min(Total - 1, int32(i * Step)));
	}
	return Out;
}

bool UGTPStreetTreesSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGTPStreetTreesSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	FString Variant = TEXT("bare");
	FParse::Value(FCommandLine::Get(), TEXT("Trees="), Variant);
	Variant = Variant.ToLower();
	int32 Count = -1;
	FParse::Value(FCommandLine::Get(), TEXT("TreeCount="), Count);

	TArray<AActor*> LeafyActors;
	TMap<FString, UHierarchicalInstancedStaticMeshComponent*> BareById;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		if (It->ActorHasTag(LeafyTag))
		{
			LeafyActors.Add(*It);
		}
		else if (It->ActorHasTag(BareTag))
		{
			for (const FName& Tag : It->Tags)
			{
				const FString S = Tag.ToString();
				if (S.StartsWith(TEXT("GTP_TreeId_")))
				{
					BareById.Add(S.RightChop(11), It->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>());
				}
			}
		}
	}
	if (LeafyActors.Num() == 0)
	{
		if (Variant != TEXT("bare"))
		{
			GTP_TRACE(LogGTPStreetTrees, TEXT("street trees: -Trees=%s asked but the map has no leafy copy (swap_street_trees.py stage)"), *Variant);
		}
		return;
	}

	const bool bLeafy = Variant == TEXT("leafy") || Variant == TEXT("light") || Variant == TEXT("impostor");
	int32 Shown = 0;
	for (AActor* Actor : LeafyActors)
	{
		UHierarchicalInstancedStaticMeshComponent* Leafy = Actor->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>();
		if (!Leafy)
		{
			continue;
		}
		if (!bLeafy)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			continue;
		}
		FString Id;
		for (const FName& Tag : Actor->Tags)
		{
			if (Tag.ToString().StartsWith(TEXT("GTP_TreeId_")))
			{
				Id = Tag.ToString().RightChop(11);
			}
		}
		UHierarchicalInstancedStaticMeshComponent* Bare = BareById.FindRef(Id);
		const int32 Total = Leafy->GetInstanceCount();
		// TreeCount is shared between the ids in proportion of their trees
		int32 Want = Total;
		if (Count >= 0)
		{
			int32 All = 0;
			for (AActor* Other : LeafyActors)
			{
				if (const UInstancedStaticMeshComponent* C = Other->FindComponentByClass<UHierarchicalInstancedStaticMeshComponent>())
				{
					All += C->GetInstanceCount();
				}
			}
			Want = All > 0 ? FMath::RoundToInt(double(Count) * Total / All) : 0;
		}
		const TArray<int32> Keep = SpreadIndices(Total, Want);
		KeepOnly(Leafy, Keep);
		if (Bare && Bare->GetInstanceCount() == Total)
		{
			// the bare copy keeps every tree the leafy one does not show
			TArray<int32> Others;
			for (int32 i = 0, k = 0; i < Total; ++i)
			{
				if (k < Keep.Num() && Keep[k] == i)
				{
					++k;
				}
				else
				{
					Others.Add(i);
				}
			}
			KeepOnly(Bare, Others);
		}
		else if (Bare)
		{
			GTP_TRACE(LogGTPStreetTrees, TEXT("street trees: %s bare and leafy counts differ (%d vs %d), bare trees left as they are"),
				*Id, Bare->GetInstanceCount(), Total);
		}
		ApplyVariant(Leafy, Variant);
		Shown += Keep.Num();
	}
	GTP_TRACE(LogGTPStreetTrees, TEXT("street trees: variant %s, %d leafy trees shown"), *Variant, bLeafy ? Shown : 0);
}

void UGTPStreetTreesSubsystem::ApplyVariant(UHierarchicalInstancedStaticMeshComponent* Leafy, const FString& Variant) const
{
	int32 Shadows = 1;
	FParse::Value(FCommandLine::Get(), TEXT("TreeShadows="), Shadows);
	if (Shadows == 0)
	{
		Leafy->SetCastShadow(false);
	}
	if (Variant == TEXT("leafy"))
	{
		return;
	}
	int32 WindCm = 3000;
	int32 CullCm = 20000;
	FParse::Value(FCommandLine::Get(), TEXT("TreeWindCm="), WindCm);
	FParse::Value(FCommandLine::Get(), TEXT("TreeCullCm="), CullCm);
	const UStaticMesh* Mesh = Leafy->GetStaticMesh();
	const bool bNanite = Mesh && Mesh->IsNaniteEnabled();

	Leafy->SetCullDistances(0, CullCm);
	Leafy->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Static;
	if (bNanite)
	{
		Leafy->bDisallowNanite = true;
	}
	if (Variant == TEXT("light"))
	{
		Leafy->WorldPositionOffsetDisableDistance = WindCm;
		Leafy->bOverrideMinLOD = true;
		Leafy->MinLOD = 1;
	}
	else if (Variant == TEXT("impostor"))
	{
		Leafy->SetEvaluateWorldPositionOffset(false);
		// ForcedLodModel is 1-based: N forces LOD N-1, the last one here
		Leafy->SetForcedLodModel(Mesh ? FMath::Max(1, Mesh->GetNumLODs()) : 0);
	}
	Leafy->MarkRenderStateDirty();
}
