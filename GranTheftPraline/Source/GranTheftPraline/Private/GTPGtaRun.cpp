#include "GTPGtaRun.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GTPLog.h"
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
	constexpr int32 StepCount = 6;
	const TCHAR* StepNames[StepCount] = { TEXT("carjack"), TEXT("chase"), TEXT("parked"), TEXT("aim"), TEXT("aimclose"), TEXT("shoot") };
	// Time limits (s) per step, counted from the step's start.
	const double StepLimits[StepCount] = { 90.0, 170.0, 200.0, 70.0, 70.0, 30.0 };

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
	GTP_TRACE(LogGTPGtaRun, TEXT("GTARun: on (%d steps)"), StepCount);
}

void UGTPGtaRun::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
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
	case 3: Result = CheckAim(Detail, false); break;
	case 4: Result = CheckAim(Detail, true); break;
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

UGTPGtaRun::EResult UGTPGtaRun::CheckAim(FString& Detail, bool bClose)
{
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
	Weapon->SetDrawn(true);
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
		Character->AddMovementInput(ToVictim, 1.f);
	}
	else if (Best < BackCm)
	{
		Character->AddMovementInput(-ToVictim, 1.f);
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
	Detail = FString::Printf(TEXT("%saiming at %s (%.0f cm): %d hands up (%d compliant or frozen), %d fleeing, %d alarmed%s"),
		bClose ? TEXT("close, ") : TEXT(""), *Victim->GetName(), Best, Hands, Obedient, Fleeing, Alarmed, bSettled ? TEXT(", settled") : TEXT(""));
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
