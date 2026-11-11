#include "GTPFarTraffic.h"

#include "GTPLog.h"
#include "GTPTraffic.h"
#include "GTPTrafficGhost.h"
#include "GTPVehicle.h"
#include "GTPEngineSoundComponent.h"
#include "CollisionQueryParams.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPFarTraffic, Log, All);

// Named, not anonymous: the unity build puts this file with GTPTraffic.cpp, which has its own CmToM.
namespace GTPFarTrafficLocal
{
	constexpr float FarCmToM = 0.01f;
	constexpr float FarFreeRoomCm = 1500.f;
}
using GTPFarTrafficLocal::FarCmToM;
using GTPFarTrafficLocal::FarFreeRoomCm;

AGTPFarTraffic::AGTPFarTraffic()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the AI drivers, so a swap sees this frame's positions.
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
	Random.Initialize(0x46415231);
}

int32 AGTPFarTraffic::Build(AGTPTrafficNetwork* InNetwork, int32 Count)
{
	Network = InNetwork;
	if (!Network || Network->GetLanes().Num() == 0 || Count <= 0)
	{
		return 0;
	}
	// Hand over where driven cars turn into ghosts (-GhostDistance= moves both).
	HandoverDistance = Network->GhostDistance;
	HideDistance = FMath::Min(HideDistance, Network->GhostWakeDistance);
	int32 InstancesSwitch = bInstances ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("FarInstances="), InstancesSwitch);
	int32 LeanSwitch = bLeanPuppets ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("FarLean="), LeanSwitch);
	bLeanPuppets = LeanSwitch != 0;
	if (InstancesSwitch != 0)
	{
		for (const FSoftObjectPath& Path : Meshes)
		{
			UStaticMesh* Mesh = Cast<UStaticMesh>(Path.TryLoad());
			if (!Mesh)
			{
				GTP_TRACE(LogGTPFarTraffic, TEXT("far traffic: mesh %s not found"), *Path.ToString());
				continue;
			}
			// Plain instances, not hierarchical: every instance moves every frame.
			UInstancedStaticMeshComponent* Model = NewObject<UInstancedStaticMeshComponent>(this);
			Model->SetMobility(EComponentMobility::Movable);
			Model->SetStaticMesh(Mesh);
			Model->SetupAttachment(RootComponent);
			Model->SetNumCustomDataFloats(3);
			Model->SetCullDistances(CullStart, CullEnd);
			Model->SetCastShadow(false);
			Model->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Model->SetCanEverAffectNavigation(false);
			Model->RegisterComponent();
			Models.Add(Model);
		}
		if (Models.Num() == 0)
		{
			GTP_TRACE(LogGTPFarTraffic, TEXT("far traffic: no parked car mesh, puppet cars instead"));
		}
	}
	bPuppets = Models.Num() == 0;
	if (Colours.Num() == 0)
	{
		Colours.Add(FLinearColor(0.8f, 0.8f, 0.8f));
	}
	int32 Placed = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FFarCar Car;
		const bool bPlaced = Place(Car, true);
		if (bPuppets)
		{
			float Yaw = 0.f;
			const FVector Location = bPlaced ? Where(Car, Yaw) : FVector(0.0, 0.0, -100000.0 - 1000.0 * Index);
			AGTPVehicle* Vehicle = Network->CreateCar(FTransform(FRotator(0.f, Yaw, 0.f), Location + FVector(0.f, 0.f, PuppetRideHeight)),
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Vehicle)
			{
				continue;
			}
			MakePuppet(Vehicle);
			Car.Puppet = Vehicle;
		}
		else
		{
			Car.Model = Index % Models.Num();
			UInstancedStaticMeshComponent* Model = Models[Car.Model];
			Car.Instance = Model->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), true);
			const FLinearColor& Paint = Colours[Random.RandRange(0, Colours.Num() - 1)];
			Model->SetCustomDataValue(Car.Instance, 0, Paint.R, false);
			Model->SetCustomDataValue(Car.Instance, 1, Paint.G, false);
			Model->SetCustomDataValue(Car.Instance, 2, Paint.B, false);
		}
		Placed += bPlaced ? 1 : 0;
		Cars.Add(Car);
		Show(Cars.Last());
	}
	for (UInstancedStaticMeshComponent* Model : Models)
	{
		Model->MarkRenderStateDirty();
	}
	GTP_TRACE(LogGTPFarTraffic, TEXT("far traffic: %d cars wanted, %d placed, %s%s"), Count, Placed,
		bPuppets ? TEXT("puppet cars") : *FString::Printf(TEXT("instances of %d models"), Models.Num()),
		bPuppets ? (bLeanPuppets ? TEXT(", lean (no glass, no sound or actor tick)") : TEXT(", whole")) : TEXT(""));
	return Placed;
}

