#include "GTPCrowd.h"
#include "Materials/MaterialInstance.h"
#include "Components/SkeletalMeshComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "GTPAudioWorld.h"
#include "GTPPedestrians.h"
#include "GTPThreat.h"
#include "Kismet/GameplayStatics.h"
#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassExecutionContext.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#if WITH_GTP_MASSTRAFFIC
#include "GTPTraffic.h"
#include "MassMovementFragments.h"
#include "MassTrafficFragments.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGTPCrowd, Log, All);

namespace GTPCrowdLocal
{
/** A person seen by the MassTraffic cars: Epic's "Traffic Obstacle" radius for a walker (cm). */
constexpr float TrafficObstacleRadius = 40.f;
}

// ---------------------------------------------------------------- subsystem

bool UGTPCrowdSubsystem::IsRequested()
{
	// The default: -Pedestrians=legacy or a number (legacy density) keeps the old pedestrian system.
	FString Mode;
	return !FParse::Value(FCommandLine::Get(), TEXT("Pedestrians="), Mode) || Mode.Equals(TEXT("mass"), ESearchCase::IgnoreCase);
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
	// Hear the shots even when no near character could be dressed (no mesh yet): the far crowd still runs.
	if (UGTPAudioWorld* Audio = InWorld.GetSubsystem<UGTPAudioWorld>())
	{
		LoudHandle = Audio->OnLoudSound.AddUObject(this, &UGTPCrowdSubsystem::OnLoud);
	}
}

void UGTPCrowdSubsystem::Deinitialize()
{
	bRunning = false;
	if (UGTPAudioWorld* Audio = GetWorld() ? GetWorld()->GetSubsystem<UGTPAudioWorld>() : nullptr)
	{
		Audio->OnLoudSound.Remove(LoudHandle);
	}
	for (AGTPPedestrian* A : NearActors)
	{
		if (A)
		{
			A->Destroy();
		}
	}
	NearActors.Reset();
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

	TArray<const UScriptStruct*> Composition{
		FGTPCrowdWalkerFragment::StaticStruct(), FGTPCrowdLookFragment::StaticStruct(), FGTPCrowdTag::StaticStruct()};
#if WITH_GTP_MASSTRAFFIC
	// With MassTraffic cars in the map, a person also carries what Epic's "Traffic Obstacle" trait
	// gives (UMassTrafficObstacleTrait: transform, velocity, radius), so that the cars can brake for
	// them. The tag FMassTrafficObstacleTag itself is worn on a crossing only (UGTPCrowdMoveProcessor).
	const bool bTrafficObstacles = AGTPTrafficNetwork::IsMassTrafficWanted(&InWorld);
	if (bTrafficObstacles)
	{
		Composition.Append({FTransformFragment::StaticStruct(), FMassVelocityFragment::StaticStruct(), FAgentRadiusFragment::StaticStruct()});
	}
#endif
	Archetype = EntityManager.CreateArchetype(Composition);
	GTPWalk::FRng Rng(987654321u);
	int32 Placed = 0;
	for (int32 Index = 0; Index < CrowdCount; ++Index)
	{
		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype);
		FGTPCrowdWalkerFragment& WalkFrag = EntityManager.GetFragmentDataChecked<FGTPCrowdWalkerFragment>(Entity);
		FGTPCrowdLookFragment& Look = EntityManager.GetFragmentDataChecked<FGTPCrowdLookFragment>(Entity);
		if (PlaceAroundPlayer(WalkFrag.Walker, Rng))
		{
			GTPWalk::FPoint P, D;
			GTPWalk::Locate(Graph, WalkFrag.Walker, P, D);
			WalkFrag.Location = FVector(P.X, P.Y, 0.f);
			WalkFrag.GroundZ = GroundAt(WalkFrag.Location);
			WalkFrag.Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
			++Placed;
		}
		Look.Scale = Rng.Range(1.f - HeightSpread, 1.f + HeightSpread);
		Look.InstanceIndex = Instances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -100000.f), BaseScale * Look.Scale), false);
		if (FAgentRadiusFragment* Radius = EntityManager.GetFragmentDataPtr<FAgentRadiusFragment>(Entity))
		{
			Radius->Radius = GTPCrowdLocal::TrafficObstacleRadius;
		}
		Entities.Add(Entity);
	}
	SetupNearActors(InWorld);
	bRunning = true;
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %d people (%d placed) on %d walkway edges"), CrowdCount, Placed, static_cast<int32>(Graph.Edges.size()));
#if WITH_GTP_MASSTRAFFIC
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %s"), bTrafficObstacles
		? TEXT("traffic obstacles on crossings (MassTraffic cars brake for people crossing)")
		: TEXT("no traffic obstacles (no MassTraffic cars in this map)"));
