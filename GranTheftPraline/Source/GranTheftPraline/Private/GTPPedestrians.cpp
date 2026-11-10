#include "GTPPedestrians.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "GTPGraphicsProfile.h"
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
	for (const FAssetData& Asset : Assets)
	{
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
	Wanted = ChooseDensity();
	if (Wanted <= 0)
	{
		GTP_TRACE(LogGTPPedestrians, TEXT("pedestrians: off"));
		return;
	}
	const FString File = WalkwaysFileFor(UGameplayStatics::GetCurrentLevelName(&InWorld, true));
	const int32 Edges = LoadWalkwaysFromCsv(File, Graph);
	if (Edges == 0)
	{
		GTP_TRACE(LogGTPPedestrians, TEXT("no pedestrians: %s not found"), *File);
		Wanted = 0;
		return;
	}

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
	Walk = Resolve(WalkAnimation, TEXT("PedWalk="), {TEXT("walk")});
	Idle = Resolve(IdleAnimation, TEXT("PedIdle="), {TEXT("idle")});
	Run = Resolve(RunAnimation, TEXT("PedRun="), {TEXT("run"), TEXT("jog")});
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
		Actor->GetBody()->SetRelativeRotation(FRotator(0.f, MeshYawOffset, 0.f));
		Actor->SetDetail(false, false, -1);
		P.Actor = Actor;
	}
	GTP_TRACE(LogGTPPedestrians, TEXT("pedestrians: on (%d), %d walkway edges, %d visible at most"), Wanted, Edges, MaxVisible);
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
		Person.Walker.SpeedScale = W.SpeedScale;
		Person.Location = Location;
		Person.GroundZ = PlayerLocation.Z;
		MoveGroundTo(Person, true);
		Person.Yaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
		Person.bActive = true;
		Person.bRunning = false;
		Person.bIdle = false;
		Person.bDrawn = false;
		Person.RagdollLeft = 0.f;
		Actor->ResetBody();
		Actor->SetActorLocationAndRotation(FVector(Location.X, Location.Y, Person.GroundZ), FRotator(0.f, Person.Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		++SpawnedTotal;
		return true;
	}
	return false;
}

void UGTPPedestrianSubsystem::Retire(FPerson& Person)
{
	Person.bActive = false;
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
		bCrossingUnsafe = CarNear(Mid, CrossingLookRadius, 150.f);
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

	const GTPWalk::FStepResult R = GTPWalk::Step(Graph, Person.Walker, DeltaTime, Rng, Settings, bCrossingUnsafe);
	GTPWalk::FPoint P, D;
	GTPWalk::Locate(Graph, Person.Walker, P, D);
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

	const float TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X));
	Person.Yaw = FMath::RInterpTo(FRotator(0.f, Person.Yaw, 0.f), FRotator(0.f, TargetYaw, 0.f), DeltaTime, 8.f).Yaw;
	Actor->SetActorLocationAndRotation(FVector(Person.Location.X, Person.Location.Y, Person.GroundZ), FRotator(0.f, Person.Yaw, 0.f), false, nullptr, ETeleportType::None);

	const bool bRun = R.bFleeing && R.bMoving;
	const bool bStand = !R.bMoving;
	if (Person.bDrawn)
	{
		if (bStand)
		{
			Actor->PlayLoop(Idle, 1.f);
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
	Order.Sort([](const FPerson* A, const FPerson* B) { return A->Distance < B->Distance; });
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
		P->bDrawn = bDraw;
	}
}

void UGTPPedestrianSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Wanted <= 0 || People.Num() == 0)
	{
		return;
	}
	Clock += DeltaTime;
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

	for (FPerson& P : People)
	{
		if (P.bActive)
		{
			Update(P, DeltaTime);
		}
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
	GTP_TRACE(LogGTPPedestrians, TEXT("active=%d drawn=%d wanted=%d spawned=%d fled=%d hit=%d"),
		Active, Drawn, Wanted, SpawnedTotal, FledTotal, HitTotal);
}
