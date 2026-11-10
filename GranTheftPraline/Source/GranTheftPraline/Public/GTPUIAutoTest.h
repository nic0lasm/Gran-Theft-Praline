#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "GTPUIAutoTest.generated.h"

/**
 * Automatic check of the settings, without a player (docs/ui/settings.md).
 * Runs only with -UITest=<step> on the command line:
 *
 *   -UITest=write   opens the pause menu and the settings screen, closes them,
 *                   saves a set of non-default settings, reads the file back in
 *                   the same run and compares;
 *   -UITest=verify  in a new launch: the saved settings are the ones written;
 *   -UITest=reset   puts the defaults back.
 *
 * The result goes to Saved/Logs/ui_test.txt, one line per step, ending with
 * "UITEST <step> PASS" or "UITEST <step> FAIL ..." (a Shipping build has no
 * log, so a file). Use with -SkipTitle. For the mini-map A/B, run the same
 * route with and without -NoMinimap.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPUIAutoTestSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
	void RunMenus();
	void RunWrite();
	void RunVerify();
	void RunReset();
	void Report(const FString& Line) const;
	void Finish(bool bPass, const FString& Detail) const;

	FString Step;
	FTimerHandle Timer;
};
