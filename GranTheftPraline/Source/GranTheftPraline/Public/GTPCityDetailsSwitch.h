#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPCityDetailsSwitch.generated.h"

/**
 * Runtime A/B of the street details for the packaged bench (tools/city/evidence/details_slot_mac.sh,
 * SLOT_MODE=bench), with no rebuild of the map:
 *
 *   -CityDetails=0        hides every actor tagged GTP_Set_details or GTP_Set_details_merged
 *   -CityDetailsMerged=1  shows the one-material bake (GTP_Set_details_merged) instead of the details
 *
 * The baked set is only on maps regenerated with GTP_DETAILS_MERGED=ab; it is hidden by default,
 * so normal play never sees two copies. Hidden actors also lose their collision.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPCityDetailsSwitch : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Which set shows, from the command line: 0 none, 1 details (default), 2 merged details. */
	static int32 ModeFromCommandLine(const TCHAR* CommandLine);
};
