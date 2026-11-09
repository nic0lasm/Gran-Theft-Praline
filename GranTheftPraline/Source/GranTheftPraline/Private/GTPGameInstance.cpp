#include "GTPGameInstance.h"

#include "GeneralProjectSettings.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void UGTPGameInstance::Init()
{
	Super::Init();

	if (FParse::Param(FCommandLine::Get(), TEXT("SkipTitle")))
	{
		bTitleDismissed = true;
	}
}

FString UGTPGameInstance::GetDisplayVersion() const
{
	const UGeneralProjectSettings* Settings = GetDefault<UGeneralProjectSettings>();
	return Settings ? Settings->ProjectVersion : FString();
}
