#include "GTPPedestrians.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/AnimInstance.h"
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
#include "GTPHealth.h"
#include "GTPThreat.h"
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

void AGTPPedestrian::ApplyLook(float HeightScale, const FLinearColor& Clothes, const TArray<FName>& TintParameters, float WidthScale)
{
	SetActorScale3D(FVector(HeightScale * WidthScale, HeightScale * WidthScale, HeightScale));
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

void AGTPPedestrian::Dress(USkeletalMesh* BodyMesh, const TArray<USkeletalMesh*>& Clothes, USkeletalMesh* Face, TSubclassOf<UAnimInstance> FaceAnim)
{
	if (BodyMesh)
	{
		Body->SetSkeletalMesh(BodyMesh);
	}
	for (USkeletalMeshComponent* Old : Parts)
	{
		if (Old)
		{
			Old->DestroyComponent();
		}
	}
	Parts.Reset();
	auto AddPart = [this](USkeletalMesh* Mesh, const TCHAR* Name) -> USkeletalMeshComponent*
	{
		if (!Mesh)
		{
			return nullptr;
		}
		USkeletalMeshComponent* Part = NewObject<USkeletalMeshComponent>(this, Name);
		Part->SetSkeletalMesh(Mesh);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->bCastContactShadow = false;
		Part->bReceivesDecals = false;
		Part->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Part->RegisterComponent();
		Parts.Add(Part);
		return Part;
	};
	int32 Index = 0;
	for (USkeletalMesh* Garment : Clothes)
	{
		if (USkeletalMeshComponent* Part = AddPart(Garment, *FString::Printf(TEXT("Garment%d"), Index++)))
		{
			Part->SetLeaderPoseComponent(Body);
		}
	}
	if (USkeletalMeshComponent* FacePart = AddPart(Face, TEXT("Face")))
	{
		if (FaceAnim)
		{
			FacePart->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			FacePart->SetAnimInstanceClass(FaceAnim);
		}
		else
		{
			// No face animation blueprint (it needs City Sample's C++): follow the body by bone name, or the head
			// stays alone in its reference pose where the actor started (a floating head).
			FacePart->SetLeaderPoseComponent(Body);
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
		for (USkeletalMeshComponent* Part : Parts)
		{
			if (Part)
			{
				Part->SetCastShadow(bCastShadow);
			}
		}
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
	// One cap for every physical body (ADR-0006): m1 3, lite 6; -Ragdoll=0 refuses all.
	// A refused pedestrian stays animated and runs.
	UGTPRagdollSubsystem* Cap = GetWorld() ? GetWorld()->GetSubsystem<UGTPRagdollSubsystem>() : nullptr;
	if (Cap && !Cap->TryClaimExternal(this))
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
		if (UGTPRagdollSubsystem* Cap = GetWorld() ? GetWorld()->GetSubsystem<UGTPRagdollSubsystem>() : nullptr)
		{
			Cap->ReleaseExternal(this);
		}
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
	// -Pedestrians=N sets the density, =mass leaves the people to the Mass crowd (GTPMass), =legacy keeps this system.
	FString Mode;
	if (!FParse::Value(FCommandLine::Get(), TEXT("Pedestrians="), Mode))
	{
		return 0; // the Mass crowd is the default (GTPMass); -Pedestrians=legacy brings this system back
	}
	{
		if (Mode.Equals(TEXT("mass"), ESearchCase::IgnoreCase))
		{
			return 0;
		}
		if (!Mode.Equals(TEXT("legacy"), ESearchCase::IgnoreCase))
		{
			return FMath::Max(0, FCString::Atoi(*Mode));
		}
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
			// Further than the ones who ran: stop and look.
			GawkAround(Location, Radius * 2.5f);
		});
	}
	Wanted = ChooseDensity();
	{
		FString ProfileName = TEXT("m1");
		if (const UGameInstance* GI = InWorld.GetGameInstance())
		{
			if (const UGTPGraphicsProfile* Gfx = GI->GetSubsystem<UGTPGraphicsProfile>())
			{
				ProfileName = Gfx->GetActiveProfile();
			}
		}
		if (ProfileName == TEXT("ultra"))
		{
			MaxVisible = FMath::Max(MaxVisible, MaxVisibleUltra);
			FarCrowdCount = FMath::Max(FarCrowdCount, FarCrowdUltra);
		}
	}
	FParse::Value(FCommandLine::Get(), TEXT("PedDemo="), DemoMode);
	if (!DemoMode.IsEmpty())
	{
		// Demo: people appear around the player, in view, so that the event can be filmed.
		MinSpawnDistance = 500.f;
		MaxSpawnDistance = FMath::Min(MaxSpawnDistance, 3500.f);
	}
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
	if (LoadedMeshes.Num() == 0)
	{
		// The player pawn has no main mesh (a MetaHuman keeps its body in other components): the game's mannequin.
		if (USkeletalMesh* Fallback = Cast<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin")).TryLoad()))
		{
			LoadedMeshes.Add(Fallback);
		}
	}
	if (LoadedMeshes.Num() == 0)
	{
		GTP_TRACE(LogGTPPedestrians, TEXT("no pedestrians: no skeletal mesh (none configured, the player has none, the mannequin is missing)"));
		Wanted = 0;
		return;
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
	// Optional: no default search, a phone loop is only played if one is named.
	Phone = Cast<UAnimSequenceBase>(PhoneAnimation.TryLoad());
	HandsUp = Resolve(HandsUpAnimation, TEXT("PedHandsUp="), {TEXT("handsup")});
	if (!HandsUp)
	{
		HandsUp = FindAnimation(LoadedMeshes[0], {TEXT("hands"), TEXT("up")});
	}
	if (!HandsUp)
	{
		HandsUp = FindAnimation(LoadedMeshes[0], {TEXT("surrender")});
	}
	GTP_TRACE(LogGTPPedestrians, TEXT("hands-up animation: %s"), HandsUp ? *HandsUp->GetPathName() : TEXT("none found (set HandsUpAnimation or -PedHandsUp=)"));
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
		Actor->ApplyLook(Rng.Range(1.f - HeightSpread, 1.f + HeightSpread), Palette(Rng.Next()), TintParameters, Rng.Range(1.f - WidthSpread, 1.f + WidthSpread));
		Actor->GetBody()->SetRelativeRotation(FRotator(0.f, MeshYawOffset, 0.f));
		Actor->SetDetail(false, false, -1);
		if (bCheapRendering)
		{
			Actor->MakeCheap();
		}
		if (UGTPThreatSubsystem::IsEnabled())
		{
			// Armed people aiming at them: the shared fear system drives hands up, backing away and running
			// (-ThreatResponse=1); locomotion stays with this subsystem.
			if (UGTPThreatResponseComponent* Fear = NewObject<UGTPThreatResponseComponent>(Actor, TEXT("Fear")))
			{
				const float Pick = Rng.Next();
				Fear->SetTemperament(Pick < 0.45f ? EGTPTemperament::Fleeing : (Pick < 0.85f ? EGTPTemperament::Compliant : EGTPTemperament::Frozen));
				Fear->RegisterComponent();
				Fear->OnThreatState.AddWeakLambda(this, [this](AActor* Body, EGTPThreatState /*Old*/, EGTPThreatState New)
				{
					OnThreatState(Body, static_cast<int32>(New));
				});
			}
		}
		P.Actor = Actor;
	}
	GTP_TRACE(LogGTPPedestrians, TEXT("pedestrians: on (%d), %d walkway edges, %d visible at most"), Wanted, Edges, MaxVisible);
	InitFarCrowd(InWorld);
}

void UGTPPedestrianSubsystem::InitFarCrowd(UWorld& InWorld)
{
	int32 Count = FarCrowdCount;
	FParse::Value(FCommandLine::Get(), TEXT("PedFar="), Count);
	UStaticMesh* Mesh = Cast<UStaticMesh>(FarCrowdMesh.TryLoad());
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
		if (!P.bActive && !P.bBorrowed && P.Actor.IsValid())
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
		FarInstances->UpdateInstanceTransform(i, FTransform(Facing, Location, FVector::OneVector), true, false, true);
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
				if (!Other.bActive && !Other.bBorrowed && Other.Actor.IsValid() && &Other != &Person)
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

AGTPPedestrian* UGTPPedestrianSubsystem::AcquireFromPool(const FVector& Location, float YawDegrees)
{
	FPerson* Pick = nullptr;
	for (FPerson& P : People)
	{
		if (!P.bActive && !P.bBorrowed && P.Actor.IsValid())
		{
			Pick = &P;
			break;
		}
	}
	if (!Pick)
	{
		// Nobody free: take the walker farthest from the player (never a ragdoll).
		float Best = -1.f;
		for (FPerson& P : People)
		{
			if (P.bActive && !P.bBorrowed && P.Actor.IsValid() && !P.Actor->IsRagdoll() && P.Distance > Best)
			{
				Best = P.Distance;
				Pick = &P;
			}
		}
		if (Pick)
		{
			Retire(*Pick);
		}
	}
	if (!Pick)
	{
		return nullptr;
	}
	AGTPPedestrian* Actor = Pick->Actor.Get();
	Pick->bBorrowed = true;
	Pick->bActive = false;
	Actor->ResetBody();
	Actor->SetDetail(true, true, -1);
	Actor->SetActorLocationAndRotation(Location, FRotator(0.f, YawDegrees, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	Actor->PlayLoop(Idle.Get() ? Idle.Get() : Walk.Get(), 1.f);
	return Actor;
}

void UGTPPedestrianSubsystem::ReturnToPool(AGTPPedestrian* Actor)
{
	for (FPerson& P : People)
	{
		if (P.bBorrowed && P.Actor.Get() == Actor)
		{
			P.bBorrowed = false;
			Retire(P);
			return;
		}
	}
}

int32 UGTPPedestrianSubsystem::GetBorrowedCount() const
{
	int32 Count = 0;
	for (const FPerson& P : People)
	{
		Count += P.bBorrowed ? 1 : 0;
	}
	return Count;
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

void UGTPPedestrianSubsystem::CallPoliceFrom(const FVector& Where)
{
	// The closest person who saw it (drawn, standing or walking, not frightened yet) takes out the phone.
	// The crime itself reaches the police through the witness logic of the wanted component.
	if (Rng.Next() >= CallPoliceChance)
	{
		return;
	}
	FPerson* Best = nullptr;
	float BestDistance = 3000.f;
	for (FPerson& P : People)
	{
		if (!P.bActive || !P.bDrawn || P.bBorrowed || P.bFighter || (P.Actor.IsValid() && P.Actor->IsRagdoll()) || P.Walker.FleeTimeLeft > 0.f)
		{
			continue;
		}
		const float D = FVector::Dist2D(P.Location, Where);
		if (D > 300.f && D < BestDistance)
		{
			BestDistance = D;
			Best = &P;
		}
	}
	if (Best)
	{
		GTPWalk::Freeze(Best->Walker, {float(Where.X), float(Where.Y)}, 8.f, GTPWalk::EPause::Phone);
	}
}

void UGTPPedestrianSubsystem::UpdateFighter(FPerson& Person, float DeltaTime)
{
	AGTPPedestrian* Actor = Person.Actor.Get();
	AActor* Target = Person.FightTarget.Get();
	Person.FightLeft -= DeltaTime;
	if (!Actor || !Target || Person.FightLeft <= 0.f)
	{
		// Calms down and leaves the scene: a new passer-by takes the place.
		Person.bFighter = false;
		Retire(Person);
		return;
	}
	const FVector To = Target->GetActorLocation() - Person.Location;
	const float Distance = To.Size2D();
	const FVector Dir = To.GetSafeNormal2D();
	const float RunSpeed = 340.f;
	if (Distance > 110.f)
	{
		Person.Location += Dir * RunSpeed * DeltaTime;
		Actor->PlayLoop(Run, RunSpeed / RunAnimationSpeed);
	}
	else
	{
		Actor->PlayLoop(Idle.Get(), 1.f);
		Person.FightCooldown -= DeltaTime;
		if (Person.FightCooldown <= 0.f)
		{
			Person.FightCooldown = 1.2f;
			if (UGTPHealthComponent* Health = Target->FindComponentByClass<UGTPHealthComponent>())
			{
				Health->ApplyDamage(FightDamage, EGTPDamageCause::Melee, Actor, true);
			}
		}
	}
	Person.Yaw = FRotator::NormalizeAxis(Dir.Rotation().Yaw);
	Actor->SetActorLocationAndRotation(FVector(Person.Location.X, Person.Location.Y, Person.GroundZ), FRotator(0.f, Person.Yaw, 0.f), false, nullptr, ETeleportType::None);
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
	if (Person.bFighter)
	{
		UpdateFighter(Person, DeltaTime);
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
			GTPWalk::Flee(Graph, Person.Walker, {float(CarLocation.X), float(CarLocation.Y)}, Settings);
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
				GawkAround(Person.Location, 2500.f);
				CallPoliceFrom(Person.Location);
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
			GTPWalk::Separate(Graph, Person.Walker, Person.Pos, Person.Dir, {float(NearCar.X), float(NearCar.Y)}, Settings, 320.f);
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
			const bool bHandsUp = Person.Walker.PauseKind == GTPWalk::EPause::HandsUp && HandsUp;
			Actor->PlayLoop(bHandsUp ? HandsUp.Get() : bPhone ? Phone.Get() : Idle.Get(), 1.f);
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
		const APawn* CausePawn = Cast<APawn>(Cause);
		if (CausePawn && CausePawn->IsPlayerControlled() && !P.bBorrowed && !P.bFighter && Rng.Next() < FightBackChance)
		{
			// Some do not run: they go for the player.
			P.bFighter = true;
			P.FightLeft = FightSeconds;
			P.FightCooldown = 0.6f;
			P.FightTarget = Cause;
			P.Walker.Pause = 0.f;
			++Count;
			continue;
		}
		GTPWalk::Flee(Graph, P.Walker, {float(Location.X), float(Location.Y)}, Settings);
		++FledTotal;
		++Count;
		OnPedestrianScared.Broadcast(P.Location, Cause);
	}
	return Count;
}

int32 UGTPPedestrianSubsystem::GawkAround(const FVector& Location, float Radius)
{
	int32 Count = 0;
	const GTPWalk::FPoint Point{float(Location.X), float(Location.Y)};
	for (FPerson& P : People)
	{
		if (!P.bActive || P.bBorrowed || (P.Actor.IsValid() && P.Actor->IsRagdoll()) || FVector::Dist2D(P.Location, Location) > Radius)
		{
			continue;
		}
		Count += GTPWalk::Gawk(P.Walker, Point, Rng, Settings) ? 1 : 0;
	}
	return Count;
}

void UGTPPedestrianSubsystem::OnThreatState(AActor* Body, int32 NewState)
{
	const UWorld* World = GetWorld();
	const UGTPThreatSubsystem* Threat = World ? World->GetSubsystem<UGTPThreatSubsystem>() : nullptr;
	if (!Threat)
	{
		return;
	}
	const FVector Origin = Threat->GetOrigin();
	const GTPWalk::FPoint From{float(Origin.X), float(Origin.Y)};
	for (FPerson& P : People)
	{
		if (!P.bActive || P.Actor.Get() != Body)
		{
			continue;
		}
		switch (static_cast<EGTPThreatState>(NewState))
		{
		case EGTPThreatState::Alarmed:
			GTPWalk::Freeze(P.Walker, From, 3.f, GTPWalk::EPause::Gawk);
			break;
		case EGTPThreatState::HandsUp:
		case EGTPThreatState::Complying:
			GTPWalk::Freeze(P.Walker, From, 60.f, GTPWalk::EPause::HandsUp);
			break;
		case EGTPThreatState::Backing:
		case EGTPThreatState::Fleeing:
			GTPWalk::Flee(Graph, P.Walker, From, Settings);
			++FledTotal;
			OnPedestrianScared.Broadcast(P.Location, Threat->GetSource());
			break;
		case EGTPThreatState::Calm:
			if (P.Walker.PauseKind == GTPWalk::EPause::HandsUp)
			{
				P.Walker.Pause = 0.f;
			}
			break;
		default:
			break;
		}
		return;
	}
}

void UGTPPedestrianSubsystem::SpreadPanic()
{
	// Each person who starts running passes the panic on once to the quiet people around, a few steps at most.
	for (FPerson& P : People)
	{
		if (!P.bActive)
		{
			continue;
		}
		if (P.Walker.FleeTimeLeft <= 0.f)
		{
			P.bPanicSpread = false;
			continue;
		}
		if (P.bPanicSpread)
		{
			continue;
		}
		P.bPanicSpread = true;
		for (FPerson& Other : People)
		{
			if (&Other == &P || !Other.bActive || Other.bBorrowed || (Other.Actor.IsValid() && Other.Actor->IsRagdoll())
				|| FVector::Dist2D(Other.Location, P.Location) > Settings.PanicRadius)
			{
				continue;
			}
			if (GTPWalk::CatchPanic(Graph, Other.Walker, P.Walker.Generation, P.Pos, Settings, Rng))
			{
				++FledTotal;
				OnPedestrianScared.Broadcast(Other.Location, nullptr);
			}
		}
	}
}

void UGTPPedestrianSubsystem::TickDemo()
{
	if (DemoMode.IsEmpty() || Clock < NextDemo)
	{
		return;
	}
	// The closest drawn person is where the event happens.
	// Preferably somebody in view, 8 to 22 m away (closest to 14 m); else the closest drawn one.
	FPerson* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (FPerson& P : People)
	{
		if (!P.bActive || !P.bDrawn || P.bBorrowed)
		{
			continue;
		}
		const bool bInView = P.Distance >= 800.f && P.Distance <= 2200.f && IsSeen(P.Location);
		const float Score = bInView ? FMath::Abs(P.Distance - 1400.f) : 100000.f + P.Distance;
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = &P;
		}
	}
	if (!Best)
	{
		return;
	}
	NextDemo = Clock + 25.f;
	const FVector Where = Best->Location;
	GTP_TRACE(LogGTPPedestrians, TEXT("demo %s at %s"), *DemoMode, *Where.ToCompactString());
	if (DemoMode == TEXT("panic"))
	{
		ScareAround(Where, 400.f, nullptr);
	}
	else if (DemoMode == TEXT("gawk"))
	{
		OnPedestrianHit.Broadcast(Where, nullptr);
		GawkAround(Where, 2500.f);
	}
	else if (DemoMode == TEXT("holdup"))
	{
		// A gun pointed at everybody within 12 m: they freeze with their hands up (the threat response does the same in play).
		for (FPerson& P : People)
		{
			if (P.bActive && P.bDrawn && !P.bBorrowed && FVector::Dist(P.Actor.IsValid() ? P.Actor->GetActorLocation() : Where, Where) < 1200.f)
			{
				GTPWalk::Freeze(P.Walker, GTPWalk::FPoint{float(Where.X), float(Where.Y)}, 20.f, GTPWalk::EPause::HandsUp);
			}
		}
	}
	else if (DemoMode == TEXT("shot"))
	{
		if (UGTPAudioWorld* AudioWorld = GetWorld() ? GetWorld()->GetSubsystem<UGTPAudioWorld>() : nullptr)
		{
			AudioWorld->OnLoudSound.Broadcast(Where, 1500.f, nullptr);
		}
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

	SeparatePeople();
	SpreadPanic();
	TickDemo();
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
				if (!P.bActive && !P.bBorrowed)
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
