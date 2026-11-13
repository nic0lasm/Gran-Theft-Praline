#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Perception/AIPerceptionTypes.h"
#include "UObject/Object.h"
#include "GTPPoliceSenses.generated.h"

class AAIController;
class UAIPerceptionComponent;
class UGTPPoliceSenses;

/**
 * One police unit's eyes and ears: an AIPerception component (sight and hearing) on the AI controller
 * that already drives the unit (the driver of a car, an officer on foot). It needs a possessed pawn: the
 * view point is the pawn's. The unit's own code never reads it; it goes through UGTPPoliceSenses.
 */
UCLASS()
class UGTPPoliceEye : public UObject
{
	GENERATED_BODY()

public:
	void Init(UGTPPoliceSenses* InHub, AAIController* Controller);

	UFUNCTION()
	void OnTargetUpdated(AActor* Actor, FAIStimulus Stimulus);

	bool bSees = false;
	TWeakObjectPtr<AAIController> Owner;

private:
	UPROPERTY()
	TObjectPtr<UGTPPoliceSenses> Hub;

	UPROPERTY()
	TObjectPtr<UAIPerceptionComponent> Perception;
};

/**
 * What the police perceive of the player (ADR-0004, decision of 12/11: Perception instead of home-made
 * line traces). Lives on the police director. Sight and hearing come from AIPerception; the rules of
 * what a sighting means stay in GTPWantedLogic.h.
 *
 * Switches: -PolicePerception=0 turns it off (the old line-of-sight test alone decides);
 * -PolicePerception=1 makes it the only judge; the default is both (a unit sees the player when either
 * does), until the PC logs show they agree ("wanted:" lines carry both).
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPPoliceSenses : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 0 = off, 1 = perception only, 2 = both (default). */
	static int32 GetMode();

	/** The pawn the police look for (changes when the player gets in or out of a car). */
	void SetTarget(APawn* Pawn);
	APawn* GetTarget() const { return Target.Get(); }

	/** Gives Controller (which possesses a pawn) eyes and ears. */
	void Attach(AAIController* Controller);

	/** Some unit sees the target now. */
	bool AnySees() const;

	/** Last place and time the target was sensed by sight or by a noise (-1000 = never). */
	const FVector& GetSensedLocation() const { return SensedLocation; }
	float GetSensedTime() const { return SensedTime; }
	bool WasHeardRecently(float Now, float Within) const { return Now - HeardTime <= Within; }

	/** Called by the eyes. */
	void NoteSensed(const FVector& Location, bool bSight, bool bSensed);

	/** A noise at Location (a shot, a crash): the units within range hear it. Does nothing with perception off. */
	static void ReportNoise(const UObject* WorldContext, const FVector& Location, AActor* Instigator, float Loudness);

	/** Sight range and the angle either side of the unit's heading (cm, degrees), hearing range (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "Perception")
	float SightRadius = 6500.f;
	UPROPERTY(Config, EditAnywhere, Category = "Perception")
	float LoseSightRadius = 7500.f;
	UPROPERTY(Config, EditAnywhere, Category = "Perception")
	float SightHalfAngle = 100.f;
	UPROPERTY(Config, EditAnywhere, Category = "Perception")
	float HearingRange = 4000.f;
	/** A sighting is remembered for this long (s). */
	UPROPERTY(Config, EditAnywhere, Category = "Perception")
	float MemorySeconds = 6.f;

private:
	UPROPERTY()
	TArray<TObjectPtr<UGTPPoliceEye>> Eyes;

	TWeakObjectPtr<APawn> Target;
	FVector SensedLocation = FVector::ZeroVector;
	float SensedTime = -1000.f;
	float HeardTime = -1000.f;
};
