#pragma once

// Traffic lights of the lane network (version 0.4), written without engine
// types so they run in tools/vehicles/traffic_sim as well as in the game.
//
// Every junction of the lane file that is reached from two crossing
// directions gets lights. Its incoming lanes are split into two groups by
// heading: lanes within 45 degrees of the first approach (or of its opposite)
// form group 0, the others group 1. The groups take turns:
//   group 0 green, group 0 amber, all red, group 1 green, group 1 amber, all red.
// Each junction starts at its own point of the cycle, so the street does not
// change all at once. Pedestrians cross a road while its group is red
// (WalkAllowed), which the pedestrian system reads for its crossings.

#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

enum class EGTPLight : uint8_t
{
	None,   // no light on this lane (not a crossing, or the lane enters no junction)
	Green,
	Amber,
	Red
};

struct FGTPLightTimings
{
	float Green = 12.f;
	float Amber = 3.f;
	float AllRed = 2.f;

	float Cycle() const { return 2.f * (Green + Amber + AllRed); }
};

class FGTPTrafficLights
{
public:
	/** One incoming lane: its index, the junction it enters and its heading on arrival (radians). */
	void AddApproach(int Lane, int Junction, float Heading)
	{
		if (Junction < 0)
		{
			return;
		}
		Approaches.push_back({Lane, Junction, Heading});
	}

	/** Splits the approaches into groups; junctions reached from one direction only get no light. */
	void Build(const FGTPLightTimings& InTimings)
	{
		Timings = InTimings;
		LaneGroup.clear();
		JunctionOffset.clear();
		std::unordered_map<int, float> Reference;
		std::unordered_map<int, int> Mask;
		for (const FApproach& A : Approaches)
		{
			const auto Found = Reference.find(A.Junction);
			if (Found == Reference.end())
			{
				Reference[A.Junction] = A.Heading;
			}
		}
		std::vector<int> Groups(Approaches.size());
		for (size_t Index = 0; Index < Approaches.size(); ++Index)
		{
			const FApproach& A = Approaches[Index];
			// Same axis as the reference when the headings are within 45 degrees, either way.
			const float Cos = std::fabs(std::cos(A.Heading - Reference[A.Junction]));
			Groups[Index] = Cos >= 0.7071f ? 0 : 1;
			Mask[A.Junction] |= 1 << Groups[Index];
		}
		for (size_t Index = 0; Index < Approaches.size(); ++Index)
		{
			const FApproach& A = Approaches[Index];
			if (Mask[A.Junction] == 3)
			{
				LaneGroup[A.Lane] = {A.Junction, Groups[Index]};
				// Spread the junctions over the cycle (golden ratio steps, deterministic).
				JunctionOffset[A.Junction] = std::fmod(static_cast<float>(A.Junction) * 0.618034f, 1.f) * Timings.Cycle();
			}
		}
	}

	bool HasLights(int Junction) const { return JunctionOffset.count(Junction) > 0; }

	int NumSignalledJunctions() const { return static_cast<int>(JunctionOffset.size()); }

	/** Group (0 or 1) of an incoming lane, -1 without light. */
	int GroupOf(int Lane) const
	{
		const auto Found = LaneGroup.find(Lane);
		return Found == LaneGroup.end() ? -1 : Found->second.Group;
	}

	/** Light seen by a car at the end of Lane at Time (seconds). */
	EGTPLight LightFor(int Lane, float Time) const
	{
		const auto Found = LaneGroup.find(Lane);
		if (Found == LaneGroup.end())
		{
			return EGTPLight::None;
		}
		return GroupLight(Found->second.Junction, Found->second.Group, Time);
	}

	/** Light of one group of a junction at Time. */
	EGTPLight GroupLight(int Junction, int Group, float Time) const
	{
		const auto Found = JunctionOffset.find(Junction);
		if (Found == JunctionOffset.end() || Group < 0)
		{
			return EGTPLight::None;
		}
		const float Half = Timings.Green + Timings.Amber + Timings.AllRed;
		float T = std::fmod(Time + Found->second, Timings.Cycle());
		if (T < 0.f)
		{
			T += Timings.Cycle();
		}
		// Shift so that this group's half of the cycle starts at 0.
		if (Group == 1)
		{
			T = std::fmod(T + Half, Timings.Cycle());
		}
		if (T < Timings.Green)
		{
			return EGTPLight::Green;
		}
		if (T < Timings.Green + Timings.Amber)
		{
			return EGTPLight::Amber;
		}
		return EGTPLight::Red;
	}

	/** Pedestrians may cross the roads of Group at Junction now (that group is red). */
	bool WalkAllowed(int Junction, int Group, float Time) const
	{
		return GroupLight(Junction, Group, Time) == EGTPLight::Red;
	}

	/**
	 * Distance to stop at for a car on Lane (metres), or a negative value to go on.
	 * Red: stop at the line. Amber: stop only if the car can do it at ComfortDecel,
	 * otherwise it clears the junction.
	 */
	float StopDistance(int Lane, float Time, float ToLaneEnd, float Speed, float ComfortDecel, float StopLineBack = 3.f) const
	{
		const EGTPLight Light = LightFor(Lane, Time);
		if (Light == EGTPLight::None || Light == EGTPLight::Green)
		{
			return -1.f;
		}
		const float Stop = ToLaneEnd - StopLineBack;
		if (Stop < -1.f)
		{
			return -1.f; // already over the line: carry on
		}
		if (Light == EGTPLight::Amber && Speed * Speed / (2.f * ComfortDecel) > Stop)
		{
			return -1.f;
		}
		return Stop > 0.f ? Stop : 0.f;
	}

	const FGTPLightTimings& GetTimings() const { return Timings; }

private:
	struct FApproach
	{
		int Lane;
		int Junction;
		float Heading;
	};
	struct FLaneGroup
	{
		int Junction;
		int Group;
	};

	std::vector<FApproach> Approaches;
	std::unordered_map<int, FLaneGroup> LaneGroup;
	std::unordered_map<int, float> JunctionOffset;
	FGTPLightTimings Timings;
};
