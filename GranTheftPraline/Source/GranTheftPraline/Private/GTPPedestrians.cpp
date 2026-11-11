#include "GTPPedestrians.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GTPGraphicsProfile.h"
#include "GTPAudioWorld.h"
#include "GTPTraffic.h"
#include "GTPVehicle.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Engine/GameInstance.h"

#if __has_include("GTPLog.h")
#include "GTPLog.h"
#else
#include <cstdio>
// Same idea as GTPLog.h: a line that survives Shipping (UE_LOG is compiled out there).
#if NO_LOGGING
#define GTP_TRACE(CategoryName, Format, ...) \
	do \
	{ \
		const FString GTPTraceLine = FString::Printf(TEXT("%s: %s"), TEXT(#CategoryName), *FString::Printf(Format, ##__VA_ARGS__)); \
		std::fputs(TCHAR_TO_UTF8(*GTPTraceLine), stdout); \
		std::fputc('\n', stdout); \
		std::fflush(stdout); \
	} while (false)
#else
#define GTP_TRACE(CategoryName, Format, ...) UE_LOG(CategoryName, Log, Format, ##__VA_ARGS__)
#endif
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGTPPedestrians, Log, All);

// ---------------------------------------------------------------- AGTPPedestrian

AGTPPedestrian::AGTPPedestrian()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Root);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// Nothing to pose while nobody sees the body.
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
}

void AGTPPedestrian::ApplyLook(float HeightScale, const FLinearColor& Clothes, const TArray<FName>& TintParameters)
{
	SetActorScale3D(FVector(HeightScale));
	for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* Material = Body->CreateAndSetMaterialInstanceDynamic(Slot))
		{
			// A parameter the material does not have is ignored.
			for (const FName& Parameter : TintParameters)
			{
				Material->SetVectorParameterValue(Parameter, Clothes);
			}
		}
	}
}

void AGTPPedestrian::MakeCheap()
{
	Body->bCastContactShadow = false;
	Body->bReceivesDecals = false;
	Body->bUseAsOccluder = false;
	Body->bAffectDynamicIndirectLighting = false;
	Body->bAffectDistanceFieldLighting = false;
	Body->bPerBoneMotionBlur = false;
	Body->bComponentUseFixedSkelBounds = true;
}

void AGTPPedestrian::SetAnimInterval(float Seconds)
{
	if (!FMath::IsNearlyEqual(Seconds, AnimInterval))
	{
		AnimInterval = Seconds;
		Body->SetComponentTickInterval(Seconds);
	}
}

void AGTPPedestrian::PlayLoop(UAnimSequenceBase* Animation, float PlayRate)
{
	if (!Animation || bRagdoll)
	{
		return;
	}
	if (CurrentAnimation != Animation)
	{
		CurrentAnimation = Animation;
		Body->SetAnimation(Animation);
		Body->SetPlayRate(PlayRate);
		Body->Play(true);
		// Not everybody in step.
		Body->SetPosition(FMath::FRand() * Animation->GetPlayLength(), false);
	}
	else
	{
		Body->SetPlayRate(PlayRate);
	}
}

void AGTPPedestrian::SetDetail(bool bVisible, bool bCastShadow, int32 ForcedLOD)
{
	if (bVisible != bShown)
	{
		bShown = bVisible;
		// The actor flag too, so other systems (footsteps) can tell by IsHidden().
		SetActorHiddenInGame(!bVisible);
		Body->SetVisibility(bVisible, false);
	}
	if (bCastShadow != bShadow)
	{
		bShadow = bCastShadow;
		Body->SetCastShadow(bCastShadow);
	}
	if (ForcedLOD != LOD)
	{
		LOD = ForcedLOD;
		// SetForcedLOD takes 1-based levels, 0 = automatic.
		Body->SetForcedLOD(ForcedLOD < 0 ? 0 : ForcedLOD + 1);
	}
}

bool AGTPPedestrian::StartRagdoll(const FVector& Impulse)
{
	if (bRagdoll || !Body->GetPhysicsAsset())
	{
		return false;
	}
	bRagdoll = true;
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetSimulatePhysics(true);
	Body->AddImpulse(Impulse, NAME_None, true);
	return true;
}

