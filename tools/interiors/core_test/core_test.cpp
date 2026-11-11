// Checks of the till and hold-up rules (GTPCashDrawerModel.h, GTPHoldUpModel.h), no engine needed:
//   g++ -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/interiors/core_test/core_test.cpp -o core_test && ./core_test
#include "GTPCashDrawerModel.h"
#include "GTPHoldUpModel.h"
#include "GTPInteriorStreamingCore.h"
#include "GTPKitchenModel.h"

#include <cstdio>

static int Failures = 0;

static void Check(bool bOk, const char* What)
{
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", What);
	if (!bOk)
	{
		++Failures;
	}
}

using namespace GTPCash;
using namespace GTPHoldUp;

static void CashChecks()
{
	const FDrawer Float = StandardFloat();
	Check(Float.Total() == 15000, "standard float is 150.00 euros");
	Check(Float.NotesTotal() == 9000, "float holds 90 euros in notes");

	FDrawer Drawer = StandardFloat();
	Check(GiveChange(Drawer, 873), "change of 8.73 can be made from the float");
	Check(Drawer.Total() == 15000 - 873, "change leaves the drawer by its exact value");

	FDrawer Empty;
	Check(!GiveChange(Empty, 5), "no change from an empty drawer");
	Check(Empty.Total() == 0, "failed change leaves the drawer untouched");

	FDrawer Pennies;
	Pennies.Count[NumDenoms - 1] = 3;
	Check(!GiveChange(Pennies, 5) && Pennies.Count[NumDenoms - 1] == 3, "partial change is refused, nothing taken");

	FRng RngA(7), RngB(7);
	bool bSame = true;
	for (int Index = 0; Index < 1000; ++Index)
	{
		bSame = bSame && RngA.Next() == RngB.Next();
	}
	Check(bSame, "same seed, same sequence");

	FRng Rng(3);
	bool bCovers = true;
	for (int Index = 0; Index < 2000; ++Index)
	{
		const int64_t Price = 100 + Rng.Range(9000);
		const auto Paid = Tender(Price, Rng);
		int64_t Total = 0;
		for (int D = 0; D < NumDenoms; ++D)
		{
			Total += Denoms[size_t(D)] * Paid[size_t(D)];
		}
		bCovers = bCovers && Total >= Price && Paid[0] == 0 && Paid[1] == 0;
	}
	Check(bCovers, "a customer always hands over enough, never a 100 or 200 note");

	const FShiftResult Morning = SimulateShift(11, 1.0);
	const FShiftResult Evening = SimulateShift(11, 6.0);
	Check(Morning.Drawer.Total() > 15000, "one hour of service adds cash to the float");
	Check(Morning.CashSales > 3 && Morning.CardSales > Morning.CashSales, "most customers pay by card");
	Check(Evening.Skims >= 1, "six hours of service trigger a skim to the safe");
	Check(Evening.Drawer.NotesTotal() <= FShiftParams().SkimAbove, "notes in the till stay under the skim ceiling");

	const FShiftResult Again = SimulateShift(11, 6.0);
	Check(Again.Drawer.Count == Evening.Drawer.Count, "a shift replays identically from its seed");

	bool bPlausible = true;
	int Failed = 0;
	for (uint64_t Seed = 1; Seed <= 200; ++Seed)
	{
		const FShiftResult Shift = SimulateShift(Seed, 1.0 + double(Seed % 10));
		const int64_t Total = Shift.Drawer.Total();
		bPlausible = bPlausible && Total >= FShiftParams().SkimKeep && Total <= FShiftParams().SkimAbove + 15000;
		Failed += Shift.ChangeFailures;
	}
	Check(bPlausible, "over 200 shifts a till holds 100 to 550 euros");
	Check(Failed == 0, "with restocks from the safe, change can always be given");

	FDrawer Robbed = SimulateShift(5, 3.0).Drawer;
	const int64_t Coins = Robbed.Total() - Robbed.NotesTotal();
	const int64_t Notes = Grab(Robbed, false);
	Check(Notes > 0 && Robbed.NotesTotal() == 0 && Robbed.Total() == Coins, "a quick grab takes every note and leaves the coins");
	std::printf("     sample till after 3 h: %lld.%02lld euros in notes, %lld.%02lld in coins\n",
		(long long)(Notes / 100), (long long)(Notes % 100), (long long)(Coins / 100), (long long)(Coins % 100));
}

