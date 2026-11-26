// Lanes the traffic cannot get along for now, written without engine types so tools/vehicles/traffic_sim runs it as
// the game does.
//
// Gates 103, 104 and 106: once the player stood still at the end of his flight, 0 to 9 cars of 28 moved around him,
// "the queue along the lanes" holding up to 21 of them. On the film of gate 104 the car he stole stands astride the
// middle of a two-lane street: no car of either way can go round it, each one that arrives waits behind the last,
// the queue reaches the junction behind, then the streets that lead to it. Nothing told the cars at that junction
// that the street was shut, and every new car was put on the road with one chance in a few of driving into it:
// tools/vehicles/traffic_sim/standing_sim.cpp counts what that does to the lanes around him, and what is left of it
// once the cars keep off a lane that is held.
//
// A car that can do nothing but wait says so (Report): the one stopped behind a thing that will not move and has no
// room to go round it, or any car stopped behind another for longer than a red light lasts. For HoldSeconds after the
// last word the lane is held: at the junction before it a car takes another exit when there is one (Pick), and no car
// is put on the road there. The cars already on the lane wait as before, in a queue no longer than the lane.
#pragma once

#include <vector>

class FGTPHeldLanes
{
public:
	/** Stopped this long behind another car, a queue or a thing on the road: the lane is held. Longer than a queue
	 *  waits at a red light (22 s with the default timings, and the cars ahead setting off). */
	float ReportAfter = 30.f;
	/** Stopped this long behind a thing that does not move with no room to go round it (the horn has sounded by then). */
	float ReportAfterNoWayRound = 8.f;
	/** A lane stays held this long after the last car said so. */
	float HoldSeconds = 20.f;

	void Reset(int NumLanes)
	{
		Until.assign(static_cast<size_t>(NumLanes > 0 ? NumLanes : 0), -1.0);
		NumReports = 0;
	}

	/** What a stopped car has to say: bBehind, it waits for a car, a queue or a thing on the road (not a light, not a
	 *  junction in use, not a person crossing); bNoWayRound, that thing does not move and the side is not clear. */
	bool ShouldReport(float StoppedSeconds, bool bBehind, bool bNoWayRound) const
	{
		return bBehind && (StoppedSeconds >= ReportAfter || (bNoWayRound && StoppedSeconds >= ReportAfterNoWayRound));
	}

	/** A car on Lane can do nothing but wait. True when the lane was not held until now. */
	bool Report(int Lane, double Now)
	{
		if (Lane < 0 || static_cast<size_t>(Lane) >= Until.size())
		{
			return false;
		}
		const bool bNew = Until[static_cast<size_t>(Lane)] < Now;
		Until[static_cast<size_t>(Lane)] = Now + static_cast<double>(HoldSeconds);
		NumReports += bNew ? 1 : 0;
		return bNew;
	}

	bool IsHeld(int Lane, double Now) const
	{
		return Lane >= 0 && static_cast<size_t>(Lane) < Until.size() && Until[static_cast<size_t>(Lane)] >= Now;
	}

	/**
	 * Held, or leading nowhere but to held lanes: ExitsOf(Lane, Visit) calls Visit(Exit) for each exit of Lane.
	 * A lane with no exit is not shut by this (a dead end is the business of whoever plans the way).
	 */
	template <typename FExitsOf>
	bool IsShut(int Lane, FExitsOf ExitsOf, double Now) const
	{
		if (IsHeld(Lane, Now))
		{
			return true;
		}
		int Exits = 0, HeldExits = 0;
		ExitsOf(Lane, [this, Now, &Exits, &HeldExits](int Exit)
		{
			++Exits;
			HeldExits += IsHeld(Exit, Now) ? 1 : 0;
		});
		return Exits > 0 && HeldExits == Exits;
	}

	int NumHeld(double Now) const
	{
		int Count = 0;
		for (const double Each : Until)
		{
			Count += Each >= Now ? 1 : 0;
		}
		return Count;
	}

	/** Lanes known (Reset). */
	int NumLanes() const { return static_cast<int>(Until.size()); }

	/** Times a lane became held since Reset. */
	int GetNumReports() const { return NumReports; }

	/**
	 * The exit to take among Count: NextAt(I) is the lane of exit I, Shut(Lane) says a lane is not to be taken
	 * (IsHeld or IsShut), Random(N) gives a number from 0 to N - 1. One that is not shut; any of them when all
	 * are (the car has to go somewhere, and will wait there). -1: no exit.
	 */
	template <typename FNextAt, typename FShut, typename FRandom>
	static int Pick(int Count, FNextAt NextAt, FShut Shut, FRandom Random)
	{
		if (Count <= 0)
		{
			return -1;
		}
		int Open = 0;
		for (int Index = 0; Index < Count; ++Index)
		{
			Open += Shut(NextAt(Index)) ? 0 : 1;
		}
		if (Open == 0 || Open == Count)
		{
			return NextAt(Random(Count));
		}
		int Skip = Random(Open);
		for (int Index = 0; Index < Count; ++Index)
		{
			if (!Shut(NextAt(Index)) && Skip-- == 0)
			{
				return NextAt(Index);
			}
		}
		return NextAt(0);
	}

	/** True when a car should plan its way again: the lane it meant to take next is shut and another exit is not. */
	template <typename FNextAt, typename FShut>
	static bool ShouldTurnAway(int NextLane, int Count, FNextAt NextAt, FShut Shut)
	{
		if (!Shut(NextLane))
		{
			return false;
		}
		for (int Index = 0; Index < Count; ++Index)
		{
			if (!Shut(NextAt(Index)))
			{
				return true;
			}
		}
		return false;
	}

private:
	std::vector<double> Until;
	int NumReports = 0;
};
