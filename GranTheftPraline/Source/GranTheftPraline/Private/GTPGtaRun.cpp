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
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GTPCombat.h"
#include "GTPDemoRoute.h"
#include "GTPEnvironmentSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "GTPHealth.h"
#include "GTPLog.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "GTPPedestrians.h"
#include "GTPThreat.h"
#include "GTPTheftDemo.h"
#include "GTPVehicle.h"
#include "GTPVehicleInteractionComponent.h"
#include "GTPMassTrafficCars.h"
#include "GTPTraffic.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
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
		enum Type : int32 { Carjack, Witness, Chase, Pursuit, Outcome, Parked, WalkArmed, Aim, AimClose, Shoot, Wasted, Crash, Count };
	}
	constexpr int32 StepCount = EStep::Count;
	const TCHAR* StepNames[StepCount] = { TEXT("carjack"), TEXT("witness"), TEXT("chase"), TEXT("pursuit"), TEXT("outcome"), TEXT("parked"),
		TEXT("walkarmed"), TEXT("aim"), TEXT("aimclose"), TEXT("shoot"), TEXT("wasted"), TEXT("crash") };
	// The steps of the wanted level, counted on their own line: the gate's rule on the seven others reads as before.
	const bool StepIsPolice[StepCount] = { false, true, false, true, true, false, false, false, false, false, true, false };
	constexpr int32 PoliceStepCount = 4;
	// Informative steps, in neither count: the crash step until the near traffic cars have a body.
	const bool StepIsInfo[StepCount] = { false, false, false, false, false, false, false, false, false, false, false, true };
	constexpr int32 InfoStepCount = 1;
	// Time limits (s) per step, counted from the step's start.
	// Witness: the call takes 4 s (GTPWitnessLogic.h). Chase: 3 stars come at 30 s and a car that flees has its
	// 150 m by then; a chase that fails must leave the other steps their time (170 s of it ate the bench's 330 s
	// on 16/11). Pursuit: its 40 s window. Outcome: a stopped player is arrested in 3 s once a unit is within 7 m;
	// the units have 60 s to come. Wasted: 6 s dead, then the respawn. Crash: a few seconds to get in, then three
	// tries of at most 13 s each.
	const double StepLimits[StepCount] = { 90.0, 12.0, 90.0, 50.0, 60.0, 200.0, 40.0, 90.0, 90.0, 30.0, 30.0, 75.0 };
	// Crash step: the player's car starts this far behind its target (centre to centre, cm), about 13 m of run-up.
	constexpr float CrashRunUpCm = 1800.f;
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
	FTSTicker::GetCoreTicker().RemoveTicker(CrashHandle);
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
	HoldUpProvedSince = -1.0;
	AimedHeld = nullptr;
	AimedHeldSince = 0.0;
	LastAimMoveTime = -1.0;
	WalkDirection = FVector::ZeroVector;
	WalkArmedPhase = 0;
	WalkArmedPhaseStart = -1.0;
	bWalkArmedExit = false;
	OutcomePhase = 0;
	bWastedExit = false;
	WastedPhase = 0;
	WastedTries = 0;
	Crash = FCrashRun();
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
	(StepIsInfo[Step] ? InfoOkCount : StepIsPolice[Step] ? PoliceOkCount : OkCount) += bOk ? 1 : 0;
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun step %d: %s %s %s (%.0f s)%s"), Step + 1, StepNames[Step], bOk ? TEXT("ok") : TEXT("fail"), *Detail,
		FPlatformTime::Seconds() - StepStart, StepIsInfo[Step] ? TEXT(" [informative, not counted]") : TEXT(""));
	if (Step == EStep::Crash)
	{
		EndCrashDrive();
	}
	if (Step == EStep::Chase)
	{
		StartChasePicture();
	}
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
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun informative: %d/%d ok (crash)"), InfoOkCount, InfoStepCount);
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun done: %d/%d ok (%.0f s)"), OkCount, StepCount - PoliceStepCount - InfoStepCount, FPlatformTime::Seconds() - RunStart);
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
	TickChasePicture();
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
	case EStep::Wasted: Result = CheckWasted(Detail); break;
	default: Result = CheckCrash(Detail); break;
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

void UGTPGtaRun::StartChasePicture()
{
	UWorld* World = GetWorld();
	APlayerController* PC = FirstController(World);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || ChasePicturePhase != 0)
	{
		return;
	}
	// By day: the clock runs 30 times faster than the run, and a picture taken at dusk shows headlights and little else.
	if (UGTPEnvironmentSubsystem* Environment = World->GetSubsystem<UGTPEnvironmentSubsystem>())
	{
		const double Hours = Environment->GetTimeOfDay();
		if (Hours < 9.0 || Hours > 16.0)
		{
			Environment->SetTimeOfDay(14.0);
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: the clock was at %.1f h, set to 14 h for the picture of the chase"), Hours);
		}
	}
	// The nearest police car: the camera stands on the far side of the player's car from it and looks back at both.
	const FVector Car = Pawn->GetActorLocation();
	FVector NearestAt = FVector::ZeroVector;
	double NearestCm = -1.0;
	if (const UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
	{
		for (const TWeakObjectPtr<AGTPVehicle>& Unit : Wanted->GetPolice())
		{
			if (Unit.IsValid())
			{
				const double Cm = FVector::Dist2D(Unit->GetActorLocation(), Car);
				if (NearestCm < 0.0 || Cm < NearestCm)
				{
					NearestCm = Cm;
					NearestAt = Unit->GetActorLocation();
				}
			}
		}
	}
	FVector Away = NearestCm > 100.0 ? (Car - NearestAt).GetSafeNormal2D() : Pawn->GetActorForwardVector().GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = FVector::ForwardVector;
	}
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Away);
	const FVector From = Car + FVector(0.f, 0.f, 150.f);
	FVector Eye = Car + Away * 900.f + Side * 250.f + FVector(0.f, 0.f, 320.f);
	// Not behind a wall: brought back to this side of whatever stands between the car and the camera.
	FHitResult Wall;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPGtaRunPicture), false, Pawn);
	if (World->LineTraceSingleByChannel(Wall, From, Eye, ECC_Visibility, Params))
	{
		Eye = Wall.ImpactPoint - (Eye - From).GetSafeNormal() * 60.f;
	}
	// It looks at the car, and a third of the way towards the nearest unit when that one is within 120 m.
	const FVector Look = (NearestCm > 0.0 && NearestCm < 12000.0 ? Car + (NearestAt - Car) * 0.35 : Car) + FVector(0.f, 0.f, 80.f);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform((Look - Eye).Rotation(), Eye), Spawn);
	if (!Camera)
	{
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: no camera for the picture of the chase"));
		return;
	}
	if (UCameraComponent* Lens = Camera->GetCameraComponent())
	{
		Lens->SetFieldOfView(70.f);
		Lens->SetConstraintAspectRatio(false);
		Lens->PostProcessBlendWeight = 1.f;
		Lens->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Lens->PostProcessSettings.MotionBlurAmount = 0.f;
	}
	// It rides with the car: the picture is taken on the next tick of the run, half a second from now.
	Camera->AttachToActor(Pawn, FAttachmentTransformRules::KeepWorldTransform);
	PC->SetViewTarget(Camera);
	ChasePictureCamera = Camera;
	ChasePicturePhase = 1;
}

