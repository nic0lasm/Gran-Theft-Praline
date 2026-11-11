// Checks the mission tables and the mission state machine without Unreal (ADR-0014).
// Build: g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/missions/mission_test/mission_test.cpp -o mission_test
// Run:   ./mission_test <Content/Data/Missions> <tools/missions/mission_test/fixtures>
#include "GTPMissionRunner.h"

#include <cstdio>
#include <fstream>
#include <iterator>

using namespace GTPMission;

static int Failures = 0;
#define CHECK(Cond)                                                    \
	do                                                                 \
	{                                                                  \
		if (!(Cond))                                                   \
		{                                                              \
			std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #Cond); \
			++Failures;                                                \
		}                                                              \
	} while (false)

static std::string ReadFile(const std::string& Path)
{
	std::ifstream In(Path);
	if (!In)
	{
		std::printf("FAIL cannot read %s\n", Path.c_str());
		++Failures;
	}
	return std::string(std::istreambuf_iterator<char>(In), std::istreambuf_iterator<char>());
}

static FTables LoadFolder(const std::string& Dir)
{
	FTables Tables;
	LoadTables(ReadFile(Dir + "/missions.csv"), ReadFile(Dir + "/objectives.csv"), ReadFile(Dir + "/zones.csv"), Tables);
	for (const std::string& Error : Tables.Errors)
	{
		std::printf("table error: %s\n", Error.c_str());
	}
	return Tables;
}

static bool Has(const std::vector<FCommand>& Commands, ECommand Kind)
{
	for (const FCommand& C : Commands)
	{
		if (C.Kind == Kind) return true;
	}
	return false;
}
static const FCommand* Get(const std::vector<FCommand>& Commands, ECommand Kind)
{
	for (const FCommand& C : Commands)
	{
		if (C.Kind == Kind) return &C;
	}
	return nullptr;
}

static FContext At(const FTables& T, const std::string& Zone)
{
	FContext Ctx;
	Ctx.X = T.FindZone(Zone)->X;
	Ctx.Y = T.FindZone(Zone)->Y;
	return Ctx;
}

static void TestShippedTables(const std::string& Dir)
{
	const FTables T = LoadFolder(Dir);
	CHECK(T.Errors.empty());
	CHECK(T.Missions.size() == 5);
	CHECK(T.Find("act_holdup_shop") && T.Find("act_order_car") && T.Find("act_lose_cops"));

	std::set<std::string> Known;
	std::istringstream Strings(ReadFile(Dir + "/strings.tsv"));
	std::string Line;
	while (std::getline(Strings, Line))
	{
		if (Line.empty() || Line[0] == '#') continue;
		const std::vector<std::string> Cells = Split(Line, '\t');
		CHECK(Cells.size() == 3 && !Cells[1].empty() && !Cells[2].empty());
		CHECK(Known.insert(Cells[0]).second);
	}
	CHECK(MissingStringKeys(T, Known).empty());
	for (const FMission& M : T.Missions)
	{
		CHECK(M.bRepeatable); // each activity is played three times in a row by QA
		CHECK(M.RewardMoney > 0);
	}
}

