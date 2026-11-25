// Checks where a police car is made upstream of the player (GTPPoliceUpstream.h).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/police_upstream_test.cpp -o /tmp/police_upstream_test && /tmp/police_upstream_test
#include "GTPPoliceUpstream.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

namespace
{
	struct FLane
	{
		std::vector<float> Xs, Ys;
		std::vector<int> NextLanes;
	};

	struct FLanes
	{
		std::vector<FLane> All;
		int Num() const { return static_cast<int>(All.size()); }
		int NumPoints(int Lane) const { return static_cast<int>(All[Lane].Xs.size()); }
		float X(int Lane, int Point) const { return All[Lane].Xs[Point]; }
		float Y(int Lane, int Point) const { return All[Lane].Ys[Point]; }
		int NumNext(int Lane) const { return static_cast<int>(All[Lane].NextLanes.size()); }
		int Next(int Lane, int Index) const { return All[Lane].NextLanes[Index]; }
	};

	// A lane from (X0, Y0) to (X1, Y1), a point every 5 m or so.
	FLane Straight(float X0, float Y0, float X1, float Y1, std::vector<int> Next)
	{
		FLane Lane;
		const float Length = std::sqrt((X1 - X0) * (X1 - X0) + (Y1 - Y0) * (Y1 - Y0));
		const int Steps = static_cast<int>(Length / 5.f);
		for (int Step = 0; Step <= Steps; ++Step)
		{
			Lane.Xs.push_back(X0 + (X1 - X0) * static_cast<float>(Step) / static_cast<float>(Steps));
			Lane.Ys.push_back(Y0 + (Y1 - Y0) * static_cast<float>(Step) / static_cast<float>(Steps));
		}
		Lane.NextLanes = std::move(Next);
		return Lane;
	}
}

