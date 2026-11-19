// Checks the way a chasing car takes on the lanes to its target (GTPLaneRoute.h).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/lane_route_test.cpp -o /tmp/lane_route_test && /tmp/lane_route_test
#include "GTPLaneRoute.h"

#include <cmath>
#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

namespace
{
	struct FLane
	{
		float Ax, Ay, Bx, By;
		std::vector<int> Next;
	};

	float ToSegment(float X, float Y, const FLane& L)
	{
		const float Dx = L.Bx - L.Ax, Dy = L.By - L.Ay;
		const float L2 = Dx * Dx + Dy * Dy;
		const float T = L2 > 0.f ? std::min(1.f, std::max(0.f, ((X - L.Ax) * Dx + (Y - L.Ay) * Dy) / L2)) : 0.f;
		return std::hypot(X - (L.Ax + Dx * T), Y - (L.Ay + Dy * T));
	}

	std::vector<int> Way(const std::vector<FLane>& Lanes, int Start, float CarX, float CarY, float GoalX, float GoalY)
	{
		// What is left of Start ahead of the car: from the car to the lane's end.
		const FLane Left = {CarX, CarY, Lanes[static_cast<std::size_t>(Start)].Bx, Lanes[static_cast<std::size_t>(Start)].By, {}};
		return GTPLaneRoute::Toward(Start, static_cast<int>(Lanes.size()), ToSegment(GoalX, GoalY, Left),
			[&Lanes](int Lane) -> const std::vector<int>& { return Lanes[static_cast<std::size_t>(Lane)].Next; },
			[&Lanes](int Lane) { const FLane& L = Lanes[static_cast<std::size_t>(Lane)]; return std::hypot(L.Bx - L.Ax, L.By - L.Ay); },
			[&Lanes, GoalX, GoalY](int Lane) { return ToSegment(GoalX, GoalY, Lanes[static_cast<std::size_t>(Lane)]); });
	}
}

int main()
{
	{
		// A junction at (100, 0). Lane 1 is a dead end that stops 20 m short of the player, behind a wall;
		// lanes 2, 3, 4 go round the block to him. The exit that points at the player is the dead end.
		const std::vector<FLane> Lanes = {
			{0.f, 0.f, 100.f, 0.f, {1, 2}},
			{100.f, 0.f, 180.f, 0.f, {}},
			{100.f, 0.f, 100.f, 100.f, {3}},
			{100.f, 100.f, 200.f, 100.f, {4}},
			{200.f, 100.f, 200.f, 0.f, {}},
		};
		const std::vector<int> Found = Way(Lanes, 0, 10.f, 0.f, 200.f, 2.f);
		CHECK((Found == std::vector<int>{2, 3, 4}));
		// The player in the dead end itself: that is where the car goes.
		CHECK((Way(Lanes, 0, 10.f, 0.f, 150.f, 3.f) == std::vector<int>{1}));
		// The player ahead on the car's own lane: nothing to take.
		CHECK(Way(Lanes, 0, 10.f, 0.f, 60.f, 2.f).empty());
		// From a lane that leads nowhere: nothing.
		CHECK(Way(Lanes, 4, 200.f, 50.f, 0.f, 0.f).empty());
		// A start outside the lanes: nothing.
		CHECK(GTPLaneRoute::Toward(9, 5, 0.f, [&Lanes](int Lane) -> const std::vector<int>& { return Lanes[static_cast<std::size_t>(Lane)].Next; },
			[](int) { return 1.f; }, [](int) { return 1.f; }).empty());
	}
	{
		// A one-way block: 0 east, 1 north, 2 west, 3 south, back to 0. The player is behind the car on lane 0:
		// round the block and onto lane 0 again.
		const std::vector<FLane> Lanes = {
			{0.f, 0.f, 100.f, 0.f, {1}},
			{100.f, 0.f, 100.f, 100.f, {2}},
			{100.f, 100.f, 0.f, 100.f, {3}},
			{0.f, 100.f, 0.f, 0.f, {0}},
		};
		CHECK((Way(Lanes, 0, 70.f, 0.f, 30.f, 0.f) == std::vector<int>{1, 2, 3, 0}));
		// Ahead of the car on lane 0: stay.
		CHECK(Way(Lanes, 0, 30.f, 0.f, 70.f, 0.f).empty());
		// On the far side of the block: two lanes to take, not three.
		CHECK((Way(Lanes, 0, 30.f, 0.f, 50.f, 100.f) == std::vector<int>{1, 2}));
	}
	{
		// Two ways to the player's street, one of 300 m and one of 120 m: the short one. The two carriageways of
		// his street (lanes 3 and 4, 4 m apart) are as good as each other: the one reached first wins.
		const std::vector<FLane> Lanes = {
			{0.f, 0.f, 50.f, 0.f, {1, 2}},
			{50.f, 0.f, 50.f, 300.f, {3}},
			{50.f, 0.f, 170.f, 0.f, {4}},
			{50.f, 300.f, 200.f, 2.f, {}},
			{170.f, 0.f, 260.f, -2.f, {}},
		};
		CHECK((Way(Lanes, 0, 5.f, 0.f, 200.f, 0.f) == std::vector<int>{2, 4}));
	}
	std::printf(Failures ? "FAILED (%d)\n" : "lane_route_test: ok\n", Failures);
	return Failures ? 1 : 0;
}
