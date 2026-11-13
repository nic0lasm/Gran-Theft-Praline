#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"

// -GTARun (GTPGtaRun.h) needs the demo route's chase: add its switch to the command line before the game
// instance, and so the demo route, reads it. The module starts after the command line is set.
class FGTPGameModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("GTARun")) && !FParse::Param(FCommandLine::Get(), TEXT("PoliceChaseDemo")))
		{
			FCommandLine::Append(TEXT(" -PoliceChaseDemo"));
		}
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FGTPGameModule, GranTheftPraline, "GranTheftPraline");
