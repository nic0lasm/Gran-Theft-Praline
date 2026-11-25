#include "GTPTraffic.h"

#include "GTPDeterminism.h"
#include "GTPFlightCircuit.h"
#include "GTPLaneRoute.h"
#include "GTPRoadWidth.h"
#include "GTPLog.h"
#include "GTPVehicle.h"
#include "GTPTrafficGhost.h"
#include "GTPParkedCars.h"
#include "GTPFarTraffic.h"
#include "GTPKerbSpot.h"
#include "GTPTrafficFar.h"
#include "GTPLiteDriver.h"
#include "GTPHornComponent.h"
#include "GTPThreat.h"
#include "GTPIncidentLogic.h"
#include "GTPTrafficIncident.h"
#include "GTPWeapon.h"
#include "GameFramework/Controller.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "EngineUtils.h"
#include "CoreGlobals.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "AIController.h"
#include "CollisionQueryParams.h"
#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPTraffic, Log, All);

namespace
{
	constexpr float CmToM = 0.01f;
	/** Metres of lanes chosen ahead of the current one (see tools/vehicles/traffic_sim). */
	constexpr float RouteAheadM = 50.f;
	/** A lane follower with a goal looks for its way again this often (the goal is a car that flees). */
	constexpr double FollowerReplanSeconds = 2.0;

	/** Centimetres between Location and the lane's line from its point First on (huge when nothing is left of it). */
	float FollowerDistanceToLane(const TArray<FVector>& Points, const FVector& Location, int32 First)
	{
		const FVector2D P(Location.X, Location.Y);
		float Best = TNumericLimits<float>::Max();
		for (int32 Index = FMath::Max(First, 0); Index + 1 < Points.Num(); ++Index)
		{
			const FVector2D A(Points[Index].X, Points[Index].Y);
			const FVector2D B(Points[Index + 1].X, Points[Index + 1].Y);
			Best = FMath::Min(Best, static_cast<float>(FVector2D::Distance(P, FMath::ClosestPointOnSegment2D(P, A, B))));
		}
		return Best;
	}

	/**
	 * Height of a parked car's pivot above the ground at spawn, so that the lowest point of its mesh
	 * starts 40 cm above the road for every car (as our cars, pivot at the ground, always did):
	 * City Sample's meshes put that point anywhere from 0 to 42 cm above their pivot.
	 * Read from the skeletal mesh's bounds on the class defaults.
	 */
	float ParkedClearance(TSubclassOf<AGTPVehicle> Class)
	{
		const AGTPVehicle* Defaults = Class ? Class->GetDefaultObject<AGTPVehicle>() : nullptr;
		const USkeletalMeshComponent* Body = Defaults ? Defaults->GetMesh() : nullptr;
		const USkeletalMesh* Mesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
		if (!Mesh)
		{
			return 40.f;
		}
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		const float Bottom = static_cast<float>(Bounds.Origin.Z - Bounds.BoxExtent.Z) + static_cast<float>(Body->GetRelativeLocation().Z);
		return FMath::Clamp(40.f - Bottom, -100.f, 200.f);
	}

	/** A parked car of Class seen from above (cm): how far its mesh's bounds reach ahead of its origin, behind it, and to each side. */
	void ParkedBody(TSubclassOf<AGTPVehicle> Class, float& OutFront, float& OutBack, float& OutHalfWidth)
	{
		OutFront = 230.f;
		OutBack = 230.f;
		OutHalfWidth = 95.f;
		const AGTPVehicle* Defaults = Class ? Class->GetDefaultObject<AGTPVehicle>() : nullptr;
		const USkeletalMeshComponent* Body = Defaults ? Defaults->GetMesh() : nullptr;
		const USkeletalMesh* Mesh = Body ? Body->GetSkeletalMeshAsset() : nullptr;
		if (!Mesh)
		{
			return;
		}
		const FBoxSphereBounds Bounds = Mesh->GetBounds();
		float Centre = static_cast<float>(Bounds.Origin.X);
		float HalfLength = static_cast<float>(Bounds.BoxExtent.X);
		float HalfWidth = static_cast<float>(Bounds.BoxExtent.Y);
		// A mesh turned a quarter turn in its car (modelled along Y): its length is along Y.
		const float MeshYaw = FMath::Abs(static_cast<float>(FRotator::NormalizeAxis(Body->GetRelativeRotation().Yaw)));
		if (MeshYaw > 45.f && MeshYaw < 135.f)
		{
			Centre = 0.f;
			HalfLength = static_cast<float>(Bounds.BoxExtent.Y);
			HalfWidth = static_cast<float>(Bounds.BoxExtent.X);
		}
		Centre += static_cast<float>(Body->GetRelativeLocation().X);
		OutFront = FMath::Clamp(Centre + HalfLength, 100.f, 450.f);
		OutBack = FMath::Clamp(HalfLength - Centre, 100.f, 450.f);
		OutHalfWidth = FMath::Clamp(HalfWidth, 60.f, 150.f);
	}
}

float FGTPTrafficLane::LengthCm() const
{
	float Length = 0.f;
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		Length += static_cast<float>(FVector::Dist2D(Points[Index - 1], Points[Index]));
	}
	return Length;
}

// --- Lane follower (demo driving) -------------------------------------------

bool FGTPLaneFollower::StartNear(AGTPTrafficNetwork* InNetwork, const FVector& Location, float YawDegrees, float MaxDistanceCm)
{
	int32 Lane = INDEX_NONE, Point = 0;
	if (!InNetwork)
	{
		return false;
	}
	// A flight on the circuit of the map: a lane of the circuit when one is near, before the nearest lane of all. The
	// theft of -GTARun is on the right-hand lane of the quay, 3 m from the lane the approach starts on.
	const bool bOnCircuitLane = bFollowCircuit && InNetwork->FindNearestLane(Location, YawDegrees, MaxDistanceCm, Lane, Point, true);
	if (!bOnCircuitLane && !InNetwork->FindNearestLane(Location, YawDegrees, MaxDistanceCm, Lane, Point))
	{
		return false;
	}
	Network = InNetwork;
	Route.Reset();
	Route.Add(Lane);
	LaneBehind.clear();
	Closest = Point;
	Plan();
	if ((bFollowCircuit || bDriveCircuitLines) && InNetwork->GetFlightCircuit().LineOf(Lane))
	{
		// The lane is driven on its line (Plan): the closest point is one of the line's, not of the lane's.
		const float AtX = static_cast<float>(Location.X) * CmToM;
		const float AtY = static_cast<float>(Location.Y) * CmToM;
		float NearestSquared = TNumericLimits<float>::Max();
		Closest = 0;
		for (int32 Index = 0; Index < CurrentLaneLength && Index < static_cast<int32>(Path.size()); ++Index)
		{
			const float GapSquared = FMath::Square(Path[Index].X - AtX) + FMath::Square(Path[Index].Y - AtY);
			if (GapSquared < NearestSquared)
			{
				NearestSquared = GapSquared;
				Closest = Index;
			}
		}
	}
	return true;
}

float FGTPLaneFollower::OffPathMetres(const FVector& Location) const
{
	const int N = static_cast<int>(Path.size());
	if (N < 2 || Route.Num() == 0)
	{
		return 0.f;
	}
	const int I = FMath::Clamp(Closest, 0, N - 2);
	const FVector2D P(static_cast<float>(Location.X) * CmToM, static_cast<float>(Location.Y) * CmToM);
	// The closest of the segments before and after the closest point.
	float Best = TNumericLimits<float>::Max();
	for (int S = FMath::Max(0, I - 1); S <= I; ++S)
	{
		const FVector2D A(Path[S].X, Path[S].Y);
		const FVector2D B(Path[S + 1].X, Path[S + 1].Y);
		Best = FMath::Min(Best, static_cast<float>(FVector2D::Distance(P, FMath::ClosestPointOnSegment2D(P, A, B))));
	}
	return Best;
}

bool FGTPLaneFollower::IsOnCircuit() const
{
	const AGTPTrafficNetwork* Net = Network.Get();
	return Net && Route.Num() > 0 && Net->GetFlightCircuit().IsKnown(Route[0]);
}

float FGTPLaneFollower::GetCircuitKmh() const
{
	const AGTPTrafficNetwork* Net = Network.Get();
	return Net && Route.Num() > 0 ? Net->GetFlightCircuit().KmhOf(Route[0]) : 0.f;
}

bool FGTPLaneFollower::HasCircuitEndPlace() const
{
	const AGTPTrafficNetwork* Net = Network.Get();
	return Net && Net->GetFlightCircuit().HasEndPlace();
}

bool FGTPLaneFollower::IsOnCircuitEndPlace() const
{
	const AGTPTrafficNetwork* Net = Network.Get();
	return Net && Route.Num() > 0 && Net->GetFlightCircuit().IsEndPlace(Route[0]);
}

float FGTPLaneFollower::GetCarriagewayM() const
{
	const AGTPTrafficNetwork* Net = Network.Get();
	return Net && Route.Num() > 0 && Net->GetLanes().IsValidIndex(Route[0]) ? Net->GetLanes()[Route[0]].CarriagewayM : 0.f;
}

int32 FGTPLaneFollower::LaneNear(const FVector& Point, float& OutMetres) const
{
	OutMetres = TNumericLimits<float>::Max();
	const AGTPTrafficNetwork* Net = Network.Get();
	if (!Net)
	{
		return INDEX_NONE;
	}
	int32 Best = INDEX_NONE;
	for (int32 Index = 0; Index < FMath::Min(Route.Num(), 2); ++Index)
	{
		if (!Net->GetLanes().IsValidIndex(Route[Index]))
		{
			continue;
		}
		const float Metres = FollowerDistanceToLane(Net->GetLanes()[Route[Index]].Points, Point, 0) * CmToM;
		if (Metres < OutMetres)
		{
			OutMetres = Metres;
			Best = Route[Index];
		}
	}
	return Best;
}

float FGTPLaneFollower::OpenRoadMetres(const TArray<FGTPTrafficLane>& Lanes, int32 Lane)
{
	if (!Lanes.IsValidIndex(Lane))
	{
		return 0.f;
	}
	return GTPDrive::OpenRoad(Lane, 6, OpenRoadWanted,
		[&Lanes](int Index) -> const TArray<int32>& { return Lanes[Index].Next; },
		[&Lanes](int Index) { return Lanes[Index].LengthCm() * CmToM; });
}

TArray<int32> FGTPLaneFollower::WayToGoal(const TArray<FGTPTrafficLane>& Lanes) const
{
	TArray<int32> Way;
	if (Route.Num() == 0 || !Lanes.IsValidIndex(Route.Last()))
	{
		return Way;
	}
	const int32 Start = Route.Last();
	const int32 Avoid = AvoidLane;
	const FVector Target = Goal;
	const auto GoalMetres = [&Lanes, &Target, Avoid](int Lane) -> float
	{
		return Lane == Avoid ? 1.e9f : FollowerDistanceToLane(Lanes[Lane].Points, Target, 0) * CmToM;
	};
	// On the car's own lane only what is still ahead of it counts: a goal behind it there is reached round the block.
	const float StartGoal = Route.Num() == 1
		? FollowerDistanceToLane(Lanes[Start].Points, Target, FMath::Clamp(Closest - 1, 0, FMath::Max(Lanes[Start].Points.Num() - 2, 0))) * CmToM
		: GoalMetres(Start);
	const std::vector<int> Found = GTPLaneRoute::Toward(Start, Lanes.Num(), StartGoal,
		[&Lanes](int Lane) -> const TArray<int32>& { return Lanes[Lane].Next; },
		[&Lanes](int Lane) -> float { return Lanes[Lane].LengthCm() * CmToM; }, GoalMetres);
	for (const int Lane : Found)
	{
		Way.Add(Lane);
	}
	return Way;
}

int32 FGTPLaneFollower::PickOpenToward(const TArray<FGTPTrafficLane>& Lanes, int32 Lane) const
{
	int32 Best = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (const int32 Candidate : Lanes[Lane].Next)
	{
		if (!Lanes.IsValidIndex(Candidate) || Lanes[Candidate].Points.Num() == 0 || Candidate == AvoidLane || OpenRoadMetres(Lanes, Candidate) < 80.f)
		{
			continue;
		}
		const double Distance = FVector::DistSquared2D(Lanes[Candidate].Points.Last(), Goal);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Candidate;
		}
	}
	return Best;
}

void FGTPLaneFollower::Plan()
{
	AGTPTrafficNetwork* Net = Network.Get();
	if (!Net || Route.Num() == 0)
	{
		return;
	}
	const TArray<FGTPTrafficLane>& Lanes = Net->GetLanes();
	float AheadM = 0.f;
	for (int32 Index = 1; Index < Route.Num(); ++Index)
	{
		AheadM += Lanes[Route[Index]].LengthCm() * CmToM;
	}
	const float WantedAheadM = FMath::Max(RouteAheadM, AheadMeters);
	// A chase: the shortest way to the goal first, then, past its end, exits with open road behind them.
	const bool bWay = bHasGoal && bRouteToGoal;
	const TArray<int32> Way = bWay && AheadM < WantedAheadM ? WayToGoal(Lanes) : TArray<int32>();
	int32 WayIndex = 0;
	while (AheadM < WantedAheadM)
	{
		int32 Next = INDEX_NONE;
		if (WayIndex < Way.Num())
		{
			Next = Way[WayIndex++];
		}
		else if (bWay)
		{
			Next = PickOpenToward(Lanes, Route.Last());
		}
		// A flight on the circuit of the map: the next lane of the lap, or off the lap the exit that leads to it.
		bool bByCircuit = false;
		if (Next == INDEX_NONE && bFollowCircuit && !bHasGoal)
		{
			const int32 CircuitNext = Net->GetFlightCircuit().Next(Route.Last());
			if (Lanes.IsValidIndex(CircuitNext) && Lanes[Route.Last()].Next.Contains(CircuitNext))
			{
				Next = CircuitNext;
				bByCircuit = true;
			}
		}
		if (Next == INDEX_NONE)
		{
			Next = bHasGoal ? Net->PickNextToward(Route.Last(), Goal) : Net->PickNext(Route.Last(), GTPDeterminism::IsOn() ? &OwnRandom : nullptr);
		}
		if (Next != INDEX_NONE && bPreferStraight && !bHasGoal && !bByCircuit)
		{
			// Never an exit a fixed obstacle blocks; an exit that never ends first (a dead end stops the flight and the
			// script puts the car elsewhere); then the best way on within 500 m, its metres weighted by width and halved on
			// lanes driven lately (gate 75: the flight lost 17 s in two narrow streets, and went round the block of the rue
			// Gentil whose way in a fixed obstacle stops); then the exit whose first segment keeps the heading of the end
			// of this lane the best (GTPRoadWidth::ExitScore).
			const TArray<FVector>& From = Lanes[Route.Last()].Points;
			const FVector Heading = From.Num() >= 2 ? (From.Last() - From[From.Num() - 2]).GetSafeNormal2D() : FVector::ZeroVector;
			const auto NextOf = [&Lanes](int Index) -> const TArray<int32>& { return Lanes[Index].Next; };
			const auto LengthOf = [&Lanes](int Index) { return Lanes[Index].LengthCm() * CmToM; };
			const auto WeightOf = [this, &Lanes](int Index)
			{
				if (!Lanes.IsValidIndex(Index) || Lanes[Index].bBlocked)
				{
					return 0.f;
				}
				return GTPRoadWidth::MetreWeight(Lanes[Index].CarriagewayM) * (IsRecent(Index) ? 0.5f : 1.f);
			};
			float BestScore = -1.e10f;
			for (const int32 Candidate : Lanes[Route.Last()].Next)
			{
				if (!Lanes.IsValidIndex(Candidate))
				{
					continue;
				}
				const TArray<FVector>& To = Lanes[Candidate].Points;
				if (To.Num() >= 2)
				{
					const float Dot = static_cast<float>(FVector::DotProduct(Heading, (To[1] - To[0]).GetSafeNormal2D()));
					const FGTPTrafficLane& Exit = Lanes[Candidate];
					int NodesLeft = GTPRoadWidth::LookNodes;
					const float Quality = GTPRoadWidth::WayQuality(Candidate, GTPRoadWidth::LookDepth, GTPRoadWidth::LookMetres, NextOf, LengthOf, WeightOf, NodesLeft);
					const float Score = GTPRoadWidth::ExitScore(Exit.bBlocked, Exit.bNeverEnds, Quality, Dot);
					if (Score > BestScore)
					{
						BestScore = Score;
						Next = Candidate;
					}
				}
			}
		}
		if (Next == INDEX_NONE)
		{
			break;
		}
		Route.Add(Next);
		AheadM += Lanes[Next].LengthCm() * CmToM;
	}
	// The end of the lane left last stays at the head of the path (GTPDrive::KeptBehind).
	Path = LaneBehind;
	CurrentLaneLength = 0;
	for (int32 RouteIndex = 0; RouteIndex < Route.Num(); ++RouteIndex)
	{
		const int32 Lane = Route[RouteIndex];
		// A lane of the flight circuit is driven on its line (GTPFlightCircuit.h), the others on their own points.
		const std::vector<GTPFlightCircuit::FLinePoint>* DrivenLine = bFollowCircuit || bDriveCircuitLines ? Net->GetFlightCircuit().LineOf(Lane) : nullptr;
		if (DrivenLine)
		{
			for (const GTPFlightCircuit::FLinePoint& LinePoint : *DrivenLine)
			{
				Path.push_back({LinePoint.X, LinePoint.Y});
			}
		}
		else
		{
			for (const FVector& Point : Lanes[Lane].Points)
			{
				Path.push_back({static_cast<float>(Point.X) * CmToM, static_cast<float>(Point.Y) * CmToM});
			}
		}
		if (RouteIndex == 0)
		{
			CurrentLaneLength = static_cast<int32>(Path.size());
		}
	}
}

bool FGTPLaneFollower::Step(const FVector& Location, float YawDegrees, float SpeedKmh, float MaxSpeedKmh, float ObstacleMetres, FGTPDriveCommand& Out)
{
	AGTPTrafficNetwork* Net = Network.Get();
	if (!Net || Route.Num() == 0)
	{
		return false;
	}
	const float Speed = SpeedKmh / 3.6f;
	const float LaneSpeed = (bIgnoreLaneLimit ? MaxSpeedKmh : FMath::Min(Net->GetLanes()[Route[0]].SpeedKmh, MaxSpeedKmh)) / 3.6f;
	if (bIgnoreLaneLimit)
	{
		// A chase: enough lanes known ahead to brake from the speed asked, or the end of the known path
		// caps the speed (50 m and the calm braking: 67 km/h for a police car asked 105).
		const float Needed = GTPDrive::PathNeeded(LaneSpeed, Settings);
		if (Needed > FMath::Max(RouteAheadM, AheadMeters))
		{
			AheadMeters = Needed;
			Plan();
		}
	}
	if (bRouteToGoal && bHasGoal && Route.Num() > 1)
	{
		const UWorld* World = Net->GetWorld();
		const double Now = World ? World->GetTimeSeconds() : 0.0;
		if (Now >= NextReplanTime)
		{
			// The goal has moved: the way is looked for again. The lanes the car can no longer avoid (its braking
			// distance and 20 m) are kept, so a bend it is braking for does not move under it.
			NextReplanTime = Now + FollowerReplanSeconds;
			const TArray<FGTPTrafficLane>& Lanes = Net->GetLanes();
			const float Keep = Speed * Speed / (2.f * FMath::Max(Settings.ComfortDecel, 0.1f)) + 20.f;
			float Ahead = 0.f;
			const int32 LastOfLane = FMath::Min(CurrentLaneLength, static_cast<int32>(Path.size())) - 1;
			for (int32 Index = FMath::Max(Closest, 0); Index < LastOfLane; ++Index)
			{
				Ahead += std::hypot(Path[Index + 1].X - Path[Index].X, Path[Index + 1].Y - Path[Index].Y);
			}
			int32 KeepLanes = 1;
			while (KeepLanes < Route.Num() && Ahead < Keep)
			{
				Ahead += Lanes[Route[KeepLanes]].LengthCm() * CmToM;
				++KeepLanes;
			}
			if (KeepLanes < Route.Num())
			{
				Route.SetNum(KeepLanes);
				Plan();
			}
		}
	}
	Out = GTPDrive::Decide(Path, static_cast<float>(Location.X) * CmToM, static_cast<float>(Location.Y) * CmToM,
		FMath::DegreesToRadians(YawDegrees), Speed, LaneSpeed, ObstacleMetres, Settings, Closest);
	if (Closest >= CurrentLaneLength && Route.Num() > 1)
	{
		// The lane goes, its last metres stay: where lanes meet in a corner the car is in the middle of it, and the
		// bend is measured with the path before it (gate 83: 24 to 57 km/h wanted in the corner of the rue Childebert).
		// -LaneKeptBehind=0: the lane goes whole, as until gate 83.
		static const bool bKeepLaneEnd = []
		{
			int32 KeepSwitch = 1;
			FParse::Value(FCommandLine::Get(), TEXT("LaneKeptBehind="), KeepSwitch);
			return KeepSwitch != 0;
		}();
		std::vector<FGTPDrivePoint> Kept = bKeepLaneEnd ? GTPDrive::KeptBehind(Path, CurrentLaneLength) : std::vector<FGTPDrivePoint>();
		Closest += static_cast<int>(Kept.size()) - CurrentLaneLength;
		LaneBehind = MoveTemp(Kept);
		// The lanes left lately count for half in a flight's way on (Plan): it does not circle one block.
		RecentLanes.Add(Route[0]);
		if (RecentLanes.Num() > RecentCount)
		{
			RecentLanes.RemoveAt(0);
		}
		Route.RemoveAt(0);
		Plan();
	}
	// Route over: standing at the end of a dead-end lane.
	const bool bDeadEnd = Route.Num() == 1 && Net->GetLanes()[Route[0]].Next.Num() == 0 && Closest >= CurrentLaneLength - 2;
	if (bDeadEnd && Speed < 0.5f)
	{
		Route.Reset();
		return false;
	}
	return true;
}

