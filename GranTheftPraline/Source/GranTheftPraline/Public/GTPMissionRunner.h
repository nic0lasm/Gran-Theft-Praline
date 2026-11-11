#pragma once

// Mission state machine (ADR-0014). Pure logic: the world feeds it a context every tick
// and discrete events as they happen, it answers with commands that an adapter executes
// (show text, set waypoint, pay, make police stars...). It never calls another system,
// which is how it consumes Police, Economy, Combat and Traffic without modifying them.
// One active mission at a time in 0.9.
//
// Life of a mission: Idle -> Active -> Succeeded | Failed. Failed waits for Retry()
// (resume at the last checkpoint, or restart, or nothing, by the mission's retry policy)
// or Abort(). Reward and completion are one atomic step: GiveReward carries a RunKey
// (mission id # completion count) the economy can use to refuse a second payment, and
// SaveDirty() tells the save thread to write at once.

#include "GTPMissionTable.h"

#include <algorithm>

namespace GTPMission
{
enum class EStatus
{
	Idle,
	Active,
	Succeeded,
	Failed
};

/** What the world looks like this tick. Distances in metres, coordinates in local metres. */
struct FContext
{
	float X = 0.f;
	float Y = 0.f;
	bool bInVehicle = false;
	/** Tag of the vehicle the player drives (empty on foot). */
	std::string VehicleTag;
	/** 0..1 */
	float VehicleHealth = 1.f;
	/** Distance from the player to the mission vehicle, -1 when unknown. */
	float BoundVehicleDistM = -1.f;
	int Stars = 0;
	/** Id of the interior the player is in, empty outside. */
	std::string Interior;
};

struct FEvent
{
	std::string Name;
	/** Who it concerns, e.g. a vehicle tag. */
	std::string Subject;
	float Value = 0.f;
};

enum class ECommand
{
	MissionStarted,
	ShowObjective,
	SetWaypoint,
	ClearWaypoint,
	/** Make the player wanted: Amount stars at least (through the police API, ReportCrime). */
	EnsureStars,
	/** Make sure a vehicle tagged Key exists near zone Extra. */
	SpawnVehicle,
	Checkpoint,
	GiveReward,
	MissionSucceeded,
	MissionFailed
};

struct FCommand
{
	ECommand Kind = ECommand::MissionStarted;
	std::string MissionId;
	/** String key (ShowObjective), zone id (SetWaypoint), vehicle tag, fail reason. */
	std::string Key;
	std::string Extra;
	int Amount = 0;
	/** GiveReward: unique per successful run, "<mission>#<completion count>". */
	std::string RunKey;
};

/** Everything that survives a save. */
struct FSaveData
{
	std::map<std::string, int> Completions;
	std::map<std::string, float> CooldownLeft;
	std::string ActiveMission;
	int CheckpointObjective = 0;
	int Retries = 0;
};

class FRunner
{
public:
	explicit FRunner(const FTables* InTables) : Tables(InTables) {}

	EStatus GetStatus() const { return Status; }
	const std::string& GetActiveId() const { return ActiveId; }
	int GetObjectiveIndex() const { return ObjectiveIndex; }
	int GetRetries() const { return Retries; }
	const std::string& GetFailReason() const { return FailReason; }
	const std::string& GetBoundVehicle() const { return BoundVehicle; }
	int GetCompletions(const std::string& Id) const { const auto It = Completions.find(Id); return It == Completions.end() ? 0 : It->second; }
	bool SaveDirty() const { return bSaveDirty; }
	void ClearSaveDirty() { bSaveDirty = false; }

	std::vector<FCommand> TakeCommands()
	{
		std::vector<FCommand> Out;
		Out.swap(Commands);
		return Out;
	}

	/** Why a mission cannot start now, empty when it can. */
	std::string CannotStartReason(const std::string& Id) const
	{
		const FMission* Mission = Tables->Find(Id);
		if (!Mission) return "unknown";
		if (Status == EStatus::Active) return "busy";
		if (!Mission->bRepeatable && GetCompletions(Id) > 0) return "done";
		const auto Cool = CooldownLeft.find(Id);
		if (Cool != CooldownLeft.end() && Cool->second > 0.f) return "cooldown";
		for (const std::string& Required : Mission->Requires)
		{
			if (GetCompletions(Required) == 0) return "locked";
		}
		return std::string();
	}

	bool Start(const std::string& Id)
	{
		if (!CannotStartReason(Id).empty())
		{
			return false;
		}
		ActiveId = Id;
		Retries = 0;
		BoundVehicle.clear();
		BeginAttempt(0, true);
		return true;
	}

