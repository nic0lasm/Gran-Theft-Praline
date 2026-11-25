#include "GTPWanted.h"

#include "GTPSirenComponent.h"
#include "GTPCombat.h"
#include "GTPPoliceSenses.h"
#include "GTPPoliceQueries.h"
#include "GTPHornComponent.h"
#include "GTPWeapon.h"
#include "Engine/TargetPoint.h"

#include "GTPLog.h"
#include "GTPVehicle.h"
#include "GTPPedestrians.h"
#include "GTPLiteDriver.h"
#include "GTPPoliceOfficer.h"
#include "GTPPoliceUpstream.h"
#include "AIController.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/DecalComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPWanted, Log, All);

namespace GTPPolice
{
	const FName PoliceTag(TEXT("GTPPolice"));
}

namespace
{
	/** The lanes of the network as GTPPoliceUpstream::Find reads them (cm). */
	struct FPoliceUpstreamLanes
	{
		const TArray<FGTPTrafficLane>& Lanes;
		int Num() const { return Lanes.Num(); }
		int NumPoints(int Lane) const { return Lanes[Lane].Points.Num(); }
		float X(int Lane, int Point) const { return static_cast<float>(Lanes[Lane].Points[Point].X); }
		float Y(int Lane, int Point) const { return static_cast<float>(Lanes[Lane].Points[Point].Y); }
		int NumNext(int Lane) const { return Lanes[Lane].Next.Num(); }
		int Next(int Lane, int Index) const { return Lanes[Lane].Next[Index]; }
	};

	constexpr float WantedCmToM = 0.01f;
	/** A police car is made no nearer than this to any car on the road (cm, flat), and with nothing in this box around it (half sizes, cm). */
	constexpr float PoliceSpawnRoomCm = 1500.f;
	const FVector PoliceSpawnRoomExtent(300.f, 130.f, 90.f);
	/**
	 * The look for scenery: a box pushed ahead at the height of the bonnet (its middle this many cm above the car's
	 * place, half this high: the ground and the kerbs pass under it), started inside the car, this far ahead of its
	 * middle; the bumper is taken this much further (as the line of a held unit does).
	 */
	constexpr float SceneryLookHeight = 90.f;
	constexpr float SceneryLookHalfHeight = 40.f;
	constexpr float SceneryLookStartCm = 100.f;
	constexpr float SceneryBumperCm = 130.f;

	bool WitnessesDisabledByCommandLine()
	{
		int32 Value = 1;
		FParse::Value(FCommandLine::Get(), TEXT("Witnesses="), Value);
		return Value == 0;
	}

	/** Wheels of a police car in contact with the ground (4 while the vehicle has no physics output yet). */
	int32 PoliceWheelsOnGround(AGTPVehicle& Car)
	{
		UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Car.GetVehicleMovementComponent());
		const USkeletalMeshComponent* Body = Car.GetMesh();
		if (!Move || !Body || !Body->IsSimulatingPhysics() || !Move->PhysicsVehicleOutput().IsValid())
		{
			return 4;
		}
		int32 Down = 0;
		for (int32 Index = 0; Index < Move->GetNumWheels() && Index < Move->Wheels.Num(); ++Index)
		{
			Down += Move->GetWheelState(Index).bInContact ? 1 : 0;
		}
		return Down;
	}
}

// --- Wanted component ---------------------------------------------------------

UGTPWantedComponent::UGTPWantedComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

bool UGTPWantedComponent::IsPoliceEnabled()
{
	int32 Value = 1;
	FParse::Value(FCommandLine::Get(), TEXT("Police="), Value);
	return Value != 0 && !FParse::Param(FCommandLine::Get(), TEXT("NoPolice"));
}

UGTPWantedComponent* UGTPWantedComponent::Find(const UObject* WorldContext)
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContext, 0);
	return PC ? PC->FindComponentByClass<UGTPWantedComponent>() : nullptr;
}

void UGTPWantedComponent::SetSenses(UGTPPoliceSenses* InSenses)
{
	Senses = InSenses;
}

float UGTPWantedComponent::GetSecondsSinceSeen() const
{
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds()) - LastSeenTime : 1000.f;
}

void UGTPWantedComponent::ReportCrime(EGTPCrimeKind Kind, const FVector& Location, bool bVictimInvolved, const AActor* Scene)
{
	if (BustedBannerLeft > 0.f || !GetWorld() || !IsComponentTickEnabled())
	{
		return;
	}
	// The combat bench (-CombatBench=1) must play the same scene every pass: no police reaction to it.
	if (const UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this))
	{
		if (Combat->IsBenchRunning())
		{
			return;
		}
	}
	// A scrape is a hit every contact: one report of a kind every half second (gate 85: 26 in three frames).
	if (!ReportGate.Take(Kind, bVictimInvolved, static_cast<float>(GetWorld()->GetTimeSeconds())))
	{
		return;
	}
	if (const int32 LeftOut = ReportGate.TakeLeftOut())
	{
		GTP_TRACE(LogGTPWanted, TEXT("wanted: %d report(s) left out since the last one looked at, each less than %.1f s after one of its kind"), LeftOut, ReportGate.Gap);
	}
	if (Kind != EGTPCrimeKind::ArmedThreat)
	{
		// Every crime makes noise the police units nearby can hear (a shot, a crash, a scream).
		const APlayerController* Owner = Cast<APlayerController>(GetOwner());
		UGTPPoliceSenses::ReportNoise(this, Location, Owner ? Owner->GetPawn() : nullptr, 1.f);
	}
	if (!AreWitnessesRequired())
	{
		ApplyCrime(Kind, Location);
		return;
	}

	// Who saw or heard it: police cars and people close enough, with a clear line to the crime.
	std::vector<FGTPWitness> Witnesses;
	const FVector Eye = Location + FVector(0.f, 0.f, 150.f);
	// The nearest person whose line to the crime was cut, and by what (logged when nobody phones).
	double BlockedCm = -1.0;
	FString BlockedBy;
	// What a witness looks at never hides the crime: the car being stolen, and the player (the car he drives).
	const APlayerController* Criminal = Cast<APlayerController>(GetOwner());
	const APawn* CriminalPawn = Criminal ? Criminal->GetPawn() : nullptr;
	auto AddWitness = [&](const AActor* Who, bool bPolice)
	{
		if (!IsValid(Who))
		{
			return;
		}
		const FVector Where = Who->GetActorLocation();
		FCollisionQueryParams WitnessParams(SCENE_QUERY_STAT(GTPWitnessSight), false, Who);
		if (Scene)
		{
			WitnessParams.AddIgnoredActor(Scene);
		}
		if (CriminalPawn)
		{
			WitnessParams.AddIgnoredActor(CriminalPawn);
		}
		FHitResult Hit;
		const double AwayCm = FVector::Dist(Where, Location);
		const bool bCut = AwayCm < 8000.0
			&& GetWorld()->LineTraceSingleByChannel(Hit, Where + FVector(0.f, 0.f, 150.f), Eye, ECC_Visibility, WitnessParams);
		const bool bSees = AwayCm < 8000.0 && !bCut;
		if (bCut && !bPolice && (BlockedCm < 0.0 || AwayCm < BlockedCm))
		{
			BlockedCm = AwayCm;
			BlockedBy = FString::Printf(TEXT("%s (%s), %.0f cm from the point of the crime"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
				FVector::Dist(Hit.ImpactPoint, Eye));
		}
		FGTPWitness Witness;
		Witness.X = static_cast<float>(Where.X) * WantedCmToM;
		Witness.Y = static_cast<float>(Where.Y) * WantedCmToM;
		Witness.bPolice = bPolice;
		Witness.bCanSee = bSees;
		Witnesses.push_back(Witness);
	};
	for (const TWeakObjectPtr<AGTPVehicle>& Car : Police)
	{
		AddWitness(Car.Get(), true);
	}
	for (TActorIterator<AGTPPedestrian> It(GetWorld()); It; ++It)
	{
		AddWitness(*It, false);
	}
	const bool bLoud = Kind == EGTPCrimeKind::ShotsFired || Kind == EGTPCrimeKind::Killing || Kind == EGTPCrimeKind::KillPolice;
	const FGTPWitnessResult Result = GTPWitness::Evaluate(static_cast<float>(Location.X) * WantedCmToM, static_cast<float>(Location.Y) * WantedCmToM,
		bLoud, bVictimInvolved, Witnesses, WitnessSettings);
	if (!Result.bReported)
	{
		++CrimeReports.Unwitnessed;
		// Who was there, so that a crime nobody phoned in reads by itself: at the gate of candidate 50 the theft of the
		// run's car went unwitnessed, where 12 to 14 passers-by had seen it at candidate 48, and the line did not say
		// whether the street was empty, the people too far, or their view blocked.
		const float Range = bLoud ? WitnessSettings.LoudPedestrianRange : WitnessSettings.PedestrianRange;
		int32 People = 0, PeopleNear = 0, PeopleSeeing = 0, PoliceCars = 0;
		float NearestPerson = -1.f;
		for (const FGTPWitness& Witness : Witnesses)
		{
			if (Witness.bPolice)
			{
				++PoliceCars;
				continue;
			}
			const float Metres = static_cast<float>(FVector2D::Distance(FVector2D(Witness.X, Witness.Y),
				FVector2D(static_cast<float>(Location.X) * WantedCmToM, static_cast<float>(Location.Y) * WantedCmToM)));
			++People;
			PeopleNear += Metres <= Range ? 1 : 0;
			PeopleSeeing += Metres <= Range && Witness.bCanSee ? 1 : 0;
			NearestPerson = NearestPerson < 0.f ? Metres : FMath::Min(NearestPerson, Metres);
		}
		GTP_TRACE(LogGTPWanted, TEXT("wanted: crime %d not witnessed at %s: %d people on the map, %d within %.0f m, %d of those with a clear line, the nearest %.0f m away (-1 = nobody); %d police car(s) on the road; victim %d; the nearest person whose view was cut is %.0f m away (-1 = none), cut by %s"),
			static_cast<int32>(Kind), *Location.ToCompactString(), People, PeopleNear, Range, PeopleSeeing, NearestPerson, PoliceCars, bVictimInvolved ? 1 : 0,
			BlockedCm < 0.0 ? -1.0 : BlockedCm * WantedCmToM, BlockedBy.IsEmpty() ? TEXT("nothing") : *BlockedBy);
		return;
	}
	if (Result.Delay <= 0.f)
	{
		++CrimeReports.ByPolice;
		ApplyCrime(Kind, Location);
		return;
	}
	++CrimeReports.Called;
	CrimeReports.LastCallPersons = Result.Persons;
	CrimeReports.bLastCallVictim = Result.bVictim;
	FPendingReport& Pending = PendingReports.AddDefaulted_GetRef();
	Pending.Kind = Kind;
	Pending.Location = Location;
	Pending.DueTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + Result.Delay;
	GTP_TRACE(LogGTPWanted, TEXT("wanted: crime %d seen at %s, the police are called (%.0f s) by %d passer(s)-by%s"), static_cast<int32>(Kind),
		*Location.ToCompactString(), Result.Delay, Result.Persons, Result.bVictim ? TEXT(" and the victim") : TEXT(""));
}

bool UGTPWantedComponent::AreWitnessesRequired() const
{
	return bRequireWitnesses && !WitnessesDisabledByCommandLine();
}

void UGTPWantedComponent::TickPendingReports(float Now)
{
	for (int32 Index = PendingReports.Num() - 1; Index >= 0; --Index)
	{
		if (Now >= PendingReports[Index].DueTime)
		{
			const FPendingReport Report = PendingReports[Index];
			PendingReports.RemoveAtSwap(Index);
			if (ApplyCrime(Report.Kind, Report.Location))
			{
				++CrimeReports.CalledApplied;
				if (CrimeReports.FirstCallTime < 0.f)
				{
					CrimeReports.FirstCallTime = Now;
					CrimeReports.FirstCallStars = State.GetStars();
					CrimeReports.FirstCallUnits = Police.Num();
				}
			}
		}
	}
}

bool UGTPWantedComponent::ApplyCrime(EGTPCrimeKind Kind, const FVector& Location)
{
	if (BustedBannerLeft > 0.f || !GetWorld() || !IsComponentTickEnabled())
	{
		return false;
	}
	if (Kind == EGTPCrimeKind::ShotsFired || Kind == EGTPCrimeKind::ArmedThreat || Kind == EGTPCrimeKind::Assault
		|| Kind == EGTPCrimeKind::Killing || Kind == EGTPCrimeKind::KillPolice || Kind == EGTPCrimeKind::HitPolice)
	{
		LastAggressiveTime = static_cast<float>(GetWorld()->GetTimeSeconds());
	}
	const int32 Before = State.GetStars();
	if (!State.AddCrime(Kind, static_cast<float>(GetWorld()->GetTimeSeconds())))
	{
		return false;
	}
	if (Before == 0)
	{
		LastKnownLocation = Location;
	}
	DispatchLeft = DispatchSeconds;
	NotifyIfChanged();
	GTP_TRACE(LogGTPWanted, TEXT("wanted: crime %d, heat %.0f, %d stars"), static_cast<int32>(Kind), State.GetHeat(), State.GetStars());
	return true;
}

void UGTPWantedComponent::ForceStars(int32 Stars)
{
	State.ForceStars(Stars);
	DispatchLeft = Stars > 0 ? DispatchSeconds : 0.f;
	NotifyIfChanged();
	if (const APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			LastKnownLocation = Pawn->GetActorLocation();
		}
	}
	GTP_TRACE(LogGTPWanted, TEXT("wanted: level forced to %d stars"), State.GetStars());
}

void UGTPWantedComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!IsPoliceEnabled())
	{
		GTP_TRACE(LogGTPWanted, TEXT("police: off"));
		SetComponentTickEnabled(false);
		return;
	}
	int32 Stars = 0;
	float Delay = 5.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("WantedStars="), Stars) && Stars > 0)
	{
		FParse::Value(FCommandLine::Get(), TEXT("WantedDelay="), Delay);
		ScriptedStars = Stars;
		ScriptedTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + Delay;
	}
	bHoldLevel = FParse::Param(FCommandLine::Get(), TEXT("WantedHold"));
	BindCombat();
}

void UGTPWantedComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this))
	{
		Combat->OnShotFired.RemoveAll(this);
		Combat->OnDamageDealt.RemoveAll(this);
		Combat->OnPawnKilled.RemoveAll(this);
		Combat->OnArmedThreat.RemoveAll(this);
		Combat->OnPlayerDied.RemoveAll(this);
		Combat->OnPlayerRespawned.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

// Crimes table (docs/gameplay/wanted.md): what the player does -> crime -> heat -> stars. Witnesses decide whether it counts.
void UGTPWantedComponent::BindCombat()
{
	UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this);
	if (!Combat)
	{
		return;
	}
	auto IsPlayerBody = [this](const AActor* Actor)
	{
		const APlayerController* PC = Cast<APlayerController>(GetOwner());
		return Actor && PC && Actor == PC->GetPawn();
	};
	auto IsPolice = [](const AActor* Actor)
	{
		return Actor && Actor->ActorHasTag(GTPPolice::PoliceTag);
	};
	Combat->OnShotFired.AddWeakLambda(this, [this, IsPlayerBody](AActor* Shooter, const FVector& Origin, const FVector&, FName, const FHitResult&, bool)
	{
		if (IsPlayerBody(Shooter))
		{
			ReportCrime(EGTPCrimeKind::ShotsFired, Origin);
		}
	});
	Combat->OnArmedThreat.AddWeakLambda(this, [this, IsPlayerBody](AActor* Source, const FVector& Origin, const FVector&)
	{
		if (IsPlayerBody(Source))
		{
			ReportCrime(EGTPCrimeKind::ArmedThreat, Origin);
		}
	});
	Combat->OnDamageDealt.AddWeakLambda(this, [this, IsPlayerBody, IsPolice](AActor* Instigator, AActor* Victim, float, GTPDamage::EZone, bool, float)
	{
		if (IsPlayerBody(Instigator) && Victim && !IsPlayerBody(Victim))
		{
			ReportCrime(IsPolice(Victim) ? EGTPCrimeKind::HitPolice : EGTPCrimeKind::Assault, Victim->GetActorLocation(), true);
		}
	});
	Combat->OnPawnKilled.AddWeakLambda(this, [this, IsPlayerBody, IsPolice](AActor* Killer, AActor* Victim, bool)
	{
		if (IsPlayerBody(Killer) && Victim && !IsPlayerBody(Victim))
		{
			ReportCrime(IsPolice(Victim) ? EGTPCrimeKind::KillPolice : EGTPCrimeKind::Killing, Victim->GetActorLocation(), true);
		}
	});
	Combat->OnPlayerDied.AddWeakLambda(this, [this](const FVector&)
	{
		OnPlayerKilled();
	});
	Combat->OnPlayerRespawned.AddWeakLambda(this, [this](const FVector&, FName)
	{
		OnPlayerRespawnedAfterDeath();
	});
}

void UGTPWantedComponent::OnPlayerKilled()
{
	bDeadNow = true;
	PendingReports.Reset();
	GTP_TRACE(LogGTPWanted, TEXT("wanted: player died (wasted)"));
	OnWasted.Broadcast();
}

void UGTPWantedComponent::OnPlayerRespawnedAfterDeath()
{
	// Dead and back at the hospital: the police forget (as in GTA). The weapon is the one the combat code gives.
	bDeadNow = false;
	bArrestWarned = false;
	ClearWanted();
}

// A different body or car out of sight: the police look for the old one.
void UGTPWantedComponent::TrackCarChange(const APawn* Pawn, bool bSeen)
{
	const APawn* Previous = LastPlayerPawn.Get();
	LastPlayerPawn = Pawn;
	if (Previous && Pawn && Previous != Pawn && !bSeen && State.ShortenSearch(CarChangeKeep))
	{
		GTP_TRACE(LogGTPWanted, TEXT("wanted: changed vehicle out of sight, search shortened"));
	}
}

void UGTPWantedComponent::EnsureMinStars(int32 Stars)
{
	if (Stars > State.GetStars())
	{
		ForceStars(Stars);
	}
}

void UGTPWantedComponent::NotifyIfChanged()
{
	const int32 Stars = State.GetStars();
	const int32 Phase = static_cast<int32>(State.GetPhase());
	if (Stars != LastNotifiedStars || Phase != LastNotifiedPhase)
	{
		LastNotifiedStars = Stars;
		LastNotifiedPhase = Phase;
		OnWantedChanged.Broadcast(Stars, State.GetPhase());
	}
}

void UGTPWantedComponent::ClearWanted()
{
	State.Clear();
	bSeenNow = false;
	NotifyIfChanged();
}

void UGTPWantedComponent::RegisterPolice(AGTPVehicle* Car)
{
	Police.AddUnique(Car);
}

void UGTPWantedComponent::UnregisterPolice(AGTPVehicle* Car)
{
	Police.Remove(Car);
}

void UGTPWantedComponent::RegisterOfficer(AActor* Officer)
{
	Officers.AddUnique(Officer);
}

void UGTPWantedComponent::UnregisterOfficer(AActor* Officer)
{
	Officers.Remove(Officer);
}

bool UGTPWantedComponent::IsPlayerAggressive() const
{
	return GetWorld() && GetWorld()->GetTimeSeconds() - LastAggressiveTime < 20.0;
}

bool UGTPWantedComponent::CanSeePlayer(const AActor* Car, const FVector& PlayerLocation) const
{
	const double Distance = FVector::Dist(Car->GetActorLocation(), PlayerLocation);
	if (Distance <= NoticeRange)
	{
		return true;
	}
	if (Distance > SightRange)
	{
		return false;
	}
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceSight), false, Car);
	if (PC && PC->GetPawn())
	{
		Params.AddIgnoredActor(PC->GetPawn());
	}
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Car->GetActorLocation() + FVector(0.f, 0.f, 150.f),
		PlayerLocation + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params);
}

void UGTPWantedComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (BustedBannerLeft > 0.f)
	{
		BustedBannerLeft = FMath::Max(0.f, BustedBannerLeft - DeltaTime);
	}
	WarningLeft = FMath::Max(0.f, WarningLeft - DeltaTime);
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	if (ScriptedStars > 0 && ScriptedTime >= 0.f && Now >= ScriptedTime)
	{
		ForceStars(ScriptedStars);
		ScriptedTime = -1.f;
	}
	if (PendingReports.Num() > 0)
	{
		TickPendingReports(Now);
	}
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (UGTPPoliceSenses* Eyes = Senses.Get())
	{
		Eyes->SetTarget(PC ? PC->GetPawn() : nullptr);
	}
	if (!Pawn || State.GetStars() == 0 || bDeadNow)
	{
		bSeenNow = false;
		bArrestWarned = false;
		LastPlayerPawn = Pawn;
		NotifyIfChanged();
		return;
	}

	const FVector PlayerLocation = Pawn->GetActorLocation();
	bool bSeen = false;
	float Nearest = -1.f;
	for (int32 Index = Police.Num() - 1; Index >= 0; --Index)
	{
		const AGTPVehicle* Car = Police[Index].Get();
		if (!IsValid(Car))
		{
			Police.RemoveAt(Index);
			continue;
		}
		const float Metres = static_cast<float>(FVector::Dist(Car->GetActorLocation(), PlayerLocation)) * WantedCmToM;
		Nearest = Nearest < 0.f ? Metres : FMath::Min(Nearest, Metres);
		bSeen = bSeen || CanSeePlayer(Car, PlayerLocation);
	}
	for (int32 Index = Officers.Num() - 1; Index >= 0; --Index)
	{
		const AActor* Officer = Officers[Index].Get();
		if (!IsValid(Officer))
		{
			Officers.RemoveAt(Index);
			continue;
		}
		const float Metres = static_cast<float>(FVector::Dist(Officer->GetActorLocation(), PlayerLocation)) * WantedCmToM;
		Nearest = Nearest < 0.f ? Metres : FMath::Min(Nearest, Metres);
		bSeen = bSeen || CanSeePlayer(Officer, PlayerLocation);
	}
	// Perception (sight and hearing of the units) next to the line-of-sight test above; see UGTPPoliceSenses for the modes.
	bool bPerceived = false;
	if (UGTPPoliceSenses* Eyes = Senses.Get())
	{
		bPerceived = Eyes->AnySees();
		if (UGTPPoliceSenses::GetMode() == 1)
		{
			bSeen = bPerceived;
		}
		else
		{
			bSeen = bSeen || bPerceived;
		}
		if (!bSeen && Eyes->WasHeardRecently(Now, 2.f))
		{
			// A shot or a crash heard: they head for the noise, but nobody has seen the player (no level hold).
			LastKnownLocation = Eyes->GetSensedLocation();
		}
	}
	bSeenNow = bSeen;
	if (bSeen)
	{
		LastKnownLocation = PlayerLocation;
		LastSeenTime = Now;
	}
	// Dispatch: just after a crime the police are told where the player is, and the level cannot drop.
	const bool bDispatched = DispatchLeft > 0.f;
	if (bDispatched)
	{
		DispatchLeft -= DeltaTime;
		LastKnownLocation = PlayerLocation;
	}
	// Until the first unit has arrived the level cannot drop: nobody has had the chance to look.
	const bool bHold = bSeen || bHoldLevel || bDispatched || (bPoliceAvailable && Police.Num() == 0);

	if (Now >= NextTraceTime)
	{
		NextTraceTime = Now + 2.f;
		GTP_TRACE(LogGTPWanted, TEXT("wanted: t=%.0f stars=%d phase=%d units=%d officers=%d nearest=%.0f m seen=%d arrest=%.2f perceived=%d"), Now, State.GetStars(),
			static_cast<int32>(State.GetPhase()), Police.Num(), Officers.Num(), Nearest, bSeen ? 1 : 0, State.GetArrestProgress(), bPerceived ? 1 : 0);
	}

	const AGTPVehicle* Vehicle = Cast<AGTPVehicle>(Pawn);
	const float SpeedKmh = Vehicle ? FMath::Abs(Vehicle->GetSpeedKmh()) : 0.f;
	const int32 StarsBefore = State.GetStars();
	TrackCarChange(Pawn, bSeen);
	// Hiding: out of sight, still, and nobody close makes the search run faster.
	const bool bHiding = !bHold && Pawn->GetVelocity().Size() < HiddenSpeedCmS && (Nearest < 0.f || Nearest * 100.f > HiddenMinDistance);
	State.Tick(DeltaTime, bHold, bHiding ? HiddenSearchRate : 1.f);
	const bool bBusted = State.UpdateArrest(DeltaTime, bArrestEnabled ? Nearest : -1.f, SpeedKmh, Vehicle == nullptr);
	if (State.GetArrestProgress() > 0.f && !bArrestWarned)
	{
		// The warning: police are on top of a player who cannot get away.
		bArrestWarned = true;
		WarningLeft = 2.5f;
		OnArrestWarning.Broadcast();
	}
	else if (State.GetArrestProgress() <= 0.f)
	{
		bArrestWarned = false;
	}
	if (bBusted)
	{
		OnBusted();
	}
	else if (StarsBefore > 0 && State.GetStars() == 0)
	{
		GTP_TRACE(LogGTPWanted, TEXT("wanted: level lost"));
	}
	NotifyIfChanged();
}

void UGTPWantedComponent::OnBusted()
{
	GTP_TRACE(LogGTPWanted, TEXT("wanted: busted"));
	State.Clear();
	bSeenNow = false;
	BustedBannerLeft = BustedSeconds;
	NotifyIfChanged();
	OnArrested.Broadcast();
	bArrestWarned = false;
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		return;
	}
	if (AGTPVehicle* Vehicle = Cast<AGTPVehicle>(PC->GetPawn()))
	{
		Vehicle->RequestExit();
		// His car does not stay in the street for good (AGTPTrafficNetwork::NoteLeftAtArrest).
		if (TActorIterator<AGTPTrafficNetwork> Streets(GetWorld()); Streets)
		{
			Streets->NoteLeftAtArrest(Vehicle);
		}
	}
	if (bArrestTakesWeapon && PC->GetPawn())
	{
		// Hands up: the weapon is holstered and the police keep it and the ammunition.
		// FindHeld also finds the gun of a driver still seated (the exit is not over yet).
		if (UGTPWeaponComponent* Weapon = UGTPWeaponComponent::FindHeld(PC->GetPawn()))
		{
			Weapon->SetDrawn(false);
			Weapon->StripAmmo();
		}
	}
	PC->SetIgnoreMoveInput(true);
	GetWorld()->GetTimerManager().SetTimer(BustedTimer, this, &UGTPWantedComponent::EndBusted, BustedSeconds, false);
}

void UGTPWantedComponent::EndBusted()
{
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			// Back to the game at the station: the police have taken the player there (a point tagged
			// GTPPoliceStation, else the configured place, else the player start).
			bool bMoved = false;
			for (TActorIterator<ATargetPoint> It(GetWorld()); It && !bMoved; ++It)
			{
				if (It->ActorHasTag(TEXT("GTPPoliceStation")))
				{
					bMoved = Pawn->TeleportTo(It->GetActorLocation() + FVector(0.f, 0.f, 100.f), It->GetActorRotation());
				}
			}
			if (!bMoved && ArrestReturnEnabled)
			{
				bMoved = Pawn->TeleportTo(ArrestReturnLocation, Pawn->GetActorRotation());
			}
			if (!bMoved)
			{
				if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
				{
					if (const AActor* Start = GameMode->FindPlayerStart(PC))
					{
						Pawn->TeleportTo(Start->GetActorLocation() + FVector(0.f, 0.f, 100.f), Start->GetActorRotation());
					}
				}
			}
		}
		// Reset, not SetIgnoreMoveInput(false): the lock is a counter, and a second arrest before the end of the first
		// (or a vehicle exit) would leave it above zero and the player unable to walk for the rest of the game.
		PC->ResetIgnoreMoveInput();
	}
}