int main()
{
	const auto First = [](int) { return 0; };
	const auto Last = [](int Count) { return Count - 1; };
	const auto Always = [](int, int) { return true; };
	// A quay of three lanes end to end, heading east, 100 m each (0, 1, 2); a side street that joins the quay at
	// 200 m from the north (3); the quay goes on into lane 4, which leads nowhere.
	FLanes Town;
	Town.All.push_back(Straight(0.f, 0.f, 100.f, 0.f, {1}));
	Town.All.push_back(Straight(100.f, 0.f, 200.f, 0.f, {2}));
	Town.All.push_back(Straight(200.f, 0.f, 300.f, 0.f, {4}));
	Town.All.push_back(Straight(200.f, -150.f, 200.f, 0.f, {2}));
	Town.All.push_back(Straight(300.f, 0.f, 400.f, 0.f, {}));
	{
		// The player at 260 m on the quay (lane 2, point 12). The first place 50 m and more behind him is on his own
		// lane, at 210 m: on his road, heading his way.
		GTPPoliceUpstream::FPlace Place;
		CHECK(GTPPoliceUpstream::Find(Town, 2, 12, 260.f, 0.f, 50.f, 120.f, 5.f, 12, First, Always, Place));
		CHECK(Place.Lane == 2 && Place.Point == 2 && std::fabs(Place.Straight - 50.f) < 0.01f && std::fabs(Place.Along - 50.f) < 0.01f);
		// A replacement, 150 m and more: back on the quay by the first lane that leads to his (lane 1, at 110 m)...
		CHECK(GTPPoliceUpstream::Find(Town, 2, 12, 260.f, 0.f, 150.f, 255.f, 5.f, 12, First, Always, Place));
		CHECK(Place.Lane == 1 && Place.Point == 2 && std::fabs(Place.Straight - 150.f) < 0.01f && std::fabs(Place.Along - 150.f) < 0.01f);
		// ... or in the side street, by the other one: 60 m of the quay then 100 m of the street is 160 m of road for
		// 117 m in a straight line, so the place is further up the street.
		CHECK(GTPPoliceUpstream::Find(Town, 2, 12, 260.f, 0.f, 150.f, 255.f, 5.f, 12, Last, Always, Place));
		CHECK(Place.Lane == 3 && Place.Straight >= 150.f && Place.Straight < 156.f && Place.Along > Place.Straight + 30.f);
		// Never at the ends of a lane (its first point, its last two), as in the ring.
		for (int Point = 0; Point < Town.NumPoints(2); ++Point)
		{
			if (GTPPoliceUpstream::Find(Town, 2, Point, Town.X(2, Point), 0.f, 0.f, 1000.f, 0.f, 100, First, Always, Place))
			{
				CHECK(Place.Point >= 1 && Place.Point <= Town.NumPoints(Place.Lane) - 3);
			}
		}
	}
	{
		// The places are refused one after the other (a car on the lane, the place in sight): the next one is at
		// least Spacing further, and the walk gives up after MaxLooks.
		GTPPoliceUpstream::FPlace Place;
		int Looks = 0;
		float LastX = 1000.f;
		bool bSpaced = true;
		const auto Never = [&](int Lane, int Point)
		{
			++Looks;
			bSpaced = bSpaced && LastX - Town.X(Lane, Point) >= 15.f;
			LastX = Town.X(Lane, Point);
			return false;
		};
		CHECK(!GTPPoliceUpstream::Find(Town, 2, 12, 260.f, 0.f, 50.f, 255.f, 15.f, 6, First, Never, Place));
		CHECK(Looks == 6 && bSpaced);
		// The third place asked is taken: 50 m, 70 m (the end of a lane is left out), then 85 m behind him, on the lane before his.
		Looks = 0;
		const auto Third = [&](int, int) { return ++Looks == 3; };
		CHECK(GTPPoliceUpstream::Find(Town, 2, 12, 260.f, 0.f, 50.f, 255.f, 15.f, 6, First, Third, Place));
		CHECK(Place.Lane == 1 && std::fabs(Place.Straight - 85.f) < 0.01f);
	}
	{
		// Nothing leads to the lane the player is on, and he is too near its start: no place upstream.
		GTPPoliceUpstream::FPlace Place;
		CHECK(!GTPPoliceUpstream::Find(Town, 0, 6, 30.f, 0.f, 50.f, 120.f, 5.f, 12, First, Always, Place));
		// A lane that leads nowhere is no place for a car (the player is past the end of lane 4, 60 m of it behind
		// him): the walk goes on to the lane before.
		CHECK(GTPPoliceUpstream::Find(Town, 4, 20, 400.f, 0.f, 50.f, 120.f, 5.f, 12, First, Always, Place));
		CHECK(Place.Lane == 2 && std::fabs(Place.Straight - 110.f) < 0.01f);
		// No lane, no room between the bounds: nothing.
		CHECK(!GTPPoliceUpstream::Find(Town, -1, 0, 0.f, 0.f, 50.f, 120.f, 5.f, 12, First, Always, Place));
		CHECK(!GTPPoliceUpstream::Find(Town, 7, 0, 0.f, 0.f, 50.f, 120.f, 5.f, 12, First, Always, Place));
		CHECK(!GTPPoliceUpstream::Find(Town, 2, 12, 260.f, 0.f, 120.f, 120.f, 5.f, 12, First, Always, Place));
	}
	{
		// A block driven round (four lanes of 40 m in a ring): the walk ends, whatever is asked.
		FLanes Block;
		Block.All.push_back(Straight(0.f, 0.f, 40.f, 0.f, {1}));
		Block.All.push_back(Straight(40.f, 0.f, 40.f, 40.f, {2}));
		Block.All.push_back(Straight(40.f, 40.f, 0.f, 40.f, {3}));
		Block.All.push_back(Straight(0.f, 40.f, 0.f, 0.f, {0}));
		GTPPoliceUpstream::FPlace Place;
		CHECK(!GTPPoliceUpstream::Find(Block, 0, 4, 20.f, 0.f, 80.f, 400.f, 5.f, 1000, First, Always, Place));
		// Across the block, 40 m and more in a straight line: found on the far side, 60 m of road behind him.
		CHECK(GTPPoliceUpstream::Find(Block, 0, 4, 20.f, 0.f, 40.f, 60.f, 5.f, 12, First, Always, Place));
		CHECK(Place.Lane == 3 && Place.Along >= 40.f && Place.Straight >= 40.f);
	}
	{
		// The rolling start: the player's speed, held to the car's chase speed and to 50 km/h; none behind a player
		// who hardly moves.
		CHECK(GTPPoliceUpstream::RollKmh(38.f, 81.f, 50.f) == 38.f);
		CHECK(GTPPoliceUpstream::RollKmh(99.f, 81.f, 50.f) == 50.f);
		CHECK(GTPPoliceUpstream::RollKmh(45.f, 30.f, 50.f) == 30.f);
		CHECK(GTPPoliceUpstream::RollKmh(8.f, 81.f, 50.f) == 0.f);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
