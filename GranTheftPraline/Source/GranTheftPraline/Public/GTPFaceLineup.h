#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPFaceLineup.generated.h"

class ACameraActor;
class AGTPPedestrian;
class APawn;
class USkeletalMeshComponent;

/**
 * -NicolitoFaces: portraits of the male heads of the City Sample crowd, for the choice of Nicolito's face
 * (docs/gameplay/player-character.md, "Nicolito's face"). 10 s after the start, every normal-weight male head of
 * the pack (skeletal meshes named *_nrw_FaceMesh under <CrowdCharacterRoot>/Male, at most MaxHeads, in name
 * order) is put on the crowd's normal-weight male body, stood 4 m in front of the player and turned towards him,
 * filmed face on from CameraDistanceCm by a camera of its own and saved as Nicolito_<head>.png in the project's
 * screenshot folder; then the player's view comes back. The game's follow camera, which takes the view back every
 * frame, is paused meanwhile, and the player and the HUD are hidden. Which way the face looks is taken from the body's
 * reference pose (clavicles), not assumed. The log says "face lineup: ..." for each head and where each image
 * went. Nothing changes without the switch.
 *
 * -NicolitoPortrait: the same bench on the player himself, as he is in the game (his visual, PlayerVisualClass or
 * -PlayerVisual=): a face portrait (Nicolito_player_face.png) and a full-length one from the front
 * (Nicolito_player_body.png, PlayerBodyDistanceCm), 10 s after the start, the player left visible. At each shot the
 * log says "rig: ..." (see LogRig).
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPFaceLineupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** Same folder and naming as the crowd's City Sample characters (GTPCrowd). */
	UPROPERTY(Config)
	FString CrowdCharacterRoot = TEXT("/Game/Crowd/Character");
	UPROPERTY(Config)
	FString FaceAnimClass = TEXT("/Game/Crowd/Character/Shared/Rig/ABP_CrowdCharacter_Head.ABP_CrowdCharacter_Head_C");
	UPROPERTY(Config)
	int32 MaxHeads = 24;
	UPROPERTY(Config)
	float CameraDistanceCm = 120.f;
	UPROPERTY(Config)
	float CameraFOV = 30.f;
	UPROPERTY(Config)
	float PlayerBodyDistanceCm = 380.f;
	UPROPERTY(Config)
	float PlayerBodyFOV = 50.f;

private:
	void ShowNext();
	void Shoot();
	/** Holds the stage for the portraits (game camera paused, player and HUD hidden), or gives it back. */
	void HoldStage(APlayerController* PC, bool bHold, bool bHidePlayer = true);
	/** -NicolitoPortrait: places the camera on the player's own head (face) or in front of him (body). */
	bool FramePlayer(APlayerController* PC, bool bFace);
	/**
	 * -NicolitoPortrait: what the player's visual body reads, in numbers, for what the images show: the left upper
	 * arm's angle from straight down on the pawn's (hidden) mesh and on the body (the same pose if the retarget
	 * follows), the body's LOD, its bones at the component's origin while their reference place is not (never
	 * written by its pose), and for each mesh following the body's pose the bones it lacks and those it reads at
	 * the origin (a vertex weighted to them is pulled towards the feet).
	 */
	void LogRig(const APawn* Pawn) const;

	/** Object path of each head's face mesh, and its short name (m_001). */
	TArray<FString> Heads;
	TArray<FString> Labels;
	int32 Current = -1;
	FTimerHandle Timer;
	UPROPERTY(Transient)
	TObjectPtr<AGTPPedestrian> Model;
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
	/** The player's visible body, found by FramePlayer. */
	TWeakObjectPtr<USkeletalMeshComponent> PlayerBody;
	/** What HoldStage changed, to put back as it was. */
	TArray<TWeakObjectPtr<UActorComponent>> PausedCameras;
	TArray<TWeakObjectPtr<AActor>> HiddenActors;
	bool bStageHeld = false;
	bool bPlayerPortrait = false;
	bool bHudWasShown = false;
};