// --- AI driver --------------------------------------------------------------

UGTPAIDriverComponent::UGTPAIDriverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Decide after physics moved the car this frame.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UGTPAIDriverComponent::Start(AGTPTrafficNetwork* InNetwork, int32 Lane, int32 StartPoint)
{
	Network = InNetwork;
	if (GTPDeterminism::IsOn())
	{
		// Seeded from the car's name (spawn order), not from a stream shared by all cars.
		DriverRandom.Initialize(static_cast<int32>(FCrc::StrCrc32(*GetOwner()->GetName()) & 0x7fffffff));
	}
	ReleaseJunctions();
	Route.Reset();
	Route.Add(Lane);
	Closest = StartPoint;
	StoppedTime = 0.f;
	PushTime = 0.f;
	BackOffTime = 0.f;
	WedgeCount = 0;
	GhostSpeed = 0.f;
	GhostAlong = 0.f;
	bAtDeadEnd = false;
	// A car put back on a lane starts calm: pooled in panic, a Kangoo of gate 67 came back three times still
	// panicking (1.6 times its speed, red lights ignored) and ended nose to nose with a parked car.
	React = GTPTrafficReact::FBrain();
	BypassBlocker = nullptr;
	WaitingFor = nullptr;
	// Each driver has a temperament: speed, patience, what they do under threat.
	const float Roll = GTPDeterminism::IsOn() ? DriverRandom.FRand() : FMath::FRand();
	Temperament = GTPTrafficReact::PickTemperament(Roll);
	DriverSettings = GTPTrafficReact::ApplyTemperament(Network->ReactSettings, Temperament, TemperamentSpeed);
	PlanRoute();
	SetComponentTickEnabled(true);
}

double UGTPAIDriverComponent::TickSeconds = 0.0;
int32 UGTPAIDriverComponent::TickCount = 0;
int32 UGTPAIDriverComponent::GhostPhysicsCaught = 0;

bool UGTPAIDriverComponent::PlaceGhost(int32 Lane, int32 Point, const FVector& Location, float YawDegrees, float SpeedMs)
{
	AGTPVehicle* Vehicle = Cast<AGTPVehicle>(GetOwner());
	if (!Vehicle || !Network)
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s cannot be placed as a ghost: %s"), *GetNameSafe(Vehicle), Vehicle ? TEXT("its driver has no network") : TEXT("no car"));
		return false;
	}
	SetGhost(true);
	// Already a ghost (put away in the pool, a far puppet): SetGhost kept the ride height measured back then.
	KeepRideHeightOnWheels(Vehicle, TEXT("placed"));
	Vehicle->SetActorLocationAndRotation(Location + FVector(0.f, 0.f, GhostRideHeight), FRotator(0.f, YawDegrees, 0.f), false, nullptr,
		ETeleportType::TeleportPhysics);
	Vehicle->NotePlaced(TEXT("placed as ghost"));
	Start(Network, Lane, Point);
	GhostSpeed = FMath::Max(0.f, SpeedMs);
	GhostAlong = 0.f;
	GhostGroundZ = static_cast<float>(Location.Z);
	GhostGroundAge = 1.f;
	return true;
}

void UGTPAIDriverComponent::KeepRideHeightOnWheels(const AGTPVehicle* Vehicle, const TCHAR* When)
{
	// On its wheels, the origin stands a little under or over its spawn height (pivot and suspension).
	const float Spawn = ParkedClearance(Vehicle->GetClass());
	if (GhostRideHeight >= Spawn - 80.f && GhostRideHeight <= Spawn + 60.f)
	{
		return;
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s %s %.0f cm above the ground, off its wheels: ridden at %.0f cm"), *Vehicle->GetName(), When,
		GhostRideHeight, Spawn);
	GhostRideHeight = Spawn;
}

void UGTPAIDriverComponent::HoldGhostKinematic(AGTPVehicle* Vehicle, const TCHAR* Where)
{
	USkeletalMeshComponent* Body = Vehicle->GetMesh();
	UChaosWheeledVehicleMovementComponent* Movement = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovementComponent());
	const bool bSimulated = Body && Body->IsSimulatingPhysics();
	const bool bChaosOn = Movement && Movement->IsComponentTickEnabled();
	if (!bGhost || (!bSimulated && !bChaosOn))
	{
		return;
	}
	++GhostPhysicsCaught;
	// The first ones named, with the last placement: which path left a ghost simulated is not known yet.
	if (GhostPhysicsCaught <= 20)
	{
		const UWorld* World = GetWorld();
		const double Now = World ? World->GetTimeSeconds() : 0.0;
		const TCHAR* What = bSimulated ? (bChaosOn ? TEXT("its physics and Chaos vehicle") : TEXT("its physics")) : TEXT("its Chaos vehicle");
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s a ghost with %s on (%s, %.1f s after %s, %.0f km/h, up %.2f): off again, %d so far"),
			*Vehicle->GetName(), What, Where, Now - Vehicle->GetLastPlacedTime(), Vehicle->GetLastPlacedHow(),
			bSimulated ? Body->GetPhysicsLinearVelocity().Size() * 0.036 : 0.0, Vehicle->GetActorUpVector().Z, GhostPhysicsCaught);
	}
	if (bSimulated)
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		Body->SetSimulatePhysics(false);
		// Its physics went off now: the pool's cooldown counts from here (PutAway).
		GhostSince = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	if (bChaosOn)
	{
		Movement->SetComponentTickEnabled(false);
	}
}

void UGTPAIDriverComponent::SetDecisionInterval(float Seconds)
{
	Seconds = FMath::Max(0.f, Seconds);
	if (!FMath::IsNearlyEqual(PrimaryComponentTick.TickInterval, Seconds))
	{
		SetComponentTickInterval(Seconds);
	}
}

bool UGTPAIDriverComponent::FindGround(const AGTPVehicle* Vehicle, const FVector& Location, float& OutZ, float ReachCm) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPTrafficGhostGround), false, Vehicle);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0.f, 0.f, 300.f), Location - FVector(0.f, 0.f, ReachCm), Objects, Params))
	{
		return false;
	}
	OutZ = static_cast<float>(Hit.ImpactPoint.Z);
	return true;
}

void UGTPAIDriverComponent::SetGhost(bool bInGhost)
{
	AGTPVehicle* Vehicle = Cast<AGTPVehicle>(GetOwner());
	if (!Vehicle)
	{
		return;
	}
	if (bGhost == bInGhost)
	{
		// Placed again (PlaceGhost), made a puppet or put away while a ghost: nothing to switch, physics checked.
		if (bGhost)
		{
			HoldGhostKinematic(Vehicle, TEXT("already a ghost"));
		}
		return;
	}
	USkeletalMeshComponent* Body = Vehicle->GetMesh();
	UChaosWheeledVehicleMovementComponent* Movement = Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovementComponent());
	if (bInGhost)
	{
		GhostSpeed = FMath::Max(0.f, Vehicle->GetSpeedKmh() / 3.6f);
		// Where the car is along its current path segment, so it does not jump back.
		const FVector Location = Vehicle->GetActorLocation();
		GhostAlong = 0.f;
		if (Closest >= 0 && Closest + 1 < static_cast<int>(Path.size()))
		{
			const FGTPDrivePoint& A = Path[Closest];
			const FGTPDrivePoint& B = Path[Closest + 1];
			const FVector2D Segment(B.X - A.X, B.Y - A.Y);
			const float Length = static_cast<float>(Segment.Size());
			if (Length > 1e-3f)
			{
				const FVector2D From(static_cast<float>(Location.X) * CmToM - A.X, static_cast<float>(Location.Y) * CmToM - A.Y);
				GhostAlong = FMath::Clamp(static_cast<float>(FVector2D::DotProduct(From, Segment)) / Length, 0.f, Length);
			}
		}
		// Down to 1 km: a car thrown in the air finds the ground under it, and its ghost slides there, not at the
		// height it flew (the next ground traces only look 10 m down).
		float GroundZ = 0.f;
		if (FindGround(Vehicle, Location, GroundZ, 100000.f))
		{
			GhostRideHeight = static_cast<float>(Location.Z) - GroundZ;
			KeepRideHeightOnWheels(Vehicle, TEXT("became a ghost"));
			GhostGroundZ = GroundZ;
		}
		else
		{
			GhostRideHeight = 40.f;
			GhostGroundZ = static_cast<float>(Location.Z) - GhostRideHeight;
		}
		GhostGroundAge = 0.f;
		Vehicle->SetAIDriveInput(0.f, 0.f, 0.f, false);
		if (Movement)
		{
			Movement->SetComponentTickEnabled(false);
		}
		if (Body)
		{
			Body->SetSimulatePhysics(false);
		}
		GhostSince = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	}
	else
	{
		Vehicle->NotePlaced(TEXT("ghost wake"));
		if (Body)
		{
			Body->SetSimulatePhysics(true);
			Body->WakeAllRigidBodies();
			Body->SetPhysicsLinearVelocity(Vehicle->GetActorForwardVector().GetSafeNormal2D() * GhostSpeed * 100.f);
		}
		if (Movement)
		{
			Movement->SetComponentTickEnabled(true);
		}
	}
	bGhost = bInGhost;
}

void UGTPAIDriverComponent::ReleaseJunctions()
{
	if (Network)
	{
		Junctions.Clear(Network->GetJunctionTable(), static_cast<int>(GetOwner()->GetUniqueID()));
	}
}

void UGTPAIDriverComponent::PlanRoute()
{
	if (!Network || Route.Num() == 0)
	{
		return;
	}
	const TArray<FGTPTrafficLane>& Lanes = Network->GetLanes();
	float AheadM = 0.f;
	for (int32 Index = 1; Index < Route.Num(); ++Index)
	{
		AheadM += Lanes[Route[Index]].LengthCm() * CmToM;
	}
	while (AheadM < RouteAheadM)
	{
		const int32 Next = Network->PickNext(Route.Last(), GTPDeterminism::IsOn() ? &DriverRandom : nullptr);
		if (Next == INDEX_NONE)
		{
			break;
		}
		Route.Add(Next);
		AheadM += Lanes[Next].LengthCm() * CmToM;
	}
	Path.clear();
	for (const int32 Lane : Route)
	{
		for (const FVector& Point : Lanes[Lane].Points)
		{
			Path.push_back({static_cast<float>(Point.X) * CmToM, static_cast<float>(Point.Y) * CmToM});
		}
	}
	CurrentLaneLength = Lanes[Route[0]].Points.Num();
}

float UGTPAIDriverComponent::FindObstacleMetres(const AGTPVehicle* Vehicle) const
{
	return ProbeAhead(Vehicle, ProbeLength, ProbeRadius);
}

float UGTPAIDriverComponent::ProbeAhead(const AGTPVehicle* Vehicle, float LengthCm, float RadiusCm, AActor** OutHit)
{
	// A sweep straight ahead at bumper height sees cars and people (a seated driver has collision off).
	const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
	const FVector Start = Vehicle->GetActorLocation() + FVector(0.f, 0.f, 80.f) + Forward * 230.f;
	const FVector End = Start + Forward * LengthCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPTrafficProbe), false, Vehicle);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FHitResult Hit;
	if (Vehicle->GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(RadiusCm), Params))
	{
		if (OutHit)
		{
			*OutHit = Hit.GetActor();
		}
		return static_cast<float>(Hit.Distance) * CmToM;
	}
	if (OutHit)
	{
		*OutHit = nullptr;
	}
	return -1.f;
}

// --- Reactions (GTPTrafficReactLogic.h) -----------------------------------------------

void UGTPAIDriverComponent::InjectReaction(bool bAimed, bool bShot, bool bRammed)
{
	InjectAimLeft = bAimed ? 1.5f : InjectAimLeft;
	bInjectShot |= bShot;
	bInjectRammed |= bRammed;
}

bool UGTPAIDriverComponent::IsStillObstacle(const AActor* Actor) const
{
	if (!Actor || Actor->GetVelocity().Size2D() > 50.f)
	{
		return false;
	}
	// A traffic car standing in a queue (red light, junction) will move by itself:
	// only one that is waiting for something that does not move counts as still.
	if (const AGTPVehicle* Car = Cast<AGTPVehicle>(Actor))
	{
		if (Car->HasAIDriver() && !Car->IsWrecked())
		{
			if (const UGTPAIDriverComponent* Other = Car->FindComponentByClass<UGTPAIDriverComponent>())
			{
				const GTPTrafficReact::EState State = Other->GetReactState();
				return State == GTPTrafficReact::EState::Waiting || State == GTPTrafficReact::EState::Shaken
					|| State == GTPTrafficReact::EState::Flee;
			}
			// Police, mission drivers: they move on their own.
			return false;
		}
	}
	// The player's car, a wreck, an abandoned car, a person standing in the road.
	return true;
}

bool UGTPAIDriverComponent::IsSideClear(const AGTPVehicle* Vehicle, float AheadM) const
{
	// Driving on the right: going round on the left, the width of a lane, from behind
	// the car to well past the obstacle. Anything there (oncoming car, person, post, wall)
	// says no. High enough not to touch the kerb.
	const FVector Forward = Vehicle->GetActorForwardVector().GetSafeNormal2D();
	const FVector Left = -Vehicle->GetActorRightVector().GetSafeNormal2D();
	const FVector Base = Vehicle->GetActorLocation() + Left * (Network ? Network->ReactSettings.BypassOffset * 100.f : 290.f) + FVector(0.f, 0.f, 120.f);
	const FVector Start = Base - Forward * 500.f;
	const FVector End = Base + Forward * (FMath::Max(AheadM, 5.f) * 100.f + 2500.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPTrafficBypass), false, Vehicle);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	return !Vehicle->GetWorld()->SweepSingleByObjectType(Hit, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(95.f), Params);
}

GTPTrafficReact::FOutput UGTPAIDriverComponent::UpdateReactions(AGTPVehicle* Vehicle, float DeltaTime, float Speed, float ProbeM, AActor* ProbeHit)
{
	const GTPTrafficReact::FSettings& Tuning = DriverSettings;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Location = Vehicle->GetActorLocation();

	GTPTrafficReact::FInput In;
	In.Dt = DeltaTime;
	In.Speed = FMath::Abs(Speed);
	In.ObstacleM = ProbeM;
	In.bObstacleStill = ProbeM >= 0.f && IsStillObstacle(ProbeHit);
	In.bObstacleIsPlayer = ProbeHit && Player
		&& (ProbeHit == Player || (Cast<AGTPVehicle>(ProbeHit) && Cast<APlayerController>(Cast<AGTPVehicle>(ProbeHit)->GetController())));
	// The side sweep only when a bypass is about to be decided.
	if (React.State == GTPTrafficReact::EState::Waiting && React.BlockedTime >= Tuning.BypassAfter)
	{
		In.bSideClear = IsSideClear(Vehicle, ProbeM);
	}
	if (const UGTPThreatSubsystem* Threats = GetWorld()->GetSubsystem<UGTPThreatSubsystem>())
	{
		if (Threats->HasThreat() && Threats->IsArmed() && Threats->GetSource() == Player)
		{
			const FVector ToCar = Location - Threats->GetOrigin();
			In.bAimedAt = ToCar.Size() < Network->ReactAimRange
				&& FVector::DotProduct(ToCar.GetSafeNormal(), Threats->GetAim().GetSafeNormal()) > FMath::Cos(FMath::DegreesToRadians(Network->ReactAimHalfAngle));
		}
		if (SeenShotSerial < 0)
		{
			SeenShotSerial = Threats->GetShotSerial();
		}
		if (Threats->GetShotSerial() != SeenShotSerial)
		{
			SeenShotSerial = Threats->GetShotSerial();
			In.bShotNear = FVector::Dist(Threats->GetLastShotFrom(), Location) < Network->ReactShotRange;
		}
	}
	else
	{
		// Threat system off (the default): the player's weapon, read by the network.
		FVector AimOrigin, AimDirection;
		if (Network->GetPlayerAim(AimOrigin, AimDirection))
		{
			const FVector ToCar = Location - AimOrigin;
			In.bAimedAt = ToCar.Size() < Network->ReactAimRange
				&& FVector::DotProduct(ToCar.GetSafeNormal(), AimDirection) > FMath::Cos(FMath::DegreesToRadians(Network->ReactAimHalfAngle));
		}
		if (SeenShotSerial < 0)
		{
			SeenShotSerial = Network->GetPlayerShotSerial();
		}
		if (Network->GetPlayerShotSerial() != SeenShotSerial)
		{
			SeenShotSerial = Network->GetPlayerShotSerial();
			In.bShotNear = FVector::Dist(Network->GetPlayerShotFrom(), Location) < Network->ReactShotRange;
		}
	}
	In.bHitByPlayer = Vehicle->ConsumeHitByPlayer();
	In.bAimedAt |= InjectAimLeft > 0.f;
	InjectAimLeft -= DeltaTime;
	In.bShotNear |= bInjectShot;
	In.bHitByPlayer |= bInjectRammed;
	bInjectShot = bInjectRammed = false;

	const GTPTrafficReact::EState Before = React.State;
	const GTPTrafficReact::FOutput Out = GTPTrafficReact::Step(React, In, Tuning);
	WaitingFor = React.State == GTPTrafficReact::EState::Waiting ? ProbeHit : nullptr;
	if (Out.bChanged)
	{
		if (React.State == GTPTrafficReact::EState::Bypass)
		{
			BypassBlocker = ProbeHit;
		}
		const FString From(UTF8_TO_TCHAR(GTPTrafficReact::StateName(Before)));
		const FString To(UTF8_TO_TCHAR(GTPTrafficReact::StateName(React.State)));
		GTP_TRACE(LogGTPTraffic, TEXT("traffic react: %s (%s) %s -> %s (ahead %s %.1f m, %.0f km/h)"), *Vehicle->GetName(),
			UTF8_TO_TCHAR(GTPTrafficReact::TemperamentName(Temperament)), *From, *To, *GetNameSafe(ProbeHit), ProbeM, In.Speed * 3.6f);
	}
	if (Out.bBailOut)
	{
		const UGTPThreatSubsystem* Threats = GetWorld()->GetSubsystem<UGTPThreatSubsystem>();
		const FVector From = Threats && Threats->HasThreat() ? Threats->GetOrigin() : (Player ? Player->GetActorLocation() : Location);
		Vehicle->PanicExit(From);
	}
	// The horn only where someone can hear it (a voice per car is not free).
	bool bHorn = Out.bHorn;
	if (bHorn)
	{
		const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
		const FVector View = Viewer && Viewer->PlayerCameraManager ? Viewer->PlayerCameraManager->GetCameraLocation() : Location;
		bHorn = FVector::Dist(View, Location) < Network->ReactHornDistance;
	}
	if (bHorn != bHornOn)
	{
		bHornOn = bHorn;
		UGTPHornComponent::SetHorn(Vehicle, bHorn);
	}
	return Out;
}

void UGTPAIDriverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const double TickStart = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		TickSeconds += FPlatformTime::Seconds() - TickStart;
		++TickCount;
	};
	AGTPVehicle* Vehicle = Cast<AGTPVehicle>(GetOwner());
	if (!Vehicle || !Network || Route.Num() == 0)
	{
		return;
	}
	if (!Vehicle->HasAIDriver() || Vehicle->IsWrecked())
	{
		// Stolen or destroyed: this driver is done, and the car is a real car again.
		SetGhost(false);
		ReleaseJunctions();
		SetComponentTickEnabled(false);
		return;
	}

	const FVector Location = Vehicle->GetActorLocation();
	const float Yaw = FMath::DegreesToRadians(static_cast<float>(Vehicle->GetActorRotation().Yaw));
	const float Speed = bGhost ? GhostSpeed : Vehicle->GetSpeedKmh() / 3.6f;
	const FGTPTrafficLane& Lane = Network->GetLanes()[Route[0]];
	const float LaneSpeed = Lane.SpeedKmh / 3.6f;
	const float X = static_cast<float>(Location.X) * CmToM;
	const float Y = static_cast<float>(Location.Y) * CmToM;
	StoppedTime = FMath::Abs(Speed) < 0.3f ? StoppedTime + DeltaTime : 0.f;

	// Nearest of: something in front (straight probe, then along the lanes for
	// queues on bends), a red light, the stop line of a junction another car is crossing.
	// A ghost skips the sweep (the queue along the lanes still sees the cars ahead).
	AActor* ProbeHit = nullptr;
	const float Probe = bGhost ? -1.f : ProbeAhead(Vehicle, ProbeLength, ProbeRadius, &ProbeHit);
	// Reactions to the street (GTPTrafficReactLogic.h): a ghost is too far for any of them.
	GTPTrafficReact::FOutput Reaction;
	if (!bGhost && Network->bTrafficReact)
	{
		Reaction = UpdateReactions(Vehicle, DeltaTime, Speed, Probe, ProbeHit);
	}
	else if (bHornOn)
	{
		bHornOn = false;
		UGTPHornComponent::SetHorn(Vehicle, false);
	}
	// Going round: the car being passed is no obstacle any more, nor the queue behind it.
	const bool bPassing = Reaction.bIgnoreBlocker && ProbeHit && ProbeHit == BypassBlocker.Get();
	float Obstacle = bPassing ? -1.f : Probe;
	// What holds the car back, the nearest stop winning as below (the "traffic cars" line of the network).
	Limit = GTPTrafficStop::ELimit::None;
	if (Obstacle >= 0.f)
	{
		Limit = Cast<AGTPVehicle>(ProbeHit) ? GTPTrafficStop::ELimit::CarAhead
			: (Cast<APawn>(ProbeHit) ? GTPTrafficStop::ELimit::PersonAhead : GTPTrafficStop::ELimit::OtherAhead);
	}
	const float Queue = Reaction.bIgnoreBlocker ? -1.f : Network->GapAlongLanes(this, 40.f);
	if (Queue >= 0.f && (Obstacle < 0.f || Queue < Obstacle))
	{
		Obstacle = Queue;
		Limit = GTPTrafficStop::ELimit::Queue;
	}
	const FVector& LaneEnd = Lane.Points.Last();
	// Signed distance to the end of the lane along its last segment: negative once past
	// the line, so a car already in the junction when the light turns red carries on.
	const FVector& LaneBeforeEnd = Lane.Points.Num() >= 2 ? Lane.Points[Lane.Points.Num() - 2] : LaneEnd;
	const FVector2D EndDir = FVector2D(LaneEnd - LaneBeforeEnd).GetSafeNormal();
	const float ToLaneEnd = EndDir.IsNearlyZero()
		? FMath::Sqrt(FMath::Square(static_cast<float>(LaneEnd.X) * CmToM - X) + FMath::Square(static_cast<float>(LaneEnd.Y) * CmToM - Y))
		: static_cast<float>(FVector2D::DotProduct(FVector2D(LaneEnd.X * CmToM - X, LaneEnd.Y * CmToM - Y), EndDir));
	// Panic: red lights and junction turns are ignored.
	const float LightStop = Reaction.bIgnoreSignals ? -1.f
		: Network->GetTrafficLights().StopDistance(Route[0], Network->GetLightTime(), ToLaneEnd, Speed, Settings.ComfortDecel);
	if (LightStop >= 0.f && (Obstacle < 0.f || LightStop < Obstacle))
	{
		Obstacle = LightStop;
		Limit = GTPTrafficStop::ELimit::RedLight;
	}
	// A car waiting at a red light claims no junction (the crossing traffic has green):
	// one claimed on green just before the light changed is given back.
	if (LightStop >= 0.f)
	{
		Junctions.Drop(Network->GetJunctionTable(), static_cast<int>(Vehicle->GetUniqueID()), Lane.ToJunction);
	}
	const float StopLine = Reaction.bIgnoreSignals ? -1.f : Junctions.Update(Network->GetJunctionTable(), static_cast<int>(Vehicle->GetUniqueID()), LightStop >= 0.f ? -1 : Lane.ToJunction,
		{static_cast<float>(LaneEnd.X) * CmToM, static_cast<float>(LaneEnd.Y) * CmToM}, X, Y, Speed, Settings.ComfortDecel, StoppedTime);
	if (StopLine >= 0.f && (Obstacle < 0.f || StopLine < Obstacle))
	{
		Obstacle = StopLine;
		Limit = GTPTrafficStop::ELimit::Junction;
	}
	if (bGhost)
	{
		HoldGhostKinematic(Vehicle, TEXT("sliding"));
		// Same speed limits as a driven car, then a slide along the path at that speed.
		int DecideClosest = Closest;
		const FGTPDriveCommand Command = GTPDrive::Decide(Path, X, Y, Yaw, Speed, LaneSpeed, Obstacle, Settings, DecideClosest);
		GhostSpeed = GTPGhost::NextSpeed(GhostSpeed, Command.TargetSpeed, 2.f, 6.f, DeltaTime);
		FGTPDrivePoint Position{X, Y};
		float NewYaw = Yaw;
		GTPGhost::Advance(Path, Closest, GhostAlong, GhostSpeed * DeltaTime, Position, NewYaw);
		FVector NewLocation(Position.X / CmToM, Position.Y / CmToM, Location.Z);
		// Ground height a few times a second: bridge decks and slopes.
		GhostGroundAge += DeltaTime;
		float GroundZ = GhostGroundZ;
		if (GhostGroundAge >= 0.25f && FindGround(Vehicle, NewLocation, GroundZ))
		{
			GhostGroundZ = GroundZ;
			GhostGroundAge = 0.f;
		}
		NewLocation.Z = GhostGroundZ + GhostRideHeight;
		Vehicle->SetActorLocationAndRotation(NewLocation, FRotator(0.f, FMath::RadiansToDegrees(NewYaw), 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	}
	else if (Reaction.bHold || Vehicle->IsAIStopRequested())
	{
		// Shaken, getting out, or the player walking up to take the car: brake and stay put.
		Limit = GTPTrafficStop::ELimit::Held;
		Vehicle->SetAIDriveInput(0.f, 1.f, 0.f, Speed < 0.5f);
	}
	else
	{
		// Going round: the car steers as if it stood LateralOffset to the right of where it
		// is, so it drives that far to the left of its lane.
		const FVector Right = Vehicle->GetActorRightVector().GetSafeNormal2D();
		const float SteerX = X + Reaction.LateralOffset * static_cast<float>(Right.X);
		const float SteerY = Y + Reaction.LateralOffset * static_cast<float>(Right.Y);
		// The car ahead's speed along our heading, when the probe's hit is what limits us (IDM).
		float LeaderSpeed = -1.f;
		if (ProbeHit && !bPassing && Obstacle >= 0.f && Obstacle == Probe && Cast<AGTPVehicle>(ProbeHit))
		{
			LeaderSpeed = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(ProbeHit->GetVelocity(), Vehicle->GetActorForwardVector())) * CmToM);
		}
		const FGTPDriveCommand Command = GTPDrive::Decide(Path, SteerX, SteerY, Yaw, Speed, LaneSpeed * Reaction.SpeedScale * TemperamentSpeed, Obstacle, Settings,
			Closest, LeaderSpeed);
		// Wedged against something the probe does not see (kerb, bollard, a car's
		// corner): full throttle for 3 s without moving, then back off 1.5 s
		// steering the other way, and try again.
		if (BackOffTime > 0.f)
		{
			BackOffTime -= DeltaTime;
			Limit = GTPTrafficStop::ELimit::Wedged;
			Vehicle->SetAIDriveInput(0.f, 1.f, -Command.Steering, false, true);
		}
		else
		{
			PushTime = Command.Throttle > 0.5f && FMath::Abs(Speed) < 0.3f ? PushTime + DeltaTime : 0.f;
			// Free again once it drives at 10 km/h.
			WedgeCount = Speed > 3.f ? 0 : WedgeCount;
			if (PushTime > 1.f)
			{
				// Pushing for a second without moving: wedged on something the probe does not see.
				Limit = GTPTrafficStop::ELimit::Wedged;
			}
			if (PushTime > 3.f)
			{
				PushTime = 0.f;
				BackOffTime = 1.5f;
				++WedgeCount;
				GTP_TRACE(LogGTPTraffic, TEXT("%s: wedged, backing off (%d)"), *Vehicle->GetName(), WedgeCount);
			}
			Vehicle->SetAIDriveInput(Command.Throttle, Command.Brake, Command.Steering, Command.Brake >= 1.f && Speed < 0.5f);
		}
	}

	if (Closest >= CurrentLaneLength && Route.Num() > 1)
	{
		Closest -= CurrentLaneLength;
		Route.RemoveAt(0);
		PlanRoute();
	}
	bAtDeadEnd = Route.Num() == 1 && Network->GetLanes()[Route[0]].Next.Num() == 0 && Closest >= CurrentLaneLength - 2 && Speed < 0.5f;
}

// --- Network ----------------------------------------------------------------

AGTPTrafficNetwork::AGTPTrafficNetwork()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	Random.Initialize(0x47545021);
}

FString AGTPTrafficNetwork::LanesFileFor(const FString& MapName)
{
	return FPaths::ProjectContentDir() / TEXT("City/Traffic") / (MapName + TEXT(".lanes.csv"));
}

FString AGTPTrafficNetwork::CircuitFileFor(const FString& MapName)
{
	return FPaths::ProjectContentDir() / TEXT("Test/Circuits") / (MapName + TEXT(".circuit.csv"));
}

void AGTPTrafficNetwork::LoadFlightCircuit()
{
	FlightCircuit = GTPFlightCircuit::FCircuit();
	int32 CircuitSwitch = 1;
	FParse::Value(FCommandLine::Get(), TEXT("FlightCircuit="), CircuitSwitch);
	if (CircuitSwitch == 0)
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: flight circuit off (-FlightCircuit=0): the flights are free on the lanes"));
		return;
	}
	const FString CircuitFile = CircuitFileFor(UGameplayStatics::GetCurrentLevelName(this, true));
	FString CircuitText;
	if (!FFileHelper::LoadFileToString(CircuitText, *CircuitFile))
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: no flight circuit for this map (%s): the flights are free on the lanes"), *CircuitFile);
		return;
	}
	const std::vector<GTPFlightCircuit::FRow> CircuitRows = GTPFlightCircuit::ParseRows(std::string(TCHAR_TO_UTF8(*CircuitText)));
	// A row names a lane by its two ends (m): the numbers of the lanes file move when it is made again.
	std::vector<GTPFlightCircuit::FEnds> LaneEnds;
	LaneEnds.reserve(static_cast<size_t>(Lanes.Num()));
	for (const FGTPTrafficLane& Lane : Lanes)
	{
		GTPFlightCircuit::FEnds& Ends = LaneEnds.emplace_back();
		const bool bHasPoints = Lane.Points.Num() > 0;
		Ends.StartX = bHasPoints ? static_cast<float>(Lane.Points[0].X) * CmToM : 1.e9f;
		Ends.StartY = bHasPoints ? static_cast<float>(Lane.Points[0].Y) * CmToM : 1.e9f;
		Ends.EndX = bHasPoints ? static_cast<float>(Lane.Points.Last().X) * CmToM : 1.e9f;
		Ends.EndY = bHasPoints ? static_cast<float>(Lane.Points.Last().Y) * CmToM : 1.e9f;
	}
	// The rows of the approach (the lanes driven to the lap from where the flight starts) and those of the lap.
	std::vector<GTPFlightCircuit::FRow> LapRows;
	std::vector<GTPFlightCircuit::FRow> ApproachRows;
	for (const GTPFlightCircuit::FRow& CircuitRow : CircuitRows)
	{
		(CircuitRow.bApproach ? ApproachRows : LapRows).push_back(CircuitRow);
	}
	std::vector<int> Lap;
	std::vector<int> ApproachLanes;
	int Renumbered = 0;
	int ApproachRenumbered = 0;
	std::string Why;
	const TArray<FGTPTrafficLane>& AllLanes = Lanes;
	const auto ExitsOf = [&AllLanes](int Index) -> const TArray<int32>& { return AllLanes[Index].Next; };
	const bool bFits = GTPFlightCircuit::Resolve(LapRows, LaneEnds, Lap, Renumbered, Why)
		&& GTPFlightCircuit::Build(AllLanes.Num(), Lap, ExitsOf,
			[&AllLanes](int Index) { return AllLanes[Index].LengthCm() * CmToM; }, FlightCircuit, Why);
	if (!bFits)
	{
		FlightCircuit = GTPFlightCircuit::FCircuit();
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: flight circuit NOT USED: the %d row(s) of %s do not fit the %d lanes read: %s. The flights are free on the lanes"),
			static_cast<int32>(CircuitRows.size()), *CircuitFile, Lanes.Num(), UTF8_TO_TCHAR(Why.c_str()));
		return;
	}
	float OnMainM = 0.f;
	float OnNarrowM = 0.f;
	FString LapText;
	for (const int LapLane : FlightCircuit.Lanes)
	{
		const GTPRoadWidth::EClass WidthClass = GTPRoadWidth::Classify(Lanes[LapLane].CarriagewayM);
		const float Metres = Lanes[LapLane].LengthCm() * CmToM;
		OnMainM += WidthClass == GTPRoadWidth::EClass::Main ? Metres : 0.f;
		OnNarrowM += WidthClass == GTPRoadWidth::EClass::Narrow ? Metres : 0.f;
		LapText += FString::Printf(TEXT(" %d"), LapLane);
	}
	const float LapMetres = FMath::Max(FlightCircuit.LapMetres, 1.f);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: flight circuit of %d lanes, %.0f m a lap, %.0f %% on main axes, %.0f %% in narrow streets, reached from %d of %d lanes, %d row(s) found under another number; lap:%s"),
		static_cast<int32>(FlightCircuit.Lanes.size()), FlightCircuit.LapMetres, OnMainM * 100.f / LapMetres, OnNarrowM * 100.f / LapMetres,
		FlightCircuit.Reaching(), Lanes.Num(), Renumbered, *LapText);
	// The line driven on the lap. Without it the lanes' own points are driven: the lap holds, slower.
	if (GTPFlightCircuit::SetLines(FlightCircuit, LapRows, LaneEnds, Why))
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: flight circuit driven on its line of %d points"), FlightCircuit.LinePoints());
	}
	else
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: flight circuit WITHOUT its driving line, the lanes' own points are driven: %s"), UTF8_TO_TCHAR(Why.c_str()));
	}
	// The speeds the rows ask, lane by lane (the approach's are added below when it holds), and the lanes the filmed
	// flight is to end on.
	GTPFlightCircuit::SetSpeeds(FlightCircuit, LapRows, {});
	GTPFlightCircuit::SetEndPlaces(FlightCircuit, LapRows);
	int32 EndPlaceRows = 0;
	for (const GTPFlightCircuit::FRow& EndRow : LapRows)
	{
		EndPlaceRows += EndRow.bEndPlace ? 1 : 0;
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: flight circuit: the filmed flight ends on %d lane(s) of the lap%s"), EndPlaceRows,
		FlightCircuit.HasEndPlace() ? TEXT("") : TEXT(" (none named: where its time ends)"));
	int32 SpeedRows = 0;
	for (const GTPFlightCircuit::FRow& SpeedRow : CircuitRows)
	{
		SpeedRows += SpeedRow.Kmh > 0.f ? 1 : 0;
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: flight circuit speeds asked by %d of %d row(s), the others by the width of their road"),
		SpeedRows, static_cast<int32>(CircuitRows.size()));
	// The approach. Without it a flight that starts off the lap takes the shortest way to it.
	if (ApproachRows.empty())
	{
		return;
	}
	if (!GTPFlightCircuit::Resolve(ApproachRows, LaneEnds, ApproachLanes, ApproachRenumbered, Why) || !GTPFlightCircuit::SetApproach(FlightCircuit, ApproachLanes, ExitsOf, Why))
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: flight circuit WITHOUT its approach of %d row(s), the shortest way to the lap is taken: %s"),
			static_cast<int32>(ApproachRows.size()), UTF8_TO_TCHAR(Why.c_str()));
		return;
	}
	FString ApproachText;
	for (const int ApproachLane : FlightCircuit.Approach)
	{
		ApproachText += FString::Printf(TEXT(" %d"), ApproachLane);
	}
	GTPFlightCircuit::SetSpeeds(FlightCircuit, LapRows, ApproachRows);
	const bool bApproachLines = GTPFlightCircuit::SetApproachLines(FlightCircuit, ApproachRows, LaneEnds, Why);
	const FString ApproachLineText = bApproachLines ? FString(TEXT("driven on its line")) : FString(TEXT("WITHOUT its driving line: ")) + UTF8_TO_TCHAR(Why.c_str());
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: flight circuit approach of %d lanes:%s, %d row(s) found under another number, %s"),
		static_cast<int32>(FlightCircuit.Approach.size()), *ApproachText, ApproachRenumbered, *ApproachLineText);
}

int32 AGTPTrafficNetwork::LoadLanesFromCsv(const FString& Path)
{
	TArray<FGTPTrafficLane> Read;
	if (!ReadLanesCsv(Path, Read))
	{
		return 0;
	}
	Lanes = MoveTemp(Read);
	LaneBounds.Reset(Lanes.Num());
	for (const FGTPTrafficLane& Lane : Lanes)
	{
		FBox2D Bounds(ForceInit);
		for (const FVector& Point : Lane.Points)
		{
			Bounds += FVector2D(Point.X, Point.Y);
		}
		LaneBounds.Add(Bounds);
	}
	return Lanes.Num();
}

bool AGTPTrafficNetwork::ReadLanesCsv(const FString& Path, TArray<FGTPTrafficLane>& OutLanes)
{
	TArray<FString> Rows;
	if (!FFileHelper::LoadFileToStringArray(Rows, *Path) || Rows.Num() < 2)
	{
		return false;
	}
	OutLanes.Reset();
	// The BD TOPO road of each lane (column road_id), as a small number: the lanes of a road add up to its carriageway.
	TMap<FString, int32> RoadKeyOf;
	std::vector<int> RoadKeys;
	for (int32 RowIndex = 1; RowIndex < Rows.Num(); ++RowIndex)
	{
		// lane_id,road_id,"name",speed_kmh,width_m,points_ue_cm,next : the name may hold commas.
		TArray<FString> Fields;
		FString Current;
		bool bQuoted = false;
		for (const TCHAR Char : Rows[RowIndex])
		{
			if (Char == TEXT('"'))
			{
				bQuoted = !bQuoted;
			}
			else if (Char == TEXT(',') && !bQuoted)
			{
				Fields.Add(Current);
				Current.Reset();
			}
			else
			{
				Current.AppendChar(Char);
			}
		}
		Fields.Add(Current);
		if (Fields.Num() < 7)
		{
			continue;
		}
		FGTPTrafficLane Lane;
		Lane.SpeedKmh = FCString::Atof(*Fields[3]);
		Lane.WidthM = FMath::Max(FCString::Atof(*Fields[4]), 0.f);
		const FString RoadId = Fields[1].TrimStartAndEnd();
		RoadKeys.push_back(RoadId.IsEmpty() ? -1 : RoadKeyOf.FindOrAdd(RoadId, RoadKeyOf.Num()));
		TArray<FString> Points;
		Fields[5].ParseIntoArray(Points, TEXT(";"));
		for (const FString& Point : Points)
		{
			FString X, Y;
			if (Point.Split(TEXT(" "), &X, &Y))
			{
				Lane.Points.Add(FVector(FCString::Atof(*X), FCString::Atof(*Y), 0.f));
			}
		}
		TArray<FString> Next;
		Fields[6].TrimEnd().ParseIntoArray(Next, TEXT(" "));
		for (const FString& Id : Next)
		{
			Lane.Next.Add(FCString::Atoi(*Id));
		}
		if (Fields.Num() >= 9)
		{
			Lane.ToJunction = FCString::Atoi(*Fields[8].TrimEnd());
		}
		OutLanes.Add(MoveTemp(Lane));
	}
	// Drop links to lanes that are not there (a hand-edited file).
	for (FGTPTrafficLane& Lane : OutLanes)
	{
		Lane.Next.RemoveAll([&OutLanes](int32 Id) { return !OutLanes.IsValidIndex(Id); });
	}
	// The carriageway around each lane (GTPRoadWidth.h): what a flight makes of the street, main axis or narrow street.
	std::vector<float> Widths;
	std::vector<std::vector<GTPRoadWidth::FPoint>> Points;
	Widths.reserve(static_cast<size_t>(OutLanes.Num()));
	Points.reserve(static_cast<size_t>(OutLanes.Num()));
	for (const FGTPTrafficLane& Lane : OutLanes)
	{
		Widths.push_back(Lane.WidthM);
		std::vector<GTPRoadWidth::FPoint>& LanePoints = Points.emplace_back();
		LanePoints.reserve(static_cast<size_t>(Lane.Points.Num()));
		for (const FVector& Point : Lane.Points)
		{
			LanePoints.push_back({static_cast<float>(Point.X) * CmToM, static_cast<float>(Point.Y) * CmToM});
		}
	}
	const std::vector<float> RoadWidths = GTPRoadWidth::Carriageways(RoadKeys, Widths, Points);
	for (int32 Index = 0; Index < OutLanes.Num() && static_cast<size_t>(Index) < RoadWidths.size(); ++Index)
	{
		OutLanes[Index].CarriagewayM = RoadWidths[static_cast<size_t>(Index)];
	}
	MarkNeverEnding(OutLanes);
	return OutLanes.Num() > 0;
}