#endif
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
	UpdateNear(DeltaTime);
	NextPanicSpread -= DeltaTime;
	if (NextPanicSpread <= 0.f)
	{
		NextPanicSpread = 0.2f;
		SpreadPanic();
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
#if WITH_GTP_MASSTRAFFIC
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	Query.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
#endif
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
#if WITH_GTP_MASSTRAFFIC
		// Epic's traffic obstacle: where the person is and how fast they go, for MassTraffic
		// (UMassTrafficFindObstaclesProcessor binds an obstacle to the car behind it on the nearby
		// lanes, UpdateDistanceToNearestObstacle brakes that car for it). The tag is worn on a
		// crossing only: on the sidewalk a car passing 2 m away would brake for nothing, and every
		// obstacle costs a lane search a frame. Adding or removing it moves the person to another
		// chunk at the end of the phase, once per crossing.
		const TArrayView<FTransformFragment> Transforms = Chunk.GetMutableFragmentView<FTransformFragment>();
		const TArrayView<FMassVelocityFragment> Velocities = Chunk.GetMutableFragmentView<FMassVelocityFragment>();
		const bool bObstacleChunk = Chunk.DoesArchetypeHaveTag<FMassTrafficObstacleTag>();
		auto Publish = [&Chunk, &Graph, &Transforms, &Velocities, bObstacleChunk](int32 Index, const FGTPCrowdWalkerFragment& W, const FVector& Velocity)
		{
			if (Transforms.IsEmpty() || Velocities.IsEmpty())
			{
				return;
			}
			const bool bOnCrossing = W.Walker.Edge >= 0 && W.Walker.Edge < static_cast<int>(Graph.Edges.size())
				&& Graph.Edges[W.Walker.Edge].Kind == GTPWalk::EKind::Crossing;
			Transforms[Index].GetMutableTransform() = FTransform(FRotator(0.f, W.Yaw, 0.f), FVector(W.Location.X, W.Location.Y, W.GroundZ));
			Velocities[Index].Value = Velocity;
			if (bOnCrossing && !bObstacleChunk)
			{
				Chunk.Defer().AddTag<FMassTrafficObstacleTag>(Chunk.GetEntity(Index));
			}
			else if (!bOnCrossing && bObstacleChunk)
			{
				Chunk.Defer().RemoveTag<FMassTrafficObstacleTag>(Chunk.GetEntity(Index));
			}
		};
#endif
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
#if WITH_GTP_MASSTRAFFIC
				Publish(Index, W, FVector::ZeroVector);
#endif
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
#if WITH_GTP_MASSTRAFFIC
			Publish(Index, W, FVector(D.X, D.Y, 0.f).GetSafeNormal2D() * W.Speed);
#endif
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
			const FTransform Transform = L.bNear
				? FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -100000.f), FVector(0.01f))
				: FTransform(FRotator(0.f, W.Yaw + YawOffset, 0.f), FVector(W.Location.X, W.Location.Y, W.GroundZ), BaseScale * L.Scale);
			Instances->UpdateInstanceTransform(L.InstanceIndex, Transform, true, false, true);
		}
	});
}

// ---------------------------------------------------------------- near people (full characters)

FGTPCrowdWalkerFragment* UGTPCrowdSubsystem::WalkerOf(int32 EntityIndex)
{
	UMassEntitySubsystem* Mass = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Mass || !Entities.IsValidIndex(EntityIndex))
	{
		return nullptr;
	}
	return Mass->GetMutableEntityManager().GetFragmentDataPtr<FGTPCrowdWalkerFragment>(Entities[EntityIndex]);
}

FGTPCrowdLookFragment* UGTPCrowdSubsystem::LookOf(int32 EntityIndex)
{
	UMassEntitySubsystem* Mass = GetWorld() ? GetWorld()->GetSubsystem<UMassEntitySubsystem>() : nullptr;
	if (!Mass || !Entities.IsValidIndex(EntityIndex))
	{
		return nullptr;
	}
	return Mass->GetMutableEntityManager().GetFragmentDataPtr<FGTPCrowdLookFragment>(Entities[EntityIndex]);
}