static void TestHoldup(const std::string& Dir)
{
	const FTables T = LoadFolder(Dir);
	FRunner R(&T);
	CHECK(R.CannotStartReason("act_holdup_shop").empty());
	CHECK(R.Start("act_holdup_shop"));
	CHECK(R.CannotStartReason("act_order_car") == "busy");
	CHECK(Has(R.TakeCommands(), ECommand::MissionStarted));

	FContext Ctx;
	R.Tick(1.f, Ctx);
	CHECK(R.GetObjectiveIndex() == 0);
	Ctx.Interior = "fastfood_bellecour";
	R.Tick(0.1f, Ctx);
	CHECK(R.GetObjectiveIndex() == 1);
	// An event from another system for another objective does nothing.
	R.OnEvent({"holdup.alarm_raised", "", 0.f});
	CHECK(R.GetObjectiveIndex() == 1);
	R.OnEvent({"holdup.cash_taken", "", 0.f}); // below min=1
	CHECK(R.GetObjectiveIndex() == 1);
	R.OnEvent({"holdup.cash_taken", "", 312.5f});
	CHECK(R.GetObjectiveIndex() == 2);
	R.TakeCommands();

	// Out on the street with police on the player: no success until stars are 0 for 2 s.
	Ctx.Interior.clear();
	Ctx.Stars = 2;
	R.Tick(10.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	Ctx.Stars = 0;
	R.Tick(1.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	Ctx.Stars = 1; // seen again: hold restarts
	R.Tick(1.f, Ctx);
	Ctx.Stars = 0;
	R.Tick(1.5f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	R.Tick(1.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Succeeded);
	const std::vector<FCommand> Out = R.TakeCommands();
	const FCommand* Reward = Get(Out, ECommand::GiveReward);
	CHECK(Reward && Reward->Amount == 150 && Reward->RunKey == "act_holdup_shop#1");
	CHECK(R.SaveDirty());
	CHECK(R.GetCompletions("act_holdup_shop") == 1);
	// Success is paid once: further ticks and events add nothing.
	R.Tick(5.f, Ctx);
	R.OnEvent({"holdup.cash_taken", "", 50.f});
	CHECK(!Has(R.TakeCommands(), ECommand::GiveReward));

	// Repeatable after the cooldown, with a new run key.
	R.Acknowledge();
	CHECK(R.CannotStartReason("act_holdup_shop") == "cooldown");
	R.Tick(61.f, Ctx);
	CHECK(R.Start("act_holdup_shop"));
	R.TakeCommands();
	Ctx.Interior = "fastfood_bellecour";
	R.Tick(0.1f, Ctx);
	R.OnEvent({"holdup.cash_taken", "", 100.f});
	Ctx.Interior.clear();
	R.Tick(3.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Succeeded);
	CHECK(Get(R.TakeCommands(), ECommand::GiveReward)->RunKey == "act_holdup_shop#2");
}

static void TestFailureAndRetry(const std::string& Dir)
{
	const FTables T = LoadFolder(Dir);
	FRunner R(&T);
	R.Start("act_holdup_shop");
	FContext Ctx;
	Ctx.Interior = "fastfood_bellecour";
	R.Tick(0.1f, Ctx);
	R.OnEvent({"holdup.cash_taken", "", 80.f});
	R.TakeCommands();
	R.OnEvent({"player.arrested", "", 0.f});
	CHECK(R.GetStatus() == EStatus::Failed);
	CHECK(R.GetFailReason() == "player.arrested");
	CHECK(Has(R.TakeCommands(), ECommand::MissionFailed));
	CHECK(R.GetCompletions("act_holdup_shop") == 0);
	// Ticks and events do nothing while failed.
	R.Tick(100.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Failed);
	// Retry policy "restart": back to the first objective, no reward ever paid.
	CHECK(R.Retry());
	CHECK(R.GetStatus() == EStatus::Active && R.GetObjectiveIndex() == 0 && R.GetRetries() == 1);
	CHECK(!Has(R.TakeCommands(), ECommand::GiveReward));
	R.Abort();
	CHECK(R.GetStatus() == EStatus::Idle && R.GetActiveId().empty());

	// The escape phase has its own 180 s limit.
	R.Start("act_holdup_shop");
	Ctx.Interior = "fastfood_bellecour";
	R.Tick(0.1f, Ctx);
	R.OnEvent({"holdup.cash_taken", "", 80.f});
	Ctx.Interior.clear();
	Ctx.Stars = 3;
	R.Tick(179.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	R.Tick(2.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Failed && R.GetFailReason() == "time");
}

static void TestCarOrder(const std::string& Dir)
{
	const FTables T = LoadFolder(Dir);
	FRunner R(&T);
	R.Start("act_order_car");
	std::vector<FCommand> Out = R.TakeCommands();
	const FCommand* Waypoint = Get(Out, ECommand::SetWaypoint);
	CHECK(Waypoint && Waypoint->Key == "z_order_search");

	FContext Ctx;
	Ctx.bInVehicle = true;
	Ctx.VehicleTag = "taxi_yellow"; // wrong car
	R.Tick(1.f, Ctx);
	CHECK(R.GetObjectiveIndex() == 0);
	Ctx.VehicleTag = "order_sedan_blue";
	R.Tick(0.1f, Ctx);
	CHECK(R.GetObjectiveIndex() == 1 && R.GetBoundVehicle() == "order_sedan_blue");
	CHECK(Get(R.TakeCommands(), ECommand::SetWaypoint)->Key == "z_garage_fictif");

	// Too damaged at the garage: no delivery.
	Ctx = At(T, "z_garage_fictif");
	Ctx.bInVehicle = true;
	Ctx.VehicleTag = "order_sedan_blue";
	Ctx.VehicleHealth = 0.3f;
	R.Tick(1.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	// A different car destroyed does not matter, the bound one does.
	R.OnEvent({"vehicle.destroyed", "other_car", 0.f});
	CHECK(R.GetStatus() == EStatus::Active);
	Ctx.VehicleHealth = 0.8f;
	R.Tick(1.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Succeeded);
	CHECK(Get(R.TakeCommands(), ECommand::GiveReward)->Amount == 400);

	// Destroyed car: fails, retry restarts from stealing a car.
	R.Acknowledge();
	R.Tick(61.f, Ctx);
	R.Start("act_order_car");
	Ctx = FContext();
	Ctx.bInVehicle = true;
	Ctx.VehicleTag = "order_sedan_blue";
	R.Tick(0.1f, Ctx);
	R.OnEvent({"vehicle.destroyed", "order_sedan_blue", 0.f});
	CHECK(R.GetStatus() == EStatus::Failed && R.GetFailReason() == "vehicle.destroyed");
	CHECK(R.Retry() && R.GetObjectiveIndex() == 0 && R.GetBoundVehicle() == "order_sedan_blue"); // binding is replaced at next EnterVehicle

	// Abandoned: out of the car, over 150 m away for 20 s.
	R.Abort();
	R.Start("act_order_car");
	R.Tick(0.1f, Ctx);
	Ctx.bInVehicle = false;
	Ctx.BoundVehicleDistM = 120.f;
	R.Tick(60.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	Ctx.BoundVehicleDistM = 200.f;
	R.Tick(19.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	R.Tick(2.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Failed && R.GetFailReason() == "vehicle_abandoned");

	// Global time limit of 600 s.
	R.Abort();
	R.Start("act_order_car");
	Ctx = FContext();
	R.Tick(599.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	R.Tick(2.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Failed && R.GetFailReason() == "time");
}

static void TestLoseCops(const std::string& Dir)
{
	const FTables T = LoadFolder(Dir);
	FRunner R(&T);
	R.Start("act_lose_cops");
	const std::vector<FCommand> Out = R.TakeCommands();
	const FCommand* Stars = Get(Out, ECommand::EnsureStars);
	CHECK(Stars && Stars->Amount == 2);

	FContext Ctx = At(T, "z_refuge_terreaux");
	Ctx.Stars = 2;
	R.Tick(5.f, Ctx); // at the refuge but still hunted, and objective 0 is the escape
	CHECK(R.GetObjectiveIndex() == 0);
	Ctx.Stars = 0;
	R.Tick(2.5f, Ctx);
	CHECK(R.GetObjectiveIndex() == 1);
	Ctx.Stars = 1; // seen at the refuge: not complete
	R.Tick(1.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	Ctx.Stars = 0;
	R.Tick(0.1f, Ctx);
	CHECK(R.GetStatus() == EStatus::Succeeded);

	// A retry restarts the chase: stars are raised again.
	R.Acknowledge();
	R.Tick(61.f, Ctx);
	R.Start("act_lose_cops");
	R.TakeCommands();
	R.OnEvent({"player.died", "", 0.f});
	CHECK(R.GetStatus() == EStatus::Failed);
	CHECK(R.Retry());
	CHECK(Get(R.TakeCommands(), ECommand::EnsureStars) != nullptr);
}

static void TestTrialAndCourier(const std::string& Dir)
{
	const FTables T = LoadFolder(Dir);
	FRunner R(&T);
	// Time trial: on foot at a checkpoint does not count, in a car it does, one star at the end is fine, two is not.
	R.Start("act_time_trial");
	FContext Ctx = At(T, "z_trial_cp1");
	R.Tick(1.f, Ctx);
	CHECK(R.GetObjectiveIndex() == 0);
	Ctx.bInVehicle = true;
	R.Tick(1.f, Ctx);
	CHECK(R.GetObjectiveIndex() == 1);
	Ctx = At(T, "z_trial_cp2");
	Ctx.bInVehicle = true;
	R.Tick(1.f, Ctx);
	Ctx = At(T, "z_trial_cp3");
	Ctx.bInVehicle = true;
	Ctx.Stars = 2;
	R.Tick(1.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	Ctx.Stars = 1;
	R.Tick(1.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Succeeded);
	CHECK(Get(R.TakeCommands(), ECommand::GiveReward)->Amount == 300);
	R.Acknowledge();
	R.Tick(61.f, FContext());
	R.Start("act_time_trial");
	R.Tick(151.f, FContext());
	CHECK(R.GetStatus() == EStatus::Failed && R.GetFailReason() == "time");
	R.Abort();

	// Courier on foot: arriving by car does not count.
	R.Start("act_courier");
	Ctx = At(T, "z_garage_fictif");
	Ctx.bInVehicle = true;
	R.Tick(1.f, Ctx);
	CHECK(R.GetObjectiveIndex() == 0);
	Ctx.bInVehicle = false;
	R.Tick(1.f, Ctx);
	CHECK(R.GetObjectiveIndex() == 1);
	R.Tick(1.f, At(T, "z_refuge_terreaux"));
	CHECK(R.GetStatus() == EStatus::Succeeded);
	CHECK(Get(R.TakeCommands(), ECommand::GiveReward)->Amount == 120);
}

static void TestCheckpointsAndGates(const std::string& Dir)
{
	const FTables T = LoadFolder(Dir);
	CHECK(T.Errors.empty());
	FRunner R(&T);
	CHECK(R.CannotStartReason("m_once") == "locked");
	CHECK(R.CannotStartReason("nope") == "unknown");
	CHECK(R.Start("m_long"));
	CHECK(Get(R.TakeCommands(), ECommand::SpawnVehicle)->Extra == "za");
	R.Tick(1.f, At(T, "za"));
	CHECK(R.GetObjectiveIndex() == 1);
	CHECK(Get(R.TakeCommands(), ECommand::Checkpoint) != nullptr);
	// Mission-specific failure event on objective 1.
	R.OnEvent({"ev.bad", "", 0.f});
	CHECK(R.GetStatus() == EStatus::Failed);
	CHECK(R.Retry());
	CHECK(R.GetObjectiveIndex() == 1); // resumed at the checkpoint, not at the start
	CHECK(!Has(R.TakeCommands(), ECommand::SpawnVehicle)); // start effects only for a full restart
	R.OnEvent({"ev.one", "", 5.f});
	CHECK(R.GetObjectiveIndex() == 2);
	// Timer 30 s on the last objective; the mission has 100 s per attempt.
	FContext Far;
	R.Tick(31.f, Far);
	CHECK(R.GetStatus() == EStatus::Failed);
	CHECK(R.Retry() && R.GetObjectiveIndex() == 1); // checkpoint 1 still the last one
	R.OnEvent({"ev.one", "", 9.f});
	FContext Ctx = At(T, "zb");
	Ctx.Stars = 1;
	R.Tick(1.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Active);
	Ctx.Stars = 0;
	R.Tick(1.f, Ctx);
	CHECK(R.GetStatus() == EStatus::Succeeded);
	CHECK(Get(R.TakeCommands(), ECommand::GiveReward)->Amount == 500);
	R.Acknowledge();
	CHECK(R.CannotStartReason("m_long") == "done"); // not repeatable
	CHECK(R.CannotStartReason("m_once").empty());   // unlocked by m_long

	// Retry policy "none".
	R.Start("m_once");
	R.OnEvent({"player.died", "", 0.f});
	CHECK(!R.Retry());
	R.Abort();

	// Cooldown on a repeatable.
	R.Start("m_gate");
	R.OnEvent({"ev.one", "", 0.f});
	R.Acknowledge();
	CHECK(R.CannotStartReason("m_gate") == "cooldown");
	R.Tick(29.f, FContext());
	CHECK(R.CannotStartReason("m_gate") == "cooldown");
	R.Tick(2.f, FContext());
	CHECK(R.CannotStartReason("m_gate").empty());
}

static void TestSave(const std::string& Dir)
{
	const FTables T = LoadFolder(Dir);
	FRunner R(&T);
	R.Start("m_long");
	R.Tick(1.f, At(T, "za"));
	R.OnEvent({"ev.bad", "", 0.f});
	R.Retry(); // retries = 1, checkpoint 1
	FSaveData Data = R.Save();
	CHECK(Data.ActiveMission == "m_long" && Data.CheckpointObjective == 1 && Data.Retries == 1);
	const std::string Text = SerializeSave(Data);

	FSaveData Back;
	CHECK(DeserializeSave(Text, Back));
	FRunner Loaded(&T);
	Loaded.Restore(Back);
	CHECK(Loaded.GetStatus() == EStatus::Active && Loaded.GetObjectiveIndex() == 1 && Loaded.GetRetries() == 1);

	// Completion counts and cooldowns survive, so a reward cannot be paid twice for one run.
	FRunner Done(&T);
	Done.Start("m_gate");
	Done.OnEvent({"ev.one", "", 0.f});
	Done.Acknowledge();
	FSaveData Saved;
	CHECK(DeserializeSave(SerializeSave(Done.Save()), Saved));
	FRunner After(&T);
	After.Restore(Saved);
	CHECK(After.GetCompletions("m_gate") == 1 && After.CannotStartReason("m_gate") == "cooldown");
	After.Tick(31.f, FContext());
	After.Start("m_gate");
	After.TakeCommands();
	After.OnEvent({"ev.one", "", 0.f});
	CHECK(Get(After.TakeCommands(), ECommand::GiveReward)->RunKey == "m_gate#2");

	// Corrupted, truncated, unknown version: refused. Unknown ids and bad indexes: absorbed.
	FSaveData Junk;
	CHECK(!DeserializeSave("", Junk));
	CHECK(!DeserializeSave("v=2\nend\n", Junk));
	CHECK(!DeserializeSave(Text.substr(0, Text.size() - 4), Junk));
	CHECK(!DeserializeSave("v=1\ndone=m_gate:abc\nend\n", Junk));
	CHECK(!DeserializeSave("v=1\nend\nextra=1\n", Junk));
	FSaveData Odd;
	CHECK(DeserializeSave("v=1\ndone=ghost:3\nactive=m_long:99:2\nend\n", Odd));
	FRunner Odds(&T);
	Odds.Restore(Odd);
	CHECK(Odds.GetCompletions("ghost") == 0);
	CHECK(Odds.GetStatus() == EStatus::Active && Odds.GetObjectiveIndex() == 0);
}

static void TestTableErrors()
{
	FTables T;
	LoadTables("id,kind,title_key,brief_key,start_zone,start_prompt_key,repeatable,cooldown_s,reward_money,time_limit_s,retry,start_effects,requires\n"
			   "a,m,t,b,nozone,p,0,0,10,0,restart,,\n"
			   "a,m,t,b,z,p,0,0,10,0,restart,,\n"
			   "c,m,t,b,z,p,0,0,10,0,sometimes,,\n"
			   "d,m,t,b,z,p,0,0,10,0,restart,,ghost\n"
			   "e,m,t,b,z,p,0,0,10,0,restart,,\n",
			   "mission_id,index,type,target,params,hint_key,checkpoint,time_limit_s,fail_on\n"
			   "a,0,GoTo,nozone,,h,0,0,\n"
			   "d,0,Dance,x,,h,0,0,\n"
			   "d,0,WaitEvent,x,,h,0,0,\n"
			   "d,2,WaitEvent,x,,h,0,0,\n"
			   "zzz,0,WaitEvent,x,,h,0,0,\n"
			   "short,row\n",
			   "id,x_m,y_m,radius_m\nz,4.8,45.7,10\nz,4.8,45.7,10\nbad,x,45.7,10\n",
			   T);
	std::string All;
	for (const std::string& E : T.Errors) All += E + "\n";
	CHECK(All.find("duplicate id z") != std::string::npos);
	CHECK(All.find("zones: bad row for 'bad'") != std::string::npos);
	CHECK(All.find("duplicate id a") != std::string::npos);
	CHECK(All.find("unknown retry 'sometimes'") != std::string::npos);
	CHECK(All.find("unknown type 'Dance'") != std::string::npos);
	CHECK(All.find("d: index 2 out of order") != std::string::npos);
	CHECK(All.find("unknown mission zzz") != std::string::npos);
	CHECK(All.find("columns, expected") != std::string::npos);
	CHECK(All.find("e has no objective") != std::string::npos);
	CHECK(All.find("unknown start zone 'nozone'") != std::string::npos);
	CHECK(All.find("bad requires 'ghost'") != std::string::npos);
	CHECK(All.find("unknown zone 'nozone'") != std::string::npos);
}

int main(int Argc, char** Argv)
{
	if (Argc < 3)
	{
		std::printf("usage: mission_test <data dir> <fixtures dir>\n");
		return 2;
	}
	const std::string Data = Argv[1], Fixtures = Argv[2];
	TestShippedTables(Data);
	TestHoldup(Data);
	TestFailureAndRetry(Data);
	TestCarOrder(Data);
	TestLoseCops(Data);
	TestTrialAndCourier(Data);
	TestCheckpointsAndGates(Fixtures);
	TestSave(Fixtures);
	TestTableErrors();
	std::printf(Failures ? "mission_test: %d failure(s)\n" : "mission_test: ok\n", Failures);
	return Failures ? 1 : 0;
}
