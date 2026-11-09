#include "GTPDemoRoute.h"

#include "ChaosVehicleMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GTPPlayerController.h"
#include "GTPVehicle.h"
#include "GTPVehicleInteractionComponent.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPDemo, Log, All);

namespace
{
	// Durations in seconds, speeds in km/h.
	constexpr double LookAroundTime = 5.0;
	constexpr double WalkStreetTime = 14.0;
	constexpr double TurnBackTime = 2.0;
	constexpr double WalkToCarMaxTime = 40.0;
	constexpr double EnterMaxTime = 8.0;
	constexpr double DriveTime = 45.0;
	constexpr double StopMaxTime = 8.0;
	constexpr double ExitMaxTime = 6.0;
	constexpr float CruiseKmh = 40.f;
	constexpr float DoorReachCm = 300.f;

	const TCHAR* PhaseName(int32 Phase)
	{
		static const TCHAR* Names[] = { TEXT("wait for pawn"), TEXT("look around"), TEXT("walk street"), TEXT("turn back"),
			TEXT("walk to car"), TEXT("enter"), TEXT("drive"), TEXT("stop"), TEXT("exit"), TEXT("done") };
		return (Phase >= 0 && Phase < static_cast<int32>(UE_ARRAY_COUNT(Names))) ? Names[Phase] : TEXT("?");
	}
}

void UGTPDemoRoute::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!FParse::Param(FCommandLine::Get(), TEXT("DemoRoute")))
	{
		return;
	}
	RouteStart = FPlatformTime::Seconds();
	PhaseStart = RouteStart;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UGTPDemoRoute::Tick));
	UE_LOG(LogGTPDemo, Log, TEXT("Demo route enabled"));
}

void UGTPDemoRoute::Deinitialize()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	Super::Deinitialize();
}

APlayerController* UGTPDemoRoute::GetController() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr;
}

void UGTPDemoRoute::SetPhase(EPhase NewPhase)
{
	Phase = NewPhase;
	PhaseStart = FPlatformTime::Seconds();
	UE_LOG(LogGTPDemo, Log, TEXT("Demo route %.1f s: %s"), static_cast<float>(PhaseStart - RouteStart), PhaseName(static_cast<int32>(NewPhase)));
}

AGTPVehicle* UGTPDemoRoute::FindCar() const
{
	const APlayerController* PC = GetController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!Pawn || !World)
	{
		return nullptr;
	}
	AGTPVehicle* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();
	for (TActorIterator<AGTPVehicle> It(World); It; ++It)
	{
		if (It->IsWrecked() || It->GetDriver() != nullptr)
		{
			continue;
		}
		const float Dist = FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = *It;
		}
	}
	return Best;
}

void UGTPDemoRoute::DriveInputs(AGTPVehicle* InCar, float Throttle, float Brake, float Steering, bool bHandbrake) const
{
	if (UChaosVehicleMovementComponent* Move = InCar ? InCar->GetVehicleMovementComponent() : nullptr)
	{
		Move->SetThrottleInput(Throttle);
		Move->SetBrakeInput(Brake);
		Move->SetSteeringInput(Steering);
		Move->SetHandbrakeInput(bHandbrake);
	}
}