void AGTPPedestrian::ResetBody()
{
	if (bRagdoll)
	{
		Body->SetSimulatePhysics(false);
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Body->AttachToComponent(Root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Body->SetRelativeLocation(FVector::ZeroVector);
		bRagdoll = false;
		CurrentAnimation = nullptr;
		Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
}

// ---------------------------------------------------------------- subsystem

FString UGTPPedestrianSubsystem::WalkwaysFileFor(const FString& MapName)
{
	return FPaths::ProjectContentDir() / TEXT("City/Pedestrians") / (MapName + TEXT(".walkways.csv"));
}

int32 UGTPPedestrianSubsystem::LoadWalkwaysFromCsv(const FString& Path, GTPWalk::FGraph& OutGraph)
{
	TArray<FString> Rows;
	if (!FFileHelper::LoadFileToStringArray(Rows, *Path) || Rows.Num() < 2)
	{
		return 0;
	}
	OutGraph = GTPWalk::FGraph();
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		// edge_id,road_id,kind,node_a,node_b,width_m,points_ue_cm (road_id is empty on links)
		TArray<FString> Fields;
		Rows[RowIndex].ParseIntoArray(Fields, TEXT(","), false);
		if (Fields.Num() < 7)
		{
			continue;
		}
		GTPWalk::FEdge Edge;
		Edge.Kind = static_cast<GTPWalk::EKind>(FCString::Atoi(*Fields[2]));
		Edge.NodeA = FCString::Atoi(*Fields[3]);
		Edge.NodeB = FCString::Atoi(*Fields[4]);
		Edge.Width = FCString::Atof(*Fields[5]) * 100.f;
		Edge.FacadeSide = Fields.Num() > 7 ? FMath::Clamp(FCString::Atoi(*Fields[7]), -1, 1) : 0;
		TArray<FString> Points;
		Fields[6].TrimEnd().ParseIntoArray(Points, TEXT(";"));
		for (const FString& Point : Points)
		{
			FString X, Y;
			if (Point.Split(TEXT(" "), &X, &Y))
			{
				Edge.Points.push_back({FCString::Atof(*X), FCString::Atof(*Y)});
			}
		}
		if (Edge.Points.size() >= 2 && Edge.NodeA >= 0 && Edge.NodeB >= 0)
		{
			OutGraph.Edges.push_back(MoveTemp(Edge));
		}
	}
	OutGraph.Finalize();
	return OutGraph.Edges.size();
}

bool UGTPPedestrianSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGTPPedestrianSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPPedestrianSubsystem, STATGROUP_Tickables);
}

int32 UGTPPedestrianSubsystem::GetActiveCount() const
{
	int32 Count = 0;
	for (const FPerson& P : People)
	{
		Count += P.bActive ? 1 : 0;
	}
	return Count;
}

int32 UGTPPedestrianSubsystem::ChooseDensity() const
{
	int32 FromCommandLine = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("Pedestrians="), FromCommandLine))
	{
		return FMath::Max(0, FromCommandLine);
	}
	FString Profile = TEXT("m1");
	if (const UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (const UGTPGraphicsProfile* Gfx = GI->GetSubsystem<UGTPGraphicsProfile>())
		{
			Profile = Gfx->GetActiveProfile();
		}
	}
	if (Profile == TEXT("ultra"))
	{
		return DensityUltra;
	}
	return Profile == TEXT("fidelity") ? DensityFidelity : DensityM1;
}

FLinearColor UGTPPedestrianSubsystem::Palette(float Pick)
{
	// Everyday clothes of a French city street: dark, navy, grey, beige, olive, brick, denim, white.
	static const FLinearColor Colours[] = {
		FLinearColor(0.02f, 0.02f, 0.025f), FLinearColor(0.02f, 0.035f, 0.10f), FLinearColor(0.18f, 0.18f, 0.19f),
		FLinearColor(0.42f, 0.34f, 0.24f), FLinearColor(0.12f, 0.14f, 0.06f), FLinearColor(0.30f, 0.07f, 0.04f),
		FLinearColor(0.07f, 0.12f, 0.25f), FLinearColor(0.70f, 0.70f, 0.68f)};
	constexpr int32 Count = UE_ARRAY_COUNT(Colours);
	return Colours[FMath::Min(Count - 1, static_cast<int32>(Pick * Count))];
}

USkeletalMesh* UGTPPedestrianSubsystem::PickMesh()
{
	if (LoadedMeshes.Num() == 0)
	{
		return nullptr;
	}
	return LoadedMeshes[FMath::Min(LoadedMeshes.Num() - 1, static_cast<int32>(Rng.Next() * LoadedMeshes.Num()))];
}

UAnimSequenceBase* UGTPPedestrianSubsystem::FindAnimation(const USkeletalMesh* Mesh, const TArray<FString>& Words)
{
	if (!Mesh || !Mesh->GetSkeleton())
	{
		return nullptr;
	}
	TArray<FAssetData> Assets;
	FAssetRegistryModule::GetRegistry().GetAssetsByPath(FName(TEXT("/Game")), Assets, true);
	const FTopLevelAssetPath SequenceClass = UAnimSequence::StaticClass()->GetClassPathName();
	FAssetData Best;
	int32 BestScore = 0;
	int32 Loads = 0;
	GTP_TRACE(LogGTPPedestrians, TEXT("find animation: scanning %d assets"), Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		if (Loads >= 4)
		{
			break; // never load more than a handful synchronously
		}
		if (Asset.AssetClassPath != SequenceClass)
		{
			continue;
		}
		const FString Name = Asset.AssetName.ToString().ToLower();
		bool bWord = false;
		for (const FString& Word : Words)
		{
			bWord |= Name.Contains(Word);
		}
		if (!bWord)
		{
			continue;
		}
		// Prefer a plain forward loop: "fwd" or a leading "f", no turn, start, stop or in-place variants.
		int32 Score = 10;
		Score += (Name.Contains(TEXT("fwd")) || Name.Contains(TEXT("_f_")) || Name.Contains(TEXT("forward"))) ? 5 : 0;
		Score += Name.Contains(TEXT("loop")) ? 3 : 0;
		Score -= (Name.Contains(TEXT("turn")) || Name.Contains(TEXT("start")) || Name.Contains(TEXT("stop")) || Name.Contains(TEXT("pivot"))
			|| Name.Contains(TEXT("back")) || Name.Contains(TEXT("left")) || Name.Contains(TEXT("right")) || Name.Contains(TEXT("strafe"))) ? 8 : 0;
		if (Score > BestScore)
		{
			++Loads;
			UAnimSequenceBase* Candidate = Cast<UAnimSequenceBase>(Asset.GetAsset());
			if (Candidate && Candidate->GetSkeleton() == Mesh->GetSkeleton())
			{
				BestScore = Score;
				Best = Asset;
			}
		}
	}
	return BestScore > 0 ? Cast<UAnimSequenceBase>(Best.GetAsset()) : nullptr;
}

void UGTPPedestrianSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Traffic lights (GTPTrafficLights.h): wait for the red of the roads being crossed.
	TSharedRef<TWeakObjectPtr<AGTPTrafficNetwork>> Cached = MakeShared<TWeakObjectPtr<AGTPTrafficNetwork>>();
	CrossingBlockedQuery = [this, Cached](const FVector& Where, const FVector& Direction)
	{
		TWeakObjectPtr<AGTPTrafficNetwork>& Network = *Cached;
		if (!Network.IsValid())
		{
			// Found once (the game mode spawns it at start), again only if it goes away.
			TActorIterator<AGTPTrafficNetwork> It(GetWorld());
			if (!It)
			{
				return false;
			}
			Network = *It;
		}
		return !Network->IsWalkAllowed(Where, Direction);
	};
	// A loud crash or a shot (GTPAudioWorld.h) scares the passers-by around it.
	if (UGTPAudioWorld* AudioWorld = InWorld.GetSubsystem<UGTPAudioWorld>())
	{
		AudioWorld->OnLoudSound.AddWeakLambda(this, [this](const FVector& Location, float Radius, AActor* Cause)
		{
			ScareAround(Location, Radius, Cause);
		});
	}
	Wanted = ChooseDensity();
	int32 Override = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("PedMaxVisible="), Override))
	{
		MaxVisible = Override;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PedCheap="), Override))
	{
		bCheapRendering = Override != 0;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PedAnimRate="), Override))
	{
		bReduceAnimRate = Override != 0;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("PedSpread="), Override))
	{
		bSpreadFarUpdates = Override != 0;
	}
	if (Wanted <= 0)
	{
		GTP_TRACE(LogGTPPedestrians, TEXT("pedestrians: off"));
		return;
	}
	GTP_TRACE(LogGTPPedestrians, TEXT("init: begin"));
	const FString File = WalkwaysFileFor(UGameplayStatics::GetCurrentLevelName(&InWorld, true));
	const int32 Edges = LoadWalkwaysFromCsv(File, Graph);
	if (Edges == 0)
	{
		GTP_TRACE(LogGTPPedestrians, TEXT("no pedestrians: %s not found"), *File);
		Wanted = 0;
		return;
	}

	GTP_TRACE(LogGTPPedestrians, TEXT("init: walkways %d edges, loading meshes"), Edges);
	for (const FSoftObjectPath& Path : Meshes)
	{
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Path.TryLoad()))
		{
			LoadedMeshes.Add(Mesh);
		}
	}
	if (LoadedMeshes.Num() == 0)
	{
		// The player's own character: same skeleton as the animations of the Game Animation Sample.
		if (const ACharacter* Player = Cast<ACharacter>(UGameplayStatics::GetPlayerPawn(&InWorld, 0)))
		{
			if (Player->GetMesh() && Player->GetMesh()->GetSkeletalMeshAsset())
			{
				LoadedMeshes.Add(Player->GetMesh()->GetSkeletalMeshAsset());
			}
		}
	}
	// -PedWalk= / -PedIdle= / -PedRun= (asset paths) override the config; anything still missing
	// is found among the animations of the mesh's skeleton by name (walk, idle, run or jog).
	auto Resolve = [&](const FSoftObjectPath& Configured, const TCHAR* Param, const TArray<FString>& Words) -> UAnimSequenceBase*
	{
		FString Override;
		if (FParse::Value(FCommandLine::Get(), Param, Override) && !Override.IsEmpty())
		{
			return Cast<UAnimSequenceBase>(FSoftObjectPath(Override).TryLoad());
		}
		if (Configured.IsValid())
		{
			return Cast<UAnimSequenceBase>(Configured.TryLoad());
		}
		return FindAnimation(LoadedMeshes[0], Words);
	};
	GTP_TRACE(LogGTPPedestrians, TEXT("init: meshes %d, resolving animations"), LoadedMeshes.Num());
	Walk = Resolve(WalkAnimation, TEXT("PedWalk="), {TEXT("walk")});
	Idle = Resolve(IdleAnimation, TEXT("PedIdle="), {TEXT("idle")});
	Run = Resolve(RunAnimation, TEXT("PedRun="), {TEXT("run"), TEXT("jog")});
	// Optional: no default search, a phone loop is only played if one is named.
	Phone = Cast<UAnimSequenceBase>(PhoneAnimation.TryLoad());
	GTP_TRACE(LogGTPPedestrians, TEXT("animations: walk=%s idle=%s run=%s"),
		Walk ? *Walk->GetPathName() : TEXT("none"), Idle ? *Idle->GetPathName() : TEXT("none"), Run ? *Run->GetPathName() : TEXT("none"));
	if (LoadedMeshes.Num() == 0 || !Walk)
	{
		GTP_TRACE(LogGTPPedestrians, TEXT("no pedestrians: mesh (%d) or walk animation (%s) missing, see tools/pedestrians/find_walk_assets.py"),
			LoadedMeshes.Num(), *WalkAnimation.ToString());
		Wanted = 0;
		return;
	}
	if (!Idle)
	{
		Idle = Walk;
	}
	if (!Run)
	{
		Run = Walk;
	}

	GTP_TRACE(LogGTPPedestrians, TEXT("init: spawning %d"), Wanted);
	People.SetNum(Wanted);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (FPerson& P : People)
	{
		AGTPPedestrian* Actor = InWorld.SpawnActor<AGTPPedestrian>(FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, Params);
		if (!Actor)
		{
			continue;
		}
		Actor->GetBody()->SetSkeletalMesh(PickMesh());
		Actor->ApplyLook(Rng.Range(1.f - HeightSpread, 1.f + HeightSpread), Palette(Rng.Next()), TintParameters);
		Actor->GetBody()->SetRelativeRotation(FRotator(0.f, MeshYawOffset, 0.f));
		Actor->SetDetail(false, false, -1);
		if (bCheapRendering)
		{
			Actor->MakeCheap();
		}
		P.Actor = Actor;
	}
	GTP_TRACE(LogGTPPedestrians, TEXT("pedestrians: on (%d), %d walkway edges, %d visible at most"), Wanted, Edges, MaxVisible);
	// The far crowd loads its mesh a few seconds into play, never while the map is loading.
	bFarPending = true;
}