// --- Police director -------------------------------------------------------------

AGTPPoliceDirector::AGTPPoliceDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	Random.Initialize(0x504F4C49);
}

void AGTPPoliceDirector::BeginPlay()
{
	Super::BeginPlay();
	FParse::Value(FCommandLine::Get(), TEXT("PoliceOfficers="), MaxOfficers);
	int32 LiteSwitch = bPoliceLite ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("PoliceLite="), LiteSwitch);
	bPoliceLite = LiteSwitch != 0;
	if (bPoliceLite)
	{
		MaxUnits = FMath::Min(MaxUnits, LiteMaxUnits);
		MaxOfficers = FMath::Min(MaxOfficers, 2);
	}
	FParse::Value(FCommandLine::Get(), TEXT("PoliceMax="), MaxUnits);
	MaxUnits = FMath::Clamp(MaxUnits, 0, 6);
	int32 AvoidSwitch = bAvoidScenery ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("PoliceAvoid="), AvoidSwitch);
	bAvoidScenery = AvoidSwitch != 0;
	if (!bAvoidScenery)
	{
		CloseInDistance = 0.f;
	}
	int32 UpstreamSwitch = bUpstreamSpawn ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("PoliceUpstream="), UpstreamSwitch);
	bUpstreamSpawn = UpstreamSwitch != 0;
	// -RoadblocksOnFlightLine: the roadblocks stand on the way of a scripted flight too, as until gate 97.
	if (FParse::Param(FCommandLine::Get(), TEXT("RoadblocksOnFlightLine")))
	{
		RoadblockFlightClearance = 0.f;
	}
	int32 CircuitLinesSwitch = bUnitsOnCircuitLines ? 1 : 0;
	FParse::Value(FCommandLine::Get(), TEXT("PoliceCircuitLines="), CircuitLinesSwitch);
	bUnitsOnCircuitLines = CircuitLinesSwitch != 0;
	LoadedPoliceClass = PoliceVehicleClass.TryLoadClass<AGTPVehicle>();
	FString PoliceLook;
	if (LoadedPoliceClass.Get() && !AGTPVehicle::IsClassDrawn(LoadedPoliceClass.Get(), PoliceLook))
	{
		// City Sample's saloon drew nothing at gates 51 to 68 (its rig only): a traffic car that is drawn stands in.
		GTP_TRACE(LogGTPWanted, TEXT("police: car class %s NOT DRAWN, a traffic car stands in: %s"), *GetNameSafe(LoadedPoliceClass.Get()), *PoliceLook);
		LoadedPoliceClass = nullptr;
	}
	LoadedDriverClass = PoliceDriverClass.TryLoadClass<ACharacter>();
	int32 LiteClassSwitch = 0;
	FParse::Value(FCommandLine::Get(), TEXT("LiteDriverClass="), LiteClassSwitch);
	if (LiteClassSwitch != 0)
	{
		// Camera-less occupant: no camera host is ever created for a police driver.
		LoadedDriverClass = AGTPLiteDriver::StaticClass();
	}
	if (UGTPPoliceSenses::GetMode() != 0)
	{
		Senses = NewObject<UGTPPoliceSenses>(this, TEXT("GTPPoliceSenses"));
		Senses->RegisterComponent();
	}
	if (UGTPPoliceQueries::IsEnabled())
	{
		Queries = NewObject<UGTPPoliceQueries>(this, TEXT("GTPPoliceQueries"));
		Queries->Init();
	}
	NextSpawnTime = GetWorld()->GetTimeSeconds() + 3.f;
	GTP_TRACE(LogGTPWanted, TEXT("police: on, up to %d units, class %s; scenery %s, straight at a known player within %.0f m with nothing between; flight circuit lanes driven on %s"), MaxUnits,
		LoadedPoliceClass.Get() ? *GetNameSafe(LoadedPoliceClass.Get()) : TEXT("traffic cars"), bAvoidScenery ? TEXT("gone round") : TEXT("not looked at (-PoliceAvoid=0)"),
		CloseInDistance * WantedCmToM, bUnitsOnCircuitLines ? TEXT("the circuit's line") : TEXT("their own points (-PoliceCircuitLines=0)"));
	GTP_TRACE(LogGTPWanted, TEXT("police: cars sent after a player who drives are made %s"), bUpstreamSpawn
		? *FString::Printf(TEXT("upstream of him on his road, rolling at his speed, %.0f km/h at most"), UpstreamStartKmh) : TEXT("on any lane of the ring (-PoliceUpstream=0)"));
}

void AGTPPoliceDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
	{
		Wanted->SetPoliceAvailable(false);
	}
	Super::EndPlay(EndPlayReason);
}

bool AGTPPoliceDirector::FindSpawnPoint(const FVector& PlayerLocation, int32& OutLane, int32& OutPoint, FTransform& OutTransform) const
{
	const TArray<FGTPTrafficLane>& Lanes = Network->GetLanes();
	if (Lanes.Num() == 0)
	{
		return false;
	}
	FRandomStream& Dice = const_cast<FRandomStream&>(Random);
	// Every car on the road, whoever drives it: a police car needs room to pull away, and the traffic's cars stand on
	// the same lanes (only the other police cars were looked at before).
	TArray<FVector, TInlineAllocator<96>> CarsOnTheRoad;
	for (TActorIterator<AGTPVehicle> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden())
		{
			CarsOnTheRoad.Add(It->GetActorLocation());
		}
	}
	int32 WrongDistance = 0, NotAhead = 0, NoGround = 0, NoRoom = 0, Taken = 0, Seen = 0;
	FString LastTakenBy;
	for (int32 Try = 0; Try < 60; ++Try)
	{
		const int32 Lane = Dice.RandRange(0, Lanes.Num() - 1);
		const TArray<FVector>& Points = Lanes[Lane].Points;
		if (Points.Num() < 4 || Lanes[Lane].Next.Num() == 0)
		{
			continue;
		}
		const int32 Point = Dice.RandRange(1, Points.Num() - 3);
		FVector Location = Points[Point];
		const double Distance = FVector::Dist2D(Location, PlayerLocation);
		// A replacement is made far (it has to drive to the player), the first cars of a level in the spawn ring.
		const double Nearest = bFarSpawn ? ReplacementMinDistance : (bAheadSpawn ? MinSpawnDistance * 1.6 : MinSpawnDistance);
		const double Farthest = bFarSpawn ? FMath::Max(ReplacementMinDistance * 1.2, LeashDistance * 0.85) : (bAheadSpawn ? MaxSpawnDistance * 1.4 : MaxSpawnDistance);
		if (Distance < Nearest || Distance > Farthest)
		{
			++WrongDistance;
			continue;
		}
		// Coming from in front: within about 35 degrees of the player's direction of travel.
		if (bAheadSpawn && !GTPPoliceTactics::IsAhead(static_cast<float>(Location.X - PlayerLocation.X), static_cast<float>(Location.Y - PlayerLocation.Y),
			static_cast<float>(AheadDirection.X), static_cast<float>(AheadDirection.Y), 0.8f))
		{
			++NotAhead;
			continue;
		}
		FHitResult Ground;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceGround), false);
		if (!GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			++NoGround;
			continue;
		}
		Location.Z = Ground.ImpactPoint.Z + 40.f;
		bool bFree = true;
		for (const FVector& Other : CarsOnTheRoad)
		{
			if (FVector::Dist2D(Other, Location) < PoliceSpawnRoomCm)
			{
				bFree = false;
				break;
			}
		}
		if (!bFree)
		{
			++NoRoom;
			continue;
		}
		const FVector Direction = (Points[Point + 1] - Points[Point]).GetSafeNormal2D();
		const FTransform Place(Direction.Rotation(), Location);
		if (!IsSpawnPlaceClear(Place, Ground, false, LastTakenBy))
		{
			++Taken;
			continue;
		}
		if (Network->IsSeenByPlayer(Location))
		{
			++Seen;
			continue;
		}
		OutLane = Lane;
		OutPoint = Point;
		OutTransform = Place;
		return true;
	}
	if (const double Now = GetWorld()->GetTimeSeconds(); Now >= NextNoPlaceLog)
	{
		NextNoPlaceLog = Now + 10.0;
		GTP_TRACE(LogGTPWanted, TEXT("police: no place for a %s in 60 tries: %d too near or too far, %d not ahead, %d without ground, %d within %.0f m of a car, "
			"%d with a car or somebody on the spot (last: %s), %d in the player's view; %d car(s) on the road"),
			bFarSpawn ? TEXT("replacement") : TEXT("car"), WrongDistance, NotAhead, NoGround, NoRoom, PoliceSpawnRoomCm * WantedCmToM, Taken,
			LastTakenBy.IsEmpty() ? TEXT("none") : *LastTakenBy, Seen, CarsOnTheRoad.Num());
	}
	return false;
}

bool AGTPPoliceDirector::FindUpstreamPoint(const FVector& PlayerLocation, int32& OutLane, int32& OutPoint, FTransform& OutTransform, float& OutAlongM) const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const TArray<FGTPTrafficLane>& Lanes = Network->GetLanes();
	if (!Player || Lanes.Num() == 0)
	{
		return false;
	}
	// The lane he drives on: the nearest one that goes his way.
	const FVector2D Travel = GetTravelDirection(*Player);
	int32 OnLane = INDEX_NONE, AtPoint = 0;
	if (Travel.IsNearlyZero()
		|| !Network->FindNearestLane(PlayerLocation, static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Travel.Y, Travel.X))), 1500.f, OnLane, AtPoint))
	{
		return false;
	}
	TArray<FVector, TInlineAllocator<96>> CarsOnTheRoad;
	for (TActorIterator<AGTPVehicle> It(GetWorld()); It; ++It)
	{
		if (!It->IsHidden())
		{
			CarsOnTheRoad.Add(It->GetActorLocation());
		}
	}
	// The same place as in the ring: ground under it, room to pull away, nobody on it, out of the player's sight.
	FTransform Found;
	const auto IsFree = [this, &Lanes, &CarsOnTheRoad, &Found](int InLane, int InPoint)
	{
		const TArray<FVector>& Points = Lanes[InLane].Points;
		FVector Location = Points[InPoint];
		FHitResult Ground;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceUpstreamGround), false);
		if (!GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			return false;
		}
		Location.Z = Ground.ImpactPoint.Z + 40.f;
		for (const FVector& Other : CarsOnTheRoad)
		{
			if (FVector::Dist2D(Other, Location) < PoliceSpawnRoomCm)
			{
				return false;
			}
		}
		const FTransform Spot((Points[InPoint + 1] - Points[InPoint]).GetSafeNormal2D().Rotation(), Location);
		FString TakenBy;
		if (!IsSpawnPlaceClear(Spot, Ground, false, TakenBy) || Network->IsSeenByPlayer(Location))
		{
			return false;
		}
		Found = Spot;
		return true;
	};
	FRandomStream& Dice = const_cast<FRandomStream&>(Random);
	const auto PickLane = [&Dice](int Count) { return Dice.RandRange(0, Count - 1); };
	const float Nearest = bFarSpawn ? ReplacementMinDistance : MinSpawnDistance;
	const float Farthest = bFarSpawn ? FMath::Max(ReplacementMinDistance * 1.2f, LeashDistance * 0.85f) : MaxSpawnDistance;
	const FPoliceUpstreamLanes View{Lanes};
	GTPPoliceUpstream::FPlace Place;
	// Two walks: where two roads lead to his, the other one may have the place the first has not.
	for (int32 Walk = 0; Walk < 2; ++Walk)
	{
		if (GTPPoliceUpstream::Find(View, OnLane, AtPoint, static_cast<float>(PlayerLocation.X), static_cast<float>(PlayerLocation.Y), Nearest, Farthest, 1000.f, 8,
			PickLane, IsFree, Place))
		{
			OutLane = Place.Lane;
			OutPoint = Place.Point;
			OutTransform = Found;
			OutAlongM = Place.Along * WantedCmToM;
			return true;
		}
	}
	return false;
}

bool AGTPPoliceDirector::IsSpawnPlaceClear(const FTransform& Place, const FHitResult& Ground, bool bRoadblock, FString& OutWhat) const
{
	// The trace that looks for the ground is a visibility trace: it stops on the roof of a car standing there.
	if (const AActor* Under = Ground.GetActor(); Under && Under->IsA<APawn>())
	{
		OutWhat = FString::Printf(TEXT("%s under the place"), *Under->GetName());
		return false;
	}
	// The room the new car takes (a little more than its body), from the ground up.
	const FVector Center = Ground.ImpactPoint + FVector(0.f, 0.f, PoliceSpawnRoomExtent.Z + 20.f);
	TArray<FOverlapResult> Overlaps;
	const FCollisionObjectQueryParams Objects(FCollisionObjectQueryParams::InitType::AllDynamicObjects);
	const FCollisionQueryParams Query(SCENE_QUERY_STAT(GTPPoliceSpawnRoom), false);
	if (!GetWorld()->OverlapMultiByObjectType(Overlaps, Center, Place.GetRotation(), Objects, FCollisionShape::MakeBox(PoliceSpawnRoomExtent), Query))
	{
		return true;
	}
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* Other = Overlap.GetActor();
		if (!Other || Other->IsHidden() || !Other->IsA<APawn>())
		{
			continue;
		}
		if (bRoadblock && Other->ActorHasTag(GTPPolice::PoliceTag))
		{
			continue;
		}
		OutWhat = FString::Printf(TEXT("%s on the place"), *Other->GetName());
		return false;
	}
	return true;
}

