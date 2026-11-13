#include "GTPTraffic.h"

#include "GTPDeterminism.h"
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
#include "Misc/ScopeExit.h"
#include "CoreGlobals.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "AIController.h"
#include "CollisionQueryParams.h"
#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
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
	if (!InNetwork || !InNetwork->FindNearestLane(Location, YawDegrees, MaxDistanceCm, Lane, Point))
	{
		return false;
	}
	Network = InNetwork;
	Route.Reset();
	Route.Add(Lane);
	Closest = Point;
	Plan();
	return true;
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
	while (AheadM < RouteAheadM)
	{
		const int32 Next = bHasGoal ? Net->PickNextToward(Route.Last(), Goal) : Net->PickNext(Route.Last(), GTPDeterminism::IsOn() ? &OwnRandom : nullptr);
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

bool FGTPLaneFollower::Step(const FVector& Location, float YawDegrees, float SpeedKmh, float MaxSpeedKmh, float ObstacleMetres, FGTPDriveCommand& Out)
{
	AGTPTrafficNetwork* Net = Network.Get();
	if (!Net || Route.Num() == 0)
	{
		return false;
	}
	const float Speed = SpeedKmh / 3.6f;
	const float LaneSpeed = (bIgnoreLaneLimit ? MaxSpeedKmh : FMath::Min(Net->GetLanes()[Route[0]].SpeedKmh, MaxSpeedKmh)) / 3.6f;
	Out = GTPDrive::Decide(Path, static_cast<float>(Location.X) * CmToM, static_cast<float>(Location.Y) * CmToM,
		FMath::DegreesToRadians(YawDegrees), Speed, LaneSpeed, ObstacleMetres, Settings, Closest);
	if (Closest >= CurrentLaneLength && Route.Num() > 1)
	{
		Closest -= CurrentLaneLength;
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
	GhostSpeed = 0.f;
	GhostAlong = 0.f;
	bAtDeadEnd = false;
	PlanRoute();
	SetComponentTickEnabled(true);
}

double UGTPAIDriverComponent::TickSeconds = 0.0;
int32 UGTPAIDriverComponent::TickCount = 0;

void UGTPAIDriverComponent::PlaceGhost(int32 Lane, int32 Point, const FVector& Location, float YawDegrees, float SpeedMs)
{
	AGTPVehicle* Vehicle = Cast<AGTPVehicle>(GetOwner());
	if (!Vehicle || !Network)
	{
		return;
	}
	SetGhost(true);
	Vehicle->SetActorLocationAndRotation(Location + FVector(0.f, 0.f, GhostRideHeight), FRotator(0.f, YawDegrees, 0.f), false, nullptr,
		ETeleportType::TeleportPhysics);
	Vehicle->NotePlaced(TEXT("placed as ghost"));
	Start(Network, Lane, Point);
	GhostSpeed = FMath::Max(0.f, SpeedMs);
	GhostAlong = 0.f;
	GhostGroundZ = static_cast<float>(Location.Z);
	GhostGroundAge = 1.f;
}

void UGTPAIDriverComponent::SetDecisionInterval(float Seconds)
{
	Seconds = FMath::Max(0.f, Seconds);
	if (!FMath::IsNearlyEqual(PrimaryComponentTick.TickInterval, Seconds))
	{
		SetComponentTickInterval(Seconds);
	}
}

bool UGTPAIDriverComponent::FindGround(const AGTPVehicle* Vehicle, const FVector& Location, float& OutZ) const
{
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPTrafficGhostGround), false, Vehicle);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Location + FVector(0.f, 0.f, 300.f), Location - FVector(0.f, 0.f, 1000.f), Objects, Params))
	{
		return false;
	}
	OutZ = static_cast<float>(Hit.ImpactPoint.Z);
	return true;
}