double AGTPFarTraffic::TakeTickSeconds()
{
	const double Seconds = TickSeconds;
	TickSeconds = 0.0;
	return Seconds;
}

const GTPFar::FLanePath& AGTPFarTraffic::LanePath(int32 Lane)
{
	if (const GTPFar::FLanePath* Found = Paths.Find(Lane))
	{
		return *Found;
	}
	std::vector<FGTPDrivePoint> Points;
	for (const FVector& Point : Network->GetLanes()[Lane].Points)
	{
		Points.push_back({static_cast<float>(Point.X) * FarCmToM, static_cast<float>(Point.Y) * FarCmToM});
	}
	return Paths.Add(Lane, GTPFar::MakeLanePath(Points));
}

float AGTPFarTraffic::GroundZ(int32 Lane, int32 Point)
{
	const TArray<FVector>& Points = Network->GetLanes()[Lane].Points;
	Point = FMath::Clamp(Point, 0, Points.Num() - 1);
	TArray<float>& Heights = Ground.FindOrAdd(Lane);
	if (Heights.Num() != Points.Num())
	{
		Heights.Init(TNumericLimits<float>::Max(), Points.Num());
	}
	if (Heights[Point] == TNumericLimits<float>::Max())
	{
		// Same trace as the parked cars: static world only (bridge decks, slopes), cars ignored.
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPFarGround), false, this);
		const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
		const FVector Top(Points[Point].X, Points[Point].Y, Points[Point].Z + 500.0);
		Heights[Point] = GetWorld()->LineTraceSingleByObjectType(Hit, Top, Top - FVector(0.0, 0.0, 2500.0), Objects, Params)
			? static_cast<float>(Hit.ImpactPoint.Z) : static_cast<float>(Points[Point].Z);
	}
	return Heights[Point];
}

FVector AGTPFarTraffic::Where(const FFarCar& Car, float& OutYawDegrees)
{
	const GTPFar::FLanePath& Path = LanePath(Car.Lane);
	int Index = Car.Index;
	float Along = Car.Along;
	FGTPDrivePoint Position;
	float Yaw = 0.f;
	GTPGhost::Advance(Path.Points, Index, Along, 0.f, Position, Yaw);
	OutYawDegrees = FMath::RadiansToDegrees(Yaw);
	// Ground between the two lane points around the car.
	const int Last = static_cast<int>(Path.Points.size()) - 1;
	const int Next = Index < Last ? Index + 1 : Last;
	const float Segment = Next > Index ? Path.From[Next] - Path.From[Index] : 0.f;
	const float T = Segment > 1e-3f ? FMath::Clamp(Along / Segment, 0.f, 1.f) : 0.f;
	const float Z = FMath::Lerp(GroundZ(Car.Lane, Index), GroundZ(Car.Lane, Next), T);
	return FVector(Position.X / FarCmToM, Position.Y / FarCmToM, Z + GroundOffset);
}

