#include "GTPTrafficIncident.h"

#include "GTPHealth.h"
#include "GTPLog.h"
#include "GTPTraffic.h"
#include "GTPVehicle.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPIncident, Log, All);

UGTPTrafficArgueComponent::UGTPTrafficArgueComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

UGTPTrafficArgueComponent* UGTPTrafficArgueComponent::Start(ACharacter* Driver, AGTPVehicle* InCar, ACharacter* InOther, AGTPTrafficNetwork* InNetwork)
{
	if (!Driver || !InCar)
	{
		return nullptr;
	}
	UGTPTrafficArgueComponent* Component = NewObject<UGTPTrafficArgueComponent>(Driver);
	Component->Car = InCar;
	Component->Other = InOther;
	Component->Network = InNetwork;
	Component->Arguer.ArgueSeconds = FMath::FRandRange(Component->Settings.ArgueMinSeconds, Component->Settings.ArgueMaxSeconds);
	if (const UGTPHealthComponent* Health = UGTPHealthComponent::Find(Driver))
	{
		Component->LastHealth = Health->GetHealth();
	}
	if (InNetwork)
	{
		Component->SeenShotSerial = InNetwork->GetPlayerShotSerial();
	}
	Component->RegisterComponent();
	// Out of the car, at walking pace (no bail out: the car has stopped or is stopping).
	InCar->AIGetOut(false, InCar->GetActorLocation());
	return Component;
}

bool UGTPTrafficArgueComponent::IsThreatened(const ACharacter* Body)
{
	bool bThreat = false;
	if (const UGTPHealthComponent* Health = UGTPHealthComponent::Find(const_cast<ACharacter*>(Body)))
	{
		bThreat = LastHealth >= 0.f && Health->GetHealth() < LastHealth - 0.5f;
		LastHealth = Health->GetHealth();
	}
	const AGTPTrafficNetwork* Net = Network.Get();
	if (!Net)
	{
		return bThreat;
	}
	const FVector Location = Body->GetActorLocation();
	FVector Origin, Aim;
	if (Net->GetPlayerAim(Origin, Aim))
	{
		const FVector ToBody = Location - Origin;
		bThreat |= ToBody.Size() < Net->ReactAimRange
			&& FVector::DotProduct(ToBody.GetSafeNormal(), Aim) > FMath::Cos(FMath::DegreesToRadians(Net->ReactAimHalfAngle * 2.f));
	}
	if (Net->GetPlayerShotSerial() != SeenShotSerial)
	{
		SeenShotSerial = Net->GetPlayerShotSerial();
		bThreat |= FVector::Dist(Net->GetPlayerShotFrom(), Location) < Net->ReactShotRange;
	}
	return bThreat;
}

void UGTPTrafficArgueComponent::BeginFlee(const FVector& From)
{
	ACharacter* Body = Cast<ACharacter>(GetOwner());
	if (!Body)
	{
		return;
	}
	if (UCharacterMovementComponent* Move = Body->GetCharacterMovement())
	{
		Move->MaxWalkSpeed = RunSpeed;
	}
	FVector Away = (Body->GetActorLocation() - From).GetSafeNormal2D();
	if (Away.IsNearlyZero())
	{
		Away = -Body->GetActorForwardVector();
	}
	if (AAIController* AI = Cast<AAIController>(Body->GetController()))
	{
		AI->MoveToLocation(Body->GetActorLocation() + Away * FleeDistance, 50.f, false, false);
	}
	FleeTime = 0.f;
}

void UGTPTrafficArgueComponent::Finish()
{
	SetComponentTickEnabled(false);
	DestroyComponent();
}

void UGTPTrafficArgueComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ACharacter* Body = Cast<ACharacter>(GetOwner());
	if (!Body)
	{
		Finish();
		return;
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (Arguer.Phase == GTPIncident::EPhase::Flee)
	{
		// Gone far and out of sight: taken away, like traffic.
		FleeTime += DeltaTime;
		if (FleeTime > 8.f && Player && FVector::Dist2D(Player->GetActorLocation(), Body->GetActorLocation()) > RemoveDistance
			&& !Body->WasRecentlyRendered(1.f))
		{
			Body->Destroy();
		}
		return;
	}

	AGTPVehicle* MyCar = Car.Get();
	const ACharacter* Opponent = Other.Get();
	const UGTPTrafficArgueComponent* OtherArgue = Opponent ? Opponent->FindComponentByClass<UGTPTrafficArgueComponent>() : nullptr;

	GTPIncident::FArguerInput In;
	In.Dt = DeltaTime;
	In.bOut = !MyCar || MyCar->GetDriver() != Body;
	In.bOtherHere = OtherArgue && !OtherArgue->IsFleeing() && OtherArgue->GetPhase() != GTPIncident::EPhase::Done
		&& OtherArgue->GetPhase() != GTPIncident::EPhase::GetOut && Opponent->GetAttachParentActor() == nullptr;
	In.OtherDistance = Opponent ? static_cast<float>(FVector::Dist2D(Opponent->GetActorLocation(), Body->GetActorLocation())) : 0.f;
	In.CarDistance = MyCar ? static_cast<float>(FVector::Dist2D(MyCar->GetDoorLocation(EGTPVehicleSide::Left), Body->GetActorLocation())) : 0.f;
	// Stolen (someone else at the wheel), wrecked or taken away.
	In.bCarGone = !MyCar || MyCar->IsWrecked() || (MyCar->GetDriver() != nullptr && MyCar->GetDriver() != Body);
	In.bThreat = IsThreatened(Body);
	In.ShoveRoll = FMath::FRand();
	In.ShoveChance = ShoveChance;

	const GTPIncident::EPhase Before = Arguer.Phase;
	const GTPIncident::FArguerOutput Out = GTPIncident::StepArguer(Arguer, In, Settings);
	if (Arguer.Phase != Before)
	{
		GTP_TRACE(LogGTPIncident, TEXT("incident: %s %s -> %s"), *Body->GetName(), UTF8_TO_TCHAR(GTPIncident::PhaseName(Before)),
			UTF8_TO_TCHAR(GTPIncident::PhaseName(Arguer.Phase)));
	}
	AAIController* AI = Cast<AAIController>(Body->GetController());
	if (Out.bStartFleeing)
	{
		const FVector From = In.bThreat && Player ? Player->GetActorLocation() : (MyCar ? MyCar->GetActorLocation() : Body->GetActorLocation());
		BeginFlee(From);
		return;
	}
	if (Out.bReclaim)
	{
		AGTPTrafficNetwork* Net = Network.Get();
		if (AI)
		{
			AI->StopMovement();
		}
		if (!Net || !MyCar || !Net->ReclaimCar(MyCar, Body))
		{
			// No lane to go back to: walks off.
			Arguer.Phase = GTPIncident::EPhase::Flee;
			BeginFlee(MyCar ? MyCar->GetActorLocation() : Body->GetActorLocation());
			return;
		}
		Finish();
		return;
	}
	if (Arguer.Phase == GTPIncident::EPhase::Done)
	{
		// Stayed in the car (boxed in): nothing more to play.
		Finish();
		return;
	}
	MoveOrderAge += DeltaTime;
	UCharacterMovementComponent* Move = Body->GetCharacterMovement();
	if (AI && (Out.bMoveToOther || Out.bMoveToCar) && MoveOrderAge > 0.5f)
	{
		MoveOrderAge = 0.f;
		if (Move)
		{
			Move->MaxWalkSpeed = Out.bMoveToOther ? WalkSpeed * 1.4f : WalkSpeed;
		}
		const FVector Target = Out.bMoveToOther && Opponent ? Opponent->GetActorLocation()
			: (MyCar ? MyCar->GetDoorLocation(EGTPVehicleSide::Left) : Body->GetActorLocation());
		AI->MoveToLocation(Target, Out.bMoveToOther ? Settings.TalkDistance * 0.8f : 60.f, false, false);
	}
	if (Out.bTalk && !Out.bMoveToOther)
	{
		if (AI && MoveOrderAge > 0.5f)
		{
			AI->StopMovement();
		}
		// Faces the other driver, or the way the other car went.
		const FVector Look = Opponent ? Opponent->GetActorLocation() : (MyCar ? MyCar->GetActorLocation() : Body->GetActorLocation());
		const FVector Dir = (Look - Body->GetActorLocation()).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			Body->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));
		}
	}
	if (Out.bShove && Opponent)
	{
		ACharacter* Target = Other.Get();
		const FVector Dir = (Target->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
		Target->LaunchCharacter(Dir * ShoveSpeed + FVector(0.f, 0.f, 60.f), true, false);
		GTP_TRACE(LogGTPIncident, TEXT("incident: %s shoves %s (%d)"), *Body->GetName(), *Target->GetName(), Arguer.Shoves);
	}
}