void UGTPAIDriverComponent::SetGhost(bool bInGhost)
{
	AGTPVehicle* Vehicle = Cast<AGTPVehicle>(GetOwner());
	if (bGhost == bInGhost || !Vehicle)
	{
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
		float GroundZ = 0.f;
		GhostRideHeight = FindGround(Vehicle, Location, GroundZ) ? static_cast<float>(Location.Z) - GroundZ : 40.f;
		GhostGroundZ = static_cast<float>(Location.Z) - GhostRideHeight;
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
	const GTPTrafficReact::FSettings& Tuning = Network->ReactSettings;
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
	In.bHitByPlayer = Vehicle->ConsumeHitByPlayer();
	In.bAimedAt |= InjectAimLeft > 0.f;
	InjectAimLeft -= DeltaTime;
	In.bShotNear |= bInjectShot;
	In.bHitByPlayer |= bInjectRammed;
	bInjectShot = bInjectRammed = false;

	const GTPTrafficReact::EState Before = React.State;
	const GTPTrafficReact::FOutput Out = GTPTrafficReact::Step(React, In, Tuning);
	if (Out.bChanged)
	{
		if (React.State == GTPTrafficReact::EState::Bypass)
		{
			BypassBlocker = ProbeHit;
		}
		const FString From(UTF8_TO_TCHAR(GTPTrafficReact::StateName(Before)));
		const FString To(UTF8_TO_TCHAR(GTPTrafficReact::StateName(React.State)));
		GTP_TRACE(LogGTPTraffic, TEXT("traffic react: %s %s -> %s (ahead %s %.1f m, %.0f km/h)"), *Vehicle->GetName(), *From, *To,
			*GetNameSafe(ProbeHit), ProbeM, In.Speed * 3.6f);
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
	const float Queue = Reaction.bIgnoreBlocker ? -1.f : Network->GapAlongLanes(this, 40.f);
	if (Queue >= 0.f && (Obstacle < 0.f || Queue < Obstacle))
	{
		Obstacle = Queue;
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
	}
	if (bGhost)
	{
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
	else if (Reaction.bHold)
	{
		// Shaken or getting out: brake and stay put.
		Vehicle->SetAIDriveInput(0.f, 1.f, 0.f, Speed < 0.5f);
	}
	else
	{
		// Going round: the car steers as if it stood LateralOffset to the right of where it
		// is, so it drives that far to the left of its lane.
		const FVector Right = Vehicle->GetActorRightVector().GetSafeNormal2D();
		const float SteerX = X + Reaction.LateralOffset * static_cast<float>(Right.X);
		const float SteerY = Y + Reaction.LateralOffset * static_cast<float>(Right.Y);
		const FGTPDriveCommand Command = GTPDrive::Decide(Path, SteerX, SteerY, Yaw, Speed, LaneSpeed * Reaction.SpeedScale, Obstacle, Settings, Closest);
		// Wedged against something the probe does not see (kerb, bollard, a car's
		// corner): full throttle for 3 s without moving, then back off 1.5 s
		// steering the other way, and try again.
		if (BackOffTime > 0.f)
		{
			BackOffTime -= DeltaTime;
			Vehicle->SetAIDriveInput(0.f, 1.f, -Command.Steering, false);
		}
		else
		{
			PushTime = Command.Throttle > 0.5f && FMath::Abs(Speed) < 0.3f ? PushTime + DeltaTime : 0.f;
			if (PushTime > 3.f)
			{
				PushTime = 0.f;
				BackOffTime = 1.5f;
				GTP_TRACE(LogGTPTraffic, TEXT("%s: wedged, backing off"), *Vehicle->GetName());
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

int32 AGTPTrafficNetwork::LoadLanesFromCsv(const FString& Path)
{
	TArray<FString> Rows;
	if (!FFileHelper::LoadFileToStringArray(Rows, *Path) || Rows.Num() < 2)
	{
		return 0;
	}
	Lanes.Reset();
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
		Lanes.Add(MoveTemp(Lane));
	}
	// Drop links to lanes that are not there (a hand-edited file).
	for (FGTPTrafficLane& Lane : Lanes)
	{
		Lane.Next.RemoveAll([this](int32 Id) { return !Lanes.IsValidIndex(Id); });
	}
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

bool AGTPTrafficNetwork::FindNearestLane(const FVector& Location, float YawDegrees, float MaxDistanceCm, int32& OutLane, int32& OutPoint) const
{
	const FVector Heading = FRotator(0.f, YawDegrees, 0.f).Vector();
	double Best = static_cast<double>(MaxDistanceCm) * MaxDistanceCm;
	OutLane = INDEX_NONE;
	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
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
	if (static_cast<bool>(Body->CastShadow) != bShadow)
	{
		Body->SetCastShadow(bShadow);
	}
	const int32 Forced = bNear ? (NearLOD > 0 ? FMath::Clamp(NearLOD + 1, 0, Body->GetNumLODs()) : 0)
		: FMath::Clamp(FarLOD + 1, 0, Body->GetNumLODs());
	if (Body->GetForcedLOD() != Forced)
	{
		Body->SetForcedLOD(Forced);
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
	FParse::Value(FCommandLine::Get(), TEXT("Traffic="), WantedCars);
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

AGTPVehicle* AGTPTrafficNetwork::SpawnParkedVehicle(FName Tag, const FVector& Near, float RadiusCm, TSubclassOf<AGTPVehicle> Class)
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
	// Out of sight first; if every spot is seen, a seen one rather than nothing.
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
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
			FVector Location(Spot.Position.X, Spot.Position.Y, (A.Z + B.Z) * 0.5);
			if (Pass == 0 && IsSeenByPlayer(Location))
			{
				continue;
			}
			FHitResult Ground;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPParkedGround), false);
			if (!GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
			{
				continue;
			}
			Location.Z = Ground.ImpactPoint.Z + 40.f;
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
			GTP_TRACE(LogGTPTraffic, TEXT("traffic: parked %s (%s) on lane %d at %s, %.0f m from the request"), *Car->GetName(),
				*Tag.ToString(), Ref.Lane, *Location.ToCompactString(), Ref.Distance / 100.f);
			return Car;
		}
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: no free spot for a parked car (%s) within %.0f m of %s"), *Tag.ToString(), RadiusCm / 100.f, *Near.ToCompactString());
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
			ReusedDriver->PlaceGhost(Lane, Point, Ground, static_cast<float>(Transform.Rotator().Yaw), 0.f);
			Reused->SetActorEnableCollision(true);
			if (!AIDrivers.Contains(ReusedDriver))
			{
				AIDrivers.Add(ReusedDriver);
			}
			Cars.AddUnique(Reused);
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
		PoolSince.Add(GetWorld()->GetTimeSeconds());
	}
	return true;
}

AGTPVehicle* AGTPTrafficNetwork::TakeFromPool(const FTransform& Transform)
{
	PoolSince.SetNum(Pool.Num());
	const double Now = GetWorld()->GetTimeSeconds();
	while (Pool.Num() > 0)
	{
		// Oldest first, and only once it has cooled down: never a car put away a moment ago.
		AGTPVehicle* Car = Pool[0];
		if (IsValid(Car) && Now - PoolSince[0] < PoolCooldown)
		{
			return nullptr;
		}
		Pool.RemoveAt(0);
		PoolSince.RemoveAt(0);
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
		const bool bMoving = FMath::Abs(Car->GetSpeedKmh()) > 8.f;
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

void AGTPTrafficNetwork::Recycle(AGTPVehicle* Car)
{
	Cars.Remove(Car);
	if (bTrafficPool && PutAway(Car))
	{
		return;
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

	// Forget stolen cars (the player's now); take away stuck or far cars out of sight.
	for (int32 Index = Cars.Num() - 1; Index >= 0; --Index)
	{
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
		const bool bStuck = AIDriver && (AIDriver->IsAtDeadEnd() || AIDriver->GetStoppedTime() > StuckSeconds);
		if ((bFar || bStuck) && !IsSeenByPlayer(Car->GetActorLocation()))
		{
			Recycle(Car);
		}
	}
	UpdateSignalHeads();
	UpdateReactDemo();
	AIDrivers.RemoveAll([](const TWeakObjectPtr<UGTPAIDriverComponent>& Weak) { return !Weak.IsValid(); });

	// New cars out of sight: one a second when it means spawning an actor (no burst of spawns),
	// up to four a tick when the pool has cars to reuse (no spawn cost), so 40 cars fill in time.
	if (Cars.Num() < WantedCars && GetWorld()->GetTimeSeconds() >= NextSpawnTime)
	{
		const int32 Tries = FMath::Min(WantedCars - Cars.Num(), bTrafficPool && Pool.Num() > 0 ? 4 : 1);
		for (int32 Try = 0; Try < Tries; ++Try)
		{
			SpawnCar();
		}
		NextSpawnTime = GetWorld()->GetTimeSeconds() + (bTrafficPool && Pool.Num() > 0 ? 0.f : 1.f);
	}
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
	for (int32 Index = 0; Index < Lanes.Num(); ++Index)
	{
		if (Lights.GroupOf(Index) < 0)
		{
			continue;
		}
		const TArray<FVector>& Points = Lanes[Index].Points;
		const FVector Dir = (Points.Last() - Points[Points.Num() - 2]).GetSafeNormal2D();
		// Right-hand side of the stop line, 3 m back from the lane end, 2.6 m to the right.
		const FVector Right(-Dir.Y, Dir.X, 0.f);
		FVector Foot = Points.Last() - Dir * 300.f + Right * 260.f;
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
		UStaticMeshComponent* Pole = NewObject<UStaticMeshComponent>(this);
		Pole->SetStaticMesh(Cylinder);
		Pole->SetWorldLocationAndRotation(Foot + FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator);
		Pole->SetWorldScale3D(FVector(0.1f, 0.1f, 3.f));
		UStaticMeshComponent* Lamp = NewObject<UStaticMeshComponent>(this);
		Lamp->SetStaticMesh(Sphere);
		Lamp->SetWorldLocation(Foot + FVector(0.f, 0.f, 320.f));
		Lamp->SetWorldScale3D(FVector(0.35f));
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
		Pole->SetMaterial(0, Base);
		Head.Lamp = Lamp;
		Head.Pole = Pole;
		Head.Material = Material;
		SignalHeads.Add(Head);
	}
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
				Pole->SetWorldLocation(Head.Location + FVector(0.f, 0.f, 150.f));
				Lamp->SetWorldLocation(Head.Location + FVector(0.f, 0.f, 320.f));
			}
		}
		// Hidden until grounded: no lamp floating or buried while its cell streams in.
		const bool bDrawn = bVisible && Head.bGrounded;
		if (Lamp->IsVisible() != bDrawn)
		{
			Lamp->SetVisibility(bDrawn);
			Pole->SetVisibility(bDrawn);
		}
		const EGTPLight Light = Lights.LightFor(Head.Lane, Now);
		if (Light != Head.Shown)
		{
			Head.Shown = Light;
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
