#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPTimedExec.generated.h"

/**
 * Console commands at set times of a game run, for GPU profiles of a packaged Development
 * build without a person at the keyboard (quarter_slot_mac.sh GPU_PROFILE=1):
 *   -ExecAt=60=ProfileGPU|62=stat+RHI|65=stat+dumpframe+-ms=0.1
 * Each entry is <seconds after begin play>=<command>; entries are separated by '|' and '+'
 * stands for a space (launch scripts split the game arguments on spaces). One log
 * line per command run: "TimedExec: t=<s> <command>". Nothing happens without -ExecAt.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPTimedExec : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Parses "60=ProfileGPU|62=stat RHI" into (seconds, command); false when malformed. */
	static bool ParseList(const FString& List, TArray<TPair<float, FString>>& Out);

private:
	TArray<FTimerHandle> Timers;
};
