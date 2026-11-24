#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPWalkLogic.h"
#include "GTPMassTrafficCars.h"
#include "MassEntityHandle.h"
#include "GTPCrowd.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class AGTPPedestrian;
class UAnimSequenceBase;
class USkeletalMesh;

/**
 * Pedestrian crowd on Mass (ADR-0015, docs/gameplay/pedestrians-mass.md). Hundreds of people are
 * entities that walk the walkway graph with the same engine-free walk logic as the legacy pedestrians
 * (GTPWalkLogic.h), drawn as instances of one static mesh. -Pedestrians=mass turns it on and the legacy
 * pedestrian system off (-Pedestrians=legacy or a number keeps the legacy one).
 */

/** Where a person is on the walkway graph. */
USTRUCT()
struct GTPMASS_API FGTPCrowdWalkerFragment : public FMassFragment
{
	GENERATED_BODY()

	GTPWalk::FWalker Walker;
	FVector Location = FVector::ZeroVector;
	float GroundZ = 0.f;
	float Yaw = 0.f;
	float Speed = 0.f;
};

/** How a person is drawn: the instance index in the crowd mesh and the body scale. */
USTRUCT()
struct GTPMASS_API FGTPCrowdLookFragment : public FMassFragment
{
	GENERATED_BODY()

	int32 InstanceIndex = INDEX_NONE;
	float Scale = 1.f;
	/** Drawn as a full character this frame (the instance is hidden). */
	bool bNear = false;
};

/** Tag of every crowd entity. */
USTRUCT()
struct GTPMASS_API FGTPCrowdTag : public FMassTag
{
	GENERATED_BODY()
};

/** Moves every person along the graph; people left far behind the player are put back around the player. */
UCLASS()
class GTPMASS_API UGTPCrowdMoveProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGTPCrowdMoveProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
	GTPWalk::FRng Rng{20261113u};
};

/** Writes the transform of every person into the instanced mesh. */
UCLASS()
class GTPMASS_API UGTPCrowdRepresentationProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UGTPCrowdRepresentationProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery Query;
};

/** Owns the walkway graph of the crowd, the entities and the instanced mesh. */
UCLASS(Config = Game, DefaultConfig)
class GTPMASS_API UGTPCrowdSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** True with -Pedestrians=mass. */
	static bool IsRequested();

	const GTPWalk::FGraph& GetGraph() const { return Graph; }
	const GTPWalk::FSettings& GetSettings() const { return Settings; }
	FVector GetPlayerLocation() const { return PlayerLocation; }
	float GetRecycleDistance() const { return RecycleDistance; }
	float GetMeshYawOffset() const { return MeshYawOffset; }
	FVector GetBaseScale() const { return BaseScale; }
	float GetMinSpawnDistance() const { return MinSpawnDistance; }
	float GetMaxSpawnDistance() const { return MaxSpawnDistance; }
	UInstancedStaticMeshComponent* GetInstances() const { return Instances; }

	/** Puts the walker on a walkway between the spawn distances from the player; false when none is found. */
	bool PlaceAroundPlayer(GTPWalk::FWalker& Walker, GTPWalk::FRng& Rng) const;

	/** Ground height at a place for somebody just placed there: the surface nearest the player's own ground height. */
	float GroundAt(const FVector& Where) const;

	/**
	 * Ground height under a person who stood at Reference (cm): the town's surfaces are looked for from Up above it
	 * downwards, one component after the other, and GTPWalk::PickGround tells the ground from what stands on it or
	 * hangs over it (a tree, a parasol, a signal pole, a bench). bOutUnderSomething: a surface above was left aside.
	 */
	float GroundUnder(const FVector& Where, float Reference, float Up, bool* bOutUnderSomething = nullptr) const;

	float GetPlayerGroundZ() const { return PlayerGroundZ; }
	float GetGroundStepUp() const { return GroundStepUp; }
	float GetStandInMinDistance() const { return StandInMinDistance; }

	/** From the move processor, once a frame: people standing well above the player's ground near him, and surfaces left aside. */
	void ReportGround(int32 High, float Highest, int32 Under);

	/**
	 * What the cars are to a crossing (an edge of the graph) right now: CrossingHasCarNear when one is about to go over
	 * it, stopped or not, CrossingHasCarDangerous too when one comes too fast to step in front of it (GTPWalk::LookAtCar).
	 * 0 for any other edge, with no car in sight and with -CrowdCrossingWait=0.
	 */
	static constexpr uint8 CrossingHasCarNear = 1;
	static constexpr uint8 CrossingHasCarDangerous = 2;
	uint8 GetCrossingCars(int32 Edge) const { return CrossingCars.IsValidIndex(Edge) ? CrossingCars[Edge] : 0; }

	/** From the move processor, once a frame: who waits at the kerb of a crossing, who is on one, who stopped waiting. */
	void ReportCrossings(int32 Waiting, int32 Crossing, float LongestWait, int32 GaveUp);

	/**
	 * The cars standing or rolling slowly near the player, seen from above, for people to walk round them
	 * (GTPWalk::AvoidBox). Empty with -CrowdAvoidCars=0.
	 */
	const TArray<GTPWalk::FCarBox>& GetStandingCars() const { return StandingCars; }

	/** From the move processor, once a frame: people going round a car, standing in front of one, within the body of one, turned back, and heights refused as ground. */
	void ReportCars(int32 Round, int32 Standing, int32 Inside, int32 TurnedBack, int32 GroundRefused);

