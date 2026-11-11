#include "GTPCityDetailsSwitch.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPCityDetails, Log, All);

namespace
{
	const FName DetailsTag(TEXT("GTP_Set_details"));
	const FName MergedTag(TEXT("GTP_Set_details_merged"));
}

bool UGTPCityDetailsSwitch::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

int32 UGTPCityDetailsSwitch::ModeFromCommandLine(const TCHAR* CommandLine)
{
	int32 Details = 1;
	int32 Merged = 0;
	FParse::Value(CommandLine, TEXT("CityDetails="), Details);
	FParse::Value(CommandLine, TEXT("CityDetailsMerged="), Merged);
	if (Details == 0)
	{
		return 0;
	}
	return Merged != 0 ? 2 : 1;
}

void UGTPCityDetailsSwitch::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const int32 Mode = ModeFromCommandLine(FCommandLine::Get());
	int32 Shown = 0;
	int32 Hidden = 0;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		AActor* Actor = *It;
		const bool bDetails = Actor->ActorHasTag(DetailsTag);
		const bool bMerged = Actor->ActorHasTag(MergedTag);
		if (!bDetails && !bMerged)
		{
			continue;
		}
		const bool bShow = (bDetails && Mode == 1) || (bMerged && Mode == 2);
		if (!bShow)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			++Hidden;
		}
		else
		{
			++Shown;
		}
	}
	UE_LOG(LogGTPCityDetails, Log, TEXT("city details mode %d: %d actors shown, %d hidden"), Mode, Shown, Hidden);
}
