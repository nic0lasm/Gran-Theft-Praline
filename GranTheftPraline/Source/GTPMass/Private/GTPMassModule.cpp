#include "Modules/ModuleManager.h"

#include "GTPZoneGraphImport.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectGlobals.h"

class FGTPMassModule : public FDefaultModuleImpl
{
public:
	virtual void StartupModule() override
	{
		// MassTraffic's lane filters, pointed at our ZoneGraph tags once every plugin is loaded and
		// before any map: the game and the package need no config file for them. Nothing to do when
		// the optional Traffic plugin is absent (Mac, CI).
		FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			if (FindObject<UClass>(nullptr, TEXT("/Script/MassTraffic.MassTrafficSettings")))
			{
				UGTPZoneGraphImport::ConfigureTrafficLaneFilters(TEXT("Car"), TEXT("Intersection"));
			}
		});
	}
};

IMPLEMENT_MODULE(FGTPMassModule, GTPMass);
