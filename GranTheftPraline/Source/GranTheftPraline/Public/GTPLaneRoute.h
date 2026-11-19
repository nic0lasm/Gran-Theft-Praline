#pragma once

// Which lanes lead a chasing car to its target, written without engine types so it can be tested outside
// Unreal (tools/vehicles/wanted_sim/lane_route_test.cpp). Units: metres.
//
// Lanes are one-way: each has the lanes it leads to. A unit used to take, at every junction, the exit whose end
// was the nearest to the player. That exit can be a dead end (gate of candidate 49: a unit stood 9 s at the end
// of one, 190 m from the player) or the wrong way round a one-way block. Here the way is the shortest one, by
// Dijkstra over the lanes (the GPS of the map, GTPGpsCore.h, does the same over the two-way road lines; the
// lanes need their direction, so they have their own search).

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace GTPLaneRoute
{
	/**
	 * The lanes to take after Start, in order, to get as near the goal as the lanes allow by the shortest way.
	 *
	 * NextOf(Lane): the lanes it leads to. LengthOf(Lane): metres. GoalMetres(Lane): metres between the goal
	 * and the lane. StartGoalMetres: the same for what is left of Start ahead of the car (a goal behind the
	 * car on its own lane is reached round the block, if a way leads back to that lane).
	 * Lanes within Slack metres of the nearest one are as good as it: the shortest way among them wins.
	 * Empty when what is left of Start is the place to be, or when Start leads nowhere.
	 */
	template <typename FNextOf, typename FLengthOf, typename FGoalOf>
	std::vector<int> Toward(int Start, int LaneCount, float StartGoalMetres, const FNextOf& NextOf, const FLengthOf& LengthOf,
		const FGoalOf& GoalMetres, float Slack = 15.f)
	{
		std::vector<int> Out;
		if (Start < 0 || Start >= LaneCount)
		{
			return Out;
		}
		const float Far = std::numeric_limits<float>::max();
		const std::size_t Count = static_cast<std::size_t>(LaneCount);
		// Metres driven from the end of Start to the end of each lane.
		std::vector<float> Driven(Count, Far);
		std::vector<int> From(Count, -1);
		using FItem = std::pair<float, int>;
		std::priority_queue<FItem, std::vector<FItem>, std::greater<FItem>> Queue;
		Driven[static_cast<std::size_t>(Start)] = 0.f;
		Queue.push({0.f, Start});
		// Back on Start after a way round: the last lane of that way, and its length.
		float RoundMetres = Far;
		int RoundFrom = -1;
		while (!Queue.empty())
		{
			const FItem Top = Queue.top();
			Queue.pop();
			if (Top.first > Driven[static_cast<std::size_t>(Top.second)])
			{
				continue;
			}
			for (const int Next : NextOf(Top.second))
			{
				if (Next < 0 || Next >= LaneCount)
				{
					continue;
				}
				const float Metres = Top.first + std::max(static_cast<float>(LengthOf(Next)), 0.1f);
				if (Next == Start)
				{
					if (Metres < RoundMetres)
					{
						RoundMetres = Metres;
						RoundFrom = Top.second;
					}
					continue;
				}
				if (Metres < Driven[static_cast<std::size_t>(Next)])
				{
					Driven[static_cast<std::size_t>(Next)] = Metres;
					From[static_cast<std::size_t>(Next)] = Top.second;
					Queue.push({Metres, Next});
				}
			}
		}
		// How near the goal each place the car can reach is: what is left of Start, every other lane, Start again.
		std::vector<float> Goal(Count, Far);
		float Nearest = StartGoalMetres;
		for (int Lane = 0; Lane < LaneCount; ++Lane)
		{
			if (Lane != Start && Driven[static_cast<std::size_t>(Lane)] < Far)
			{
				Goal[static_cast<std::size_t>(Lane)] = static_cast<float>(GoalMetres(Lane));
				Nearest = std::min(Nearest, Goal[static_cast<std::size_t>(Lane)]);
			}
		}
		const float StartAgain = RoundFrom >= 0 ? static_cast<float>(GoalMetres(Start)) : Far;
		Nearest = std::min(Nearest, StartAgain);
		if (StartGoalMetres <= Nearest + Slack)
		{
			return Out;
		}
		int Target = -1;
		float Shortest = Far;
		for (int Lane = 0; Lane < LaneCount; ++Lane)
		{
			const std::size_t At = static_cast<std::size_t>(Lane);
			if (Lane != Start && Goal[At] <= Nearest + Slack && Driven[At] < Shortest)
			{
				Target = Lane;
				Shortest = Driven[At];
			}
		}
		const bool bRound = StartAgain <= Nearest + Slack && RoundMetres < Shortest;
		for (int Lane = bRound ? RoundFrom : Target; Lane >= 0 && Lane != Start; Lane = From[static_cast<std::size_t>(Lane)])
		{
			Out.push_back(Lane);
		}
		std::reverse(Out.begin(), Out.end());
		if (bRound)
		{
			Out.push_back(Start);
		}
		return Out;
	}
}