void AGTPTrafficNetwork::MarkNeverEnding(TArray<FGTPTrafficLane>& InLanes)
{
	const std::vector<bool> Alive = GTPRoadWidth::NeverEnding(InLanes.Num(),
		[&InLanes](int Index) -> const TArray<int32>& { return InLanes[Index].Next; },
		[&InLanes](int Index) { return InLanes[Index].bBlocked; });
	for (int32 Index = 0; Index < InLanes.Num(); ++Index)
	{
		InLanes[Index].bNeverEnds = static_cast<size_t>(Index) < Alive.size() && Alive[static_cast<size_t>(Index)];
	}
}

bool AGTPTrafficNetwork::MarkLaneBlocked(int32 Lane, const AActor* By, const FVector& At)
{
	if (!Lanes.IsValidIndex(Lane) || Lanes[Lane].bBlocked)
	{
		return false;
	}
	Lanes[Lane].bBlocked = true;
	MarkNeverEnding(Lanes);
	int32 NeverEnd = 0;
	int32 Blocked = 0;
	for (const FGTPTrafficLane& Each : Lanes)
	{
		NeverEnd += Each.bNeverEnds ? 1 : 0;
		Blocked += Each.bBlocked ? 1 : 0;
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: lane %d blocked by %s at %s (a fixed obstacle of the map on the lane): flights keep off it; %d lane(s) blocked, %d of %d never end now"),
		Lane, *GetNameSafe(By), *At.ToCompactString(), Blocked, NeverEnd, Lanes.Num());
	return true;
}

void AGTPTrafficNetwork::LanesInRing(const FVector& Center, float MinCm, float MaxCm, TArray<int32>& Out) const
{
	Out.Reset();
	for (int32 Index = 0; Index < Lanes.Num(); ++Index)
	{
		const FGTPTrafficLane& Lane = Lanes[Index];
		if (Lane.Points.Num() < 4 || Lane.Next.Num() == 0)
		{
			continue;
		}
		if (LaneBounds.IsValidIndex(Index) && LaneBounds[Index].bIsValid)
		{
			const FBox2D& Box = LaneBounds[Index];
			if (!GTPFar::BoxTouchesRing(static_cast<float>(Box.Min.X), static_cast<float>(Box.Min.Y), static_cast<float>(Box.Max.X),
					static_cast<float>(Box.Max.Y), static_cast<float>(Center.X), static_cast<float>(Center.Y), MinCm, MaxCm))
			{
				continue;
			}
		}
		Out.Add(Index);
	}
}

bool AGTPTrafficNetwork::FindNearestLane(const FVector& Location, float YawDegrees, float MaxDistanceCm, int32& OutLane, int32& OutPoint, bool bFlightCircuitOnly) const
{
	const FVector Heading = FRotator(0.f, YawDegrees, 0.f).Vector();
	double Best = static_cast<double>(MaxDistanceCm) * MaxDistanceCm;
	OutLane = INDEX_NONE;
	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		if (bFlightCircuitOnly && !FlightCircuit.IsKnown(LaneIndex))
		{
			continue;
		}
		const TArray<FVector>& Points = Lanes[LaneIndex].Points;
		for (int32 PointIndex = 0; PointIndex + 1 < Points.Num(); ++PointIndex)
		{
			const double D = FVector::DistSquared2D(Points[PointIndex], Location);
			if (D >= Best)
			{
				continue;
			}
			const FVector Direction = (Points[PointIndex + 1] - Points[PointIndex]).GetSafeNormal2D();
			if (FVector::DotProduct(Direction, Heading) < 0.5f)
			{
				continue;
			}
			Best = D;
			OutLane = LaneIndex;
			OutPoint = PointIndex;
		}
	}
	return OutLane != INDEX_NONE;
}

bool AGTPTrafficNetwork::FindJoinPoint(const FVector& Location, float MaxDistanceCm, float AheadCm, FVector& OutPoint) const
{
	double Best = static_cast<double>(MaxDistanceCm) * MaxDistanceCm;
	int32 BestLane = INDEX_NONE, BestPoint = 0;
	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		const TArray<FVector>& Points = Lanes[LaneIndex].Points;
		for (int32 PointIndex = 0; PointIndex < Points.Num(); ++PointIndex)
		{
			const double D = FVector::DistSquared2D(Points[PointIndex], Location);
			if (D < Best)
			{
				Best = D;
				BestLane = LaneIndex;
				BestPoint = PointIndex;
			}
		}
	}
	if (BestLane == INDEX_NONE)
	{
		return false;
	}
	const TArray<FVector>& Points = Lanes[BestLane].Points;
	OutPoint = Points[BestPoint];
	double Left = AheadCm;
	for (int32 PointIndex = BestPoint + 1; PointIndex < Points.Num() && Left > 0.0; ++PointIndex)
	{
		const double Seg = FVector::Dist2D(Points[PointIndex - 1], Points[PointIndex]);
		OutPoint = Seg >= Left ? FMath::Lerp(Points[PointIndex - 1], Points[PointIndex], Left / FMath::Max(Seg, 1.0)) : Points[PointIndex];
		Left -= Seg;
	}
	return true;
}

void AGTPTrafficNetwork::MakeCheap(AGTPVehicle* Car, ACharacter* Driver) const
{
	// Lights of AI cars never cast shadows: a shadowed spot light is a full extra shadow pass.
	// The lights the car blueprint carries are also switched off (-CarBPLights=1 keeps them):
	// they were on day and night on every car, up to 44 dynamic lights in view. Headlights at
	// night come from the environment, on the nearest cars only (LitCars, GTPEnvironmentSubsystem).
	static const bool bKeepBlueprintLights = []
	{
		int32 Switch = 0;
		FParse::Value(FCommandLine::Get(), TEXT("CarBPLights="), Switch);
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: car blueprint lights %s (-CarBPLights=%d)"), Switch != 0 ? TEXT("kept") : TEXT("off"), Switch);
		return Switch != 0;
	}();
	TArray<ULightComponent*> CarLights;
	Car->GetComponents<ULightComponent>(CarLights);
	for (ULightComponent* Light : CarLights)
	{
		Light->SetCastShadows(false);
		if (!bKeepBlueprintLights && Light->IsVisible())
		{
			Light->SetVisibility(false, false);
		}
	}
	if (Driver)
	{
		// Hidden at the wheel: no pose update, no skinning, until pulled out.
		if (USkeletalMeshComponent* Mesh = Driver->GetMesh())
		{
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
			Mesh->SetCastShadow(false);
		}
	}
}

void AGTPTrafficNetwork::SetCarRenderStatic(USkeletalMeshComponent* Body, bool bStatic)
{
	static const FName StaticTag(TEXT("GTPRenderStatic"));
	if (Body == nullptr || Body->ComponentHasTag(StaticTag) == bStatic)
	{
		return;
	}
	if (bStatic)
	{
		Body->ComponentTags.AddUnique(StaticTag);
	}
	else
	{
		Body->ComponentTags.Remove(StaticTag);
	}
	Body->SetRenderStatic(bStatic);
}

void AGTPTrafficNetwork::UpdateDetail(AGTPVehicle* Car, float DistanceToView) const
{
	USkeletalMeshComponent* Body = Car ? Car->GetMesh() : nullptr;
	if (!Body)
	{
		return;
	}
	const bool bNear = DistanceToView < DetailDistance;
	const bool bShadow = bNear && bCarShadows;
	// A City Sample car is drawn by its look parts (AGTPVehicle::LookMeshes), not by its body: same levers on each.
	TArray<USkeletalMeshComponent*, TInlineAllocator<3>> DrawnParts;
	DrawnParts.Add(Body);
	for (USkeletalMeshComponent* LookPart : Car->GetLookComponents())
	{
		if (LookPart)
		{
			DrawnParts.Add(LookPart);
		}
	}
	for (USkeletalMeshComponent* Part : DrawnParts)
	{
		if (static_cast<bool>(Part->CastShadow) != bShadow)
		{
			Part->SetCastShadow(bShadow);
		}
		const int32 Forced = bNear ? (NearLOD > 0 ? FMath::Clamp(NearLOD + 1, 0, Part->GetNumLODs()) : 0)
			: FMath::Clamp(FarLOD + 1, 0, Part->GetNumLODs());
		if (Part->GetForcedLOD() != Forced)
		{
			Part->SetForcedLOD(Forced);
		}
	}
	const bool bDrawn = DrawDistance <= 0.f || DistanceToView < DrawDistance;
	if (Body->IsVisible() != bDrawn)
	{
		Body->SetVisibility(bDrawn, true);
	}
	// Draw cost levers (section 8), all off by default. Hidden-in-game flags, so they do not
	// fight the visibility above, which propagates to the attached parts.
	if (DriverDrawDistance >= 0.f && Car->HasAIDriver())
	{
		if (ACharacter* AtTheWheel = Car->GetDriver())
		{
			if (USkeletalMeshComponent* Person = AtTheWheel->GetMesh())
			{
				const bool bHide = DistanceToView > DriverDrawDistance;
				if (static_cast<bool>(Person->bHiddenInGame) != bHide)
				{
					Person->SetHiddenInGame(bHide);
				}
			}
		}
	}
	if (LightDistance >= 0.f)
	{
		const bool bLit = DistanceToView <= LightDistance;
		TArray<ULightComponent*> CarLights;
		Car->GetComponents<ULightComponent>(CarLights);
		for (ULightComponent* Light : CarLights)
		{
			// The environment switches visibility for the nearest lit cars: only a light this
			// lever turned off (tagged) is turned back on, so the two never fight.
			static const FName LeverOffTag(TEXT("GTPLightLeverOff"));
			if (!bLit && Light->IsVisible())
			{
				Light->ComponentTags.AddUnique(LeverOffTag);
				Light->SetVisibility(false, false);
			}
			else if (bLit && Light->ComponentHasTag(LeverOffTag))
			{
				Light->ComponentTags.Remove(LeverOffTag);
				Light->SetVisibility(true, false);
			}
		}
	}
	if (StaticRenderDistance >= 0.f)
	{
		const bool bStatic = DistanceToView > StaticRenderDistance;
		SetCarRenderStatic(Body, bStatic);
	}
	if (bVehicleLod)
	{
		// Cost tiers: fewer driver decisions with distance, no pose refresh for a car that is not drawn.
		if (UGTPAIDriverComponent* Driver = Car->FindComponentByClass<UGTPAIDriverComponent>())
		{
			// A ghost moves at each decision: 20 times a second keeps the slide smooth.
			Driver->SetDecisionInterval(Driver->IsGhost() ? 0.05f
				: (DistanceToView < LodNearDistance ? 0.f : (DistanceToView < LodMidDistance ? 0.1f : 0.25f)));
		}
		const EVisibilityBasedAnimTickOption Option = bNear ? EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones
			: EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		if (Body->VisibilityBasedAnimTickOption != Option)
		{
			Body->VisibilityBasedAnimTickOption = Option;
		}
	}
	if (DrawnParts.Num() > 1)
	{
		// The rig of a City Sample car is never rendered: it poses its look parts while one of them was drawn
		// in the last second, or near, and not at all out of view (AGTPVehicle::BeginPlay sets it on).
		bool bPartSeen = false;
		for (int32 Index = 1; Index < DrawnParts.Num(); ++Index)
		{
			bPartSeen = bPartSeen || DrawnParts[Index]->WasRecentlyRendered(1.f);
		}
		const EVisibilityBasedAnimTickOption LookOption = bNear || bPartSeen ? EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones
			: EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		if (Body->VisibilityBasedAnimTickOption != LookOption)
		{
			Body->VisibilityBasedAnimTickOption = LookOption;
		}
	}
}

int32 AGTPTrafficNetwork::PickNext(int32 Lane, FRandomStream* Stream)
{
	if (!Lanes.IsValidIndex(Lane) || Lanes[Lane].Next.Num() == 0)
	{
		return INDEX_NONE;
	}
	const TArray<int32>& Next = Lanes[Lane].Next;
	return Next[(Stream ? *Stream : Random).RandRange(0, Next.Num() - 1)];
}

int32 AGTPTrafficNetwork::PickNextToward(int32 Lane, const FVector& Target) const
{
	if (!Lanes.IsValidIndex(Lane) || Lanes[Lane].Next.Num() == 0)
	{
		return INDEX_NONE;
	}
	int32 Best = INDEX_NONE;
	double BestDistance = TNumericLimits<double>::Max();
	for (const int32 Id : Lanes[Lane].Next)
	{
		if (!Lanes.IsValidIndex(Id) || Lanes[Id].Points.Num() == 0)
		{
			continue;
		}
		const double Distance = FVector::DistSquared2D(Lanes[Id].Points.Last(), Target);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Id;
		}
	}
	return Best;
}

void AGTPTrafficNetwork::BeginPlay()
{
	Super::BeginPlay();
	if (Lanes.Num() == 0)
	{
		const FString File = LanesFileFor(UGameplayStatics::GetCurrentLevelName(this, true));
		const int32 Count = LoadLanesFromCsv(File);
		GTP_TRACE(LogGTPTraffic, TEXT("%d lanes from %s"), Count, *File);
	}
	else
	{
		// Lanes set by hand on the actor: no carriageway known, but the dead ends are.
		MarkNeverEnding(Lanes);
	}
	{
		// What a flight finds on these lanes (GTPRoadWidth.h): the lanes it can keep to without a dead end, and the widths.
		int32 NeverEnd = 0;
		int32 ByClass[4] = {0, 0, 0, 0};
		for (const FGTPTrafficLane& Lane : Lanes)
		{
			NeverEnd += Lane.bNeverEnds ? 1 : 0;
			++ByClass[static_cast<int32>(GTPRoadWidth::Classify(Lane.CarriagewayM))];
		}
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: lanes for a flight: %d of %d never end (no dead end ahead); carriageway: %d main axis (%.0f m and over), %d middle, %d narrow (under %.0f m), %d unknown"),
			NeverEnd, Lanes.Num(), ByClass[static_cast<int32>(GTPRoadWidth::EClass::Main)], GTPRoadWidth::MainMetres,
			ByClass[static_cast<int32>(GTPRoadWidth::EClass::Middle)], ByClass[static_cast<int32>(GTPRoadWidth::EClass::Narrow)], GTPRoadWidth::NarrowMetres,
			ByClass[static_cast<int32>(GTPRoadWidth::EClass::Unknown)]);
	}
	LoadFlightCircuit();
	int32 ParkedSwitch = bParkedCars ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("ParkedCars="), ParkedSwitch);
	bParkedCars = ParkedSwitch != 0;
	if (bParkedCars)
	{
		FActorSpawnParameters ParkedParams;
		ParkedParams.Owner = this;
		if (AGTPParkedCars* Parked = GetWorld()->SpawnActor<AGTPParkedCars>(AGTPParkedCars::StaticClass(), FTransform::Identity, ParkedParams))
		{
			Parked->Build(AGTPParkedCars::ParkingFileFor(UGameplayStatics::GetCurrentLevelName(this, true)));
		}
	}
	int32 LightsFlag = bTrafficLights ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("TrafficLights="), LightsFlag);
	bTrafficLights = LightsFlag != 0;
	if (bTrafficLights)
	{
		BuildLights();
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic lights: %s (%d junctions, %d signal heads)"), bTrafficLights ? TEXT("on") : TEXT("off"),
		Lights.NumSignalledJunctions(), SignalHeads.Num());

	WantedCars = NumCars;
	if (IsMassTrafficRequested())
	{
		// MassTraffic drives the cars (module GTPMass); lights, street parked cars and police stay here.
		// Without the plugin: the legacy traffic, as if nothing was asked.
		const bool bMass = IsMassTrafficWanted(GetWorld());
		WantedCars = bMass ? 0 : NumCars;
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: -Traffic=mass, %s"), bMass ? TEXT("MassTraffic drives")
			: (FModuleManager::Get().IsModuleLoaded(TEXT("MassTraffic")) ? TEXT("no MassTraffic spawner in the map, legacy traffic")
				: TEXT("MassTraffic plugin not loaded, legacy traffic")));
	}
	else
	{
		FParse::Value(FCommandLine::Get(), TEXT("Traffic="), WantedCars);
	}
	WantedCars = FMath::Clamp(WantedCars, 0, 40);
	int32 VehicleLodSwitch = bVehicleLod ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("VehicleLod="), VehicleLodSwitch);
	bVehicleLod = VehicleLodSwitch != 0;
	int32 VehicleGhostSwitch = bVehicleGhost ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("VehicleGhost="), VehicleGhostSwitch);
	bVehicleGhost = VehicleGhostSwitch != 0;
	int32 TrafficPoolSwitch = bTrafficPool ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("TrafficPool="), TrafficPoolSwitch);
	bTrafficPool = TrafficPoolSwitch != 0;
	FParse::Value(FCommandLine::Get(), TEXT("PoolCooldown="), PoolCooldown);
	PoolCooldown = FMath::Max(PoolCooldown, 0.f);
	int32 PoolSpawnCheckSwitch = 1;
	FParse::Value(FCommandLine::Get(), TEXT("PoolSpawnCheck="), PoolSpawnCheckSwitch);
	bPoolSpawnCheck = PoolSpawnCheckSwitch != 0;
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: car pool %s, cooldown %.1f s, spot check %d"), bTrafficPool ? TEXT("on") : TEXT("off"), PoolCooldown,
		bPoolSpawnCheck ? 1 : 0);
	int32 ReactSwitch = bTrafficReact ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("TrafficReact="), ReactSwitch);
	bTrafficReact = ReactSwitch != 0;
	ReactSettings.HonkAfter = ReactHonkAfter;
	ReactSettings.BypassAfter = ReactBypassAfter;
	ReactSettings.PanicSeconds = ReactPanicSeconds;
	FParse::Value(FCommandLine::Get(), TEXT("TrafficReactDemo="), ReactDemoInterval);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: reactions %s, demo every %.0f s"), bTrafficReact ? TEXT("on") : TEXT("off"), ReactDemoInterval);
	int32 FarCars = NumFarCars;
	FParse::Value(FCommandLine::Get(), TEXT("FarTraffic="), FarCars);
	// -Traffic=0 turns all traffic off, far cars included (benches of other systems rely on it).
	FarCars = WantedCars > 0 ? FMath::Clamp(FarCars, 0, 60) : 0;
	if (FarCars > 0)
	{
		// Far cars hand over to driven cars that are ghosts by then.
		bVehicleGhost = true;
	}
	// Sweeps of the draw cost (tools/perf/traffic-lod-sweep-mac.sh), in metres:
	// -GhostDistance=M (cars past M are ghosts, awake again 20 m nearer; far cars hand over at M),
	// -LodNear=M (full LOD and shadow only under M, the FarLOD beyond), -FarLOD=N.
	float GhostMetres = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("GhostDistance="), GhostMetres) && GhostMetres > 0.f)
	{
		GhostDistance = GhostMetres * 100.f;
		GhostWakeDistance = FMath::Max(GhostDistance - 2000.f, GhostDistance * 0.5f);
	}
	// -TrafficRing=M: driven cars are made out of sight between MinSpawnDistance and M from the player, and taken away
	// past M out of sight (MaxCarDistance). 150 m since candidate 61: at 250 m, 19 of the 22 driven cars were ghosts past
	// 90 m at the end of gate 59's default pass, and its video showed none around the player.
	float RingMetres = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("TrafficRing="), RingMetres) && RingMetres * 100.f > MinSpawnDistance)
	{
		MaxCarDistance = RingMetres * 100.f;
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: driven cars made out of sight %.0f to %.0f m from the player, taken away past %.0f m out of sight (-TrafficRing=M)"),
		MinSpawnDistance / 100.f, MaxCarDistance / 100.f, MaxCarDistance / 100.f);
	float LodNearMetres = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("LodNear="), LodNearMetres) && LodNearMetres >= 0.f)
	{
		DetailDistance = LodNearMetres * 100.f;
	}
	FParse::Value(FCommandLine::Get(), TEXT("FarLOD="), FarLOD);
	float CarMetres = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("CarDriverDraw="), CarMetres))
	{
		DriverDrawDistance = CarMetres < 0.f ? -1.f : CarMetres * 100.f;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("CarLightDistance="), CarMetres))
	{
		LightDistance = CarMetres < 0.f ? -1.f : CarMetres * 100.f;
	}
	if (FParse::Value(FCommandLine::Get(), TEXT("CarRenderStatic="), CarMetres))
	{
		StaticRenderDistance = CarMetres < 0.f ? -1.f : CarMetres * 100.f;
	}
	FParse::Value(FCommandLine::Get(), TEXT("CarNearLOD="), NearLOD);
	int32 CarShadowsSwitch = bCarShadows ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("CarShadows="), CarShadowsSwitch);
	bCarShadows = CarShadowsSwitch != 0;
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: draw levers driver %.0f m, lights %.0f m, static %.0f m, near LOD %d, shadows %s (negative = off)"),
		DriverDrawDistance / 100.f, LightDistance / 100.f, StaticRenderDistance / 100.f, NearLOD, bCarShadows ? TEXT("on") : TEXT("off"));
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: full detail under %.0f m, LOD %d beyond"), DetailDistance / 100.f, FarLOD);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: ghost mode %s (past %.0f m, awake inside %.0f m)"),
		bVehicleGhost ? TEXT("on") : TEXT("off"), GhostDistance / 100.f, GhostWakeDistance / 100.f);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: vehicle LOD tiers %s"), bVehicleLod ? TEXT("on") : TEXT("off"));

	for (const FSoftClassPath& Path : VehicleClasses)
	{
		if (UClass* Class = Path.TryLoadClass<AGTPVehicle>())
		{
			LoadedVehicleClasses.Add(Class);
		}
	}
	KeepDrawnClasses();
	if (LoadedVehicleClasses.Num() == 0)
	{
		// Same car as -TestVehicle, read from the game mode's config.
		FString TestCar;
		GConfig->GetString(TEXT("/Script/GranTheftPraline.GTPGameMode"), TEXT("TestVehicleClass"), TestCar, GGameIni);
		if (UClass* Class = FSoftClassPath(TestCar).TryLoadClass<AGTPVehicle>())
		{
			LoadedVehicleClasses.Add(Class);
		}
	}
	LoadedDriverClass = DriverClass.TryLoadClass<ACharacter>();
	int32 LiteClassSwitch = 0;
	FParse::Value(FCommandLine::Get(), TEXT("LiteDriverClass="), LiteClassSwitch);
	if (LiteClassSwitch != 0)
	{
		// Camera-less occupant (no camera host created at spawn), for traffic and far puppets.
		LoadedDriverClass = AGTPLiteDriver::StaticClass();
	}
	if (!LoadedDriverClass)
	{
		AGameModeBase* Mode = GetWorld()->GetAuthGameMode();
		UClass* PawnClass = Mode ? Mode->GetDefaultPawnClassForController(nullptr) : nullptr;
		LoadedDriverClass = PawnClass && PawnClass->IsChildOf(ACharacter::StaticClass()) ? PawnClass : nullptr;
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: %d cars wanted, %d car classes, driver %s"), WantedCars, LoadedVehicleClasses.Num(),
		LoadedDriverClass ? *LoadedDriverClass->GetName() : TEXT("none"));
	// Far traffic last: its puppet cars need the car and driver classes loaded above.
	if (FarCars > 0)
	{
		FActorSpawnParameters FarParams;
		FarParams.Owner = this;
		FarTraffic = GetWorld()->SpawnActor<AGTPFarTraffic>(AGTPFarTraffic::StaticClass(), FTransform::Identity, FarParams);
		if (FarTraffic)
		{
			FarTraffic->Build(this, FarCars);
		}
	}
	// Let the player and the level settle before the first car.
	NextSpawnTime = GetWorld()->GetTimeSeconds() + 2.f;
}

