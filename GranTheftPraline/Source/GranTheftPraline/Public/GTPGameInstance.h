#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GTPGameInstance.generated.h"

/**
 * Lives for the whole run of the game. Remembers whether the title screen
 * has already been dismissed, so reloading a level does not show it again.
 *
 * Command line:
 *   -SkipTitle   start directly in gameplay (automated tests, captures).
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	bool HasDismissedTitle() const { return bTitleDismissed; }
	void MarkTitleDismissed() { bTitleDismissed = true; }

	/** Version string shown on the title screen (ProjectVersion in DefaultGame.ini). */
	FString GetDisplayVersion() const;

private:
	bool bTitleDismissed = false;
};
