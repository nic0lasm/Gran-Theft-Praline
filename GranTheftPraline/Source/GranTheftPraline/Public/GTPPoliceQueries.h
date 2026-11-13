#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "UObject/Object.h"
#include "GTPPoliceQueries.generated.h"

class UEnvQuery;
class AGTPPoliceDirector;

/** EQS context: the player's pawn. */
UCLASS()
class UGTPEnvContext_Player : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/** EQS context: where the police last saw or heard the player (UGTPWantedComponent::GetLastKnownLocation). */
UCLASS()
class UGTPEnvContext_LastKnown : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/**
 * EQS generator: places along the traffic lanes where a roadblock across the player's road would work
 * (AGTPPoliceDirector::CollectRoadblockSites). The query owner must be the police director.
 */
UCLASS()
class UGTPEnvGenerator_RoadblockSites : public UEnvQueryGenerator
{
	GENERATED_BODY()

public:
	UGTPEnvGenerator_RoadblockSites();
	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;
};

/**
 * The police's questions to EQS (decision of 12/11: EQS picks the points, the star rules stay in
 * GTPWantedLogic.h). The queries are built here in C++, without an asset, so they live in Git.
 *
 * Search points: a ring of points around the last known position, scored by distance from the
 * player (officers look where the player is not standing). Officers go to them in turn instead of
 * sweeping a fixed circle. -PoliceEqs=0 turns EQS off (the circle sweep of GTPPoliceTactics.h stays).
 * Roadblock sites are still chosen from the traffic lanes in code: EQS knows no lanes, a custom
 * generator over them is the next step.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPPoliceQueries : public UObject
{
	GENERATED_BODY()

public:
	static bool IsEnabled();

	/** Builds the queries. Call once. */
	void Init();

	/** Asks for search points around the last known position; the answer comes later (see GetSearchPoints). */
	void RequestSearchPoints();

	/** The best points of the last answer, best first, and when it came (world seconds, -1000 = none). */
	const TArray<FVector>& GetSearchPoints() const { return SearchPoints; }
	float GetSearchPointsTime() const { return SearchPointsTime; }

	/** Asks for roadblock sites on the lanes ahead of the player; Director is the query owner. */
	void RequestRoadblockSites(AGTPPoliceDirector* Director);
	const TArray<FVector>& GetRoadblockSites() const { return RoadblockSites; }

	/** Ring radius and spacing (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float SearchRingRadius = 2500.f;
	UPROPERTY(Config, EditAnywhere, Category = "Police")
	float SearchRingSpacing = 700.f;

private:
	void OnSearchFinished(TSharedPtr<FEnvQueryResult> Result);
	void OnRoadblockFinished(TSharedPtr<FEnvQueryResult> Result);

	UPROPERTY(Transient)
	TObjectPtr<UEnvQuery> SearchQuery;

	UPROPERTY(Transient)
	TObjectPtr<UEnvQuery> RoadblockQuery;

	TArray<FVector> RoadblockSites;
	bool bRoadblockRunning = false;
	TArray<FVector> SearchPoints;
	float SearchPointsTime = -1000.f;
	bool bRunning = false;
};