bool AGTPTrafficNetwork::IsSeenByPlayer(const FVector& Location) const
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return false;
	}
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector View = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector To = Location - Eye;
	// In the view cone and not hidden behind a building.
	if (FVector::DotProduct(To.GetSafeNormal(), View) < FMath::Cos(FMath::DegreesToRadians(60.f)))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPTrafficSight), false, PC->GetPawn());
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eye, Location + FVector(0.f, 0.f, 150.f), ECC_Visibility, Params);
}

void AGTPTrafficNetwork::KeepDrawnClasses()
{
	if (bDrawnClassesKept || LoadedVehicleClasses.Num() == 0)
	{
		return;
	}
	bDrawnClassesKept = true;
	TArray<TSubclassOf<AGTPVehicle>> Drawn;
	for (const TSubclassOf<AGTPVehicle>& Class : LoadedVehicleClasses)
	{
		FString Why;
		const bool bClassDrawn = AGTPVehicle::IsClassDrawn(Class.Get(), Why);
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: car class %s %s: %s"), *GetNameSafe(Class.Get()),
			bClassDrawn ? TEXT("drawn, in the traffic") : TEXT("NOT DRAWN, left out of the traffic and the parked cars"), *Why);
		if (bClassDrawn)
		{
			Drawn.Add(Class);
		}
	}
	if (Drawn.Num() == 0)
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: no car class is drawn: all %d kept"), LoadedVehicleClasses.Num());
		return;
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: %d car class(es) drawn of %d"), Drawn.Num(), LoadedVehicleClasses.Num());
	LoadedVehicleClasses = MoveTemp(Drawn);
}

AGTPVehicle* AGTPTrafficNetwork::SpawnParkedVehicle(FName Tag, const FVector& Near, float RadiusCm, TSubclassOf<AGTPVehicle> Class, bool bQuietMiss)
{
	if (!Class)
	{
		if (LoadedVehicleClasses.Num() == 0)
		{
			for (const FSoftClassPath& Path : VehicleClasses)
			{
				if (UClass* Loaded = Path.TryLoadClass<AGTPVehicle>())
				{
					LoadedVehicleClasses.Add(Loaded);
				}
			}
			KeepDrawnClasses();
		}
		if (LoadedVehicleClasses.Num() == 0)
		{
			GTP_TRACE(LogGTPTraffic, TEXT("traffic: no car class for a parked mission car"));
			return nullptr;
		}
		Class = LoadedVehicleClasses[Random.RandRange(0, LoadedVehicleClasses.Num() - 1)];
	}
	std::vector<std::vector<GTPKerb::FPoint2>> Polylines;
	Polylines.reserve(Lanes.Num());
	for (const FGTPTrafficLane& Lane : Lanes)
	{
		std::vector<GTPKerb::FPoint2>& Line = Polylines.emplace_back();
		Line.reserve(Lane.Points.Num());
		for (const FVector& Point : Lane.Points)
		{
			Line.push_back({static_cast<float>(Point.X), static_cast<float>(Point.Y)});
		}
	}
	const std::vector<GTPKerb::FSegmentRef> Segments = GTPKerb::SegmentsNear(Polylines,
		{static_cast<float>(Near.X), static_cast<float>(Near.Y)}, FMath::Max(RadiusCm, 100.f));
	// Every lane segment a spot could stand on: a spot is ParkOffset from the middle of a segment of Segments, and lane
	// points are 4 m apart, so 10 m more than the request's radius holds them all.
	const std::vector<GTPKerb::FSegmentRef> Nearby = GTPKerb::SegmentsNear(Polylines,
		{static_cast<float>(Near.X), static_cast<float>(Near.Y)}, FMath::Max(RadiusCm, 100.f) + FMath::Max(ParkOffset, 0.f) + 1000.f);
	int32 OnLane = 0;
	int32 OnFurniture = 0;
	int32 NearEnd = 0;
	int32 BodyOnLane = 0;
	float BodyFront = 230.f;
	float BodyBack = 230.f;
	float BodyHalfWidth = 95.f;
	ParkedBody(Class, BodyFront, BodyBack, BodyHalfWidth);
	// Out of sight first; if every spot is seen, a seen one rather than nothing. Passes 2 and 3, only for a request that
	// must be met (bQuietMiss false): the same with the spots near a lane's end, on a bend or with the body over a lane.
	const int32 Passes = bQuietMiss ? 2 : 4;
	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		const bool bUnseenOnly = Pass % 2 == 0;
		const bool bClearSpotsOnly = Pass < 2;
		int32 Tried = 0;
		for (const GTPKerb::FSegmentRef& Ref : Segments)
		{
			if (++Tried > 64)
			{
				break;
			}
			const FVector& A = Lanes[Ref.Lane].Points[Ref.Point];
			const FVector& B = Lanes[Ref.Lane].Points[Ref.Point + 1];
			GTPKerb::FSpot Spot;
			if (!GTPKerb::SpotBeside({static_cast<float>(A.X), static_cast<float>(A.Y)}, {static_cast<float>(B.X), static_cast<float>(B.Y)}, ParkOffset, Spot))
			{
				continue;
			}
			// Never on a lane: beside one lane, the spot can be on the next one (gate 59, a parked car in the chase's way).
			if (!GTPKerb::IsClearOfLanes(Polylines, Nearby, Spot.Position, ParkLaneClearance))
			{
				OnLane += Pass == 0 ? 1 : 0;
				continue;
			}
			if (bClearSpotsOnly)
			{
				// Out of the way of traffic where it cuts corners (gate 62: queues behind parked cars at lane ends and bends,
				// and behind a Clio whose bonnet stood over another lane).
				const GTPKerb::FLaneAround Around = GTPKerb::LaneAroundSpot(Polylines[Ref.Lane], Ref.Point, ParkEndMargin);
				if (Around.FromStart < ParkEndMargin || Around.ToEnd < ParkEndMargin || Around.TowardKerb > ParkBendAllowance)
				{
					NearEnd += Pass == 0 ? 1 : 0;
					continue;
				}
				if (!GTPKerb::IsBodyClearOfLanes(Polylines, Nearby, Spot, BodyFront, BodyBack, BodyHalfWidth, ParkBodyClearance, Ref.Lane))
				{
					BodyOnLane += Pass == 0 ? 1 : 0;
					continue;
				}
			}
			FVector Location(Spot.Position.X, Spot.Position.Y, (A.Z + B.Z) * 0.5);
			if (bUnseenOnly && IsSeenByPlayer(Location))
			{
				continue;
			}
			FHitResult Ground;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPParkedGround), false);
			if (!GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
			{
				continue;
			}
			// The first thing under the trace can be street furniture: never a car on a bus shelter roof. The road at
			// the lane beside gives the height a kerb spot can have (something over the lane would only let a spot by).
			FHitResult Road;
			const FVector LaneMiddle = (A + B) * 0.5;
			if (GetWorld()->LineTraceSingleByChannel(Road, LaneMiddle + FVector(0.f, 0.f, 500.f), LaneMiddle - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params)
				&& Ground.ImpactPoint.Z - Road.ImpactPoint.Z > ParkKerbRise)
			{
				OnFurniture += Pass == 0 ? 1 : 0;
				continue;
			}
			const float Clearance = ParkedClearance(Class);
			Location.Z = Ground.ImpactPoint.Z + Clearance;
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
			AGTPVehicle* Car = GetWorld()->SpawnActor<AGTPVehicle>(Class, FTransform(FRotator(0.f, Spot.YawDegrees, 0.f), Location), SpawnParams);
			if (!Car)
			{
				continue;
			}
			if (!Tag.IsNone())
			{
				Car->Tags.AddUnique(Tag);
			}
			GTP_TRACE(LogGTPTraffic, TEXT("traffic: parked %s (%s) on lane %d at %s, %.0f m from the request, pivot %.0f cm above the ground (the mesh's lowest point at 40 cm), %d spot(s) on another lane left out, %d on street furniture, %d near a lane end or bend, %d with the body over a lane%s"),
				*Car->GetName(), *Tag.ToString(), Ref.Lane, *Location.ToCompactString(), Ref.Distance / 100.f, Clearance, OnLane, OnFurniture,
				NearEnd, BodyOnLane, bClearSpotsOnly ? TEXT("") : TEXT(" (no clear spot: one in the way of traffic)"));
			return Car;
		}
	}
	if (!bQuietMiss)
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: no free spot for a parked car (%s) within %.0f m of %s (%d spot(s) on another lane left out, %d on street furniture, %d near a lane end or bend, %d with the body over a lane)"),
			*Tag.ToString(), RadiusCm / 100.f, *Near.ToCompactString(), OnLane, OnFurniture, NearEnd, BodyOnLane);
	}
	return nullptr;
}

bool AGTPTrafficNetwork::FindSpawnPoint(int32& OutLane, int32& OutPoint, FTransform& OutTransform) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector PlayerLocation = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	// More tries when many cars are wanted: most lanes of the city are out of range.
	const int32 MaxTries = WantedCars > 12 ? 120 : 40;
	// Only lanes that reach the spawn ring: on the quarter's 1 000 lanes most are out of range.
	TArray<int32> Candidates;
	if (Player)
	{
		LanesInRing(PlayerLocation, MinSpawnDistance, MaxCarDistance, Candidates);
		if (Candidates.Num() == 0)
		{
			return false;
		}
	}
	for (int32 Try = 0; Try < MaxTries; ++Try)
	{
		const int32 Lane = Player ? Candidates[Random.RandRange(0, Candidates.Num() - 1)] : Random.RandRange(0, Lanes.Num() - 1);
		const TArray<FVector>& Points = Lanes[Lane].Points;
		if (Points.Num() < 4 || Lanes[Lane].Next.Num() == 0)
		{
			continue;
		}
		const int32 Point = Random.RandRange(1, Points.Num() - 3);
		FVector Location = Points[Point];
		const double Distance = FVector::Dist2D(Location, PlayerLocation);
		if (Player && (Distance < MinSpawnDistance || Distance > MaxCarDistance))
		{
			continue;
		}
		// Onto the ground.
		FHitResult Ground;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPTrafficGround), false);
		if (!GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			continue;
		}
		Location.Z = Ground.ImpactPoint.Z + 40.f;
		// Free room around it, and out of sight.
		bool bFree = true;
		for (const AGTPVehicle* Car : Cars)
		{
			if (Car && FVector::Dist(Car->GetActorLocation(), Location) < 1500.f)
			{
				bFree = false;
				break;
			}
		}
		if (!bFree || IsSeenByPlayer(Location))
		{
			continue;
		}
		const FVector Direction = (Points[Point + 1] - Points[Point]).GetSafeNormal2D();
		OutLane = Lane;
		OutPoint = Point;
		OutTransform = FTransform(Direction.Rotation(), Location);
		return true;
	}
	return false;
}

AGTPVehicle* AGTPTrafficNetwork::CreateCar(const FTransform& Transform, ESpawnActorCollisionHandlingMethod Collision)
{
	if (LoadedVehicleClasses.Num() == 0)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = Collision;
	TSubclassOf<AGTPVehicle> Class = LoadedVehicleClasses[Random.RandRange(0, LoadedVehicleClasses.Num() - 1)];
	AGTPVehicle* Car = GetWorld()->SpawnActor<AGTPVehicle>(Class, Transform, Params);
	if (!Car)
	{
		return nullptr;
	}
	Car->NotePlaced(TEXT("spawn"));
	if (LoadedDriverClass)
	{
		FActorSpawnParameters DriverParams;
		DriverParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ACharacter* Driver = GetWorld()->SpawnActor<ACharacter>(LoadedDriverClass, Transform, DriverParams))
		{
			// An AI controller lets the driver run away once pulled out.
			Driver->AIControllerClass = AAIController::StaticClass();
			Driver->SpawnDefaultController();
			Car->SeatOccupant(Driver);
			MakeCheap(Car, Driver);
		}
	}
	if (!Car->HasAIDriver())
	{
		Car->Destroy();
		return nullptr;
	}
	UGTPAIDriverComponent* AIDriver = NewObject<UGTPAIDriverComponent>(Car, TEXT("AIDriver"));
	// Known from the start, even before Start: a far traffic puppet is made here and first driven through PlaceGhost,
	// which did nothing without it. Until candidate 63 every puppet handed over stood still where it was, counted as a
	// driven ghost stopped for nothing (gate 60's default pass: 0 of 22 driven cars moving at 102 s).
	AIDriver->SetNetwork(this);
	AIDrivers.Add(AIDriver);
	AIDriver->RegisterComponent();
	return Car;
}

bool AGTPTrafficNetwork::SpawnCar()
{
	if (LoadedVehicleClasses.Num() == 0 || Lanes.Num() == 0)
	{
		return false;
	}
	int32 Lane = 0, Point = 0;
	FTransform Transform;
	if (!FindSpawnPoint(Lane, Point, Transform))
	{
		return false;
	}
	if (AGTPVehicle* Reused = bTrafficPool ? TakeFromPool(Transform) : nullptr)
	{
		if (UGTPAIDriverComponent* ReusedDriver = Reused->FindComponentByClass<UGTPAIDriverComponent>())
		{
			// Back on the road the way a far puppet wakes: kinematic ghost, plain teleport, and
			// the ghost switch of the next ticks turns the Chaos vehicle back on near the player.
			// Physics used to come back in the very frame of a ResetPhysics teleport, and a car
			// spawned beyond the ghost distance went kinematic again the next frame: the Mac
			// crashes (8 of 8) are all in the Chaos suspension graph (CreateGraphEdge).
			const FVector Ground = Transform.GetLocation() - FVector(0.f, 0.f, 40.f);
			// Rolling, as the traffic around it (out of sight, at least MinSpawnDistance away): from a standstill a
			// car counted as standing for its first second, and a refill after a jump stood all at once.
			const float RollMs = FMath::Min(Lanes.IsValidIndex(Lane) ? Lanes[Lane].SpeedKmh : 30.f, 30.f) / 3.6f;
			if (!ReusedDriver->PlaceGhost(Lane, Point, Ground, static_cast<float>(Transform.Rotator().Yaw), RollMs))
			{
				// Not moved. Its collision back on where it was put away, it would stand there, kinematic, in whatever came
				// by since: until candidate 63 the frozen puppets did, and the chase script puts away the car in front of
				// the player's van (the probable cause of the van thrown over the roofs in gate 61's video). Taken away.
				if (ACharacter* Seated = Reused->GetDriver())
				{
					if (AController* Brain = Seated->GetController())
					{
						Brain->Destroy();
					}
					Seated->Destroy();
				}
				Reused->Destroy();
				return false;
			}
			Reused->SetActorEnableCollision(true);
			if (!AIDrivers.Contains(ReusedDriver))
			{
				AIDrivers.Add(ReusedDriver);
			}
			Cars.AddUnique(Reused);
			++ReusedFromPool;
			GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s reused on lane %d"), *Reused->GetName(), Lane);
			return true;
		}
		Reused->Destroy();
	}
	else if (bTrafficPool && Pool.Num() > 0)
	{
		// Pooled cars are cooling down: wait for them rather than spawn a new car (a spawn hitch).
		return false;
	}
	AGTPVehicle* Car = CreateCar(Transform, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
	UGTPAIDriverComponent* AIDriver = Car ? Car->FindComponentByClass<UGTPAIDriverComponent>() : nullptr;
	if (!AIDriver)
	{
		return false;
	}
	AIDriver->Start(this, Lane, Point);
	Cars.Add(Car);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s on lane %d at %s"), *Car->GetName(), Lane, *Transform.GetLocation().ToCompactString());
	return true;
}

bool AGTPTrafficNetwork::PutAway(AGTPVehicle* Car)
{
	UGTPAIDriverComponent* AIDriver = Car->FindComponentByClass<UGTPAIDriverComponent>();
	if (!AIDriver || !Car->HasAIDriver() || Car->IsWrecked())
	{
		return false;
	}
	AIDriver->ReleaseJunctions();
	// Physics and the Chaos vehicle off (the ghost switch does both), no decisions, not drawn, no collision.
	AIDriver->SetGhost(true);
	// The pool's cooldown counts from when its physics went off: a ghost for a while (a far car) has cooled down
	// already, a car with physics until now (off just above) waits PoolCooldown.
	const double PhysicsOffSince = AIDriver->GetGhostSince();
	AIDriver->SetComponentTickEnabled(false);
	Car->SetActorHiddenInGame(true);
	Car->SetActorEnableCollision(false);
	if (ACharacter* Driver = Car->GetDriver())
	{
		Driver->SetActorHiddenInGame(true);
	}
	if (!Pool.Contains(Car))
	{
		Pool.Add(Car);
		PoolSince.Add(PhysicsOffSince);
	}
	return true;
}

AGTPVehicle* AGTPTrafficNetwork::TakeFromPool(const FTransform& Transform)
{
	PoolSince.SetNum(Pool.Num());
	const double Now = GetWorld()->GetTimeSeconds();
	while (Pool.Num() > 0)
	{
		// Oldest first, and only once it has cooled down (PoolSince: its physics off since): never a car whose
		// physics went off a moment ago. A far car pooled as a ghost of long standing goes before one pooled with
		// its physics on.
		int32 Pick = INDEX_NONE;
		for (int32 Index = 0; Index < Pool.Num() && Pick == INDEX_NONE; ++Index)
		{
			Pick = !IsValid(Pool[Index]) || Now - PoolSince[Index] >= PoolCooldown ? Index : INDEX_NONE;
		}
		if (Pick == INDEX_NONE)
		{
			return nullptr;
		}
		AGTPVehicle* Car = Pool[Pick];
		Pool.RemoveAt(Pick);
		PoolSince.RemoveAt(Pick);
		if (!IsValid(Car))
		{
			continue;
		}
		// Only a car that is still whole goes back on the road: its driver seated and alive,
		// its AI driver there, not wrecked, not taken back meanwhile (far traffic, police).
		ACharacter* Seated = Car->GetDriver();
		if (!IsValid(Seated) || !Car->HasAIDriver() || Car->IsWrecked() || Cars.Contains(Car)
			|| !IsValid(Car->FindComponentByClass<UGTPAIDriverComponent>()))
		{
			GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s dropped from the pool (driver %s, wrecked %d, already driven %d)"), *Car->GetName(),
				IsValid(Seated) ? TEXT("ok") : TEXT("gone"), Car->IsWrecked() ? 1 : 0, Cars.Contains(Car) ? 1 : 0);
			if (!Cars.Contains(Car))
			{
				// Destroyed, not put back: Recycle would pool it again.
				if (IsValid(Seated))
				{
					if (AController* Brain = Seated->GetController())
					{
						Brain->Destroy();
					}
					Seated->Destroy();
				}
				Car->Destroy();
			}
			continue;
		}
		// The lane spot was checked against the traffic cars only: a police car, a puppet or a
		// parked car may stand there. Teleporting a simulated car into another one gave the
		// solver a deep overlap (Mac crash right after "reused on lane", police on the same lane).
		if (const AActor* Blocker = bPoolSpawnCheck ? FindCarAt(Car, Transform) : nullptr)
		{
			GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s reuse blocked by %s"), *Car->GetName(), *Blocker->GetName());
			Pool.Insert(Car, 0);
			PoolSince.Insert(Now - PoolCooldown, 0);
			return nullptr;
		}
		// Placed by the caller as a ghost (PlaceGhost), collision on once it is there.
		Car->SetActorHiddenInGame(false);
		if (ACharacter* Driver = Car->GetDriver())
		{
			Driver->SetActorHiddenInGame(false);
		}
		return Car;
	}
	return nullptr;
}

