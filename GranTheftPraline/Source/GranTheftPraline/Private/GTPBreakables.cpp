#include "GTPBreakables.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GTPGraphicsProfile.h"
#include "GTPLog.h"
#include "GTPVehicle.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogGTPBreakables);

namespace
{
const FName TagTippable(TEXT("GTP_Breakable_R"));
const FName TagBreakable(TEXT("GTP_Breakable_C"));
}

bool UGTPBreakableSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	int32 Value = 1;
	FParse::Value(FCommandLine::Get(), TEXT("-Breakables="), Value);
	return Value != 0 && Super::ShouldCreateSubsystem(Outer);
}

int32 UGTPBreakableSubsystem::MaxLive() const
{
	if (const UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (const UGTPGraphicsProfile* Gfx = GI->GetSubsystem<UGTPGraphicsProfile>())
		{
			return Gfx->GetActiveProfile() == TEXT("m1") ? MaxLiveM1 : MaxLiveOther;
		}
	}
	return MaxLiveM1;
}

void UGTPBreakableSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Collect();
	GTP_TRACE(LogGTPBreakables, TEXT("breakables: %d groups"), Groups.Num());
	if (Groups.Num() > 0)
	{
		InWorld.GetTimerManager().SetTimer(CheckTimer, this, &UGTPBreakableSubsystem::Check, CheckInterval, true);
	}
}

void UGTPBreakableSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	Groups.Reset();
	Impactors.Reset();
	Thrown.Reset();
	Super::Deinitialize();
}

void UGTPBreakableSubsystem::Collect()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TArray<UHierarchicalInstancedStaticMeshComponent*> Components;
		It->GetComponents(Components);
		for (UHierarchicalInstancedStaticMeshComponent* Component : Components)
		{
			const bool bR = Component->ComponentHasTag(TagTippable);
			const bool bC = Component->ComponentHasTag(TagBreakable);
			if ((!bR && !bC) || !Component->GetStaticMesh())
			{
				continue;
			}
			FGroup Group;
			Group.Component = Component;
			Group.Kind = bC ? GTPBreak::EKind::Breakable : GTPBreak::EKind::Tippable;
			Group.MassKg = bC ? 100.f : 20.f;
			for (const FName& Tag : Component->ComponentTags)
			{
				const FString Text = Tag.ToString();
				if (Text.StartsWith(TEXT("GTP_MassKg=")))
				{
					Group.MassKg = FMath::Max(1.f, FCString::Atof(*Text.RightChop(11)));
				}
				else if (Text.StartsWith(TEXT("GTP_BrokenMesh=")))
				{
					Group.BrokenMesh = FSoftObjectPath(Text.RightChop(15));
				}
				else if (Text.StartsWith(TEXT("GTP_BrokenTopMesh=")))
				{
					Group.TopMesh = FSoftObjectPath(Text.RightChop(18));
				}
			}
			Groups.Add(Group);
		}
	}
}

void UGTPBreakableSubsystem::RefreshImpactors()
{
	Impactors.Reset();
	for (TActorIterator<AGTPVehicle> It(GetWorld()); It; ++It)
	{
		Impactors.Add(*It);
	}
	if (const APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Impactors.AddUnique(Pawn); // on foot; when driving this is the vehicle, already listed
		}
	}
}

void UGTPBreakableSubsystem::Check()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextRefresh)
	{
		RefreshImpactors();
		NextRefresh = Now + 1.f;
	}
	const APawn* PlayerPawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;

	for (const TWeakObjectPtr<AActor>& Weak : Impactors)
	{
		AActor* Impactor = Weak.Get();
		if (!Impactor)
		{
			continue;
		}
		const FVector Velocity = Impactor->GetVelocity();
		if (Velocity.Size2D() * 0.036f < MinImpactorKmh)
		{
			continue;
		}
		const bool bCar = Impactor->IsA<AGTPVehicle>();
		float Mass = PlayerMassKg;
		if (bCar)
		{
			const UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Impactor->GetRootComponent());
			Mass = Body ? FMath::Max(300.f, Body->GetMass()) : 1200.f;
		}
		else if (Impactor != PlayerPawn)
		{
			continue;
		}
		FVector Origin, Extent;
		Impactor->GetActorBounds(true, Origin, Extent);
		const FBox Box = FBox::BuildAABB(Origin, Extent).ExpandBy(20.f);
		const float Radius = Extent.Size2D() + 60.f;
		const FVector Direction = Velocity.GetSafeNormal2D();

		for (FGroup& Group : Groups)
		{
			UHierarchicalInstancedStaticMeshComponent* Component = Group.Component.Get();
			if (!Component)
			{
				continue;
			}
			TArray<int32> Near = Component->GetInstancesOverlappingSphere(Origin, Radius, true);
			// Highest index first: removing an instance moves the last one into its place.
			Near.Sort([](int32 A, int32 B) { return A > B; });
			for (int32 Index : Near)
			{
				FTransform Transform;
				if (!Component->GetInstanceTransform(Index, Transform, true))
				{
					continue;
				}
				if (!Box.IsInside(Transform.GetLocation() + FVector(0.f, 0.f, 50.f)))
				{
					continue;
				}
				Hit(Group, Index, Direction, static_cast<float>(Velocity.Size2D()), Mass, Impactor);
			}
		}
	}
}

