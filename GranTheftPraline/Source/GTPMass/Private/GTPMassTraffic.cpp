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
	for (TActorIterator<AMassSpawner> It(&InWorld); It; ++It)
	{
		if (It->ActorHasTag(SpawnerTag))
		{
			It->DoSpawning();
			++StartedSpawners;
		}
	}
	UE_LOG(LogGTPMassTraffic, Log, TEXT("MassTraffic: %d spawner(s) started (tag %s)"), StartedSpawners, *SpawnerTag.ToString());
	if (StartedSpawners == 0)
	{
		UE_LOG(LogGTPMassTraffic, Warning, TEXT("MassTraffic: no spawner tagged %s in the map, run tools/city/unreal/place_masstraffic.py"),
			*SpawnerTag.ToString());
	}
#else
	UE_LOG(LogGTPMassTraffic, Warning, TEXT("MassTraffic: -Traffic=mass but the plugin is not installed (Plugins/MassTraffic), legacy traffic kept"));
#endif
}