bool AGTPPoliceDirector::SpawnUnit(const FVector& PlayerLocation, const FTransform* Placement, bool bBlock)
{
	TSubclassOf<AGTPVehicle> Class = LoadedPoliceClass;
	if (Class.Get() != nullptr && !IsValid(Class.Get()))
	{
		// A class that died or is being replaced: forget it, the traffic cars stand in.
		LoadedPoliceClass = nullptr;
		Class = nullptr;
	}
	if (!Class && Network->LoadedVehicleClasses.Num() > 0)
	{
		Class = Network->LoadedVehicleClasses[Random.RandRange(0, Network->LoadedVehicleClasses.Num() - 1)];
	}
	if (!Class || !(LoadedDriverClass.Get() || Network->LoadedDriverClass.Get()))
	{
		return false;
	}
	int32 Lane = 0, Point = 0;
	FTransform Transform;
	bool bUpstream = false;
	float UpstreamAlongM = 0.f;
	if (Placement)
	{
		Transform = *Placement;
	}
	else if (bUpstreamTry && FindUpstreamPoint(PlayerLocation, Lane, Point, Transform, UpstreamAlongM))
	{
		bUpstream = true;
	}
	else
	{
		UpstreamNoPlace += bUpstreamTry ? 1 : 0;
		if (!FindSpawnPoint(PlayerLocation, Lane, Point, Transform))
		{
			return false;
		}
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
	AGTPVehicle* Car = GetWorld()->SpawnActor<AGTPVehicle>(Class, Transform, Params);
	if (!Car)
	{
		return false;
	}
	Car->Tags.AddUnique(GTPPolice::PoliceTag);
	Car->NotePlaced(TEXT("police spawn"));

	FUnit& Unit = Units.AddDefaulted_GetRef();
	Unit.Car = Car;
	Unit.bBlock = bBlock;
	// Chase tuning: harder bends than the calm city driving, and a brake that holds the car to the corner ahead.
	GTPDrive::SetChaseDriving(Unit.Settings);
	Unit.Settings.SafeGap = 3.f;
	// The driver is seated in the frame the car is made, like the traffic's cars (a car that got its driver a
	// frame later stayed in neutral on the PC); the light bar and the registration come on the next frames.
	if (!SeatDriver(Unit))
	{
		Car->Destroy();
		Units.Pop();
		return false;
	}
	Unit.Stage = 2;
	const float FromPlayer = static_cast<float>(FVector::Dist2D(Transform.GetLocation(), PlayerLocation)) * WantedCmToM;
	if (!bBlock)
	{
		// Counted for the pursuit: how many cars were sent, and how near the player a replacement was made.
		ChaseTally.NoteSent(FromPlayer, bFarSpawn);
		++ResponseSent;
	}
	// Made upstream it rolls from its first frame, as a car of the traffic does when it is given back to the solver
	// (AGTPTrafficNetwork, "ghost wake"): from a standstill 50 m behind a player at 60 km/h it was 110 m behind him
	// by the time it had his speed.
	float RollKmh = 0.f;
	if (bUpstream)
	{
		++UpstreamMade;
		const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
		UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Car->GetRootComponent());
		if (Player && Body && Body->IsSimulatingPhysics())
		{
			RollKmh = GTPPoliceUpstream::RollKmh(static_cast<float>(Player->GetVelocity().Size2D()) * 0.036f,
				(Profile.ChaseKmh > 0.f ? Profile.ChaseKmh : GTPWanted::ChaseSpeedKmh(LastStars)) * 0.9f, UpstreamStartKmh);
			if (RollKmh > 0.f)
			{
				Body->SetPhysicsLinearVelocity(Transform.GetRotation().GetForwardVector().GetSafeNormal2D() * (RollKmh / 0.036f));
			}
		}
	}
	GTP_TRACE(LogGTPWanted, TEXT("police: %s on lane %d at %s, %.0f m from the player%s%s"), *Car->GetName(), Lane, *Transform.GetLocation().ToCompactString(),
		FromPlayer, bBlock ? TEXT(" (roadblock)") : (bFarSpawn ? TEXT(" (replacement)") : TEXT("")),
		bUpstream ? *FString::Printf(TEXT(" (upstream: %.0f m of road to him, rolling at %.0f km/h; %d made so, %d time(s) no place there)"), UpstreamAlongM, RollKmh,
			UpstreamMade, UpstreamNoPlace) : TEXT(""));
	return true;
}

void AGTPPoliceDirector::OnArrestWarning()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	AGTPVehicle* Nearest = nullptr;
	double Best = 1e12;
	for (const FUnit& Unit : Units)
	{
		AGTPVehicle* Car = Unit.Car.Get();
		if (Player && IsValid(Car) && !Unit.bBlock)
		{
			const double Distance = FVector::Dist(Car->GetActorLocation(), Player->GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Nearest = Car;
			}
		}
	}
	if (Nearest && Best < 3000.0)
	{
		HornCar = Nearest;
		UGTPHornComponent::SetHorn(Nearest, true);
		GetWorldTimerManager().SetTimer(HornTimer, this, &AGTPPoliceDirector::StopWarningHorn, 0.5f, false);
	}
}

void AGTPPoliceDirector::StopWarningHorn()
{
	if (AGTPVehicle* Car = HornCar.Get())
	{
		UGTPHornComponent::SetHorn(Car, false);
	}
	HornCar.Reset();
}

FVector2D AGTPPoliceDirector::GetTravelDirection(const APawn& Player) const
{
	const FVector Velocity = Player.GetVelocity();
	if (Velocity.Size2D() >= 400.f)
	{
		LastTravelDirection = FVector2D(Velocity.X, Velocity.Y).GetSafeNormal();
	}
	else if (LastTravelDirection.IsNearlyZero())
	{
		const FVector Forward = Player.GetActorForwardVector();
		LastTravelDirection = FVector2D(Forward.X, Forward.Y).GetSafeNormal();
	}
	return LastTravelDirection;
}

bool AGTPPoliceDirector::IsRoadblockSite(const APawn& Player, const FGTPTrafficLane& Lane, int32 Level, FVector& OutPoint, FVector& OutAlong, double& OutDistance) const
{
	if (Lane.Points.Num() < 4)
	{
		return false;
	}
	const FVector2D Direction = GetTravelDirection(Player);
	const int32 Index = Lane.Points.Num() / 2;
	const FVector Point = Lane.Points[Index];
	const FVector Offset = Point - Player.GetActorLocation();
	const double Distance = FVector::Dist2D(Point, Player.GetActorLocation());
	// Level 0: 140 to 240 m straight ahead. The lanes of the quarter do not always go that far in the player's
	// direction (the map ends, the street bends): levels 1 and 2 look nearer and wider, and the EQS answer is a few
	// seconds old, so it is judged at level 1.
	static const double Mins[3] = {14000.0, 8000.0, 5000.0};
	static const double Maxs[3] = {24000.0, 27000.0, 30000.0};
	static const float Cosines[3] = {0.85f, 0.75f, 0.6f};
	static const double Parallels[3] = {0.6, 0.5, 0.35};
	const int32 L = FMath::Clamp(Level, 0, 2);
	const double Min = Mins[L];
	const double Max = Maxs[L];
	if (Distance < Min || Distance > Max
		|| !GTPPoliceTactics::IsAhead(static_cast<float>(Offset.X), static_cast<float>(Offset.Y), static_cast<float>(Direction.X), static_cast<float>(Direction.Y), Cosines[L]))
	{
		return false;
	}
	const FVector Along = (Lane.Points[Index + 1] - Point).GetSafeNormal2D();
	// A road the player is driving along, not a cross street.
	if (FMath::Abs(FVector2D::DotProduct(FVector2D(Along.X, Along.Y), Direction)) < Parallels[L])
	{
		return false;
	}
	// Not on the way of a flight a script drives (RoadblockFlightClearance).
	if (IsFlightLineKeptClear())
	{
		const float ToLine = FlightLineMetres(Point);
		if (ToLine >= 0.f && ToLine * 100.f < RoadblockFlightClearance)
		{
			return false;
		}
	}
	OutPoint = Point;
	OutAlong = Along;
	OutDistance = Distance;
	return true;
}

void AGTPPoliceDirector::KeepFlightLineClear(float Seconds)
{
	const UWorld* World = GetWorld();
	if (!World || RoadblockFlightClearance <= 0.f)
	{
		return;
	}
	const bool bNews = !IsFlightLineKeptClear();
	FlightLineClearUntil = World->GetTimeSeconds() + static_cast<double>(Seconds);
	if (bNews)
	{
		const bool bCircuit = Network && Network->GetFlightCircuit().IsValid() && Network->GetFlightCircuit().LinePoints() > 0;
		GTP_TRACE(LogGTPWanted, TEXT("police: a script drives the flight, %s"), bCircuit
			? *FString::Printf(TEXT("no roadblock within %.1f m of the driving lines of the map's circuit while it does"), RoadblockFlightClearance * 0.01f)
			: TEXT("and the map's circuit has no driving line: the roadblocks stand where they would for a player"));
	}
}

bool AGTPPoliceDirector::IsFlightLineKeptClear() const
{
	const UWorld* World = GetWorld();
	return World && RoadblockFlightClearance > 0.f && FlightLineClearUntil >= 0.0 && World->GetTimeSeconds() <= FlightLineClearUntil;
}

float AGTPPoliceDirector::FlightLineMetres(const FVector& Location) const
{
	if (!Network || !Network->GetFlightCircuit().IsValid())
	{
		return -1.f;
	}
	const float Metres = Network->GetFlightCircuit().MetresToLines(static_cast<float>(Location.X) * 0.01f, static_cast<float>(Location.Y) * 0.01f);
	return Metres < GTPFlightCircuit::NoLineMetres ? Metres : -1.f;
}

void AGTPPoliceDirector::CollectRoadblockSites(const APawn& Player, TArray<FVector>& OutSites, int32 Level) const
{
	if (!Network)
	{
		return;
	}
	for (const FGTPTrafficLane& Lane : Network->GetLanes())
	{
		FVector Point, Along;
		double Distance = 0.0;
		if (IsRoadblockSite(Player, Lane, Level, Point, Along, Distance))
		{
			OutSites.Add(Point);
		}
	}
}

bool AGTPPoliceDirector::PlaceRoadblockCar(const APawn& Player, int32 CarIndex, const FVector& Point, const FVector& Along, double Distance)
{
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceBlockGround), false);
	const bool bGround = GetWorld()->LineTraceSingleByChannel(Ground, Point + FVector(0.f, 0.f, 500.f), Point - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params);
	if (!bGround || Network->IsSeenByPlayer(Point))
	{
		GTP_TRACE(LogGTPWanted, TEXT("police: roadblock site refused at %.0f m: %s"), Distance * 0.01, bGround ? TEXT("in the player's view") : TEXT("no ground"));
		return false;
	}
	// Two cars nose to tail at an angle across the lane (a V): the first on the lane axis, the second a car's width further.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Along).GetSafeNormal();
	const float Side = CarIndex % 2 == 0 ? 1.f : -1.f;
	FVector Location = Point + Right * Side * 130.f;
	Location.Z = Ground.ImpactPoint.Z + 40.f;
	const FRotator Facing(0.f, static_cast<float>(Along.Rotation().Yaw) + Side * 60.f, 0.f);
	const FTransform Placement(Facing, Location);
	// Not on the roof of a car, not onto one, not onto somebody (the ground under Point, the room at the car's own place).
	FString TakenBy;
	FHitResult Room = Ground;
	Room.ImpactPoint = FVector(Location.X, Location.Y, Ground.ImpactPoint.Z);
	if (!IsSpawnPlaceClear(Placement, Room, true, TakenBy))
	{
		GTP_TRACE(LogGTPWanted, TEXT("police: roadblock site refused at %.0f m: %s"), Distance * 0.01, *TakenBy);
		return false;
	}
	if (SpawnUnit(Player.GetActorLocation(), &Placement, true))
	{
		GTP_TRACE(LogGTPWanted, TEXT("police: roadblock car %d at %s, %.0f m ahead, %.1f m from the flight circuit's line (-1 = no circuit)%s"), CarIndex,
			*Location.ToCompactString(), Distance * 0.01, FlightLineMetres(Location),
			IsFlightLineKeptClear() ? TEXT(", kept off it for the script that drives the flight") : TEXT(""));
		return true;
	}
	return false;
}

bool AGTPPoliceDirector::SpawnRoadblockCar(const APawn& Player, int32 CarIndex)
{
	const TArray<FGTPTrafficLane>& Lanes = Network->GetLanes();
	FRandomStream& Dice = Random;
	// Sites chosen by EQS (a generator over the lanes, UGTPPoliceQueries) first.
	if (Queries && Queries->GetRoadblockSites().Num() > 0)
	{
		const TArray<FVector>& Sites = Queries->GetRoadblockSites();
		for (int32 Try = 0; Try < 6; ++Try)
		{
			const FVector Site = Sites[Dice.RandRange(0, Sites.Num() - 1)];
			for (const FGTPTrafficLane& Lane : Lanes)
			{
				FVector Point, Along;
				double Distance = 0.0;
				if (Lane.Points.Num() >= 4 && FVector::DistSquared(Lane.Points[Lane.Points.Num() / 2], Site) < 100.0
					&& IsRoadblockSite(Player, Lane, 1, Point, Along, Distance))
				{
					if (PlaceRoadblockCar(Player, CarIndex, Point, Along, Distance))
					{
						return true;
					}
					break;
				}
			}
		}
	}
	// No answer yet, or nothing fit: look at every lane (at most every 6 s), from the strict window to the widest.
	int32 Found[3] = {0, 0, 0};
	for (int32 Level = 0; Level < 3; ++Level)
	{
		TArray<FVector> Fallback;
		CollectRoadblockSites(Player, Fallback, Level);
		Found[Level] = Fallback.Num();
		for (int32 Try = 0; Try < 8 && Fallback.Num() > 0; ++Try)
		{
			const FVector Site = Fallback[Dice.RandRange(0, Fallback.Num() - 1)];
			for (const FGTPTrafficLane& Lane : Lanes)
			{
				FVector Point, Along;
				double Distance = 0.0;
				if (Lane.Points.Num() >= 4 && FVector::DistSquared(Lane.Points[Lane.Points.Num() / 2], Site) < 100.0
					&& IsRoadblockSite(Player, Lane, Level, Point, Along, Distance))
				{
					if (PlaceRoadblockCar(Player, CarIndex, Point, Along, Distance))
					{
						return true;
					}
					break;
				}
			}
		}
	}
	GTP_TRACE(LogGTPWanted, TEXT("police: no roadblock placed: lane sites ahead %d / %d / %d (strict, wide, widest), %d from EQS, %d lanes, player %.0f km/h, %d units%s"),
		Found[0], Found[1], Found[2], Queries ? Queries->GetRoadblockSites().Num() : 0, Lanes.Num(), Player.GetVelocity().Size2D() * 0.036f, Units.Num(),
		IsFlightLineKeptClear() ? TEXT("; a script drives the flight: the sites on its way are not counted") : TEXT(""));
	return false;
}

