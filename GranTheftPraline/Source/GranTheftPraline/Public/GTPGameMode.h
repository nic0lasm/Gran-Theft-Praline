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
 *
 * Driving test (version 0.3): with -TestVehicle on the command line, one
 * TestVehicleClass car is parked TestVehicleOffset in front of the first
 * spawned player, so a car can be tried in any level without editing it.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGTPGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;

protected:
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes", meta = (MetaClass = "/Script/Engine.Pawn"))
	FSoftClassPath PlayerPawnClass;

	/** Car parked in front of the player by -TestVehicle (a Blueprint child of AGTPVehicle). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes", meta = (MetaClass = "/Script/GranTheftPraline.GTPVehicle"))
	FSoftClassPath TestVehicleClass;

	/** Where the test car is parked, in the player's frame (cm, X forward). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes")
	FVector TestVehicleOffset = FVector(600.f, 250.f, 50.f);

private:
	bool bTestVehicleSpawned = false;
};