bool UGTPDemoRoute::Tick(float DeltaTime)
{
	APlayerController* PC = GetController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const double Now = FPlatformTime::Seconds();
	const double T = Now - PhaseStart;

	if (!PC || !Pawn)
	{
		return true;
	}

	switch (Phase)
	{
	case EPhase::WaitForPawn:
		if (Cast<ACharacter>(Pawn))
		{
			StreetYaw = Pawn->GetActorRotation().Yaw;
			StartControlYaw = PC->GetControlRotation().Yaw;
			SetPhase(EPhase::LookAround);
		}
		break;

	case EPhase::LookAround:
	{
		// A slow pan left then right, back to the street axis.
		const float Pan = 60.f * static_cast<float>(FMath::Sin(2.0 * UE_DOUBLE_PI * T / LookAroundTime));
		PC->SetControlRotation(FRotator(-8.f, StartControlYaw + Pan, 0.f));
		if (T >= LookAroundTime)
		{
			SetPhase(EPhase::WalkStreet);
		}
		break;
	}

	case EPhase::WalkStreet:
	{
		const float Sway = 12.f * static_cast<float>(FMath::Sin(2.0 * UE_DOUBLE_PI * T / 7.0));
		const FRotator View(-6.f, StreetYaw + Sway, 0.f);
		PC->SetControlRotation(View);
		Pawn->AddMovementInput(FRotator(0.f, StreetYaw, 0.f).Vector(), 1.f);
		if (T >= WalkStreetTime)
		{
			SetPhase(EPhase::TurnBack);
		}
		break;
	}

	case EPhase::TurnBack:
	{
		const float Alpha = FMath::Clamp(static_cast<float>(T / TurnBackTime), 0.f, 1.f);
		PC->SetControlRotation(FRotator(-6.f, StreetYaw + 180.f * FMath::SmoothStep(0.f, 1.f, Alpha), 0.f));
		if (T >= TurnBackTime)
		{
			Car = FindCar();
			if (!Car.IsValid())
			{
				UE_LOG(LogGTPDemo, Warning, TEXT("Demo route: no free car found (start with -TestVehicle); staying on foot"));
				SetPhase(EPhase::Done);
				break;
			}
			SetPhase(EPhase::WalkToCar);
		}
		break;
	}

	case EPhase::WalkToCar:
	{
		AGTPVehicle* Target = Car.Get();
		if (!Target)
		{
			SetPhase(EPhase::Done);
			break;
		}
		const FVector Door = Target->GetDoorLocation(EGTPVehicleSide::Left);
		FVector ToDoor = Door - Pawn->GetActorLocation();
		ToDoor.Z = 0.f;
		const FRotator Desired(-8.f, ToDoor.Rotation().Yaw, 0.f);
		PC->SetControlRotation(FMath::RInterpTo(PC->GetControlRotation(), Desired, DeltaTime, 3.f));
		if (ToDoor.Size() > DoorReachCm * 0.5f)
		{
			Pawn->AddMovementInput(ToDoor.GetSafeNormal(), 1.f);
		}
		const AGTPPlayerController* GTPController = Cast<AGTPPlayerController>(PC);
		UGTPVehicleInteractionComponent* Interaction = GTPController ? GTPController->GetVehicleInteraction() : nullptr;
		if (Interaction && ToDoor.Size() <= DoorReachCm && Interaction->FindBestVehicle() == Target)
		{
			Interaction->HandleEnterExit();
			SetPhase(EPhase::Enter);
		}
		else if (T >= WalkToCarMaxTime)
		{
			UE_LOG(LogGTPDemo, Warning, TEXT("Demo route: could not reach the car in %.0f s; staying on foot"), WalkToCarMaxTime);
			SetPhase(EPhase::Done);
		}
		break;
	}

	case EPhase::Enter:
		if (Pawn == Car.Get())
		{
			StreetYaw = Pawn->GetActorRotation().Yaw;
			SetPhase(EPhase::Drive);
		}
		else if (T >= EnterMaxTime)
		{
			UE_LOG(LogGTPDemo, Warning, TEXT("Demo route: entry did not finish in %.0f s"), EnterMaxTime);
			SetPhase(EPhase::Done);
		}
		break;

	case EPhase::Drive:
	{
		AGTPVehicle* Driven = Cast<AGTPVehicle>(Pawn);
		if (!Driven)
		{
			SetPhase(EPhase::Done);
			break;
		}
		// Hold the street heading with a light steering correction and keep a
		// city speed; a gentle weave in the middle shows the handling.
		const float Weave = (T > 15.0 && T < 30.0) ? 10.f * static_cast<float>(FMath::Sin(2.0 * UE_DOUBLE_PI * (T - 15.0) / 7.5)) : 0.f;
		const float YawError = FRotator::NormalizeAxis(StreetYaw + Weave - Driven->GetActorRotation().Yaw);
		const float Steering = FMath::Clamp(YawError / 25.f, -1.f, 1.f);
		const float Speed = Driven->GetSpeedKmh();
		const float Throttle = Speed < CruiseKmh ? FMath::Clamp((CruiseKmh - Speed) / 15.f, 0.25f, 0.8f) : 0.f;
		DriveInputs(Driven, Throttle, 0.f, Steering, false);
		if (T >= DriveTime)
		{
			SetPhase(EPhase::Stop);
		}
		break;
	}

	case EPhase::Stop:
	{
		AGTPVehicle* Driven = Cast<AGTPVehicle>(Pawn);
		if (!Driven)
		{
			SetPhase(EPhase::Done);
			break;
		}
		DriveInputs(Driven, 0.f, 1.f, 0.f, false);
		if (FMath::Abs(Driven->GetSpeedKmh()) < 2.f || T >= StopMaxTime)
		{
			DriveInputs(Driven, 0.f, 0.f, 0.f, true);
			const AGTPPlayerController* GTPController = Cast<AGTPPlayerController>(PC);
			if (UGTPVehicleInteractionComponent* Interaction = GTPController ? GTPController->GetVehicleInteraction() : nullptr)
			{
				Interaction->HandleEnterExit();
			}
			SetPhase(EPhase::Exit);
		}
		break;
	}

	case EPhase::Exit:
		if (Cast<ACharacter>(Pawn))
		{
			UE_LOG(LogGTPDemo, Log, TEXT("Demo route: back on foot"));
			SetPhase(EPhase::Done);
		}
		else if (T >= ExitMaxTime)
		{
			UE_LOG(LogGTPDemo, Warning, TEXT("Demo route: exit did not finish in %.0f s"), ExitMaxTime);
			SetPhase(EPhase::Done);
		}
		break;

	case EPhase::Done:
		TickHandle.Reset();
		return false;
	}
	return true;
}