void UGTPGtaRun::TickChasePicture()
{
	if (ChasePicturePhase == 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	APlayerController* PC = FirstController(World);
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	ACameraActor* Camera = ChasePictureCamera.Get();
	if (ChasePicturePhase == 1 && Pawn && Camera)
	{
		// What the picture shows, in numbers (nobody on the PC looks at it before it is copied): which cars are inside
		// the frame with nothing between them and the camera.
		const FVector Eye = Camera->GetActorLocation();
		int32 Width = 0, Height = 0;
		PC->GetViewportSize(Width, Height);
		const auto InFrame = [PC, World, Pawn, Eye, Width, Height](const AActor& Seen)
		{
			const FVector At = Seen.GetActorLocation() + FVector(0.f, 0.f, 80.f);
			FVector2D Screen;
			if (!PC->ProjectWorldLocationToScreen(At, Screen) || Screen.X < 0.0 || Screen.Y < 0.0 || Screen.X > Width || Screen.Y > Height)
			{
				return false;
			}
			FHitResult Between;
			FCollisionQueryParams Sight(SCENE_QUERY_STAT(GTPGtaRunPictureSight), false, Pawn);
			Sight.AddIgnoredActor(&Seen);
			return !World->LineTraceSingleByChannel(Between, Eye, At, ECC_Visibility, Sight);
		};
		int32 Units = 0, UnitsShown = 0;
		double NearestShownCm = -1.0;
		if (const UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
		{
			for (const TWeakObjectPtr<AGTPVehicle>& Unit : Wanted->GetPolice())
			{
				if (!Unit.IsValid())
				{
					continue;
				}
				++Units;
				if (InFrame(*Unit))
				{
					++UnitsShown;
					const double Cm = FVector::Dist2D(Unit->GetActorLocation(), Pawn->GetActorLocation());
					NearestShownCm = NearestShownCm < 0.0 ? Cm : FMath::Min(NearestShownCm, Cm);
				}
			}
		}
		const UGTPEnvironmentSubsystem* Environment = World->GetSubsystem<UGTPEnvironmentSubsystem>();
		const FString Shot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ScreenShotDir(), TEXT("gtarun-chase.png")));
		FScreenshotRequest::RequestScreenshot(Shot, false, false);
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: picture of the chase asked: %s; the player's car %s the frame, %.0f m from the camera; %d police car(s) on the road, %d of them in the frame with nothing in front, the nearest of those %.0f m from the player's car (-1 = none); clock %.1f h (-1 = no day and night), motion blur off"),
			*Shot, InFrame(*Pawn) ? TEXT("in") : TEXT("NOT in"), FVector::Dist(Eye, Pawn->GetActorLocation()) / 100.0, Units, UnitsShown,
			NearestShownCm < 0.0 ? -1.0 : NearestShownCm / 100.0, Environment ? Environment->GetTimeOfDay() : -1.0);
		ChasePicturePhase = 2;
		return;
	}
	// The tick after the picture (or nothing to take it with): the view goes back to the player.
	if (PC && Pawn && (!Camera || PC->GetViewTarget() == Camera))
	{
		PC->SetViewTarget(Pawn);
	}
	if (Camera)
	{
		Camera->Destroy();
	}
	ChasePictureCamera = nullptr;
	ChasePicturePhase = 0;
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
	// A pursuit the test helped proves nothing: a police car made close to the player has not driven to him, and a
	// player's car put back on a lane by the script has not been followed there. Either one fails the step, whatever
	// the numbers (candidate 49 passed with 4 units put back at 50 to 120 m and the player's car moved 4 times).
	const int32 Relocations = Demo ? Demo->GetChaseRelocations() : 0;
	const bool bUnaided = GTPChase::Unaided(Tally, Settings) && Relocations == 0;
	Detail = FString::Printf(TEXT("%s%s: units mean=%.0f km/h (wanted %.0f, player %.0f) peak=%.0f, moving %.0f %% of their time, nearest mean=%.0f m min=%.0f m, "
		"within %.0f m %.0f %% of %.0f s (wanted %.0f %%), up to %d units, %d sent, %d of them replacements (nearest made %.0f m from the player, wanted %.0f at least, -1 = none), "
		"%d back-off(s), %d unit(s) given up, the player's car put back on a lane %d time(s) (wanted 0), stars=%d"),
		ANSI_TO_TCHAR(GTPChase::VerdictName(Verdict)), bUnaided ? TEXT("") : TEXT(", but the test helped"), Tally.MeanKmh(), GTPChase::WantedMeanKmh(Tally, Settings),
		Tally.PlayerMeanKmh(), Tally.UnitPeakKmh, Tally.MovingShare() * 100.f, Tally.MeanNearest(), Tally.NearestMin, Settings.NearMetres, Tally.NearShare() * 100.f,
		static_cast<float>(Tally.Seconds), Settings.MinNearShare * 100.f, Tally.PeakUnits, Tally.Sent, Tally.Replacements, Tally.ReplacementNearest,
		Settings.ReplacementMinMetres, Tally.BackOffs, Tally.GivenUp, Relocations, Wanted->GetStars());
	// Over when the window is, or when nobody is wanted any more (lost, arrested, dead: the outcome step says which).
	if (FPlatformTime::Seconds() - StepStart < PursuitSeconds && Wanted->GetStars() > 0)
	{
		return EResult::Pending;
	}
	return Verdict == GTPChase::EVerdict::Follows && bUnaided ? EResult::Ok : EResult::Fail;
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
	if (!bHoldUpLit)
	{
		// The pictures of the hold-up are for the eye: by day, motion blur off. At night with the blur of a turning
		// camera the person aimed at could not be read on any of them (gates 46 to 49). Said in the log, not hidden.
		bHoldUpLit = true;
		UGTPEnvironmentSubsystem* Environment = World->GetSubsystem<UGTPEnvironmentSubsystem>();
		const double Before = Environment ? Environment->GetTimeOfDay() : -1.0;
		if (Environment)
		{
			Environment->SetTimeScale(0.f);
			Environment->SetTimeOfDay(14.0);
		}
		if (GEngine)
		{
			GEngine->Exec(World, TEXT("r.MotionBlurQuality 0"));
		}
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: hold-up pictured by day: time of day %.1f h -> 14.0 h and stopped, motion blur off"), Before);
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
	// The gun is raised only inside the hold-up range (GTPThreatLogic.h: from closer a person surrenders, from further
	// away he runs). The test used to threaten from any distance: people ran at 16 m and it chased them for 90 s.
	const double AimFromCm = FMath::Min(StopCm + 400.0, static_cast<double>(GTPThreat::FSettings().SurrenderRangeCm) - 100.0);
	const bool bAimNow = Best <= AimFromCm;
	Weapon->SetAiming(bAimNow);
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
	// pedestrian's fear tests (his chest). Recomputed each frame: the camera moves as the view turns.
	const FVector Body = UGTPThreatResponseComponent::AimPointOf(Victim);
	FVector View = Character->GetPawnViewLocation();
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(View, ViewRotation);
	PC->SetControlRotation((Body - View).Rotation());
	const FVector Aim = (Body - Character->GetPawnViewLocation()).GetSafeNormal();
	// The body: while the gun is raised the weapon turns it to the view, as in play (UGTPWeaponComponent::TickComponent).
	// The test used to turn it to the person every frame on top of that, which no player can do: at the close hold-up of
	// candidate 57 the camera was beside the player (33 cm ahead of his eyes, 1.5 m to his side) and the person was not
	// in the picture. It turns the body itself only with the gun down.
	if (!Weapon->IsAiming())
	{
		Character->SetActorRotation(FRotator(0.f, Aim.Rotation().Yaw, 0.f));
	}
	// Where the person is in what the player sees: degrees from the reticle (the middle of the view), from the camera.
	double OffReticleDeg = 180.0;
	bool bInPicture = false;
	FString Framing = TEXT("no camera");
	if (PC->PlayerCameraManager)
	{
		const FVector CameraAt = PC->PlayerCameraManager->GetCameraLocation();
		const FRotator CameraLooks = PC->PlayerCameraManager->GetCameraRotation();
		const FVector Seen = CameraLooks.UnrotateVector(Body - CameraAt);
		const double Right = FMath::RadiansToDegrees(FMath::Atan2(Seen.Y, Seen.X));
		const double Up = FMath::RadiansToDegrees(FMath::Atan2(Seen.Z, Seen.X));
		OffReticleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Seen.GetSafeNormal().X, -1.0, 1.0)));
		// The picture is 16:9: half the field of view each side, 9/16 of its tangent up and down.
		const double HalfSide = PC->PlayerCameraManager->GetFOVAngle() * 0.5;
		const double HalfUp = FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(HalfSide)) * 9.0 / 16.0));
		bInPicture = Seen.X > 0.0 && FMath::Abs(Right) < HalfSide && FMath::Abs(Up) < HalfUp;
		const double Line = (Victim->GetActorLocation() - Character->GetActorLocation()).Rotation().Yaw;
		Framing = FString::Printf(TEXT("his chest is %.0f degrees from the reticle (%.0f to the right, %.0f up, the picture shows %.0f each side and %.0f up and down), %.0f cm from the camera; "
			"from the line from the player to him, the view looks %.0f degrees aside and the player's body faces %.0f aside"),
			OffReticleDeg, Right, Up, HalfSide, HalfUp, Seen.Size(),
			FMath::FindDeltaAngleDegrees(Line, static_cast<double>(CameraLooks.Yaw)), FMath::FindDeltaAngleDegrees(Line, static_cast<double>(Character->GetActorRotation().Yaw)));
	}
	if (bAimNow)
	{
		Threat->SetThreat(Character, true, Character->GetPawnViewLocation(), Aim);
	}
	else if (Threat->GetSource() == Character)
	{
		Threat->ClearThreat();
	}
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
	// What a hold-up looks like, read on the body itself: the states counted above say nothing of the pose shown
	// (gate of candidate 48: "1 hands up" counted while the person aimed at ran, and another walked into the player).
	// A person shows his hands up when he stands upright with both hands higher than his head.
	auto Pose = [](const USkeletalMeshComponent* Mesh, bool* bHandsShown) -> FString
	{
		if (bHandsShown)
		{
			*bHandsShown = false;
		}
		if (!Mesh || Mesh->GetBoneIndex(TEXT("head")) == INDEX_NONE)
		{
			return TEXT("no head bone");
		}
		const FVector Root = Mesh->GetComponentLocation();
		const FVector Head = Mesh->GetSocketLocation(TEXT("head"));
		const float Left = Mesh->GetBoneIndex(TEXT("hand_l")) != INDEX_NONE ? Mesh->GetSocketLocation(TEXT("hand_l")).Z : -1e4f;
		const float Right = Mesh->GetBoneIndex(TEXT("hand_r")) != INDEX_NONE ? Mesh->GetSocketLocation(TEXT("hand_r")).Z : -1e4f;
		const float Pelvis = Mesh->GetBoneIndex(TEXT("pelvis")) != INDEX_NONE ? Mesh->GetSocketLocation(TEXT("pelvis")).Z : -1e4f;
		const float Foot = Mesh->GetBoneIndex(TEXT("foot_l")) != INDEX_NONE ? Mesh->GetSocketLocation(TEXT("foot_l")).Z : -1e4f;
		const bool bUpright = Head.Z - Root.Z > 100.f;
		const bool bOver = Left > Head.Z && Right > Head.Z;
		if (bHandsShown)
		{
			*bHandsShown = bUpright && bOver;
		}
		const UAnimSingleNodeInstance* Single = Mesh->GetSingleNodeInstance();
		const UAnimationAsset* Played = Single ? Single->GetAnimationAsset() : nullptr;
		const UAnimInstance* Anim = Mesh->GetAnimInstance();
		const FRotator Turn = Mesh->GetComponentRotation();
		return FString::Printf(TEXT("head %.0f above its root and %.0f beside it, pelvis %.0f, foot %.0f (%s), hands %.0f %.0f from the head (%s), body pitch %.0f roll %.0f scale %.2f, physics %d, rendered %.1f s ago, plays %s, mesh %s"),
			Head.Z - Root.Z, FVector::Dist2D(Head, Root), Pelvis - Root.Z, Foot - Root.Z, bUpright ? TEXT("upright") : TEXT("NOT UPRIGHT"), Left - Head.Z, Right - Head.Z,
			bOver ? TEXT("hands over the head") : TEXT("hands NOT over the head"), Turn.Pitch, Turn.Roll, Mesh->GetComponentScale().Z, Mesh->IsSimulatingPhysics() ? 1 : 0,
			Mesh->GetWorld() ? Mesh->GetWorld()->GetTimeSeconds() - Mesh->GetLastRenderTime() : -1.f,
			Played ? *Played->GetName() : (Anim ? *Anim->GetClass()->GetName() : TEXT("nothing")),
			Mesh->GetSkeletalMeshAsset() ? *Mesh->GetSkeletalMeshAsset()->GetName() : TEXT("none"));
	};
	const AGTPPedestrian* Aimed = Cast<AGTPPedestrian>(Victim);
	const UGTPThreatResponseComponent* AimedFear = Victim->FindComponentByClass<UGTPThreatResponseComponent>();
	const bool bAimedHeld = AimedFear && (AimedFear->GetState() == EGTPThreatState::HandsUp || AimedFear->GetState() == EGTPThreatState::Complying);
	bool bShown = false;
	const FString AimedPose = Aimed ? Pose(Aimed->GetBody(), &bShown) : FString(TEXT("not a pedestrian"));
	// Since when this same person is held up at the intended range.
	if (!bInRange || !bAimedHeld || AimedHeld.Get() != Victim)
	{
		AimedHeld = bInRange && bAimedHeld ? Victim : nullptr;
		AimedHeldSince = Now;
	}
	Detail += FString::Printf(TEXT("; the one aimed at: state %d for %.1f s, %s"), AimedFear ? static_cast<int32>(AimedFear->GetState()) : -1, Now - AimedHeldSince, *AimedPose);
	// Every 5 s the step says where it is: a timeout line alone shows only its last moment (candidate 49 in the editor:
	// 90 s of the close hold-up, and one line saying a person 5 m away lay on his side).
	static double LastAimLog = -10.0;
	if (Now - LastAimLog > 5.0)
	{
		LastAimLog = Now;
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun aim: %s"), *Detail);
	}
	const bool bProved = bInRange && bAimedHeld && bShown && Now - AimedHeldSince >= 1.0;
	// His state says hands up for 5 s and his body does not show it: that is the defect, said at once.
	const bool bStateOnly = bInRange && bAimedHeld && !bShown && Now - AimedHeldSince >= 5.0;
	if (!bProved && !bStateOnly)
	{
		HoldUpProvedSince = -1.0;
	}
	else
	{
		// The pose is proved; the picture is for the eye, and shows nothing unless the view has come onto the person
		// (the view turns to him every frame above, as a player's does). Up to 3 s for that, then the picture is
		// taken as it is and the line below says that he is not in it: said, not hidden.
		if (HoldUpProvedSince < 0.0)
		{
			HoldUpProvedSince = Now;
		}
		if (bProved && OffReticleDeg > 8.0 && Now - HoldUpProvedSince < 3.0)
		{
			Detail += TEXT("; proved, waiting for the view to come onto him for the picture");
			return EResult::Pending;
		}
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun hold-up picture: the one aimed at is %s (%.1f s after the proof): %s"),
			bInPicture ? (OffReticleDeg <= 8.0 ? TEXT("IN THE PICTURE, under the reticle") : TEXT("IN THE PICTURE, away from the reticle")) : TEXT("NOT IN THE PICTURE"),
			Now - HoldUpProvedSince, *Framing);
		HoldUpProvedSince = -1.0;
		// What the player sees at that moment, for the eye. Written at the end of the frame, hands still up.
		const FString Shot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ScreenShotDir(), bClose ? TEXT("gtarun-aimclose.png") : TEXT("gtarun-aim.png")));
		FScreenshotRequest::RequestScreenshot(Shot, false, false);
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: picture of the hold-up asked: %s"), *Shot);
		// The picture at night is not readable: the same in numbers. Heights in cm above the player's feet and above
		// each body's own root.
		const float Feet = Character->GetActorLocation().Z - Character->GetSimpleCollisionHalfHeight();
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
				It->GetActorRotation().Yaw, To.Rotation().Yaw, *Pose(It->GetBody(), nullptr));
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
					GTP_TRACE(LogGTPGtaRun, TEXT("GTARun hold-up probe: the player (%s.%s): %s"), *Visual->GetName(), *Mesh->GetName(), *Pose(Mesh, nullptr));
					break;
				}
			}
		}
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun hold-up probe: the player's own mesh: %s; camera pitch %.0f roll %.0f"), *Pose(Character->GetMesh(), nullptr),
			PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Pitch : 0.f, PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraRotation().Roll : 0.f);
		if (PC->PlayerCameraManager)
		{
			// Where the camera is when the picture is taken: the close hold-up of candidate 51 gave a black picture, with
			// only the HUD on it. Inside what, how far from the two heads, and whether the screen is faded.
			const FVector Camera = PC->PlayerCameraManager->GetCameraLocation();
			const FVector Eyes = Character->GetPawnViewLocation();
			const FVector HisHead = Aimed && Aimed->GetBody() ? Aimed->GetBody()->GetSocketLocation(TEXT("head")) : Victim->GetActorLocation();
			FCollisionQueryParams CameraParams(SCENE_QUERY_STAT(GTPGtaRunCamera), false, Character);
			FHitResult Between;
			const bool bBlocked = World->LineTraceSingleByChannel(Between, Eyes, Camera, ECC_Camera, CameraParams);
			FHitResult Ahead;
			const bool bWall = World->LineTraceSingleByChannel(Ahead, Camera, Camera + PC->PlayerCameraManager->GetCameraRotation().Vector() * 300.0, ECC_Visibility, CameraParams);
			TArray<FOverlapResult> Around;
			World->OverlapMultiByChannel(Around, Camera, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(8.f), CameraParams);
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun hold-up probe: the camera is %.0f cm from the player's eyes (%.0f behind, %.0f above), %.0f cm from the head of the one aimed at, "
				"field of view %.0f; from the eyes to it: %s; within 8 cm of it: %s; first thing within 3 m ahead of it: %s; screen fade %.2f"),
				FVector::Dist(Camera, Eyes), FVector::DotProduct(Eyes - Camera, Character->GetActorForwardVector()), Camera.Z - Eyes.Z, FVector::Dist(Camera, HisHead),
				PC->PlayerCameraManager->GetFOVAngle(),
				bBlocked ? *FString::Printf(TEXT("BLOCKED by %s"), Between.GetActor() ? *Between.GetActor()->GetName() : TEXT("something")) : TEXT("clear"),
				Around.Num() > 0 ? *FString::Printf(TEXT("INSIDE %s"), Around[0].GetActor() ? *Around[0].GetActor()->GetName() : TEXT("something")) : TEXT("nothing"),
				bWall ? *FString::Printf(TEXT("%s at %.0f cm"), Ahead.GetActor() ? *Ahead.GetActor()->GetName() : TEXT("something"), Ahead.Distance) : TEXT("nothing"),
				PC->PlayerCameraManager->FadeAmount);
		}
		Threat->ClearThreat();
		if (!bClose)
		{
			AimedBefore = Victim;
		}
		if (bStateOnly)
		{
			Detail = FString::Printf(TEXT("%s has his hands up in his state for %.0f s, not on his body (%.0f cm away): %s"), *Victim->GetName(), Now - AimedHeldSince, Best, *AimedPose);
			return EResult::Fail;
		}
		Detail = FString::Printf(TEXT("%s at %.0f cm stands with his hands over his head: %s"), *Victim->GetName(), Best, *AimedPose);
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

