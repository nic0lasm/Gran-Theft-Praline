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
#include "AIController.h"
#include "ChaosVehicleMovementComponent.h"
#include "ChaosWheeledVehicleMovementComponent.h"
#include "CollisionQueryParams.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPWanted, Log, All);

namespace GTPPolice
{
	const FName PoliceTag(TEXT("GTPPolice"));
}

namespace
{
	constexpr float WantedCmToM = 0.01f;

	bool WitnessesDisabledByCommandLine()
	{
		int32 Value = 1;
		FParse::Value(FCommandLine::Get(), TEXT("Witnesses="), Value);
		return Value == 0;
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

void UGTPWantedComponent::ReportCrime(EGTPCrimeKind Kind, const FVector& Location, bool bVictimInvolved)
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
	if (Kind != EGTPCrimeKind::ArmedThreat)
	{
		// Every crime makes noise the police units nearby can hear (a shot, a crash, a scream).
		const APlayerController* Owner = Cast<APlayerController>(GetOwner());
		UGTPPoliceSenses::ReportNoise(this, Location, Owner ? Owner->GetPawn() : nullptr, 1.f);
	}
	if (!bRequireWitnesses || WitnessesDisabledByCommandLine())
	{
		ApplyCrime(Kind, Location);
		return;
	}

	// Who saw or heard it: police cars and people close enough, with a clear line to the crime.
	std::vector<FGTPWitness> Witnesses;
	const FVector Eye = Location + FVector(0.f, 0.f, 150.f);
	auto AddWitness = [&](const AActor* Who, bool bPolice)
	{
		if (!IsValid(Who))
		{
			return;
		}
		const FVector Where = Who->GetActorLocation();
		FCollisionQueryParams WitnessParams(SCENE_QUERY_STAT(GTPWitnessSight), false, Who);
		FHitResult Hit;
		const bool bSees = FVector::Dist(Where, Location) < 8000.f
			&& !GetWorld()->LineTraceSingleByChannel(Hit, Where + FVector(0.f, 0.f, 150.f), Eye, ECC_Visibility, WitnessParams);
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
		GTP_TRACE(LogGTPWanted, TEXT("wanted: crime %d not witnessed"), static_cast<int32>(Kind));
		return;
	}
	if (Result.Delay <= 0.f)
	{
		ApplyCrime(Kind, Location);
		return;
	}
	FPendingReport& Pending = PendingReports.AddDefaulted_GetRef();
	Pending.Kind = Kind;
	Pending.Location = Location;
	Pending.DueTime = static_cast<float>(GetWorld()->GetTimeSeconds()) + Result.Delay;
	GTP_TRACE(LogGTPWanted, TEXT("wanted: crime %d seen, the police are called (%.0f s)"), static_cast<int32>(Kind), Result.Delay);
}

void UGTPWantedComponent::TickPendingReports(float Now)
{
	for (int32 Index = PendingReports.Num() - 1; Index >= 0; --Index)
	{
		if (Now >= PendingReports[Index].DueTime)
		{
			const FPendingReport Report = PendingReports[Index];
			PendingReports.RemoveAtSwap(Index);
			ApplyCrime(Report.Kind, Report.Location);
		}
	}
}

void UGTPWantedComponent::ApplyCrime(EGTPCrimeKind Kind, const FVector& Location)
{
	if (BustedBannerLeft > 0.f || !GetWorld() || !IsComponentTickEnabled())
	{
		return;
	}
	if (Kind == EGTPCrimeKind::ShotsFired || Kind == EGTPCrimeKind::ArmedThreat || Kind == EGTPCrimeKind::Assault
		|| Kind == EGTPCrimeKind::Killing || Kind == EGTPCrimeKind::KillPolice || Kind == EGTPCrimeKind::HitPolice)
	{
		LastAggressiveTime = static_cast<float>(GetWorld()->GetTimeSeconds());
	}
	const int32 Before = State.GetStars();
	if (!State.AddCrime(Kind, static_cast<float>(GetWorld()->GetTimeSeconds())))
	{
		return;
	}
	if (Before == 0)
	{
		LastKnownLocation = Location;
	}
	DispatchLeft = DispatchSeconds;
	NotifyIfChanged();
	GTP_TRACE(LogGTPWanted, TEXT("wanted: crime %d, heat %.0f, %d stars"), static_cast<int32>(Kind), State.GetHeat(), State.GetStars());
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
		PC->SetIgnoreMoveInput(false);
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
	LoadedPoliceClass = PoliceVehicleClass.TryLoadClass<AGTPVehicle>();
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
	GTP_TRACE(LogGTPWanted, TEXT("police: on, up to %d units, class %s"), MaxUnits, LoadedPoliceClass ? *LoadedPoliceClass->GetName() : TEXT("traffic cars"));
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
		if (bAheadSpawn)
		{
			// Coming from in front: farther out, within about 35 degrees of the player's direction of travel.
			if (Distance < MinSpawnDistance * 1.6 || Distance > MaxSpawnDistance * 1.4
				|| !GTPPoliceTactics::IsAhead(static_cast<float>(Location.X - PlayerLocation.X), static_cast<float>(Location.Y - PlayerLocation.Y),
					static_cast<float>(AheadDirection.X), static_cast<float>(AheadDirection.Y), 0.8f))
			{
				continue;
			}
		}
		else if (Distance < MinSpawnDistance || Distance > MaxSpawnDistance)
		{
			continue;
		}
		FHitResult Ground;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPPoliceGround), false);
		if (!GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0.f, 0.f, 500.f), Location - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			continue;
		}
		Location.Z = Ground.ImpactPoint.Z + 40.f;
		bool bFree = true;
		for (const FUnit& Unit : Units)
		{
			if (Unit.Car.IsValid() && FVector::Dist(Unit.Car->GetActorLocation(), Location) < 1500.f)
			{
				bFree = false;
				break;
			}
		}
		if (!bFree || Network->IsSeenByPlayer(Location))
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