	/** After Failed: resume per the retry policy. False when the policy forbids it. */
	bool Retry()
	{
		if (Status != EStatus::Failed)
		{
			return false;
		}
		const FMission* Mission = Tables->Find(ActiveId);
		if (!Mission || Mission->Retry == ERetryPolicy::None)
		{
			return false;
		}
		++Retries;
		const int From = Mission->Retry == ERetryPolicy::Checkpoint ? CheckpointObjective : 0;
		BeginAttempt(From, From == 0);
		return true;
	}

	/** Gives up the mission (active or failed). Nothing is paid. */
	void Abort()
	{
		if (Status == EStatus::Active || Status == EStatus::Failed)
		{
			Emit(ECommand::ClearWaypoint, std::string());
			Status = EStatus::Idle;
			ActiveId.clear();
		}
	}

	/** The success screen was dismissed. A failure is resolved with Retry or Abort. */
	void Acknowledge()
	{
		if (Status == EStatus::Succeeded)
		{
			Status = EStatus::Idle;
			ActiveId.clear();
		}
	}

	void Tick(float Dt, const FContext& Ctx)
	{
		for (auto& Pair : CooldownLeft)
		{
			Pair.second = std::max(0.f, Pair.second - Dt);
		}
		if (Status != EStatus::Active)
		{
			return;
		}
		const FMission* Mission = Tables->Find(ActiveId);
		if (!Mission)
		{
			return;
		}
		Elapsed += Dt;
		ObjectiveElapsed += Dt;
		const FObjective& Objective = Mission->Objectives[ObjectiveIndex];

		if (Mission->TimeLimit > 0.f && Elapsed > Mission->TimeLimit)
		{
			Fail("time");
			return;
		}
		if (Objective.TimeLimit > 0.f && ObjectiveElapsed > Objective.TimeLimit)
		{
			Fail("time");
			return;
		}
		if (!BoundVehicle.empty() && Objective.Type == EObjectiveType::DeliverVehicle)
		{
			const float AbandonM = Objective.Param("abandon_m", 0.f);
			const float AbandonS = Objective.Param("abandon_s", 0.f);
			const bool bAway = !Ctx.bInVehicle && AbandonM > 0.f && Ctx.BoundVehicleDistM > AbandonM;
			AwayTime = bAway ? AwayTime + Dt : 0.f;
			if (bAway && AwayTime >= AbandonS)
			{
				Fail("vehicle_abandoned");
				return;
			}
		}
		if (Satisfied(Objective, Ctx, Dt))
		{
			Advance();
		}
	}

	void OnEvent(const FEvent& Event)
	{
		if (Status != EStatus::Active)
		{
			return;
		}
		const FMission* Mission = Tables->Find(ActiveId);
		if (!Mission)
		{
			return;
		}
		const FObjective& Objective = Mission->Objectives[ObjectiveIndex];
		if (Event.Name == "player.arrested" || Event.Name == "player.died")
		{
			Fail(Event.Name);
			return;
		}
		for (const std::string& Rule : Objective.FailOn)
		{
			const size_t At = Rule.find('@');
			const std::string Name = Rule.substr(0, At);
			if (Name != Event.Name)
			{
				continue;
			}
			if (At != std::string::npos && Rule.substr(At + 1) == "bound" && Event.Subject != BoundVehicle)
			{
				continue;
			}
			Fail(Event.Name);
			return;
		}
		if (Objective.Type == EObjectiveType::WaitEvent && Event.Name == Objective.Target && Event.Value >= Objective.Param("min", 0.f))
		{
			Advance();
		}
	}

	FSaveData Save() const
	{
		FSaveData Data;
		Data.Completions = Completions;
		Data.CooldownLeft = CooldownLeft;
		if (Status == EStatus::Active || Status == EStatus::Failed)
		{
			Data.ActiveMission = ActiveId;
			Data.CheckpointObjective = CheckpointObjective;
			Data.Retries = Retries;
		}
		return Data;
	}