bool AGTPPoliceDirector::SeatDriver(FUnit& Unit)
{
	AGTPVehicle* Car = Unit.Car.Get();
	TSubclassOf<ACharacter> DriverClass = LoadedDriverClass.Get() != nullptr ? LoadedDriverClass : Network->LoadedDriverClass;
	if (!Car || !DriverClass)
	{
		return false;
	}
	FActorSpawnParameters DriverParams;
	DriverParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Driver = GetWorld()->SpawnActor<ACharacter>(DriverClass, Car->GetActorTransform(), DriverParams);
	if (!Driver)
	{
		return false;
	}
	Driver->AIControllerClass = AAIController::StaticClass();
	Driver->SpawnDefaultController();
	Car->SeatOccupant(Driver);
	Network->MakeCheap(Car, Driver);
	if (!Car->HasAIDriver())
	{
		// Not seated: do not leave an orphan character (and its controller) behind.
		if (AController* Controller = Driver->GetController())
		{
			Controller->Destroy();
		}
		Driver->Destroy();
		return false;
	}
	return true;
}

void AGTPPoliceDirector::UpdateUnitDetail(AGTPVehicle* Car, float DistanceToView) const
{
	// Same levers as the traffic cars (shadow, forced LOD, draw distance) with the tighter police distances.
	USkeletalMeshComponent* Body = Car ? Car->GetMesh() : nullptr;
	if (!Body)
	{
		return;
	}
	const bool bNear = DistanceToView < LiteDetailDistance;
	if (static_cast<bool>(Body->CastShadow) != bNear)
	{
		Body->SetCastShadow(bNear);
	}
	const int32 Forced = bNear ? 0 : FMath::Clamp(Network->FarLOD + 1, 0, Body->GetNumLODs());
	if (Body->GetForcedLOD() != Forced)
	{
		Body->SetForcedLOD(Forced);
	}
	const bool bDrawn = Network->DrawDistance <= 0.f || DistanceToView < Network->DrawDistance;
	if (Body->IsVisible() != bDrawn)
	{
		Body->SetVisibility(bDrawn, true);
	}
}

void AGTPPoliceDirector::AddLivery(AGTPVehicle& Car)
{
	int32 LiverySwitch = 1;
	FParse::Value(FCommandLine::Get(), TEXT("PoliceLivery="), LiverySwitch);
	USkeletalMeshComponent* Body = Car.GetMesh();
	if (LiverySwitch == 0 || !Body)
	{
		return;
	}
	const bool bFirst = !bLiveryLoaded;
	if (bFirst)
	{
		bLiveryLoaded = true;
		LoadedLiveryBand = LiveryBandMaterial.IsValid() ? Cast<UMaterialInterface>(LiveryBandMaterial.TryLoad()) : nullptr;
		// Asked for quietly: the stripe was added after the band, a project that has not made it yet goes without.
		LoadedLiveryStripe = LiveryStripeMaterial.IsValid()
			? LoadObject<UMaterialInterface>(nullptr, *LiveryStripeMaterial.ToString(), nullptr, LOAD_NoWarn | LOAD_Quiet) : nullptr;
		// What the folder holds, asked once: only the paints that are there are loaded (gate 75: 30 of the 32
		// slots were asked for nothing, a line each in the log). No answer: every slot is asked once, as before.
		TArray<FAssetData> InFolder;
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get().GetAssetsByPath(FName(*LiveryPaintFolder), InFolder, false);
		for (const FAssetData& Found : InFolder)
		{
			const FString FoundName = Found.AssetName.ToString();
			if (FoundName.StartsWith(TEXT("MI_GTP_PolicePaint_")))
			{
				LiveryPaintListed.Add(FoundName);
			}
		}
	}
	// What draws the car: its body and, on City Sample's cars, the look parts posed by it (their body is a rig
	// without geometry: painting it alone showed nothing). The paint is asked for by mesh name and slot number:
	// absent for most slots (glass, tyres, lights), and that is not an error.
	TArray<USkeletalMeshComponent*> PaintedParts;
	PaintedParts.Add(Body);
	for (const TObjectPtr<USkeletalMeshComponent>& LookPart : Car.GetLookComponents())
	{
		if (LookPart)
		{
			PaintedParts.Add(LookPart.Get());
		}
	}
	int32 Painted = 0;
	int32 SlotCount = 0;
	FString PaintedNames;
	for (USkeletalMeshComponent* PaintedPart : PaintedParts)
	{
		const USkeletalMesh* PartAsset = PaintedPart->GetSkeletalMeshAsset();
		if (!PartAsset)
		{
			continue;
		}
		for (int32 Slot = 0; Slot < PaintedPart->GetNumMaterials(); ++Slot)
		{
			++SlotCount;
			const FString PaintName = FString::Printf(TEXT("MI_GTP_PolicePaint_%s_%d"), *PartAsset->GetName(), Slot);
			UMaterialInterface* PaintMaterial = nullptr;
			if (const TObjectPtr<UMaterialInterface>* Known = LoadedLiveryPaint.Find(PaintName))
			{
				PaintMaterial = Known->Get();
			}
			else
			{
				if (LiveryPaintListed.Num() == 0 || LiveryPaintListed.Contains(PaintName))
				{
					PaintMaterial = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *LiveryPaintFolder, *PaintName, *PaintName),
						nullptr, LOAD_NoWarn | LOAD_Quiet);
				}
				LoadedLiveryPaint.Add(PaintName, TObjectPtr<UMaterialInterface>(PaintMaterial));
			}
			if (PaintMaterial)
			{
				PaintedPart->SetMaterial(Slot, PaintMaterial);
				++Painted;
				if (bFirst)
				{
					PaintedNames += FString::Printf(TEXT(" %s"), *PaintName);
				}
			}
		}
	}
	// The car's own box, in its mesh's space: the band goes across it at door height, between the wheel arches.
	const FBox Box = Body->CalcBounds(FTransform::Identity).GetBox();
	const FVector Size = Box.GetSize();
	const FVector Middle(Box.GetCenter().X + LiveryBandForward, Box.GetCenter().Y, Box.Min.Z + Size.Z * LiveryBandHeightShare);
	const bool bRoomForBand = Size.X > 100.0 && Size.Y > 50.0;
	if (LoadedLiveryBand && bRoomForBand)
	{
		UDecalComponent* Band = NewObject<UDecalComponent>(&Car);
		Band->SetDecalMaterial(LoadedLiveryBand);
		// A decal projects along its own X: turned to point across the car, it paints both sides at once. Its box:
		// the car's half width and a little more deep, the band's half length, the band's half height.
		Band->DecalSize = FVector(Size.Y * 0.5 + 10.0, Size.X * LiveryBandLengthShare, LiveryBandHalfHeight);
		// Never faded out with distance: a police car is told from far away.
		Band->FadeScreenSize = 0.f;
		Band->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Band->SetRelativeLocationAndRotation(Middle, FRotator(0.f, 90.f, 0.f));
		Band->RegisterComponent();
	}
	if (LoadedLiveryStripe && bRoomForBand)
	{
		// The same box as the band, thinner, just above it.
		const FVector StripeMiddle = Middle + FVector(0.0, 0.0, LiveryBandHalfHeight + LiveryStripeGap + LiveryStripeHalfHeight);
		UDecalComponent* RedStripe = NewObject<UDecalComponent>(&Car);
		RedStripe->SetDecalMaterial(LoadedLiveryStripe);
		RedStripe->DecalSize = FVector(Size.Y * 0.5 + 10.0, Size.X * LiveryBandLengthShare, LiveryStripeHalfHeight);
		RedStripe->FadeScreenSize = 0.f;
		RedStripe->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		RedStripe->SetRelativeLocationAndRotation(StripeMiddle, FRotator(0.f, 90.f, 0.f));
		RedStripe->RegisterComponent();
	}
	if (bFirst)
	{
		GTP_TRACE(LogGTPWanted, TEXT("police: livery on %s: band %s, stripe %s, %d of %d material slot(s) painted on %d mesh(es):%s (from %s, %d paint(s) listed there); "
			"car box %.0f x %.0f x %.0f cm, band %.0f cm long and %.0f high, its middle %.0f cm above the box's lowest point"), *Car.GetName(),
			LoadedLiveryBand ? *LoadedLiveryBand->GetName() : TEXT("NOT THERE (tools/police/unreal/make_police_look.py makes it)"),
			LoadedLiveryStripe ? *LoadedLiveryStripe->GetName() : TEXT("not there"), Painted, SlotCount, PaintedParts.Num(),
			PaintedNames.IsEmpty() ? TEXT(" none") : *PaintedNames, *LiveryPaintFolder, LiveryPaintListed.Num(), Size.X, Size.Y, Size.Z,
			Size.X * LiveryBandLengthShare * 2.0, LiveryBandHalfHeight * 2.f, Size.Z * LiveryBandHeightShare);
	}
}

void AGTPPoliceDirector::AddLightBar(FUnit& Unit)
{
	AGTPVehicle* Car = Unit.Car.Get();
	if (!Car)
	{
		return;
	}
	for (int32 Side = 0; Side < (bPoliceLite ? 1 : 2); ++Side)
	{
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Car);
		Light->SetupAttachment(Car->GetRootComponent());
		Light->SetRelativeLocation(LightBarOffset + FVector(0.f, Side == 0 ? -30.f : 30.f, 0.f));
		Light->SetIntensityUnits(ELightUnits::Candelas);
		// Readable from the end of the street: bright and wide, no shadow.
		Light->SetIntensity(3000.f);
		Light->SetAttenuationRadius(4500.f);
		Light->SetLightColor(Side == 0 ? FLinearColor(0.1f, 0.25f, 1.f) : FLinearColor(1.f, 0.08f, 0.05f));
		Light->SetCastShadows(false);
		Light->SetVisibility(false);
		Light->RegisterComponent();
		Unit.Lights.Add(Light);
	}
}

void AGTPPoliceDirector::BlinkLights(FUnit& Unit, bool bOn, bool bPhase)
{
	// 0 = off, 1 = blue, 2 = red: touch the lights only when the state changes.
	const int8 Wanted = !bOn ? 0 : (bPhase ? 1 : 2);
	if (Unit.LightState == Wanted)
	{
		return;
	}
	Unit.LightState = Wanted;
	if (bPoliceLite && Unit.Lights.Num() == 1)
	{
		// One light whose colour alternates: half the lights of the full bar.
		if (UPointLightComponent* Light = Unit.Lights[0].Get())
		{
			if (Wanted != 0)
			{
				Light->SetLightColor(Wanted == 1 ? FLinearColor(0.1f, 0.25f, 1.f) : FLinearColor(1.f, 0.08f, 0.05f));
			}
			Light->SetVisibility(Wanted != 0);
		}
		return;
	}
	for (int32 Index = 0; Index < Unit.Lights.Num(); ++Index)
	{
		if (UPointLightComponent* Light = Unit.Lights[Index].Get())
		{
			Light->SetVisibility(Wanted == Index + 1);
		}
	}
}

void AGTPPoliceDirector::UpdateSiren(FUnit& Unit, int32 Stars, bool bPursuit)
{
	const EGTPSirenMode Mode = Stars <= 0 ? EGTPSirenMode::Off : (bPursuit ? EGTPSirenMode::Yelp : EGTPSirenMode::TwoTone);
	if (Unit.SirenState == static_cast<int8>(Mode))
	{
		return;
	}
	Unit.SirenState = static_cast<int8>(Mode);
	// Null when game audio is off (-Audio=0): nothing to do then.
	UGTPSirenComponent::SetSiren(Unit.Car.Get(), Mode);
}

bool AGTPPoliceDirector::HasLineOfSight(const AGTPVehicle* Car, const AActor& Player, const FVector& Target) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceLOS), false, Car);
	Params.AddIgnoredActor(&Player);
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Car->GetActorLocation() + FVector(0.f, 0.f, 150.f),
		Target + FVector(0.f, 0.f, 100.f), ECC_Visibility, Params);
}

float AGTPPoliceDirector::SceneryAhead(const AGTPVehicle& Car, float YawOffsetRadians, float LengthCm, float RadiusCm, float& OutLateralM) const
{
	OutLateralM = 0.f;
	// What does not move: buildings, walls, the street's furniture (instanced meshes). Cars and people are the
	// other look's (UGTPAIDriverComponent::ProbeAhead), and the car's own parts are not looked at.
	const FVector Forward = Car.GetActorForwardVector().GetSafeNormal2D();
	const FRotator Heading(0.f, static_cast<float>(Forward.Rotation().Yaw) + FMath::RadiansToDegrees(YawOffsetRadians), 0.f);
	const FVector Along = Heading.Vector();
	const FVector Start = Car.GetActorLocation() + FVector(0.f, 0.f, SceneryLookHeight) + Forward * SceneryLookStartCm;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceScenery), false, &Car);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	if (!GetWorld()->SweepSingleByObjectType(Hit, Start, Start + Along * (LengthCm + SceneryBumperCm), Heading.Quaternion(), Objects,
		FCollisionShape::MakeBox(FVector(10.f, RadiusCm, SceneryLookHalfHeight)), Params))
	{
		return -1.f;
	}
	// A slope or a ramp is ground, not scenery.
	if (Hit.ImpactNormal.Z > 0.6f)
	{
		return -1.f;
	}
	// The wide look of a car going round something starts beside its own flanks: what stands there (a wall it
	// drives along) is not on its way. Looked at again no wider than the car.
	const float NarrowCm = AvoidSettings.LookRadiusM * 100.f;
	if (Hit.bStartPenetrating && RadiusCm > NarrowCm + 1.f)
	{
		return SceneryAhead(Car, YawOffsetRadians, LengthCm, NarrowCm, OutLateralM);
	}
	OutLateralM = static_cast<float>(FVector::DotProduct(Hit.ImpactPoint - Start, FVector::CrossProduct(FVector::UpVector, Along))) * WantedCmToM;
	return FMath::Max(0.f, static_cast<float>(Hit.Distance) - SceneryBumperCm) * WantedCmToM;
}

