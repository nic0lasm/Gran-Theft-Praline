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
