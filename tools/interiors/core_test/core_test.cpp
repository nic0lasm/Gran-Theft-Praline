// Checks of the till and hold-up rules (GTPCashDrawerModel.h, GTPHoldUpModel.h), no engine needed:
//   g++ -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/interiors/core_test/core_test.cpp -o core_test && ./core_test
#include "GTPCashDrawerModel.h"
#include "GTPCustomerQueue.h"
#include "GTPHoldUpModel.h"
#include "GTPHoldUpTills.h"
#include "GTPInteriorStreamingCore.h"
#include "GTPKitchenModel.h"
#include "GTPWalletModel.h"

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

static void WalletChecks()
{
	GTPWallet::FWallet Wallet;
	Check(Wallet.Add(15000, "act_holdup_shop#1") && Wallet.Cents == 15000, "a keyed reward is paid");
	Check(!Wallet.Add(15000, "act_holdup_shop#1") && Wallet.Cents == 15000, "the same key is never paid twice");
	Check(!Wallet.Add(-20000) && Wallet.Cents == 15000, "spending more than the balance is refused");
	Check(Wallet.Add(-5000) && Wallet.Cents == 10000, "spending lowers the balance");
	GTPWallet::FWallet Loaded;
	Check(Loaded.Load(Wallet.Save()) && Loaded.Cents == 10000 && Loaded.WasPaid("act_holdup_shop#1"), "save and load keep balance and paid keys");
	Check(!Loaded.Load("abc") && !Loaded.Load("") && !Loaded.Load("-5"), "a broken save is refused");
}

static void InteriorIdChecks()
{
	using namespace GTPInterior;
	Check(IdFromLevel("/Game/Interiors/L_Interior_fastfood_bellecour") == "fastfood_bellecour", "interior id from the level path");
	Check(IdFromLevel("/Game/Interiors/L_Interior_fastfood_bellecour.L_Interior_fastfood_bellecour") == "fastfood_bellecour", "interior id from an object path");
	Check(Inside(950, -800, 90, 1900, 1600, 720), "middle of the restaurant is inside");
	Check(!Inside(950, 20, 90, 1900, 1600, 720) && !Inside(950, -30, 90, 1900, 1600, 720), "the pavement and the doorway are outside");
	Check(!Inside(950, -800, 900, 1900, 1600, 720), "above the roof is outside");
}

static GTPHoldUpTills::FRun MakeShop()
{
	using namespace GTPHoldUpTills;
	FRun Run;
	Run.Staff.resize(5);
	Run.Staff[0].Till = 0;
	Run.Staff[0].bCanReachAlarm = true;
	Run.Staff[1].Till = 1;
	Run.Staff[1].bCanReachAlarm = true;
	Run.Begin(2, 11);
	return Run;
}

static bool HasTill(const GTPHoldUpTills::FRun& Run, EEvent Kind)
{
	for (const FEventRecord& Event : Run.Events)
	{
		if (Event.Kind == Kind)
		{
			return true;
		}
	}
	return false;
}