namespace GTPGtaRunCrash
{
	/** A car's axis on the ground plane, and its right. */
	void Axes(const FVector& Heading, FVector& OutForward, FVector& OutRight)
	{
		OutForward = FVector(Heading.X, Heading.Y, 0.0).GetSafeNormal();
		if (OutForward.IsNearlyZero())
		{
			OutForward = FVector::ForwardVector;
		}
		OutRight = FVector::CrossProduct(FVector::UpVector, OutForward);
	}

	/** What a trace from the player's car ignores: the car and whoever rides in it. */
	FCollisionQueryParams IgnoringCar(const AGTPVehicle& Car)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTARunCrash), false, &Car);
		TArray<AActor*> Riders;
		Car.GetAttachedActors(Riders);
		Params.AddIgnoredActors(Riders);
		return Params;
	}

	/** The run-up's start behind At on its axis, on the ground found there (false: that ground is 1.5 m off the car's). */
	bool SpotBehind(const UWorld& World, const AGTPVehicle& Car, const FVector& At, const FVector& Forward, FVector& Out)
	{
		const FVector Flat = At - Forward * CrashRunUpCm;
		FHitResult Hit;
		if (World.LineTraceSingleByChannel(Hit, Flat + FVector(0.0, 0.0, 300.0), Flat - FVector(0.0, 0.0, 500.0), ECC_Visibility, IgnoringCar(Car)))
		{
			if (FMath::Abs(Hit.ImpactPoint.Z - At.Z) > 150.0)
			{
				return false;
			}
			// As GTPVehicle's launch test: 30 cm above the road, the wheels settle.
			Out = Hit.ImpactPoint + FVector(0.0, 0.0, 30.0);
			return true;
		}
		Out = Flat + FVector(0.0, 0.0, 30.0);
		return true;
	}
}