void UGTPPedestrianSubsystem::InitFarCrowd(UWorld& InWorld)
{
	int32 Count = FarCrowdCount;
	FParse::Value(FCommandLine::Get(), TEXT("PedFar="), Count);
	if (Count <= 0)
	{
		// Off: load nothing at all.
		GTP_TRACE(LogGTPPedestrians, TEXT("far crowd: off"));
		return;
	}
	FString MeshOverride;
	if (FParse::Value(FCommandLine::Get(), TEXT("PedFarMesh="), MeshOverride) && !MeshOverride.IsEmpty())
	{
		FarCrowdMesh = FSoftObjectPath(MeshOverride);
	}
	int32 ForceSilhouette = 0;
	FParse::Value(FCommandLine::Get(), TEXT("PedFarSilhouette="), ForceSilhouette);
	UStaticMesh* Mesh = ForceSilhouette != 0 ? nullptr : Cast<UStaticMesh>(FarCrowdMesh.TryLoad());
	if (Count > 0 && !Mesh && (bFarSilhouetteFallback || ForceSilhouette != 0))
	{
		// No imported person: a 45 cm wide, 175 cm tall column. The engine's cylinder is 100 cm
		// each way and centred, so lift it by half its scaled height.
		Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		FarScale = FVector(0.45f, 0.45f, 1.75f);
		FarZOffset = 87.5f;
		GTP_TRACE(LogGTPPedestrians, TEXT("far crowd: silhouette columns (no far mesh)"));
	}
	if (Count <= 0 || !Mesh)
	{
		GTP_TRACE(LogGTPPedestrians, TEXT("far crowd: off (count %d, mesh %s)"), Count, Mesh ? TEXT("set") : TEXT("missing"));
		return;
	}
	AActor* Holder = InWorld.SpawnActor<AActor>();
	if (!Holder)
	{
		return;
	}
	FarInstances = NewObject<UInstancedStaticMeshComponent>(Holder, TEXT("FarCrowd"));
	FarInstances->SetStaticMesh(Mesh);
	FarInstances->SetMobility(EComponentMobility::Movable);
	FarInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FarInstances->SetCastShadow(false);
	FarInstances->bAffectDynamicIndirectLighting = false;
	FarInstances->bAffectDistanceFieldLighting = false;
	FarInstances->bReceivesDecals = false;
	// Two floats per instance for a vertex animation material: walk cycle phase and speed scale
	// (the material reads Time itself, so no per-frame cost here). Plain static meshes ignore them.
	FarInstances->NumCustomDataFloats = 2;
	FarInstances->SetCullDistances(0, static_cast<int32>(FarMaxDistance));
	Holder->SetRootComponent(FarInstances);
	FarInstances->RegisterComponent();
	FarAgents.SetNum(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		// All instances exist from the start, parked far below the map until their agent is placed.
		const int32 Instance = FarInstances->AddInstance(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -100000.f), FVector(0.001f)), true);
		FarInstances->SetCustomDataValue(Instance, 0, Rng.Next(), false);
		FarInstances->SetCustomDataValue(Instance, 1, Rng.Range(0.9f, 1.1f), false);
	}
	GTP_TRACE(LogGTPPedestrians, TEXT("far crowd: on (%d)"), Count);
}