static void TillChecks()
{
	using namespace GTPHoldUpTills;
	FRun Calm = MakeShop();
	for (int Step = 0; Step < 100; ++Step)
	{
		Calm.Step(0.1, false);
	}
	Check(!Calm.bActive && Calm.Events.empty(), "no threat: nothing happens");
	const int64_t Before = Calm.CashLeft();
	Check(Before > 20000, "two tills mid-service hold more than 200 euros");

	// Both cashiers covered and obeying, the crew hands up: two tills emptied, no alarm.
	FRun Shop = MakeShop();
	for (FMember& Member : Shop.Staff)
	{
		Member.State = EThreat::HandsUp;
		Member.SecondsSinceAimed = 0.5;
	}
	Shop.Staff[0].State = EThreat::Complying;
	Shop.Staff[1].State = EThreat::Complying;
	for (int Step = 0; Step < 80; ++Step)
	{
		Shop.Step(0.1, true);
	}
	Check(Shop.bActive && Shop.TillsEmptied == 2 && Shop.Loot > 0, "two obeying cashiers empty both tills in 8 s");
	Check(!HasTill(Shop, EEvent::AlarmPressed) && !Shop.bCrimeReported, "covered staff do not press the alarm");
	Check(Shop.CashLeft() > 0 && Shop.CashLeft() < Before, "coins stay in the drawers");
	Check(Shop.End(false) == EOutcome::Escaped && Shop.End(false) == EOutcome::None, "leaving with cash is an escape, reported once");

	// The second cashier is not covered: alarm, then the police 20 s later.
	FRun Alarm = MakeShop();
	Alarm.Staff[0].State = EThreat::Complying;
	Alarm.Staff[0].SecondsSinceAimed = 0.2;
	Alarm.Staff[1].State = EThreat::Alarmed;
	Alarm.Staff[1].SecondsSinceAimed = 10.0;
	for (int Step = 0; Step < 100; ++Step)
	{
		Alarm.Step(0.1, true);
	}
	Check(HasTill(Alarm, EEvent::AlarmPressed) && !Alarm.bCrimeReported, "an uncovered cashier presses the silent alarm");
	for (int Step = 0; Step < 200; ++Step)
	{
		Alarm.Step(0.1, true);
	}
	Check(Alarm.bCrimeReported, "the monitoring centre reports the crime 20 s later");

	// A cashier stops obeying while opening: the till stays closed. A cook escapes: crime reported.
	FRun Broken = MakeShop();
	Broken.Staff[0].State = EThreat::Complying;
	Broken.Staff[0].SecondsSinceAimed = 0.0;
	Broken.Staff[1].SecondsSinceAimed = 0.0;
	Broken.Staff[1].State = EThreat::HandsUp;
	for (int Step = 0; Step < 10; ++Step)
	{
		Broken.Step(0.1, true);
	}
	Broken.Staff[0].State = EThreat::Fleeing;
	Broken.Staff[3].State = EThreat::Fleeing;
	Broken.Staff[3].bReachedExit = true;
	for (int Step = 0; Step < 60; ++Step)
	{
		Broken.Step(0.1, true);
	}
	Check(Broken.TillsEmptied == 0 && Broken.Tills[0].Phase == ETillPhase::Closed, "a cashier who runs off leaves the till closed");
	Check(HasTill(Broken, EEvent::StaffEscaped) && Broken.bCrimeReported, "staff out of the back door call the police");
	Check(Broken.End(true) == EOutcome::Caught, "arrested before leaving: caught");
	FRun Empty = MakeShop();
	Empty.Staff[2].State = EThreat::Alarmed;
	Empty.Step(0.1, true);
	Check(Empty.End(false) == EOutcome::Aborted, "leaving without cash: aborted");
}

static void CustomerChecks()
{
	using namespace GTPCustomers;
	FRun Run;
	Run.Begin({4, 4, 4}, 7, 3);
	Check(Run.Count() == 7, "customers: the shop starts with MaxPeople");
	Check(Run.Lines[0].People.size() == 3 && Run.Lines[2].People.size() == 2, "customers: spread over the lines, shortest first");
	Run.Events.clear();
	for (int Step = 0; Step < 200; ++Step)
	{
		Run.Step(0.1, true);
	}
	Check(Run.Events.empty() && Run.Count() == 7, "customers: frozen during a hold-up");
	int Served = 0;
	int Arrived = 0;
	bool bInOrder = true;
	for (int Step = 0; Step < 6000; ++Step)   // 10 minutes
	{
		const size_t Before = Run.Events.size();
		Run.Step(0.1, false);
		for (size_t Index = Before; Index < Run.Events.size(); ++Index)
		{
			const FEvent& Event = Run.Events[Index];
			Served += Event.Kind == ECustomerEvent::Served ? 1 : 0;
			Arrived += Event.Kind == ECustomerEvent::Arrived ? 1 : 0;
			bInOrder = bInOrder && Event.Index < Run.Lines[size_t(Event.Line)].Spots;
		}
		if (Run.Count() > 7)
		{
			bInOrder = false;
		}
	}
	Check(bInOrder, "customers: never more than MaxPeople, never past the last spot");
	Check(Served >= 60 && Served <= 160, "customers: about one served per line every 12-24 s");
	Check(Run.Count() >= 4, "customers: the lines stay busy");
	Check(Arrived >= Served - 7, "customers: newcomers replace those served");
	FRun Small;
	Small.Begin({2}, 9, 1);
	Check(Small.MaxPeople == 2 && Small.Count() == 2, "customers: no more than the spots");
}

int main()
{
	CashChecks();
	HoldUpChecks();
	StreamingChecks();
	KitchenChecks();
	WalletChecks();
	InteriorIdChecks();
	TillChecks();
	CustomerChecks();
	std::printf(Failures == 0 ? "all checks passed\n" : "%d check(s) failed\n", Failures);
	return Failures == 0 ? 0 : 1;
}