void UGTPBreakableSubsystem::SpawnFixed(UHierarchicalInstancedStaticMeshComponent* Source, UStaticMesh* Mesh, const FTransform& Transform)
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Transform.GetLocation(), Transform.Rotator(), Params);
	if (!Actor)
	{
		return;
	}
	Actor->SetMobility(EComponentMobility::Movable);
	UStaticMeshComponent* Visual = Actor->GetStaticMeshComponent();
	Visual->SetStaticMesh(Mesh);
	Visual->SetWorldScale3D(Transform.GetScale3D());
	for (int32 Slot = 0; Slot < Source->GetNumMaterials(); ++Slot)
	{
		Visual->SetMaterial(Slot, Source->GetMaterial(Slot));
	}
	Visual->SetCollisionProfileName(TEXT("BlockAll"));
	Actor->SetMobility(EComponentMobility::Static);
}

void UGTPBreakableSubsystem::Hit(FGroup& Group, int32 InstanceIndex, const FVector& Direction, float ClosingCmS, float ImpactorMassKg, AActor* Impactor)
{
	UHierarchicalInstancedStaticMeshComponent* Component = Group.Component.Get();
	const GTPBreak::FResult R = GTPBreak::Resolve(Group.Kind, Group.MassKg, ImpactorMassKg, ClosingCmS);
	if (!Component || (R.Outcome != GTPBreak::EOutcome::Topple && R.Outcome != GTPBreak::EOutcome::Break))
	{
		return;
	}
	FTransform Transform;
	if (!Component->GetInstanceTransform(InstanceIndex, Transform, true))
	{
		return;
	}
	UStaticMesh* Mesh = Component->GetStaticMesh();
	bool bInstanceStillThere = true;
	if (R.Outcome == GTPBreak::EOutcome::Break && Group.BrokenMesh.IsValid())
	{
		// A stump or a bent pole stays in place; the top (if any) falls, else nothing falls.
		UStaticMesh* Stays = Cast<UStaticMesh>(Group.BrokenMesh.TryLoad());
		if (Stays)
		{
			SpawnFixed(Component, Stays, Transform);
			UStaticMesh* Top = Group.TopMesh.IsValid() ? Cast<UStaticMesh>(Group.TopMesh.TryLoad()) : nullptr;
			Component->RemoveInstance(InstanceIndex);
			++TotalHit;
			bInstanceStillThere = false;
			if (!Top)
			{
				return;
			}
			Mesh = Top;
		}
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Body = GetWorld()->SpawnActor<AStaticMeshActor>(Transform.GetLocation(), Transform.Rotator(), Params);
	if (!Body)
	{
		return;
	}
	Body->SetMobility(EComponentMobility::Movable);
	UStaticMeshComponent* Visual = Body->GetStaticMeshComponent();
	Visual->SetStaticMesh(Mesh);
	Visual->SetWorldScale3D(Transform.GetScale3D());
	for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
	{
		Visual->SetMaterial(Slot, Component->GetMaterial(Slot));
	}
	Visual->SetCollisionProfileName(TEXT("PhysicsActor"));
	Visual->SetSimulatePhysics(true);
	Visual->SetMassOverrideInKg(NAME_None, Group.MassKg, true);
	const FVector Throw = Direction * R.ThrowCmS + FVector(0.f, 0.f, R.ThrowCmS * 0.25f);
	Visual->SetPhysicsLinearVelocity(Throw);
	Visual->SetPhysicsAngularVelocityInRadians(FVector::CrossProduct(FVector::UpVector, Direction) * 4.f);
	Body->SetLifeSpan(ThrownLifetime);
	if (bInstanceStillThere)
	{
		Component->RemoveInstance(InstanceIndex);
		++TotalHit;
	}

	Thrown.Add(Body);
	Thrown.RemoveAll([](const TWeakObjectPtr<AStaticMeshActor>& P) { return !P.IsValid(); });
	while (Thrown.Num() > MaxLive())
	{
		if (AStaticMeshActor* Oldest = Thrown[0].Get())
		{
			Oldest->Destroy();
		}
		Thrown.RemoveAt(0);
	}

	// The car gives up a little speed for the momentum it hands over.
	if (R.ImpactorLossKmh > 0.01f && Impactor->IsA<AGTPVehicle>())
	{
		if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Impactor->GetRootComponent()))
		{
			Root->AddImpulse(-Direction * (R.ImpactorLossKmh / 0.036f), NAME_None, true);
		}
	}
}