bool AGTPPoliceDirector::IsWayClear(const AGTPVehicle& Car, const FVector& Target) const
{
	const FVector From = Car.GetActorLocation() + FVector(0.f, 0.f, SceneryLookHeight);
	const FVector End(Target.X, Target.Y, Target.Z + SceneryLookHeight);
	const FVector Line = (End - From).GetSafeNormal2D();
	// Looked at up to 3 m short of him: what he stands against (the block of gate 75) is not between them.
	const float Reach = static_cast<float>(FVector::Dist2D(From, End)) - 300.f;
	if (Line.IsNearlyZero() || Reach <= 0.f)
	{
		return true;
	}
	const FVector To = From + (End - From).GetSafeNormal() * Reach;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceWayClear), false, &Car);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FHitResult Hit;
	// As wide as the car: a way it can take, not a line it can see along.
	if (!GetWorld()->SweepSingleByObjectType(Hit, From, To, FRotator(0.f, static_cast<float>(Line.Rotation().Yaw), 0.f).Quaternion(), Objects,
		FCollisionShape::MakeBox(FVector(10.f, 100.f, SceneryLookHalfHeight)), Params))
	{
		return true;
	}
	return Hit.ImpactNormal.Z > 0.6f;
}

void AGTPPoliceDirector::DriveStraight(FUnit& Unit, const FVector& Target, float MaxSpeedKmh)
{
	AGTPVehicle* Car = Unit.Car.Get();
	const FVector Location = Car->GetActorLocation();
	const FVector Direction = (Target - Location).GetSafeNormal2D();
	std::vector<FGTPDrivePoint> Path;
	Path.push_back({static_cast<float>(Location.X) * WantedCmToM, static_cast<float>(Location.Y) * WantedCmToM});
	const FVector End = Target + Direction * 2000.f;
	Path.push_back({static_cast<float>(End.X) * WantedCmToM, static_cast<float>(End.Y) * WantedCmToM});
	int Closest = 0;
	const float Obstacle = ObstacleAhead(Unit, *Car, 4000.f, 100.f);
	const float Speed = Car->GetSpeedKmh() / 3.6f;
	const FGTPDriveCommand Command = GTPDrive::Decide(Path, Path[0].X, Path[0].Y, FMath::DegreesToRadians(static_cast<float>(Car->GetActorRotation().Yaw)),
		Speed, MaxSpeedKmh / 3.6f, Obstacle, Unit.Settings, Closest);
	ApplyDrive(Unit, Command, Command.Brake >= 1.f && Speed < 0.5f);
	Unit.bOnLanes = false;
}

float AGTPPoliceDirector::ObstacleAhead(FUnit& Unit, const AGTPVehicle& Car, float LengthCm, float RadiusCm)
{
	Unit.LastObstacle = -1.f;
	Unit.LastMet = GTPPoliceTactics::EMet::Nothing;
	// Pushing through what held it up (RamUntil): nothing is looked at for those seconds.
	if (GetWorld()->GetTimeSeconds() < Unit.RamUntil)
	{
		return -1.f;
	}
	// -PoliceRamBlind: from three stars a unit looks at nothing ahead of it, as until gate 101.
	static const bool bRamBlind = FParse::Param(FCommandLine::Get(), TEXT("PoliceRamBlind"));
	if (Profile.bRam && bRamBlind)
	{
		return -1.f;
	}
	AActor* MetActor = nullptr;
	const float Metres = UGTPAIDriverComponent::ProbeAhead(&Car, LengthCm, RadiusCm, &MetActor);
	if (MetActor)
	{
		const APawn* Quarry = UGameplayStatics::GetPlayerPawn(this, 0);
		if (MetActor == Quarry)
		{
			Unit.LastMet = GTPPoliceTactics::EMet::Quarry;
		}
		else if (MetActor->ActorHasTag(GTPPolice::PoliceTag) || MetActor->IsA<AGTPPoliceOfficer>())
		{
			Unit.LastMet = GTPPoliceTactics::EMet::Police;
		}
		else if (MetActor->IsA<ACharacter>() || MetActor->IsA<AGTPPedestrian>())
		{
			Unit.LastMet = GTPPoliceTactics::EMet::Person;
		}
		else
		{
			Unit.LastMet = GTPPoliceTactics::EMet::Other;
		}
	}
	// A unit that rams brakes for everything but the man it is after and his car (gate 93: four units off their wheels).
	Unit.LastObstacle = GTPPoliceTactics::ObstacleFor(Metres, Unit.LastMet, Profile.bRam);
	return Unit.LastObstacle;
}

void AGTPPoliceDirector::ApplyDrive(FUnit& Unit, const FGTPDriveCommand& Command, bool bHandbrake)
{
	AGTPVehicle* Car = Unit.Car.Get();
	// What the driving asked for is kept: the recovery and the logs judge the car on it.
	Unit.LastThrottle = Command.Throttle;
	Unit.LastBrake = Command.Brake;
	Unit.LastSteer = Command.Steering;
	// What the car gets follows the steering: full lock with the throttle held down put the units on two
	// wheels, then on none (gate of candidate 48).
	float Throttle = Command.Throttle;
	float Brake = Command.Brake;
	float Steering = Command.Steering;
	if (bAvoidScenery)
	{
		// Scenery on the car's way (GTPUnitAvoid.h): looked for along its heading leaned into the turn, then to
		// both sides when something is there. The car goes round it or stops short of it.
		const float SpeedNow = Car->GetSpeedKmh() / 3.6f;
		const float LookM = FGTPUnitAvoid::LookMetres(SpeedNow, AvoidSettings);
		const float Lean = Steering * AvoidSettings.LeanRadians;
		float LateralM = 0.f;
		float OtherLateralM = 0.f;
		// Rolling backwards the look ahead says nothing.
		const float AheadM = SpeedNow > -0.5f ? SceneryAhead(*Car, Lean, LookM * 100.f, Unit.Avoid.LookRadius(AvoidSettings) * 100.f, LateralM) : -1.f;
		float LeftM = -1.f;
		float RightM = -1.f;
		if (AheadM >= 0.f)
		{
			LeftM = SceneryAhead(*Car, Lean - AvoidSettings.SideRadians, LookM * 100.f, AvoidSettings.LookRadiusM * 100.f, OtherLateralM);
			RightM = SceneryAhead(*Car, Lean + AvoidSettings.SideRadians, LookM * 100.f, AvoidSettings.LookRadiusM * 100.f, OtherLateralM);
		}
		Unit.Avoid.Step(GetWorld()->GetDeltaSeconds(), SpeedNow, LookM, AheadM, LateralM, LeftM, RightM, AvoidSettings, Throttle, Brake, Steering);
		// A car held all the same backs off with its wheels turned away from where it steered: away from the way round too.
		Unit.LastSteer = Steering;
	}
	GTPUnitRecovery::Tame(Throttle, Brake, Steering, Car->GetSpeedKmh() / 3.6f, Unit.Settings.MaxLateralAccel, RecoverySettings);
	Car->SetAIDriveInput(Throttle, Brake, Steering, bHandbrake);
}

void AGTPPoliceDirector::TraceUnitTrouble(const FUnit& Unit, const TCHAR* What) const
{
	AGTPVehicle* Car = Unit.Car.Get();
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Car)
	{
		return;
	}
	// What the nose is against: static and moving things alike, from inside the car to 3 m past the bumper.
	const FVector Forward = Car->GetActorForwardVector().GetSafeNormal2D();
	const FVector Start = Car->GetActorLocation() + FVector(0.f, 0.f, 70.f) + Forward * 100.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceTrouble), false, Car);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_Pawn);
	Objects.AddObjectTypesToQuery(ECC_Vehicle);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FHitResult Hit;
	FString Ahead = TEXT("nothing within 3 m of the bumper");
	if (GetWorld()->SweepSingleByObjectType(Hit, Start, Start + Forward * 430.f, FQuat::Identity, Objects, FCollisionShape::MakeSphere(45.f), Params))
	{
		Ahead = FString::Printf(TEXT("%s (%s) %.0f cm from the bumper"), Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("no actor"),
			Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("no component"), Hit.Distance - 130.f);
	}
	int32 Lane = INDEX_NONE, Point = 0;
	const FVector Location = Car->GetActorLocation();
	const float Yaw = static_cast<float>(Car->GetActorRotation().Yaw);
	FString OnLane = TEXT("no lane within 80 m");
	if (Network && Network->FindNearestLane(Location, Yaw, 8000.f, Lane, Point) && Network->GetLanes().IsValidIndex(Lane)
		&& Network->GetLanes()[Lane].Points.IsValidIndex(Point))
	{
		OnLane = FString::Printf(TEXT("%.0f m from lane %d"), FVector::Dist2D(Location, Network->GetLanes()[Lane].Points[Point]) * WantedCmToM, Lane);
	}
	// On its lanes: how far the car is from the line it follows (0 = the thing ahead stands on the lane itself).
	const float OffPath = Unit.bOnLanes && Unit.Follower.IsActive() ? Unit.Follower.OffPathMetres(Location) : -1.f;
	GTP_TRACE(LogGTPWanted, TEXT("police unit: %s %s at %s yaw=%.0f up=%.2f, %d wheel(s) on the ground, %.0f km/h, %s, lanes=%d (lane %d, %.1f m off its path), back-offs %d (%d here), %.0f m from the player; ahead: %s"),
		*Car->GetName(), What, *Location.ToCompactString(), Yaw, Car->GetActorUpVector().Z, PoliceWheelsOnGround(*Car), Car->GetSpeedKmh(), *OnLane,
		Unit.bOnLanes ? 1 : 0, Unit.Follower.GetLane(), OffPath, Unit.Recovery.BackOffs, Unit.Recovery.BackOffsHere,
		Player ? FVector::Dist2D(Location, Player->GetActorLocation()) * WantedCmToM : -1.0, *Ahead);
}

void AGTPPoliceDirector::DriveOnLanes(FUnit& Unit, const FVector& Goal, bool bHasGoal, float MaxSpeedKmh)
{
	AGTPVehicle* Car = Unit.Car.Get();
	const FVector Location = Car->GetActorLocation();
	const float Yaw = static_cast<float>(Car->GetActorRotation().Yaw);
	const float NowSeconds = static_cast<float>(GetWorld()->GetTimeSeconds());
	// After the end of a dead end the lane to take is the one back the other way, until the car heads that way
	// (the driving turns it at full lock; held against a kerb it backs off, wheels the other way: a three-point turn),
	// and no longer than the half minute the dead end is kept out of its way.
	if (Unit.bTurnAround && (FMath::Abs(FRotator::NormalizeAxis(Yaw - Unit.TurnYaw)) < 60.f || NowSeconds >= Unit.AvoidUntil))
	{
		Unit.bTurnAround = false;
	}
	if (!Unit.bOnLanes || !Unit.Follower.IsActive())
	{
		const float LaneYaw = Unit.bTurnAround ? Unit.TurnYaw : Yaw;
		// The follower knows its goal and its settings before it plans: its first lanes are already the way to the goal.
		Unit.Follower = FGTPLaneFollower();
		Unit.Follower.bIgnoreLaneLimit = true;
		Unit.Follower.bRouteToGoal = true;
		Unit.Follower.bDriveCircuitLines = bUnitsOnCircuitLines;
		Unit.Follower.AvoidLane = NowSeconds < Unit.AvoidUntil ? Unit.AvoidLane : INDEX_NONE;
		// The unit's chase settings on the lanes too: the follower's own are the calm city ones
		// (bends at 2.2 m/s2, soft throttle), which held a chase at 105 km/h near 70.
		Unit.Follower.SetSettings(Unit.Settings);
		if (bHasGoal)
		{
			Unit.Follower.SetGoal(Goal);
		}
		if (!Unit.Follower.StartNear(Network, Location, LaneYaw, 1500.f) && !Unit.Follower.StartNear(Network, Location, LaneYaw, 8000.f))
		{
			// Off the lanes (after a straight chase): head for the goal in a straight line, slowly, until a lane is near.
			Unit.Follower = FGTPLaneFollower();
			if (bHasGoal)
			{
				DriveStraight(Unit, Goal, FMath::Min(MaxSpeedKmh, 30.f));
			}
			else
			{
				Car->SetAIDriveInput(0.f, 1.f, 0.f, true);
			}
			return;
		}
		Unit.bOnLanes = true;
	}
	if (bHasGoal)
	{
		Unit.Follower.SetGoal(Goal);
	}
	else
	{
		Unit.Follower.ClearGoal();
	}
	// Watched as far as the car needs to stop from its speed (40 m is the braking distance of 85 km/h).
	const float ProbeCm = FMath::Max(4000.f, GTPDrive::PathNeeded(FMath::Abs(Car->GetSpeedKmh()) / 3.6f, Unit.Settings) * 100.f);
	const float Obstacle = ObstacleAhead(Unit, *Car, ProbeCm, 90.f);
	FGTPDriveCommand Command;
	const int32 LaneBefore = Unit.Follower.GetLane();
	if (!Unit.Follower.Step(Location, Yaw, Car->GetSpeedKmh(), MaxSpeedKmh, Obstacle, Command))
	{
		// Stopped at the end of a dead end. Started again from here with its own heading, the follower found the same
		// lane and stopped again, for good (gate of candidate 49: a unit stood 9 s at the end of one). It turns round,
		// and that lane is kept out of its way for half a minute.
		Unit.bTurnAround = true;
		Unit.TurnYaw = FRotator::NormalizeAxis(Yaw + 180.f);
		Unit.AvoidLane = LaneBefore;
		Unit.AvoidUntil = NowSeconds + 30.f;
		GTP_TRACE(LogGTPWanted, TEXT("police unit: %s at the end of lane %d, a dead end, at %s: turning round"), *Car->GetName(), LaneBefore,
			*Location.ToCompactString());
		Unit.bOnLanes = false;
		Unit.LastThrottle = 0.f;
		Unit.LastBrake = 1.f;
		Unit.LastSteer = 0.f;
		Car->SetAIDriveInput(0.f, 1.f, 0.f, true);
		return;
	}
	ApplyDrive(Unit, Command, Command.Brake >= 1.f && Car->GetSpeedKmh() < 2.f);
}