bool AGTPFarTraffic::Place(FFarCar& Car, bool bOutOfSight)
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector PlayerLocation = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	const TArray<FGTPTrafficLane>& Lanes = Network->GetLanes();
	TArray<int32> Candidates;
	if (Player)
	{
		Network->LanesInRing(PlayerLocation, SpawnMinDistance, MaxDistance, Candidates);
		if (Candidates.Num() == 0)
		{
			return false;
		}
	}
	for (int32 Try = 0; Try < 30; ++Try)
	{
		const int32 Lane = Player ? Candidates[Random.RandRange(0, Candidates.Num() - 1)] : Random.RandRange(0, Lanes.Num() - 1);
		const TArray<FVector>& Points = Lanes[Lane].Points;
		if (Points.Num() < 4 || Lanes[Lane].Next.Num() == 0)
		{
			continue;
		}
		const int32 Point = Random.RandRange(1, Points.Num() - 3);
		const FVector& Location = Points[Point];
		const double Distance = FVector::Dist2D(Location, PlayerLocation);
		if (Player && (Distance < SpawnMinDistance || Distance > MaxDistance))
		{
			continue;
		}
		bool bFree = true;
		for (const FFarCar& Other : Cars)
		{
			if (&Other != &Car && Other.Lane != INDEX_NONE)
			{
				float Unused = 0.f;
				if (FVector::Dist2D(Where(Other, Unused), Location) < FarFreeRoomCm)
				{
					bFree = false;
					break;
				}
			}
		}
		for (const AGTPVehicle* Driven : Network->Cars)
		{
			if (bFree && Driven && FVector::Dist2D(Driven->GetActorLocation(), Location) < FarFreeRoomCm)
			{
				bFree = false;
			}
		}
		if (!bFree || (bOutOfSight && Network->IsSeenByPlayer(Location)))
		{
			continue;
		}
		Car.Lane = Lane;
		Car.Index = Point;
		Car.Along = 0.f;
		Car.Speed = Lanes[Lane].SpeedKmh / 3.6f * 0.8f;
		return true;
	}
	Car.Lane = INDEX_NONE;
	return false;
}

bool AGTPFarTraffic::Promote(FFarCar& Car)
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return false;
	}
	// Driven cars that are ghosts by now (past the ghost distance), with their distance.
	TArray<AGTPVehicle*> Candidates;
	std::vector<float> Distances;
	for (AGTPVehicle* Driven : Network->Cars)
	{
		const UGTPAIDriverComponent* Driver = IsValid(Driven) ? Driven->FindComponentByClass<UGTPAIDriverComponent>() : nullptr;
		if (Driver && Driver->IsGhost() && Driven->HasAIDriver() && !Driven->IsWrecked() && Driver->GetLane() != INDEX_NONE)
		{
			Candidates.Add(Driven);
			Distances.push_back(static_cast<float>(FVector::Dist2D(Driven->GetActorLocation(), Player->GetActorLocation())));
		}
	}
	const int Pick = GTPFar::PickSwap(Distances, HandoverDistance, Car.ToPlayer);
	if (Pick < 0)
	{
		return false;
	}
	AGTPVehicle* Driven = Candidates[Pick];
	UGTPAIDriverComponent* Driver = Driven->FindComponentByClass<UGTPAIDriverComponent>();
	const int32 DrivenLane = Driver->GetLane();
	const int32 DrivenPoint = Driver->GetLanePoint();
	const float DrivenSpeed = Driver->GetGhostSpeed();

	float Yaw = 0.f;
	const FVector Location = Where(Car, Yaw);
	if (bPuppets)
	{
		// Swap the roles: the puppet is driven from here, the driven car becomes this far car's puppet.
		AGTPVehicle* Puppet = Car.Puppet.Get();
		if (!Puppet)
		{
			return false;
		}
		Wake(Puppet, Car.Lane, Car.Index, Location, Yaw, Car.Speed);
		Network->UpdateDetail(Puppet, static_cast<float>(FVector::Dist(Player->GetActorLocation(), Location)));
		Network->Cars.Remove(Driven);
		MakePuppet(Driven);
		Car.Puppet = Driven;
	}
	else
	{
		Driver->PlaceGhost(Car.Lane, Car.Index, Location, Yaw, Car.Speed);
		// Drawn at once: the network only updates the detail of its cars twice a second.
		Network->UpdateDetail(Driven, static_cast<float>(FVector::Dist(Player->GetActorLocation(), Location)));
	}

	const TArray<FVector>& Points = Network->GetLanes()[DrivenLane].Points;
	Car.Lane = DrivenLane;
	Car.Index = FMath::Clamp(DrivenPoint, 0, FMath::Max(Points.Num() - 2, 0));
	Car.Along = 0.f;
	Car.Speed = DrivenSpeed;
	++Promoted;
	return true;
}

