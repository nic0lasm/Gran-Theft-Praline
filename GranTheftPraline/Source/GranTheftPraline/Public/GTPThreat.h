#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GTPHitReaction.h"
#include "GTPThreatLogic.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPThreat.generated.h"

class UAnimMontage;

UENUM(BlueprintType)
enum class EGTPTemperament : uint8
{
	Compliant,
	Frozen,
	Brave,
	Fleeing
};

UENUM(BlueprintType)
enum class EGTPThreatState : uint8
{
	Calm,
	Alarmed,
	HandsUp,
	/** Hands up and obeying: the shop hold-up script opens the till on this. */
	Complying,
	Backing,
	Fleeing,
	Down
};

DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPThreatStateEvent, AActor* /*Body*/, EGTPThreatState /*Old*/, EGTPThreatState /*New*/);

GRANTHEFTPRALINE_API DECLARE_LOG_CATEGORY_EXTERN(LogGTPThreat, Log, All);

class UGTPThreatResponseComponent;

/**
 * Who threatens whom (version 0.7, hold-ups): the combat system (or -ThreatDemo for
 * tests) tells this subsystem about an armed person aiming, and about shots; the
 * response components of the people around read it. One fear for the whole game.
 * Off unless -ThreatResponse=1 is on the command line (ADR-0006).
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPThreatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static bool IsEnabled();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** An armed (or not) person aims along AimDirection from Origin. Call every frame the aim holds, or once and ClearThreat later. */
	void SetThreat(AActor* Source, bool bArmed, const FVector& Origin, const FVector& AimDirection);
	void ClearThreat();

	/** A shot was fired from From (Target may be null). */
	void NotifyShot(const FVector& From, AActor* Target);

	bool HasThreat() const { return Source.IsValid() && bActive; }
	AActor* GetSource() const { return Source.Get(); }
	bool IsArmed() const { return bArmed; }
	FVector GetOrigin() const { return Origin; }
	FVector GetAim() const { return Aim; }

	int32 GetShotSerial() const { return ShotSerial; }
	FVector GetLastShotFrom() const { return LastShotFrom; }
	TWeakObjectPtr<AActor> GetLastShotTarget() const { return LastShotTarget; }

	void Register(UGTPThreatResponseComponent* Component);
	void Unregister(UGTPThreatResponseComponent* Component);
	/** Highest spreading fear among the registered people near Location, other than Self. */
	float NearbyFear(const UGTPThreatResponseComponent* Self, const FVector& Location, float RadiusCm) const;

private:
	void RefreshDemoThreat();

	TWeakObjectPtr<AActor> Source;
	bool bActive = false;
	bool bArmed = true;
	FVector Origin = FVector::ZeroVector;
	FVector Aim = FVector::ForwardVector;
	int32 ShotSerial = 0;
	FVector LastShotFrom = FVector::ZeroVector;
	TWeakObjectPtr<AActor> LastShotTarget;
	TArray<TWeakObjectPtr<UGTPThreatResponseComponent>> Components;
	FTimerHandle DemoTimer;
};

/**
 * What one person does when threatened: fear, hands up, obeying, backing away, fleeing,
 * falling when hit (ADR-0006, rules in GTPThreatLogic.h, checked offline). Add it to any
 * body (shop staff, pedestrians). It never ticks unless -ThreatResponse=1. The owner's
 * AI listens to OnThreatState; locomotion of characters (backing, fleeing) is done here,
 * of pooled pedestrians by their own system.
 */
UCLASS(Config = Game, ClassGroup = (Gameplay), meta = (BlueprintSpawnableComponent))
class GRANTHEFTPRALINE_API UGTPThreatResponseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGTPThreatResponseComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Threat")
	void SetTemperament(EGTPTemperament NewTemperament) { Brain.Temperament = static_cast<GTPThreat::ETemperament>(static_cast<int32>(NewTemperament)); }

	/** Where this person runs when he flees (the shop's back door); unset = away from the threat. */
	UFUNCTION(BlueprintCallable, Category = "Threat")
	void SetFleeTarget(FVector Location) { FleeTarget = Location; bHasFleeTarget = true; }

	UFUNCTION(BlueprintPure, Category = "Threat")
	EGTPTemperament GetTemperament() const { return static_cast<EGTPTemperament>(static_cast<int32>(Brain.Temperament)); }

	UFUNCTION(BlueprintPure, Category = "Threat")
	EGTPThreatState GetState() const { return static_cast<EGTPThreatState>(static_cast<int32>(Brain.State)); }

	/** Fear, 0..1. */
	UFUNCTION(BlueprintPure, Category = "Threat")
	float GetFear() const { return Brain.Fear; }

	/** Seconds since an armed person last aimed at him (a large number when never). */
	UFUNCTION(BlueprintPure, Category = "Threat")
	float GetSecondsSinceAimed() const { return Brain.SinceAimed; }

	UFUNCTION(BlueprintPure, Category = "Threat")
	bool ObeysOrders() const { return GTPThreat::ObeysOrders(Brain); }

	/** Fear spreading to the people around (0 when calm or down). */
	float GetSpreadFear() const { return GTPThreat::SpreadFear(Brain); }

	/** The shop script, the police and the pedestrians listen here. */
	FGTPThreatStateEvent OnThreatState;

	/** Reached the flee target (the shop script: StaffEscaped). */
	DECLARE_MULTICAST_DELEGATE_OneParam(FGTPThreatArrived, AActor* /*Body*/);
	FGTPThreatArrived OnFledToTarget;

	/** Keyed clips by name: AM_HandsUp_Enter and AM_HandsUp_Idle. */
	UPROPERTY(EditAnywhere, Category = "Threat")
	TMap<FName, TSoftObjectPtr<UAnimMontage>> Clips;

	UPROPERTY(EditAnywhere, Category = "Threat")
	EGTPTemperament InitialTemperament = EGTPTemperament::Compliant;

protected:
	/** Aim cone half angle (degrees) and range (cm). */
	UPROPERTY(Config)
	float AimHalfAngleDeg = 12.f;

	UPROPERTY(Config)
	float AimRangeCm = 2000.f;

	UPROPERTY(Config)
	float ThinkInterval = 0.1f;

	UPROPERTY(Config)
	float BackingSpeedScale = 0.35f;

	UPROPERTY(Config)
	float FleeArrivalCm = 120.f;

private:
	void Think(float Dt);
	void OnStateChanged(GTPThreat::EState Old, GTPThreat::EState New);
	void OnHitReaction(EGTPHitReaction Reaction, const FGTPHitInfo& Hit, AActor* Body);
	void PlayClip(const TCHAR* Name);
	void PlayIdle();
	void MoveCharacter(float Dt);

	GTPThreat::FBrain Brain;
	GTPThreat::FSettings Settings;
	float ThinkAccumulator = 0.f;
	int32 SeenShotSerial = 0;
	FVector FleeTarget = FVector::ZeroVector;
	bool bHasFleeTarget = false;
	bool bArrivedReported = false;
	FTimerHandle IdleTimer;
};
