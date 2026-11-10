#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GTPWalkLogic.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPPedestrians.generated.h"

class AGTPVehicle;
class UAnimSequenceBase;
class USceneComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * A person on foot, not the player: a skeletal mesh played by a single animation
 * (no animation blueprint, no movement component, no physics until hit). The
 * pedestrian subsystem moves it; the actor itself never ticks.
 */
UCLASS(NotBlueprintable)
class GRANTHEFTPRALINE_API AGTPPedestrian : public AActor
{
	GENERATED_BODY()

public:
	AGTPPedestrian();

	USkeletalMeshComponent* GetBody() const { return Body; }

	/** Plays Animation looping at PlayRate (no change when it is already playing). */
	void PlayLoop(UAnimSequenceBase* Animation, float PlayRate);

	/** Shadow, level of detail and drawing, set by distance from the viewer. */
	void SetDetail(bool bVisible, bool bCastShadow, int32 ForcedLOD);

	bool IsRagdoll() const { return bRagdoll; }

	/** Hit by a car: physics takes over, the subsystem takes the body away a few seconds later. */
	bool StartRagdoll(const FVector& Impulse);

	/** Back to the standing pose, ready to be placed again. */
	void ResetBody();

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> CurrentAnimation;

	bool bRagdoll = false;
	bool bShadow = true;
	bool bShown = true;
	int32 LOD = -1;
};

/** A person was hit by a car (Car may be null) or scared into running away, at Location. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPPedestrianEvent, const FVector& /*Location*/, AActor* /*Cause*/);

/**
 * Pedestrians for the streets that have a walkways file (version 0.4).
 *
 * Walkways come from Content/City/Pedestrians/<MapName>.walkways.csv (written by
 * tools/city/pedestrians.py, staged with the game). The subsystem keeps a fixed
 * pool of people, sized by the graphics profile, and moves them along the walkway
 * graph with the rules of GTPWalkLogic.h. People appear and disappear only out of
 * sight. Cost limits (ADR-0005): one animation per person and no animation
 * blueprint, only MaxVisible people drawn, no shadow and a lower level of detail
 * with distance, nothing drawn beyond DrawDistance.
 *
 * Count: -Pedestrians=N on the command line (0 turns them off), else the
 * density of the active profile (DensityM1, DensityFidelity, DensityUltra).
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPPedestrianSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Path of the walkways file for a map name, under the project content folder. */
	static FString WalkwaysFileFor(const FString& MapName);

	/** Reads a walkways CSV (pedestrians.py format) into Graph; returns the number of edges. */
	static int32 LoadWalkwaysFromCsv(const FString& Path, GTPWalk::FGraph& OutGraph);

	int32 GetActiveCount() const;

	/** Police and other systems listen here (a person hit by a car / frightened into running). */
	FGTPPedestrianEvent OnPedestrianHit;
	FGTPPedestrianEvent OnPedestrianScared;

