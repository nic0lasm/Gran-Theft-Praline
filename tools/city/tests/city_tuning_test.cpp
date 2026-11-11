// Off-engine checks of GTPCityTuningCore.h (cull arithmetic of UGTPCityRenderTuning).
#include "GTPCityTuningCore.h"

#include <cstdio>
#include <cstdlib>

static int Failures = 0;

static void Expect(bool Ok, const char* What)
{
	if (!Ok)
	{
		std::printf("FAIL: %s\n", What);
		++Failures;
	}
}

int main()
{
	using namespace GTPCityTuning;
	Expect(ClampPct(50) == 50, "50 % kept");
	Expect(ClampPct(0) == 10 && ClampPct(-5) == 10, "0 or negative clamped to 10 %");
	Expect(ClampPct(250) == 100, "above 100 % clamped");
	Expect(ScaledEndCm(4000, 60) == 2400, "40 m at 60 % = 24 m");
	Expect(ScaledEndCm(2000, 50) == 1000, "20 m at 50 % = 10 m");
	Expect(ScaledEndCm(4000, 100) == 4000, "100 % unchanged");
	Expect(ScaledEndCm(0, 50) == 0, "0 (never culled) unchanged");
	Expect(ScaledEndCm(-1, 50) == -1, "negative unchanged");
	Expect(ScaledEndCm(600, 10) == MinEndCm, "never under 5 m");
	Expect(ScaledEndCm(300, 10) == 300, "a 3 m end is not raised");
	Expect(ScaledEndCm(2147483000, 60) > 0, "no overflow on a huge end");
	Expect(StartCm(2400) == 1920, "fade starts at 80 %");
	for (int End = 1; End < 200000; End += 997)
	{
		for (int Pct = -20; Pct <= 140; Pct += 7)
		{
			const int S = ScaledEndCm(End, Pct);
			if (S <= 0 || S > End || StartCm(S) > S)
			{
				std::printf("FAIL: end %d pct %d -> %d\n", End, Pct, S);
				return 1;
			}
		}
	}
	if (Failures)
	{
		return 1;
	}
	std::printf("city tuning: all checks passed\n");
	return 0;
}
