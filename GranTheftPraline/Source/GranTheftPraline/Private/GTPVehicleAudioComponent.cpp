#include "GTPVehicleAudioComponent.h"

#include "ChaosWheeledVehicleMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "GTPSurfaceLogic.h"
#include "GTPSynthSoundWave.h"
#include "WheeledVehiclePawn.h"

namespace
{
	/** The squeal is only heard (and computed) this close to the listener (cm). */
	constexpr float SkidListenDistance = 6000.f;
	/** Sideways slide below this is normal cornering (cm/s). */
	constexpr float SlideDeadZone = 180.f;
}

UGTPVehicleAudioComponent::UGTPVehicleAudioComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickInterval = 0.05f;
	bAutoActivate = false;
	bAllowSpatialization = true;
}

void UGTPVehicleAudioComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGTPAudioMix* Mix = UGTPAudioMix::Get(this))
	{
		SoundClassOverride = Mix->GetClass(EGTPAudioCategory::Vehicles);
		AttenuationSettings = Mix->GetAttenuation(EGTPAudioCategory::Vehicles);
		ConcurrencySet.Add(Mix->GetConcurrency(EGTPAudioCategory::Vehicles));
	}
	Skid = MakeShared<FGTPSkidSynth, ESPMode::ThreadSafe>();
	Skid->Init(UGTPSynthSoundWave::SynthSampleRate, FGTPSkidSettings());
	TSharedPtr<FGTPSkidSynth, ESPMode::ThreadSafe> Shared = Skid;
	SkidWave = NewObject<UGTPSynthSoundWave>(this);
	SkidWave->Setup([Shared](float* Out, int32 Count) { Shared->Render(Out, Count); });
	SetSound(SkidWave);
	SetVolumeMultiplier(GetDefault<UGTPAudioSettings>()->SkidVolume);

	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(GetOwner() ? GetOwner()->GetRootComponent() : nullptr))
	{
		Body->SetNotifyRigidBodyCollision(true);
		Body->OnComponentHit.AddDynamic(this, &UGTPVehicleAudioComponent::OnBodyHit);
		BoundBody = Body;
	}
}

void UGTPVehicleAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UPrimitiveComponent* Body = BoundBody.Get())
	{
		Body->OnComponentHit.RemoveDynamic(this, &UGTPVehicleAudioComponent::OnBodyHit);
	}
	Stop();
	Super::EndPlay(EndPlayReason);
}

EGTPImpactKind UGTPVehicleAudioComponent::ClassifyHit(const AActor* OtherActor, const FHitResult& Hit) const
{
	if (Cast<AWheeledVehiclePawn>(OtherActor))
	{
		return EGTPImpactKind::Car;
	}
	if (Cast<ACharacter>(OtherActor))
	{
		return EGTPImpactKind::Body;
	}
	// Pedestrians of the crowd are plain actors with a skeletal mesh.
	if (UGTPAudioMix::IsPedestrian(OtherActor))
	{
		return EGTPImpactKind::Body;
	}
	if (const UGTPAudioMix* Mix = UGTPAudioMix::Get(this))
	{
		switch (Mix->ResolveSurface(Hit))
		{
		case EGTPSurface::Metal:
			return EGTPImpactKind::Metal;
		case EGTPSurface::Wood:
			return EGTPImpactKind::Wood;
		default:
			break;
		}
	}
	return EGTPImpactKind::Wall;
}

void UGTPVehicleAudioComponent::OnBodyHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	const float Now = GetWorld()->GetTimeSeconds();
	const float Mass = HitComponent ? HitComponent->GetMass() : 0.f;
	if (Mass <= 0.f || Now - LastImpactTime < Settings->ImpactCooldown)
	{
		return;
	}
	// Change of speed caused by the impact, in cm/s (same measure as the damage, ADR-0003).
	const float DeltaV = NormalImpulse.Size() / Mass;
	int32 Tier = GTPImpactTier(DeltaV, Settings->ImpactTierSpeeds.X, Settings->ImpactTierSpeeds.Y, Settings->ImpactTierSpeeds.Z);
	// Bumps on the road itself (kerbs, landing after a jump) only count when hard.
	if (Hit.ImpactNormal.Z > 0.7f && Tier < 2)
	{
		Tier = 0;
	}
	if (Tier == 0)
	{
		return;
	}
	LastImpactTime = Now;
	PlayImpact(ClassifyHit(OtherActor, Hit), Tier, Hit.ImpactPoint);
}

