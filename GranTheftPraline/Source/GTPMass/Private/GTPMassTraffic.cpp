#include "GTPMassTraffic.h"

#include "GTPTraffic.h"
#include "EngineUtils.h"
#include "MassSpawner.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPMassTraffic, Log, All);

const FName UGTPMassTrafficSubsystem::SpawnerTag(TEXT("GTPMassTraffic"));

bool UGTPMassTrafficSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGTPMassTrafficSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!AGTPTrafficNetwork::IsMassTrafficRequested())
	{
		return;
	}
#if WITH_GTP_MASSTRAFFIC
	// Started in the order of their GTPMassTrafficOrder<n> tag (place_masstraffic.py): intersections,
	// then parked cars, then the driving cars that need both.
	TArray<TPair<int32, AMassSpawner*>> Spawners;
	const FString OrderPrefix = SpawnerTag.ToString() + TEXT("Order");
	for (TActorIterator<AMassSpawner> It(&InWorld); It; ++It)
	{
		if (!It->ActorHasTag(SpawnerTag))
		{
			continue;
		}
		int32 Order = 100;
		for (const FName& Tag : It->Tags)
		{
			const FString Text = Tag.ToString();
			if (Text.StartsWith(OrderPrefix))
			{
				Order = FCString::Atoi(*Text.RightChop(OrderPrefix.Len()));
			}
		}
		Spawners.Emplace(Order, *It);
	}
	Spawners.StableSort([](const TPair<int32, AMassSpawner*>& A, const TPair<int32, AMassSpawner*>& B) { return A.Key < B.Key; });
	for (const TPair<int32, AMassSpawner*>& Entry : Spawners)
	{
		Entry.Value->DoSpawning();
		++StartedSpawners;
		UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic: started %s (order %d)"), *Entry.Value->GetName(), Entry.Key);
	}
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic: %d spawner(s) started (tag %s)"), StartedSpawners, *SpawnerTag.ToString());
	if (StartedSpawners == 0)
	{
		UE_LOG(LogGTPMassTraffic, Warning, TEXT("MassTraffic: no spawner tagged %s in the map, run tools/city/unreal/place_masstraffic.py"),
			*SpawnerTag.ToString());
	}
#else
	UE_LOG(LogGTPMassTraffic, Warning, TEXT("MassTraffic: -Traffic=mass but the plugin is not installed (Plugins/Traffic and Plugins/RuleProcessor from City Sample), legacy traffic kept"));
#endif
}
