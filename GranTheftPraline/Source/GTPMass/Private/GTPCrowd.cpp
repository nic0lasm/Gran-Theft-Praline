#include "GTPCrowd.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GTPPedestrians.h"
#include "Kismet/GameplayStatics.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPCrowd, Log, All);

// ---------------------------------------------------------------- subsystem

bool UGTPCrowdSubsystem::IsRequested()
{
	FString Mode;
	return FParse::Value(FCommandLine::Get(), TEXT("Pedestrians="), Mode) && Mode.Equals(TEXT("mass"), ESearchCase::IgnoreCase);
}

bool UGTPCrowdSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsRequested() && World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGTPCrowdSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPCrowdSubsystem, STATGROUP_Tickables);
}

void UGTPCrowdSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FString File = FPaths::ProjectContentDir() / TEXT("City/Pedestrians")
		/ (UGameplayStatics::GetCurrentLevelName(&InWorld, true) + TEXT(".walkways.csv"));
	const int32 Edges = UGTPPedestrianSubsystem::LoadWalkwaysFromCsv(File, Graph);
	if (Edges == 0)
	{
		UE_LOG(LogGTPCrowd, Warning, TEXT("crowd: off, %s not found"), *File);
		return;
	}
	int32 Override = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("PedMass="), Override))
	{
		CrowdCount = FMath::Max(0, Override);
	}
	Spawn(InWorld);
}

void UGTPCrowdSubsystem::Deinitialize()
{
	bRunning = false;
	Entities.Reset();
	if (InstanceActor)
	{
		InstanceActor->Destroy();
		InstanceActor = nullptr;
	}
	Instances = nullptr;
	Super::Deinitialize();
}

float UGTPCrowdSubsystem::GroundAt(const FVector& Where) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return PlayerGroundZ;
	}
	FHitResult Hit;
	const FVector Start(Where.X, Where.Y, PlayerGroundZ + 400.f);
	const FVector End(Where.X, Where.Y, PlayerGroundZ - 1200.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPCrowdGround), false);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint.Z;
	}
	return PlayerGroundZ;
}

bool UGTPCrowdSubsystem::PlaceAroundPlayer(GTPWalk::FWalker& Walker, GTPWalk::FRng& Rng) const
{
	for (int32 Try = 0; Try < 60; ++Try)
	{
		if (!GTPWalk::Place(Graph, Walker, Rng, Settings))
		{
			return false;
		}
		GTPWalk::FPoint P, D;
		GTPWalk::Locate(Graph, Walker, P, D);
		const float Distance = FVector::Dist2D(FVector(P.X, P.Y, 0.f), FVector(PlayerLocation.X, PlayerLocation.Y, 0.f));
		if (Distance >= MinSpawnDistance && Distance <= MaxSpawnDistance)
		{
			return true;
		}
	}
	return false;
}

void UGTPCrowdSubsystem::Spawn(UWorld& InWorld)
{
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(&InWorld, 0))
	{
		PlayerLocation = Pawn->GetActorLocation();
		PlayerGroundZ = PlayerLocation.Z - 90.f;
	}
	UMassEntitySubsystem* Mass = InWorld.GetSubsystem<UMassEntitySubsystem>();
	if (!Mass)
	{
		UE_LOG(LogGTPCrowd, Warning, TEXT("crowd: off, no Mass entity subsystem"));
		return;
	}
	FMassEntityManager& EntityManager = Mass->GetMutableEntityManager();

	// The instanced mesh: the baked walking person, else a capsule stand-in.
	InstanceActor = InWorld.SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
	Instances = NewObject<UInstancedStaticMeshComponent>(InstanceActor, TEXT("CrowdInstances"));
	UStaticMesh* Mesh = Cast<UStaticMesh>(CrowdMesh.TryLoad());
	BaseScale = FVector(1.f);
	if (!Mesh)
	{
		// The engine cylinder is 100 cm wide and 100 cm high: a person is 60 cm wide and 170 cm tall.
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		BaseScale = FVector(0.6f, 0.6f, 1.7f);
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: no %s, drawing capsule stand-ins"), *CrowdMesh.ToString());
	}
	Instances->SetStaticMesh(Mesh);
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetCastShadow(true);
	Instances->bCastContactShadow = false;
	InstanceActor->SetRootComponent(Instances);
	Instances->RegisterComponent();

	Archetype = EntityManager.CreateArchetype(TArray<const UScriptStruct*>{
		FGTPCrowdWalkerFragment::StaticStruct(), FGTPCrowdLookFragment::StaticStruct(), FGTPCrowdTag::StaticStruct()});
	GTPWalk::FRng Rng(987654321u);
	int32 Placed = 0;
	for (int32 Index = 0; Index < CrowdCount; ++Index)
	{
		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype);
		FGTPCrowdWalkerFragment& Walk = EntityManager.GetFragmentDataChecked<FGTPCrowdWalkerFragment>(Entity);
		FGTPCrowdLookFragment& Look = EntityManager.GetFragmentDataChecked<FGTPCrowdLookFragment>(Entity);
		if (PlaceAroundPlayer(Walk.Walker, Rng))
		{
			GTPWalk::FPoint P, D;
			GTPWalk::Locate(Graph, Walk.Walker, P, D);
			Walk.Location = FVector(P.X, P.Y, 0.f);
			Walk.GroundZ = GroundAt(Walk.Location);
			Walk.Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			++Placed;
		}
		Look.Scale = Rng.Range(1.f - HeightSpread, 1.f + HeightSpread);
		Look.InstanceIndex = Instances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -100000.f), BaseScale * Look.Scale), false);
		Entities.Add(Entity);
	}
	bRunning = true;
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %d people (%d placed) on %d walkway edges"), CrowdCount, Placed, static_cast<int32>(Graph.Edges.size()));
}

void UGTPCrowdSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bRunning)
	{
		return;
	}
	if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		PlayerLocation = Pawn->GetActorLocation();
		PlayerGroundZ = PlayerLocation.Z - 90.f;
	}
	if (Instances)
	{
		Instances->MarkRenderStateDirty();
	}
}

// ---------------------------------------------------------------- processors

UGTPCrowdMoveProcessor::UGTPCrowdMoveProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	bRequiresGameThreadExecution = true;
}

void UGTPCrowdMoveProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Query.AddRequirement<FGTPCrowdWalkerFragment>(EMassFragmentAccess::ReadWrite);
	Query.AddTagRequirement<FGTPCrowdTag>(EMassFragmentPresence::All);
	Query.RegisterWithProcessor(*this);
}

void UGTPCrowdMoveProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UGTPCrowdSubsystem* Crowd = Context.GetWorld() ? Context.GetWorld()->GetSubsystem<UGTPCrowdSubsystem>() : nullptr;
	if (!Crowd)
	{
		return;
	}
	const GTPWalk::FGraph& Graph = Crowd->GetGraph();
	const GTPWalk::FSettings& Settings = Crowd->GetSettings();
	const FVector Player = Crowd->GetPlayerLocation();
	const float Recycle = Crowd->GetRecycleDistance();
	Query.ForEachEntityChunk(Context, [this, Crowd, &Graph, &Settings, Player, Recycle](FMassExecutionContext& Chunk)
	{
		const float Dt = Chunk.GetDeltaTimeSeconds();
		const TArrayView<FGTPCrowdWalkerFragment> Walkers = Chunk.GetMutableFragmentView<FGTPCrowdWalkerFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			FGTPCrowdWalkerFragment& W = Walkers[Index];
			if (W.Walker.Edge < 0)
			{
				// Not placed yet (no free walkway near the player at spawn): try again.
				if (Crowd->PlaceAroundPlayer(W.Walker, Rng))
				{
					GTPWalk::FPoint P, D;
					GTPWalk::Locate(Graph, W.Walker, P, D);
					W.Location = FVector(P.X, P.Y, 0.f);
					W.GroundZ = Crowd->GroundAt(W.Location);
				}
				continue;
			}
			const GTPWalk::FStepResult R = GTPWalk::Step(Graph, W.Walker, Dt, Rng, Settings, false);
			GTPWalk::FPoint P, D;
			GTPWalk::Locate(Graph, W.Walker, P, D);
			W.Speed = R.Speed;
			const float Target = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			W.Yaw = FMath::RInterpTo(FRotator(0.f, W.Yaw, 0.f), FRotator(0.f, Target, 0.f), Dt, 8.f).Yaw;
			const FVector Previous = W.Location;
			W.Location = FVector(P.X, P.Y, 0.f);
			if (FVector::Dist2D(W.Location, Player) > Recycle)
			{
				if (Crowd->PlaceAroundPlayer(W.Walker, Rng))
				{
					GTPWalk::Locate(Graph, W.Walker, P, D);
					W.Location = FVector(P.X, P.Y, 0.f);
					W.GroundZ = Crowd->GroundAt(W.Location);
				}
			}
			else if (FVector::DistSquared2D(Previous, W.Location) > 2500.f || Rng.Next() < Dt * 0.5f)
			{
				// New ground height after a long step and, now and then, as the ground changes along the walk.
				W.GroundZ = Crowd->GroundAt(W.Location);
			}
		}
	});
}

UGTPCrowdRepresentationProcessor::UGTPCrowdRepresentationProcessor()
	: Query(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::All);
	ExecutionOrder.ExecuteAfter.Add(UGTPCrowdMoveProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
}

void UGTPCrowdRepresentationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Query.AddRequirement<FGTPCrowdWalkerFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FGTPCrowdLookFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddTagRequirement<FGTPCrowdTag>(EMassFragmentPresence::All);
	Query.RegisterWithProcessor(*this);
}

void UGTPCrowdRepresentationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UGTPCrowdSubsystem* Crowd = Context.GetWorld() ? Context.GetWorld()->GetSubsystem<UGTPCrowdSubsystem>() : nullptr;
	UInstancedStaticMeshComponent* Instances = Crowd ? Crowd->GetInstances() : nullptr;
	if (!Instances)
	{
		return;
	}
	const float YawOffset = Crowd->GetMeshYawOffset();
	const FVector BaseScale = Crowd->GetBaseScale();
	Query.ForEachEntityChunk(Context, [Instances, YawOffset, BaseScale](FMassExecutionContext& Chunk)
	{
		const TConstArrayView<FGTPCrowdWalkerFragment> Walkers = Chunk.GetFragmentView<FGTPCrowdWalkerFragment>();
		const TConstArrayView<FGTPCrowdLookFragment> Looks = Chunk.GetFragmentView<FGTPCrowdLookFragment>();
		for (int32 Index = 0; Index < Chunk.GetNumEntities(); ++Index)
		{
			const FGTPCrowdWalkerFragment& W = Walkers[Index];
			const FGTPCrowdLookFragment& L = Looks[Index];
			if (L.InstanceIndex == INDEX_NONE || W.Walker.Edge < 0)
			{
				continue;
			}
			const FTransform Transform(FRotator(0.f, W.Yaw + YawOffset, 0.f), FVector(W.Location.X, W.Location.Y, W.GroundZ), BaseScale * L.Scale);
			Instances->UpdateInstanceTransform(L.InstanceIndex, Transform, true, false, true);
		}
	});
}
