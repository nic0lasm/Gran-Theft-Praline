#include "GTPCameraProbe.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

namespace
{
	/** Under this bounds radius (cm) a pawn's part is a person's, not a car's. */
	constexpr float PersonRadius = 150.f;

	/** A person, or a prop small enough to see past (its whole bounds under SmallRadius). Cars and walls are not. */
	bool IsSeeThrough(const UPrimitiveComponent* Component, float SmallRadius)
	{
		if (!Component)
		{
			return false;
		}
		const AActor* Owner = Component->GetOwner();
		const bool bOnPawn = Owner && (Owner->IsA<APawn>() || (Owner->GetAttachParentActor() && Owner->GetAttachParentActor()->IsA<APawn>()));
		return Component->Bounds.SphereRadius < (bOnPawn ? PersonRadius : SmallRadius);
	}

	FCollisionQueryParams MakeParams(const TArray<const AActor*>& Ignored)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPCameraProbe), false);
		for (const AActor* Actor : Ignored)
		{
			if (Actor)
			{
				Params.AddIgnoredActor(Actor);
			}
		}
		return Params;
	}
}

float GTPCameraProbe::Sweep(UWorld* World, const FVector& From, const FVector& To, float Radius, float SmallRadius,
	const TArray<const AActor*>& Ignored, FString* HitName)
{
	if (!World || From.Equals(To, 1.0))
	{
		return 1.f;
	}
	FCollisionQueryParams Params = MakeParams(Ignored);
	// Walls, ground and cars pull the camera in; people and small props are passed (and hidden by the occluders).
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	// A few passes: each small prop hit is set aside and the sweep goes on past it.
	for (int32 Pass = 0; Pass < 6; ++Pass)
	{
		FHitResult Hit;
		if (!World->SweepSingleByObjectType(Hit, From, To, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Params))
		{
			return 1.f;
		}
		UPrimitiveComponent* Component = Hit.GetComponent();
		if (IsSeeThrough(Component, SmallRadius))
		{
			Params.AddIgnoredComponent(Component);
			continue;
		}
		if (Hit.bStartPenetrating)
		{
			// The start is already in that wall (the pivot through a thin facade): stay at the start, never beyond.
			if (HitName)
			{
				*HitName = GetNameSafe(Hit.GetActor());
			}
			return 0.f;
		}
		if (HitName)
		{
			*HitName = GetNameSafe(Hit.GetActor());
		}
		return FMath::Clamp(static_cast<float>(Hit.Time), 0.f, 1.f);
	}
	return 1.f;
}

int32 FGTPCameraOccluders::Update(UWorld* World, const FVector& Camera, const FVector& Target, float Radius, float SmallRadius, const TArray<const AActor*>& Ignored)
{
	TArray<UPrimitiveComponent*> InTheWay;
	if (World)
	{
		FCollisionQueryParams Params = MakeParams(Ignored);
		FCollisionObjectQueryParams Objects;
		Objects.AddObjectTypesToQuery(ECC_Pawn);
		Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
		Objects.AddObjectTypesToQuery(ECC_Vehicle);
		Objects.AddObjectTypesToQuery(ECC_WorldStatic);
		Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
		TArray<FHitResult> Hits;
		World->SweepMultiByObjectType(Hits, Camera, Target, FQuat::Identity, Objects, FCollisionShape::MakeSphere(Radius), Params);
		for (const FHitResult& Hit : Hits)
		{
			UPrimitiveComponent* Component = Hit.GetComponent();
			AActor* Owner = Hit.GetActor();
			if (!Component || !Owner)
			{
				continue;
			}
			const bool bCar = Owner->IsA<APawn>() && Component->Bounds.SphereRadius >= PersonRadius;
			if (Owner->IsA<APawn>() && !bCar)
			{
				// A person: every visible part of it (body, clothes, the visual attached to it).
				TArray<AActor*> Actors;
				Owner->GetAttachedActors(Actors, false, true);
				Actors.Add(Owner);
				for (AActor* Part : Actors)
				{
					TInlineComponentArray<UPrimitiveComponent*> Pieces(Part);
					for (UPrimitiveComponent* Piece : Pieces)
					{
						InTheWay.AddUnique(Piece);
					}
				}
			}
			else if (!bCar && IsSeeThrough(Component, SmallRadius))
			{
				InTheWay.AddUnique(Component);
			}
		}
	}
	// Shown again: what we hid and is no longer in the way.
	for (int32 Index = Hidden.Num() - 1; Index >= 0; --Index)
	{
		UPrimitiveComponent* Was = Hidden[Index].Get();
		if (!Was || !InTheWay.Contains(Was))
		{
			if (Was)
			{
				Was->SetHiddenInGame(false);
			}
			Hidden.RemoveAt(Index);
		}
	}
	// Hidden now: what is in the way and visible (something already hidden by other code is left to that code).
	for (UPrimitiveComponent* Is : InTheWay)
	{
		if (!Is->bHiddenInGame)
		{
			Is->SetHiddenInGame(true);
			Hidden.Add(Is);
		}
	}
	return Hidden.Num();
}

void FGTPCameraOccluders::Clear()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Was : Hidden)
	{
		if (Was.IsValid())
		{
			Was->SetHiddenInGame(false);
		}
	}
	Hidden.Reset();
}
