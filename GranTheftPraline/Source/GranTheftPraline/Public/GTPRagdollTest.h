#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPRagdollTest.generated.h"

/**
 * Automatic scenario without a player at the keyboard, started by -RagdollTest on
 * the command line (-Ragdoll=0 turns the whole ragdoll system off instead). It uses
 * the player's character and copies of it, and logs one "GTPRagdollTest:" line per
 * check, then "GTPRagdollTest: PASS" or "FAIL" and quits when -RagdollTestQuit is set:
 *   1. car hits at 20, 40 and 70 km/h (the launch the rules give is applied to a body);
 *   2. falls of 3 m and 8 m;
 *   3. three, then four, bodies ragdolling at once (the fourth must be refused);
 *   4. every body that may get up does, the heavy one stays down.
 * It measures the game-thread time of the scenario frames for the budget of ADR-0006.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPRagdollTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

private:
	void Fail(const FString& Why);
	void Finish();
	class ACharacter* SpawnBody(const FVector& Offset);

	int32 Step = 0;
	float StepTime = 0.f;
	float WaitBudget = 0.f;
	bool bFailed = false;
	TArray<TWeakObjectPtr<class ACharacter>> Bodies;
	TWeakObjectPtr<class ACharacter> Player;
	double FrameMsSum = 0.0;
	int32 FrameCount = 0;
};
