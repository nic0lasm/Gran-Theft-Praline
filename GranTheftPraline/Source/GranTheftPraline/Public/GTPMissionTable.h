#pragma once

// Mission tables (ADR-0014): missions, objectives and zones are rows of CSV files in
// Content/Data/Missions/, never code. This header holds the row types, the parser and the
// table checks. No engine types, so tools/missions/mission_test compiles it with g++.
//
// Conventions: first non-comment line is the header, columns are found by name, lines
// starting with '#' and blank lines are skipped, lists use ';', params are key=value pairs
// joined by ';'. Texts are never in these tables, only string keys (strings.tsv).

#include <cmath>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace GTPMission
{
enum class EObjectiveType
{
	/** Be inside a zone (optionally with at most max_stars wanted stars). */
	GoTo,
	/** Be inside the named interior (id of the interior level). */
	EnterInterior,
	/** The named gameplay event arrives (param min = smallest accepted value). */
	WaitEvent,
	/** Be driving the vehicle tagged with the target. The vehicle is bound to the mission. */
	EnterVehicle,
	/** Bring the bound vehicle into a zone, whole enough (min_health), not abandoned. */
	DeliverVehicle,
	/** Wanted stars at 0 for hold_s seconds. */
	LoseWanted,
	Invalid
};

enum class ERetryPolicy
{
	Checkpoint,
	Restart,
	None,
	Invalid
};

using FParams = std::map<std::string, std::string>;

struct FZone
{
	std::string Id;
	/** City local metres: x east, y north (Unreal X = x * 100, Y = -y * 100). */
	float X = 0.f;
	float Y = 0.f;
	float RadiusM = 10.f;
};

struct FObjective
{
	int Index = 0;
	EObjectiveType Type = EObjectiveType::Invalid;
	std::string Target;
	FParams Params;
	std::string HintKey;
	bool bCheckpoint = false;
	float TimeLimit = 0.f;
	std::vector<std::string> FailOn;

	float Param(const std::string& Key, float Default) const
	{
		const auto It = Params.find(Key);
		return It == Params.end() ? Default : static_cast<float>(std::atof(It->second.c_str()));
	}
	std::string ParamStr(const std::string& Key) const
	{
		const auto It = Params.find(Key);
		return It == Params.end() ? std::string() : It->second;
	}
};

struct FMission
{
	std::string Id;
	std::string Kind;
	std::string TitleKey;
	std::string BriefKey;
	std::string StartZone;
	std::string PromptKey;
	bool bRepeatable = false;
	float Cooldown = 0.f;
	int RewardMoney = 0;
	float TimeLimit = 0.f;
	ERetryPolicy Retry = ERetryPolicy::Restart;
	FParams StartEffects;
	std::vector<std::string> Requires;
	std::vector<FObjective> Objectives;
};

struct FTables
{
	std::map<std::string, FZone> Zones;
	std::vector<FMission> Missions;
	std::vector<std::string> Errors;

	const FMission* Find(const std::string& Id) const
	{
		for (const FMission& Mission : Missions)
		{
			if (Mission.Id == Id)
			{
				return &Mission;
			}
		}
		return nullptr;
	}
	const FZone* FindZone(const std::string& Id) const
	{
		const auto It = Zones.find(Id);
		return It == Zones.end() ? nullptr : &It->second;
	}
};

inline std::string Trim(const std::string& Text)
{
	size_t Begin = 0, End = Text.size();
	while (Begin < End && (Text[Begin] == ' ' || Text[Begin] == '\t' || Text[Begin] == '\r'))
	{
		++Begin;
	}
	while (End > Begin && (Text[End - 1] == ' ' || Text[End - 1] == '\t' || Text[End - 1] == '\r'))
	{
		--End;
	}
	return Text.substr(Begin, End - Begin);
}

inline std::vector<std::string> Split(const std::string& Text, char Separator)
{
	std::vector<std::string> Parts;
	std::string Current;
	for (char C : Text)
	{
		if (C == Separator)
		{
			Parts.push_back(Trim(Current));
			Current.clear();
		}
		else
		{
			Current += C;
		}
	}
	Parts.push_back(Trim(Current));
	return Parts;
}

inline std::vector<std::string> SplitList(const std::string& Text)
{
	std::vector<std::string> Out;
	for (const std::string& Part : Split(Text, ';'))
	{
		if (!Part.empty())
		{
			Out.push_back(Part);
		}
	}
	return Out;
}

inline FParams ParseParams(const std::string& Text)
{
	FParams Out;
	for (const std::string& Part : SplitList(Text))
	{
		const size_t Eq = Part.find('=');
		if (Eq == std::string::npos)
		{
			Out[Part] = "";
		}
		else
		{
			Out[Trim(Part.substr(0, Eq))] = Trim(Part.substr(Eq + 1));
		}
	}
	return Out;
}

inline EObjectiveType ParseObjectiveType(const std::string& Text)
{
	if (Text == "GoTo") return EObjectiveType::GoTo;
	if (Text == "EnterInterior") return EObjectiveType::EnterInterior;
	if (Text == "WaitEvent") return EObjectiveType::WaitEvent;
	if (Text == "EnterVehicle") return EObjectiveType::EnterVehicle;
	if (Text == "DeliverVehicle") return EObjectiveType::DeliverVehicle;
	if (Text == "LoseWanted") return EObjectiveType::LoseWanted;
	return EObjectiveType::Invalid;
}

inline ERetryPolicy ParseRetry(const std::string& Text)
{
	if (Text == "checkpoint") return ERetryPolicy::Checkpoint;
	if (Text == "restart") return ERetryPolicy::Restart;
	if (Text == "none") return ERetryPolicy::None;
	return ERetryPolicy::Invalid;
}

/** Rows of a CSV text as column-name maps. Row width must match the header. */
inline std::vector<std::map<std::string, std::string>> ParseRows(const std::string& Csv, const std::string& Name, std::vector<std::string>& Errors)
{
	std::vector<std::map<std::string, std::string>> Rows;
	std::istringstream Stream(Csv);
	std::string Line;
	std::vector<std::string> Header;
	int LineNo = 0;
	while (std::getline(Stream, Line))
	{
		++LineNo;
		const std::string Clean = Trim(Line);
		if (Clean.empty() || Clean[0] == '#')
		{
			continue;
		}
		const std::vector<std::string> Cells = Split(Clean, ',');
		if (Header.empty())
		{
			Header = Cells;
			continue;
		}
		if (Cells.size() != Header.size())
		{
			Errors.push_back(Name + ":" + std::to_string(LineNo) + ": " + std::to_string(Cells.size()) + " columns, expected " + std::to_string(Header.size()));
			continue;
		}
		std::map<std::string, std::string> Row;
		for (size_t I = 0; I < Header.size(); ++I)
		{
			Row[Header[I]] = Cells[I];
		}
		Rows.push_back(Row);
	}
	return Rows;
}

inline bool ParseNumber(const std::string& Text, double& Out)
{
	if (Text.empty())
	{
		return false;
	}
	char* End = nullptr;
	Out = std::strtod(Text.c_str(), &End);
	return End && *End == '\0';
}

/** Parses the three tables and checks them. Every problem is appended to Out.Errors. */
inline void LoadTables(const std::string& MissionsCsv, const std::string& ObjectivesCsv, const std::string& ZonesCsv, FTables& Out)
{
	for (const auto& Row : ParseRows(ZonesCsv, "zones", Out.Errors))
	{
		FZone Zone;
		Zone.Id = Row.at("id");
		double X = 0.0, Y = 0.0, Radius = 0.0;
		if (Zone.Id.empty() || !ParseNumber(Row.at("x_m"), X) || !ParseNumber(Row.at("y_m"), Y) || !ParseNumber(Row.at("radius_m"), Radius) || Radius <= 0.0)
		{
			Out.Errors.push_back("zones: bad row for '" + Zone.Id + "'");
			continue;
		}
		Zone.X = static_cast<float>(X);
		Zone.Y = static_cast<float>(Y);
		Zone.RadiusM = static_cast<float>(Radius);
		if (!Out.Zones.emplace(Zone.Id, Zone).second)
		{
			Out.Errors.push_back("zones: duplicate id " + Zone.Id);
		}
	}

	for (const auto& Row : ParseRows(MissionsCsv, "missions", Out.Errors))
	{
		FMission Mission;
		Mission.Id = Row.at("id");
		Mission.Kind = Row.at("kind");
		Mission.TitleKey = Row.at("title_key");
		Mission.BriefKey = Row.at("brief_key");
		Mission.StartZone = Row.at("start_zone");
		Mission.PromptKey = Row.at("start_prompt_key");
		double Repeatable = 0, Cooldown = 0, Reward = 0, Limit = 0;
		const bool bNumbers = ParseNumber(Row.at("repeatable"), Repeatable) && ParseNumber(Row.at("cooldown_s"), Cooldown) && ParseNumber(Row.at("reward_money"), Reward) && ParseNumber(Row.at("time_limit_s"), Limit);
		if (Mission.Id.empty() || !bNumbers || Cooldown < 0 || Reward < 0 || Limit < 0)
		{
			Out.Errors.push_back("missions: bad numbers for '" + Mission.Id + "'");
			continue;
		}
		Mission.bRepeatable = Repeatable != 0;
		Mission.Cooldown = static_cast<float>(Cooldown);
		Mission.RewardMoney = static_cast<int>(Reward);
		Mission.TimeLimit = static_cast<float>(Limit);
		Mission.Retry = ParseRetry(Row.at("retry"));
		if (Mission.Retry == ERetryPolicy::Invalid)
		{
			Out.Errors.push_back("missions: " + Mission.Id + ": unknown retry '" + Row.at("retry") + "'");
			continue;
		}
		Mission.StartEffects = ParseParams(Row.at("start_effects"));
		Mission.Requires = SplitList(Row.at("requires"));
		if (Out.Find(Mission.Id))
		{
			Out.Errors.push_back("missions: duplicate id " + Mission.Id);
			continue;
		}
		Out.Missions.push_back(Mission);
	}

	std::map<std::string, int> Expected;
	for (const auto& Row : ParseRows(ObjectivesCsv, "objectives", Out.Errors))
	{
		const std::string Id = Row.at("mission_id");
		FMission* Mission = nullptr;
		for (FMission& Candidate : Out.Missions)
		{
			if (Candidate.Id == Id)
			{
				Mission = &Candidate;
			}
		}
		if (!Mission)
		{
			Out.Errors.push_back("objectives: unknown mission " + Id);
			continue;
		}
		FObjective Objective;
		double Index = 0, Checkpoint = 0, Limit = 0;
		if (!ParseNumber(Row.at("index"), Index) || !ParseNumber(Row.at("checkpoint"), Checkpoint) || !ParseNumber(Row.at("time_limit_s"), Limit) || Limit < 0)
		{
			Out.Errors.push_back("objectives: " + Id + ": bad numbers");
			continue;
		}
		Objective.Index = static_cast<int>(Index);
		Objective.Type = ParseObjectiveType(Row.at("type"));
		Objective.Target = Row.at("target");
		Objective.Params = ParseParams(Row.at("params"));
		Objective.HintKey = Row.at("hint_key");
		Objective.bCheckpoint = Checkpoint != 0;
		Objective.TimeLimit = static_cast<float>(Limit);
		Objective.FailOn = SplitList(Row.at("fail_on"));
		if (Objective.Type == EObjectiveType::Invalid)
		{
			Out.Errors.push_back("objectives: " + Id + ": unknown type '" + Row.at("type") + "'");
			continue;
		}
		if (Objective.Index != Expected[Id])
		{
			Out.Errors.push_back("objectives: " + Id + ": index " + std::to_string(Objective.Index) + " out of order, expected " + std::to_string(Expected[Id]));
			continue;
		}
		++Expected[Id];
		Mission->Objectives.push_back(Objective);
	}

	for (const FMission& Mission : Out.Missions)
	{
		if (Mission.Objectives.empty())
		{
			Out.Errors.push_back("missions: " + Mission.Id + " has no objective");
		}
		if (!Out.FindZone(Mission.StartZone))
		{
			Out.Errors.push_back("missions: " + Mission.Id + ": unknown start zone '" + Mission.StartZone + "'");
		}
		for (const std::string& Required : Mission.Requires)
		{
			if (Required == Mission.Id || !Out.Find(Required))
			{
				Out.Errors.push_back("missions: " + Mission.Id + ": bad requires '" + Required + "'");
			}
		}
		for (const FObjective& Objective : Mission.Objectives)
		{
			const bool bZoneTarget = Objective.Type == EObjectiveType::GoTo || Objective.Type == EObjectiveType::DeliverVehicle;
			if (bZoneTarget && !Out.FindZone(Objective.Target))
			{
				Out.Errors.push_back("objectives: " + Mission.Id + "#" + std::to_string(Objective.Index) + ": unknown zone '" + Objective.Target + "'");
			}
			const std::string Search = Objective.ParamStr("search_zone");
			if (!Search.empty() && !Out.FindZone(Search))
			{
				Out.Errors.push_back("objectives: " + Mission.Id + "#" + std::to_string(Objective.Index) + ": unknown search_zone '" + Search + "'");
			}
			if (Objective.Type != EObjectiveType::LoseWanted && Objective.Target.empty())
			{
				Out.Errors.push_back("objectives: " + Mission.Id + "#" + std::to_string(Objective.Index) + ": empty target");
			}
		}
	}
}

/** Every string key used by the tables must exist in the key set; returns the missing keys. */
inline std::vector<std::string> MissingStringKeys(const FTables& Tables, const std::set<std::string>& Known)
{
	std::set<std::string> Missing;
	auto Need = [&](const std::string& Key) {
		if (!Key.empty() && !Known.count(Key))
		{
			Missing.insert(Key);
		}
	};
	for (const FMission& Mission : Tables.Missions)
	{
		Need(Mission.TitleKey);
		Need(Mission.BriefKey);
		Need(Mission.PromptKey);
		for (const FObjective& Objective : Mission.Objectives)
		{
			Need(Objective.HintKey);
		}
	}
	return std::vector<std::string>(Missing.begin(), Missing.end());
}
} // namespace GTPMission