bool UGTPPedestrianSubsystem::PlaceFarAgent(int32 Index)
{
	FFarAgent& Agent = FarAgents[Index];
	for (int32 Try = 0; Try < 20; ++Try)
	{
		GTPWalk::FWalker W;
		if (!GTPWalk::Place(Graph, W, Rng, Settings))
		{
			return false;
		}
		GTPWalk::FPoint P, D;
		GTPWalk::Locate(Graph, W, P, D);
		const float Distance = FVector::Dist2D(FVector(P.X, P.Y, 0.f), PlayerLocation);
		if (Distance >= FarMinDistance && Distance <= FarMaxDistance && !IsSeen(FVector(P.X, P.Y, PlayerLocation.Z)))
		{
			Agent.Walker = W;
			Agent.bActive = true;
			return true;
		}
	}
	return false;
}

bool UGTPPedestrianSubsystem::PromoteToPool(const GTPWalk::FWalker& Walker, const FVector& Location)
{
	for (FPerson& P : People)
	{
		if (!P.bActive && P.Actor.IsValid())
		{
			P.Walker = Walker;
			P.Leader = -1;
			GTPWalk::FPoint Pt, Dir;
			GTPWalk::Locate(Graph, P.Walker, Pt, Dir);
			Activate(P, Pt, Dir);
			return true;
		}
	}
	return false;
}

void UGTPPedestrianSubsystem::TickFarCrowd(float DeltaTime)
{
	if (!FarInstances || FarAgents.Num() == 0)
	{
		return;
	}
	// Layer 3: simulated 5 times a second, no actor. Updates are spread, never all in one frame.
	FarAccumulated += DeltaTime;
	if (FarAccumulated < 0.2f)
	{
		return;
	}
	const float Elapsed = FarAccumulated;
	FarAccumulated = 0.f;
	for (int32 i = 0; i < FarAgents.Num(); ++i)
	{
		FFarAgent& Agent = FarAgents[i];
		if (!Agent.bActive && !PlaceFarAgent(i))
		{
			continue;
		}
		GTPWalk::Step(Graph, Agent.Walker, Elapsed, Rng, Settings, false);
		GTPWalk::FPoint P, D;
		GTPWalk::Locate(Graph, Agent.Walker, P, D);
		const FVector Location(P.X, P.Y, PlayerLocation.Z);
		const float Distance = FVector::Dist2D(Location, PlayerLocation);
		if (Distance < PromoteDistance && PromoteToPool(Agent.Walker, Location))
		{
			Agent.bActive = false;
			Agent.bShown = false;
			FarInstances->UpdateInstanceTransform(i, FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -100000.f), FVector(0.001f)), true, false, true);
			continue;
		}
		if (Distance > FarMaxDistance * 1.1f && !IsSeen(Location))
		{
			Agent.bActive = false; // placed again closer, out of sight, next time
			continue;
		}
		const FRotator Facing(0.f, FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) + MeshYawOffset, 0.f);
		FarInstances->UpdateInstanceTransform(i, FTransform(Facing, Location + FVector(0.f, 0.f, FarZOffset), FarScale), true, false, true);
		Agent.bShown = true;
	}
	FarInstances->MarkRenderStateDirty();
}

void UGTPPedestrianSubsystem::Deinitialize()
{
	for (FPerson& P : People)
	{
		if (AGTPPedestrian* A = P.Actor.Get())
		{
			A->Destroy();
		}
	}
	People.Reset();
	Super::Deinitialize();
}

void UGTPPedestrianSubsystem::RefreshViewer()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	bHasViewer = PC && PC->PlayerCameraManager;
	if (!bHasViewer)
	{
		return;
	}
	ViewerLocation = PC->PlayerCameraManager->GetCameraLocation();
	ViewerForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const APawn* Pawn = PC->GetPawn();
	PlayerLocation = Pawn ? Pawn->GetActorLocation() : ViewerLocation;
}

bool UGTPPedestrianSubsystem::IsSeen(const FVector& Location) const
{
	// Cheap: the view cone only (no trace). A person behind a wall in the cone waits for the next try.
	const FVector To = (Location - ViewerLocation).GetSafeNormal();
	return FVector::DotProduct(To, ViewerForward) > FMath::Cos(FMath::DegreesToRadians(65.f));
}

void UGTPPedestrianSubsystem::RefreshCars()
{
	Cars.Reset();
	for (TActorIterator<AGTPVehicle> It(GetWorld()); It; ++It)
	{
		Cars.Add({It->GetActorLocation(), It->GetVelocity()});
	}
}

bool UGTPPedestrianSubsystem::CarNear(const FVector& Where, float Radius, float MinSpeed, FVector* OutCarLocation, FVector* OutCarVelocity) const
{
	for (const FCar& Car : Cars)
	{
		if (FVector::DistSquaredXY(Car.Location, Where) < Radius * Radius && Car.Velocity.SizeSquared2D() > MinSpeed * MinSpeed)
		{
			if (OutCarLocation)
			{
				*OutCarLocation = Car.Location;
			}
			if (OutCarVelocity)
			{
				*OutCarVelocity = Car.Velocity;
			}
			return true;
		}
	}
	return false;
}

void UGTPPedestrianSubsystem::MoveGroundTo(FPerson& Person, bool bSnap)
{
	// The walkways are flat lines: find the ground under the feet (kerbs, ramps, the bridge).
	FHitResult Hit;
	const FVector Top(Person.Location.X, Person.Location.Y, FMath::Max(PlayerLocation.Z, Person.GroundZ) + 400.f);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GTPPedestrianGround), false);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Top, Top - FVector(0.f, 0.f, 1600.f), ECC_Visibility, Query))
	{
		Person.GroundZ = bSnap ? Hit.ImpactPoint.Z : FMath::FInterpTo(Person.GroundZ, Hit.ImpactPoint.Z, 0.2f, 8.f);
	}
	else if (bSnap)
	{
		Person.GroundZ = PlayerLocation.Z;
	}
}

