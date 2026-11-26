// Checks, outside Unreal, of who crosses a junction (GTPDriveLogic.h) and of the lanes the traffic keeps off
// (GTPTrafficHeld.h): g++ -std=c++17 -I <Public> junction_test.cpp
#include "GTPDriveLogic.h"
#include "GTPTrafficHeld.h"

#include <cstdio>
#include <vector>

static int Failures = 0;

static void Check(bool bOk, const char* What)
{
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", What);
	Failures += bOk ? 0 : 1;
}

int main()
{
	// --- A junction: the cars of one lane follow each other through, the others wait ---
	{
		FGTPJunctionTable Table;
		Check(Table.TryClaim(-1, 1, 5), "no junction at the end of a lane: always free");
		Check(Table.TryClaim(7, 1, 10), "the first car takes the junction");
		Check(Table.TryClaim(7, 1, 10), "and keeps it when it asks again");
		Check(Table.TryClaim(7, 2, 10) && Table.NumCrossing(7) == 2 && Table.NumFollowed() == 1, "the car behind it on the same lane follows it through");
		Check(!Table.TryClaim(7, 3, 20), "a car of another lane waits");
		Check(Table.TryClaim(7, 4, 10) && Table.TryClaim(7, 5, 10) && Table.TryClaim(7, 6, 10), "three more cars of the first lane get through once it waits");
		Check(!Table.TryClaim(7, 8, 10), "the fourth does not");
		for (const int Car : {1, 2, 4, 5, 6})
		{
			Table.Release(7, Car);
		}
		Check(Table.NumCrossing(7) == 0, "the junction empties");
		Check(!Table.TryClaim(7, 8, 10), "the lane that just had it lets the other go first");
		Check(Table.TryClaim(7, 3, 20), "the car that waited takes it");
		Check(!Table.TryClaim(7, 8, 10), "and the first lane waits in its turn");
		Table.ReleaseAll(3);
		Check(Table.TryClaim(7, 8, 10), "then goes");
	}
	{
		// Nobody comes to take its turn: the lane is not kept waiting.
		FGTPJunctionTable Table;
		Table.TryClaim(7, 1, 10);
		Table.TryClaim(7, 3, 20);
		Table.Release(7, 1);
		int Asks = 0;
		while (!Table.TryClaim(7, 2, 10) && Asks < 1000)
		{
			++Asks;
		}
		Check(Asks == Table.YieldAsks, "the car that waited has gone: the lane takes the junction again after a moment");
	}
	{
		FGTPJunctionTable Table;
		Table.bPlatoons = false;
		Check(Table.TryClaim(7, 1, 10) && !Table.TryClaim(7, 2, 10), "-TrafficJunctionFollow=0: one car at a time, as until gate 106");
		Table.Release(7, 1);
		Check(Table.TryClaim(7, 2, 10), "and the next one as soon as it is free");
	}
	{
		FGTPJunctionTable Table;
		Check(Table.TryClaim(7, 1) && !Table.TryClaim(7, 2), "a caller that names no lane gets one car at a time");
	}

	// --- A car that stands in a queue before a junction gives it back ---
	{
		FGTPJunctionTable Table;
		FGTPJunctionClaims Queued, Crossing;
		const FGTPDrivePoint End{0.f, 0.f};
		// 8 m before the end of its lane, slow: it asks for the junction and gets it.
		Check(Queued.Update(Table, 1, 7, End, -8.f, 0.f, 2.f, 3.5f, 0.f, 10, false) < 0.f && Table.NumCrossing(7) == 1, "a car that comes up to a junction takes it");
		Check(Crossing.Update(Table, 2, 7, {0.f, 0.f}, 0.f, -8.f, 2.f, 3.5f, 0.f, 20, false) >= 0.f, "a car of the other street gets a stop line");
		// It stops behind the queue of its own way: 1 s, it still holds; past HeldUpSeconds, it gives the junction back.
		Check(Queued.Update(Table, 1, 7, End, -6.f, 0.f, 0.f, 3.5f, 1.f, 10, true) < 0.f && Table.NumCrossing(7) == 1, "stopped 1 s behind a queue it keeps it");
		Check(Queued.Update(Table, 1, 7, End, -6.f, 0.f, 0.f, 3.5f, 2.5f, 10, true) < 0.f && Table.NumCrossing(7) == 0 && Table.NumGivenBack() == 1,
			"stopped 2.5 s behind a queue it gives it back");
		Check(Crossing.Update(Table, 2, 7, {0.f, 0.f}, 0.f, -8.f, 0.f, 3.5f, 3.f, 20, false) < 0.f, "and the car of the other street crosses");
		Check(Queued.Update(Table, 1, 7, End, -6.f, 0.f, 0.f, 3.5f, 9.f, 10, true) < 0.f && Table.NumGivenBack() == 1, "it does not ask while it stands (and is counted once)");
		// The queue moves: it asks again, and waits for the car that crosses.
		Check(Queued.Update(Table, 1, 7, End, -6.f, 0.f, 0.5f, 3.5f, 0.f, 10, true) >= 0.f, "it moves again: it asks, and waits for the one that crosses");
		Crossing.Clear(Table, 2);
		Check(Queued.Update(Table, 1, 7, End, -6.f, 0.f, 0.5f, 3.5f, 0.f, 10, false) < 0.f && Table.NumCrossing(7) == 1, "then takes the junction");
	}
	{
		FGTPJunctionTable Table;
		FGTPJunctionClaims Queued;
		Queued.HeldUpSeconds = -1.f;
		Queued.Update(Table, 1, 7, {0.f, 0.f}, -8.f, 0.f, 2.f, 3.5f, 0.f, 10, false);
		Queued.Update(Table, 1, 7, {0.f, 0.f}, -6.f, 0.f, 0.f, 3.5f, 60.f, 10, true);
		Check(Table.NumCrossing(7) == 1, "HeldUpSeconds < 0: a car in a queue keeps its junction, as until gate 106");
	}
	{
		// A car stopped at the line by the junction itself (taken by another) is not behind something: it goes on asking.
		FGTPJunctionTable Table;
		FGTPJunctionClaims First, Second;
		First.Update(Table, 1, 7, {0.f, 0.f}, -8.f, 0.f, 2.f, 3.5f, 0.f, 10, false);
		Check(Second.Update(Table, 2, 7, {0.f, 0.f}, 0.f, -5.f, 0.f, 3.5f, 20.f, 20, false) >= 0.f, "a car that waits for the junction itself goes on asking for it");
	}

	// --- Lanes the traffic keeps off ---
	{
		FGTPHeldLanes Held;
		Held.Reset(6);
		Check(!Held.ShouldReport(29.f, true, false) && Held.ShouldReport(30.f, true, false), "30 s behind a car or a queue: the lane is held (a red light keeps a queue 22 s)");
		Check(!Held.ShouldReport(7.f, true, true) && Held.ShouldReport(8.f, true, true), "8 s behind a thing it cannot go round");
		Check(!Held.ShouldReport(300.f, false, false) && !Held.ShouldReport(300.f, false, true), "never at a red light or a junction in use, however long");
		Check(Held.Report(2, 100.0) && !Held.Report(2, 105.0) && Held.GetNumReports() == 1, "a lane becomes held once while a car goes on saying so");
		Check(Held.IsHeld(2, 124.9) && !Held.IsHeld(2, 125.1), "20 s after the last word it is free again");
		Check(!Held.Report(-1, 100.0) && !Held.Report(6, 100.0) && !Held.IsHeld(6, 100.0), "a lane the file does not have is never held");

		// Lane 0 leads to 1 and 2; 1 leads to 3; 2 leads to 4; 5 leads nowhere.
		const std::vector<std::vector<int>> Next = {{1, 2}, {3}, {4}, {}, {}, {}};
		auto ExitsOf = [&Next](int Lane, const auto& Visit)
		{
			for (const int Exit : Next[static_cast<size_t>(Lane)])
			{
				Visit(Exit);
			}
		};
		Held.Reset(6);
		Held.Report(3, 0.0);
		Check(Held.IsShut(3, ExitsOf, 1.0) && Held.IsShut(1, ExitsOf, 1.0), "a lane that leads nowhere but to a held lane is shut too");
		Check(!Held.IsShut(0, ExitsOf, 1.0) && !Held.IsShut(2, ExitsOf, 1.0) && !Held.IsShut(5, ExitsOf, 1.0), "not the one two junctions back, nor a dead end");
		auto Shut = [&](int Lane) { return Held.IsShut(Lane, ExitsOf, 1.0); };
		auto ExitOf0 = [&Next](int Index) { return Next[0][static_cast<size_t>(Index)]; };
		bool bAlwaysOpen = true;
		for (int Draw = 0; Draw < 8; ++Draw)
		{
			bAlwaysOpen = bAlwaysOpen && FGTPHeldLanes::Pick(2, ExitOf0, Shut, [Draw](int Count) { return Draw % Count; }) == 2;
		}
		Check(bAlwaysOpen, "at the junction before, a car takes the exit that is not shut, whatever is drawn");
		Check(FGTPHeldLanes::ShouldTurnAway(1, 2, ExitOf0, Shut) && !FGTPHeldLanes::ShouldTurnAway(2, 2, ExitOf0, Shut), "a car that meant to take the shut one plans again; the other does not");
		Held.Report(4, 0.0);
		Check(FGTPHeldLanes::Pick(2, ExitOf0, Shut, [](int) { return 0; }) == 1 && FGTPHeldLanes::Pick(2, ExitOf0, Shut, [](int Count) { return Count - 1; }) == 2,
			"every exit shut: any of them, as drawn");
		Check(!FGTPHeldLanes::ShouldTurnAway(1, 2, ExitOf0, Shut), "and no car plans again for nothing");
		Check(FGTPHeldLanes::Pick(0, ExitOf0, Shut, [](int) { return 0; }) == -1, "no exit: none");
		Check(Held.NumHeld(1.0) == 2 && Held.NumHeld(30.0) == 0, "held lanes are counted, and forgotten");
	}

	std::printf(Failures == 0 ? "PASS\n" : "FAIL (%d)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