protected:
	/** People in the crowd; -PedMass=N overrides. */
	UPROPERTY(Config)
	int32 CrowdCount = 300;

	/** Person drawn as an instance (a static mesh of a walking pose); empty = a capsule stand-in. */
	UPROPERTY(Config)
	FSoftObjectPath CrowdMesh = FSoftObjectPath(TEXT("/Game/Characters/Far/SM_PedFar_A.SM_PedFar_A"));

	UPROPERTY(Config)
	float MinSpawnDistance = 2500.f;

	UPROPERTY(Config)
	float MaxSpawnDistance = 12000.f;

	/** Past this distance from the player a person is moved back around the player (cm). */
	UPROPERTY(Config)
	float RecycleDistance = 15000.f;

	UPROPERTY(Config)
	float HeightSpread = 0.06f;

	/**
	 * Crossings with a car about to go over them are crossed in groups, as at a pedestrian light: people step off the
	 * kerb during the first CrossingWalkSeconds of every CrossingCycleSeconds (each crossing has its own clock) and wait
	 * for the rest, so that the car gets through between two groups. Nobody waits longer than CrossingWaitMaxSeconds,
	 * and a road with no car is crossed at once. -CrowdCrossingWait=0: nobody waits (as before candidate 58).
	 */
	UPROPERTY(Config)
	float CrossingCycleSeconds = 30.f;

	UPROPERTY(Config)
	float CrossingWalkSeconds = 8.f;

	UPROPERTY(Config)
	float CrossingWaitMaxSeconds = 45.f;

	/** What a walking person climbs in one step (cm): a surface higher than that above his ground is not his ground when there is one under it. */
	UPROPERTY(Config)
	float GroundStepUp = 35.f;

	/**
	 * No stand-in (the instanced far mesh) is drawn nearer the player than this (cm): when more people are near him
	 * than there are full characters (NearCount), those left without one are not drawn rather than shown as a still
	 * mannequin a few metres from the camera. 0: stand-ins drawn at any distance, as before candidate 66.
	 */
	UPROPERTY(Config)
	float StandInMinDistance = 2500.f;

	/** People walk round the cars standing within this distance of the player (cm); further away nobody sees them walk through one. */
	UPROPERTY(Config)
	float CarAvoidDistance = 7000.f;

	/**
	 * Hair of the assembled characters (-CrowdHair=0: none, as before candidate 64). City Sample's crowd has no hair
	 * on its heads: its own character code adds a groom bound to the face and paints a baked texture on the scalp.
	 * Ours does the same with what is found under CrowdCharacterRoot: near the player the groom of the style with the
	 * binding Epic made for that face (GroomBindings/GB_<face>_<groom>), as hair cards; further away the hair mesh
	 * City Sample shows from a distance (Hair_*_LOD5); under both the baked scalp texture (Hair_*_baked). A style
	 * whose name contains this word is left to the player.
	 */
	UPROPERTY(Config)
	FString CrowdHairKeptForPlayer = TEXT("PulledBack");

	/** Share of the men left with a bare scalp (0 to 1). */
	UPROPERTY(Config)
	float CrowdBaldShare = 0.1f;

	/**
	 * A person's groom shows and ticks within this distance of the player (cm); beyond it the far hair mesh and the
	 * painted scalp. A groom's tick is its game-thread cost: the distance is the budget's lever. -CrowdGrooms=0: none.
	 */
	UPROPERTY(Config)
	float CrowdGroomDistance = 2000.f;

	/** Grooms as hair cards (the component's bUseCards), and only the grooms that have cards: strands cost several times more. */
	UPROPERTY(Config)
	bool bCrowdGroomCards = true;

	/** The mannequin faces +Y: yaw applied to the instances so they face the walking direction. */
	UPROPERTY(Config)
	float MeshYawOffset = -90.f;

	/** Near people drawn as full characters (the pedestrian actors of the legacy system) and their animations. */
	UPROPERTY(Config)
	int32 NearCount = 36;

	UPROPERTY(Config)
	float NearRadius = 4000.f;

	/** Beyond this distance (cm) a near character casts no shadow and is drawn from NearFarLOD (0 = never). Same values as the legacy pedestrians and the AI cars. */
	UPROPERTY(Config)
	float NearDetailDistance = 2000.f;

	/** Level of detail beyond NearDetailDistance (0-based, clamped to what the body has). */
	UPROPERTY(Config)
	int32 NearFarLOD = 2;

	/** The pose of a near character is refreshed 20 times a second beyond 15 m and 12 times beyond 25 m. */
	UPROPERTY(Config)
	bool bNearReduceAnimRate = true;

	UPROPERTY(Config)
	FSoftObjectPath WalkAnimation = FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Animations/Walk/M_Neutral_Walk_Loop_F_L_20.M_Neutral_Walk_Loop_F_L_20"));

	UPROPERTY(Config)
	FSoftObjectPath IdleAnimation = FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Animations/Idle/M_Relaxed_Stand_Idle_Loop.M_Relaxed_Stand_Idle_Loop"));

	UPROPERTY(Config)
	FSoftObjectPath RunAnimation = FSoftObjectPath(TEXT("/Game/Characters/UEFN_Mannequin/Animations/Run/M_Neutral_Run_Loop_F_R_20.M_Neutral_Run_Loop_F_R_20"));

	UPROPERTY(Config)
	FSoftObjectPath HandsUpAnimation = FSoftObjectPath(TEXT("/Game/Characters/Pedestrians/Anims/A_HandsUp.A_HandsUp"));

	/**
	 * City Sample Crowds characters (copied to the game from the Game Animation Sample project, not in Git):
	 * body, garments and face assembled per person. Empty root = the mannequin of the player.
	 */
	UPROPERTY(Config)
	FString CrowdCharacterRoot = TEXT("/Game/Crowd/Character");

	/** Face animation of the assembled characters. */
	UPROPERTY(Config)
	FSoftClassPath FaceAnimClass = FSoftClassPath(TEXT("/Game/Crowd/Character/Shared/Rig/ABP_CrowdCharacter_Head.ABP_CrowdCharacter_Head_C"));

	/** Walk, idle and run baked for the MetaHuman skeleton (tools/character/retarget_player_animations.py). */
	UPROPERTY(Config)
	FSoftObjectPath CrowdWalkAnimation = FSoftObjectPath(TEXT("/Game/MetaHumans/GTP/Anims/MH_M_Neutral_Walk_Loop_F_L_20.MH_M_Neutral_Walk_Loop_F_L_20"));

	UPROPERTY(Config)
	FSoftObjectPath CrowdIdleAnimation = FSoftObjectPath(TEXT("/Game/MetaHumans/GTP/Anims/MH_M_Relaxed_Stand_Idle_Loop.MH_M_Relaxed_Stand_Idle_Loop"));

	UPROPERTY(Config)
	FSoftObjectPath CrowdRunAnimation = FSoftObjectPath(TEXT("/Game/MetaHumans/GTP/Anims/MH_M_Neutral_Run_Loop_F_R_20.MH_M_Neutral_Run_Loop_F_R_20"));

	/** Hands up baked for the MetaHuman skeleton: retarget_player_animations.py with GTP_RTG_ANIMS=/Game/Characters/Pedestrians/Anims/A_HandsUp. */
	UPROPERTY(Config)
	FSoftObjectPath CrowdHandsUpAnimation = FSoftObjectPath(TEXT("/Game/MetaHumans/GTP/Anims/MH_A_HandsUp.MH_A_HandsUp"));

	/** Folder of the same loops baked on the pack's own skeleton, named CS_<loop> (taken when they exist; empty = never). */
	UPROPERTY(Config)
	FString CrowdSkeletonAnimationFolder = TEXT("/Game/MetaHumans/GTP/AnimsCrowd");

	UPROPERTY(Config)
	float WalkAnimationSpeed = 140.f;

	UPROPERTY(Config)
	float RunAnimationSpeed = 340.f;