int32 UGTPGtaRun::ChooseCrashTarget(UWorld& World, const AGTPVehicle& Car, FString& Why)
{
	TArray<FGTPMassTrafficCar> Cars;
	if (!GTPMassTrafficCars::Find(World, Car.GetActorLocation(), 30000.f, Cars))
	{
		Why = TEXT("no MassTraffic car to drive into: -Traffic=mass is off or MassTraffic is not installed");
		return -1;
	}
	const FVector From = Car.GetActorLocation();
	// A stopped car first (at its light or in a queue: still there when the player's car arrives), then the nearest.
	Cars.Sort([&From](const FGTPMassTrafficCar& A, const FGTPMassTrafficCar& B)
	{
		const bool bStoppedA = A.SpeedCmS < 150.f;
		const bool bStoppedB = B.SpeedCmS < 150.f;
		if (bStoppedA != bStoppedB)
		{
			return bStoppedA;
		}
		return FVector::DistSquared(A.Location, From) < FVector::DistSquared(B.Location, From);
	});
	int32 Stopped = 0;
	for (const FGTPMassTrafficCar& Each : Cars)
	{
		Stopped += Each.SpeedCmS < 150.f ? 1 : 0;
	}
	int32 Tested = 0;
	int32 TriedBefore = 0;
	int32 CarOnRunUp = 0;
	int32 NoGround = 0;
	int32 InTheWay = 0;
	FString FirstInTheWay = TEXT("none");
	const FCollisionQueryParams Params = GTPGtaRunCrash::IgnoringCar(Car);
	for (const FGTPMassTrafficCar& Candidate : Cars)
	{
		if (Tested >= 40)
		{
			break;
		}
		if (Crash.Tried.Contains(Candidate.Id))
		{
			++TriedBefore;
			continue;
		}
		++Tested;
		FVector Forward;
		FVector Right;
		GTPGtaRunCrash::Axes(Candidate.Forward, Forward, Right);
		// No other traffic car on the run-up: the player's car must come to this one first.
		bool bCarOnRunUp = false;
		for (const FGTPMassTrafficCar& Other : Cars)
		{
			const FVector Rel = Other.Location - Candidate.Location;
			const double Along = FVector::DotProduct(Rel, Forward);
			if (Other.Id != Candidate.Id && FMath::Abs(FVector::DotProduct(Rel, Right)) < 250.0 && Along < 0.0 && Along > -(CrashRunUpCm + 400.0)
				&& FMath::Abs(Rel.Z) < 300.0)
			{
				bCarOnRunUp = true;
				break;
			}
		}
		if (bCarOnRunUp)
		{
			++CarOnRunUp;
			continue;
		}
		FVector Spot;
		if (!GTPGtaRunCrash::SpotBehind(World, Car, Candidate.Location, Forward, Spot))
		{
			++NoGround;
			continue;
		}
		// Nothing standing on the run-up (a wall where the lane bends, a post, a person), up to the car's bumper.
		const FVector Lift(0.0, 0.0, 80.0);
		FHitResult Hit;
		bool bInTheWay = World.LineTraceSingleByChannel(Hit, Spot + Lift, Candidate.Location - Forward * (Candidate.RadiusCm + 50.f) + Lift, ECC_Visibility, Params);
		if (bInTheWay)
		{
			// The target itself when MassTraffic shows it with a body (-MassTrafficBodies): its rear is not in the way.
			const AGTPVehicle* HitCar = Cast<AGTPVehicle>(Hit.GetActor());
			bInTheWay = !(HitCar && HitCar->IsShownByMassTraffic() && FVector::Dist2D(HitCar->GetActorLocation(), Candidate.Location) < 300.0);
		}
		if (bInTheWay)
		{
			if (InTheWay == 0)
			{
				FirstInTheWay = GetNameSafe(Hit.GetActor());
			}
			++InTheWay;
			continue;
		}
		++Crash.Attempts;
		Crash.Tried.Add(Candidate.Id);
		Crash.TargetId = Candidate.Id;
		Crash.TargetAt = Candidate.Location;
		Crash.TargetForward = Candidate.Forward;
		Crash.TargetRadius = FMath::Max(Candidate.RadiusCm, 150.f);
		Crash.TargetKmh = Candidate.SpeedCmS * 0.036f;
		const FBox Local = Car.CalculateComponentsBoundingBoxInLocalSpace();
		Crash.OwnHalfLength = Local.IsValid ? FMath::Clamp(static_cast<float>(Local.GetExtent().X), 180.f, 320.f) : 230.f;
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun crash: attempt %d/3, MassTraffic car %llu at %s, %.0f km/h, radius %.0f cm, %.0f m from the player's car (own half length %.0f cm); %d car(s) within 300 m, %d stopped; set aside: %d with a car on the run-up, %d with no ground behind, %d with something in the way (first: %s)"),
			Crash.Attempts, Crash.TargetId, *Candidate.Location.ToCompactString(), Crash.TargetKmh, Candidate.RadiusCm, FVector::Dist(Candidate.Location, From) / 100.0,
			Crash.OwnHalfLength, Cars.Num(), Stopped, CarOnRunUp, NoGround, InTheWay, *FirstInTheWay);
		return 1;
	}
	Why = FString::Printf(TEXT("no MassTraffic car to drive into for now: %d within 300 m (%d stopped, %d tried already), %d with a car on the run-up, %d with no ground behind, %d with something in the way (first: %s)"),
		Cars.Num(), Stopped, TriedBefore, CarOnRunUp, NoGround, InTheWay, *FirstInTheWay);
	return 0;
}