void AGTPFarTraffic::Show(FFarCar& Car)
{
	if (bPuppets)
	{
		AGTPVehicle* Puppet = Car.Puppet.Get();
		if (!Puppet)
		{
			return;
		}
		// Past the fade distance a puppet is hidden and not moved: no transform update for nobody.
		const bool bDrawn = Car.Lane != INDEX_NONE && Car.ToPlayer < static_cast<float>(CullEnd);
		if (Puppet->IsHidden() == bDrawn)
		{
			Puppet->SetActorHiddenInGame(!bDrawn);
		}
		if (bDrawn)
		{
			float Yaw = 0.f;
			const FVector Location = Where(Car, Yaw) + FVector(0.f, 0.f, PuppetRideHeight);
			Puppet->SetActorLocationAndRotation(Location, FRotator(0.f, Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		return;
	}
	if (!Models.IsValidIndex(Car.Model) || Car.Instance == INDEX_NONE)
	{
		return;
	}
	FTransform Transform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
	if (Car.Lane != INDEX_NONE)
	{
		float Yaw = 0.f;
		const FVector Location = Where(Car, Yaw);
		Transform = FTransform(FRotator(0.f, Yaw, 0.f), Location);
	}
	Models[Car.Model]->UpdateInstanceTransform(Car.Instance, Transform, true, false, true);
}

void AGTPFarTraffic::MakePuppet(AGTPVehicle* Vehicle)
{
	if (UGTPAIDriverComponent* Driver = Vehicle->FindComponentByClass<UGTPAIDriverComponent>())
	{
		Driver->ReleaseJunctions();
		// Physics and the Chaos vehicle off (the ghost switch does both), then no decisions at all.
		Driver->SetGhost(true);
		Driver->SetComponentTickEnabled(false);
	}
	Vehicle->SetActorEnableCollision(false);
	if (USkeletalMeshComponent* Body = Vehicle->GetMesh())
	{
		Body->SetForcedLOD(Body->GetNumLODs());
		Body->SetCastShadow(false);
		// No pose, no wheel animation: the body stays as it is until woken.
		Body->SetComponentTickEnabled(false);
		// Draw levers of section 8: no skinning at all, when the static draw is on.
		if (Network->StaticRenderDistance >= 0.f)
		{
			AGTPTrafficNetwork::SetCarRenderStatic(Body, true);
		}
	}
	if (bLeanPuppets)
	{
		SetPuppetLean(Vehicle, true);
	}
	if (ACharacter* Occupant = Vehicle->GetDriver())
	{
		Occupant->SetActorHiddenInGame(true);
	}
}

void AGTPFarTraffic::SetPuppetLean(AGTPVehicle* Vehicle, bool bLean)
{
	// Nothing a car 90 m away shows: its translucent glass (forward-lit by every street
	// lamp at night, fogged in the rain), the engine sound tick and the actor tick.
	TInlineComponentArray<UStaticMeshComponent*> Parts(Vehicle);
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part->ComponentHasTag(TEXT("Glass")) && Part->IsVisible() == bLean)
		{
			Part->SetVisibility(!bLean, false);
		}
	}
	if (UGTPEngineSoundComponent* Engine = Vehicle->FindComponentByClass<UGTPEngineSoundComponent>())
	{
		Engine->SetComponentTickEnabled(!bLean);
	}
	Vehicle->SetActorTickEnabled(!bLean);
}

void AGTPFarTraffic::Wake(AGTPVehicle* Vehicle, int32 Lane, int32 Point, const FVector& Location, float YawDegrees, float SpeedMs)
{
	Vehicle->SetActorHiddenInGame(false);
	Vehicle->SetActorEnableCollision(true);
	if (USkeletalMeshComponent* Body = Vehicle->GetMesh())
	{
		Body->SetComponentTickEnabled(true);
		AGTPTrafficNetwork::SetCarRenderStatic(Body, false);
	}
	SetPuppetLean(Vehicle, false);
	if (ACharacter* Occupant = Vehicle->GetDriver())
	{
		Occupant->SetActorHiddenInGame(false);
	}
	if (UGTPAIDriverComponent* Driver = Vehicle->FindComponentByClass<UGTPAIDriverComponent>())
	{
		Driver->PlaceGhost(Lane, Point, Location, YawDegrees, SpeedMs);
	}
	Network->Cars.Add(Vehicle);
}

void AGTPFarTraffic::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double Start = FPlatformTime::Seconds();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Network || !Player || Cars.Num() == 0)
	{
		return;
	}
	const FVector PlayerLocation = Player->GetActorLocation();
	const TArray<FGTPTrafficLane>& Lanes = Network->GetLanes();
	const FGTPTrafficLights& Lights = Network->GetTrafficLights();
	const float Time = Network->GetLightTime();

	// Who is where on each lane (m from its start): far cars and driven cars.
	TMap<int32, std::vector<float>> Occupants;
	for (const FFarCar& Car : Cars)
	{
		if (Car.Lane != INDEX_NONE)
		{
			Occupants.FindOrAdd(Car.Lane).push_back(GTPFar::DistanceOnLane(LanePath(Car.Lane), Car.Index, Car.Along));
		}
	}
	for (const AGTPVehicle* Driven : Network->Cars)
	{
		const UGTPAIDriverComponent* Driver = IsValid(Driven) ? Driven->FindComponentByClass<UGTPAIDriverComponent>() : nullptr;
		if (Driver && Lanes.IsValidIndex(Driver->GetLane()))
		{
			Occupants.FindOrAdd(Driver->GetLane()).push_back(GTPFar::DistanceOnLane(LanePath(Driver->GetLane()), Driver->GetLanePoint(), 0.f));
		}
	}

	for (FFarCar& Car : Cars)
	{
		if (Car.Lane == INDEX_NONE)
		{
			// Not placed yet (no free spot out of sight last time): try again now and then.
			if (Random.FRand() < 0.05f && Place(Car, true))
			{
				float Unused = 0.f;
				Car.ToPlayer = static_cast<float>(FVector::Dist2D(Where(Car, Unused), PlayerLocation));
				Show(Car);
			}
			continue;
		}
		const GTPFar::FLanePath& Path = LanePath(Car.Lane);
		const float Position = GTPFar::DistanceOnLane(Path, Car.Index, Car.Along);
		const std::vector<float>* Others = Occupants.Find(Car.Lane);
		const float Gap = Others ? GTPFar::GapAhead(Position, *Others) : -1.f;
		const float StopAt = Lights.StopDistance(Car.Lane, Time, Path.Length - Position, Car.Speed, Decel);
		const float Want = GTPFar::WantSpeed(Lanes[Car.Lane].SpeedKmh / 3.6f, Gap, StopAt, MinGap, Decel);
		Car.Speed = GTPGhost::NextSpeed(Car.Speed, Want, 2.f, 6.f, DeltaSeconds);

		FGTPDrivePoint Moved;
		float Yaw = 0.f;
		bool bDeadEnd = false;
		if (!GTPGhost::Advance(Path.Points, Car.Index, Car.Along, Car.Speed * DeltaSeconds, Moved, Yaw) && Car.Speed > 0.f)
		{
			// End of the lane, past the stop line: on to a next lane.
			const int32 Next = Network->PickNext(Car.Lane);
			if (Next != INDEX_NONE && Lanes[Next].Points.Num() >= 2)
			{
				Car.Lane = Next;
				Car.Index = 0;
				Car.Along = 0.f;
			}
			else
			{
				bDeadEnd = true;
				Car.Speed = 0.f;
			}
		}

		float Unused = 0.f;
		const FVector Location = Where(Car, Unused);
		Car.ToPlayer = static_cast<float>(FVector::Dist2D(Location, PlayerLocation));
		const bool bWantsRelocate = Car.ToPlayer < HideDistance || Car.ToPlayer > MaxDistance || bDeadEnd;
		// Line of sight only when the answer matters: one trace per far car would cost.
		const bool bSeen = bWantsRelocate && Network->IsSeenByPlayer(Location);
		const bool bPromote = Car.ToPlayer < HandoverDistance && Promote(Car);
		if (!bPromote && GTPFar::Decide(Car.ToPlayer, bSeen, false, bDeadEnd, HandoverDistance, HideDistance, MaxDistance) == GTPFar::EAction::Relocate)
		{
			Place(Car, true);
			++Relocated;
		}
		Show(Car);
	}
	for (UInstancedStaticMeshComponent* Model : Models)
	{
		Model->MarkRenderStateDirty();
	}

	StatsAge += DeltaSeconds;
	if (StatsAge >= 5.f)
	{
		int32 Placed = 0;
		for (const FFarCar& Car : Cars)
		{
			Placed += Car.Lane != INDEX_NONE ? 1 : 0;
		}
		GTP_TRACE(LogGTPFarTraffic, TEXT("far traffic: %d placed of %d, %d handed over, %d started again (last %.0f s)"),
			Placed, Cars.Num(), Promoted, Relocated, StatsAge);
		Promoted = 0;
		Relocated = 0;
		StatsAge = 0.f;
	}
	TickSeconds += FPlatformTime::Seconds() - Start;
}