	/** Restores a save. A mission that was running resumes at its checkpoint. Unknown ids are dropped. */
	void Restore(const FSaveData& Data)
	{
		Completions.clear();
		CooldownLeft.clear();
		for (const auto& Pair : Data.Completions)
		{
			if (Tables->Find(Pair.first) && Pair.second > 0)
			{
				Completions[Pair.first] = Pair.second;
			}
		}
		for (const auto& Pair : Data.CooldownLeft)
		{
			if (Tables->Find(Pair.first) && Pair.second > 0.f)
			{
				CooldownLeft[Pair.first] = Pair.second;
			}
		}
		Status = EStatus::Idle;
		ActiveId.clear();
		Commands.clear();
		const FMission* Mission = Tables->Find(Data.ActiveMission);
		if (Mission)
		{
			ActiveId = Mission->Id;
			Retries = Data.Retries;
			BoundVehicle.clear();
			const int Checkpoint = (Data.CheckpointObjective >= 0 && Data.CheckpointObjective < static_cast<int>(Mission->Objectives.size())) ? Data.CheckpointObjective : 0;
			BeginAttempt(Checkpoint, false);
		}
	}

private:
	void Emit(ECommand Kind, const std::string& Key, const std::string& Extra = std::string(), int Amount = 0, const std::string& RunKey = std::string())
	{
		FCommand Command;
		Command.Kind = Kind;
		Command.MissionId = ActiveId;
		Command.Key = Key;
		Command.Extra = Extra;
		Command.Amount = Amount;
		Command.RunKey = RunKey;
		Commands.push_back(Command);
	}

	void BeginAttempt(int FromObjective, bool bFirstStart)
	{
		const FMission* Mission = Tables->Find(ActiveId);
		Status = EStatus::Active;
		Elapsed = 0.f;
		AwayTime = 0.f;
		FailReason.clear();
		ObjectiveIndex = FromObjective;
		CheckpointObjective = FromObjective;
		if (bFirstStart)
		{
			Emit(ECommand::MissionStarted, Mission->TitleKey);
		}
		// Start effects run each time the mission restarts from its first objective.
		if (FromObjective == 0)
		{
			for (const auto& Effect : Mission->StartEffects)
			{
				if (Effect.first == "ensure_stars")
				{
					Emit(ECommand::EnsureStars, std::string(), std::string(), std::atoi(Effect.second.c_str()));
				}
				else if (Effect.first == "spawn_vehicle")
				{
					const size_t At = Effect.second.find('@');
					Emit(ECommand::SpawnVehicle, Effect.second.substr(0, At), At == std::string::npos ? std::string() : Effect.second.substr(At + 1));
				}
			}
		}
		BeginObjective();
	}

	void BeginObjective()
	{
		const FMission* Mission = Tables->Find(ActiveId);
		const FObjective& Objective = Mission->Objectives[ObjectiveIndex];
		ObjectiveElapsed = 0.f;
		HoldTime = 0.f;
		AwayTime = 0.f;
		if (Objective.bCheckpoint && ObjectiveIndex > CheckpointObjective)
		{
			CheckpointObjective = ObjectiveIndex;
			Emit(ECommand::Checkpoint, std::string(), std::string(), ObjectiveIndex);
		}
		Emit(ECommand::ShowObjective, Objective.HintKey, std::string(), ObjectiveIndex);
		const std::string Zone = (Objective.Type == EObjectiveType::GoTo || Objective.Type == EObjectiveType::DeliverVehicle) ? Objective.Target : Objective.ParamStr("search_zone");
		if (!Zone.empty())
		{
			Emit(ECommand::SetWaypoint, Zone);
		}
		else
		{
			Emit(ECommand::ClearWaypoint, std::string());
		}
	}

	bool InZone(const std::string& ZoneId, const FContext& Ctx) const
	{
		const FZone* Zone = Tables->FindZone(ZoneId);
		if (!Zone)
		{
			return false;
		}
		const float DX = Ctx.X - Zone->X, DY = Ctx.Y - Zone->Y;
		return DX * DX + DY * DY <= Zone->RadiusM * Zone->RadiusM;
	}

	bool Satisfied(const FObjective& Objective, const FContext& Ctx, float Dt)
	{
		switch (Objective.Type)
		{
		case EObjectiveType::GoTo:
			return InZone(Objective.Target, Ctx) && Ctx.Stars <= static_cast<int>(Objective.Param("max_stars", 5.f));
		case EObjectiveType::EnterInterior:
			return !Ctx.Interior.empty() && Ctx.Interior == Objective.Target;
		case EObjectiveType::EnterVehicle:
			if (Ctx.bInVehicle && Ctx.VehicleTag == Objective.Target)
			{
				BoundVehicle = Objective.Target;
				return true;
			}
			return false;
		case EObjectiveType::DeliverVehicle:
			return Ctx.bInVehicle && Ctx.VehicleTag == BoundVehicle && Ctx.VehicleHealth >= Objective.Param("min_health", 0.f) && InZone(Objective.Target, Ctx);
		case EObjectiveType::LoseWanted:
			HoldTime = Ctx.Stars == 0 ? HoldTime + Dt : 0.f;
			return Ctx.Stars == 0 && HoldTime >= Objective.Param("hold_s", 0.f);
		case EObjectiveType::WaitEvent:
		case EObjectiveType::Invalid:
			break;
		}
		return false;
	}