void UGTPVehicleAudioComponent::PlayImpact(EGTPImpactKind Kind, int32 Tier, const FVector& Location)
{
	UGTPAudioMix* Mix = UGTPAudioMix::Get(this);
	if (!Mix)
	{
		return;
	}
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	const float Volume = Settings->ImpactVolume * (Tier == 1 ? 0.55f : (Tier == 2 ? 0.8f : 1.f));
	Mix->PlayAt(this, Mix->PickImpact(Kind, Tier), EGTPAudioCategory::Impacts, Location, Volume, FMath::FRandRange(0.94f, 1.06f));
	const float Now = GetWorld()->GetTimeSeconds();
	if (Tier >= 3 && Kind != EGTPImpactKind::Body && Now - LastGlassTime > 1.5f && FMath::FRand() < 0.6f)
	{
		LastGlassTime = Now;
		Mix->PlayAt(this, Mix->PickGlass(), EGTPAudioCategory::Impacts, Location, Volume * 0.8f, FMath::FRandRange(0.95f, 1.05f));
	}
}

float UGTPVehicleAudioComponent::ComputeSkid() const
{
	const AWheeledVehiclePawn* Vehicle = Cast<AWheeledVehiclePawn>(GetOwner());
	const UChaosWheeledVehicleMovementComponent* Movement = Vehicle ? Cast<UChaosWheeledVehicleMovementComponent>(Vehicle->GetVehicleMovementComponent()) : nullptr;
	if (!Movement)
	{
		return 0.f;
	}
	const float FullSlip = FMath::Max(100.f, GetDefault<UGTPAudioSettings>()->SkidFullSlipSpeed);
	bool bAnyContact = false;
	float WheelSkid = 0.f;
	for (int32 Index = 0; Index < Movement->GetNumWheels(); ++Index)
	{
		const FWheelStatus& Wheel = Movement->GetWheelState(Index);
		if (!Wheel.bInContact)
		{
			continue;
		}
		bAnyContact = true;
		if (Wheel.bIsSkidding)
		{
			WheelSkid = FMath::Max(WheelSkid, FMath::Clamp(Wheel.SkidMagnitude / FullSlip, 0.f, 1.f));
		}
	}
	if (!bAnyContact)
	{
		return 0.f;
	}
	// Sideways slide of the body: drifts and handbrake turns.
	const FVector Velocity = Vehicle->GetVelocity();
	const float Sideways = FMath::Abs(FVector::DotProduct(Velocity, Vehicle->GetActorRightVector()));
	const float Slide = FMath::Clamp((Sideways - SlideDeadZone) / (FullSlip - SlideDeadZone), 0.f, 1.f);
	return FMath::Max(Slide, WheelSkid);
}

void UGTPVehicleAudioComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Skid.IsValid())
	{
		return;
	}
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	bool bNear = true;
	if (Player)
	{
		FVector Listener, Front, Right;
		Player->GetAudioListenerPosition(Listener, Front, Right);
		bNear = FVector::DistSquared(Listener, GetComponentLocation()) < FMath::Square(SkidListenDistance);
	}
	SkidAmount = bNear ? ComputeSkid() : 0.f;
	Skid->SetAmount(SkidAmount);
	if (SkidAmount > 0.02f)
	{
		SilentTime = 0.f;
		if (!IsPlaying())
		{
			Play();
		}
	}
	else if (IsPlaying())
	{
		SilentTime += DeltaTime;
		if (SilentTime > 0.6f)
		{
			Stop();
		}
	}
}