protected:
	/** People per graphics profile. */
	UPROPERTY(Config)
	int32 DensityM1 = 12;

	UPROPERTY(Config)
	int32 DensityFidelity = 24;

	UPROPERTY(Config)
	int32 DensityUltra = 40;

	/** At most this many people are drawn at once, the closest ones (0 = no limit). */
	UPROPERTY(Config)
	int32 MaxVisible = 10;

	/** Skeletal meshes to pick from; empty = the mesh of the player's character. */
	UPROPERTY(Config)
	TArray<FSoftObjectPath> Meshes;

	UPROPERTY(Config)
	FSoftObjectPath WalkAnimation;

	UPROPERTY(Config)
	FSoftObjectPath IdleAnimation;

	UPROPERTY(Config)
	FSoftObjectPath RunAnimation;

	/** Walking speed the walk animation was made for (cm/s): play rate = speed / this. */
	UPROPERTY(Config)
	float AnimationWalkSpeed = 140.f;

	UPROPERTY(Config)
	float RunAnimationSpeed = 340.f;

	/** The mannequin faces +Y: yaw applied to the mesh so it faces the walking direction. */
	UPROPERTY(Config)
	float MeshYawOffset = -90.f;

	/** Closer than this to the viewer: shadow and full detail (cm). */
	UPROPERTY(Config)
	float DetailDistance = 2500.f;

	/** Beyond DetailDistance the mesh is drawn from this level of detail (0-based, clamped to what the mesh has). */
	UPROPERTY(Config)
	int32 FarLOD = 2;

	/** Beyond this distance people are not drawn (cm). */
	UPROPERTY(Config)
	float DrawDistance = 9000.f;

	/** People appear no closer than this, out of sight (cm). */
	UPROPERTY(Config)
	float MinSpawnDistance = 2500.f;

	/** ...and no farther than this. */
	UPROPERTY(Config)
	float MaxSpawnDistance = 11000.f;

	/** People farther than this, out of sight, are moved closer to the player (cm). */
	UPROPERTY(Config)
	float MaxDistance = 14000.f;

	/** They step aside from the player closer than this (cm). */
	UPROPERTY(Config)
	float PersonalSpace = 130.f;

	/** A car faster than this (cm/s) coming at someone within FleeRadius makes them run. */
	UPROPERTY(Config)
	float FleeCarSpeed = 900.f;

	UPROPERTY(Config)
	float FleeRadius = 1400.f;

	/** A car faster than this (cm/s) touching someone knocks them down (needs a physics asset). */
	UPROPERTY(Config)
	float HitCarSpeed = 500.f;

	UPROPERTY(Config)
	bool bRagdollOnHit = true;

	UPROPERTY(Config)
	float RagdollSeconds = 8.f;

	/** A car this close (cm) to a crossing, moving, makes people wait at the kerb. */
	UPROPERTY(Config)
	float CrossingLookRadius = 2200.f;

private:
	struct FPerson
	{
		TWeakObjectPtr<AGTPPedestrian> Actor;
		GTPWalk::FWalker Walker;
		FVector Location = FVector::ZeroVector;
		float GroundZ = 0.f;
		float Yaw = 0.f;
		float RagdollLeft = 0.f;
		float Distance = 0.f;
		bool bActive = false;
		bool bRunning = false;
		bool bIdle = false;
		bool bDrawn = false;
	};

	int32 ChooseDensity() const;
	void RefreshCars();
	void RefreshViewer();
	void RefreshDetail();
	bool IsSeen(const FVector& Location) const;
	bool TrySpawn(FPerson& Person);
	void Retire(FPerson& Person);
	void MoveGroundTo(FPerson& Person, bool bSnap);
	void Update(FPerson& Person, float DeltaTime);
	void LogStats();
	bool CarNear(const FVector& Where, float Radius, float MinSpeed, FVector* OutCarLocation = nullptr, FVector* OutCarVelocity = nullptr) const;
	USkeletalMesh* PickMesh();

	GTPWalk::FGraph Graph;
	GTPWalk::FSettings Settings;
	GTPWalk::FRng Rng{0x50454421u};
	TArray<FPerson> People;
	int32 Wanted = 0;

	struct FCar
	{
		FVector Location;
		FVector Velocity;
	};
	TArray<FCar> Cars;

	FVector ViewerLocation = FVector::ZeroVector;
	FVector ViewerForward = FVector::ForwardVector;
	FVector PlayerLocation = FVector::ZeroVector;
	bool bHasViewer = false;
	float NextCarRefresh = 0.f;
	float NextDetailRefresh = 0.f;
	float NextSpawn = 0.f;
	float NextStats = 10.f;
	float Clock = 0.f;
	int32 GroundCursor = 0;
	int32 SpawnedTotal = 0;
	int32 FledTotal = 0;
	int32 HitTotal = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkeletalMesh>> LoadedMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Walk;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Idle;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Run;

};
