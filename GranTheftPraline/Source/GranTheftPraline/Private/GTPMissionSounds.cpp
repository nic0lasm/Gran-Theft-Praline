#include "GTPMissionSounds.h"

#include "Engine/World.h"
#include "GTPMissionSubsystem.h"
#include "GTPPlaceAudio.h"

namespace
{
	constexpr float MissionLookupInterval = 0.5f;
	/** After this many lookups (10 s) without a subsystem, missions are off: stop looking. */
	constexpr int32 MaxLookups = 20;
	constexpr float RewardDelay = 1.1f;
}

void UGTPMissionSounds::Start(UWorld* World)
{
	WorldPtr = World;
	Bind();
}

void UGTPMissionSounds::Bind()
{
	if (MissionsPtr.IsValid() || Lookups >= MaxLookups)
	{
		return;
	}
	++Lookups;
	UGTPMissionSubsystem* Missions = UGTPMissionSubsystem::Get(WorldPtr.Get());
	if (!Missions)
	{
		return;
	}
	MissionsPtr = Missions;
	AvailableHandle = Missions->OnStartAvailable.AddUObject(this, &UGTPMissionSounds::OnStartAvailable);
	ObjectiveHandle = Missions->OnObjective.AddUObject(this, &UGTPMissionSounds::OnObjective);
	FailedHandle = Missions->OnFailed.AddUObject(this, &UGTPMissionSounds::OnFailed);
	SucceededHandle = Missions->OnSucceeded.AddUObject(this, &UGTPMissionSounds::OnSucceeded);
}

void UGTPMissionSounds::Stop()
{
	if (UGTPMissionSubsystem* Missions = MissionsPtr.Get())
	{
		Missions->OnStartAvailable.Remove(AvailableHandle);
		Missions->OnObjective.Remove(ObjectiveHandle);
		Missions->OnFailed.Remove(FailedHandle);
		Missions->OnSucceeded.Remove(SucceededHandle);
	}
	MissionsPtr.Reset();
	WorldPtr.Reset();
	RewardLeft = -1.f;
}

void UGTPMissionSounds::Tick(float DeltaTime)
{
	if (const UGTPMissionSubsystem* Missions = MissionsPtr.Get())
	{
		// Aborted or retried runs: the next objective starts a new run.
		if (!Missions->HasActiveMission())
		{
			RunMission = NAME_None;
		}
	}
	else
	{
		LookupLeft -= DeltaTime;
		if (LookupLeft <= 0.f)
		{
			LookupLeft = MissionLookupInterval;
			Bind();
		}
	}
	if (RewardLeft >= 0.f)
	{
		RewardLeft -= DeltaTime;
		if (RewardLeft < 0.f)
		{
			UGTPPlaceAudio::PlayMission(WorldPtr.Get(), EGTPMissionSound::Reward);
		}
	}
}

void UGTPMissionSounds::OnStartAvailable(FName MissionId)
{
	// Broadcast once each time the player enters a start zone.
	if (!RunMission.IsNone())
	{
		return;
	}
	UGTPPlaceAudio::PlayMission(WorldPtr.Get(), EGTPMissionSound::Available);
}

void UGTPMissionSounds::OnObjective(FName MissionId, FName HintKey)
{
	if (MissionId != RunMission)
	{
		RunMission = MissionId;
		LastHint = HintKey;
		++Starts;
		UGTPPlaceAudio::PlayMission(WorldPtr.Get(), EGTPMissionSound::Start);
		return;
	}
	if (HintKey == LastHint)
	{
		return;
	}
	LastHint = HintKey;
	++Objectives;
	UGTPPlaceAudio::PlayMission(WorldPtr.Get(), EGTPMissionSound::Objective);
}

void UGTPMissionSounds::OnFailed(FName /*Reason*/)
{
	++Fails;
	RunMission = NAME_None;
	RewardLeft = -1.f;
	UGTPPlaceAudio::PlayMission(WorldPtr.Get(), EGTPMissionSound::Fail);
}

void UGTPMissionSounds::OnSucceeded(FName /*MissionId*/, int32 Reward)
{
	++Successes;
	RunMission = NAME_None;
	UGTPPlaceAudio::PlayMission(WorldPtr.Get(), EGTPMissionSound::Success);
	RewardLeft = Reward > 0 ? RewardDelay : -1.f;
}

FString UGTPMissionSounds::Describe() const
{
	if (!MissionsPtr.IsValid())
	{
		return TEXT("missions=off");
	}
	return FString::Printf(TEXT("missions start %d objective %d success %d fail %d"), Starts, Objectives, Successes, Fails);
}
