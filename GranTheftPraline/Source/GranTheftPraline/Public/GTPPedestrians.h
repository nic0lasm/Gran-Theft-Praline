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

	/** Height scale and clothing colour of this person (variety from one mesh). */
	void ApplyLook(float HeightScale, const FLinearColor& Clothes, const TArray<FName>& TintParameters);

	/** Plays Animation looping at PlayRate (no change when it is already playing). */
	void PlayLoop(UAnimSequenceBase* Animation, float PlayRate);

	/** Cost switches (ADR-0005): no contact shadow, decals, occlusion, GI or per-bone blur, fixed bounds. */
	void MakeCheap();

	/** Seconds between animation updates of the body (0 = every frame). */
	void SetAnimInterval(float Seconds);

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
	float AnimInterval = 0.f;
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

	/**
	 * Makes everybody within Radius (cm) of Location run away from it: a shot, an explosion,
	 * a police charge. Cause is passed on to OnPedestrianScared. Returns how many ran.
	 */
	int32 ScareAround(const FVector& Location, float Radius, AActor* Cause = nullptr);

	/**
	 * Traffic lights hook: return true when people may NOT set off across the crossing at Where
	 * walking along WalkDirection (world, unit): the roads they cross are not red. Unset = they
	 * only look out for cars. Set by the code that owns the lights (e.g. from
	 * AGTPTrafficNetwork::IsWalkAllowed).
	 */
	TFunction<bool(const FVector& Where, const FVector& WalkDirection)> CrossingBlockedQuery;

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

	/** Optional loop for people on their phone (they stand in the idle pose without it). */
	UPROPERTY(Config)
	FSoftObjectPath PhoneAnimation;

	/** Walking speed the walk animation was made for (cm/s): play rate = speed / this. */
	UPROPERTY(Config)
	float AnimationWalkSpeed = 140.f;

	UPROPERTY(Config)
	float RunAnimationSpeed = 340.f;

	/** Cost levers, each on by default; -PedCheap=0/1, -PedAnimRate=0/1, -PedSpread=0/1 and -PedMaxVisible=N override them for a measurement. */
	UPROPERTY(Config)
	bool bCheapRendering = true;

	/** Animation updates less often with distance (every frame < 15 m, 1/20 s < 25 m, 1/12 s < 50 m, 1/8 s beyond). */
	UPROPERTY(Config)
	bool bReduceAnimRate = true;

	/** People not drawn are updated 5 times a second instead of every frame. */
	UPROPERTY(Config)
	bool bSpreadFarUpdates = true;

	/** Height of a person varies by up to this fraction around 1. */
	UPROPERTY(Config)
	float HeightSpread = 0.06f;

	/** Vector parameters of the body materials set to a clothing colour (those that exist). */
	UPROPERTY(Config)
	TArray<FName> TintParameters = {FName(TEXT("Tint")), FName(TEXT("BaseColor")), FName(TEXT("Color"))};

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
		/** Index of the partner this person follows (a pair walking together), -1 = walks alone. */
		int32 Leader = -1;
		float FarAccumulated = 0.f;
		GTPWalk::FStepResult LastResult;
		GTPWalk::FPoint Pos;
		GTPWalk::FPoint Dir;
	};

	int32 ChooseDensity() const;
	void RefreshCars();
	void RefreshViewer();
	void RefreshDetail();
	bool IsSeen(const FVector& Location) const;
	bool TrySpawn(FPerson& Person);
	void Activate(FPerson& Person, const GTPWalk::FPoint& P, const GTPWalk::FPoint& D);
	void SeparatePeople();
	void Retire(FPerson& Person);
	void MoveGroundTo(FPerson& Person, bool bSnap);
	void Update(FPerson& Person, float DeltaTime);
	void LogStats();
	bool CarNear(const FVector& Where, float Radius, float MinSpeed, FVector* OutCarLocation = nullptr, FVector* OutCarVelocity = nullptr) const;
	USkeletalMesh* PickMesh();
	static FLinearColor Palette(float Pick);

	/** Best animation of the mesh's skeleton whose name has one of Words (walk, idle...). */
	static UAnimSequenceBase* FindAnimation(const USkeletalMesh* Mesh, const TArray<FString>& Words);

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

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Phone;

};
