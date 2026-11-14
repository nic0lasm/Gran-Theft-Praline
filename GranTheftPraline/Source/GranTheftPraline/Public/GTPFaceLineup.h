#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPFaceLineup.generated.h"

class ACameraActor;
class AGTPPedestrian;

/**
 * -NicolitoFaces: portraits of the male heads of the City Sample crowd, for the choice of Nicolito's face
 * (docs/gameplay/player-character.md, "Nicolito's face"). 10 s after the start, every normal-weight male head of
 * the pack (skeletal meshes named *_nrw_FaceMesh under <CrowdCharacterRoot>/Male, at most MaxHeads, in name
 * order) is put on the crowd's normal-weight male body, stood 4 m in front of the player and turned towards him,
 * filmed face on from CameraDistanceCm by a camera of its own and saved as Nicolito_<head>.png in the project's
 * screenshot folder; then the player's view comes back. Which way the face looks is taken from the body's
 * reference pose (clavicles), not assumed. The log says "face lineup: ..." for each head and where each image
 * went. Nothing changes without the switch.
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

private:
	void ShowNext();
	void Shoot();

	/** Object path of each head's face mesh, and its short name (m_001). */
	TArray<FString> Heads;
	TArray<FString> Labels;
	int32 Current = -1;
	FTimerHandle Timer;
	UPROPERTY(Transient)
	TObjectPtr<AGTPPedestrian> Model;
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> Camera;
};