bool UGTPPedestrianSubsystem::TrySpawn(FPerson& Person)
{
	AGTPPedestrian* Actor = Person.Actor.Get();
	if (!Actor)
	{
		return false;
	}
	for (int32 Try = 0; Try < 30; ++Try)
	{
		GTPWalk::FWalker W;
		if (!GTPWalk::Place(Graph, W, Rng, Settings))
		{
			return false;
		}
		GTPWalk::FPoint P, D;
		GTPWalk::Locate(Graph, W, P, D);
		const FVector Location(P.X, P.Y, PlayerLocation.Z);
		const float Distance = FVector::Dist2D(Location, PlayerLocation);
		if (Distance < MinSpawnDistance || Distance > MaxSpawnDistance)
		{
			continue;
		}
		// Out of sight, unless so far that it is a speck in the street's end.
		if (IsSeen(Location) && Distance < DrawDistance)
		{
			continue;
		}
		if (CarNear(Location, 400.f, 0.f))
		{
			continue;
		}
		Person.Walker = W;
		Person.Leader = -1;
		Activate(Person, P, D);
		// Sometimes two people walk together: the partner takes a free place beside this one.
		if (Rng.Next() < Settings.GroupChance)
		{
			const int32 LeaderIndex = static_cast<int32>(&Person - People.GetData());
			for (FPerson& Other : People)
			{
				if (!Other.bActive && Other.Actor.IsValid() && &Other != &Person)
				{
					Other.Walker = GTPWalk::FWalker();
					GTPWalk::Follow(Graph, Other.Walker, Person.Walker, Settings);
					GTPWalk::FPoint OP, OD;
					GTPWalk::Locate(Graph, Other.Walker, OP, OD);
					Other.Leader = LeaderIndex;
					Activate(Other, OP, OD);
					++SpawnedTotal;
					break;
				}
			}
		}
		++SpawnedTotal;
		return true;
	}
	return false;
}