void UGTPCrowdSubsystem::SetupNearActors(UWorld& InWorld)
{
	// Assembled characters (City Sample Crowds) use the animations baked for the MetaHuman skeleton.
	{
		// Off by default until the City Sample crowd keeps the frame budget and cooks without material errors:
		// -CrowdCitySample=1 turns it on (the mannequins of before stay otherwise).
		USkeletalMesh* Probe = nullptr;
		int32 CitySampleOn = 0;
		if (!CrowdCharacterRoot.IsEmpty() && (FParse::Param(FCommandLine::Get(), TEXT("CrowdCitySample")) || (FParse::Value(FCommandLine::Get(), TEXT("CrowdCitySample="), CitySampleOn) && CitySampleOn != 0)))
		{
			Probe = LoadObject<USkeletalMesh>(nullptr, *(CrowdCharacterRoot + TEXT("/Male/NormalWeight/Meshes/m_tal_nrw_body.m_tal_nrw_body")));
		}
		const UAnimSequenceBase* BakedWalk = Probe ? Cast<UAnimSequenceBase>(CrowdWalkAnimation.TryLoad()) : nullptr;
		bAssembled = Probe && BakedWalk;
		if (bAssembled)
		{
			WalkAnimation = CrowdWalkAnimation;
			IdleAnimation = CrowdIdleAnimation;
			RunAnimation = CrowdRunAnimation;
		}
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: assembled City Sample characters %s (body %s, baked walk %s)"),
			bAssembled ? TEXT("on") : TEXT("off, mannequins"), Probe ? TEXT("found") : TEXT("missing"), BakedWalk ? TEXT("found") : TEXT("missing"));
	}
	Walk = Cast<UAnimSequenceBase>(WalkAnimation.TryLoad());
	Idle = Cast<UAnimSequenceBase>(IdleAnimation.TryLoad());
	Run = Cast<UAnimSequenceBase>(RunAnimation.TryLoad());
	HandsUp = Cast<UAnimSequenceBase>(HandsUpAnimation.TryLoad());
	// Fear animations baked from MoCap Online (tools/pedestrians/bake_fear_animations.py, prefix MF_), found by name.
	if (bAssembled)
	{
		FAssetRegistryModule& Reg = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> Anims;
		Reg.Get().GetAssetsByPath(FName(TEXT("/Game/MetaHumans/GTP/Anims")), Anims, false);
		auto Find = [&Anims](std::initializer_list<const TCHAR*> Words) -> UAnimSequenceBase*
		{
			for (const FAssetData& A : Anims)
			{
				const FString N = A.AssetName.ToString();
				bool bAll = N.StartsWith(TEXT("MF_"));
				for (const TCHAR* W : Words)
				{
					bAll = bAll && N.Contains(W);
				}
				if (bAll)
				{
					return Cast<UAnimSequenceBase>(A.GetAsset());
				}
			}
			return nullptr;
		};
		FearRun = Find({TEXT("Scared"), TEXT("Run")});
		if (!FearRun)
		{
			FearRun = Find({TEXT("Run")});
		}
		Cower = Find({TEXT("Cower")});
		if (!Cower)
		{
			Cower = Find({TEXT("Crouch")});
		}
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: fear animations: run %s, cower %s"), FearRun ? *FearRun->GetName() : TEXT("none"), Cower ? *Cower->GetName() : TEXT("none"));
	}
	USkeletalMesh* Mesh = nullptr;
	if (const ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(&InWorld, 0)))
	{
		Mesh = Player->GetMesh() ? Player->GetMesh()->GetSkeletalMeshAsset() : nullptr;
	}
	if (bAssembled && !Mesh)
	{
		Mesh = LoadObject<USkeletalMesh>(nullptr, *(CrowdCharacterRoot + TEXT("/Male/NormalWeight/Meshes/m_tal_nrw_body.m_tal_nrw_body")));
	}
	if (!Mesh || !Walk)
	{
		UE_LOG(LogGTPCrowd, Warning, TEXT("crowd: no near characters (mesh %s, walk %s)"), Mesh ? TEXT("ok") : TEXT("none"), Walk ? TEXT("ok") : TEXT("none"));
		return;
	}
	if (!Idle)
	{
		Idle = Walk;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 Slot = 0; Slot < NearCount; ++Slot)
	{
		AGTPPedestrian* Actor = InWorld.SpawnActor<AGTPPedestrian>(FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, Params);
		if (!Actor)
		{
			continue;
		}
		Actor->GetBody()->SetSkeletalMesh(Mesh);
		if (bAssembled)
		{
			DressFromCitySample(Actor);
		}
		// The mannequin faces +Y, the MetaHuman bodies of the pack face +X.
		Actor->GetBody()->SetRelativeRotation(FRotator(0.f, bAssembled ? 0.f : MeshYawOffset, 0.f));
		Actor->ApplyLook(ReactionRng.Range(1.f - HeightSpread, 1.f + HeightSpread), FLinearColor::White, {FName(TEXT("Tint"))}, ReactionRng.Range(0.92f, 1.08f));
		Actor->SetDetail(false, false, -1);
		if (UGTPThreatSubsystem::IsEnabled())
		{
			if (UGTPThreatResponseComponent* Fear = NewObject<UGTPThreatResponseComponent>(Actor, TEXT("Fear")))
			{
				const float Pick = ReactionRng.Next();
				Fear->SetTemperament(Pick < 0.45f ? EGTPTemperament::Fleeing : (Pick < 0.85f ? EGTPTemperament::Compliant : EGTPTemperament::Frozen));
				Fear->RegisterComponent();
				Fear->OnThreatState.AddWeakLambda(this, [this](AActor* Body, EGTPThreatState /*Old*/, EGTPThreatState New)
				{
					OnActorThreat(Body, static_cast<int32>(New));
				});
			}
		}
		NearActors.Add(Actor);
		NearEntity.Add(INDEX_NONE);
	}
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %d near characters, hands-up animation %s"), NearActors.Num(), HandsUp ? TEXT("found") : TEXT("missing"));
}

