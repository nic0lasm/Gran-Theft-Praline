#include "GTPGtaRun.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GTPCombat.h"
#include "GTPDemoRoute.h"
#include "GTPHealth.h"
#include "GTPLog.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "GTPPedestrians.h"
#include "GTPThreat.h"
#include "GTPTheftDemo.h"
#include "GTPVehicle.h"
#include "GTPWanted.h"
#include "GTPWeapon.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPGtaRun, Log, All);

namespace
{
	// In a namespace of their own: the step functions have locals called Aim or Car.
	namespace EStep
	{
		enum Type : int32 { Carjack, Witness, Chase, Pursuit, Outcome, Parked, WalkArmed, Aim, AimClose, Shoot, Wasted, Count };
	}
	constexpr int32 StepCount = EStep::Count;
	const TCHAR* StepNames[StepCount] = { TEXT("carjack"), TEXT("witness"), TEXT("chase"), TEXT("pursuit"), TEXT("outcome"), TEXT("parked"),
		TEXT("walkarmed"), TEXT("aim"), TEXT("aimclose"), TEXT("shoot"), TEXT("wasted") };
	// The steps of the wanted level, counted on their own line: the gate's rule on the seven others reads as before.
	const bool StepIsPolice[StepCount] = { false, true, false, true, true, false, false, false, false, false, true };
	constexpr int32 PoliceStepCount = 4;
	// Time limits (s) per step, counted from the step's start.
	// Witness: the call takes 4 s (GTPWitnessLogic.h). Chase: 3 stars come at 30 s and a car that flees has its
	// 150 m by then; a chase that fails must leave the other steps their time (170 s of it ate the bench's 330 s
	// on 16/11). Pursuit: its 40 s window. Outcome: a stopped player is arrested in 3 s once a unit is within 7 m;
	// the units have 60 s to come. Wasted: 6 s dead, then the respawn.
	const double StepLimits[StepCount] = { 90.0, 12.0, 90.0, 50.0, 60.0, 200.0, 40.0, 90.0, 90.0, 30.0, 30.0 };
	// The flight measured by the pursuit step after the chase step (s).
	constexpr double PursuitSeconds = 40.0;

	AGTPPoliceDirector* FindDirector(const UWorld* World)
	{
		TActorIterator<AGTPPoliceDirector> It(World);
		return It ? *It : nullptr;
	}

	/** Distance (m) from the player to the nearest police car, -1 when there is none. */
	float NearestPoliceCar(const UGTPWantedComponent& Wanted, const FVector& Player)
	{
		float Nearest = -1.f;
		for (const TWeakObjectPtr<AGTPVehicle>& Car : Wanted.GetPolice())
		{
			if (Car.IsValid())
			{
				const float Metres = static_cast<float>(FVector::Dist(Car->GetActorLocation(), Player)) * 0.01f;
				Nearest = Nearest < 0.f ? Metres : FMath::Min(Nearest, Metres);
			}
		}
		return Nearest;
	}

	APlayerController* FirstController(const UWorld* World)
	{
		return World ? World->GetFirstPlayerController() : nullptr;
	}
}

bool UGTPGtaRun::IsRequested()
{
	return FParse::Param(FCommandLine::Get(), TEXT("GTARun"));
}

void UGTPGtaRun::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!IsRequested())
	{
		return;
	}
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPGtaRun::Tick), 0.5f);
	// The steps think twice a second, but a walk needs its input every frame: one frame of input per half second
	// is a player who stands still (measured: 0 cm/s for 4 s in every aim step).
	WalkHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
	{
		if (!WalkDirection.IsNearlyZero())
		{
			if (APlayerController* PC = FirstController(GetWorld()))
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					// -GTARunWalk=input (default: AddMovementInput), key (the W key through the controller, as a player
					// does), velocity (the walk speed written straight into the movement): to tell where a lost input goes.
					FString Strategy;
					FParse::Value(FCommandLine::Get(), TEXT("GTARunWalk="), Strategy);
					if (Strategy.Equals(TEXT("key"), ESearchCase::IgnoreCase))
					{
						PC->InputKey(FInputKeyParams(EKeys::W, WalkInputs == 0 ? EInputEvent::IE_Pressed : EInputEvent::IE_Repeat, 1.0, false));
					}
					else if (Strategy.Equals(TEXT("velocity"), ESearchCase::IgnoreCase))
					{
						if (ACharacter* Walker = Cast<ACharacter>(Pawn))
						{
							if (UCharacterMovementComponent* Move = Walker->GetCharacterMovement())
							{
								Move->Velocity = FVector(WalkDirection.X, WalkDirection.Y, 0.f).GetSafeNormal() * 400.f;
							}
						}
					}
					else
					{
						WalkPendingBefore += Pawn->GetPendingMovementInputVector().Size() > 0.01f ? 1 : 0; // not consumed since the last add
						Pawn->AddMovementInput(WalkDirection, 1.f);
						WalkRejected += Pawn->GetPendingMovementInputVector().Size() > 0.01f ? 0 : 1; // refused by the pawn
					}
					++WalkInputs;
				}
			}
		}
		return true;
	}), 0.f);
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: on (%d steps)"), StepCount);
}

void UGTPGtaRun::Deinitialize()
{
	if (UGTPWantedComponent* Wanted = BoundWanted.Get())
	{
		Wanted->OnArrested.RemoveAll(this);
	}
	if (UGTPCombatSubsystem* Combat = BoundCombat.Get())
	{
		Combat->OnPlayerDied.RemoveAll(this);
		Combat->OnPlayerRespawned.RemoveAll(this);
	}
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	FTSTicker::GetCoreTicker().RemoveTicker(WalkHandle);
	Super::Deinitialize();
}

void UGTPGtaRun::BeginStep(int32 Index)
{
	Step = Index;
	StepStart = FPlatformTime::Seconds();
	bParkedStarted = false;
	bExitRequested = false;
	ParkedTarget.Reset();
	ParkedCharacter.Reset();
	ShotsFired = 0;
	ShotAttempts = 0;
	NextShot = 0.0;
	bAimExitRequested = false;
	AimFirstReaction = -1.0;
	LastAimMoveTime = -1.0;
	WalkDirection = FVector::ZeroVector;
	WalkArmedPhase = 0;
	WalkArmedPhaseStart = -1.0;
	bWalkArmedExit = false;
	OutcomePhase = 0;
	bWastedExit = false;
	WastedPhase = 0;
	WastedTries = 0;
	if (Step == EStep::Chase)
	{
		ChaseBegan = StepStart;
	}
	else if (Step == EStep::Pursuit)
	{
		// The chase is proven or not; from here the level lives by itself: a pursuit that only lasts because the
		// script keeps forcing 3 stars proves nothing about the police.
		if (UGTPDemoRoute* Demo = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGTPDemoRoute>() : nullptr)
		{
			Demo->FreeChaseLevel();
		}
	}
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun step %d: %s start (t=%.0f s)"), Step + 1, StepNames[Step], FPlatformTime::Seconds() - RunStart);
}

