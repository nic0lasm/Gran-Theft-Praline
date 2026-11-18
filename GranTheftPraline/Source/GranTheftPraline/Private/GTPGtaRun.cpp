#include "GTPGtaRun.h"

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
	constexpr int32 StepCount = 7;
	const TCHAR* StepNames[StepCount] = { TEXT("carjack"), TEXT("chase"), TEXT("parked"), TEXT("walkarmed"), TEXT("aim"), TEXT("aimclose"), TEXT("shoot") };
	// Time limits (s) per step, counted from the step's start.
	// Chase: 3 stars come at 30 s and a car that flees has its 150 m by then; a chase that fails must leave the
	// other steps their time (170 s of it ate the bench's 330 s on 16/11).
	const double StepLimits[StepCount] = { 90.0, 90.0, 200.0, 40.0, 90.0, 90.0, 30.0 };

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
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun step %d: %s start (t=%.0f s)"), Step + 1, StepNames[Step], FPlatformTime::Seconds() - RunStart);
}

void UGTPGtaRun::EndStep(EResult Result, const FString& Detail)
{
	const bool bOk = Result == EResult::Ok;
	OkCount += bOk ? 1 : 0;
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun step %d: %s %s %s (%.0f s)"), Step + 1, StepNames[Step], bOk ? TEXT("ok") : TEXT("fail"), *Detail,
		FPlatformTime::Seconds() - StepStart);
	if (Step + 1 < StepCount)
	{
		BeginStep(Step + 1);
		return;
	}
	bDone = true;
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun done: %d/%d ok (%.0f s)"), OkCount, StepCount, FPlatformTime::Seconds() - RunStart);
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
	case 0: Result = CheckCarjack(Detail); break;
	case 1: Result = CheckChase(Detail); break;
	case 2: Result = CheckParked(Detail); break;
	case 3: Result = CheckWalkArmed(Detail); break;
	case 4: Result = CheckAim(Detail, false); break;
	case 5: Result = CheckAim(Detail, true); break;
	default: Result = CheckShoot(Detail); break;
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
	// A chase is a flight: the metres the player's car really drove (a jump of more than 20 m between two
	// frames is the demo putting the car on another lane, not driving) and its best speed. At door 41 the
	// step passed on stars and units with the car standing still in a dead end from its 8th second.
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
	Detail = FString::Printf(TEXT("best stars=%d units=%d driven=%.0f m peak=%.0f km/h"), MaxStars, MaxUnits, ChaseDrivenCm / 100.0, ChasePeakKmh);
	return (MaxStars >= 3 && MaxUnits >= 3 && ChaseDrivenCm >= ChaseDrivenWantedCm) ? EResult::Ok : EResult::Pending;
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
		AGTPVehicle* Car = nullptr;
		for (int32 Attempt = 0; Attempt < 3 && !Car; ++Attempt)
		{
			AGTPVehicle* Candidate = UGTPTheftDemoLibrary::PlaceAlarmCar(World, Character->GetActorLocation(), 3000.f);
			if (!Candidate)
			{
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