	void Advance()
	{
		const FMission* Mission = Tables->Find(ActiveId);
		++ObjectiveIndex;
		if (ObjectiveIndex < static_cast<int>(Mission->Objectives.size()))
		{
			BeginObjective();
			return;
		}
		ObjectiveIndex = static_cast<int>(Mission->Objectives.size()) - 1;
		const int Count = ++Completions[ActiveId];
		if (Mission->Cooldown > 0.f)
		{
			CooldownLeft[ActiveId] = Mission->Cooldown;
		}
		Status = EStatus::Succeeded;
		bSaveDirty = true;
		Emit(ECommand::ClearWaypoint, std::string());
		if (Mission->RewardMoney > 0)
		{
			Emit(ECommand::GiveReward, std::string(), std::string(), Mission->RewardMoney, ActiveId + "#" + std::to_string(Count));
		}
		Emit(ECommand::MissionSucceeded, Mission->TitleKey, std::string(), Mission->RewardMoney);
	}

	void Fail(const std::string& Reason)
	{
		FailReason = Reason;
		Status = EStatus::Failed;
		Emit(ECommand::ClearWaypoint, std::string());
		Emit(ECommand::MissionFailed, Reason);
	}

	const FTables* Tables = nullptr;
	EStatus Status = EStatus::Idle;
	std::string ActiveId;
	int ObjectiveIndex = 0;
	int CheckpointObjective = 0;
	int Retries = 0;
	float Elapsed = 0.f;
	float ObjectiveElapsed = 0.f;
	float HoldTime = 0.f;
	float AwayTime = 0.f;
	std::string BoundVehicle;
	std::string FailReason;
	std::map<std::string, int> Completions;
	std::map<std::string, float> CooldownLeft;
	std::vector<FCommand> Commands;
	bool bSaveDirty = false;
};

/** Plain text form of the save data (one key=value per line, versioned, strict on read). */
inline std::string SerializeSave(const FSaveData& Data)
{
	std::ostringstream Out;
	Out << "v=1\n";
	for (const auto& Pair : Data.Completions) Out << "done=" << Pair.first << ":" << Pair.second << "\n";
	for (const auto& Pair : Data.CooldownLeft) Out << "cool=" << Pair.first << ":" << Pair.second << "\n";
	if (!Data.ActiveMission.empty()) Out << "active=" << Data.ActiveMission << ":" << Data.CheckpointObjective << ":" << Data.Retries << "\n";
	Out << "end\n";
	return Out.str();
}

/** False on any malformed or truncated input; Out is left empty then. */
inline bool DeserializeSave(const std::string& Text, FSaveData& Out)
{
	FSaveData Data;
	std::istringstream Stream(Text);
	std::string Line;
	bool bVersion = false, bEnd = false;
	while (std::getline(Stream, Line))
	{
		if (bEnd) return false;
		if (Line == "v=1") { bVersion = true; continue; }
		if (Line == "end") { bEnd = true; continue; }
		const size_t Eq = Line.find('=');
		if (!bVersion || Eq == std::string::npos) return false;
		const std::string Key = Line.substr(0, Eq);
		const std::vector<std::string> Parts = Split(Line.substr(Eq + 1), ':');
		double A = 0, B = 0;
		if ((Key == "done" || Key == "cool") && Parts.size() == 2 && !Parts[0].empty() && ParseNumber(Parts[1], A) && A >= 0)
		{
			if (Key == "done") Data.Completions[Parts[0]] = static_cast<int>(A);
			else Data.CooldownLeft[Parts[0]] = static_cast<float>(A);
		}
		else if (Key == "active" && Parts.size() == 3 && !Parts[0].empty() && ParseNumber(Parts[1], A) && ParseNumber(Parts[2], B) && A >= 0 && B >= 0)
		{
			Data.ActiveMission = Parts[0];
			Data.CheckpointObjective = static_cast<int>(A);
			Data.Retries = static_cast<int>(B);
		}
		else
		{
			return false;
		}
	}
	if (!bVersion || !bEnd) return false;
	Out = Data;
	return true;
}
} // namespace GTPMission