UGTPGtaRun::EResult UGTPGtaRun::CheckCrash(FString& Detail)
{
	// The player's car into a traffic car, from behind at full throttle: what stops it (docs/gameplay/traffic-mass.md).
	// MassTraffic's cars are drawn as instances with no body: until the near ones have one, the player's car goes
	// through them and this step says so, on the car the player sees (its Mass transform).
	UWorld* World = GetWorld();
	APlayerController* PC = FirstController(World);
	if (!World || !PC || !PC->GetPawn())
	{
		return EResult::Pending;
	}
	const double Now = FPlatformTime::Seconds();
	if (Crash.Phase == 0)
	{
		if (AGTPVehicle* Driven = Cast<AGTPVehicle>(PC->GetPawn()))
		{
			Crash.Car = Driven;
			Crash.Phase = 2;
			Crash.PhaseStart = Now;
			return EResult::Pending;
		}
		ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
		if (!Character)
		{
			Detail = TEXT("the player is neither on foot nor at a wheel");
			return EResult::Pending;
		}
		AGTPTrafficNetwork* Network = nullptr;
		{
			TActorIterator<AGTPTrafficNetwork> It(World);
			Network = It ? *It : nullptr;
		}
		// An unlocked car with no alarm at a kerb (the parked step tests the break-in): 30 m around the player, then 300 m.
		AGTPVehicle* Car = nullptr;
		const float Radii[] = { 3000.f, 30000.f };
		for (const float Radius : Radii)
		{
			AGTPVehicle* Candidate = Network ? Network->SpawnStreetParkedNear(Character->GetActorLocation(), Radius, false, false) : nullptr;
			if (Candidate && UGTPTheftDemoLibrary::PlaceAtDriverDoor(Character, Candidate))
			{
				Car = Candidate;
				break;
			}
		}
		if (!Car)
		{
			Detail = Network ? TEXT("no parked car with a free door to drive") : TEXT("no traffic network to park a car");
			return EResult::Fail;
		}
		if (!UGTPTheftDemoLibrary::PressEnter(World))
		{
			Detail = FString::Printf(TEXT("could not start the entry into %s"), *Car->GetName());
			return EResult::Fail;
		}
		Crash.Car = Car;
		Crash.Character = Character;
		Crash.Phase = 1;
		Crash.PhaseStart = Now;
		Crash.EnterPresses = 1;
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun crash: getting into %s to drive into a traffic car"), *Car->GetName());
		Detail = TEXT("getting in");
		return EResult::Pending;
	}
	if (Crash.Phase == 1)
	{
		// At the wheel of a car: the one the enter key chose is the one to drive (gate 57 timed out here for 75 s,
		// waiting on the car it had placed while nothing said which car the key took, if any).
		if (AGTPVehicle* Driven = Cast<AGTPVehicle>(PC->GetPawn()))
		{
			if (Driven != Crash.Car.Get())
			{
				GTP_TRACE(LogGTPGtaRun, TEXT("GTARun crash: the enter key took %s, not %s: driving it"), *Driven->GetName(),
					*GetNameSafe(Crash.Car.Get()));
				Crash.Car = Driven;
			}
			Crash.Phase = 2;
			Crash.PhaseStart = Now;
		}
		else
		{
			AGTPVehicle* Target = Crash.Car.Get();
			ACharacter* Walker = Cast<ACharacter>(PC->GetPawn());
			if (!Target || !Walker)
			{
				Detail = !Target ? TEXT("the parked car to drive is gone while the player got in") : TEXT("the player is neither on foot nor at a wheel");
				return EResult::Fail;
			}
			const EGTPVehicleState TargetState = Target->GetVehicleState();
			// An entry under way (this car or another one the key chose) freezes the player's move input until it ends.
			const bool bUnderWay = TargetState == EGTPVehicleState::Entering || PC->IsMoveInputIgnored();
			if (Now - Crash.PhaseStart < (bUnderWay ? 12.0 : 4.0))
			{
				Detail = TEXT("getting in");
				return EResult::Pending;
			}
			// Still on foot 4 s after the key with no entry under way (12 s with one): what holds the player, then the key
			// again (a player presses it twice), three times at most.
			const UGTPVehicleInteractionComponent* Interaction = PC->FindComponentByClass<UGTPVehicleInteractionComponent>();
			const AGTPVehicle* Best = Interaction ? Interaction->FindBestVehicle() : nullptr;
			const double DoorCm = FMath::Min(FVector::Dist2D(Walker->GetActorLocation(), Target->GetDoorLocation(EGTPVehicleSide::Left)),
				FVector::Dist2D(Walker->GetActorLocation(), Target->GetDoorLocation(EGTPVehicleSide::Right)));
			const FString Why = FString::Printf(
				TEXT("still on foot %.0f s after the enter key (press %d): %s %s, can enter=%d, %.1f km/h, asleep=%d, shown by MassTraffic=%d, ")
				TEXT("driver %s, %.0f cm from a door, car the key would take %s, paused=%d, move input ignored=%d"),
				Now - Crash.PhaseStart, Crash.EnterPresses, *Target->GetName(), *StaticEnum<EGTPVehicleState>()->GetNameStringByValue(static_cast<int64>(TargetState)),
				Target->CanEnter(Walker) ? 1 : 0, Target->GetSpeedKmh(), Target->IsParkedAsleep() ? 1 : 0, Target->IsShownByMassTraffic() ? 1 : 0,
				*GetNameSafe(Target->GetDriver()), DoorCm, *GetNameSafe(Best), PC->IsPaused() ? 1 : 0, PC->IsMoveInputIgnored() ? 1 : 0);
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun crash: %s"), *Why);
			if (Crash.EnterPresses >= 3)
			{
				Detail = Why;
				return EResult::Fail;
			}
			UGTPTheftDemoLibrary::PlaceAtDriverDoor(Walker, Target);
			UGTPTheftDemoLibrary::PressEnter(World);
			++Crash.EnterPresses;
			Crash.PhaseStart = Now;
			Detail = TEXT("getting in");
			return EResult::Pending;
		}
	}
	AGTPVehicle* Car = Crash.Car.Get();
	if (!Car)
	{
		Detail = TEXT("the player's car is gone");
		return EResult::Fail;
	}
	if (Crash.Phase == 2)
	{
		FString Why;
		const int32 Chosen = ChooseCrashTarget(*World, *Car, Why);
		if (Chosen <= 0)
		{
			// None for now: lights change and queues form, 15 s to find one.
			Detail = Why;
			return Chosen < 0 || Now - Crash.PhaseStart > 15.0 ? EResult::Fail : EResult::Pending;
		}
		// Physics off while the car is moved and held behind its target: the solver undoes the teleport of a simulated
		// car (GTPVehicle's launch test). TickCrash gives it back after half a second.
		if (USkeletalMeshComponent* Body = Car->GetMesh())
		{
			Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			Body->SetSimulatePhysics(false);
		}
		Car->SetScriptedDriveInput(0.f, 0.f, 0.f, false);
		Crash.Phase = 3;
		Crash.HoldUntil = Now + 0.5;
		Crash.LastSeen = Now;
		Crash.Verdict = 0;
		Crash.ContactAt = -1.0;
		Crash.SlowSince = -1.0;
		Crash.PeakKmh = 0.f;
		Crash.ClosestCm = 1.0e9f;
		Crash.MaxAsideCm = 0.f;
		Crash.PrevAlong = -1.0e9f;
		FTSTicker::GetCoreTicker().RemoveTicker(CrashHandle);
		CrashHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPGtaRun::TickCrash), 0.f);
		Detail = TEXT("placing");
		return EResult::Pending;
	}
	const float Contact = Crash.TargetRadius + Crash.OwnHalfLength;
	if (Crash.Verdict == 1)
	{
		Detail = FString::Printf(TEXT("held behind MassTraffic car %llu: hit at %.0f km/h (best %.0f), down to %.0f km/h, centres %.1f m apart at the closest (contact from about %.1f m), %.1f m aside at most; the car stood at %.0f km/h (%s)"),
			Crash.TargetId, Crash.ContactKmh, Crash.PeakKmh, Crash.MinKmhAfter, Crash.ClosestCm / 100.f, Contact / 100.f, Crash.MaxAsideCm / 100.f, Crash.TargetKmh, *Crash.Note);
		return EResult::Ok;
	}
	if (Crash.Verdict == 2)
	{
		Detail = FString::Printf(TEXT("went through MassTraffic car %llu: hit at %.0f km/h, still %.0f km/h with the centres %.1f m apart along its axis and %.1f m aside (contact from about %.1f m); the car stood at %.0f km/h; the traffic cars near the player have no body"),
			Crash.TargetId, Crash.ContactKmh, Crash.EndKmh, Crash.EndAlong / 100.f, Crash.EndAside / 100.f, Contact / 100.f, Crash.TargetKmh);
		return EResult::Fail;
	}
	if (Crash.Verdict == 3)
	{
		EndCrashDrive();
		if (Crash.Attempts < 3)
		{
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun crash: attempt %d/3 left: %s; another car"), Crash.Attempts, *Crash.Note);
			Crash.Phase = 2;
			Crash.PhaseStart = Now;
			Crash.Verdict = 0;
			Detail = Crash.Note;
			return EResult::Pending;
		}
		Detail = FString::Printf(TEXT("three cars and no hit (last: %s)"), *Crash.Note);
		return EResult::Fail;
	}
	Detail = FString::Printf(TEXT("attempt %d/3, phase %d, best %.0f km/h, centres %.1f m apart at the closest"), Crash.Attempts, Crash.Phase, Crash.PeakKmh,
		Crash.ClosestCm < 1.0e8f ? Crash.ClosestCm / 100.f : -1.f);
	return EResult::Pending;
}

