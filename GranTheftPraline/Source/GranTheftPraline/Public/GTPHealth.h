#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPHealthLogic.h"
#include "GTPHealth.generated.h"

/** Why a body lost health (the HUD, the police and the statistics tell them apart). */
UENUM(BlueprintType)
enum class EGTPDamageCause : uint8
{
	Bullet,
	Melee,
	Vehicle,
	Fall,
	Other
};

USTRUCT(BlueprintType)
struct FGTPDamageOutcome
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	float HealthLost = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float ArmorLost = 0.f;

	/** Health left after the hit (0..MaxHealth). */
	UPROPERTY(BlueprintReadOnly)
	float HealthAfter = 0.f;

	UPROPERTY(BlueprintReadOnly)
	bool bKilled = false;

	/** Dead already, invulnerable (dodge, respawn grace) or zero damage. */
	UPROPERTY(BlueprintReadOnly)
	bool bIgnored = false;
};

class UGTPHealthComponent;
DECLARE_MULTICAST_DELEGATE_FourParams(FGTPHealthHurt, UGTPHealthComponent* /*Health*/, const FGTPDamageOutcome& /*Outcome*/, EGTPDamageCause /*Cause*/, AActor* /*Instigator*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPHealthDied, UGTPHealthComponent* /*Health*/, EGTPDamageCause /*Cause*/, AActor* /*Killer*/);

/**
 * The health of a body (player, police officer, shop staff, a pedestrian once hit):
 * health, armour, stamina, death (ADR-0011, rules in GTPHealthLogic.h). It never ticks:
 * UGTPCombatSubsystem steps every registered body at 10 Hz. It decides nothing about
 * reactions (ragdoll thread) or crimes (police thread); it holds the numbers and says
 * when they change.
 *
 * Other systems that hurt a body (cars, falls) call ApplyDamage here when the body has
 * this component, so there is a single health per body.
 */
UCLASS(Config = Game, ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPHealthComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The component of an actor, created when missing (Create = true). */
	static UGTPHealthComponent* Find(AActor* Actor, bool bCreate = false);

	/** Takes Amount points of damage. bIgnoreArmor: melee, falls, cars. */
	UFUNCTION(BlueprintCallable, Category = "Health")
	FGTPDamageOutcome ApplyDamage(float Amount, EGTPDamageCause Cause, AActor* Instigator, bool bIgnoreArmor = false);

	UFUNCTION(BlueprintCallable, Category = "Health")
	float Heal(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Health")
	float AddArmor(float Amount);

	/** Pays stamina (punch, dodge, block); false and nothing spent when short. */
	bool SpendStamina(float Cost);

	/** Back to life with a short grace without damage (respawn, tests). */
	void Revive(float GraceSeconds = 3.f);

	/** Invulnerable for this long (a dodge's middle) (s). */
	void SetInvulnerableFor(float Seconds);

	/** One step, called by the combat subsystem. Returns true when a dead body is ready to respawn. */
	bool Step(float DeltaSeconds);

	// The HUD reads these every frame (interface thread).
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth() const { return State.Health; }
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetHealth01() const { return Settings.MaxHealth > 0.f ? State.Health / Settings.MaxHealth : 0.f; }
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetArmor01() const { return Settings.MaxArmor > 0.f ? State.Armor / Settings.MaxArmor : 0.f; }
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetStamina() const { return State.Stamina; }
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetStamina01() const { return Settings.MaxStamina > 0.f ? State.Stamina / Settings.MaxStamina : 0.f; }
	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return State.bDead; }
	/** Seconds since the last damage (the HUD hides the arcs after a while). */
	UFUNCTION(BlueprintPure, Category = "Health")
	float GetSecondsSinceDamage() const { return State.SinceDamage; }

	const GTPHealth::FSettings& GetSettings() const { return Settings; }

	/** Any loss of health or armour (not ignored hits). */
	FGTPHealthHurt OnHurt;
	/** Once, when the body dies. */
	FGTPHealthDied OnDied;

	/** Armour on spawn (police officers wear some). */
	UPROPERTY(EditAnywhere, Category = "Health")
	float StartArmor = 0.f;

protected:
	UPROPERTY(Config)
	float MaxHealth = 100.f;
	UPROPERTY(Config)
	float MaxArmor = 100.f;
	UPROPERTY(Config)
	float ArmorAbsorb = 0.7f;
	UPROPERTY(Config)
	float RegenDelaySeconds = 6.f;
	UPROPERTY(Config)
	float RegenPerSecond = 4.f;
	UPROPERTY(Config)
	float RegenCapShare = 0.5f;
	UPROPERTY(Config)
	float MaxStamina = 100.f;
	UPROPERTY(Config)
	float StaminaRegenDelaySeconds = 0.8f;
	UPROPERTY(Config)
	float StaminaRegenPerSecond = 30.f;
	UPROPERTY(Config)
	float RespawnDelaySeconds = 6.f;

private:
	GTPHealth::FSettings Settings;
	GTPHealth::FState State;
	bool bInitialised = false;
	void InitialiseIfNeeded();
};