void UGTPGtaRun::EndStep(EResult Result, const FString& Detail)
{
	const bool bOk = Result == EResult::Ok;
	(StepIsPolice[Step] ? PoliceOkCount : OkCount) += bOk ? 1 : 0;
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun step %d: %s %s %s (%.0f s)"), Step + 1, StepNames[Step], bOk ? TEXT("ok") : TEXT("fail"), *Detail,
		FPlatformTime::Seconds() - StepStart);
	UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this);
	if (Step == EStep::Outcome && Wanted)
	{
		// The steps on foot test the theft, the walk and the hold-up, not the police: what is left of the level goes, and
		// nobody arrests the player in the middle of a hold-up (every run so far had the arrest off from here: the chase
		// script allowed it from its 100th second and the run left the car before).
		const int32 Left = Wanted->GetStars();
		if (Left > 0)
		{
			bRunCleared = true;
			Wanted->ClearWanted();
		}
		Wanted->SetArrestEnabled(false);
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: for the steps on foot the level is cleared by the run (%d star(s) were left) and the arrest is off until the end"), Left);
	}
	if (Step + 1 < StepCount)
	{
		BeginStep(Step + 1);
		return;
	}
	bDone = true;
	if (Wanted)
	{
		Wanted->SetArrestEnabled(true);
	}
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun police: %d/%d ok (witness, pursuit, outcome, wasted)"), PoliceOkCount, PoliceStepCount);
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun done: %d/%d ok (%.0f s)"), OkCount, StepCount - PoliceStepCount, FPlatformTime::Seconds() - RunStart);
}

void UGTPGtaRun::TrackWanted()
{
	const double Now = FPlatformTime::Seconds();
	UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this);
	if (Wanted && BoundWanted.Get() != Wanted)
	{
		BoundWanted = Wanted;
		Wanted->OnArrested.AddWeakLambda(this, [this]()
		{
			// Who made the arrest, as the player sees it at that instant (the rule is a unit within 7 m for 3 s): at the
			// gate of candidate 48 the step passed on an arrest it could not describe, made 4 s after the script had put
			// the car 8 m from a police car.
			ArrestedAt = FPlatformTime::Seconds();
			const APlayerController* PC = FirstController(GetWorld());
			const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			ArrestedWhere = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
			ArrestKmh = Pawn ? static_cast<float>(Pawn->GetVelocity().Size()) * 0.036f : 0.f;
			ArrestChasing = ArrestBlock = ArrestOfficer = -1.f;
			if (const AGTPPoliceDirector* Director = FindDirector(GetWorld()))
			{
				Director->NearestUnits(ArrestedWhere, ArrestChasing, ArrestBlock, ArrestOfficer);
			}
			const UGTPDemoRoute* Demo = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGTPDemoRoute>() : nullptr;
			ArrestSinceRelocation = Demo ? Demo->GetSecondsSinceRelocation() : 1.0e9;
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: arrested at %.0f km/h; nearest chasing car %.0f m, roadblock car %.0f m, officer on foot %.0f m (-1 = none); the script last moved the car %.0f s before"),
				ArrestKmh, ArrestChasing, ArrestBlock, ArrestOfficer, ArrestSinceRelocation < 1.0e8 ? ArrestSinceRelocation : -1.0);
		});
	}
	UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this);
	if (Combat && BoundCombat.Get() != Combat)
	{
		BoundCombat = Combat;
		Combat->OnPlayerDied.AddWeakLambda(this, [this](const FVector& Where)
		{
			DiedAt = FPlatformTime::Seconds();
			DiedWhere = Where;
		});
		Combat->OnPlayerRespawned.AddWeakLambda(this, [this](const FVector&, FName Hospital)
		{
			RespawnedAt = FPlatformTime::Seconds();
			RespawnHospital = Hospital;
		});
	}
	// The level gone with no arrest, no respawn and no clearing by the run since the last look: the police lost the player.
	const int32 Stars = Wanted ? Wanted->GetStars() : 0;
	if (LastStars > 0 && Stars == 0)
	{
		const bool bArrest = ArrestedAt >= LastTrackTime;
		const bool bRespawn = RespawnedAt >= LastTrackTime;
		if (!bArrest && !bRespawn && !bRunCleared)
		{
			LostAt = Now;
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: the level is lost (t=%.0f s)"), Now - RunStart);
		}
		bRunCleared = false;
	}
	LastStars = Stars;
	LastTrackTime = Now;
}