bool UGTPGtaRun::TickCrash(float /*DeltaTime*/)
{
	UWorld* World = GetWorld();
	AGTPVehicle* Car = Crash.Car.Get();
	if (!World || !Car || Crash.Verdict != 0 || Crash.Phase < 3)
	{
		return true;
	}
	const double Now = FPlatformTime::Seconds();
	// Where the target is now (it may pull away when its light turns green).
	TArray<FGTPMassTrafficCar> Near;
	GTPMassTrafficCars::Find(*World, Crash.TargetAt, 2500.f, Near);
	const uint64 Id = Crash.TargetId;
	if (const FGTPMassTrafficCar* Target = Near.FindByPredicate([Id](const FGTPMassTrafficCar& Each) { return Each.Id == Id; }))
	{
		Crash.TargetAt = Target->Location;
		Crash.TargetForward = Target->Forward;
		Crash.TargetKmh = Target->SpeedCmS * 0.036f;
		Crash.LastSeen = Now;
	}
	else if (Now - Crash.LastSeen > 1.0)
	{
		Crash.Verdict = 3;
		Crash.Note = TEXT("the car was not found again (recycled, or 25 m away in a frame)");
		return true;
	}
	FVector Forward;
	FVector Right;
	GTPGtaRunCrash::Axes(Crash.TargetForward, Forward, Right);
	if (Crash.Phase == 3)
	{
		// Held behind the target, on its axis, wherever it goes, then given back to the solver.
		FVector Spot;
		if (!GTPGtaRunCrash::SpotBehind(*World, *Car, Crash.TargetAt, Forward, Spot))
		{
			Crash.Verdict = 3;
			Crash.Note = TEXT("no ground behind the car any more");
			return true;
		}
		Car->SetActorLocationAndRotation(Spot, Forward.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);
		if (Now < Crash.HoldUntil)
		{
			return true;
		}
		if (USkeletalMeshComponent* Body = Car->GetMesh())
		{
			Body->SetSimulatePhysics(true);
			Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
			Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			Body->WakeAllRigidBodies();
		}
		Car->NotePlaced(TEXT("GTARun crash"));
		Crash.Phase = 4;
		Crash.DriveStart = Now;
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun crash: the player's car put %.0f m behind MassTraffic car %llu at %s, physics back on, full throttle at it"),
			CrashRunUpCm / 100.f, Crash.TargetId, *Spot.ToCompactString());
		return true;
	}
	// Full throttle, steering at the target's centre (the run-up is on its axis: the steering only keeps it there).
	const FVector ToTarget = Crash.TargetAt - Car->GetActorLocation();
	const double AheadCm = FVector::DotProduct(ToTarget, Car->GetActorForwardVector());
	const double RightCm = FVector::DotProduct(ToTarget, Car->GetActorRightVector());
	const float Steering = FMath::Clamp(static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(RightCm, AheadCm))) / 25.f, -1.f, 1.f);
	Car->SetScriptedDriveInput(1.f, 0.f, Steering, false);
	// The player's car in the target's frame: along its axis (negative behind it) and aside.
	const FVector Rel = -ToTarget;
	const float Along = static_cast<float>(FVector::DotProduct(Rel, Forward));
	const float Aside = static_cast<float>(FVector::DotProduct(Rel, Right));
	const float Centres = static_cast<float>(FVector(Rel.X, Rel.Y, 0.0).Size());
	const float Kmh = Car->GetSpeedKmh();
	Crash.PeakKmh = FMath::Max(Crash.PeakKmh, Kmh);
	Crash.ClosestCm = FMath::Min(Crash.ClosestCm, Centres);
	const float Contact = Crash.TargetRadius + Crash.OwnHalfLength;
	// Through: in line with it, the centres nearer than half the shorter car along its axis, or the player's car went from
	// behind it to in front of it between two frames. Two bodies stop about Contact apart.
	const bool bInLine = FMath::Abs(Aside) < 150.f;
	const bool bThrough = bInLine && (FMath::Abs(Along) < 0.5f * FMath::Min(Crash.TargetRadius, Crash.OwnHalfLength) || (Crash.PrevAlong < 0.f && Along >= 0.f));
	Crash.PrevAlong = Along;
	if (Crash.ContactAt < 0.0 && FMath::Abs(Aside) < 250.f && Along > -(Contact + 150.f) && Along < 0.f)
	{
		Crash.ContactAt = Now;
		Crash.ContactKmh = Kmh;
		Crash.MinKmhAfter = Kmh;
	}
	if (Crash.ContactAt >= 0.0)
	{
		Crash.MinKmhAfter = FMath::Min(Crash.MinKmhAfter, Kmh);
		Crash.MaxAsideCm = FMath::Max(Crash.MaxAsideCm, FMath::Abs(Aside));
	}
	// Stopped short of the contact range (a body longer than MassTraffic's radius says): 1.5 s at a crawl near it.
	const bool bSlowNear = Now - Crash.DriveStart > 2.0 && Kmh < 3.f && Centres < Contact + 400.f;
	Crash.SlowSince = bSlowNear ? (Crash.SlowSince < 0.0 ? Now : Crash.SlowSince) : -1.0;
	int32 Verdict = 0;
	const TCHAR* Note = TEXT("");
	if (bThrough)
	{
		Verdict = 2;
		Note = TEXT("went through");
	}
	else if (Crash.ContactAt >= 0.0 && Now - Crash.ContactAt > 2.5)
	{
		Verdict = 1;
		Note = Crash.MaxAsideCm >= 150.f ? TEXT("pushed aside") : Crash.MinKmhAfter < 5.f ? TEXT("stopped") : TEXT("pushing it");
	}
	else if (Crash.SlowSince >= 0.0 && Now - Crash.SlowSince > 1.5)
	{
		Verdict = 1;
		Note = TEXT("stopped short of the contact range");
	}
	else if (Crash.ContactAt < 0.0 && (Now - Crash.DriveStart > 12.0 || Centres > 4000.f))
	{
		Verdict = 3;
		Note = Centres > 4000.f ? TEXT("never reached it: the car drove away") : TEXT("never reached it in 12 s");
	}
	if (Verdict != 0)
	{
		Crash.Verdict = Verdict;
		Crash.Note = Note;
		Crash.EndAlong = Along;
		Crash.EndAside = Aside;
		Crash.EndKmh = Kmh;
		// Measured: off the throttle now, not at the step's next half-second tick.
		Car->SetScriptedDriveInput(0.f, 1.f, 0.f, false);
		if (Verdict != 3)
		{
			// What the player sees at that moment: his car in the traffic car, or against it.
			const FString Shot = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ScreenShotDir(), TEXT("gtarun-crash.png")));
			FScreenshotRequest::RequestScreenshot(Shot, false, false);
			GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: picture of the crash asked: %s (%s, %.0f km/h, %.1f m along the car's axis, %.1f m aside)"), *Shot, Note, Kmh, Along / 100.f, Aside / 100.f);
		}
	}
	return true;
}

void UGTPGtaRun::EndCrashDrive()
{
	FTSTicker::GetCoreTicker().RemoveTicker(CrashHandle);
	if (AGTPVehicle* Car = Crash.Car.Get())
	{
		if (Crash.Phase == 3)
		{
			// Ended while held: the physics back.
			if (USkeletalMeshComponent* Body = Car->GetMesh())
			{
				Body->SetSimulatePhysics(true);
				Body->WakeAllRigidBodies();
			}
		}
		Car->SetScriptedDriveInput(0.f, 1.f, 0.f, false);
	}
}