void AGTPTrafficNetwork::UpdateReactDemo()
{
	// -TrafficReactDemo=S: every S seconds a driving car 10 to 25 m in front of the camera
	// is aimed at, rammed or shot near, in turn, so a video shows every reaction.
	const double Now = GetWorld()->GetTimeSeconds();
	if (ReactDemoInterval <= 0.f || !bTrafficReact || Now < NextReactDemoTime)
	{
		return;
	}
	NextReactDemoTime = Now + ReactDemoInterval;
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	if (!Viewer || !Viewer->PlayerCameraManager)
	{
		return;
	}
	const FVector View = Viewer->PlayerCameraManager->GetCameraLocation();
	const FVector Look = Viewer->PlayerCameraManager->GetCameraRotation().Vector();
	// The car to film: driving (over 8 km/h), 10 to 25 m away, in front of the camera.
	// Each miss adds to the score as metres: out of view +40, slow +30, too near +20.
	// A car that is not ideal is used only after a few 2 s waits for a better one.
	UGTPAIDriverComponent* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	float BestDistance = 0.f;
	bool bBestIdeal = false;
	int32 Candidates = 0;
	for (AGTPVehicle* Car : Cars)
	{
		UGTPAIDriverComponent* Driver = IsValid(Car) ? Car->FindComponentByClass<UGTPAIDriverComponent>() : nullptr;
		if (!Driver || Driver->IsGhost() || !Car->HasAIDriver() || Car->IsWrecked() || Driver->GetReactState() != GTPTrafficReact::EState::Cruise)
		{
			continue;
		}
		const FVector To = Car->GetActorLocation() - View;
		const float Distance = static_cast<float>(To.Size());
		if (Distance < 600.f || Distance > 9000.f)
		{
			continue;
		}
		++Candidates;
		const bool bInView = FVector::DotProduct(To.GetSafeNormal(), Look) > 0.7f;
		// Driving forward: a car backing out is no car to film.
		const bool bMoving = Car->GetSpeedKmh() > 8.f;
		const bool bNear = Distance >= 1000.f && Distance <= 2500.f;
		const float Score = Distance / 100.f + (bInView ? 0.f : 40.f) + (bMoving ? 0.f : 30.f) + (Distance < 1000.f ? 20.f : 0.f);
		if (Score < BestScore)
		{
			Best = Driver;
			BestScore = Score;
			BestDistance = Distance;
			bBestIdeal = bInView && bMoving && bNear;
		}
	}
	if (!Best)
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic react demo: no car within 90 m (%d driven in all)"), Cars.Num());
		NextReactDemoTime = Now + 2.0;
		return;
	}
	if (!bBestIdeal && ReactDemoWaits < 2)
	{
		++ReactDemoWaits;
		NextReactDemoTime = Now + 2.0;
		return;
	}
	if (!bBestIdeal && !bReactDemoMoved && MovePlayerAhead(Best))
	{
		// The player now stands on the kerb ahead of that car, facing it: the next
		// pass, a second later, finds it coming into view 15 to 25 m away.
		bReactDemoMoved = true;
		NextReactDemoTime = Now + 1.0;
		return;
	}
	ReactDemoWaits = 0;
	bReactDemoMoved = false;
	static const TCHAR* Events[] = {TEXT("aim"), TEXT("ram"), TEXT("shot")};
	const int32 Event = ReactDemoStep++ % 3;
	Best->InjectReaction(Event == 0, Event == 2, Event == 1);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic react demo: %s on %s at %.0f m, %.0f km/h, %s (%d candidates)"), Events[Event], *GetNameSafe(Best->GetOwner()),
		BestDistance / 100.f, Cast<AGTPVehicle>(Best->GetOwner()) ? Cast<AGTPVehicle>(Best->GetOwner())->GetSpeedKmh() : 0.f, bBestIdeal ? TEXT("in view") : TEXT("best found"), Candidates);
}

bool AGTPTrafficNetwork::MovePlayerAhead(const UGTPAIDriverComponent* Driver)
{
	// -TrafficReactDemo only: no driving car near the camera, so the player on foot goes
	// to the kerb 25 m ahead of one, 4 m to its right, and looks at it.
	const AGTPVehicle* Car = Driver ? Cast<AGTPVehicle>(Driver->GetOwner()) : nullptr;
	APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	ACharacter* Player = Viewer ? Cast<ACharacter>(Viewer->GetPawn()) : nullptr;
	if (!Car || !Player)
	{
		return false;
	}
	const FVector Forward = Car->GetActorForwardVector().GetSafeNormal2D();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector Ahead = Car->GetActorLocation() + Forward * 2500.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPReactDemoMove), false, Car);
	Params.AddIgnoredActor(Player);
	for (const float Side : {400.f, 250.f, -400.f})
	{
		const FVector Spot = Ahead + Right * Side;
		FHitResult Hit;
		// Not through a wall from the lane, and ground under the spot.
		if (GetWorld()->LineTraceSingleByChannel(Hit, Ahead + FVector(0.f, 0.f, 100.f), Spot + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params))
		{
			continue;
		}
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Spot + FVector(0.f, 0.f, 500.f), Spot - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			continue;
		}
		const float HalfHeight = Player->GetSimpleCollisionHalfHeight();
		const FVector Feet = Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight + 5.f);
		const FRotator Facing = (Car->GetActorLocation() - Feet).GetSafeNormal2D().Rotation();
		if (!Player->TeleportTo(Feet, Facing))
		{
			continue;
		}
		Viewer->SetControlRotation(FRotator(-5.f, Facing.Yaw, 0.f));
		GTP_TRACE(LogGTPTraffic, TEXT("traffic react demo: player moved 25 m ahead of %s (%.0f m to the side)"), *Car->GetName(), Side / 100.f);
		return true;
	}
	return false;
}

const AActor* AGTPTrafficNetwork::FindCarAt(const AGTPVehicle* Car, const FTransform& Transform) const
{
	const UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Car->GetRootComponent());
	if (!Body)
	{
		return nullptr;
	}
	// The car's own box at the new place, 30 cm wider on each side.
	const FBox Local = Body->CalcBounds(FTransform::Identity).GetBox();
	const FVector Extent = Local.GetExtent() + FVector(30.f);
	const FVector Center = Transform.TransformPosition(Local.GetCenter());
	TArray<FOverlapResult> Overlaps;
	const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::InitType::AllDynamicObjects);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GTPPoolReuse), false, Car);
	if (!GetWorld()->OverlapMultiByObjectType(Overlaps, Center, Transform.GetRotation(), Objects, FCollisionShape::MakeBox(Extent), Query))
	{
		return nullptr;
	}
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* Other = Overlap.GetActor();
		if (Other && Other != Car && Other->IsA<AGTPVehicle>() && !Other->IsHidden())
		{
			return Other;
		}
	}
	return nullptr;
}

bool AGTPTrafficNetwork::Recycle(AGTPVehicle* Car)
{
	if (!IsValid(Car))
	{
		return false;
	}
	// One owner for a car. A far traffic puppet is the far traffic's: drawn and moved by it at every tick, without
	// collision. The chase script hands in every car with an AI driver near the player's, puppets too: put in the pool,
	// a puppet came back as a driven car while the far traffic still moved it, and at its next handover it was
	// listed a second time in Cars. Taking it away then removed two entries in one pass of Tick's loop, which ran
	// past the end of the list. Read in the code after gate 81's crash at 152 s, not seen in its log.
	if (FarTraffic && FarTraffic->IsPuppet(Car))
	{
		Cars.Remove(Car);
		if (++PuppetsLeft <= 3)
		{
			GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s is a far traffic puppet, not taken away (%d so far)"), *Car->GetName(), PuppetsLeft);
		}
		return false;
	}
	// Put away already (the chase script looks at every car, hidden ones too): nothing to do, nothing to count.
	if (Pool.Contains(Car))
	{
		return false;
	}
	++RecycledAll;
	Cars.Remove(Car);
	if (bTrafficPool && PutAway(Car))
	{
		return true;
	}
	if (UGTPAIDriverComponent* AIDriver = Car->FindComponentByClass<UGTPAIDriverComponent>())
	{
		AIDriver->ReleaseJunctions();
	}
	if (ACharacter* Driver = Car->GetDriver())
	{
		if (AController* Controller = Driver->GetController())
		{
			Controller->Destroy();
		}
		Driver->Destroy();
	}
	Car->Destroy();
	return true;
}

void AGTPTrafficNetwork::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const double TickStart = FPlatformTime::Seconds();
	ON_SCOPE_EXIT
	{
		NetworkTickSeconds += FPlatformTime::Seconds() - TickStart;
	};
	LogBudget();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	// Moved farther than the traffic reaches since the last tick with a player (respawn, a script's teleport): every
	// car is far now, taken away below, and the road around the player is filled again from the pool at once.
	const FVector PlayerWasAt = PlayerJumpCheckAt;
	const bool bPlayerJumped = Player && !PlayerWasAt.IsZero()
		&& FVector::Dist2D(PlayerWasAt, Player->GetActorLocation()) > MaxCarDistance;
	const int32 RecycledFarBefore = RecycledFar;
	if (bPlayerJumped)
	{
		PoolRefillUntil = GetWorld()->GetTimeSeconds() + PoolCooldown + 1.0;
	}
	if (Player)
	{
		LastPlayerPawn = Player;
		LastPlayerAt = Player->GetActorLocation();
		PlayerJumpCheckAt = LastPlayerAt;
	}
	else if (!LastPlayerAt.IsZero())
	{
		// Once: from here no car is spawned or taken away by its distance to the player.
		const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
		const FString ViewTarget = Controller ? GetNameSafe(Controller->GetViewTarget()) : FString(TEXT("no controller"));
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: no player pawn from t=%.0fs (last %s at %s, %s; view target %s): cars spawn on any lane, none is taken away as far"),
			GetWorld()->GetTimeSeconds(), *GetNameSafe(LastPlayerPawn.Get()), *LastPlayerAt.ToCompactString(),
			LastPlayerPawn.IsValid() ? TEXT("still in the world, not possessed") : TEXT("destroyed"), *ViewTarget);
		LastPlayerAt = FVector::ZeroVector;
	}

	// Forget stolen cars (the player's now); take away stuck or far cars out of sight.
	for (int32 Index = Cars.Num() - 1; Index >= 0; --Index)
	{
		// Gate 81 crashed here at t=152 s (index out of range): one pass of the loop took more than one car out of
		// Cars. Recycle removes every entry of a car, and a far traffic puppet the chase script puts in the pool can be
		// listed twice (see Recycle and AGTPFarTraffic::Wake, which no longer let it happen). Kept as a net: the
		// index walks down until it is in the array again.
		if (!Cars.IsValidIndex(Index))
		{
			if (!bLoggedCarsShrank)
			{
				bLoggedCarsShrank = true;
				GTP_TRACE(LogGTPTraffic, TEXT("traffic: the list of driven cars shrank by more than one in a tick (index %d, %d left), went on"), Index, Cars.Num());
			}
			continue;
		}
		AGTPVehicle* Car = Cars[Index];
		if (!IsValid(Car) || !Car->HasAIDriver())
		{
			if (IsValid(Car))
			{
				if (UGTPAIDriverComponent* Stolen = Car->FindComponentByClass<UGTPAIDriverComponent>())
				{
					Stolen->ReleaseJunctions();
				}
				// The player's car now: full detail again.
				UpdateDetail(Car, 0.f);
			}
			Cars.RemoveAt(Index);
			continue;
		}
		UGTPAIDriverComponent* AIDriver = Car->FindComponentByClass<UGTPAIDriverComponent>();
		// A wreck keeps its physics: no ghost switch on a car lying in a pile-up.
		if (bVehicleGhost && AIDriver && Player && !Car->IsWrecked())
		{
			const float ToPlayer = static_cast<float>(FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()));
			const bool bWantsGhost = GTPGhost::WantsGhost(AIDriver->IsGhost(), ToPlayer, GhostDistance, GhostWakeDistance);
			// A ghost slides through other cars: woken inside one, the solver threw both apart at
			// up to 128 m/s and one ended wrecked against a wall. It wakes once the place is clear.
			const AActor* Inside = AIDriver->IsGhost() && !bWantsGhost && bPoolSpawnCheck ? FindCarAt(Car, Car->GetActorTransform()) : nullptr;
			if (Inside)
			{
				GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s stays a ghost, inside %s"), *Car->GetName(), *Inside->GetName());
			}
			else
			{
				AIDriver->SetGhost(bWantsGhost);
			}
		}
		else if (!bVehicleGhost && AIDriver && AIDriver->IsGhost() && !Car->IsWrecked())
		{
			// A pooled car comes back as a ghost: without the ghost switch, physics on a tick later.
			AIDriver->SetGhost(false);
		}
		const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
		const FVector View = Viewer && Viewer->PlayerCameraManager ? Viewer->PlayerCameraManager->GetCameraLocation()
			: (Player ? Player->GetActorLocation() : Car->GetActorLocation());
		UpdateDetail(Car, static_cast<float>(FVector::Dist(View, Car->GetActorLocation())));
		const bool bFar = Player && FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) > MaxCarDistance;
		// Three back-offs that did not free it: taken away like a car stopped too long.
		const bool bStuck = AIDriver && (AIDriver->IsAtDeadEnd() || AIDriver->GetStoppedTime() > StuckSeconds || AIDriver->GetWedgeCount() >= 3);
		// Overturned (a ghost is always drawn upright): out of sight it goes at once; in sight the car puts itself back
		// on its wheels after a moment (AGTPVehicle::PutBackOnWheels).
		const bool bOverturned = AIDriver && !AIDriver->IsGhost() && Car->GetActorUpVector().Z < 0.7071;
		if ((bFar || bStuck || bOverturned) && !IsSeenByPlayer(Car->GetActorLocation()))
		{
			if (bOverturned)
			{
				LogOverturned(Car, Player, TEXT("taken away out of sight"));
			}
			RecycledFar += bFar ? 1 : 0;
			RecycledOverturned += !bFar && bOverturned ? 1 : 0;
			RecycledStuck += !bFar && !bOverturned ? 1 : 0;
			Recycle(Car);
		}
	}
	UpdateSignalHeads();
	UpdateReactDemo();
	UpdatePlayerShots(Player);
	UpdateStreetParked(Player);
	UpdateIncidents(Player);
	AIDrivers.RemoveAll([](const TWeakObjectPtr<UGTPAIDriverComponent>& Weak) { return !Weak.IsValid(); });

	// New cars out of sight: one a second when it means spawning an actor (no burst of spawns),
	// up to four a tick when the pool has cars to reuse (no spawn cost), so 40 cars fill in time.
	// After a jump, every cooled car of the pool at once, for as long as the others cool down: gate 67 read
	// "driven=0 of 22" in the tick GTARun moved the player 700 m, and the road filled four cars a tick after.
	const bool bRefill = bTrafficPool && GetWorld()->GetTimeSeconds() < PoolRefillUntil;
	if (Cars.Num() < WantedCars && (bRefill || GetWorld()->GetTimeSeconds() >= NextSpawnTime))
	{
		const int32 Tries = FMath::Min(WantedCars - Cars.Num(), bRefill ? WantedCars : (bTrafficPool && Pool.Num() > 0 ? 4 : 1));
		for (int32 Try = 0; Try < Tries && !(bRefill && Pool.Num() == 0); ++Try)
		{
			if (!SpawnCar() && bRefill)
			{
				break;
			}
		}
		NextSpawnTime = GetWorld()->GetTimeSeconds() + (bTrafficPool && Pool.Num() > 0 ? 0.f : 1.f);
	}
	if (bPlayerJumped)
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: the player moved %.0f m in one tick: %d car(s) taken away as far, %d driven again around the player (%d pooled)"),
			FVector::Dist2D(PlayerWasAt, PlayerJumpCheckAt) / 100.0, RecycledFar - RecycledFarBefore, Cars.Num(), Pool.Num());
	}
	// Last, the traffic as this tick leaves it (until 23/11 it was read between the cars taken away and the new ones).
	LogCars(Player);
}

void AGTPTrafficNetwork::LogBudget()
{
	// Every 5 s (the network ticks twice a second): CPU time of the traffic per frame and per driven car.
	if (++BudgetTicks < 10)
	{
		return;
	}
	const uint64 Frames = FMath::Max<uint64>(GFrameCounter - BudgetFrame, 1);
	int32 Ghosts = 0;
	for (const AGTPVehicle* Car : Cars)
	{
		const UGTPAIDriverComponent* Driver = IsValid(Car) ? Car->FindComponentByClass<UGTPAIDriverComponent>() : nullptr;
		Ghosts += Driver && Driver->IsGhost() ? 1 : 0;
	}
	const double FarSeconds = FarTraffic ? FarTraffic->TakeTickSeconds() : 0.0;
	const double PerFrame = 1000.0 / static_cast<double>(Frames);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic budget: %d driven (%d ghosts), %d pooled, %d far | drivers %.3f ms/frame (%.1f us per driver tick), network %.3f ms/frame, far %.3f ms/frame"),
		Cars.Num(), Ghosts, Pool.Num(), FarTraffic ? FarTraffic->Num() : 0,
		UGTPAIDriverComponent::TickSeconds * PerFrame,
		UGTPAIDriverComponent::TickCount > 0 ? UGTPAIDriverComponent::TickSeconds * 1e6 / UGTPAIDriverComponent::TickCount : 0.0,
		NetworkTickSeconds * PerFrame, FarSeconds * PerFrame);
	UGTPAIDriverComponent::TickSeconds = 0.0;
	UGTPAIDriverComponent::TickCount = 0;
	NetworkTickSeconds = 0.0;
	BudgetTicks = 0;
	BudgetFrame = GFrameCounter;
}