void UGTPCrowdSubsystem::OnLoud(const FVector& Where, float Radius, AActor* Cause)
{
	// Everybody within earshot runs from the noise, near or far.
	// Sirens and engines re-emit every frame: one pass per half second is plenty.
	const double Now = FPlatformTime::Seconds();
	// One reaction per source and half second at most, wherever the source moves to (crashes re-emit every frame in a pile-up).
	double& Last = LastLoudBySource.FindOrAdd(Cause);
	if (Now - Last < 0.5)
	{
		return;
	}
	Last = Now;
	const GTPWalk::FPoint Point{float(Where.X), float(Where.Y)};
	int32 Fled = 0;
	for (int32 Index = 0; Index < Entities.Num(); ++Index)
	{
		FGTPCrowdWalkerFragment* W = WalkerOf(Index);
		if (W && W->Walker.Edge >= 0 && FVector::Dist2D(W->Location, Where) <= Radius)
		{
			GTPWalk::Flee(Graph, W->Walker, Point, Settings);
			++Fled;
		}
	}
	if (Now - LastLoudLog > 2.0)
	{
		LastLoudLog = Now;
		UE_LOG(LogGTPCrowd, Log, TEXT("crowd: loud sound r=%.0f cm, %d of %d flee"), Radius, Fled, Entities.Num());
	}
}

void UGTPCrowdSubsystem::OnActorThreat(AActor* Body, int32 NewState)
{
	const UGTPThreatSubsystem* Threat = GetWorld() ? GetWorld()->GetSubsystem<UGTPThreatSubsystem>() : nullptr;
	const int32 Slot = NearActors.IndexOfByKey(Body);
	if (!Threat || Slot == INDEX_NONE || NearEntity[Slot] == INDEX_NONE)
	{
		return;
	}
	FGTPCrowdWalkerFragment* W = WalkerOf(NearEntity[Slot]);
	if (!W)
	{
		return;
	}
	UE_LOG(LogGTPCrowd, Log, TEXT("crowd: %s threat state %d"), *Body->GetName(), NewState);
	const FVector Origin = Threat->GetOrigin();
	const GTPWalk::FPoint From{float(Origin.X), float(Origin.Y)};
	switch (static_cast<EGTPThreatState>(NewState))
	{
	case EGTPThreatState::Alarmed:
		GTPWalk::Freeze(W->Walker, From, 3.f, GTPWalk::EPause::Gawk);
		break;
	case EGTPThreatState::HandsUp:
	case EGTPThreatState::Complying:
		GTPWalk::Freeze(W->Walker, From, 60.f, GTPWalk::EPause::HandsUp);
		break;
	case EGTPThreatState::Backing:
	case EGTPThreatState::Fleeing:
		GTPWalk::Flee(Graph, W->Walker, From, Settings);
		break;
	case EGTPThreatState::Calm:
		if (W->Walker.PauseKind == GTPWalk::EPause::HandsUp)
		{
			W->Walker.Pause = 0.f;
		}
		break;
	default:
		break;
	}
}

