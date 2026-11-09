#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/SoftObjectPath.h"
#include "GTPGameMode.generated.h"

/**
 * Default game mode of the project.
 *
 * The playable character is chosen in Config/DefaultGame.ini:
 *   [/Script/GranTheftPraline.GTPGameMode]
 *   PlayerPawnClass=/Game/Characters/.../BP_Player.BP_Player_C
 * When it is empty or cannot be loaded, the engine default pawn is used.
 * The class must also be cooked (DirectoriesToAlwaysCook or a hard reference).
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGTPGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

protected:
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes", meta = (MetaClass = "/Script/Engine.Pawn"))
	FSoftClassPath PlayerPawnClass;
};