void AGTPTrafficNetwork::LogCars(const APawn* Player)
{
	// The legacy traffic's counterpart of "MassTraffic cars:" (GTPMass), what the gate measures by default: the driven
	// cars, how many move (over 1 m/s, upright) and how many stand, near the player too, and what holds the standing ones
	// (the nearest stop of their last decision, UGTPAIDriverComponent::GetLimit).
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextCarsLogTime || WantedCars <= 0)
	{
		return;
	}
	NextCarsLogTime = Now + 10.0;
	const FVector PlayerAt = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	int32 Driven = 0;
	int32 Ghosts = 0;
	int32 Moving = 0;
	int32 Stopped = 0;
	int32 Lying = 0;
	int32 NotDriving = 0;
	int32 Within100 = 0;
	int32 MovingWithin100 = 0;
	int32 StoppedWithin100 = 0;
	int32 Within200 = 0;
	// The ring where the gate judges the legacy traffic (100 to 150 m around the player).
	int32 Within150 = 0;
	int32 MovingWithin150 = 0;
	int32 UprightWithin150 = 0;
	int32 GhostsWithin150 = 0;
	int32 LongStopped = 0;
	int32 AtDeadEnd = 0;
	int32 Why[static_cast<int32>(GTPTrafficStop::ELimit::Num)] = {};
	double Nearest = TNumericLimits<double>::Max();
	double SpeedSum = 0.0;
	for (const AGTPVehicle* Car : Cars)
	{
		if (!IsValid(Car))
		{
			continue;
		}
		++Driven;
		const UGTPAIDriverComponent* Driver = Car->FindComponentByClass<UGTPAIDriverComponent>();
		const bool bGhostCar = Driver && Driver->IsGhost();
		const float Ms = bGhostCar ? Driver->GetGhostSpeed() : FMath::Abs(Car->GetSpeedKmh()) / 3.6f;
		const bool bUpright = Car->GetActorUpVector().Z > 0.7071;
		const bool bMoving = bUpright && Ms > 1.f;
		Ghosts += bGhostCar ? 1 : 0;
		Lying += bUpright ? 0 : 1;
		NotDriving += Driver && Driver->IsDriving() ? 0 : 1;
		Moving += bMoving ? 1 : 0;
		SpeedSum += Ms;
		if (bUpright && !bMoving)
		{
			++Stopped;
			++Why[static_cast<int32>(Driver ? Driver->GetLimit() : GTPTrafficStop::ELimit::None)];
			LongStopped += Driver && Driver->GetStoppedTime() > 20.f ? 1 : 0;
			AtDeadEnd += Driver && Driver->IsAtDeadEnd() ? 1 : 0;
		}
		if (Player)
		{
			const double Distance = FVector::Dist(Car->GetActorLocation(), PlayerAt);
			Nearest = FMath::Min(Nearest, Distance);
			Within200 += Distance < 20000.0 ? 1 : 0;
			if (Distance < 15000.0)
			{
				++Within150;
				MovingWithin150 += bMoving ? 1 : 0;
				UprightWithin150 += bUpright ? 1 : 0;
				GhostsWithin150 += bGhostCar ? 1 : 0;
			}
			if (Distance < 10000.0)
			{
				++Within100;
				MovingWithin100 += bMoving ? 1 : 0;
				StoppedWithin100 += bUpright && !bMoving ? 1 : 0;
			}
		}
	}
	for (const AGTPVehicle* Car : Cars)
	{
		// Each car the line counts lying, named, so a gate says which one, where and after what.
		if (IsValid(Car) && Car->GetActorUpVector().Z <= 0.7071)
		{
			LogOverturned(Car, Player, TEXT("counted lying"));
		}
	}
	const FString NearestText = Nearest < TNumericLimits<double>::Max() ? FString::Printf(TEXT("%.0f m"), Nearest / 100.0) : FString(TEXT("none"));
	// The far traffic's puppets drawn near the player: cars he sees too, with no body (they become driven cars at the ghost distance).
	const int32 FarWithin150 = Player && FarTraffic ? FarTraffic->CountDrawnWithin(15000.f) : 0;
	using GTPTrafficStop::ELimit;
	GTP_TRACE(LogGTPTraffic, TEXT("traffic cars t=%.0fs: driven=%d of %d wanted (ghosts=%d) moving=%d (over 1 m/s, upright: %.0f %%) stopped=%d lying=%d not driving (no route or no decisions)=%d mean speed=%.0f km/h | within 100 m=%d (moving %d, stopped %d) within 150 m=%d (moving %d, upright %d, ghosts %d; far puppets drawn there %d) within 200 m=%d nearest=%s | stopped for: a car ahead=%d, the queue along the lanes=%d, a person=%d, something else ahead=%d, a red light=%d, a junction in use=%d, held (reaction or stop asked)=%d, wedged=%d, nothing ahead=%d; stopped over 20 s=%d, at a dead end=%d | recycled so far: stuck=%d far=%d overturned=%d by the chase script or the police=%d, reused from the pool=%d"),
		Now, Driven, WantedCars, Ghosts, Moving, Driven > 0 ? 100.0 * Moving / Driven : 0.0, Stopped, Lying, NotDriving, Driven > 0 ? SpeedSum / Driven * 3.6 : 0.0,
		Within100, MovingWithin100, StoppedWithin100, Within150, MovingWithin150, UprightWithin150, GhostsWithin150, FarWithin150, Within200, *NearestText,
		Why[static_cast<int32>(ELimit::CarAhead)], Why[static_cast<int32>(ELimit::Queue)], Why[static_cast<int32>(ELimit::PersonAhead)],
		Why[static_cast<int32>(ELimit::OtherAhead)], Why[static_cast<int32>(ELimit::RedLight)], Why[static_cast<int32>(ELimit::Junction)],
		Why[static_cast<int32>(ELimit::Held)], Why[static_cast<int32>(ELimit::Wedged)], Why[static_cast<int32>(ELimit::None)],
		LongStopped, AtDeadEnd, RecycledStuck, RecycledFar, RecycledOverturned, RecycledAll - RecycledStuck - RecycledFar - RecycledOverturned,
		ReusedFromPool);
}

void AGTPTrafficNetwork::LogOverturned(const AGTPVehicle* Car, const APawn* Player, const TCHAR* What) const
{
	const UGTPAIDriverComponent* Driver = Car->FindComponentByClass<UGTPAIDriverComponent>();
	const USkeletalMeshComponent* Body = Car->GetMesh();
	const double Now = GetWorld()->GetTimeSeconds();
	const FString Distance = Player ? FString::Printf(TEXT("%.0f m"), FVector::Dist(Car->GetActorLocation(), Player->GetActorLocation()) / 100.0)
		: FString(TEXT("no player"));
	const FString Reaction = Driver ? FString(UTF8_TO_TCHAR(GTPTrafficReact::StateName(Driver->GetReactState()))) : FString(TEXT("none"));
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s overturned, %s: up %.2f at %s, %s%s, %.0f km/h, %s from the player, reaction %s, %.1f s after %s"),
		*Car->GetName(), What, Car->GetActorUpVector().Z, *Car->GetActorLocation().ToCompactString(),
		Body && Body->IsSimulatingPhysics() ? TEXT("simulated") : TEXT("kinematic"), Car->IsWrecked() ? TEXT(", wrecked") : TEXT(""),
		Car->GetSpeedKmh(), *Distance, *Reaction, Now - Car->GetLastPlacedTime(), Car->GetLastPlacedHow());
}

// --- Traffic lights (version 0.4) --------------------------------------------

float AGTPTrafficNetwork::GetLightTime() const
{
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) : 0.f;
}

void AGTPTrafficNetwork::BuildLights()
{
	FGTPTrafficLights Built;
	for (int32 Index = 0; Index < Lanes.Num(); ++Index)
	{
		const TArray<FVector>& Points = Lanes[Index].Points;
		if (Points.Num() >= 2 && Lanes[Index].ToJunction >= 0)
		{
			const FVector Dir = Points.Last() - Points[Points.Num() - 2];
			// Lane points are Unreal cm (Y = -north); the lights only compare headings, any frame works.
			Built.AddApproach(Index, Lanes[Index].ToJunction, FMath::Atan2(static_cast<float>(Dir.Y), static_cast<float>(Dir.X)));
		}
	}
	FGTPLightTimings Timings;
	Timings.Green = GreenSeconds;
	Timings.Amber = AmberSeconds;
	Timings.AllRed = AllRedSeconds;
	Built.Build(Timings);
	Lights = MoveTemp(Built);

	TMap<int32, int32> Count;
	for (int32 Index = 0; Index < Lanes.Num(); ++Index)
	{
		const int32 Group = Lights.GroupOf(Index);
		if (Group < 0)
		{
			continue;
		}
		const int32 Junction = Lanes[Index].ToJunction;
		const TArray<FVector>& Points = Lanes[Index].Points;
		JunctionCentres.FindOrAdd(Junction, FVector::ZeroVector) += Points.Last();
		Count.FindOrAdd(Junction) += 1;
		if (Group == 0 && !JunctionAxis.Contains(Junction))
		{
			const FVector Dir = Points.Last() - Points[Points.Num() - 2];
			JunctionAxis.Add(Junction, FMath::Atan2(static_cast<float>(Dir.Y), static_cast<float>(Dir.X)));
		}
	}
	for (TPair<int32, FVector>& Centre : JunctionCentres)
	{
		Centre.Value /= static_cast<double>(FMath::Max(Count[Centre.Key], 1));
	}

	int32 HeadsFlag = bShowSignalHeads ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("SignalHeads="), HeadsFlag);
	if (HeadsFlag == 0)
	{
		return;
	}
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!Sphere || !Cylinder || !Base)
	{
		GTP_TRACE(LogGTPTraffic, TEXT("traffic lights: engine basic shapes missing, no signal heads"));
		return;
	}
	// The project's traffic light, unless the stand-in is asked for (-SignalHeads=2) or it does not load.
	UStaticMesh* Body = nullptr;
	if (HeadsFlag != 2 && SignalHeadMesh.IsValid())
	{
		Body = Cast<UStaticMesh>(SignalHeadMesh.TryLoad());
	}
	float YawOffset = SignalHeadYawOffset;
	FParse::Value(FCommandLine::Get(), TEXT("SignalHeadYaw="), YawOffset);
	const FBox BodyBounds = Body ? Body->GetBoundingBox() : FBox(ForceInit);
	if (Body)
	{
		const FVector BodySize = BodyBounds.GetSize();
		GTP_TRACE(LogGTPTraffic, TEXT("traffic lights: signal heads are %s, %.0f x %.0f x %.0f cm (front at X=%.0f, top at Z=%.0f), its +X turned %.0f deg from facing the incoming cars, red lens %.0f cm below the top, then %.0f cm steps"),
			*SignalHeadMesh.ToString(), BodySize.X, BodySize.Y, BodySize.Z, BodyBounds.Max.X, BodyBounds.Max.Z, YawOffset,
			SignalLensBelowTop, SignalLensStep);
	}
	else
	{
		const FString Why = HeadsFlag == 2 ? FString(TEXT("-SignalHeads=2"))
			: SignalHeadMesh.IsValid() ? SignalHeadMesh.ToString() + TEXT(" not found") : FString(TEXT("no SignalHeadMesh"));
		GTP_TRACE(LogGTPTraffic, TEXT("traffic lights: signal heads are the stand-in spheres (%s)"), *Why);
	}
	// One head per approach, on the kerb side: a head 2.6 m right of every lane end stood on the
	// next lane of a multi-lane approach (gate 65 video: five heads in a row across the square,
	// driven through). A lane keeps its head only when no lane of the same light group, ending
	// within 15 m in the same direction, lies further to its right.
	auto EndOf = [this](int32 L, FVector& OutDir) -> FVector
	{
		const TArray<FVector>& P = Lanes[L].Points;
		OutDir = (P.Last() - P[P.Num() - 2]).GetSafeNormal2D();
		return P.Last();
	};
	TArray<int32> Signalled;
	for (int32 Index = 0; Index < Lanes.Num(); ++Index)
	{
		if (Lights.GroupOf(Index) >= 0 && Lanes[Index].Points.Num() >= 2)
		{
			Signalled.Add(Index);
		}
	}
	int32 Merged = 0;
	// A street light in the same folder as the head's mesh is the same light (SM_..._A, or its _Merged bake).
	SignalHeadFolder = Body ? FPaths::GetPath(Body->GetPathName()) : FString();
	// The heads' feet off every lane centre line (SignalHeadLaneClearance), the lanes near first by their bounds.
	const double LaneClearance = FMath::Max(0.0, static_cast<double>(SignalHeadLaneClearance));
	auto IsOffLanes = [this, LaneClearance](const FVector& Spot) -> bool
	{
		const FVector Flat(Spot.X, Spot.Y, 0.0);
		for (int32 L = 0; L < Lanes.Num(); ++L)
		{
			if (!LaneBounds.IsValidIndex(L) || !LaneBounds[L].bIsValid || !LaneBounds[L].ExpandBy(LaneClearance).IsInside(FVector2D(Spot.X, Spot.Y)))
			{
				continue;
			}
			const TArray<FVector>& Line = Lanes[L].Points;
			for (int32 K = 0; K + 1 < Line.Num(); ++K)
			{
				if (FMath::PointDistToSegment(Flat, FVector(Line[K].X, Line[K].Y, 0.0), FVector(Line[K + 1].X, Line[K + 1].Y, 0.0)) < LaneClearance)
				{
					return false;
				}
			}
		}
		return true;
	};
	int32 MovedRight = 0;
	int32 LeftOut = 0;
	for (int32 Index = 0; Index < Lanes.Num(); ++Index)
	{
		if (Lights.GroupOf(Index) < 0 || Lanes[Index].Points.Num() < 2)
		{
			continue;
		}
		FVector OwnDir;
		const FVector OwnEnd = EndOf(Index, OwnDir);
		const FVector OwnRight(-OwnDir.Y, OwnDir.X, 0.f);
		bool bInner = false;
		for (int32 Other : Signalled)
		{
			FVector OtherDir;
			const FVector OtherEnd = EndOf(Other, OtherDir);
			if (Other != Index && Lights.GroupOf(Other) == Lights.GroupOf(Index) && FVector::DotProduct(OwnDir, OtherDir) > 0.9f
				&& FVector::Dist2D(OwnEnd, OtherEnd) < 1500.f && FVector::DotProduct(OtherEnd - OwnEnd, OwnRight) > 100.f)
			{
				bInner = true;
				break;
			}
		}
		if (bInner)
		{
			++Merged;
			continue;
		}
		const TArray<FVector>& Points = Lanes[Index].Points;
		const FVector Dir = (Points.Last() - Points[Points.Num() - 2]).GetSafeNormal2D();
		// Right-hand side of the stop line, 3 m back from the lane end, 2.6 m to the right.
		const FVector Right(-Dir.Y, Dir.X, 0.f);
		FVector Foot = Points.Last() - Dir * 300.f + Right * 260.f;
		// Never on a lane: further right, 50 cm at a time, up to 3 m, else no head for this approach.
		bool bOffLanes = IsOffLanes(Foot);
		for (int32 Step = 1; !bOffLanes && Step <= 6; ++Step)
		{
			Foot += Right * 50.f;
			bOffLanes = IsOffLanes(Foot);
			MovedRight += bOffLanes ? 1 : 0;
		}
		if (!bOffLanes)
		{
			++LeftOut;
			continue;
		}
		FHitResult Ground;
		const FVector Top(Foot.X, Foot.Y, Foot.Z + 10000.f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPSignalGround), false, this);
		const bool bGrounded = GetWorld()->LineTraceSingleByChannel(Ground, Top, Top - FVector(0.f, 0.f, 20000.f), ECC_WorldStatic, Params);
		if (bGrounded)
		{
			Foot = Ground.ImpactPoint;
		}
		FSignalHead Head;
		Head.bGrounded = bGrounded;
		Head.Lane = Index;
		Head.Location = Foot;
		Head.FacingYaw = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(-Dir.Y, -Dir.X))) + YawOffset;
		UStaticMeshComponent* Pole = NewObject<UStaticMeshComponent>(this);
		UStaticMeshComponent* Lamp = NewObject<UStaticMeshComponent>(this);
		if (Body)
		{
			// Faces the incoming cars: its +X points back up the lane.
			const FRotator Facing(0.f, Head.FacingYaw, 0.f);
			Pole->SetStaticMesh(Body);
			Pole->SetWorldRotation(Facing);
			Head.BodyOffset = FVector::ZeroVector;
			for (int32 LensStep = 0; LensStep < 3; ++LensStep)
			{
				// Red, amber, green from the top down, 2 cm in front of the mesh's front face.
				const FVector Local(BodyBounds.Max.X + 2.f, BodyBounds.GetCenter().Y,
					BodyBounds.Max.Z - SignalLensBelowTop - SignalLensStep * static_cast<float>(LensStep));
				Head.LensOffset[LensStep] = Facing.RotateVector(Local);
			}
			// The cylinder's axis laid along the facing: a flat disc seen from the road.
			Lamp->SetStaticMesh(Cylinder);
			Lamp->SetWorldRotation(FRotator(90.f, Facing.Yaw, 0.f));
			Lamp->SetWorldScale3D(FVector(SignalLensSize / 100.f, SignalLensSize / 100.f, 0.03f));
		}
		else
		{
			Pole->SetStaticMesh(Cylinder);
			Pole->SetWorldScale3D(FVector(0.1f, 0.1f, 3.f));
			Pole->SetMaterial(0, Base);
			Head.BodyOffset = FVector(0.f, 0.f, 150.f);
			for (FVector& Lens : Head.LensOffset)
			{
				Lens = FVector(0.f, 0.f, 320.f);
			}
			Lamp->SetStaticMesh(Sphere);
			Lamp->SetWorldScale3D(FVector(0.35f));
		}
		Pole->SetWorldLocation(Foot + Head.BodyOffset);
		Lamp->SetWorldLocation(Foot + Head.LensOffset[0]);
		for (UStaticMeshComponent* Part : {Pole, Lamp})
		{
			Part->SetMobility(EComponentMobility::Movable);
			Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Part->SetCastShadow(false);
			Part->SetGenerateOverlapEvents(false);
			Part->RegisterComponent();
		}
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, this);
		Lamp->SetMaterial(0, Material);
		Head.Lamp = Lamp;
		Head.Pole = Pole;
		Head.Material = Material;
		SignalHeads.Add(Head);
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic lights: %d signal heads, %d inner lanes share the kerb-side head of their approach, %d moved further right off another lane, %d left out (on a lane up to 3 m further right)"),
		SignalHeads.Num(), Merged, MovedRight, LeftOut);
}

void AGTPTrafficNetwork::BorrowStreetLight(FSignalHead& Head, UStaticMeshComponent& Lamp, UStaticMeshComponent& Pole, int32 Lens)
{
	if (SignalHeadFolder.IsEmpty() || SignalBorrowRadius <= 0.f)
	{
		return;
	}
	// The street lights are instances with collision (tools/city/unreal/place_props.py): an overlap gives each one's index.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPSignalBorrow), false, this);
	GetWorld()->OverlapMultiByObjectType(Overlaps, Head.Location + FVector(0.f, 0.f, 150.f), FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionShape::MakeSphere(SignalBorrowRadius), Params);
	const UInstancedStaticMeshComponent* Best = nullptr;
	FTransform BestAt;
	uint64 BestKey = 0;
	double BestDistance = TNumericLimits<double>::Max();
	bool bBesideOther = false;
	for (const FOverlapResult& Each : Overlaps)
	{
		const UInstancedStaticMeshComponent* Street = Cast<UInstancedStaticMeshComponent>(Each.GetComponent());
		const UStaticMesh* Mesh = Street ? Street->GetStaticMesh() : nullptr;
		FTransform At;
		if (!Mesh || FPaths::GetPath(Mesh->GetPathName()) != SignalHeadFolder || !Street->GetInstanceTransform(Each.ItemIndex, At, true))
		{
			continue;
		}
		const uint64 Key = (static_cast<uint64>(Street->GetUniqueID()) << 32) | static_cast<uint32>(Each.ItemIndex);
		// Facing the same cars (its front is +X, as the head's), and not taken by another head.
		if (FMath::Abs(FRotator::NormalizeAxis(At.Rotator().Yaw - Head.FacingYaw)) > 60.0 || BorrowedStreetLights.Contains(Key))
		{
			bBesideOther = true;
			continue;
		}
		const double Distance = FVector::DistSquared2D(At.GetLocation(), Head.Location);
		if (Distance < BestDistance)
		{
			Best = Street;
			BestAt = At;
			BestKey = Key;
			BestDistance = Distance;
		}
	}
	if (!Best)
	{
		SignalHeadsBesideOther += bBesideOther ? 1 : 0;
		return;
	}
	BorrowedStreetLights.Add(BestKey);
	++SignalHeadsBorrowed;
	const FBox Bounds = Best->GetStaticMesh()->GetBoundingBox();
	const FVector Was = Head.Location;
	Head.Location = BestAt.GetLocation();
	Head.FacingYaw = static_cast<float>(BestAt.Rotator().Yaw);
	for (int32 Step = 0; Step < 3; ++Step)
	{
		// As BuildLights puts them on the head's own mesh, in the street light's frame (its yaw and scale).
		const FVector Local(Bounds.Max.X + 2.f, Bounds.GetCenter().Y, Bounds.Max.Z - SignalLensBelowTop - SignalLensStep * static_cast<float>(Step));
		Head.LensOffset[Step] = BestAt.TransformPosition(Local) - Head.Location;
	}
	Lamp.SetWorldRotation(FRotator(90.f, Head.FacingYaw, 0.f));
	Lamp.SetWorldLocation(Head.Location + Head.LensOffset[FMath::Clamp(Lens, 0, 2)]);
	Pole.SetStaticMesh(nullptr);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic lights: the head of lane %d drawn on the street light %s %d, %.1f m from its own spot (%d so far, %d beside a light facing other cars)"),
		Head.Lane, *GetNameSafe(Best->GetOwner()), static_cast<int32>(BestKey & 0xffffffffull), FVector::Dist2D(Was, Head.Location) / 100.0, SignalHeadsBorrowed,
		SignalHeadsBesideOther);
}

