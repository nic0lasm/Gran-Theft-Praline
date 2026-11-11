#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPMissionRunner.h"
#include "GTPVehicle.h"
#include "GTPMissionSubsystem.generated.h"


DECLARE_LOG_CATEGORY_EXTERN(LogGTPMission, Log, All);

DECLARE_MULTICAST_DELEGATE_OneParam(FGTPMissionStartAvailable, FName /*MissionId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPMissionObjective, FName /*MissionId*/, FName /*HintKey*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FGTPMissionFailed, FName /*Reason*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPMissionSucceeded, FName /*MissionId*/, int32 /*Reward*/);
/** Economy listens: pay Amount once per RunKey. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPMissionReward, int32 /*Amount*/, FName /*RunKey*/, FName /*MissionId*/);

/**
 * Adapter between the mission state machine (GTPMissionRunner.h, ADR-0014) and the game.
 * It reads the world (player, wanted level, vehicle) without modifying the systems it
 * reads, feeds the runner, and executes its commands. Tables: Content/Data/Missions/.
 *
 * Command line: -Missions=0 turns it all off (no subsystem). -MissionAutoStart=1 starts a
 * mission on entering its start zone (tests; the interface thread offers a prompt otherwise).
 * -MissionStart=<id> [-MissionStartDelay=<s>] starts a mission by itself (scripted scenarios, with -TestDrive).
 * Console: GTPMission.Start <id>, GTPMission.Retry, GTPMission.Abort, GTPMission.TagVehicle <tag>
 * (tags the vehicle the player drives, to test the car order without a spawned target).
 * Log lines "LogGTPMission: mission: ..." follow the soak-log convention.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPMissionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	static bool IsEnabled();
	static UGTPMissionSubsystem* Get(const UObject* WorldContext);

	bool StartMission(FName MissionId);
	bool RetryMission();
	void AbortMission();

	/** Marks the vehicle with the tag a mission looks for. */
	static void TagVehicle(AGTPVehicle* Vehicle, FName Tag);

	/** Reads by the interface: current state. */
	bool HasActiveMission() const { return Runner && Runner->GetStatus() == GTPMission::EStatus::Active; }
	bool HasFailedMission() const { return Runner && Runner->GetStatus() == GTPMission::EStatus::Failed; }
	FName GetActiveMissionId() const { return Runner ? FName(UTF8_TO_TCHAR(Runner->GetActiveId().c_str())) : NAME_None; }
	FName GetCurrentHintKey() const { return CurrentHint; }
	/** Waypoint in Unreal cm; false when there is none. */
	bool GetWaypoint(FVector& OutLocationCm, float& OutRadiusCm) const;
	/** Start zones for the map markers: mission id, centre in Unreal cm, radius in cm. */
	struct FStartMarker
	{
		FName MissionId;
		FName TitleKey;
		FVector LocationCm = FVector::ZeroVector;
		float RadiusCm = 0.f;
		bool bAvailable = false;
	};
	TArray<FStartMarker> GetStartMarkers() const;

	/** Last result, kept until the next Start, Retry or Abort (the HUD draws it while it is there). */
	bool HasResult() const { return !ResultMission.IsNone(); }
	bool ResultIsSuccess() const { return bResultSuccess; }
	FName GetResultMission() const { return ResultMission; }
	/** Success: the reward in euros. Failure: 0. */
	int32 GetResultReward() const { return ResultReward; }
	/** Failure: the reason (player.arrested, time, vehicle.destroyed...), the key is "mission.fail.<reason>". */
	FName GetResultReason() const { return ResultReason; }

	/** Other systems report events here by name (holdup.cash_taken, vehicle.destroyed...). */
	void ReportEvent(FName Name, FName Subject = NAME_None, float Value = 0.f);

	FGTPMissionStartAvailable OnStartAvailable;
	FGTPMissionObjective OnObjective;
	FGTPMissionFailed OnFailed;
	FGTPMissionSucceeded OnSucceeded;
	FGTPMissionReward OnReward;

private:
	bool LoadTables();
	GTPMission::FContext BuildContext();
	void Execute(const std::vector<GTPMission::FCommand>& Commands);
	void LoadSave();
	void WriteSave();
	void BindWorldEvents();
	void SpawnMissionVehicle(FName Tag, const FString& ZoneId);
	void HandleArrested();
	void HandlePlayerDied(const FVector& Where);
	void HandleVehicleLost(AGTPVehicle* Vehicle, EGTPVehicleLoss Loss);
	FVector ToCm(float X, float Y) const { return FVector(X * 100.f, -Y * 100.f, 0.f); }
	static FString SavePath();

	GTPMission::FTables Tables;
	TUniquePtr<GTPMission::FRunner> Runner;
	std::set<std::string> VehicleTags;
	FName CurrentHint;
	FString WaypointZone;
	TWeakObjectPtr<AGTPVehicle> BoundVehicle;
	FName BoundTag;
	bool bAutoStart = false;
	bool bTablesOk = false;
	bool bWantedBound = false;
	bool bCombatBound = false;
	FDelegateHandle WreckHandle;
	/** -MissionStart=<id>: start that mission once the game has run -MissionStartDelay seconds (scripted scenarios). */
	FName ScriptedMission;
	float ScriptedDelay = 6.f;
	float RunTime = 0.f;
	FString InsideStartZone;
	float LogTimer = 0.f;
	FName OfferedMission;
	FName ResultMission;
	FName ResultReason;
	int32 ResultReward = 0;
	bool bResultSuccess = false;
	void ClearResult() { ResultMission = NAME_None; ResultReason = NAME_None; ResultReward = 0; }
};