private:
	void Spawn(UWorld& InWorld);
	void SetupNearActors(UWorld& InWorld);
	bool DressFromCitySample(AGTPPedestrian* Actor);
	void UpdateNear(float DeltaTime);
	void SpreadPanic();
	void LookAtCars();
	void FindHair();
	void GiveHair(AGTPPedestrian* Actor, bool bFemale);
	void OnLoud(const FVector& Where, float Radius, AActor* Cause);
	void OnActorThreat(AActor* Body, int32 NewState);
	void SayWhatIsPlayed(const AGTPPedestrian* Actor, const UAnimSequenceBase* Asked, const TCHAR* What);
	void HitByPlayerVehicle(int32 Slot);

	/** The player's car this frame, when he drives one fast enough to knock somebody over (above 18 km/h). */
	struct FPlayerCar
	{
		bool bValid = false;
		FVector Centre = FVector::ZeroVector;
		FVector Forward = FVector::ForwardVector;
		float HalfLength = 200.f;
		float HalfWidth = 90.f;
		/** Speed along its own length (cm/s), negative when it backs. */
		float AlongSpeed = 0.f;
	};
	FPlayerCar PlayerCar;
	void ReadPlayerCar();
	/** Does the leading end of the player's car, or a flank, reach a person standing there (GTPImpact::CarReaches)? */
	bool PlayerCarReaches(const FVector& Person) const;
	int32 CarHits = 0;
	FGTPCrowdWalkerFragment* WalkerOf(int32 EntityIndex);
	FGTPCrowdLookFragment* LookOf(int32 EntityIndex);

	GTPWalk::FGraph Graph;
	GTPWalk::FSettings Settings;
	FVector PlayerLocation = FVector::ZeroVector;
	float PlayerGroundZ = 0.f;
	bool bRunning = false;
	FVector BaseScale = FVector(1.f);

	UPROPERTY(Transient)
	TObjectPtr<AActor> InstanceActor;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	FMassArchetypeHandle Archetype;
	TArray<FMassEntityHandle> Entities;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGTPPedestrian>> NearActors;

	/** Entity index served by each near actor (INDEX_NONE = free). */
	TArray<int32> NearEntity;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Walk;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Idle;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Run;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> HandsUp;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> FearRun;
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Cower;

	GTPWalk::FRng ReactionRng{777u};
	bool bAssembled = false;
	TSet<FString> SaidPlayed;
	/** Bodies taken back from a person since the start, and what was reset with them (logged every 10 s). */
	int32 BodiesReleased = 0;
	int32 HandsUpReset = 0;
	int32 FearReset = 0;
	int32 DeadReset = 0;
	double LastReleaseLog = 0.0;
	/** People whose body's fear said "held up" while their walker did not stand: put back standing (logged with the above). */
	int32 HeldPutBack = 0;
	struct FPoseToSay
	{
		TWeakObjectPtr<const AGTPPedestrian> Actor;
		FString What;
		double When = 0.0;
	};
	TArray<FPoseToSay> PoseToSay;
	TArray<uint8> PanicSpread;
	float NextPanicSpread = 0.f;
	/** The crossings of the graph, what the cars are to each (by edge, GetCrossingCars) and the cars last looked at. */
	TArray<int32> CrossingEdges;
	TArray<uint8> CrossingCars;
	TArray<FGTPMassTrafficCar> CarsSeen;
	bool bCrossingWait = true;
	bool bCarsToSee = false;
	float NextCarLook = 0.f;
	/** Now (from the move processor) and since the start, logged every 10 s. */
	int32 WaitingAtKerb = 0;
	int32 OnCrossing = 0;
	float LongestWaitNow = 0.f;
	int32 MostWaitingAtKerb = 0;
	float LongestWaitEver = 0.f;
	int32 GaveUpWaiting = 0;
	int32 CrossingsWithCarNear = 0;
	int32 CrossingsWithCarDangerous = 0;
	int32 CrossingsLookedAt = 0;
	double LastCrossingLog = 0.0;
	/** Cars to walk round (LookAtCars), and what people do about them, now and since the start. */
	TArray<GTPWalk::FCarBox> StandingCars;
	bool bAvoidCars = true;
	int32 GoingRoundCar = 0;
	int32 StandingAtCar = 0;
	int32 InsideCar = 0;
	int32 MostInsideCar = 0;
	int32 TurnedBackAtCar = 0;
	int32 GroundRefusals = 0;
	int32 HighNow = 0;
	int32 MostHigh = 0;
	float HighestNow = 0.f;
	float HighestEver = 0.f;
	int32 UnderSomething = 0;
	int32 WithoutBodyNow = 0;
	int32 MostWithoutBody = 0;
	int32 NotDrawnNow = 0;
	int32 MostNotDrawn = 0;
	/** Hair found in the project, by sex (0 male, 1 female): baked scalp textures and meshes, by style name. */
	struct FHairStyle
	{
		FString Style;
		/** Hair_<size>_<style>: the name of the groom, as the bindings spell it. */
		FString GroomName;
		FSoftObjectPath Baked;
		FSoftObjectPath Mesh;
		FSoftObjectPath Groom;
		bool bMeshIsSkinned = false;
		bool bHasCards = false;
	};
	TArray<FHairStyle> HairStyles[2];
	/** Groom bindings found, by asset name (GB_<face mesh>_<groom>), and the baked eyebrows. */
	TMap<FString, FSoftObjectPath> HairBindings;
	TArray<FSoftObjectPath> EyebrowTextures;
	bool bGrooms = true;
	int32 HairGrooms = 0;
	int32 GroomsShown = 0;
	int32 MostGroomsShown = 0;
	int32 GroomsNotMade = 0;
	int32 BrowsPainted = 0;
	int32 WithoutShoesWomen = 0;
	bool bHair = true;
	bool bHairLooked = false;
	int32 Dressed = 0;
	int32 HairPainted = 0;
	int32 HairMeshes = 0;
	int32 LeftBald = 0;
	int32 NoHeadMaterial = 0;
	int32 WithoutTop = 0;
	int32 WithoutBottom = 0;
	int32 WithoutShoes = 0;
	int32 WithoutFace = 0;
	TSet<FString> HairSaid;
	FDelegateHandle LoudHandle;
	TMap<const AActor*, double> LastLoudBySource;
	double LastLoudLog = -10.0;
};