static FHoldUp MakeCrew(uint64_t Seed)
{
	FHoldUp HoldUp;
	FStaff Cashier1; Cashier1.Role = ERole::Cashier; Cashier1.Till = 0;
	FStaff Cashier2; Cashier2.Role = ERole::Cashier; Cashier2.Till = 1;
	FStaff Cook1; Cook1.ExitDistance = 6.0;
	FStaff Cook2; Cook2.ExitDistance = 6.0;
	FStaff Manager; Manager.Role = ERole::Manager; Manager.bCanReachAlarm = true;
	HoldUp.Staff = {Cashier1, Cashier2, Cook1, Cook2, Manager};
	HoldUp.Tills = {SimulateShift(Seed, 2.0).Drawer, SimulateShift(Seed + 1, 2.0).Drawer};
	HoldUp.Begin(Seed);
	return HoldUp;
}

static void Run(FHoldUp& HoldUp, double Seconds, const std::vector<FThreat>& Threats)
{
	const double Dt = 1.0 / 30.0;
	for (double T = 0.0; T < Seconds; T += Dt)
	{
		HoldUp.Step(Dt, Threats);
	}
}

static bool Has(const FHoldUp& HoldUp, EEvent Kind)
{
	for (const FEventRecord& Event : HoldUp.Events)
	{
		if (Event.Kind == Kind)
		{
			return true;
		}
	}
	return false;
}

static void HoldUpChecks()
{
	{
		FHoldUp HoldUp = MakeCrew(21);
		Run(HoldUp, 10.0, {});
		Check(HoldUp.CountIn(EState::Working) == 5 && HoldUp.Events.empty(), "no threat: everyone keeps working");
	}
	{
		FHoldUp HoldUp = MakeCrew(21);
		const int64_t InTill = HoldUp.Tills[0].NotesTotal();
		FThreat Aim; Aim.bAimedAt = true; Aim.bWeaponSeen = true;
		FThreat Seen; Seen.bWeaponSeen = true;
		Run(HoldUp, 1.0, {Aim, Seen, Seen, Seen, Seen});
		Check(HoldUp.Staff[0].State == EState::HandsUp, "the cashier covered by the weapon raises their hands");
		FThreat Order = Aim; Order.bOrderedCash = true;
		HoldUp.Shout();
		Run(HoldUp, 8.0, {Order, Seen, Seen, Seen, Seen});
		Check(Has(HoldUp, EEvent::CashHandedOver) && HoldUp.Loot == InTill, "ordered at gunpoint, the cashier hands over the notes of their till");
		Check(HoldUp.Tills[1].NotesTotal() > 0, "the other till is untouched until its cashier is covered");
	}
	{
		// Player covers one cashier only, never shouts, for a long time: someone acts.
		FHoldUp HoldUp = MakeCrew(4);
		FThreat Aim; Aim.bAimedAt = true; Aim.bWeaponSeen = true;
		FThreat Seen; Seen.bWeaponSeen = true;
		Run(HoldUp, 1.0, {Aim, Seen, Seen, Seen, Seen});
		Run(HoldUp, 40.0, {Aim, FThreat(), FThreat(), FThreat(), FThreat()});
		Check(Has(HoldUp, EEvent::AlarmPressed) || Has(HoldUp, EEvent::StaffEscaped), "uncovered staff press the alarm or slip out");
		Check(HoldUp.bCrimeReported, "the crime ends up reported to the police");
	}
	{
		FHoldUp HoldUp = MakeCrew(9);
		HoldUp.Staff[4].bCanReachAlarm = false;
		for (FStaff& Member : HoldUp.Staff) { Member.Courage = 0.0; }
		FThreat Aim; Aim.bAimedAt = true; Aim.bWeaponSeen = true;
		FThreat Seen; Seen.bWeaponSeen = true;
		Run(HoldUp, 2.0, {Aim, Seen, Seen, Seen, Seen});
		Run(HoldUp, 20.0, {Aim, Seen, Seen, Seen, Seen});
		Check(!HoldUp.bCrimeReported && HoldUp.CountIn(EState::Gone) == 0, "a room under control: nobody escapes, nothing reported yet");
	}
	{
		FHoldUp A = MakeCrew(33), B = MakeCrew(33);
		FThreat Aim; Aim.bAimedAt = true; Aim.bWeaponSeen = true; Aim.bOrderedCash = true;
		Run(A, 30.0, {Aim, FThreat(), FThreat(), FThreat(), FThreat()});
		Run(B, 30.0, {Aim, FThreat(), FThreat(), FThreat(), FThreat()});
		bool bSame = A.Events.size() == B.Events.size() && A.Loot == B.Loot;
		for (size_t Index = 0; bSame && Index < A.Events.size(); ++Index)
		{
			bSame = A.Events[Index].Kind == B.Events[Index].Kind && A.Events[Index].Staff == B.Events[Index].Staff;
		}
		Check(bSame, "same seed and inputs: same hold-up, event for event");
	}
}