bool AGTPPoliceDirector::SpawnUnit(const FVector& PlayerLocation, const FTransform* Placement, bool bBlock)
{
	TSubclassOf<AGTPVehicle> Class = LoadedPoliceClass;
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
	if (Placement)
	{
		Transform = *Placement;
	}
	else if (!FindSpawnPoint(PlayerLocation, Lane, Point, Transform))
	{
		return false;
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
	// Chase tuning: harder bends and braking than the calm city driving.
	Unit.bBlock = bBlock;
	Unit.Settings.MaxLateralAccel = 6.f;
	Unit.Settings.ComfortDecel = 7.f;
	Unit.Settings.SafeGap = 3.f;
	Unit.Settings.ThrottleGain = 0.8f;
	// The driver is seated in the frame the car is made, like the traffic's cars (a car that got its driver a
	// frame later stayed in neutral on the PC); the light bar and the registration come on the next frames.
	if (!SeatDriver(Unit))
	{
		Car->Destroy();
		Units.Pop();
		return false;
	}
	Unit.Stage = 2;
	GTP_TRACE(LogGTPWanted, TEXT("police: %s on lane %d at %s"), *Car->GetName(), Lane, *Transform.GetLocation().ToCompactString());
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
	OutPoint = Point;
	OutAlong = Along;
	OutDistance = Distance;
	return true;
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
	if (SpawnUnit(Player.GetActorLocation(), &Placement, true))
	{
		GTP_TRACE(LogGTPWanted, TEXT("police: roadblock car %d at %s, %.0f m ahead"), CarIndex, *Location.ToCompactString(), Distance * 0.01);
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
	GTP_TRACE(LogGTPWanted, TEXT("police: no roadblock placed: lane sites ahead %d / %d / %d (strict, wide, widest), %d from EQS, %d lanes, player %.0f km/h, %d units"),
		Found[0], Found[1], Found[2], Queries ? Queries->GetRoadblockSites().Num() : 0, Lanes.Num(), Player.GetVelocity().Size2D() * 0.036f, Units.Num());
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

void AGTPPoliceDirector::RepositionUnit(FUnit& Unit, const FVector& PlayerLocation)
{
	AGTPVehicle* Car = Unit.Car.Get();
	int32 Lane = 0, Point = 0;
	FTransform Transform;
	if (!Car || !FindSpawnPoint(PlayerLocation, Lane, Point, Transform))
	{
		return;
	}
	// Never teleported into another car (traffic, puppet, parked): the solver does not survive a deep overlap.
	if (const AActor* Blocker = Network->bPoolSpawnCheck ? Network->FindCarAt(Car, Transform) : nullptr)
	{
		GTP_TRACE(LogGTPWanted, TEXT("police: %s move blocked by %s"), *Car->GetName(), *Blocker->GetName());
		return;
	}
	// A move instead of a new car and driver: no spawn hitch.
	Car->SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);
	Car->NotePlaced(TEXT("police move"));
	if (USkeletalMeshComponent* Body = Car->GetMesh())
	{
		Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
	Unit.Follower = FGTPLaneFollower();
	Unit.bOnLanes = false;
	Unit.StoppedTime = 0.f;
	GTP_TRACE(LogGTPWanted, TEXT("police: %s moved to lane %d"), *Car->GetName(), Lane);
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
	const float Obstacle = (Profile.bRam || GetWorld()->GetTimeSeconds() < Unit.RamUntil) ? -1.f : UGTPAIDriverComponent::ProbeAhead(Car, 4000.f, 100.f);
	Unit.LastObstacle = Obstacle;
	const float Speed = Car->GetSpeedKmh() / 3.6f;
	const FGTPDriveCommand Command = GTPDrive::Decide(Path, Path[0].X, Path[0].Y, FMath::DegreesToRadians(static_cast<float>(Car->GetActorRotation().Yaw)),
		Speed, MaxSpeedKmh / 3.6f, Obstacle, Unit.Settings, Closest);
	Unit.LastThrottle = Command.Throttle;
	Unit.LastBrake = Command.Brake;
	Car->SetAIDriveInput(Command.Throttle, Command.Brake, Command.Steering, Command.Brake >= 1.f && Speed < 0.5f);
	Unit.bOnLanes = false;
}

void AGTPPoliceDirector::DriveOnLanes(FUnit& Unit, const FVector& Goal, bool bHasGoal, float MaxSpeedKmh)
{
	AGTPVehicle* Car = Unit.Car.Get();
	const FVector Location = Car->GetActorLocation();
	const float Yaw = static_cast<float>(Car->GetActorRotation().Yaw);
	if (!Unit.bOnLanes || !Unit.Follower.IsActive())
	{
		Unit.Follower = FGTPLaneFollower();
		if (!Unit.Follower.StartNear(Network, Location, Yaw, 1500.f) && !Unit.Follower.StartNear(Network, Location, Yaw, 8000.f))
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
		Unit.Follower.bIgnoreLaneLimit = true;
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
	const float Obstacle = (Profile.bRam || GetWorld()->GetTimeSeconds() < Unit.RamUntil) ? -1.f : UGTPAIDriverComponent::ProbeAhead(Car, 4000.f, 90.f);
	Unit.LastObstacle = Obstacle;
	FGTPDriveCommand Command;
	if (!Unit.Follower.Step(Location, Yaw, Car->GetSpeedKmh(), MaxSpeedKmh, Obstacle, Command))
	{
		Unit.bOnLanes = false;
		Unit.LastThrottle = 0.f;
		Unit.LastBrake = 1.f;
		Car->SetAIDriveInput(0.f, 1.f, 0.f, true);
		return;
	}
	Unit.LastThrottle = Command.Throttle;
	Unit.LastBrake = Command.Brake;
	Car->SetAIDriveInput(Command.Throttle, Command.Brake, Command.Steering, Command.Brake >= 1.f && Car->GetSpeedKmh() < 2.f);
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

	if (Stars == 0)
	{
		// Level lost or arrest: back to ordinary driving on the lanes.
		DriveOnLanes(Unit, FVector::ZeroVector, false, 40.f);
		return;
	}
	const float ChaseKmh = Profile.ChaseKmh > 0.f ? Profile.ChaseKmh : GTPWanted::ChaseSpeedKmh(Stars);
	const float NowSeconds = static_cast<float>(GetWorld()->GetTimeSeconds());
	// Throttle held and the car does not move: not traffic, the vehicle itself (asleep, parked, no gear). Wake it.
	if (Unit.StoppedTime > 1.f && Unit.LastThrottle > 0.5f && Unit.LastObstacle < 0.f)
	{
		// First gear forced every frame while the car does not move with the throttle held.
		if (UChaosVehicleMovementComponent* Move = Car->GetVehicleMovementComponent())
		{
			Move->SetTargetGear(1, true);
		}
	}
	if (Unit.StoppedTime > 3.f && Unit.LastThrottle > 0.8f && Unit.LastObstacle < 0.f && NowSeconds >= Unit.NextWakeTime)
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
	Unit.SlowTime = (Stars >= 2 && Car->GetSpeedKmh() < 15.f && Unit.LastObstacle > 0.f && Unit.LastObstacle < 12.f) ? Unit.SlowTime + DeltaSeconds : 0.f;
	if (Unit.SlowTime > 1.5f && NowSeconds >= Unit.RamUntil)
	{
		Unit.RamUntil = NowSeconds + 3.f;
		Unit.SlowTime = 0.f;
	}
	// Stopped far from the player in a chase: push through whatever is in the way for a few seconds.
	if (Unit.StoppedTime > 4.f && NowSeconds >= Unit.RamUntil && FVector::Dist2D(Location, Player.GetActorLocation()) > 1500.f)
	{
		Unit.RamUntil = NowSeconds + 3.f;
		Unit.StoppedTime = 0.f;
		Unit.bOnLanes = false;
		GTP_TRACE(LogGTPWanted, TEXT("police unit: stopped, pushing through (obstacle %.1f m)"), Unit.LastObstacle);
	}
	const bool bPursuit = Wanted.GetPhase() == EGTPWantedPhase::Pursuit;
	const FVector Target = bPursuit ? Player.GetActorLocation() : Wanted.GetLastKnownLocation();
	const float Distance = static_cast<float>(FVector::Dist2D(Location, Target));
	if (bPursuit && HasLineOfSight(Car, Player, Target))
	{
		DriveStraight(Unit, Target, ChaseKmh);
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
		if (!GTPPoliceTactics::ShouldDeploy(Tactics, Stars, bOnFoot, Distance, Unit.StoppedTime > 4.f, Officers.Num(), MaxOfficers))
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
			if (const AGTPVehicle* Car = Unit.Car.Get())
			{
				GTP_TRACE(LogGTPWanted, TEXT("police unit: dist=%.0f m speed=%.0f km/h lanes=%d stopped=%.0f s stage=%d obstacle=%.1f m throttle=%.2f brake=%.2f brain=%d"),
					FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) * 0.01f, Car->GetSpeedKmh(), Unit.bOnLanes ? 1 : 0, Unit.StoppedTime, Unit.Stage,
					Unit.LastObstacle, Unit.LastThrottle, Unit.LastBrake, static_cast<int32>(Unit.Brain));
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
		// A unit left far from a new chase is moved near the player (out of sight) instead of creating another.
		if (!Unit.bBlock && Stars > 0 && Index < Desired && Now >= Unit.NextRepositionTime
			&& FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) > MaxSpawnDistance * 1.5f
			&& !Network->IsSeenByPlayer(Car->GetActorLocation()))
		{
			Unit.NextRepositionTime = Now + 3.f;
			RepositionUnit(Unit, Player->GetActorLocation());
		}
		DriveUnit(Unit, *Wanted, *Player, DeltaSeconds);
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
		const bool bStuck = Unit.StoppedTime > 12.f && !(Stars > 0 && FVector::Dist2D(Car->GetActorLocation(), Player->GetActorLocation()) < 1500.f);
		bool bBlockDone = false;
		if (Unit.bBlock)
		{
			// A roadblock goes once the player is past it, or the level is gone.
			const FVector Relative = Car->GetActorLocation() - Player->GetActorLocation();
			const bool bPassed = Relative.Size2D() > 2500.f && FVector::DotProduct(Relative, Player->GetVelocity()) < 0.f;
			bBlockDone = bPassed || (Stars == 0 && Unit.IdleTime >= 8.f);
		}
		if ((bTooFar || bStuck || bBlockDone || (Stars == 0 && !Unit.bBlock && Unit.IdleTime >= IdleLifetime)) && !Network->IsSeenByPlayer(Car->GetActorLocation()))
		{
			RemoveUnit(Index);
		}
	}

	int32 Chasing = 0;
	int32 Blocks = 0;
	for (const FUnit& Unit : Units)
	{
		(Unit.bBlock ? Blocks : Chasing) += 1;
	}
	if (Chasing < Desired && Now >= NextSpawnTime)
	{
		// From the second car on, every other one comes from in front of a moving player.
		const FVector Velocity = Player->GetVelocity();
		bAheadSpawn = Profile.bFlank && (Chasing % 2 == 1) && Velocity.Size2D() > 500.f;
		AheadDirection = FVector2D(Velocity.X, Velocity.Y).GetSafeNormal();
		if (!SpawnUnit(Player->GetActorLocation()) && bAheadSpawn)
		{
			bAheadSpawn = false;
			SpawnUnit(Player->GetActorLocation());
		}
		bAheadSpawn = false;
		NextSpawnTime = Now + SpawnInterval;
	}
	// Roadblocks at three stars and more: two cars across the road ahead of a player driving fast.
	if (Profile.RoadblockCars > 0 && Blocks < Profile.RoadblockCars && Cast<AGTPVehicle>(Player) != nullptr && Now >= NextBlockTime)
	{
		if (Queries && Now >= NextSiteQueryTime)
		{
			NextSiteQueryTime = Now + 4.f;
			Queries->RequestRoadblockSites(this);
		}
		NextBlockTime = Now + (SpawnRoadblockCar(*Player, Blocks) ? 2.f : 6.f);
	}
}
