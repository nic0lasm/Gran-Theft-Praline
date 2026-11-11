// Checks of the mission parking spot logic (GTPKerbSpot.h), no engine needed:
//   g++ -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/kerb_test/kerb_test.cpp -o kerb_test && ./kerb_test
#include "GTPKerbSpot.h"

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

static bool Near(float A, float B, float Tolerance = 0.01f)
{
	return std::fabs(A - B) <= Tolerance;
}

int main()
{
	using namespace GTPKerb;
	FSpot Spot;
	Check(SpotBeside({0.f, 0.f}, {1000.f, 0.f}, 250.f, Spot), "a spot beside a segment along +X");
	Check(Near(Spot.Position.X, 500.f) && Near(Spot.Position.Y, 250.f), "middle of the segment, 250 to the right (+Y)");
	Check(Near(Spot.YawDegrees, 0.f), "faces the travel direction");
	Check(SpotBeside({0.f, 0.f}, {0.f, 1000.f}, 250.f, Spot), "a spot beside a segment along +Y");
	Check(Near(Spot.Position.X, -250.f) && Near(Spot.Position.Y, 500.f), "right of +Y is -X");
	Check(Near(Spot.YawDegrees, 90.f), "yaw 90 along +Y");
	Check(!SpotBeside({5.f, 5.f}, {5.f, 5.f}, 250.f, Spot), "no spot on a zero-length segment");

	const std::vector<std::vector<FPoint2>> Lanes = {
		{{0.f, 0.f}, {1000.f, 0.f}, {2000.f, 0.f}},
		{{0.f, 5000.f}, {1000.f, 5000.f}},
		{{3000.f, 0.f}},
	};
	const std::vector<FSegmentRef> Found = SegmentsNear(Lanes, {1600.f, 0.f}, 1200.f);
	Check(Found.size() == 2, "two segments within the radius, none of a far lane or a one-point lane");
	Check(!Found.empty() && Found[0].Lane == 0 && Found[0].Point == 1, "nearest first");
	Check(Found.size() == 2 && Found[1].Point == 0 && Found[0].Distance <= Found[1].Distance, "then the next nearest");
	Check(SegmentsNear(Lanes, {0.f, 0.f}, 10.f).empty(), "nothing when no middle is inside the radius");

	std::printf("%d failure(s)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
