#include "GTPGtaRun.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GTPLog.h"
#include "GTPTheftDemo.h"
#include "GTPVehicle.h"
#include "GTPWanted.h"
#include "GTPWeapon.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPGtaRun, Log, All);

namespace
{
	constexpr int32 StepCount = 4;
	const TCHAR* StepNames[StepCount] = { TEXT("carjack"), TEXT("chase"), TEXT("parked"), TEXT("shoot") };
	// Time limits (s) per step, counted from the step's start.
	const double StepLimits[StepCount] = { 90.0, 170.0, 200.0, 30.0 };

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
			Detail = TEXT("player still driving");
			return EResult::Pending;
		}
		// The theft library of the traffic thread: a locked car with an alarm at the kerb, the player at its door, the enter key.
		AGTPVehicle* Car = UGTPTheftDemoLibrary::PlaceAlarmCar(World, Character->GetActorLocation(), 3000.f);
		if (!Car)
		{
			Detail = TEXT("no kerb space for an alarm car");
			return EResult::Fail;
		}
		if (!UGTPTheftDemoLibrary::PlaceAtDriverDoor(Character, Car) || !UGTPTheftDemoLibrary::PressEnter(World))
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