void UGTPCrowdSubsystem::HitByPlayerVehicle(int32 Slot)
{
	AGTPPedestrian* Actor = NearActors.IsValidIndex(Slot) ? NearActors[Slot].Get() : nullptr;
	if (!IsValid(Actor) || !NearEntity.IsValidIndex(Slot))
	{
		return;
	}
	FGTPCrowdWalkerFragment* W = WalkerOf(NearEntity[Slot]);
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Actor || !W || !Pawn)
	{
		return;
	}
	const FVector Velocity = Pawn->GetVelocity();
	if (Actor->StartRagdoll(Velocity * 0.4f + FVector(0.f, 0.f, 200.f)))
	{
		// The ones around stop and look.
		const GTPWalk::FPoint Point{float(W->Location.X), float(W->Location.Y)};
		for (int32 Index = 0; Index < Entities.Num(); ++Index)
		{
			FGTPCrowdWalkerFragment* Other = WalkerOf(Index);
			if (Other && Other != W && Other->Walker.Edge >= 0 && FVector::Dist2D(Other->Location, W->Location) < 2500.f)
			{
				GTPWalk::Gawk(Other->Walker, Point, ReactionRng, Settings);
			}
		}
		GTPWalk::Freeze(W->Walker, Point, 10.f, GTPWalk::EPause::None);
	}
}

void UGTPCrowdSubsystem::UpdateNear(float DeltaTime)
{
	if (NearActors.Num() == 0)
	{
		return;
	}
	// A near actor destroyed under us (level change, kill volume, recycling) leaves its slot: purge it, free its person.
	for (int32 Slot = NearActors.Num() - 1; Slot >= 0; --Slot)
	{
		if (!IsValid(NearActors[Slot]))
		{
			if (NearEntity.IsValidIndex(Slot) && NearEntity[Slot] != INDEX_NONE)
			{
				if (FGTPCrowdLookFragment* Look = LookOf(NearEntity[Slot]))
				{
					Look->bNear = false;
				}
			}
			UE_LOG(LogGTPCrowd, Warning, TEXT("crowd: near actor %d was destroyed, slot dropped"), Slot);
			NearActors.RemoveAt(Slot);
			if (NearEntity.IsValidIndex(Slot))
			{
				NearEntity.RemoveAt(Slot);
			}
		}
	}
	if (NearActors.Num() == 0 || NearEntity.Num() != NearActors.Num())
	{
		return;
	}
	const FVector2D Player2D(PlayerLocation.X, PlayerLocation.Y);
	// Candidates within the radius, the closest first, one per near actor.
	TArray<TPair<float, int32>> Candidates;
	for (int32 Index = 0; Index < Entities.Num(); ++Index)
	{
		const FGTPCrowdWalkerFragment* W = WalkerOf(Index);
		if (W && W->Walker.Edge >= 0)
		{
			const float D = FVector2D::Distance(FVector2D(W->Location.X, W->Location.Y), Player2D);
			if (D <= NearRadius)
			{
				Candidates.Emplace(D, Index);
			}
		}
	}
	Candidates.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	TSet<int32> Wanted;
	for (int32 I = 0; I < FMath::Min(Candidates.Num(), NearActors.Num()); ++I)
	{
		Wanted.Add(Candidates[I].Value);
	}
	// Release the actors whose person left the closest set, then give free actors to the new ones.
	for (int32 Slot = 0; Slot < NearActors.Num(); ++Slot)
	{
		if (NearEntity[Slot] != INDEX_NONE && !Wanted.Contains(NearEntity[Slot]))
		{
			if (FGTPCrowdLookFragment* Look = LookOf(NearEntity[Slot]))
			{
				Look->bNear = false;
			}
			NearActors[Slot]->ResetBody();
			NearActors[Slot]->SetDetail(false, false, -1);
			NearActors[Slot]->SetActorLocation(FVector(0.f, 0.f, -100000.f), false, nullptr, ETeleportType::TeleportPhysics);
			NearEntity[Slot] = INDEX_NONE;
		}
	}
	for (const int32 EntityIndex : Wanted)
	{
		if (NearEntity.Contains(EntityIndex))
		{
			continue;
		}
		const int32 Free = NearEntity.IndexOfByKey(INDEX_NONE);
		if (Free == INDEX_NONE)
		{
			break;
		}
		NearEntity[Free] = EntityIndex;
		if (FGTPCrowdLookFragment* Look = LookOf(EntityIndex))
		{
			Look->bNear = true;
		}
		NearActors[Free]->SetDetail(true, true, -1);
	}
	// Follow the entities: position, and the animation that matches what the person does.
	for (int32 Slot = 0; Slot < NearActors.Num(); ++Slot)
	{
		AGTPPedestrian* Actor = NearActors[Slot];
		const int32 EntityIndex = NearEntity[Slot];
		FGTPCrowdWalkerFragment* W = EntityIndex != INDEX_NONE ? WalkerOf(EntityIndex) : nullptr;
		if (!Actor || !W)
		{
			continue;
		}
		if (Actor->IsRagdoll())
		{
			continue;
		}
		if (FVector::Dist2D(W->Location, PlayerLocation) < 170.f && GetWorld())
		{
			const APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
			if (Pawn && Pawn->GetVelocity().Size2D() > 500.f)
			{
				HitByPlayerVehicle(Slot);
				continue;
			}
		}
		Actor->SetActorLocationAndRotation(FVector(W->Location.X, W->Location.Y, W->GroundZ), FRotator(0.f, W->Yaw, 0.f), false, nullptr, ETeleportType::None);
		const bool bMoving = W->Speed > 1.f;
		if (!bMoving)
		{
			const bool bHandsUp = W->Walker.PauseKind == GTPWalk::EPause::HandsUp && HandsUp;
			const bool bGawk = W->Walker.PauseKind == GTPWalk::EPause::Gawk && Cower;
			Actor->PlayLoop(bHandsUp ? HandsUp.Get() : (bGawk ? Cower.Get() : Idle.Get()), 1.f);
		}
		else if (W->Walker.FleeTimeLeft > 0.f && Run)
		{
			Actor->PlayLoop(FearRun ? FearRun.Get() : Run.Get(), W->Speed / RunAnimationSpeed);
		}
		else
		{
			Actor->PlayLoop(Walk, W->Speed / WalkAnimationSpeed);
		}
	}
}

