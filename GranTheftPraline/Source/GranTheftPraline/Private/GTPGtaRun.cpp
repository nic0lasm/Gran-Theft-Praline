#include "GTPGtaRun.h"

#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogGTPGtaRun, Log, All);

namespace
{
	constexpr int32 StepCount = 7;
	const TCHAR* StepNames[StepCount] = { TEXT("carjack"), TEXT("chase"), TEXT("parked"), TEXT("walkarmed"), TEXT("aim"), TEXT("aimclose"), TEXT("shoot") };
	// Time limits (s) per step, counted from the step's start.
	const double StepLimits[StepCount] = { 90.0, 170.0, 200.0, 40.0, 90.0, 90.0, 30.0 };

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
						Pawn->AddMovementInput(WalkDirection, 1.f);
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
	Detail = FString::Printf(TEXT("best stars=%d units=%d"), MaxStars, MaxUnits);
	return (MaxStars >= 3 && MaxUnits >= 3) ? EResult::Ok : EResult::Pending;
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
		WalkArmedYaw = PC->GetControlRotation().Yaw;
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
			return EResult::Fail;
		}
		GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: walked %.0f cm in 4 s (%s)"), Moved, Phase);
		if (WalkArmedPhase == 0)
		{
			WalkArmedPhase = 1;
			WalkArmedPhaseStart = -1.0;
			return EResult::Pending;
		}
		// The gun stays drawn: holstering plays a clip, and the next step must not start inside it.
		Weapon->SetAiming(false);
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
		if (LastAimMoveTime < 0.0 || Character->GetVelocity().Size2D() > 20.0)
		{
			LastAimMoveTime = Now2;
		}
		else if (Now2 - LastAimMoveTime > 4.0)
		{
			const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
			Detail = FString::Printf(TEXT("the player does not walk for 4 s (%.0f cm from %s, ignore move input = %d, movement mode %d, max walk speed %.0f, "
				"attached to %s, possessed by %s, movement active %d, collision %d)"), Best, *Victim->GetName(), PC->IsMoveInputIgnored() ? 1 : 0,
				Movement ? static_cast<int32>(Movement->MovementMode.GetValue()) : -1, Movement ? Movement->MaxWalkSpeed : -1.f,
				Character->GetAttachParentActor() ? *Character->GetAttachParentActor()->GetName() : TEXT("nothing"),
				Character->GetController() ? *Character->GetController()->GetName() : TEXT("nobody"),
				Movement && Movement->IsActive() ? 1 : 0, static_cast<int32>(Character->GetActorEnableCollision()));
			Detail += FString::Printf(TEXT(" [weapon drawn %d, aiming %d, root motion %d, anim montage playing %d]"), Weapon->IsDrawn() ? 1 : 0, Weapon->IsAiming() ? 1 : 0,
				Character->HasAnyRootMotion() ? 1 : 0, Character->GetMesh() && Character->GetMesh()->GetAnimInstance() && Character->GetMesh()->GetAnimInstance()->IsAnyMontagePlaying() ? 1 : 0);
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
