#include "GTPCarjackVictim.h"

#include "GTPHealth.h"
#include "GTPLog.h"
#include "GTPVehicle.h"
#include "AIController.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPCarjack, Log, All);

UGTPCarjackVictimComponent::UGTPCarjackVictimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

UGTPCarjackVictimComponent* UGTPCarjackVictimComponent::Start(ACharacter* InVictim, AGTPVehicle* InCar, ACharacter* InThief)
{
	if (!InVictim)
	{
		return nullptr;
	}
	UGTPCarjackVictimComponent* Component = NewObject<UGTPCarjackVictimComponent>(InVictim);
	Component->Car = InCar;
	Component->Thief = InThief;
	Component->Settings.ResistShare = Component->ResistShare;
	Component->Settings.CowerShare = Component->CowerShare;
	Component->Victim.Mood = GTPCarjack::PickMood(FMath::FRand(), Component->Settings);
	FString Forced;
	if (FParse::Value(FCommandLine::Get(), TEXT("CarjackMood="), Forced))
	{
		Component->Victim.Mood = Forced == TEXT("resist") ? GTPCarjack::EMood::Resist
			: Forced == TEXT("cower") ? GTPCarjack::EMood::Cower : GTPCarjack::EMood::Flee;
	}
	if (const UGTPHealthComponent* Health = UGTPHealthComponent::Find(InVictim))
	{
		Component->LastHealth = Health->GetHealth();
	}
	Component->RegisterComponent();
	GTP_TRACE(LogGTPCarjack, TEXT("carjack: %s pulled out of %s, %s"), *InVictim->GetName(), *GetNameSafe(InCar),
		UTF8_TO_TCHAR(GTPCarjack::MoodName(Component->Victim.Mood)));
	return Component;
}

FVector UGTPCarjackVictimComponent::TargetLocation() const
{
	const AGTPVehicle* Stolen = Car.Get();
	const ACharacter* Player = Thief.Get();
	// The thief inside: the driver door. On foot: the thief.
	if (Stolen && Stolen->GetDriver() == Player)
	{
		return Stolen->GetDoorLocation(EGTPVehicleSide::Left);
	}
	return Player ? Player->GetActorLocation() : (Stolen ? Stolen->GetActorLocation() : FVector::ZeroVector);
}

void UGTPCarjackVictimComponent::BeginFlee()
{
	ACharacter* Body = Cast<ACharacter>(GetOwner());
	if (!Body)
	{
		return;
	}
	Body->UnCrouch();
	if (UCharacterMovementComponent* Move = Body->GetCharacterMovement())
	{
		Move->MaxWalkSpeed = RunSpeed;
	}
	const FVector From = Car.IsValid() ? Car->GetActorLocation() : Body->GetActorLocation();
	FVector Away = (Body->GetActorLocation() - From).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = Body->GetActorForwardVector();
	}
	if (AAIController* AI = Cast<AAIController>(Body->GetController()))
	{
		// No navigation mesh needed: straight away from the car.
		AI->MoveToLocation(Body->GetActorLocation() + Away * FleeDistance, 50.f, false, false);
	}
}

void UGTPCarjackVictimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Body = Cast<ACharacter>(GetOwner());
	if (!Body)
	{
		DestroyComponent();
		return;
	}
	if (Victim.Phase == GTPCarjack::EPhase::Flee)
	{
		FleeTime += DeltaTime;
		if (FleeTime > 6.f)
		{
			DestroyComponent();
		}
		return;
	}

	GTPCarjack::FVictimInput In;
	In.Dt = DeltaTime;
	const FVector Target = TargetLocation();
	In.TargetDistance = static_cast<float>(FVector::Dist2D(Body->GetActorLocation(), Target));
	In.CarSpeedKmh = Car.IsValid() ? FMath::Abs(Car->GetSpeedKmh()) : 0.f;
	if (const UGTPHealthComponent* Health = UGTPHealthComponent::Find(Body))
	{
		In.bHurt = LastHealth >= 0.f && Health->GetHealth() < LastHealth - 0.5f;
		LastHealth = Health->GetHealth();
	}

	const GTPCarjack::EPhase Before = Victim.Phase;
	const GTPCarjack::FVictimOutput Out = GTPCarjack::StepVictim(Victim, In, Settings);
	if (Victim.Phase == GTPCarjack::EPhase::Cower && Before != GTPCarjack::EPhase::Cower)
	{
		if (UCharacterMovementComponent* Move = Body->GetCharacterMovement())
		{
			Move->GetNavAgentPropertiesRef().bCanCrouch = true;
		}
		Body->Crouch();
	}
	if (Out.bStartFleeing)
	{
		GTP_TRACE(LogGTPCarjack, TEXT("carjack: %s runs away (%d strikes)"), *Body->GetName(), Victim.Strikes);
		BeginFlee();
		return;
	}
	AAIController* AI = Cast<AAIController>(Body->GetController());
	MoveOrderAge += DeltaTime;
	if (Out.bMoveToTarget && AI && MoveOrderAge > 0.4f)
	{
		MoveOrderAge = 0.f;
		if (UCharacterMovementComponent* Move = Body->GetCharacterMovement())
		{
			Move->MaxWalkSpeed = RunSpeed;
		}
		AI->MoveToLocation(Target, 60.f, false, false);
	}
	if (Out.bStrike)
	{
		if (AI)
		{
			AI->StopMovement();
		}
		Body->SetActorRotation(FRotator(0.f, (Target - Body->GetActorLocation()).GetSafeNormal2D().Rotation().Yaw, 0.f));
		AGTPVehicle* Stolen = Car.Get();
		ACharacter* Player = Thief.Get();
		if (Stolen && Stolen->GetDriver() == Player)
		{
			// Bangs on the driver door: a dent and the car rocks.
			const FVector Dir = (Stolen->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
			FHitResult Hit;
			Hit.ImpactPoint = Target;
			UGameplayStatics::ApplyPointDamage(Stolen, DoorBangDamage, Dir, Hit, AI, Body, nullptr);
			if (UPrimitiveComponent* CarBody = Cast<UPrimitiveComponent>(Stolen->GetRootComponent()))
			{
				CarBody->AddImpulseAtLocation(Dir * 25000.f, Target + FVector(0.f, 0.f, 80.f));
			}
		}
		else if (Player)
		{
			// A punch: damage and a shove.
			const FVector Dir = (Player->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
			if (UGTPHealthComponent* Health = UGTPHealthComponent::Find(Player))
			{
				Health->ApplyDamage(PunchDamage, EGTPDamageCause::Melee, Body, true);
			}
			else
			{
				UGameplayStatics::ApplyDamage(Player, PunchDamage, AI, Body, nullptr);
			}
			Player->LaunchCharacter(Dir * 320.f + FVector(0.f, 0.f, 80.f), false, false);
		}
		GTP_TRACE(LogGTPCarjack, TEXT("carjack: %s strikes %s (%d)"), *Body->GetName(),
			Stolen && Stolen->GetDriver() == Player ? TEXT("the door") : TEXT("the thief"), Victim.Strikes);
	}
}