void UGTPCrowdSubsystem::SpreadPanic()
{
	// Each person who starts running passes the panic on once to the quiet people around, a few steps at most
	// (same rule as the legacy pedestrians: GTPWalk::CatchPanic).
	PanicSpread.SetNumZeroed(Entities.Num());
	for (int32 Index = 0; Index < Entities.Num(); ++Index)
	{
		FGTPCrowdWalkerFragment* W = WalkerOf(Index);
		if (!W || W->Walker.Edge < 0)
		{
			continue;
		}
		if (W->Walker.FleeTimeLeft <= 0.f)
		{
			PanicSpread[Index] = 0;
			continue;
		}
		if (PanicSpread[Index])
		{
			continue;
		}
		PanicSpread[Index] = 1;
		const GTPWalk::FPoint Source{float(W->Location.X), float(W->Location.Y)};
		for (int32 Other = 0; Other < Entities.Num(); ++Other)
		{
			FGTPCrowdWalkerFragment* O = Other != Index ? WalkerOf(Other) : nullptr;
			if (O && O->Walker.Edge >= 0 && FVector::Dist2D(O->Location, W->Location) <= Settings.PanicRadius)
			{
				GTPWalk::CatchPanic(Graph, O->Walker, W->Walker.Generation, Source, Settings, ReactionRng);
			}
		}
	}
}