void AGTPPoliceDirector::DriveUnit(FUnit& Unit, UGTPWantedComponent& Wanted, const AActor& Player, float DeltaSeconds)
{
	AGTPVehicle* Car = Unit.Car.Get();
	const int32 Stars = Wanted.GetStars();
	const FVector Location = Car->GetActorLocation();
	Unit.StoppedTime = FMath::Abs(Car->GetSpeedKmh()) < 1.f ? Unit.StoppedTime + DeltaSeconds : 0.f;
	if (Unit.bBlock)
	{
		// A roadblock car stands where it was put.
		Car->SetAIDriveInput(0.f, 1.f, 0.f, true);
		return;
	}

	// Held against something with the throttle down: back off, wheels turned the other way, then drive on.
	// Off its wheels, or held in the same place after three tries: the car is given up where it is (its
	// officers get out when the player is near, another car is sent), never moved back onto a lane.
	// Against the wanted player's own car a stopped unit is where it should be (it boxes him in, the arrest counts from there).
	const bool bAtPlayer = Stars > 0 && FVector::Dist2D(Location, Player.GetActorLocation()) < 1200.f;
	const FGTPUnitRecovery::EState Before = Unit.Recovery.State;
	const FGTPUnitRecovery::EState Recovery = Unit.Recovery.Step(DeltaSeconds, Car->GetSpeedKmh(), bAtPlayer ? 0.f : Unit.LastThrottle, Unit.LastSteer,
		PoliceWheelsOnGround(*Car), static_cast<float>(Car->GetActorUpVector().Z), static_cast<float>(Location.X) * WantedCmToM,
		static_cast<float>(Location.Y) * WantedCmToM, RecoverySettings);
	if (Recovery != Before && Recovery == FGTPUnitRecovery::EState::BackingOff)
	{
		ChaseTally.NoteBackedOff();
		TraceUnitTrouble(Unit, TEXT("held with the throttle down, backing off"));
	}
	else if (Recovery != Before && Recovery == FGTPUnitRecovery::EState::GivenUp)
	{
		ChaseTally.NoteGivenUp();
		TraceUnitTrouble(Unit, *FString::Printf(TEXT("given up (%s)"), ANSI_TO_TCHAR(GTPUnitRecovery::WhyName(Unit.Recovery.Why))));
	}
	if (Recovery == FGTPUnitRecovery::EState::GivenUp)
	{
		Unit.LastThrottle = 0.f;
		Unit.LastBrake = 1.f;
		Unit.LastSteer = 0.f;
		Unit.bOnLanes = false;
		Car->SetAIDriveInput(0.f, 1.f, 0.f, true);
		return;
	}
	if (Recovery == FGTPUnitRecovery::EState::BackingOff)
	{
		// The brake pedal backs a stopped car when asked to (AGTPVehicle::SetAIDriveInput, as the chase demo's car does).
		Unit.LastThrottle = 0.f;
		Unit.LastBrake = 0.f;
		Unit.bOnLanes = false;
		Car->SetAIDriveInput(0.f, RecoverySettings.BackPedal, Unit.Recovery.BackSteer, false, true);
		return;
	}

	if (Stars == 0)
	{
		// Level lost or arrest: back to ordinary driving on the lanes.
		DriveOnLanes(Unit, FVector::ZeroVector, false, 40.f);
		return;
	}
	const float ChaseKmh = Profile.ChaseKmh > 0.f ? Profile.ChaseKmh : GTPWanted::ChaseSpeedKmh(Stars);
	const float NowSeconds = static_cast<float>(GetWorld()->GetTimeSeconds());
	// Throttle held and the car does not move: not traffic, the vehicle itself (asleep, parked, no gear). Wake it.
	// (A car standing short of scenery it cannot get round is not asleep: its recovery backs it off.)
	if (Unit.StoppedTime > 1.f && Unit.LastThrottle > 0.5f && Unit.LastObstacle < 0.f && !Unit.Avoid.bStood)
	{
		// First gear forced every frame while the car does not move with the throttle held.
		if (UChaosVehicleMovementComponent* Move = Car->GetVehicleMovementComponent())
		{
			Move->SetTargetGear(1, true);
		}
	}
	if (Unit.StoppedTime > 3.f && Unit.LastThrottle > 0.8f && Unit.LastObstacle < 0.f && !Unit.Avoid.bStood && NowSeconds >= Unit.NextWakeTime)
	{
		Unit.NextWakeTime = NowSeconds + 5.f;
		if (UChaosVehicleMovementComponent* Move = Car->GetVehicleMovementComponent())
		{
			Move->SetParked(false);
			Move->SetHandbrakeInput(false);
			Move->SetUseAutomaticGears(true);
			Move->SetTargetGear(1, true);
		}
		if (USkeletalMeshComponent* Body = Car->GetMesh())
		{
			Body->WakeAllRigidBodies();
			GTP_TRACE(LogGTPWanted, TEXT("police unit: not moving with the throttle held, woken (simulating=%d gravity=%d mass=%.0f kg)"),
				Body->IsSimulatingPhysics() ? 1 : 0, Body->IsGravityEnabled() ? 1 : 0, Body->GetMass());
		}
	}
	// Slow behind something close ahead (a taxi, a queue): after 1.5 s the unit shoves through, the way the police do in a chase.
	// Not through a colleague nor a person; from three stars at any speed (GTPPoliceTactics::IsHeldUp).
	Unit.SlowTime = GTPPoliceTactics::IsHeldUp(Car->GetSpeedKmh(), Unit.LastObstacle, Unit.LastMet, Stars, Profile.bRam) ? Unit.SlowTime + DeltaSeconds : 0.f;
	if (Unit.SlowTime > 1.5f && NowSeconds >= Unit.RamUntil)
	{
		Unit.RamUntil = NowSeconds + 3.f;
		Unit.SlowTime = 0.f;
	}
	// Stopped far from the player in a chase: push through whatever is in the way for a few seconds.
	if (Unit.StoppedTime > 4.f && NowSeconds >= Unit.RamUntil + 1.f && FVector::Dist2D(Location, Player.GetActorLocation()) > 1500.f)
	{
		// The stopped time is kept: reset here every 4 s, it never reached the 12 s after which a stuck unit is taken away.
		Unit.RamUntil = NowSeconds + 3.f;
		Unit.bOnLanes = false;
		GTP_TRACE(LogGTPWanted, TEXT("police unit: stopped, pushing through (obstacle %.1f m)"), Unit.LastObstacle);
	}
	const bool bPursuit = Wanted.GetPhase() == EGTPWantedPhase::Pursuit;
	const FVector Target = bPursuit ? Player.GetActorLocation() : Wanted.GetLastKnownLocation();
	const float Distance = static_cast<float>(FVector::Dist2D(Location, Target));
	const bool bSight = bPursuit && HasLineOfSight(Car, Player, Target);
	// Near a player whose place is known, with no scenery between: straight at him, as when he is in sight. A car,
	// a passer-by or a tree's crown cuts the line of sight, and the lanes then took the unit round the block, away
	// from a player stopped off them (gate 75).
	const bool bNear = bPursuit && !bSight && Distance < CloseInDistance;
	if (!bNear)
	{
		Unit.bWayClear = false;
	}
	else if (NowSeconds >= Unit.NextWayLookTime)
	{
		Unit.NextWayLookTime = NowSeconds + 0.25f;
		Unit.bWayClear = IsWayClear(*Car, Target);
	}
	if (bSight || (bNear && Unit.bWayClear))
	{
		// A player on foot or nearly stopped is boxed in, not pushed away from his arrest: the unit slows as it
		// comes and stops CloseInStop from him (at gate 75 a unit ran into the stopped car at 3 stars and the
		// arrest, begun, was lost).
		const bool bHelpless = CloseInStop > 0.f && (Cast<AGTPVehicle>(&Player) == nullptr || Player.GetVelocity().Size() < 330.f);
		if (bHelpless && Distance < CloseInStop)
		{
			Unit.LastThrottle = 0.f;
			Unit.LastBrake = 1.f;
			Unit.LastSteer = 0.f;
			Unit.bOnLanes = false;
			Car->SetAIDriveInput(0.f, 1.f, 0.f, true);
			return;
		}
		float StraightKmh = ChaseKmh;
		if (bHelpless)
		{
			const float RoomM = (Distance - CloseInStop) * WantedCmToM;
			StraightKmh = FMath::Min(ChaseKmh, FMath::Max(FMath::Sqrt(2.f * Unit.Settings.ComfortDecel * RoomM) * 3.6f, 10.f));
		}
		DriveStraight(Unit, Target, StraightKmh);
	}
	else if (Distance > 2500.f)
	{
		// No way through the buildings: take the lanes, always towards the target.
		DriveOnLanes(Unit, Target, true, ChaseKmh * 0.9f);
	}
	else if (!bPursuit && Distance > 1200.f && HasLineOfSight(Car, Player, Target))
	{
		DriveStraight(Unit, Target, 40.f);
	}
	else
	{
		// Arrived at the search zone: roam the lanes there.
		DriveOnLanes(Unit, Target, true, 35.f);
	}
}

void AGTPPoliceDirector::CountUnits(int32& OutChasing, int32& OutBlocks, int32& OutOfficers) const
{
	OutChasing = 0;
	OutBlocks = 0;
	for (const FUnit& Unit : Units)
	{
		// A given-up car (off its wheels, held for good) is a wreck: neither chasing nor a roadblock.
		(Unit.bBlock ? OutBlocks : OutChasing) += Unit.Car.IsValid() && !Unit.Recovery.IsGivenUp() ? 1 : 0;
	}
	OutOfficers = Officers.Num();
}

void AGTPPoliceDirector::NearestUnits(const FVector& Where, float& OutChasing, float& OutBlock, float& OutOfficer) const
{
	OutChasing = OutBlock = OutOfficer = -1.f;
	auto Closer = [&Where](const AActor* Who, float& InOut)
	{
		if (IsValid(Who))
		{
			const float Metres = static_cast<float>(FVector::Dist(Who->GetActorLocation(), Where)) * WantedCmToM;
			InOut = InOut < 0.f ? Metres : FMath::Min(InOut, Metres);
		}
	};
	for (const FUnit& Unit : Units)
	{
		Closer(Unit.Car.Get(), Unit.bBlock ? OutBlock : OutChasing);
	}
	for (const TWeakObjectPtr<AGTPPoliceOfficer>& Officer : Officers)
	{
		Closer(Officer.Get(), OutOfficer);
	}
}

void AGTPPoliceDirector::RemoveUnit(int32 Index)
{
	FUnit& Unit = Units[Index];
	if (AGTPVehicle* Car = Unit.Car.Get())
	{
		if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
		{
			Wanted->UnregisterPolice(Car);
		}
		Network->Recycle(Car);
	}
	Units.RemoveAtSwap(Index);
}

void AGTPPoliceDirector::RemoveOfficer(int32 Index, UGTPWantedComponent& Wanted)
{
	if (AGTPPoliceOfficer* Officer = Officers[Index].Get())
	{
		Wanted.UnregisterOfficer(Officer);
		Officer->Destroy();
	}
	Officers.RemoveAtSwap(Index);
}

void AGTPPoliceDirector::ManageOfficers(UGTPWantedComponent& Wanted, const APawn& Player, int32 Stars, float Now, float DeltaSeconds)
{
	for (int32 Index = Officers.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(Officers[Index].Get()))
		{
			Officers.RemoveAtSwap(Index);
		}
	}
	if (MaxOfficers <= 0)
	{
		return;
	}
	NoStarsSeconds = Stars > 0 ? 0.f : NoStarsSeconds + DeltaSeconds;
	if (Stars == 0)
	{
		// The level is gone: officers go off duty out of sight, all of them after a minute.
		for (int32 Index = Officers.Num() - 1; Index >= 0; --Index)
		{
			const AGTPPoliceOfficer* Officer = Officers[Index].Get();
			if (NoStarsSeconds > 60.f || (NoStarsSeconds > 15.f && !Network->IsSeenByPlayer(Officer->GetActorLocation())))
			{
				RemoveOfficer(Index, Wanted);
			}
		}
		return;
	}
	for (int32 Index = 0; Index < Officers.Num(); ++Index)
	{
		Officers[Index]->SetSlot(Index, Officers.Num());
	}
	PlayerStillSeconds = Player.GetVelocity().Size() < 140.f ? PlayerStillSeconds + DeltaSeconds : 0.f;
	if (Now < NextDeployTime)
	{
		return;
	}
	// While searching, ask EQS for places to look; officers get the answer when it comes.
	if (Queries && Wanted.GetPhase() == EGTPWantedPhase::Searching && Officers.Num() > 0 && Now >= NextSearchQueryTime)
	{
		NextSearchQueryTime = Now + 6.f;
		Queries->RequestSearchPoints();
	}
	if (Queries && Queries->GetSearchPointsTime() > PushedSearchPointsTime)
	{
		PushedSearchPointsTime = Queries->GetSearchPointsTime();
		for (const TWeakObjectPtr<AGTPPoliceOfficer>& Officer : Officers)
		{
			if (Officer.IsValid())
			{
				Officer->SetSearchPoints(Queries->GetSearchPoints());
			}
		}
	}
	// On foot, or parked and not going anywhere: officers get out, point guns and close in.
	const bool bOnFoot = Cast<AGTPVehicle>(&Player) == nullptr || PlayerStillSeconds > 2.f;
	for (const FUnit& Unit : Units)
	{
		const AGTPVehicle* Car = Unit.Car.Get();
		if (!IsValid(Car) || Unit.Stage != 0)
		{
			continue;
		}
		const float Distance = static_cast<float>(FVector::Dist2D(Car->GetActorLocation(), Player.GetActorLocation()));
		if (!GTPPoliceTactics::ShouldDeploy(Tactics, Stars, bOnFoot, Distance, Unit.StoppedTime > 4.f || Unit.Recovery.IsGivenUp(), Officers.Num(), MaxOfficers))
		{
			continue;
		}
		const FTransform Where(Car->GetActorRotation(), Car->GetActorLocation() + Car->GetActorRightVector() * 220.f + FVector(0.f, 0.f, 100.f));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		if (AGTPPoliceOfficer* Officer = GetWorld()->SpawnActor<AGTPPoliceOfficer>(AGTPPoliceOfficer::StaticClass(), Where, Params))
		{
			Officers.Add(Officer);
			Wanted.RegisterOfficer(Officer);
			if (Senses)
			{
				Officer->AIControllerClass = AAIController::StaticClass();
				Officer->SpawnDefaultController();
				Senses->Attach(Cast<AAIController>(Officer->GetController()));
			}
			GTP_TRACE(LogGTPWanted, TEXT("police: officer out (%d) stars=%d"), Officers.Num(), Stars);
		}
		NextDeployTime = Now + 2.f;
		break;
	}
}

void AGTPPoliceDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Network)
	{
		for (TActorIterator<AGTPTrafficNetwork> It(GetWorld()); It; ++It)
		{
			Network = *It;
			break;
		}
	}
	UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this);
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Network || !Wanted || !Player || MaxUnits == 0)
	{
		return;
	}
	Wanted->SetPoliceAvailable(Network->GetLanes().Num() > 0 && (LoadedPoliceClass.Get() != nullptr || Network->LoadedVehicleClasses.Num() > 0));

	const int32 Stars = Wanted->GetStars();
	Profile = GTPPoliceTactics::Profile(Stars, MaxUnits);
	const int32 Desired = Profile.Cars;
	const float Now = static_cast<float>(GetWorld()->GetTimeSeconds());
	const bool bPhase = FMath::Fmod(Now, LightPeriod * 2.f) < LightPeriod;
	const APlayerController* Viewer = GetWorld()->GetFirstPlayerController();

	ManageOfficers(*Wanted, *Player, Stars, Now, DeltaSeconds);
	if (Stars > 0 && Now >= NextUnitTraceTime)
	{
		NextUnitTraceTime = Now + 3.f;
		for (const FUnit& Unit : Units)
		{
			if (AGTPVehicle* Car = Unit.Car.Get())
			{
				GTP_TRACE(LogGTPWanted, TEXT("police unit: dist=%.0f m speed=%.0f km/h lanes=%d stopped=%.0f s stage=%d obstacle=%.1f m met=%s throttle=%.2f brake=%.2f brain=%d steer=%.2f recovery=%d back-offs=%d wheels=%d up=%.2f scenery=%.1f m side=%d rounds=%d stood=%d straight=%d at %s"),
					FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) * 0.01f, Car->GetSpeedKmh(), Unit.bOnLanes ? 1 : 0, Unit.StoppedTime, Unit.Stage,
					Unit.LastObstacle, ANSI_TO_TCHAR(GTPPoliceTactics::MetName(Unit.LastMet)), Unit.LastThrottle, Unit.LastBrake, static_cast<int32>(Unit.Brain), Unit.LastSteer, static_cast<int32>(Unit.Recovery.State),
					Unit.Recovery.BackOffs, PoliceWheelsOnGround(*Car), Car->GetActorUpVector().Z, Unit.Avoid.LastAheadM, Unit.Avoid.Side, Unit.Avoid.Rounds,
					Unit.Avoid.Blocked, Unit.bWayClear ? 1 : 0, *Car->GetActorLocation().ToCompactString());
				if (const UChaosWheeledVehicleMovementComponent* Move = Cast<UChaosWheeledVehicleMovementComponent>(Car->GetVehicleMovementComponent()))
				{
					GTP_TRACE(LogGTPWanted, TEXT("police unit physics: gear=%d rpm=%.0f forward=%.0f cm/s simulating=%d class=%s"), Move->GetCurrentGear(),
						Move->GetEngineRotationSpeed(), Move->GetForwardSpeed(), Car->GetMesh() && Car->GetMesh()->IsSimulatingPhysics() ? 1 : 0,
						*Car->GetClass()->GetName());
				}
			}
		}
	}

	for (int32 Index = Units.Num() - 1; Index >= 0; --Index)
	{
		FUnit& Unit = Units[Index];
		AGTPVehicle* Car = Unit.Car.Get();
		if (!IsValid(Car))
		{
			Units.RemoveAtSwap(Index);
			continue;
		}
		if (Unit.Stage == 1)
		{
			if (!SeatDriver(Unit))
			{
				Car->Destroy();
				Units.RemoveAtSwap(Index);
				continue;
			}
			Unit.Stage = 2;
			continue;
		}
		if (Unit.Stage == 2)
		{
			AddLightBar(Unit);
			AddLivery(*Car);
			Wanted->RegisterPolice(Car);
			if (!bWarningBound)
			{
				bWarningBound = true;
				Wanted->OnArrestWarning.AddUObject(this, &AGTPPoliceDirector::OnArrestWarning);
			}
			if (Senses)
			{
				Wanted->SetSenses(Senses);
				Senses->Attach(Cast<AAIController>(Car->GetController()));
			}
			Unit.Stage = 0;
			continue;
		}
		if (!Car->HasAIDriver() || Car->IsWrecked())
		{
			// Stolen or destroyed: no longer a unit (a wreck stays where it is).
			Wanted->UnregisterPolice(Car);
			BlinkLights(Unit, false, false);
			UpdateSiren(Unit, 0, false);
			Units.RemoveAtSwap(Index);
			continue;
		}
		Unit.IdleTime = Stars > 0 ? 0.f : Unit.IdleTime + DeltaSeconds;
		// No unit is ever moved by hand: one left behind drives back, or is taken away beyond the leash and replaced
		// by a car made far from the player (ReplacementMinDistance).
		DriveUnit(Unit, *Wanted, *Player, DeltaSeconds);
		if (Stars > 0 && !Unit.bBlock && !Unit.Recovery.IsGivenUp())
		{
			ChaseTally.AddUnit(Car->GetSpeedKmh(), static_cast<float>(FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation())) * 0.01f, ChaseTallySettings);
		}
		// With -VehicleLod=1 the light bar is dark beyond LightBarDistance: two lights nobody can tell apart from far away.
		const float LightLimit = bPoliceLite ? LiteLightDistance : LightBarDistance;
		const bool bLightsNear = !(Network->bVehicleLod || bPoliceLite)
			|| FVector::Dist(Car->GetActorLocation(), Player->GetActorLocation()) < LightLimit;
		BlinkLights(Unit, Stars > 0 && bLightsNear, bPhase);
		GTPPoliceBrain::FInput BrainIn;
		BrainIn.Stars = Stars;
		BrainIn.bSensed = Wanted->IsSeenNow();
		BrainIn.SecondsSinceSensed = Wanted->GetSecondsSinceSeen();
		BrainIn.DistanceM = static_cast<float>(FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation())) * 0.01f;
		BrainIn.bTargetHelpless = Cast<AGTPVehicle>(Player) == nullptr || Player->GetVelocity().Size() < 330.f;
		BrainIn.bIsRoadblock = Unit.bBlock;
		Unit.Brain = GTPPoliceBrain::Decide(BrainIn, Unit.Brain);
		UpdateSiren(Unit, Stars, GTPPoliceBrain::WantsYelp(Unit.Brain));

		const FVector View = Viewer && Viewer->PlayerCameraManager ? Viewer->PlayerCameraManager->GetCameraLocation() : Player->GetActorLocation();
		if (bPoliceLite)
		{
			UpdateUnitDetail(Car, static_cast<float>(FVector::Dist(View, Car->GetActorLocation())));
		}
		else
		{
			Network->UpdateDetail(Car, static_cast<float>(FVector::Dist(View, Car->GetActorLocation())));
		}

		// Taken away out of sight: too many for the level, too far, stuck, or the level is gone.
		const bool bTooFar = FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) > LeashDistance;
		const bool bStuck = Unit.Recovery.IsGivenUp()
			|| (Unit.StoppedTime > 12.f && !(Stars > 0 && FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) < 1500.f));
		bool bBlockDone = false;
		if (Unit.bBlock)
		{
			// A roadblock goes once the player is past it, or the level is gone.
			const FVector Relative = Car->GetActorLocation() - Player->GetActorLocation();
			// Passed by a player driving away from it: one who stands has a velocity that points anywhere (gate 75:
			// the roadblock ahead of a stopped player was taken away and made again every 2 s, 40 cars in 80 s).
			const bool bPassed = Relative.Size2D() > 2500.f && Player->GetVelocity().Size2D() > RoadblockMinSpeed * 0.5f
				&& FVector::DotProduct(Relative, Player->GetVelocity()) < 0.f;
			bBlockDone = bPassed || (Stars == 0 && Unit.IdleTime >= 8.f);
		}
		if ((bTooFar || bStuck || bBlockDone || (Stars == 0 && !Unit.bBlock && Unit.IdleTime >= IdleLifetime)) && !Network->IsSeenByPlayer(Car->GetActorLocation()))
		{
			RemoveUnit(Index);
		}
	}

	ChaseTally.EndFrame(DeltaSeconds, static_cast<float>(Player->GetVelocity().Size()) * 0.036f, ChaseTallySettings);
	if (Stars > 0 && Now >= NextTallyTraceTime && ChaseTally.Seconds > 0.0)
	{
		// What the player sees of the pursuit, every 10 s (the per-unit lines above say why a car does not move).
		NextTallyTraceTime = Now + 10.f;
		GTP_TRACE(LogGTPWanted, TEXT("police chase: units mean=%.0f km/h peak=%.0f moving=%.0f %% nearest mean=%.0f m min=%.0f m within %.0f m %.0f %% of %.0f s, player mean=%.0f km/h, up to %d units, %d sent, %d of them replacements (nearest made %.0f m from the player), %d back-off(s), %d unit(s) given up"),
			ChaseTally.MeanKmh(), ChaseTally.UnitPeakKmh, ChaseTally.MovingShare() * 100.f, ChaseTally.MeanNearest(), ChaseTally.NearestMin, ChaseTallySettings.NearMetres,
			ChaseTally.NearShare() * 100.f, static_cast<float>(ChaseTally.Seconds), ChaseTally.PlayerMeanKmh(), ChaseTally.PeakUnits, ChaseTally.Sent, ChaseTally.Replacements,
			ChaseTally.ReplacementNearest, ChaseTally.BackOffs, ChaseTally.GivenUp);
	}

	int32 Chasing = 0;
	int32 Blocks = 0;
	int32 Wrecks = 0;
	for (const FUnit& Unit : Units)
	{
		(Unit.bBlock ? Blocks : (Unit.Recovery.IsGivenUp() ? Wrecks : Chasing)) += 1;
	}
	// The level rose from nothing: the cars it asks for come from the spawn ring (those still around count among them).
	if (Stars > 0 && LastStars == 0)
	{
		ResponseSent = Chasing;
	}
	LastStars = Stars;
	// A given-up car does not hold a chasing car's place: left in the player's sight it would keep the next
	// one from coming. No more than two of them on the map at a time.
	if (Chasing < Desired && Wrecks <= 2 && Now >= NextSpawnTime)
	{
		const FVector Velocity = Player->GetVelocity();
		const bool bMoving = Velocity.Size2D() > 500.f;
		// Past the cars the level asks for, a car sent replaces one that was lost: it is made far from the player
		// and has to drive to him, from in front when he moves and a place is found there (it meets him instead of
		// running after him). Among the first cars, from the second on, every other one comes from in front.
		bFarSpawn = ResponseSent >= Desired;
		const bool bFlankCar = Profile.bFlank && (Chasing % 2 == 1);
		bAheadSpawn = bMoving && (bFarSpawn || bFlankCar);
		AheadDirection = FVector2D(Velocity.X, Velocity.Y).GetSafeNormal();
		// Behind a player who drives, on his own road (FindUpstreamPoint): every car but the flank cars of a level.
		// A replacement is looked for there first, then in front, then anywhere, at a replacement's distances.
		bUpstreamTry = bUpstreamSpawn && bMoving && Cast<AGTPVehicle>(Player) != nullptr && (bFarSpawn || !bFlankCar);
		if (!SpawnUnit(Player->GetActorLocation()) && bAheadSpawn)
		{
			bAheadSpawn = false;
			bUpstreamTry = false;
			SpawnUnit(Player->GetActorLocation());
		}
		bAheadSpawn = false;
		bFarSpawn = false;
		bUpstreamTry = false;
		NextSpawnTime = Now + SpawnInterval;
	}
	// Roadblocks at three stars and more: two cars across the road ahead of a player driving fast.
	if (Profile.RoadblockCars > 0 && Blocks < Profile.RoadblockCars && Cast<AGTPVehicle>(Player) != nullptr && Now >= NextBlockTime
		&& Player->GetVelocity().Size2D() >= RoadblockMinSpeed)
	{
		if (Queries && Now >= NextSiteQueryTime)
		{
			NextSiteQueryTime = Now + 4.f;
			Queries->RequestRoadblockSites(this);
		}
		NextBlockTime = Now + (SpawnRoadblockCar(*Player, Blocks) ? 2.f : 6.f);
	}
}
