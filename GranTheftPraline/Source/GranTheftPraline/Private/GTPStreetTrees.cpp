#include "GTPStreetTrees.h"

#include "GTPLog.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPStreetTrees, Log, All);

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
	const bool bLeafy = Variant == TEXT("leafy") || Variant == TEXT("light") || Variant == TEXT("impostor");
	if (!bLeafy)
	{
		return;
	}
	FString SwapArg;
	FParse::Value(FCommandLine::Get(), TEXT("TreeSwap="), SwapArg, false);
	int32 Count = -1;
	FParse::Value(FCommandLine::Get(), TEXT("TreeCount="), Count);

	// bare mesh package -> leafy mesh
	TMap<FString, UStaticMesh*> Swaps;
	TArray<FString> Pairs;
	SwapArg.ParseIntoArray(Pairs, TEXT("+"));
	for (const FString& Pair : Pairs)
	{
		FString Bare, LeafyPath;
		if (!Pair.Split(TEXT("~"), &Bare, &LeafyPath))
		{
			continue;
		}
		const FString ObjectPath = LeafyPath.Contains(TEXT(".")) ? LeafyPath : LeafyPath + TEXT(".") + FPaths::GetBaseFilename(LeafyPath);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *ObjectPath);
		if (!Mesh)
		{
			GTP_TRACE(LogGTPStreetTrees, TEXT("street trees: cannot load %s (not cooked?)"), *ObjectPath);
			continue;
		}
		Swaps.Add(Bare, Mesh);
	}
	if (Swaps.Num() == 0)
	{
		GTP_TRACE(LogGTPStreetTrees, TEXT("street trees: -Trees=%s needs -TreeSwap=<bare>~<leafy>, nothing changed"), *Variant);
		return;
	}

	// Every bare tree component, with its leafy mesh.
	TArray<TPair<UHierarchicalInstancedStaticMeshComponent*, UStaticMesh*>> Found;
	int32 All = 0;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		TArray<UHierarchicalInstancedStaticMeshComponent*> Comps;
		It->GetComponents(Comps);
		for (UHierarchicalInstancedStaticMeshComponent* Comp : Comps)
		{
			const UStaticMesh* Mesh = Comp->GetStaticMesh();
			UStaticMesh* const* Leafy = Mesh ? Swaps.Find(Mesh->GetPackage()->GetName()) : nullptr;
			if (Leafy)
			{
				Found.Emplace(Comp, *Leafy);
				All += Comp->GetInstanceCount();
			}
		}
	}

	int32 Shown = 0;
	for (const TPair<UHierarchicalInstancedStaticMeshComponent*, UStaticMesh*>& Item : Found)
	{
		UHierarchicalInstancedStaticMeshComponent* Bare = Item.Key;
		AActor* Owner = Bare->GetOwner();
		const int32 Total = Bare->GetInstanceCount();
		// TreeCount is shared between the components in proportion of their trees
		const int32 Want = Count >= 0 && All > 0 ? FMath::RoundToInt(double(Count) * Total / All) : Total;
		const TArray<int32> Keep = SpreadIndices(Total, Want);
		if (Keep.Num() == 0)
		{
			continue;
		}

		UHierarchicalInstancedStaticMeshComponent* Leafy = NewObject<UHierarchicalInstancedStaticMeshComponent>(Owner);
		Leafy->SetMobility(EComponentMobility::Static);
		Leafy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Leafy->SetStaticMesh(Item.Value);
		Leafy->SetupAttachment(Owner->GetRootComponent());
		Leafy->RegisterComponent();
		TArray<FTransform> Transforms;
		for (int32 i : Keep)
		{
			FTransform T;
			Bare->GetInstanceTransform(i, T, true);
			Transforms.Add(T);
		}
		Leafy->AddInstances(Transforms, false, true);

		// the bare component keeps every tree the leafy one does not show
		TArray<int32> Drop(Keep);
		Drop.Sort([](int32 A, int32 B) { return A > B; });
		Bare->RemoveInstances(Drop);

		ApplyVariant(Leafy, Variant);
		Shown += Keep.Num();
	}
	GTP_TRACE(LogGTPStreetTrees, TEXT("street trees: variant %s, %d of %d trees leafy"), *Variant, Shown, All);
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

	Leafy->SetCullDistances(0, CullCm);
	Leafy->ShadowCacheInvalidationBehavior = EShadowCacheInvalidationBehavior::Static;
	// The street meshes are rebuilt without Nanite by tools/assets/unreal/prepare_trees_m1.py
	// (decreasing LOD chain, impostor last). Should a Nanite mesh come through anyway, draw its
	// fallback and keep it on LOD 0: below LOD 0, Megascans Nanite trees get heavier, not lighter.
	const bool bNanite = Mesh && Mesh->HasValidNaniteData();
	if (bNanite)
	{
		Leafy->bDisallowNanite = true;
	}
	if (Variant == TEXT("light"))
	{
		Leafy->WorldPositionOffsetDisableDistance = WindCm;
		if (bNanite)
		{
			Leafy->SetForcedLodModel(1);
		}
	}
	else if (Variant == TEXT("impostor"))
	{
		Leafy->SetEvaluateWorldPositionOffset(false);
		// ForcedLodModel is 1-based: N forces LOD N-1, the last one here
		Leafy->SetForcedLodModel(Mesh ? FMath::Max(1, Mesh->GetNumLODs()) : 0);
	}
	Leafy->MarkRenderStateDirty();
}
