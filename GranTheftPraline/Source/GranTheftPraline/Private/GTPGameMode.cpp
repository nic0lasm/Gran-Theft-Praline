#include "GTPGameMode.h"

#include "GTPPlayerController.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPGameMode, Log, All);

AGTPGameMode::AGTPGameMode()
{
	PlayerControllerClass = AGTPPlayerController::StaticClass();
}

void AGTPGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (PlayerPawnClass.IsNull())
	{
		return;
	}

	if (UClass* PawnClass = PlayerPawnClass.TryLoadClass<APawn>())
	{
		DefaultPawnClass = PawnClass;
	}
	else
	{
		UE_LOG(LogGTPGameMode, Warning, TEXT("PlayerPawnClass '%s' could not be loaded, using the default pawn."),
			*PlayerPawnClass.ToString());
	}
}