bool UGTPCrowdSubsystem::DressFromCitySample(AGTPPedestrian* Actor)
{
	// Path pattern of the pack: <Root>/<Female|Male>/<NormalWeight|OverWeight|UnderWeight>/Meshes/<f|m>_tal_<nrw|ovw|unw>_<part>.
	const bool bFemale = ReactionRng.Next() < 0.5f;
	// Only the normal-weight body is in the migrated subset (the clothes of the other weights do not fit it anyway).
	const TCHAR* WeightDir = TEXT("NormalWeight");
	const TCHAR* WeightTag = TEXT("nrw");
	const FString Sex = bFemale ? TEXT("Female") : TEXT("Male");
	const FString Prefix = bFemale ? TEXT("f") : TEXT("m");
	auto Pick = [this](const TArray<FString>& Options) -> FString
	{
		return Options[FMath::Min(Options.Num() - 1, static_cast<int32>(ReactionRng.Next() * Options.Num()))];
	};
	auto Load = [&](const FString& Part) -> USkeletalMesh*
	{
		const FString Name = FString::Printf(TEXT("%s_tal_%s_%s"), *Prefix, WeightTag, *Part);
		return LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/%s/%s/Meshes/%s.%s"), *CrowdCharacterRoot, *Sex, WeightDir, *Name, *Name));
	};
	USkeletalMesh* Body = Load(TEXT("body"));
	if (!Body)
	{
		return false;
	}
	// A garment that is not in the subset falls back to the next one of its list, so nobody walks out half dressed.
	auto LoadAny = [&](TArray<FString> Options) -> USkeletalMesh*
	{
		const int32 Start = FMath::Min(Options.Num() - 1, static_cast<int32>(ReactionRng.Next() * Options.Num()));
		for (int32 I = 0; I < Options.Num(); ++I)
		{
			if (USkeletalMesh* Found = Load(Options[(Start + I) % Options.Num()]))
			{
				return Found;
			}
		}
		return nullptr;
	};
	TArray<USkeletalMesh*> Clothes;
	Clothes.Add(LoadAny((bFemale
		? TArray<FString>{TEXT("buttonDown"), TEXT("buttonOpen"), TEXT("scoopneck"), TEXT("turtleneck"), TEXT("buttonDown_blazer"), TEXT("scoopneck_croppedJacket")}
		: TArray<FString>{TEXT("buttonDown"), TEXT("buttonOpen"), TEXT("crewneck"), TEXT("turtleneck"), TEXT("buttonDown_tie"), TEXT("crewneck_blazer")})));
	Clothes.Add(LoadAny((bFemale
		? TArray<FString>{TEXT("jeans"), TEXT("slacks"), TEXT("skirt"), TEXT("jeans_belt")}
		: TArray<FString>{TEXT("jeans"), TEXT("jeansLong"), TEXT("slacks"), TEXT("slacks_belt")})));
	Clothes.Add(LoadAny((bFemale
		? TArray<FString>{TEXT("loafers"), TEXT("oxfords"), TEXT("dressFlats")}
		: TArray<FString>{TEXT("loafers"), TEXT("oxfords")})));
	// Heads 001 to 003 of each sex are in the migrated subset.
	const int32 FaceIndex = 1 + FMath::Min(2, static_cast<int32>(ReactionRng.Next() * 3));
	const FString FaceName = FString::Printf(TEXT("%s_%03d_nrw_FaceMesh"), *Prefix, FaceIndex);
	USkeletalMesh* Face = LoadObject<USkeletalMesh>(nullptr, *FString::Printf(TEXT("%s/%s/%s_%03d/Face/%s.%s"),
		*CrowdCharacterRoot, *Sex, *Prefix, FaceIndex, *FaceName, *FaceName));
	if (FParse::Param(FCommandLine::Get(), TEXT("CrowdNoFace")))
	{
		Face = nullptr; // diagnostic: is the uncooked material in the face?
	}
	TSubclassOf<UAnimInstance> FaceAnim = FaceAnimClass.TryLoadClass<UAnimInstance>();
	Actor->Dress(Body, Clothes, Face, FaceAnim);
	// Once: which materials the assembled character really uses (a material missing from the cook shows as an
	// uncooked shader map in the packaged build, and the log says no name).
	static bool bMaterialsListed = false;
	if (!bMaterialsListed)
	{
		bMaterialsListed = true;
		TArray<USkeletalMeshComponent*> Comps;
		Actor->GetComponents<USkeletalMeshComponent>(Comps);
		for (const USkeletalMeshComponent* Comp : Comps)
		{
			for (int32 Slot = 0; Slot < Comp->GetNumMaterials(); ++Slot)
			{
				const UMaterialInterface* Material = Comp->GetMaterial(Slot);
				const UMaterialInstance* Instance = Cast<UMaterialInstance>(Material);
				UE_LOG(LogGTPCrowd, Log, TEXT("crowd material: %s slot %d = %s (parent %s)"), *Comp->GetName(), Slot,
					Material ? *Material->GetPathName() : TEXT("none"),
					Instance && Instance->Parent ? *Instance->Parent->GetPathName() : TEXT("-"));
			}
		}
	}
	return true;
}
