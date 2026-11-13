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
 *
 * Traffic (version 0.3): when the map has no AGTPTrafficNetwork but
 * Content/City/Traffic/<MapName>.lanes.csv exists, one is spawned at start
 * (cars with a driver to pull out; -Traffic=N sets how many, 0 for none).
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API AGTPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGTPGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void StartPlay() override;

protected:
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes", meta = (MetaClass = "/Script/Engine.Pawn"))
	FSoftClassPath PlayerPawnClass;

	/**
	 * A visual that follows the player pawn: an Actor class (Nicolito's MetaHuman blueprint) is spawned and
	 * attached to the pawn at each spawn of the player; the pawn's own mesh keeps playing the animation but is hidden.
	 * -PlayerVisual=<class path> overrides the config (empty = none, the pawn's own mesh shows).
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes", meta = (MetaClass = "/Script/Engine.Actor"))
	FSoftClassPath PlayerVisualClass;

	/** Where the visual sits relative to the pawn: Z -capsule half height puts the feet on the ground, yaw lines it up with the mesh. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes")
	FVector PlayerVisualOffset = FVector(0.f, 0.f, -90.f);

	/** Name of the visual's body component that must hang from the pawn's mesh (-PlayerVisualBody= overrides); empty = none. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes")
	FString PlayerVisualBodyName = TEXT("Body");

	/**
	 * IK Retargeter put on the body's "Retarget Pose From Mesh" node at run time: the sample's MetaHuman is wired for a
	 * MetaHuman source, ours is a UEFN mannequin (the sample ships RTG_UEFN_to_Metahuman_nrw). Empty = leave it.
	 * -PlayerVisualRetargeter= overrides.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes")
	FString PlayerVisualRetargeter = TEXT("/Game/MetaHumans/Common/Common/Rigs/RTG_UEFN_to_Metahuman_nrw.RTG_UEFN_to_Metahuman_nrw");

	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes")
	float PlayerVisualYaw = -90.f;

	/**
	 * The visual's body plays the baked MH_ walk/idle/run clips instead of the runtime retarget (a way around a
	 * T-pose), and goes back to the runtime retarget while a weapon is drawn. Off by default (the slice uses the
	 * runtime retarget); -PlayerVisualBaked turns it on for one run.
	 */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes")
	bool bPlayerVisualBaked = false;

	/** Car parked in front of the player by -TestVehicle (a Blueprint child of AGTPVehicle). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes", meta = (MetaClass = "/Script/GranTheftPraline.GTPVehicle"))
	FSoftClassPath TestVehicleClass;

	/** Where the test car is parked, in the player's frame (cm, X forward): straight ahead in the street, its driver door within reach. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "Classes")
	FVector TestVehicleOffset = FVector(280.f, 0.f, 30.f);

private:
	void ApplyPlayerVisual(APawn* Pawn);

	bool bTestVehicleSpawned = false;
};
