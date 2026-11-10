#include "GTPTraffic.h"

#include "GTPLog.h"
#include "GTPVehicle.h"
#include "AIController.h"
#include "CollisionQueryParams.h"
#include "Components/LightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
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
		const int32 Next = bHasGoal ? Net->PickNextToward(Route.Last(), Goal) : Net->PickNext(Route.Last());
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
	const float LaneSpeed = FMath::Min(Net->GetLanes()[Route[0]].SpeedKmh, MaxSpeedKmh) / 3.6f;
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
	ReleaseJunctions();
	Route.Reset();
	Route.Add(Lane);
	Closest = StartPoint;
	StoppedTime = 0.f;
	bAtDeadEnd = false;
	PlanRoute();
	SetComponentTickEnabled(true);
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
		const int32 Next = Network->PickNext(Route.Last());
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

float UGTPAIDriverComponent::ProbeAhead(const AGTPVehicle* Vehicle, float LengthCm, float RadiusCm)
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
		return static_cast<float>(Hit.Distance) * CmToM;
	}
	return -1.f;
}

void UGTPAIDriverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AGTPVehicle* Vehicle = Cast<AGTPVehicle>(GetOwner());
	if (!Vehicle || !Network || Route.Num() == 0)
	{
		return;
	}
	if (!Vehicle->HasAIDriver() || Vehicle->IsWrecked())
	{
		// Stolen or destroyed: this driver is done.
		ReleaseJunctions();
		SetComponentTickEnabled(false);
		return;
	}

	const FVector Location = Vehicle->GetActorLocation();
	const float Yaw = FMath::DegreesToRadians(static_cast<float>(Vehicle->GetActorRotation().Yaw));
	const float Speed = Vehicle->GetSpeedKmh() / 3.6f;
	const FGTPTrafficLane& Lane = Network->GetLanes()[Route[0]];
	const float LaneSpeed = Lane.SpeedKmh / 3.6f;
	const float X = static_cast<float>(Location.X) * CmToM;
	const float Y = static_cast<float>(Location.Y) * CmToM;
	StoppedTime = FMath::Abs(Speed) < 0.3f ? StoppedTime + DeltaTime : 0.f;

	// Nearest of: something in front, the stop line of a junction another car is crossing.
	float Obstacle = FindObstacleMetres(Vehicle);
	const FVector& LaneEnd = Lane.Points.Last();
	const float StopLine = Junctions.Update(Network->GetJunctionTable(), static_cast<int>(Vehicle->GetUniqueID()), Lane.ToJunction,
		{static_cast<float>(LaneEnd.X) * CmToM, static_cast<float>(LaneEnd.Y) * CmToM}, X, Y, Speed, Settings.ComfortDecel, StoppedTime);
	if (StopLine >= 0.f && (Obstacle < 0.f || StopLine < Obstacle))
	{
		Obstacle = StopLine;
	}
	const FGTPDriveCommand Command = GTPDrive::Decide(Path, X, Y, Yaw, Speed, LaneSpeed, Obstacle, Settings, Closest);
	Vehicle->SetAIDriveInput(Command.Throttle, Command.Brake, Command.Steering, Command.Brake >= 1.f && Speed < 0.5f);

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
	return Lanes.Num();
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

void AGTPTrafficNetwork::MakeCheap(AGTPVehicle* Car, ACharacter* Driver) const
{
	// Lights of AI cars never cast shadows: a shadowed spot light is a full extra shadow pass.
	TArray<ULightComponent*> Lights;
	Car->GetComponents<ULightComponent>(Lights);
	for (ULightComponent* Light : Lights)
	{
		Light->SetCastShadows(false);
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

void AGTPTrafficNetwork::UpdateDetail(AGTPVehicle* Car, float DistanceToView) const
{
	USkeletalMeshComponent* Body = Car ? Car->GetMesh() : nullptr;
	if (!Body)
	{
		return;
	}
	const bool bNear = DistanceToView < DetailDistance;
	if (static_cast<bool>(Body->CastShadow) != bNear)
	{
		Body->SetCastShadow(bNear);
	}
	const int32 Forced = bNear ? 0 : FMath::Clamp(FarLOD + 1, 0, Body->GetNumLODs());
	if (Body->GetForcedLOD() != Forced)
	{
		Body->SetForcedLOD(Forced);
	}
	const bool bDrawn = DrawDistance <= 0.f || DistanceToView < DrawDistance;
	if (Body->IsVisible() != bDrawn)
	{
		Body->SetVisibility(bDrawn, true);
	}
}

int32 AGTPTrafficNetwork::PickNext(int32 Lane)
{
	if (!Lanes.IsValidIndex(Lane) || Lanes[Lane].Next.Num() == 0)
	{
		return INDEX_NONE;
	}
	const TArray<int32>& Next = Lanes[Lane].Next;
	return Next[Random.RandRange(0, Next.Num() - 1)];
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
	WantedCars = NumCars;
	FParse::Value(FCommandLine::Get(), TEXT("Traffic="), WantedCars);
	WantedCars = FMath::Clamp(WantedCars, 0, 40);

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
	if (!LoadedDriverClass)
	{
		AGameModeBase* Mode = GetWorld()->GetAuthGameMode();
		UClass* PawnClass = Mode ? Mode->GetDefaultPawnClassForController(nullptr) : nullptr;
		LoadedDriverClass = PawnClass && PawnClass->IsChildOf(ACharacter::StaticClass()) ? PawnClass : nullptr;
	}
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: %d cars wanted, %d car classes, driver %s"), WantedCars, LoadedVehicleClasses.Num(),
		LoadedDriverClass ? *LoadedDriverClass->GetName() : TEXT("none"));
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

bool AGTPTrafficNetwork::FindSpawnPoint(int32& OutLane, int32& OutPoint, FTransform& OutTransform) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector PlayerLocation = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	for (int32 Try = 0; Try < 40; ++Try)
	{
		const int32 Lane = Random.RandRange(0, Lanes.Num() - 1);
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
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	TSubclassOf<AGTPVehicle> Class = LoadedVehicleClasses[Random.RandRange(0, LoadedVehicleClasses.Num() - 1)];
	AGTPVehicle* Car = GetWorld()->SpawnActor<AGTPVehicle>(Class, Transform, Params);
	if (!Car)
	{
		return false;
	}
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
		return false;
	}
	UGTPAIDriverComponent* AIDriver = NewObject<UGTPAIDriverComponent>(Car, TEXT("AIDriver"));
	AIDriver->RegisterComponent();
	AIDriver->Start(this, Lane, Point);
	Cars.Add(Car);
	GTP_TRACE(LogGTPTraffic, TEXT("traffic: %s on lane %d at %s"), *Car->GetName(), Lane, *Transform.GetLocation().ToCompactString());
	return true;
}

void AGTPTrafficNetwork::Recycle(AGTPVehicle* Car)
{
	Cars.Remove(Car);
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
		const UGTPAIDriverComponent* AIDriver = Car->FindComponentByClass<UGTPAIDriverComponent>();
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
	// One new car at a time, so spawns never come in a burst.
	if (Cars.Num() < WantedCars && GetWorld()->GetTimeSeconds() >= NextSpawnTime)
	{
		SpawnCar();
		NextSpawnTime = GetWorld()->GetTimeSeconds() + 1.f;
	}
}
