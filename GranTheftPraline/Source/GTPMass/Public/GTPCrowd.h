#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPWalkLogic.h"
#include "GTPCrowd.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * Pedestrian crowd on Mass (ADR-0015, docs/gameplay/pedestrians-mass.md). Hundreds of people are
 * entities that walk the walkway graph with the same engine-free walk logic as the legacy pedestrians
 * (GTPWalkLogic.h), drawn as instances of one static mesh. -Pedestrians=mass turns it on and the legacy
 * pedestrian system off (-Pedestrians=legacy or a number keeps the legacy one).
 */

/** Where a person is on the walkway graph. */
USTRUCT()
struct GTPMASS_API FGTPCrowdWalkerFragment : public FMassFragment
{
	GENERATED_BODY()

	GTPWalk::FWalker Walker;
	FVector Location = FVector::ZeroVector;
	float GroundZ = 0.f;
	float Yaw = 0.f;
	float Speed = 0.f;
};

/** How a person is drawn: the instance index in the crowd mesh and the body scale. */
USTRUCT()
struct GTPMASS_API FGTPCrowdLookFragment : public FMassFragment
{
	GENERATED_BODY()

	int32 InstanceIndex = INDEX_NONE;
	float Scale = 1.f;
};

/** Tag of every crowd entity. */
USTRUCT()
struct GTPMASS_API FGTPCrowdTag : public FMassTag
{
	GENERATED_BODY()
};

/** Moves every person along the graph; people left far behind the player are put back around the player. */
UCLASS()
class GTPMASS_API UGTPCrowdMoveProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGTPCrowdMoveProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
	GTPWalk::FRng Rng{20261113u};
};

/** Writes the transform of every person into the instanced mesh. */
UCLASS()
class GTPMASS_API UGTPCrowdRepresentationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGTPCrowdRepresentationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};

/** Owns the walkway graph of the crowd, the entities and the instanced mesh. */
UCLASS(Config = Game, DefaultConfig)
class GTPMASS_API UGTPCrowdSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** True with -Pedestrians=mass. */
	static bool IsRequested();

	const GTPWalk::FGraph& GetGraph() const { return Graph; }
	const GTPWalk::FSettings& GetSettings() const { return Settings; }
	FVector GetPlayerLocation() const { return PlayerLocation; }
	float GetRecycleDistance() const { return RecycleDistance; }
	float GetMeshYawOffset() const { return MeshYawOffset; }
	FVector GetBaseScale() const { return BaseScale; }
	float GetMinSpawnDistance() const { return MinSpawnDistance; }
	float GetMaxSpawnDistance() const { return MaxSpawnDistance; }
	UInstancedStaticMeshComponent* GetInstances() const { return Instances; }

	/** Puts the walker on a walkway between the spawn distances from the player; false when none is found. */
	bool PlaceAroundPlayer(GTPWalk::FWalker& Walker, GTPWalk::FRng& Rng) const;

	/** Ground height at a place (a downward trace; the player's ground height when there is none). */
	float GroundAt(const FVector& Where) const;

protected:
	/** People in the crowd; -PedMass=N overrides. */
	UPROPERTY(Config)
	int32 CrowdCount = 300;

	/** Person drawn as an instance (a static mesh of a walking pose); empty = a capsule stand-in. */
	UPROPERTY(Config)
	FSoftObjectPath CrowdMesh = FSoftObjectPath(TEXT("/Game/Characters/Far/SM_PedFar_A.SM_PedFar_A"));

	UPROPERTY(Config)
	float MinSpawnDistance = 2500.f;

	UPROPERTY(Config)
	float MaxSpawnDistance = 12000.f;

	/** Past this distance from the player a person is moved back around the player (cm). */
	UPROPERTY(Config)
	float RecycleDistance = 15000.f;

	UPROPERTY(Config)
	float HeightSpread = 0.06f;

	/** The mannequin faces +Y: yaw applied to the instances so they face the walking direction. */
	UPROPERTY(Config)
	float MeshYawOffset = -90.f;

private:
	void Spawn(UWorld& InWorld);

	GTPWalk::FGraph Graph;
	GTPWalk::FSettings Settings;
	FVector PlayerLocation = FVector::ZeroVector;
	float PlayerGroundZ = 0.f;
	bool bRunning = false;
	FVector BaseScale = FVector(1.f);

	UPROPERTY(Transient)
	TObjectPtr<AActor> InstanceActor;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	FMassArchetypeHandle Archetype;
	TArray<FMassEntityHandle> Entities;
};
