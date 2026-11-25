// A driver that leaves a lane in the middle of a corner: what it keeps of the lane left (GTPDrive::KeptBehind,
// FGTPLaneFollower::Step), on the three corners of the Bellecour circuit where gate 83's car left its lane.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/lane_change_corner_test.cpp -o /tmp/lane_change_corner_test && /tmp/lane_change_corner_test
//
// The lanes and the car: circuit_corners.h. Three drivers on the same lanes: one that drops each lane whole (the
// lane follower until gate 83), one that keeps its last 12 m at the head of its path (since), one whose path is
// never cut. The second must decide as the third does, frame after frame.
#include "circuit_corners.h"

int main(int ArgCount, char**)
{
	const bool bTrace = ArgCount > 1;
	int JumpsDropped = 0, Runs = 0;
	for (const FCorner& Corner : Corners())
	{
		const int Points = static_cast<int>(Corner.Lanes[0].Line.size());
		for (const int From : {0, Points / 2, Points - 5})
		{
			for (const bool bFollowSteering : {false, true})
			{
				const FRun Dropped = Drive(Corner, From, bFollowSteering, ELeave::Drop);
				const FRun Kept = Drive(Corner, From, bFollowSteering, ELeave::KeepEnd, bTrace);
				const FRun Whole = Drive(Corner, From, bFollowSteering, ELeave::Never);
				std::printf("%s, from point %d of %d, pedals %s: lane dropped whole %.1f s%s, %.1f s of throttle past what the wheels hold; its end kept %.1f s%s, %.1f s; path never cut %.1f s, %.1f s\n",
					Corner.Name, From, Points, bFollowSteering ? "following the steering" : "alone",
					static_cast<double>(Dropped.Seconds), Dropped.bWantJumped ? " (speed wanted jumped)" : "", static_cast<double>(Dropped.ThrottleTurned),
					static_cast<double>(Kept.Seconds), Kept.bWantJumped ? " (speed wanted jumped)" : "", static_cast<double>(Kept.ThrottleTurned),
					static_cast<double>(Whole.Seconds), static_cast<double>(Whole.ThrottleTurned));
				++Runs;
				JumpsDropped += Dropped.bWantJumped ? 1 : 0;
				CHECK(Kept.bArrived && Whole.bArrived);
				// The same decisions as on a path never cut: the same time to the frame, the same speeds wanted, the same line.
				CHECK(!Kept.bWantJumped);
				CHECK(Kept.Seconds == Whole.Seconds);
				CHECK(std::fabs(Kept.WantSum - Whole.WantSum) <= 0.001f * Whole.WantSum);
				CHECK(std::fabs(Kept.Off - Whole.Off) < 0.05f);
				CHECK(Kept.ThrottleTurned <= Dropped.ThrottleTurned);
			}
		}
	}
	// What gate 83 showed with the lane dropped whole: at the corners from the quay and into the place de la Republique.
	CHECK(JumpsDropped >= Runs * 2 / 3);
	{
		// What is kept: the last points that cover the metres asked, in their order, the last one included.
		std::vector<FGTPDrivePoint> Path;
		for (int Index = 0; Index < 20; ++Index)
		{
			Path.push_back({static_cast<float>(Index) * 3.f, 0.f});
		}
		const std::vector<FGTPDrivePoint> Kept = GTPDrive::KeptBehind(Path, 10);
		CHECK(Kept.size() == 5 && Kept.front().X == 15.f && Kept.back().X == 27.f);
		// A lane shorter than that: all of it. No lane: nothing.
		CHECK(GTPDrive::KeptBehind(Path, 3).size() == 3);
		CHECK(GTPDrive::KeptBehind(Path, 1).size() == 1);
		CHECK(GTPDrive::KeptBehind(Path, 0).empty());
		CHECK(GTPDrive::KeptBehind(Path, 50).size() == 5);
		CHECK(GTPDrive::KeptBehind({}, 4).empty());
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