void UGTPPedestrianSubsystem::Activate(FPerson& Person, const GTPWalk::FPoint& P, const GTPWalk::FPoint& D)
{
	AGTPPedestrian* Actor = Person.Actor.Get();
	Person.Location = FVector(P.X, P.Y, PlayerLocation.Z);
	Person.GroundZ = PlayerLocation.Z;
	MoveGroundTo(Person, true);
	Person.Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	Person.Pos = P;
	Person.Dir = D;
	Person.bActive = true;
	Person.bRunning = false;
	Person.bIdle = false;
	Person.bDrawn = false;
	Person.RagdollLeft = 0.f;
	Actor->ResetBody();
	Actor->SetActorLocationAndRotation(FVector(Person.Location.X, Person.Location.Y, Person.GroundZ), FRotator(0.f, Person.Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
}

void UGTPPedestrianSubsystem::SeparatePeople()
{
	// Walkers closer than the personal distance step sideways round each other (pairs excepted).
	for (int32 i = 0; i < People.Num(); ++i)
	{
		FPerson& A = People[i];
		if (!A.bActive || A.Leader >= 0 || (A.Actor.IsValid() && A.Actor->IsRagdoll()) || A.Walker.FleeTimeLeft > 0.f)
		{
			continue;
		}
		for (int32 j = 0; j < People.Num(); ++j)
		{
			const FPerson& B = People[j];
			if (i != j && B.bActive && B.Leader != i && A.Leader != j)
			{
				GTPWalk::Separate(Graph, A.Walker, A.Pos, A.Dir, B.Pos, Settings);
			}
		}
	}
}

void UGTPPedestrianSubsystem::Retire(FPerson& Person)
{
	Person.bActive = false;
	const int32 Index = static_cast<int32>(&Person - People.GetData());
	for (FPerson& Other : People)
	{
		Other.Leader = Other.Leader == Index ? -1 : Other.Leader;
	}
	Person.Leader = -1;
	if (AGTPPedestrian* A = Person.Actor.Get())
	{
		A->ResetBody();
		A->SetDetail(false, false, -1);
		A->SetActorLocation(FVector(0.f, 0.f, -100000.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UGTPPedestrianSubsystem::Update(FPerson& Person, float DeltaTime)
{
	AGTPPedestrian* Actor = Person.Actor.Get();
	if (!Actor)
	{
		Person.bActive = false;
		return;
	}
	if (Actor->IsRagdoll())
	{
		Person.RagdollLeft -= DeltaTime;
		return;
	}

	// Cars: wait at crossings, run from a fast one, get knocked down by one.
	bool bCrossingUnsafe = false;
	const GTPWalk::FEdge& Edge = Graph.Edges[Person.Walker.Edge];
	if (Edge.Kind == GTPWalk::EKind::Crossing && Person.Walker.S < 30.f)
	{
		const FVector Mid(Edge.Points[Edge.Points.size() / 2].X, Edge.Points[Edge.Points.size() / 2].Y, 0.f);
		const GTPWalk::FPoint& From = Person.Walker.bForward ? Edge.Points.front() : Edge.Points.back();
		const GTPWalk::FPoint& To = Person.Walker.bForward ? Edge.Points.back() : Edge.Points.front();
		const FVector CrossingDirection = FVector(To.X - From.X, To.Y - From.Y, 0.f).GetSafeNormal();
		bCrossingUnsafe = CarNear(Mid, CrossingLookRadius, 150.f)
			|| (CrossingBlockedQuery && CrossingBlockedQuery(Mid, CrossingDirection));
	}
	FVector CarLocation, CarVelocity;
	if (Cars.Num() > 0 && CarNear(Person.Location, FleeRadius, FleeCarSpeed, &CarLocation, &CarVelocity))
	{
		const FVector ToPerson = (Person.Location - CarLocation).GetSafeNormal2D();
		const float Distance = FVector::Dist2D(Person.Location, CarLocation);
		const bool bComingAtThem = FVector::DotProduct(CarVelocity.GetSafeNormal2D(), ToPerson) > 0.6f;
		if (bComingAtThem && Person.Walker.FleeTimeLeft <= 0.f)
		{
			GTPWalk::Flee(Graph, Person.Walker, {CarLocation.X, CarLocation.Y}, Settings);
			++FledTotal;
			OnPedestrianScared.Broadcast(Person.Location, nullptr);
		}
		if (bRagdollOnHit && bComingAtThem && Distance < 170.f && CarVelocity.Size2D() > HitCarSpeed)
		{
			if (Actor->StartRagdoll(CarVelocity * 0.4f + FVector(0.f, 0.f, 200.f)))
			{
				Person.RagdollLeft = RagdollSeconds;
				++HitTotal;
				OnPedestrianHit.Broadcast(Person.Location, nullptr);
				return;
			}
		}
	}

	// A car passing close on the same walkway: step away from it towards the facade side.
	if (Cars.Num() > 0 && Person.Walker.FleeTimeLeft <= 0.f && Person.Leader < 0)
	{
		FVector NearCar;
		if (CarNear(Person.Location, 320.f, 150.f, &NearCar))
		{
			GTPWalk::Separate(Graph, Person.Walker, Person.Pos, Person.Dir, {NearCar.X, NearCar.Y}, Settings, 320.f);
		}
	}

	GTPWalk::FStepResult R;
	if (Person.Leader >= 0 && People.IsValidIndex(Person.Leader) && People[Person.Leader].bActive)
	{
		// Half of a pair: stays beside the partner and does what the partner does.
		const FPerson& Partner = People[Person.Leader];
		GTPWalk::Follow(Graph, Person.Walker, Partner.Walker, Settings);
		R = Partner.LastResult;
	}
	else
	{
		Person.Leader = -1;
		R = GTPWalk::Step(Graph, Person.Walker, DeltaTime, Rng, Settings, bCrossingUnsafe);
	}
	Person.LastResult = R;
	GTPWalk::FPoint P, D;
	GTPWalk::Locate(Graph, Person.Walker, P, D);
	Person.Pos = P;
	Person.Dir = D;
	FVector Wanted2D(P.X, P.Y, 0.f);

	// Social space: step aside for the player (display only, the walker keeps its line).
	const FVector Away = Wanted2D - FVector(PlayerLocation.X, PlayerLocation.Y, 0.f);
	const float PlayerDistance = Away.Size2D();
	if (PlayerDistance < PersonalSpace && PlayerDistance > 1.f)
	{
		Wanted2D += Away.GetSafeNormal2D() * FMath::Min(80.f, (PersonalSpace - PlayerDistance) * 0.6f);
	}
	Person.Location = FVector(Wanted2D.X, Wanted2D.Y, 0.f);
	Person.Distance = FVector::Dist2D(Person.Location, PlayerLocation);

	// Looking at a window: face the facade; otherwise the way it walks.
	const GTPWalk::FPoint Facing = GTPWalk::Facing(Graph, Person.Walker);
	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Facing.Y, Facing.X));
	Person.Yaw = FMath::RInterpTo(FRotator(0.f, Person.Yaw, 0.f), FRotator(0.f, TargetYaw, 0.f), DeltaTime, 8.f).Yaw;
	Actor->SetActorLocationAndRotation(FVector(Person.Location.X, Person.Location.Y, Person.GroundZ), FRotator(0.f, Person.Yaw, 0.f), false, nullptr, ETeleportType::None);

	const bool bRun = R.bFleeing && R.bMoving;
	const bool bStand = !R.bMoving;
	if (Person.bDrawn)
	{
		if (bStand)
		{
			const bool bPhone = Person.Walker.PauseKind == GTPWalk::EPause::Phone && Phone;
			Actor->PlayLoop(bPhone ? Phone.Get() : Idle.Get(), 1.f);
		}
		else if (bRun)
		{
			Actor->PlayLoop(Run, R.Speed / RunAnimationSpeed);
		}
		else
		{
			Actor->PlayLoop(Walk, R.Speed / AnimationWalkSpeed);
		}
	}
}

void UGTPPedestrianSubsystem::RefreshDetail()
{
	// Rank by distance: the closest MaxVisible within DrawDistance are drawn.
	TArray<FPerson*, TInlineAllocator<64>> Order;
	for (FPerson& P : People)
	{
		if (P.bActive)
		{
			Order.Add(&P);
		}
	}
	Order.Sort([](const FPerson& A, const FPerson& B) { return A.Distance < B.Distance; });
	int32 Shown = 0;
	for (FPerson* P : Order)
	{
		AGTPPedestrian* Actor = P->Actor.Get();
		if (!Actor)
		{
			continue;
		}
		const bool bDraw = P->Distance <= DrawDistance && (MaxVisible <= 0 || Shown < MaxVisible);
		Shown += bDraw ? 1 : 0;
		const bool bNear = P->Distance <= DetailDistance;
		const int32 NumLODs = Actor->GetBody()->GetSkeletalMeshAsset() ? Actor->GetBody()->GetSkeletalMeshAsset()->GetLODNum() : 1;
		const int32 Far = FMath::Clamp(FarLOD, 0, FMath::Max(0, NumLODs - 1));
		Actor->SetDetail(bDraw, bDraw && bNear, bNear ? -1 : Far);
		if (bReduceAnimRate)
		{
			Actor->SetAnimInterval(P->Distance < 1500.f ? 0.f : P->Distance < 2500.f ? 0.05f : P->Distance < 5000.f ? 0.083f : 0.125f);
		}
		P->bDrawn = bDraw;
	}
}

int32 UGTPPedestrianSubsystem::ScareAround(const FVector& Location, float Radius, AActor* Cause)
{
	int32 Count = 0;
	for (FPerson& P : People)
	{
		if (!P.bActive || (P.Actor.IsValid() && P.Actor->IsRagdoll()) || FVector::Dist2D(P.Location, Location) > Radius)
		{
			continue;
		}
		GTPWalk::Flee(Graph, P.Walker, {Location.X, Location.Y}, Settings);
		++FledTotal;
		++Count;
		OnPedestrianScared.Broadcast(P.Location, Cause);
	}
	return Count;
}

void UGTPPedestrianSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Wanted <= 0 || People.Num() == 0)
	{
		return;
	}
	Clock += DeltaTime;
	if (bFarPending && Clock > 3.f)
	{
		bFarPending = false;
		if (UWorld* World = GetWorld())
		{
			InitFarCrowd(*World);
		}
	}
	RefreshViewer();
	if (!bHasViewer)
	{
		return;
	}
	if (Clock >= NextCarRefresh)
	{
		NextCarRefresh = Clock + 0.4f;
		RefreshCars();
	}

	SeparatePeople();
	TickFarCrowd(DeltaTime);
	for (FPerson& P : People)
	{
		if (!P.bActive)
		{
			continue;
		}
		if (bSpreadFarUpdates && !P.bDrawn)
		{
			// Nobody sees this one: move it 5 times a second with the time that went by.
			P.FarAccumulated += DeltaTime;
			if (P.FarAccumulated < 0.2f)
			{
				continue;
			}
			const float Elapsed = P.FarAccumulated;
			P.FarAccumulated = 0.f;
			Update(P, Elapsed);
			continue;
		}
		P.FarAccumulated = 0.f;
		Update(P, DeltaTime);
	}

	// Ground height: a few people per frame, round robin.
	for (int32 i = 0; i < 3 && People.Num() > 0; ++i)
	{
		GroundCursor = (GroundCursor + 1) % People.Num();
		FPerson& P = People[GroundCursor];
		if (P.bActive && P.bDrawn && !(P.Actor.IsValid() && P.Actor->IsRagdoll()))
		{
			MoveGroundTo(P, false);
		}
	}

	if (Clock >= NextDetailRefresh)
	{
		NextDetailRefresh = Clock + 0.3f;
		RefreshDetail();
		for (FPerson& P : People)
		{
			if (!P.bActive)
			{
				continue;
			}
			const bool bFar = P.Distance > MaxDistance;
			const bool bDown = P.Actor.IsValid() && P.Actor->IsRagdoll() && P.RagdollLeft <= 0.f;
			if ((bFar || bDown) && !IsSeen(P.Location))
			{
				Retire(P);
			}
		}
	}

	// One person appears at a time.
	if (Clock >= NextSpawn)
	{
		NextSpawn = Clock + 0.15f;
		if (GetActiveCount() < Wanted)
		{
			for (FPerson& P : People)
			{
				if (!P.bActive)
				{
					TrySpawn(P);
					break;
				}
			}
		}
	}

	if (Clock >= NextStats)
	{
		NextStats = Clock + 10.f;
		LogStats();
	}
}

void UGTPPedestrianSubsystem::LogStats()
{
	int32 Drawn = 0, Active = 0;
	for (const FPerson& P : People)
	{
		Active += P.bActive ? 1 : 0;
		Drawn += (P.bActive && P.bDrawn) ? 1 : 0;
	}
	int32 Far = 0;
	for (const FFarAgent& A : FarAgents)
	{
		Far += A.bShown ? 1 : 0;
	}
	GTP_TRACE(LogGTPPedestrians, TEXT("active=%d drawn=%d wanted=%d spawned=%d fled=%d hit=%d far=%d"),
		Active, Drawn, Wanted, SpawnedTotal, FledTotal, HitTotal, Far);
}