void AGTPTrafficNetwork::UpdateSignalHeads()
{
	if (SignalHeads.Num() == 0)
	{
		return;
	}
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();
	const FVector View = Viewer && Viewer->PlayerCameraManager ? Viewer->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const float Now = GetLightTime();
	// Lens of each light on the head: red (and no light) at the top, amber, green at the bottom.
	auto LensOf = [](EGTPLight State) { return State == EGTPLight::Green ? 2 : State == EGTPLight::Amber ? 1 : 0; };
	for (FSignalHead& Head : SignalHeads)
	{
		UStaticMeshComponent* Lamp = Head.Lamp.Get();
		UStaticMeshComponent* Pole = Head.Pole.Get();
		if (!Lamp || !Pole)
		{
			continue;
		}
		const bool bVisible = SignalDrawDistance <= 0.f || FVector::DistSquared2D(View, Head.Location) < FMath::Square(SignalDrawDistance);
		if (bVisible && !Head.bGrounded)
		{
			// Streamed quarter: the cell under this head was not loaded at start. Find the ground
			// now that the head is about to be seen (one trace, then never again).
			FHitResult Ground;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPSignalGround), false, this);
			const FVector Top(Head.Location.X, Head.Location.Y, Head.Location.Z + 10000.f);
			if (GetWorld()->LineTraceSingleByChannel(Ground, Top, Top - FVector(0.f, 0.f, 20000.f), ECC_WorldStatic, Params))
			{
				Head.bGrounded = true;
				Head.Location = Ground.ImpactPoint;
				Pole->SetWorldLocation(Head.Location + Head.BodyOffset);
				Lamp->SetWorldLocation(Head.Location + Head.LensOffset[LensOf(Head.Shown)]);
			}
		}
		// Hidden until grounded: no lamp floating or buried while its cell streams in. And hidden while the view stands
		// at the head itself: its parts have no collision, so a camera's own sweeps do not find them (gate 78: the chase
		// camera stood in a signal head at a junction 40 s into the film, the car held up beside it).
		const double CameraGapSquared = FMath::Min(FVector::DistSquared2D(View, Head.Location), FVector::DistSquared2D(View, Lamp->GetComponentLocation()));
		const bool bAtCamera = CameraGapSquared < FMath::Square(static_cast<double>(SignalCameraClearance)) && View.Z < Head.Location.Z + 700.0;
		const bool bDrawn = bVisible && Head.bGrounded && !bAtCamera;
		if (bDrawn && !Head.bLookedForStreetLight)
		{
			// Once, now that the cells around it are loaded: the details layer's light there, if any, carries its lenses.
			Head.bLookedForStreetLight = true;
			BorrowStreetLight(Head, *Lamp, *Pole, LensOf(Head.Shown));
		}
		if (Lamp->IsVisible() != bDrawn)
		{
			Lamp->SetVisibility(bDrawn);
			Pole->SetVisibility(bDrawn);
		}
		const EGTPLight Light = Lights.LightFor(Head.Lane, Now);
		if (Light != Head.Shown)
		{
			Head.Shown = Light;
			Lamp->SetWorldLocation(Head.Location + Head.LensOffset[LensOf(Light)]);
			if (UMaterialInstanceDynamic* Material = Head.Material.Get())
			{
				const FLinearColor Colour = Light == EGTPLight::Green ? FLinearColor(0.f, 1.f, 0.25f)
					: Light == EGTPLight::Amber ? FLinearColor(1.f, 0.55f, 0.f) : FLinearColor(1.f, 0.02f, 0.f);
				Material->SetVectorParameterValue(TEXT("Color"), Colour);
			}
		}
	}
}

bool AGTPTrafficNetwork::IsWalkAllowed(const FVector& Location, const FVector& WalkDirection, float JunctionRadiusCm) const
{
	if (!bTrafficLights)
	{
		return true;
	}
	const int32* Nearest = nullptr;
	double Best = FMath::Square(static_cast<double>(JunctionRadiusCm));
	for (const TPair<int32, FVector>& Centre : JunctionCentres)
	{
		const double D = FVector::DistSquared2D(Centre.Value, Location);
		if (D < Best)
		{
			Best = D;
			Nearest = &Centre.Key;
		}
	}
	if (!Nearest)
	{
		return true;
	}
	const float* Axis = JunctionAxis.Find(*Nearest);
	if (!Axis)
	{
		return true;
	}
	// Walking along group 0's axis crosses group 1's roads, and the other way round.
	const float Walk = FMath::Atan2(static_cast<float>(WalkDirection.Y), static_cast<float>(WalkDirection.X));
	const int32 Crossed = FMath::Abs(FMath::Cos(Walk - *Axis)) >= 0.7071f ? 1 : 0;
	return Lights.WalkAllowed(*Nearest, Crossed, GetLightTime());
}

float AGTPTrafficNetwork::GapAlongLanes(const UGTPAIDriverComponent* Driver, float MaxMetres) const
{
	const int32 Lane = Driver ? Driver->GetLane() : INDEX_NONE;
	if (!Lanes.IsValidIndex(Lane))
	{
		return -1.f;
	}
	auto Length = [this](int32 L, int32 From, int32 To)
	{
		const TArray<FVector>& P = Lanes[L].Points;
		double Sum = 0.0;
		for (int32 K = FMath::Max(From, 0); K < To && K + 1 < P.Num(); ++K)
		{
			Sum += FVector::Dist2D(P[K], P[K + 1]);
		}
		return static_cast<float>(Sum) * CmToM;
	};
	const int32 Next = Driver->GetNextLane();
	const int32 Mine = Driver->GetLanePoint();
	float Best = -1.f;
	for (const TWeakObjectPtr<UGTPAIDriverComponent>& Weak : AIDrivers)
	{
		const UGTPAIDriverComponent* Other = Weak.Get();
		if (!Other || Other == Driver || !Other->IsComponentTickEnabled())
		{
			continue;
		}
		float Along = -1.f;
		if (Other->GetLane() == Lane && Other->GetLanePoint() > Mine)
		{
			Along = Length(Lane, Mine, Other->GetLanePoint());
		}
		else if (Next != INDEX_NONE && Other->GetLane() == Next)
		{
			Along = Length(Lane, Mine, Lanes[Lane].Points.Num() - 1) + Length(Next, 0, Other->GetLanePoint());
		}
		if (Along >= 0.f && Along < MaxMetres)
		{
			// Lane points are car centres: keep a car length between them.
			const float Gap = FMath::Max(0.f, Along - 4.2f);
			if (Best < 0.f || Gap < Best)
			{
				Best = Gap;
			}
		}
	}
	return Best;
}

// --- The player's weapon, read when the threat system is off ------------------

namespace
{
	/** The player's weapon: on the character, or on the driver seated in the player's car (drive-by). */
	const UGTPWeaponComponent* PlayerWeapon(const APawn* Player)
	{
		if (const AGTPVehicle* Car = Cast<AGTPVehicle>(Player))
		{
			const ACharacter* Seated = Car->GetDriver();
			return Seated ? Seated->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
		}
		return Player ? Player->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
	}
}

bool AGTPTrafficNetwork::GetPlayerAim(FVector& OutOrigin, FVector& OutDirection) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const UGTPWeaponComponent* Weapon = PlayerWeapon(Player);
	if (!Weapon || !Weapon->IsAiming() || !Player->GetController())
	{
		return false;
	}
	OutOrigin = Player->GetActorLocation();
	OutDirection = Player->GetController()->GetControlRotation().Vector();
	return true;
}

void AGTPTrafficNetwork::UpdatePlayerShots(const APawn* Player)
{
	// A magazine that went down since the last tick: shots were fired (a reload raises it).
	const UGTPWeaponComponent* Weapon = PlayerWeapon(Player);
	if (!Weapon)
	{
		LastPlayerMagazine = -1;
		return;
	}
	const int32 Magazine = Weapon->GetMagazine();
	if (LastPlayerMagazine >= 0 && Magazine < LastPlayerMagazine)
	{
		++PlayerShotSerial;
		PlayerShotFrom = Player->GetActorLocation();
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: player fired %d shot(s)"), LastPlayerMagazine - Magazine);
	}
	LastPlayerMagazine = Magazine;
}

// --- Real cars parked along the streets, to steal --------------------------------

void AGTPTrafficNetwork::UpdateStreetParked(const APawn* Player)
{
	if (StreetParkedWanted < 0)
	{
		int32 Count = NumStreetParked;
		FParse::Value(FCommandLine::Get(), TEXT("StreetParked="), Count);
		StreetParkedWanted = FMath::Max(0, Count);
		GTP_TRACE(LogGTPTraffic, TEXT("traffic: %d street parked cars to steal (-StreetParked=N)"), StreetParkedWanted);
	}
	const int32 Wanted = StreetParkedWanted;
	if (!Player || Wanted == 0 || Lanes.Num() == 0)
	{
		return;
	}
	const FVector PlayerLocation = Player->GetActorLocation();
	// The traffic cars waiting behind something still: the longest wait behind each thing.
	struct FHeldUp
	{
		float Seconds = 0.f;
		const AGTPVehicle* Waiter = nullptr;
	};
	TMap<const AActor*, FHeldUp> HeldUp;
	if (StreetParked.Num() > 0)
	{
		for (const AGTPVehicle* Driven : Cars)
		{
			const UGTPAIDriverComponent* WaitingDriver = IsValid(Driven) ? Driven->FindComponentByClass<UGTPAIDriverComponent>() : nullptr;
			float Waited = 0.f;
			const AActor* Holder = WaitingDriver ? WaitingDriver->GetWaitingFor(Waited) : nullptr;
			if (Holder)
			{
				FHeldUp& Longest = HeldUp.FindOrAdd(Holder);
				if (Waited > Longest.Seconds)
				{
					Longest.Seconds = Waited;
					Longest.Waiter = Driven;
				}
			}
		}
	}
	for (int32 Index = StreetParked.Num() - 1; Index >= 0; --Index)
	{
		AGTPVehicle* Car = StreetParked[Index].Car.Get();
		if (!IsValid(Car))
		{
			StreetParked.RemoveAt(Index);
			continue;
		}
		// Taken by someone, or wrecked: not ours to manage any more.
		if (Car->GetDriver() != nullptr || Car->GetVehicleState() != EGTPVehicleState::Parked || Car->IsWrecked())
		{
			Car->SetParkedAsleep(false);
			StreetParked.RemoveAt(Index);
			continue;
		}
		// In the way of the traffic (StreetParkedBlockSeconds): taken away out of sight, the keeper puts one elsewhere.
		const FHeldUp* Held = StreetParked[Index].bOnRequest ? nullptr : HeldUp.Find(Car);
		if (Held && Held->Seconds >= StreetParkedBlockSeconds)
		{
			if (!IsSeenByPlayer(Car->GetActorLocation()))
			{
				++StreetParkedTakenForBlocking;
				GTP_TRACE(LogGTPTraffic, TEXT("traffic: street parked %s taken away out of sight, in the way: %s waited %.0f s behind it (%d so far)"),
					*Car->GetName(), *GetNameSafe(Held->Waiter), Held->Seconds, StreetParkedTakenForBlocking);
				Car->Destroy();
				StreetParked.RemoveAt(Index);
				continue;
			}
			if (!StreetParked[Index].bBlockLogged)
			{
				StreetParked[Index].bBlockLogged = true;
				GTP_TRACE(LogGTPTraffic, TEXT("traffic: street parked %s in the way (%s waited %.0f s behind it), kept while in view"),
					*Car->GetName(), *GetNameSafe(Held->Waiter), Held->Seconds);
			}
		}
		const float Distance = static_cast<float>(FVector::Dist2D(Car->GetActorLocation(), PlayerLocation));
		bool bAsleep = Distance > StreetParkedWakeDistance;
		if (!bAsleep && Car->IsParkedAsleep())
		{
			// Waking inside another car (traffic brushing a kinematic parked car) makes Chaos push
			// the two apart at hundreds of km/h: stays asleep until the other one has gone.
			if (const AActor* Inside = FindCarAt(Car, Car->GetActorTransform()))
			{
				bAsleep = true;
				if (!StreetParked[Index].bWakeBlocked)
				{
					GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s stays asleep, %s stands in it"), *Car->GetName(), *Inside->GetName());
				}
			}
			StreetParked[Index].bWakeBlocked = bAsleep;
		}
		Car->SetParkedAsleep(bAsleep);
		if (Distance > StreetParkedRemoveDistance && !IsSeenByPlayer(Car->GetActorLocation()))
		{
			Car->Destroy();
			StreetParked.RemoveAt(Index);
		}
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (StreetParked.Num() >= Wanted || Now < NextStreetParkedTime)
	{
		return;
	}
	// One a second, out of sight, 50 to 110 m away: a point of a lane in that ring (a random
	// direction mostly fell inside building blocks, with no lane within 15 m: 17 misses in a run).
	NextStreetParkedTime = Now + 1.0;
	LanesInRing(PlayerLocation, 6000.f, 9500.f, StreetParkedLanes);
	if (StreetParkedLanes.Num() == 0)
	{
		return;
	}
	FVector Near = PlayerLocation;
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const TArray<FVector>& Points = Lanes[StreetParkedLanes[Random.RandRange(0, StreetParkedLanes.Num() - 1)]].Points;
		const FVector& Point = Points[Random.RandRange(0, Points.Num() - 1)];
		const double Distance = FVector::Dist2D(Point, PlayerLocation);
		if (Distance >= 5000.0 && Distance <= 11000.0)
		{
			Near = Point;
			break;
		}
	}
	if (Near == PlayerLocation)
	{
		return;
	}
	AGTPVehicle* Car = SpawnParkedVehicle(TEXT("GTPStreetParked"), Near, 1500.f, nullptr, true);
	if (!Car)
	{
		++StreetParkedMisses;
		return;
	}
	if (FVector::Dist2D(Car->GetActorLocation(), PlayerLocation) < 4000.f || IsSeenByPlayer(Car->GetActorLocation()))
	{
		// Popped up close or in view: no.
		Car->Destroy();
		return;
	}
	Car->bLocked = Random.FRand() < StreetParkedLockedShare;
	Car->bAlarm = Car->bLocked && Random.FRand() < StreetParkedAlarmShare;
	Car->SetParkedAsleep(true);
	StreetParked.Add({Car, Now});
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: street parked %s%s%s (%d/%d, %d spots missed so far)"), *Car->GetName(), Car->bLocked ? TEXT(" locked") : TEXT(""),
		Car->bAlarm ? TEXT(" alarm") : TEXT(""), StreetParked.Num(), Wanted, StreetParkedMisses);
}

// --- Crashes between traffic cars (GTPIncidentLogic.h) ----------------------------

void AGTPTrafficNetwork::UpdateIncidents(const APawn* Player)
{
	for (int32 Index = IncidentCars.Num() - 1; Index >= 0; --Index)
	{
		AGTPVehicle* Car = IncidentCars[Index].Get();
		if (!IsValid(Car) || Car->HasAIDriver() || (Car->GetDriver() && Car->GetDriver()->IsPlayerControlled()))
		{
			IncidentCars.RemoveAt(Index);
			continue;
		}
		// Left in the street (the driver ran): taken away far out of sight.
		if (!Car->GetDriver() && Player && FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) > StreetParkedRemoveDistance
			&& !IsSeenByPlayer(Car->GetActorLocation()))
		{
			Car->Destroy();
			IncidentCars.RemoveAt(Index);
		}
	}
}

void AGTPTrafficNetwork::NotifyTrafficCrash(AGTPVehicle* A, AGTPVehicle* B, float SpeedChange)
{
	static const bool bIncidents = []
	{
		int32 Switch = 1;
		FParse::Value(FCommandLine::Get(), TEXT("TrafficIncidents="), Switch);
		return Switch != 0;
	}();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!bIncidents || !A || !B || !Player || SpeedChange < IncidentMinSpeedChange || IncidentCars.Num() >= MaxIncidents * 2
		|| !A->HasAIDriver() || !B->HasAIDriver() || IncidentCars.Contains(A) || IncidentCars.Contains(B)
		|| FVector::Dist2D(A->GetActorLocation(), Player->GetActorLocation()) > IncidentRange)
	{
		return;
	}
	UGTPAIDriverComponent* DriverA = A->FindComponentByClass<UGTPAIDriverComponent>();
	UGTPAIDriverComponent* DriverB = B->FindComponentByClass<UGTPAIDriverComponent>();
	ACharacter* PersonA = A->GetDriver();
	ACharacter* PersonB = B->GetDriver();
	// Traffic of this network only: police and mission drivers have their own rules.
	if (A->ActorHasTag(TEXT("GTPPolice")) || B->ActorHasTag(TEXT("GTPPolice")) || !DriverA || !DriverB
		|| DriverA->GetNetwork() != this || DriverB->GetNetwork() != this)
	{
		return;
	}
	if (DriverA->IsGhost() || DriverB->IsGhost() || !PersonA || !PersonB
		|| !Cast<AAIController>(PersonA->GetController()) || !Cast<AAIController>(PersonB->GetController()))
	{
		return;
	}
	const bool bTimidA = DriverA->GetTemperament() == GTPTrafficReact::ETemperament::Timid;
	const bool bTimidB = DriverB->GetTemperament() == GTPTrafficReact::ETemperament::Timid;
	const GTPIncident::FSettings IncidentSettings;
	const GTPIncident::EOutcome Outcome = GTPIncident::PickOutcome(Random.FRand(), SpeedChange, bTimidA || bTimidB, IncidentSettings);
	// The one going faster hit the other.
	const bool bAStruck = FMath::Abs(A->GetSpeedKmh()) >= FMath::Abs(B->GetSpeedKmh());
	AGTPVehicle* Striker = bAStruck ? A : B;
	AGTPVehicle* Struck = bAStruck ? B : A;
	const FVector Where = (A->GetActorLocation() + B->GetActorLocation()) * 0.5;
	GTP_TRACE(LogGTPTraffic, TEXT("traffic incident: %s and %s crashed (%.0f cm/s): %s"), *A->GetName(), *B->GetName(), SpeedChange,
		UTF8_TO_TCHAR(GTPIncident::OutcomeName(Outcome)));
	switch (Outcome)
	{
	case GTPIncident::EOutcome::DriveOn:
		// Shaken, a blast of the horn, and on.
		DriverA->InjectReaction(false, false, true);
		DriverB->InjectReaction(false, false, true);
		return;
	case GTPIncident::EOutcome::HitAndRun:
	{
		Striker->FindComponentByClass<UGTPAIDriverComponent>()->InjectReaction(false, true, false);
		UGTPTrafficArgueComponent::Start(Struck->GetDriver(), Struck, nullptr, this);
		IncidentCars.Add(Struck);
		return;
	}
	case GTPIncident::EOutcome::FleeOnFoot:
		if (SpeedChange >= IncidentSettings.PanicSpeedChange)
		{
			A->AIGetOut(true, Where);
			B->AIGetOut(true, Where);
		}
		else
		{
			// The timid one runs, the other gets out and shouts after them.
			AGTPVehicle* Runner = bTimidA ? A : B;
			AGTPVehicle* Stayer = Runner == A ? B : A;
			ACharacter* RunnerPerson = Runner->GetDriver();
			Runner->AIGetOut(true, Where);
			UGTPTrafficArgueComponent::Start(Stayer->GetDriver(), Stayer, RunnerPerson, this);
		}
		IncidentCars.Add(A);
		IncidentCars.Add(B);
		return;
	case GTPIncident::EOutcome::Argue:
		UGTPTrafficArgueComponent::Start(PersonA, A, PersonB, this);
		UGTPTrafficArgueComponent::Start(PersonB, B, PersonA, this);
		IncidentCars.Add(A);
		IncidentCars.Add(B);
		return;
	}
}

bool AGTPTrafficNetwork::ReclaimCar(AGTPVehicle* Car, ACharacter* Driver)
{
	if (!IsValid(Car) || !IsValid(Driver) || Car->IsWrecked() || Car->GetDriver() || Car->GetVehicleState() != EGTPVehicleState::Parked)
	{
		return false;
	}
	UGTPAIDriverComponent* AIDriver = Car->FindComponentByClass<UGTPAIDriverComponent>();
	int32 Lane = INDEX_NONE, Point = 0;
	if (!AIDriver || !FindNearestLane(Car->GetActorLocation(), static_cast<float>(Car->GetActorRotation().Yaw), 800.f, Lane, Point))
	{
		return false;
	}
	Car->SeatOccupant(Driver);
	if (!Car->HasAIDriver())
	{
		return false;
	}
	AIDriver->Start(this, Lane, Point);
	Cars.AddUnique(Car);
	IncidentCars.Remove(Car);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic incident: %s back in %s, drives on from lane %d"), *Driver->GetName(), *Car->GetName(), Lane);
	return true;
}

// --- MassTraffic switch (module GTPMass) -------------------------------------------

bool AGTPTrafficNetwork::IsMassTrafficRequested()
{
	FString Mode;
	return FParse::Value(FCommandLine::Get(), TEXT("Traffic="), Mode) && Mode.Equals(TEXT("mass"), ESearchCase::IgnoreCase);
}

bool AGTPTrafficNetwork::IsMassTrafficWanted(const UWorld* World)
{
	if (!World || !IsMassTrafficRequested() || !FModuleManager::Get().IsModuleLoaded(TEXT("MassTraffic")))
	{
		return false;
	}
	static const FName SpawnerTag(TEXT("GTPMassTraffic"));
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(SpawnerTag))
		{
			return true;
		}
	}
	return false;
}

AGTPVehicle* AGTPTrafficNetwork::SpawnStreetParkedNear(const FVector& Near, float RadiusCm, bool bLocked, bool bAlarm)
{
	// Widens the search twice before giving up (a scripted run must not wait on one bad spot).
	AGTPVehicle* Car = nullptr;
	for (float Radius = RadiusCm; !Car && Radius <= RadiusCm * 4.f; Radius *= 2.f)
	{
		Car = SpawnParkedVehicle(TEXT("GTPStreetParked"), Near, Radius, nullptr, Radius < RadiusCm * 4.f);
	}
	if (!Car)
	{
		return nullptr;
	}
	Car->bLocked = bLocked;
	Car->bAlarm = bAlarm;
	FStreetParked Placed;
	Placed.Car = Car;
	Placed.Born = GetWorld()->GetTimeSeconds();
	Placed.bOnRequest = true;
	StreetParked.Add(Placed);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: street parked %s placed on request%s%s"), *Car->GetName(), bLocked ? TEXT(" locked") : TEXT(""),
		bAlarm ? TEXT(" alarm") : TEXT(""));
	return Car;
}