static void StreamingChecks()
{
	using namespace GTPInterior;
	FStreamingRules Rules;  // 40 m in, 70 m out, 2 at most
	std::vector<FSite> Sites = {{0.f, 0.f, 0.f, false}, {10000.f, 0.f, 0.f, false}, {2000.f, 0.f, 0.f, false}, {3000.f, 0.f, 0.f, false}};
	std::vector<bool> Wanted = Plan(Sites, 0.f, 0.f, 0.f, Rules);
	Check(Wanted[0] && !Wanted[1], "the interior at the door loads, the far one does not");
	Check(Wanted[2] && !Wanted[3], "at most two interiors, nearest first");
	Sites.resize(2);  // only the restaurant and a far one from here
	Sites[0].bLoaded = true;
	Check(Plan(Sites, 5500.f, 0.f, 0.f, Rules)[0], "a loaded interior stays loaded at 55 m (hysteresis)");
	Sites[0].bLoaded = false;
	Check(!Plan(Sites, 5500.f, 0.f, 0.f, Rules)[0], "an unloaded interior does not load at 55 m");
	Sites[0].bLoaded = true;
	Check(!Plan(Sites, 7100.f, 0.f, 0.f, Rules)[0], "past 70 m it is released");
	Check(DefaultDensity(true) == 2 && DefaultDensity(false) == 3, "density 2 on m1, 3 elsewhere");
	Check(ClampDensity(9) == 3 && ClampDensity(-1) == 0, "density clamped to 0..3");
	Check(ShownAt(0, 0) && ShownAt(2, 3) && !ShownAt(3, 2), "a component shows from its own density up");
}

static GTPKitchen::FKitchen RunKitchen(int Crew, int PerHour, uint64_t Seed, double Hours)
{
	GTPKitchen::FKitchen Kitchen;
	Kitchen.Params.CustomersPerHour = PerHour;
	Kitchen.Staff = GTPKitchen::FKitchen::DefaultCrew(Crew);
	Kitchen.Begin(Seed);
	for (int Step = 0; Step < int(Hours * 3600.0 * 30.0); ++Step)
	{
		Kitchen.Step(1.0 / 30.0);
	}
	return Kitchen;
}

static void KitchenChecks()
{
	using namespace GTPKitchen;
	const FKitchen Evening = RunKitchen(8, 120, 7, 1.0);
	std::printf("     crew of 8, 120 customers/h: %d served, wait %.0f s on average, %.0f s at most\n", Evening.Served, Evening.AverageWait(), Evening.MaxWait);
	Check(Evening.Served >= 100, "a crew of 8 serves the evening rush (120 customers an hour)");
	Check(Evening.AverageWait() < 180.0 && Evening.MaxWait < 300.0, "orders ready in under 3 minutes on average, 5 at most");
	bool bAllTasks = true;
	for (ETask Task : {ETask::TakeOrder, ETask::FryDrop, ETask::FryLift, ETask::FryBag, ETask::GrillLoad, ETask::GrillUnload, ETask::Assemble, ETask::PourDrink, ETask::BagOrder})
	{
		bool bSeen = false;
		for (const FTaskEvent& Event : Evening.Events)
		{
			bSeen = bSeen || Event.Task == Task;
		}
		bAllTasks = bAllTasks && bSeen;
	}
	Check(bAllTasks, "every station task happens (each one drives an animation and moves objects)");
	const FKitchen Short = RunKitchen(2, 120, 7, 1.0);
	Check(Short.AverageWait() > 600.0 && Short.Orders.size() > 20, "two people cannot hold the rush: the queue grows");
	const FKitchen Quiet = RunKitchen(4, 30, 3, 2.0);
	Check(Quiet.FriesWasted + Quiet.PattiesWasted > 0, "a quiet afternoon throws away food held too long");
	const FKitchen Again = RunKitchen(8, 120, 7, 1.0);
	Check(Again.Served == Evening.Served && Again.Events.size() == Evening.Events.size() && Again.TotalWait == Evening.TotalWait,
	      "same seed, same service");
}

int main()
{
	CashChecks();
	HoldUpChecks();
	StreamingChecks();
	KitchenChecks();
	std::printf(Failures == 0 ? "all checks passed\n" : "%d check(s) failed\n", Failures);
	return Failures == 0 ? 0 : 1;
}
