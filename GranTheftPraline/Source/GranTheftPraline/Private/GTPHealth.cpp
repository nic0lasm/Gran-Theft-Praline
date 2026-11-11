#include "GTPHealth.h"

#include "GTPCombat.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UGTPHealthComponent::UGTPHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UGTPHealthComponent* UGTPHealthComponent::Find(AActor* Actor, bool bCreate)
{
	if (!Actor)
	{
		return nullptr;
	}
	UGTPHealthComponent* Health = Actor->FindComponentByClass<UGTPHealthComponent>();
	if (!Health && bCreate && !Actor->IsActorBeingDestroyed())
	{
		Health = NewObject<UGTPHealthComponent>(Actor, MakeUniqueObjectName(Actor, UGTPHealthComponent::StaticClass(), TEXT("GTPHealth")));
		Health->RegisterComponent();
	}
	return Health;
}

void UGTPHealthComponent::InitialiseIfNeeded()
{
	if (bInitialised)
	{
		return;
	}
	bInitialised = true;
	Settings.MaxHealth = FMath::Max(1.f, MaxHealth);
	Settings.MaxArmor = FMath::Max(0.f, MaxArmor);
	Settings.ArmorAbsorb = FMath::Clamp(ArmorAbsorb, 0.f, 1.f);
	Settings.RegenDelaySeconds = RegenDelaySeconds;
	Settings.RegenPerSecond = RegenPerSecond;
	Settings.RegenCapShare = FMath::Clamp(RegenCapShare, 0.f, 1.f);
	Settings.MaxStamina = FMath::Max(1.f, MaxStamina);
	Settings.StaminaRegenDelaySeconds = StaminaRegenDelaySeconds;
	Settings.StaminaRegenPerSecond = StaminaRegenPerSecond;
	Settings.RespawnDelaySeconds = RespawnDelaySeconds;
	State = GTPHealth::MakeState(Settings, StartArmor);
}

void UGTPHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	InitialiseIfNeeded();
	if (UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this))
	{
		Combat->RegisterHealth(this);
	}
}

void UGTPHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGTPCombatSubsystem* Combat = UGTPCombatSubsystem::Get(this))
	{
		Combat->UnregisterHealth(this);
	}
	Super::EndPlay(EndPlayReason);
}

FGTPDamageOutcome UGTPHealthComponent::ApplyDamage(float Amount, EGTPDamageCause Cause, AActor* Instigator, bool bIgnoreArmor)
{
	InitialiseIfNeeded();
	const GTPHealth::FDamageResult R = GTPHealth::ApplyDamage(State, Settings, Amount, bIgnoreArmor);
	FGTPDamageOutcome Out;
	Out.HealthLost = R.HealthLost;
	Out.ArmorLost = R.ArmorLost;
	Out.HealthAfter = State.Health;
	Out.bKilled = R.bKilled;
	Out.bIgnored = R.bIgnored;
	if (!R.bIgnored)
	{
		OnHurt.Broadcast(this, Out, Cause, Instigator);
		if (R.bKilled)
		{
			OnDied.Broadcast(this, Cause, Instigator);
		}
	}
	return Out;
}

float UGTPHealthComponent::Heal(float Amount)
{
	InitialiseIfNeeded();
	return GTPHealth::Heal(State, Settings, Amount);
}

float UGTPHealthComponent::AddArmor(float Amount)
{
	InitialiseIfNeeded();
	return GTPHealth::AddArmor(State, Settings, Amount);
}

bool UGTPHealthComponent::SpendStamina(float Cost)
{
	InitialiseIfNeeded();
	return GTPHealth::SpendStamina(State, Cost);
}

void UGTPHealthComponent::Revive(float GraceSeconds)
{
	InitialiseIfNeeded();
	GTPHealth::Respawn(State, Settings, GraceSeconds);
}

void UGTPHealthComponent::SetInvulnerableFor(float Seconds)
{
	State.InvulnerableSeconds = FMath::Max(State.InvulnerableSeconds, Seconds);
}

bool UGTPHealthComponent::Step(float DeltaSeconds)
{
	InitialiseIfNeeded();
	return GTPHealth::Tick(State, Settings, DeltaSeconds);
}