bool UGTPGtaRun::Tick(float /*DeltaTime*/)
{
	if (bDone)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const APlayerController* PC = FirstController(World);
	if (!World || !PC || !PC->GetPawn())
	{
		return true;
	}
	TrackWanted();
	if (Step < 0)
	{
		RunStart = FPlatformTime::Seconds();
		BeginStep(0);
		return true;
	}
	FString Detail;
	EResult Result = EResult::Pending;
	switch (Step)
	{
	case EStep::Carjack: Result = CheckCarjack(Detail); break;
	case EStep::Witness: Result = CheckWitness(Detail); break;
	case EStep::Chase: Result = CheckChase(Detail); break;
	case EStep::Pursuit: Result = CheckPursuit(Detail); break;
	case EStep::Outcome: Result = CheckOutcome(Detail); break;
	case EStep::Parked: Result = CheckParked(Detail); break;
	case EStep::WalkArmed: Result = CheckWalkArmed(Detail); break;
	case EStep::Aim: Result = CheckAim(Detail, false); break;
	case EStep::AimClose: Result = CheckAim(Detail, true); break;
	case EStep::Shoot: Result = CheckShoot(Detail); break;
	default: Result = CheckWasted(Detail); break;
	}
	if (Result == EResult::Pending && FPlatformTime::Seconds() - StepStart > StepLimits[Step])
	{
		Result = EResult::Fail;
		Detail = FString::Printf(TEXT("timeout after %.0f s (%s)"), StepLimits[Step], *Detail);
	}
	if (Result != EResult::Pending)
	{
		EndStep(Result, Detail);
	}
	return true;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckCarjack(FString& Detail)
{
	const APlayerController* PC = FirstController(GetWorld());
	const AGTPVehicle* Car = PC ? Cast<AGTPVehicle>(PC->GetPawn()) : nullptr;
	Detail = TEXT("not in a car yet");
	if (Car)
	{
		Detail = FString::Printf(TEXT("in %s"), *Car->GetName());
		return EResult::Ok;
	}
	return EResult::Pending;
}

void UGTPGtaRun::TrackFlight()
{
	// A chase is a flight: the metres the player's car really drove (a jump of more than 20 m between two
	// frames is the demo putting the car on another lane, not driving) and its best speed. At door 41 the
	// step passed on stars and units with the car standing still in a dead end from its 8th second.
	// Counted from the wheel being taken, as before the witness step came in between.
	const APlayerController* PC = FirstController(GetWorld());
	if (const AGTPVehicle* Car = PC ? Cast<AGTPVehicle>(PC->GetPawn()) : nullptr)
	{
		const FVector At = Car->GetActorLocation();
		if (ChaseCar.Get() == Car)
		{
			const double Moved = FVector::Dist2D(At, ChaseLastAt);
			if (Moved < 2000.0)
			{
				ChaseDrivenCm += Moved;
			}
		}
		ChaseCar = Car;
		ChaseLastAt = At;
		ChasePeakKmh = FMath::Max(ChasePeakKmh, FMath::Abs(Car->GetSpeedKmh()));
	}
}

UGTPGtaRun::EResult UGTPGtaRun::CheckChase(FString& Detail)
{
	const UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this);
	if (!Wanted)
	{
		Detail = TEXT("no wanted component");
		return EResult::Fail;
	}
	MaxStars = FMath::Max(MaxStars, Wanted->GetStars());
	MaxUnits = FMath::Max(MaxUnits, Wanted->GetPolice().Num());
	TrackFlight();
	Detail = FString::Printf(TEXT("best stars=%d units=%d driven=%.0f m peak=%.0f km/h"), MaxStars, MaxUnits, ChaseDrivenCm / 100.0, ChasePeakKmh);
	if (const AGTPPoliceDirector* Director = FindDirector(GetWorld()); Director && Director->GetChaseTally().Seconds > 0.0)
	{
		// Read only here: the pursuit step judges them after 40 more seconds of flight.
		Detail += FString::Printf(TEXT(", police so far mean=%.0f km/h nearest mean=%.0f m"), Director->GetChaseTally().MeanKmh(), Director->GetChaseTally().MeanNearest());
	}
	return (MaxStars >= 3 && MaxUnits >= 3 && ChaseDrivenCm >= ChaseDrivenWantedCm) ? EResult::Ok : EResult::Pending;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckWitness(FString& Detail)
{
	// Stealing a car in front of people gives a star because somebody phones: the driver pulled out, or a passer-by
	// who saw it. The component says how each crime reached the police; a star forced by the chase script (2 stars,
	// 8 s after the wheel is taken) is not a call and does not count here.
	TrackFlight();
	const UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this);
	if (!Wanted)
	{
		Detail = TEXT("no wanted component");
		return EResult::Fail;
	}
	if (!Wanted->AreWitnessesRequired())
	{
		Detail = TEXT("witnesses are off (-Witnesses=0 or bRequireWitnesses): every crime counts at once, nothing to prove");
		return EResult::Fail;
	}
	const UGTPWantedComponent::FCrimeReports& Reports = Wanted->GetCrimeReports();
	Detail = FString::Printf(TEXT("calls=%d (last by %d passer(s)-by%s), calls counted=%d, seen by police=%d, not witnessed=%d, stars=%d"), Reports.Called,
		Reports.LastCallPersons, Reports.bLastCallVictim ? TEXT(" and the victim") : TEXT(""), Reports.CalledApplied, Reports.ByPolice, Reports.Unwitnessed,
		Wanted->GetStars());
	if (Reports.CalledApplied > 0 && Reports.FirstCallStars >= 1)
	{
		Detail = FString::Printf(TEXT("a call reached the police: %d star(s) then, with %d police car(s) on the road; "), Reports.FirstCallStars, Reports.FirstCallUnits) + Detail;
		return EResult::Ok;
	}
	return EResult::Pending;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckPursuit(FString& Detail)
{
	// Do the police follow? The director counts, every frame since the level rose, the speed of each chasing car
	// and how far the nearest one is (roadblock cars apart). The flight goes on for 40 s here with the level left
	// to itself, then the numbers are judged (GTPChase::Judge): at candidate 43 the units averaged 1 km/h and the
	// chase step, which looks at the player's car and counts units, passed.
	const UWorld* World = GetWorld();
	const AGTPPoliceDirector* Director = FindDirector(World);
	const UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this);
	if (!Director || !Wanted)
	{
		Detail = TEXT("no police director (-Police=0?)");
		return EResult::Fail;
	}
	const FGTPChaseTally& Tally = Director->GetChaseTally();
	const FGTPChaseTallySettings& Settings = Director->GetChaseTallySettings();
	const UGTPDemoRoute* Demo = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGTPDemoRoute>() : nullptr;
	const GTPChase::EVerdict Verdict = GTPChase::Judge(Tally, Settings);
	Detail = FString::Printf(TEXT("%s: units mean=%.0f km/h (wanted %.0f, player %.0f) peak=%.0f, moving %.0f %% of their time, nearest mean=%.0f m min=%.0f m, "
		"within %.0f m %.0f %% of %.0f s (wanted %.0f %%), up to %d units, %d put back near the player (%d moves not held), %d back-off(s), %d unit(s) given up, the player's car put back on a lane %d time(s), stars=%d"),
		ANSI_TO_TCHAR(GTPChase::VerdictName(Verdict)), Tally.MeanKmh(), GTPChase::WantedMeanKmh(Tally, Settings), Tally.PlayerMeanKmh(), Tally.UnitPeakKmh,
		Tally.MovingShare() * 100.f, Tally.MeanNearest(), Tally.NearestMin, Settings.NearMetres, Tally.NearShare() * 100.f, static_cast<float>(Tally.Seconds),
		Settings.MinNearShare * 100.f, Tally.PeakUnits, Tally.Moves, Tally.MovesLost, Tally.BackOffs, Tally.GivenUp, Demo ? Demo->GetChaseRelocations() : 0, Wanted->GetStars());
	// Over when the window is, or when nobody is wanted any more (lost, arrested, dead: the outcome step says which).
	if (FPlatformTime::Seconds() - StepStart < PursuitSeconds && Wanted->GetStars() > 0)
	{
		return EResult::Pending;
	}
	return Verdict == GTPChase::EVerdict::Follows ? EResult::Ok : EResult::Fail;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckOutcome(FString& Detail)
{
	// How does it end? A chase ends three ways for the player: arrested, killed, or the police lose him. Whatever
	// happened since the chase began counts (a car held up in the pursuit can be arrested there). If he is still
	// wanted, the player gives up: the car stops, and the police have to come to it and end it.
	UWorld* World = GetWorld();
	APlayerController* PC = FirstController(World);
	UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this);
	if (!PC || !PC->GetPawn() || !Wanted)
	{
		Detail = TEXT("no wanted component");
		return Wanted ? EResult::Pending : EResult::Fail;
	}
	const double Now = FPlatformTime::Seconds();
	AGTPPoliceDirector* Director = FindDirector(World);
	if (OutcomePhase == 0)
	{
		OutcomePhase = 1;
		OutcomeStars = Wanted->GetStars();
		GaveUpAt = Now;
		GaveUpNearest = NearestPoliceCar(*Wanted, PC->GetPawn()->GetActorLocation());
		if (OutcomeStars > 0)
		{
			if (Director)
			{
				// From here the numbers are those of the police coming to a stopped player.
				Director->ResetChaseTally();
			}
			Wanted->SetArrestEnabled(true);
			UGTPDemoRoute* Demo = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGTPDemoRoute>() : nullptr;
			const bool bDriven = Demo && Demo->IsChaseDriving();
			if (bDriven)
			{
				Demo->GiveUpChase();
			}
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: the player gives up at %d star(s), %s, nearest police car %.0f m"), OutcomeStars,
				bDriven ? TEXT("the car stops") : (Cast<AGTPVehicle>(PC->GetPawn()) ? TEXT("in a car the script does not drive") : TEXT("on foot")), GaveUpNearest);
		}
	}
	int32 Cars = 0, Blocks = 0, Officers = 0;
	if (Director)
	{
		Director->CountUnits(Cars, Blocks, Officers);
	}
	// The director's numbers: since the player gave up, or since the level rose when the chase had ended before.
	const FString Police = Director ? FString::Printf(TEXT("police cars mean=%.0f km/h, nearest min=%.0f m, %d car(s) %d roadblock car(s) %d officer(s) on foot"),
		Director->GetChaseTally().MeanKmh(), Director->GetChaseTally().NearestMin, Cars, Blocks, Officers) : FString(TEXT("no police director"));
	if (ArrestedAt >= ChaseBegan && ChaseBegan >= 0.0)
	{
		// Arrested. What the player sees next: out of the car, held for the banner, then free on foot (at the station
		// when the map has one), the level gone, the ammunition taken.
		const double Since = Now - ArrestedAt;
		if (Since < Wanted->BustedSeconds + 1.5)
		{
			Detail = TEXT("arrested, waiting for the release");
			return EResult::Pending;
		}
		const ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
		if (!Character && Since < Wanted->BustedSeconds + 6.0)
		{
			Detail = TEXT("arrested, still at the wheel");
			return EResult::Pending;
		}
		const UGTPWeaponComponent* Weapon = Character ? Character->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
		const bool bCanWalk = !PC->IsMoveInputIgnored();
		// Who the player saw arrest him: the nearest of the three kinds at that instant. The rule wants a unit within 7 m;
		// nobody within 10 m is an arrest the player cannot understand. And an arrest in the seconds after the script moved
		// the car is the script's doing (the script itself keeps the arrest off then; checked all the same).
		const TCHAR* By = TEXT("nobody");
		float ByMetres = -1.f;
		auto Nearer = [&By, &ByMetres](const TCHAR* Kind, float Metres)
		{
			if (Metres >= 0.f && (ByMetres < 0.f || Metres < ByMetres))
			{
				By = Kind;
				ByMetres = Metres;
			}
		};
		Nearer(TEXT("a chasing car"), ArrestChasing);
		Nearer(TEXT("a roadblock car"), ArrestBlock);
		Nearer(TEXT("an officer on foot"), ArrestOfficer);
		const bool bSeenUnit = ByMetres >= 0.f && ByMetres <= 10.f;
		const bool bOwnDoing = ArrestSinceRelocation >= UGTPDemoRoute::RelocationGraceSeconds;
		Detail = FString::Printf(TEXT("arrested %.0f s %s at %.0f km/h by %s %.0f m away%s%s (chasing car %.0f m, roadblock car %.0f m, officer %.0f m), released %s %.0f m from the arrest, can walk=%d, stars=%d, rounds left=%d; %s"),
			FMath::Abs(ArrestedAt - GaveUpAt), ArrestedAt >= GaveUpAt ? TEXT("after giving up") : TEXT("before the step, during the flight"), ArrestKmh, By, ByMetres,
			bSeenUnit ? TEXT("") : TEXT(": NO POLICE WITHIN 10 m"), bOwnDoing ? TEXT("") : TEXT(": THE SCRIPT HAD JUST MOVED THE CAR THERE"), ArrestChasing, ArrestBlock, ArrestOfficer,
			Character ? TEXT("on foot") : TEXT("STILL AT THE WHEEL"), FVector::Dist2D(PC->GetPawn()->GetActorLocation(), ArrestedWhere) / 100.0, bCanWalk ? 1 : 0,
			Wanted->GetStars(), Weapon ? Weapon->GetMagazine() + Weapon->GetReserve() : -1, *Police);
		return (Character && bCanWalk && Wanted->GetStars() == 0 && bSeenUnit && bOwnDoing) ? EResult::Ok : EResult::Fail;
	}
	if (DiedAt >= ChaseBegan && ChaseBegan >= 0.0)
	{
		// Killed (the police shoot from three stars). Then the respawn has to bring the player back.
		if (RespawnedAt < DiedAt)
		{
			Detail = FString::Printf(TEXT("killed %.0f s ago, waiting for the respawn"), Now - DiedAt);
			return Now - DiedAt > 20.0 ? EResult::Fail : EResult::Pending;
		}
		if (Now - RespawnedAt < 1.0)
		{
			return EResult::Pending;
		}
		ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
		const UGTPHealthComponent* Health = Character ? UGTPHealthComponent::Find(Character) : nullptr;
		const bool bAlive = Health && !Health->IsDead();
		Detail = FString::Printf(TEXT("killed %.0f s %s, back %.0f s later at %s, alive=%d, stars=%d; %s"), FMath::Abs(DiedAt - GaveUpAt),
			DiedAt >= GaveUpAt ? TEXT("after giving up") : TEXT("before the step, during the flight"), RespawnedAt - DiedAt,
			RespawnHospital.IsNone() ? TEXT("the player start (no hospital point on the map)") : *RespawnHospital.ToString(), bAlive ? 1 : 0, Wanted->GetStars(), *Police);
		return (bAlive && Wanted->GetStars() == 0) ? EResult::Ok : EResult::Fail;
	}
	if (Wanted->GetStars() == 0)
	{
		if (LostAt >= ChaseBegan && ChaseBegan >= 0.0)
		{
			Detail = FString::Printf(TEXT("the police lost the player %.0f s %s; %s"), FMath::Abs(LostAt - GaveUpAt), LostAt >= GaveUpAt ? TEXT("after he stopped") : TEXT("before the step, during the flight"), *Police);
			return EResult::Ok;
		}
		Detail = TEXT("no star left and nothing ended the chase (the player was never wanted, or the level was cleared by something else)");
		return EResult::Fail;
	}
	const AGTPVehicle* Car = Cast<AGTPVehicle>(PC->GetPawn());
	Detail = FString::Printf(TEXT("still wanted %.0f s after giving up: stars=%d phase=%d seen=%d arrest=%.0f %%, player %s at %.0f km/h, nearest police car %.0f m; %s"), Now - GaveUpAt,
		Wanted->GetStars(), Wanted->GetPhaseIndex(), Wanted->IsSeenNow() ? 1 : 0, Wanted->GetArrestProgress() * 100.f, Car ? TEXT("in the car") : TEXT("on foot"),
		Car ? FMath::Abs(Car->GetSpeedKmh()) : 0.f, NearestPoliceCar(*Wanted, PC->GetPawn()->GetActorLocation()), *Police);
	return EResult::Pending;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckWasted(FString& Detail)
{
	// Death and respawn: the player takes a lethal hit through the health component (the path of a fall or of a car),
	// dies, and must be back on his feet at the hospital with the level gone. Who can kill him in the game is another
	// matter (the outcome step says when the police did); this is what follows a death.
	UWorld* World = GetWorld();
	APlayerController* PC = FirstController(World);
	UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this);
	if (!PC || !PC->GetPawn())
	{
		return EResult::Pending;
	}
	if (!UGTPCombatSubsystem::Get(this))
	{
		Detail = TEXT("combat is off (-Combat=0): no health, no death");
		return EResult::Fail;
	}
	const double Now = FPlatformTime::Seconds();
	if (WastedPhase == 0)
	{
		if (AGTPVehicle* Car = Cast<AGTPVehicle>(PC->GetPawn()))
		{
			if (!bWastedExit)
			{
				bWastedExit = true;
				Car->RequestExit();
			}
			Detail = TEXT("getting out");
			return EResult::Pending;
		}
		ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
		UGTPHealthComponent* Health = Character ? UGTPHealthComponent::Find(Character) : nullptr;
		if (!Health)
		{
			Detail = TEXT("the player has no health component");
			return EResult::Fail;
		}
		if (Wanted)
		{
			// One star at least, so that "the police forget a dead player" is something to see.
			Wanted->EnsureMinStars(1);
			WastedStars = Wanted->GetStars();
		}
		WastedFrom = Character->GetActorLocation();
		WastedHitAt = Now;
		const FGTPDamageOutcome Hit = Health->ApplyDamage(100000.f, EGTPDamageCause::Other, nullptr, true);
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: lethal hit on the player (health lost %.0f, killed=%d, ignored=%d)"), Hit.HealthLost, Hit.bKilled ? 1 : 0, Hit.bIgnored ? 1 : 0);
		if (Hit.bIgnored)
		{
			// Dead already or in the grace after a respawn: try again.
			Detail = TEXT("the lethal hit was ignored (grace after a respawn, or dead already)");
			return ++WastedTries > 10 ? EResult::Fail : EResult::Pending;
		}
		WastedPhase = 1;
		return EResult::Pending;
	}
	if (DiedAt < WastedHitAt)
	{
		Detail = TEXT("a lethal hit and no death");
		return Now - WastedHitAt > 3.0 ? EResult::Fail : EResult::Pending;
	}
	if (RespawnedAt < DiedAt)
	{
		Detail = FString::Printf(TEXT("dead for %.0f s, no respawn"), Now - DiedAt);
		return Now - DiedAt > 20.0 ? EResult::Fail : EResult::Pending;
	}
	if (Now - RespawnedAt < 1.0)
	{
		return EResult::Pending;
	}
	ACharacter* Fresh = Cast<ACharacter>(PC->GetPawn());
	const UGTPHealthComponent* Health = Fresh ? UGTPHealthComponent::Find(Fresh) : nullptr;
	const bool bAlive = Health && !Health->IsDead() && Health->GetHealth01() > 0.9f;
	const bool bCanWalk = !PC->IsMoveInputIgnored();
	const int32 Stars = Wanted ? Wanted->GetStars() : 0;
	Detail = FString::Printf(TEXT("dead, back %.1f s later at %s, %.0f m from the body, health %.0f %%, can walk=%d, stars %d -> %d"), RespawnedAt - DiedAt,
		RespawnHospital.IsNone() ? TEXT("the player start (no hospital point on the map)") : *RespawnHospital.ToString(),
		Fresh ? FVector::Dist2D(Fresh->GetActorLocation(), WastedFrom) / 100.0 : -1.0, Health ? Health->GetHealth01() * 100.f : 0.f, bCanWalk ? 1 : 0, WastedStars, Stars);
	return (bAlive && bCanWalk && Stars == 0) ? EResult::Ok : EResult::Fail;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckParked(FString& Detail)
{
	UWorld* World = GetWorld();
	const APlayerController* PC = FirstController(World);
	if (!bParkedStarted)
	{
		ACharacter* Character = PC ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
		if (!Character)
		{
			// Still at the wheel after the chase: get out now rather than wait for the step to time out.
			if (AGTPVehicle* Driven = PC ? Cast<AGTPVehicle>(PC->GetPawn()) : nullptr; Driven && !bParkedExitRequested)
			{
				bParkedExitRequested = true;
				Driven->RequestExit();
				GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: getting out of %s for the parked-car theft"), *Driven->GetName());
			}
			Detail = TEXT("player still driving");
			return EResult::Pending;
		}
		// The theft library of the traffic thread: a locked car with an alarm at the kerb, the player at its door, the enter key.
		// Up to three cars: one with both doors blocked (wall, post, another car) stays parked and
		// the next one takes another kerb spot.
		// A fourth try looks 300 m around: after an arrest or a death the player stands where the police or the hospital
		// left him, which can be a street with no kerb to park at (the player start is in a pedestrian street).
		AGTPVehicle* Car = nullptr;
		for (int32 Attempt = 0; Attempt < 4 && !Car; ++Attempt)
		{
			AGTPVehicle* Candidate = UGTPTheftDemoLibrary::PlaceAlarmCar(World, Character->GetActorLocation(), Attempt < 3 ? 3000.f : 30000.f);
			if (!Candidate)
			{
				if (Attempt < 3)
				{
					Attempt = 2; // nothing within 30 m: straight to the wide search
					continue;
				}
				break;
			}
			if (UGTPTheftDemoLibrary::PlaceAtDriverDoor(Character, Candidate))
			{
				Car = Candidate;
			}
		}
		if (!Car)
		{
			Detail = TEXT("no alarm car with a free door (see the theft demo lines)");
			return EResult::Fail;
		}
		if (!UGTPTheftDemoLibrary::PressEnter(World))
		{
			Detail = FString::Printf(TEXT("could not start the entry into %s"), *Car->GetName());
			return EResult::Fail;
		}
		ParkedTarget = Car;
		ParkedCharacter = Character;
		bParkedStarted = true;
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: breaking into %s (alarm=%d)"), *Car->GetName(), Car->bAlarm ? 1 : 0);
		Detail = TEXT("entering");
		return EResult::Pending;
	}
	const AGTPVehicle* Target = ParkedTarget.Get();
	if (Target && UGTPTheftDemoLibrary::IsDriving(ParkedCharacter.Get(), Target))
	{
		Detail = FString::Printf(TEXT("in %s alarm=%d"), *Target->GetName(), Target->bAlarm ? 1 : 0);
		return EResult::Ok;
	}
	Detail = TEXT("entering");
	return EResult::Pending;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckWalkArmed(FString& Detail)
{
	// A player walks with the gun out, and walks while aiming (in a GTA one does both): 4 s each, at least 3 m.
	// The failure says what holds the player; nothing here works around it.
	WalkDirection = FVector::ZeroVector;
	UWorld* World = GetWorld();
	APlayerController* PC = FirstController(World);
	if (!PC || !PC->GetPawn())
	{
		return EResult::Pending;
	}
	if (AGTPVehicle* Car = Cast<AGTPVehicle>(PC->GetPawn()))
	{
		if (!bWalkArmedExit)
		{
			bWalkArmedExit = true;
			Car->RequestExit();
		}
		Detail = TEXT("getting out");
		return EResult::Pending;
	}
	ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
	UGTPWeaponComponent* Weapon = Character ? Character->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
	if (!Weapon)
	{
		Detail = TEXT("the player has no weapon component");
		return EResult::Fail;
	}
	if (Weapon->GetMagazine() + Weapon->GetReserve() <= 0)
	{
		Weapon->AddAmmo(36);
	}
	const double Now = FPlatformTime::Seconds();
	if (WalkArmedPhaseStart < 0.0)
	{
		WalkArmedPhaseStart = Now;
		WalkArmedFrom = Character->GetActorLocation();
		if (WalkArmedPhase == 0)
		{
			// Where a tester would walk: the direction with the longest clear way, of eight (the step used to walk
			// where the camera happened to look, 4 s then 4 s more the same way: 40 m in a street ends in a wall or
			// on a car, and the failure blamed the weapon).
			double BestClear = -1.0;
			for (int32 Dir = 0; Dir < 8; ++Dir)
			{
				const double Yaw = PC->GetControlRotation().Yaw + Dir * 45.0;
				const FVector Way = FRotator(0.0, Yaw, 0.0).Vector();
				double Clear = 2500.0;
				for (const double Height : {-40.0, 40.0})
				{
					FHitResult Hit;
					FCollisionQueryParams WayParams(SCENE_QUERY_STAT(GTPGtaRunWay), false, Character);
					const FVector From = WalkArmedFrom + FVector(0.0, 0.0, Height);
					if (World->LineTraceSingleByChannel(Hit, From, From + Way * 2500.0, ECC_Visibility, WayParams))
					{
						Clear = FMath::Min(Clear, static_cast<double>(Hit.Distance));
					}
				}
				if (Clear > BestClear)
				{
					BestClear = Clear;
					WalkArmedYaw = Yaw;
				}
			}
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: walking with the gun towards yaw %.0f, clear for %.0f cm"), WalkArmedYaw, BestClear);
		}
		else
		{
			// Aiming: back the way he came, which he has just walked.
			WalkArmedYaw += 180.0;
		}
		// The tester turns the camera to where he walks, once.
		PC->SetControlRotation(FRotator(0.0, WalkArmedYaw, 0.0));
		Weapon->SetDrawn(true);
		Weapon->SetAiming(WalkArmedPhase == 1);
	}
	WalkDirection = FRotator(0.f, WalkArmedYaw, 0.f).Vector();
	const double Moved = FVector::Dist2D(Character->GetActorLocation(), WalkArmedFrom);
	const TCHAR* Phase = WalkArmedPhase == 0 ? TEXT("gun drawn") : TEXT("aiming");
	if (Now - WalkArmedPhaseStart >= 4.0)
	{
		if (Moved < 300.0)
		{
			const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			Detail = FString::Printf(TEXT("the player does not walk with the weapon (%s): %.0f cm in 4 s, drawn %d, aiming %d, velocity %.0f, acceleration %.0f, movement mode %d, ignore move input = %d"),
				Phase, Moved, Weapon->IsDrawn() ? 1 : 0, Weapon->IsAiming() ? 1 : 0, Movement ? Movement->Velocity.Size() : -1.f,
				Movement ? Movement->GetCurrentAcceleration().Size() : -1.f, Movement ? static_cast<int32>(Movement->MovementMode.GetValue()) : -1, PC->IsMoveInputIgnored() ? 1 : 0);
			// What he stands on (falling with the input taken: he lost his floor, the question is to what).
			if (Movement)
			{
				FHitResult Ahead;
				FCollisionQueryParams AheadParams(SCENE_QUERY_STAT(GTPGtaRunAhead), false, Character);
				const bool bAhead = GetWorld()->SweepSingleByChannel(Ahead, Character->GetActorLocation(), Character->GetActorLocation() + WalkDirection.GetSafeNormal() * 200.0,
					FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(30.f), AheadParams);
				Detail += FString::Printf(TEXT(" [ahead within 2 m: %s]"), bAhead && Ahead.GetActor() ? *FString::Printf(TEXT("%s at %.0f cm"), *Ahead.GetActor()->GetName(), Ahead.Distance) : TEXT("clear"));
				const FHitResult& Floor = Movement->CurrentFloor.HitResult;
				const UPrimitiveComponent* Base = Character->GetMovementBase();
				FHitResult Below;
				FCollisionQueryParams BelowParams(SCENE_QUERY_STAT(GTPGtaRunFloor), false, Character);
				const FVector Feet = Character->GetActorLocation() - FVector(0.f, 0.f, Character->GetSimpleCollisionHalfHeight());
				const bool bBelow = GetWorld()->LineTraceSingleByChannel(Below, Feet + FVector(0.f, 0.f, 10.f), Feet - FVector(0.f, 0.f, 300.f), ECC_Visibility, BelowParams);
				Detail += FString::Printf(TEXT(" [floor %s (walkable %d, %.0f cm), movement base %s, under the feet %s at %.0f cm, at %.0f %.0f %.0f]"),
					Floor.GetActor() ? *Floor.GetActor()->GetName() : TEXT("none"), Movement->CurrentFloor.IsWalkableFloor() ? 1 : 0, Movement->CurrentFloor.FloorDist,
					Base && Base->GetOwner() ? *Base->GetOwner()->GetName() : TEXT("none"),
					bBelow && Below.GetActor() ? *Below.GetActor()->GetName() : TEXT("nothing within 3 m"), bBelow ? Below.Distance - 10.f : -1.f,
					Feet.X, Feet.Y, Feet.Z);
			}
			return EResult::Fail;
		}
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: walked %.0f cm in 4 s (%s)"), Moved, Phase);
		if (WalkArmedPhase == 0)
		{
			WalkArmedPhase = 1;
			WalkArmedPhaseStart = -1.0;
			return EResult::Pending;
		}
		Weapon->SetAiming(false);
		Weapon->SetDrawn(false);
		WalkDirection = FVector::ZeroVector;
		Detail = FString::Printf(TEXT("walked with the gun out and aiming, 3 m or more in 4 s each"));
		return EResult::Ok;
	}
	Detail = FString::Printf(TEXT("%s: %.0f cm so far"), Phase, Moved);
	return EResult::Pending;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckAim(FString& Detail, bool bClose)
{
	WalkDirection = FVector::ZeroVector; // set again below while walking up to somebody
	// The "does not walk" clock runs only while this step asks for a walk, tick after tick. It used to keep its value
	// through the waits (nobody calm in sight, or standing in range): the first walking tick after a wait of 4 s failed
	// at once, with a player who had been asked nothing (gate of candidate 40: pending and last input both 0, which a
	// walk asked every frame cannot give).
	const double WalkAskedSince = LastAimMoveTime;
	LastAimMoveTime = -1.0;
	// Hold-up: draw the weapon and aim at the nearest pedestrian; somebody must raise his hands or run.
	UWorld* World = GetWorld();
	APlayerController* PC = FirstController(World);
	if (!PC || !PC->GetPawn())
	{
		return EResult::Pending;
	}
	if (AGTPVehicle* Car = Cast<AGTPVehicle>(PC->GetPawn()))
	{
		if (!bAimExitRequested)
		{
			bAimExitRequested = true;
			Car->RequestExit();
		}
		Detail = TEXT("getting out");
		return EResult::Pending;
	}
	ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
	UGTPWeaponComponent* Weapon = Character ? Character->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
	UGTPThreatSubsystem* Threat = World ? World->GetSubsystem<UGTPThreatSubsystem>() : nullptr;
	if (!Weapon || !Threat)
	{
		Detail = TEXT("no weapon or no threat subsystem (-ThreatResponse=0?)");
		return EResult::Fail;
	}
	// Busted in the chase: the police took the ammunition. Aiming needs a loaded gun, so it comes back here, not at the shot.
	if (Weapon->GetMagazine() + Weapon->GetReserve() <= 0)
	{
		const int32 Given = Weapon->AddAmmo(36);
		Weapon->SetDrawn(true);
		Weapon->Reload();
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: player re-armed after the arrest (+%d rounds)"), Given);
	}
	// The person to hold up: nearest one not already running or knocked down (the alarm of the theft sends everybody
	// running; they calm down in a few seconds), with a clear line of fire from the eyes (a parked car hides people).
	AActor* Victim = nullptr;
	double Best = 4000.0;
	int32 Running = 0, Hidden = 0;
	float RunningFear = 0.f;
	const FVector EyeNow = Character->GetPawnViewLocation();
	for (TActorIterator<AGTPPedestrian> It(World); It; ++It)
	{
		const FVector Where = It->GetActorLocation();
		const double Distance = FVector::Dist(Where, Character->GetActorLocation());
		if (Where.Z <= -50000.0 || Distance >= Best || (bClose && *It == AimedBefore.Get()))
		{
			continue;
		}
		if (const UGTPThreatResponseComponent* Fear = It->FindComponentByClass<UGTPThreatResponseComponent>())
		{
			const EGTPThreatState State = Fear->GetState();
			if (State == EGTPThreatState::Fleeing || State == EGTPThreatState::Down || State == EGTPThreatState::Backing)
			{
				++Running;
				RunningFear += Fear->GetFear();
				continue;
			}
		}
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPGtaRunAim), false, Character);
		if (World->LineTraceSingleByChannel(Hit, EyeNow, Where + FVector(0.f, 0.f, 80.f), ECC_Visibility, Params) && Hit.GetActor() != *It)
		{
			++Hidden;
			continue;
		}
		Best = Distance;
		Victim = *It;
	}
	if (!Victim)
	{
		// Nobody to hold up yet: lower the weapon and drop the threat so the crowd can calm down (the aim of the
		// previous step kept every person in the cone afraid, and re-frightened the runners by contagion).
		Weapon->SetAiming(false);
		Threat->ClearThreat();
		Detail = FString::Printf(TEXT("waiting for a calm pedestrian in the clear (%d running at fear %.2f, %d hidden)"), Running,
			Running > 0 ? RunningFear / Running : 0.f, Hidden);
		return EResult::Pending;
	}
	// The gun comes out only near the person: the draw and the aim poses take the walk over (velocity written straight
	// into the movement is discarded too), so a player walking up with it drawn does not walk.
	if (Best <= (bClose ? 120.0 : 450.0) + 400.0)
	{
		Weapon->SetDrawn(true);
	}
	// Hold-up range is under 10 m: walk up to the person first (the aim cone and the fear both grow with closeness).
	// Stop at about 4 m (an aim from the shoulder camera misses somebody closer than 2 m), back off if too close.
	// The weapon stays lowered on the way: raised, the aim stance slows or pins the walk (measured: 15 m away after 60 s).
	const FVector ToVictim = (Victim->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal2D();
	const double StopCm = bClose ? 120.0 : 450.0;
	const double BackCm = bClose ? 70.0 : 250.0;
	Weapon->SetAiming(Best <= StopCm + 400.0);
	if (Best > StopCm)
	{
		PC->SetControlRotation(ToVictim.Rotation());
		WalkDirection = ToVictim;
		// The test shows a player who cannot walk instead of hiding it (a walk lock left by an arrest was found so).
		const double Now2 = FPlatformTime::Seconds();
		if (WalkAskedSince < 0.0)
		{
			// First tick of this walk: the counters of the failure line start here, so they describe this walk alone.
			WalkInputs = 0;
			WalkPendingBefore = 0;
			WalkRejected = 0;
		}
		LastAimMoveTime = (WalkAskedSince < 0.0 || Character->GetVelocity().Size2D() > 20.0) ? Now2 : WalkAskedSince;
		if (Now2 - LastAimMoveTime > 4.0)
		{
			const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			Detail = FString::Printf(TEXT("the player does not walk for 4 s (%.0f cm from %s, ignore move input = %d, movement mode %d, max walk speed %.0f, "
				"attached to %s, possessed by %s, movement active %d, collision %d)"), Best, *Victim->GetName(), PC->IsMoveInputIgnored() ? 1 : 0,
				Movement ? static_cast<int32>(Movement->MovementMode.GetValue()) : -1, Movement ? Movement->MaxWalkSpeed : -1.f,
				Character->GetAttachParentActor() ? *Character->GetAttachParentActor()->GetName() : TEXT("nothing"),
				Character->GetController() ? *Character->GetController()->GetName() : TEXT("nobody"),
				Movement && Movement->IsActive() ? 1 : 0, static_cast<int32>(Character->GetActorEnableCollision()));
			Detail += FString::Printf(TEXT(" [weapon drawn %d, aiming %d, root motion %d, anim montage playing %d, inputs left unconsumed %d, inputs refused %d, yaw actor %.0f control %.0f, use controller yaw %d, orient to movement %d, desired rotation %d]"), Weapon->IsDrawn() ? 1 : 0, Weapon->IsAiming() ? 1 : 0,
				Character->HasAnyRootMotion() ? 1 : 0, Character->GetMesh() && Character->GetMesh()->GetAnimInstance() && Character->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying() ? 1 : 0, WalkPendingBefore, WalkRejected, Character->GetActorRotation().Yaw,
				PC->GetControlRotation().Yaw, Character->bUseControllerRotationYaw ? 1 : 0, Movement && Movement->bOrientRotationToMovement ? 1 : 0,
				Movement && Movement->bUseControllerDesiredRotation ? 1 : 0);
			if (Movement)
			{
				const FVector Where = Character->GetActorLocation();
				Detail += FString::Printf(TEXT(" [at %.0f %.0f %.0f, velocity %.0f, acceleration %.0f of max %.0f, braking %.0f, on ground %d, walkable floor %d, tick %d, "
					"time dilation %.2f, input ignored by pawn %d, to victim %.0f %.0f, walk inputs sent %d, pending input %.2f, last input %.2f, walk direction %.2f %.2f]"), Where.X, Where.Y, Where.Z, Movement->Velocity.Size(),
					Movement->GetCurrentAcceleration().Size(), Movement->MaxAcceleration, Movement->BrakingDecelerationWalking,
					Movement->IsMovingOnGround() ? 1 : 0, Movement->CurrentFloor.IsWalkableFloor() ? 1 : 0, Movement->IsComponentTickEnabled() ? 1 : 0,
					Character->CustomTimeDilation * World->GetWorldSettings()->GetEffectiveTimeDilation(), Character->IsMoveInputIgnored() ? 1 : 0,
					ToVictim.X, ToVictim.Y, WalkInputs, Character->GetPendingMovementInputVector().Size(), Character->GetLastMovementInputVector().Size(),
					WalkDirection.X, WalkDirection.Y);
			}
			return EResult::Fail;
		}
	}
	else if (Best < BackCm)
	{
		WalkDirection = -ToVictim;
	}
	// The reticle on the person, as a player aims: every frame the weapon reports the aim from the eyes to
	// what the reticle covers, so the view (the camera over the shoulder) must look at the point the
	// pedestrian's fear tests (80 cm above a pedestrian's root, at the feet). Recomputed each frame: the
	// camera moves as the view turns.
	const FVector Body = Victim->GetActorLocation() + FVector(0.f, 0.f, 80.f);
	FVector View = Character->GetPawnViewLocation();
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(View, ViewRotation);
	PC->SetControlRotation((Body - View).Rotation());
	const FVector Aim = (Body - Character->GetPawnViewLocation()).GetSafeNormal();
	Character->SetActorRotation(FRotator(0.f, Aim.Rotation().Yaw, 0.f));
	Threat->SetThreat(Character, true, Character->GetPawnViewLocation(), Aim);
	int32 Hands = 0, Fleeing = 0, Alarmed = 0, Obedient = 0;
	for (TActorIterator<AGTPPedestrian> It(World); It; ++It)
	{
		if (const UGTPThreatResponseComponent* Fear = It->FindComponentByClass<UGTPThreatResponseComponent>())
		{
			switch (Fear->GetState())
			{
			case EGTPThreatState::HandsUp: case EGTPThreatState::Complying:
				++Hands;
				Obedient += Fear->GetTemperament() == EGTPTemperament::Compliant || Fear->GetTemperament() == EGTPTemperament::Frozen ? 1 : 0;
				break;
			case EGTPThreatState::Fleeing: case EGTPThreatState::Backing: ++Fleeing; break;
			case EGTPThreatState::Alarmed: ++Alarmed; break;
			default: break;
			}
		}
	}
	// Strict: a balance 3 s after the first reaction, and at least one compliant or frozen person with his hands up
	// (the fleeing temperament runs by design and proves nothing about the hold-up).
	const double Now = FPlatformTime::Seconds();
	// Only a hold-up at the intended range counts: reactions caught while still walking up (contagion, the alarm) prove nothing.
	const bool bInRange = Best <= StopCm + 60.0;
	if (bInRange && AimFirstReaction < 0.0 && Hands + Fleeing > 0)
	{
		AimFirstReaction = Now;
	}
	const bool bSettled = AimFirstReaction >= 0.0 && Now - AimFirstReaction >= 3.0;
	// Every 5 s: why the runners still run (state time, fear, time since any threat, fear around them).
	static double LastFearLog = -10.0;
	if (Now - LastFearLog > 5.0)
	{
		LastFearLog = Now;
		int32 Logged = 0;
		for (TActorIterator<AGTPPedestrian> It(World); It && Logged < 4; ++It)
		{
			const UGTPThreatResponseComponent* Fear = It->FindComponentByClass<UGTPThreatResponseComponent>();
			if (Fear && Fear->GetState() == EGTPThreatState::Fleeing && It->GetActorLocation().Z > -50000.0)
			{
				++Logged;
				GTP_TRACE(LogGTPGtaRun, TEXT("GTARun fear: %s fleeing %.1f s, fear %.2f, no threat for %.1f s, fear around %.2f, temperament %d"),
					*It->GetName(), Fear->GetStateTime(), Fear->GetFear(), Fear->GetSinceThreat(), Fear->GetLastNearbyFear(), static_cast<int32>(Fear->GetTemperament()));
			}
		}
	}
	Detail = FString::Printf(TEXT("%saiming at %s (%.0f cm, walking at %.0f cm/s, aiming %d): %d hands up (%d compliant or frozen), %d fleeing, %d alarmed%s"),
		bClose ? TEXT("close, ") : TEXT(""), *Victim->GetName(), Best, Character->GetVelocity().Size2D(), Weapon->IsAiming() ? 1 : 0, Hands, Obedient, Fleeing, Alarmed, bSettled ? TEXT(", settled") : TEXT(""));
	if (bInRange && bSettled && Obedient > 0)
	{
		// What the player sees at that moment, for the eye: the states counted above say nothing of the pose shown
		// (an animation of the wrong skeleton raises no hands). Written at the end of the frame, hands still up.
		const FString Shot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ScreenShotDir(), bClose ? TEXT("gtarun-aimclose.png") : TEXT("gtarun-aim.png")));
		FScreenshotRequest::RequestScreenshot(Shot, false, false);
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: picture of the hold-up asked: %s"), *Shot);
		// The picture at night is not readable: the same in numbers. Heights in cm above the player's feet and above
		// each body's own root; "hands over the head" is what a hold-up looks like.
		const float Feet = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
		auto Pose = [](const USkeletalMeshComponent* Mesh) -> FString
		{
			if (!Mesh || Mesh->GetBoneIndex(TEXT("head")) == INDEX_NONE)
			{
				return TEXT("no head bone");
			}
			const float Root = Mesh->GetComponentLocation().Z;
			const float Head = Mesh->GetSocketLocation(TEXT("head")).Z;
			const float Left = Mesh->GetBoneIndex(TEXT("hand_l")) != INDEX_NONE ? Mesh->GetSocketLocation(TEXT("hand_l")).Z : -1e4f;
			const float Right = Mesh->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE ? Mesh->GetSocketLocation(TEXT("hand_r")).Z : -1e4f;
			const UAnimSingleNodeInstance* Single = Mesh->GetSingleNodeInstance();
			const UAnimationAsset* Played = Single ? Single->GetAnimationAsset() : nullptr;
			const UAnimInstance* Anim = Mesh->GetAnimInstance();
			return FString::Printf(TEXT("head %.0f above its root, hands %.0f %.0f from the head (%s), plays %s, mesh %s"), Head - Root, Left - Head, Right - Head,
				Left > Head && Right > Head ? TEXT("hands over the head") : TEXT("hands NOT over the head"),
				Played ? *Played->GetName() : (Anim ? *Anim->GetClass()->GetName() : TEXT("nothing")),
				Mesh->GetSkeletalMeshAsset() ? *Mesh->GetSkeletalMeshAsset()->GetName() : TEXT("none"));
		};
		int32 Probed = 0;
		for (TActorIterator<AGTPPedestrian> It(World); It && Probed < 4; ++It)
		{
			const UGTPThreatResponseComponent* Fear = It->FindComponentByClass<UGTPThreatResponseComponent>();
			const EGTPThreatState State = Fear ? Fear->GetState() : EGTPThreatState::Calm;
			if ((State != EGTPThreatState::HandsUp && State != EGTPThreatState::Complying) || FVector::Dist(It->GetActorLocation(), Character->GetActorLocation()) > 1500.0)
			{
				continue;
			}
			++Probed;
			const FVector To = Character->GetActorLocation() - It->GetActorLocation();
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun hold-up probe: %s%s at %.0f cm, root %.0f cm above the player's feet, yaw %.0f (the player is at yaw %.0f from him): %s"),
				*It->GetName(), *It == Victim ? TEXT(" (the one aimed at)") : TEXT(""), To.Size2D(), It->GetBody()->GetComponentLocation().Z - Feet,
				It->GetActorRotation().Yaw, To.Rotation().Yaw, *Pose(It->GetBody()));
		}
		TArray<AActor*> Attached;
		Character->GetAttachedActors(Attached);
		for (const AActor* Visual : Attached)
		{
			TArray<USkeletalMeshComponent*> Meshes;
			Visual->GetComponents<USkeletalMeshComponent>(Meshes);
			for (const USkeletalMeshComponent* Mesh : Meshes)
			{
				if (Mesh->GetBoneIndex(TEXT("head")) != INDEX_NONE && Mesh->IsVisible())
				{
					GTP_TRACE(LogGTPGtaRun, TEXT("GTARun hold-up probe: the player (%s.%s): %s"), *Visual->GetName(), *Mesh->GetName(), *Pose(Mesh));
					break;
				}
			}
		}
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun hold-up probe: the player's own mesh: %s; camera pitch %.0f roll %.0f"), *Pose(Character->GetMesh()),
			PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Pitch : 0.f, PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Roll : 0.f);
		Threat->ClearThreat();
		if (!bClose)
		{
			AimedBefore = Victim;
		}
		return EResult::Ok;
	}
	return EResult::Pending;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckShoot(FString& Detail)
{
	UWorld* World = GetWorld();
	APlayerController* PC = FirstController(World);
	if (!PC || !PC->GetPawn())
	{
		return EResult::Pending;
	}
	if (AGTPVehicle* Car = Cast<AGTPVehicle>(PC->GetPawn()))
	{
		if (!bExitRequested)
		{
			bExitRequested = true;
			Car->RequestExit();
		}
		Detail = TEXT("getting out");
		return EResult::Pending;
	}
	ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
	UGTPWeaponComponent* Weapon = Character ? Character->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
	if (!Weapon)
	{
		Detail = TEXT("the player has no weapon component");
		return EResult::Fail;
	}
	const double Now = FPlatformTime::Seconds();
	if (Now < NextShot)
	{
		return EResult::Pending;
	}
	// Busted in the chase (step 2): the police took the ammunition, as in GTA. The run tests the
	// shot, not the arrest, so the pistol gets a box of rounds back and loads it before firing.
	if (Weapon->GetMagazine() + Weapon->GetReserve() <= 0)
	{
		const int32 Given = Weapon->AddAmmo(36);
		Weapon->SetDrawn(true);
		Weapon->Reload();
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: player re-armed after the arrest (+%d rounds)"), Given);
	}
	if (Weapon->IsReloading())
	{
		// Not a try: the magazine is going in.
		NextShot = Now + 0.25;
		Detail = TEXT("reloading");
		return EResult::Pending;
	}
	// The nearest other person, else a point ahead.
	FVector Target = Character->GetActorLocation() + Character->GetActorForwardVector() * 2000.f;
	double Best = 4000.0;
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (*It != Character)
		{
			const double Distance = FVector::Dist(It->GetActorLocation(), Character->GetActorLocation());
			if (Distance < Best)
			{
				Best = Distance;
				Target = It->GetActorLocation() + FVector(0.f, 0.f, 40.f);
			}
		}
	}
	for (TActorIterator<AGTPPedestrian> It(World); It; ++It)
	{
		const FVector Where = It->GetActorLocation();
		const double Distance = FVector::Dist(Where, Character->GetActorLocation());
		if (Where.Z > -50000.0 && Distance < Best)
		{
			Best = Distance;
			Target = Where + FVector(0.f, 0.f, 40.f);
		}
	}
	Weapon->SetDrawn(true);
	++ShotAttempts;
	if (Weapon->FireAt(Target, 1.f))
	{
		++ShotsFired;
	}
	NextShot = Now + 0.5;
	Detail = FString::Printf(TEXT("%d shots of %d tries, magazine %d"), ShotsFired, ShotAttempts, Weapon->GetMagazine());
	if (ShotsFired >= 3)
	{
		return EResult::Ok;
	}
	return ShotAttempts >= 12 ? EResult::Fail : EResult::Pending;
}
