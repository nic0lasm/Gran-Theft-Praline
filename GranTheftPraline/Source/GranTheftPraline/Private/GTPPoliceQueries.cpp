#include "GTPPoliceQueries.h"

#include "Engine/World.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_OnCircle.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Distance.h"
#include "GTPLog.h"
#include "GTPWanted.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPQueries, Log, All);

void UGTPEnvContext_Player::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const UObject* Owner = QueryInstance.Owner.Get();
	if (const APawn* Pawn = Owner ? UGameplayStatics::GetPlayerPawn(Owner, 0) : nullptr)
	{
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Pawn);
	}
}

void UGTPEnvContext_LastKnown::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	const UObject* Owner = QueryInstance.Owner.Get();
	if (const UGTPWantedComponent* Wanted = Owner ? UGTPWantedComponent::Find(Owner) : nullptr)
	{
		UEnvQueryItemType_Point::SetContextHelper(ContextData, Wanted->GetLastKnownLocation());
	}
}

UGTPEnvGenerator_RoadblockSites::UGTPEnvGenerator_RoadblockSites()
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
}

void UGTPEnvGenerator_RoadblockSites::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	const AGTPPoliceDirector* Director = Cast<AGTPPoliceDirector>(QueryInstance.Owner.Get());
	const APawn* Player = Director ? UGameplayStatics::GetPlayerPawn(Director, 0) : nullptr;
	if (!Player)
	{
		return;
	}
	TArray<FVector> Sites;
	Director->CollectRoadblockSites(*Player, Sites);
	// Generated before any test: tells the generator from the filter when a query comes back empty.
	GTP_TRACE(LogGTPQueries, TEXT("eqs: roadblock generator made %d items (player %.0f km/h)"), Sites.Num(), Player->GetVelocity().Size2D() * 0.036f);
	for (const FVector& Site : Sites)
	{
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(Site);
	}
}

FText UGTPEnvGenerator_RoadblockSites::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Roadblock sites on the traffic lanes"));
}

FText UGTPEnvGenerator_RoadblockSites::GetDescriptionDetails() const
{
	return FText::FromString(TEXT("140-240 m ahead of a fast player, on a road along their travel"));
}

bool UGTPPoliceQueries::IsEnabled()
{
	static int32 Switch = -1;
	if (Switch < 0)
	{
		Switch = 1;
		FParse::Value(FCommandLine::Get(), TEXT("PoliceEqs="), Switch);
	}
	return Switch != 0;
}

void UGTPPoliceQueries::Init()
{
	SearchQuery = NewObject<UEnvQuery>(this, TEXT("GTPSearchPoints"));
	UEnvQueryOption* Option = NewObject<UEnvQueryOption>(SearchQuery);

	UEnvQueryGenerator_OnCircle* Ring = NewObject<UEnvQueryGenerator_OnCircle>(Option);
	Ring->CircleCenter = UGTPEnvContext_LastKnown::StaticClass();
	Ring->CircleRadius.DefaultValue = SearchRingRadius;
	Ring->SpaceBetween.DefaultValue = SearchRingSpacing;
	// No navigation mesh on the map yet: the points stay where the ring puts them.
	Ring->ProjectionData.TraceMode = EEnvQueryTrace::None;
	Option->Generator = Ring;

	UEnvQueryTest_Distance* FromPlayer = NewObject<UEnvQueryTest_Distance>(Option);
	FromPlayer->TestPurpose = EEnvTestPurpose::Score;
	FromPlayer->TestMode = EEnvTestDistance::Distance2D;
	FromPlayer->DistanceTo = UGTPEnvContext_Player::StaticClass();
	FromPlayer->ScoringEquation = EEnvTestScoreEquation::Linear;
	FromPlayer->ScoringFactor.DefaultValue = 1.f;
	Option->Tests.Add(FromPlayer);

	SearchQuery->GetOptionsMutable().Add(Option);

	RoadblockQuery = NewObject<UEnvQuery>(this, TEXT("GTPRoadblockSites"));
	UEnvQueryOption* BlockOption = NewObject<UEnvQueryOption>(RoadblockQuery);
	BlockOption->Generator = NewObject<UGTPEnvGenerator_RoadblockSites>(BlockOption);
	RoadblockQuery->GetOptionsMutable().Add(BlockOption);
}

void UGTPPoliceQueries::RequestRoadblockSites(AGTPPoliceDirector* Director)
{
	UWorld* World = GetWorld();
	UEnvQueryManager* Manager = World ? UEnvQueryManager::GetCurrent(World) : nullptr;
	if (!RoadblockQuery || !Manager || !Director || bRoadblockRunning)
	{
		return;
	}
	bRoadblockRunning = true;
	FEnvQueryRequest Request(RoadblockQuery, Director);
	Manager->RunQuery(Request, EEnvQueryRunMode::AllMatching, FQueryFinishedSignature::CreateUObject(this, &UGTPPoliceQueries::OnRoadblockFinished));
}

void UGTPPoliceQueries::OnRoadblockFinished(TSharedPtr<FEnvQueryResult> Result)
{
	bRoadblockRunning = false;
	RoadblockSites.Reset();
	if (!Result.IsValid() || !Result->IsSuccessful())
	{
		GTP_TRACE(LogGTPQueries, TEXT("eqs: roadblock sites: query %s"), Result.IsValid() ? TEXT("failed (no item?)") : TEXT("gave no result"));
		return;
	}
	for (int32 Index = 0; Index < Result->Items.Num(); ++Index)
	{
		RoadblockSites.Add(Result->GetItemAsLocation(Index));
	}
	GTP_TRACE(LogGTPQueries, TEXT("eqs: %d roadblock sites"), RoadblockSites.Num());
}

void UGTPPoliceQueries::RequestSearchPoints()
{
	UWorld* World = GetWorld();
	UEnvQueryManager* Manager = World ? UEnvQueryManager::GetCurrent(World) : nullptr;
	if (!SearchQuery || !Manager || bRunning)
	{
		return;
	}
	bRunning = true;
	FEnvQueryRequest Request(SearchQuery, this);
	Manager->RunQuery(Request, EEnvQueryRunMode::AllMatching, FQueryFinishedSignature::CreateUObject(this, &UGTPPoliceQueries::OnSearchFinished));
}

void UGTPPoliceQueries::OnSearchFinished(TSharedPtr<FEnvQueryResult> Result)
{
	bRunning = false;
	if (!Result.IsValid() || !Result->IsSuccessful() || Result->Items.Num() == 0)
	{
		GTP_TRACE(LogGTPQueries, TEXT("eqs: search points: no answer"));
		return;
	}
	TArray<TPair<float, FVector>> Scored;
	Scored.Reserve(Result->Items.Num());
	for (int32 Index = 0; Index < Result->Items.Num(); ++Index)
	{
		Scored.Emplace(Result->GetItemScore(Index), Result->GetItemAsLocation(Index));
	}
	Scored.Sort([](const TPair<float, FVector>& A, const TPair<float, FVector>& B) { return A.Key > B.Key; });
	SearchPoints.Reset();
	for (const TPair<float, FVector>& Item : Scored)
	{
		SearchPoints.Add(Item.Value);
	}
	SearchPointsTime = GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
	GTP_TRACE(LogGTPQueries, TEXT("eqs: %d search points"), SearchPoints.Num());
}
