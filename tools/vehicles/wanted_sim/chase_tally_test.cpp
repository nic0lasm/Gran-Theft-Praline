// Checks the numbers of a pursuit as the player would see it (GTPChaseTally.h).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/chase_tally_test.cpp -o /tmp/chase_tally_test && /tmp/chase_tally_test
#include "GTPChaseTally.h"

#include <cmath>
#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

static bool Near(float A, float B, float Tolerance = 0.01f) { return std::fabs(A - B) <= Tolerance; }

int main()
{
	const FGTPChaseTallySettings S;
	const float Dt = 1.f / 30.f;
	{
		// Candidate 43: five units on the road, 1 km/h on average, 150 m behind a player at 60 km/h. Not a pursuit.
		FGTPChaseTally T;
		for (int Frame = 0; Frame < 30 * 35; ++Frame)
		{
			for (int Unit = 0; Unit < 5; ++Unit)
			{
				T.AddUnit(1.f, 150.f + 10.f * Unit, S);
			}
			T.EndFrame(Dt, 60.f, S);
		}
		CHECK(Near(T.MeanKmh(), 1.f) && Near(T.MovingShare(), 0.f) && Near(T.MeanNearest(), 150.f, 0.1f) && T.PeakUnits == 5);
		CHECK(Near(static_cast<float>(T.Seconds), 35.f, 0.1f));
		CHECK(GTPChase::Judge(T, S) == GTPChase::EVerdict::TooSlow);
	}
	{
		// Two units at 55 and 45 km/h, the nearest 40 m behind: the police follow.
		FGTPChaseTally T;
		for (int Frame = 0; Frame < 30 * 20; ++Frame)
		{
			T.AddUnit(55.f, 40.f, S);
			T.AddUnit(-45.f, 90.f, S); // a speed read backwards counts for its size
			T.EndFrame(Dt, 70.f, S);
		}
		CHECK(Near(T.MeanKmh(), 50.f) && Near(T.MovingShare(), 1.f) && Near(T.NearShare(), 1.f) && Near(T.NearestMin, 40.f));
		CHECK(Near(T.PlayerMeanKmh(), 70.f) && Near(T.UnitPeakKmh, 55.f));
		CHECK(GTPChase::Judge(T, S) == GTPChase::EVerdict::Follows);
	}
	{
		// Fast units that never come near (put back 200 m away again and again): they drive, but nobody is in the mirror.
		FGTPChaseTally T;
		for (int Frame = 0; Frame < 30 * 20; ++Frame)
		{
			T.AddUnit(60.f, 200.f, S);
			T.EndFrame(Dt, 80.f, S);
		}
		CHECK(GTPChase::Judge(T, S) == GTPChase::EVerdict::TooFar);
	}
	{
		// A player stuck in a jam at 10 km/h: units at 8 km/h, 12 m from the car, are on him. The speed wanted
		// does not drop with the player (25 km/h, fixed floor); what passes is that they stay within 30 m.
		FGTPChaseTally T;
		for (int Frame = 0; Frame < 30 * 20; ++Frame)
		{
			T.AddUnit(8.f, 12.f, S);
			T.EndFrame(Dt, 10.f, S);
		}
		CHECK(Near(GTPChase::WantedMeanKmh(T, S), 25.f));
		CHECK(GTPChase::Judge(T, S) == GTPChase::EVerdict::Follows);
	}
	{
		// Gate of candidate 49: units at 22 km/h, the nearest 75 m away on average, behind a player at 33.
		// 0.6 x 33 = 20 let it pass; under the floor of 25 it does not.
		FGTPChaseTally T;
		for (int Frame = 0; Frame < 30 * 66; ++Frame)
		{
			T.AddUnit(22.f, 75.f, S);
			T.EndFrame(Dt, 33.f, S);
		}
		CHECK(Near(GTPChase::WantedMeanKmh(T, S), 25.f));
		CHECK(GTPChase::Judge(T, S) == GTPChase::EVerdict::TooSlow);
	}
	{
		// A fast flight asks for more than the floor: a player at 70 km/h wants units at 42.
		FGTPChaseTally T;
		for (int Frame = 0; Frame < 30 * 30; ++Frame)
		{
			T.AddUnit(35.f, 60.f, S);
			T.EndFrame(Dt, 70.f, S);
		}
		CHECK(Near(GTPChase::WantedMeanKmh(T, S), 42.f));
		CHECK(GTPChase::Judge(T, S) == GTPChase::EVerdict::TooSlow);
	}
	{
		// A unit that arrives late weighs only the time it was there; frames without a unit count for nothing.
		FGTPChaseTally T;
		for (int Frame = 0; Frame < 30 * 10; ++Frame)
		{
			T.EndFrame(Dt, 50.f, S);
		}
		CHECK(T.Seconds == 0.0 && GTPChase::Judge(T, S) == GTPChase::EVerdict::TooShort);
		for (int Frame = 0; Frame < 30 * 12; ++Frame)
		{
			T.AddUnit(40.f, 60.f, S);
			if (Frame >= 30 * 6)
			{
				T.AddUnit(80.f, 100.f, S);
			}
			T.EndFrame(Dt, 50.f, S);
		}
		// 12 s at 40 and 6 s at 80: (480 + 480) / 18.
		CHECK(Near(T.MeanKmh(), 960.f / 18.f, 0.05f) && T.PeakUnits == 2 && Near(static_cast<float>(T.Seconds), 12.f, 0.05f));
		// The cars sent: two for the level, then one to replace a car lost, made 190 m away, and one made 90 m away.
		T.NoteSent(60.f, false);
		T.NoteSent(110.f, false);
		CHECK(T.Sent == 2 && T.Replacements == 0 && T.ReplacementNearest < 0.f && GTPChase::Unaided(T, S));
		T.NoteSent(190.f, true);
		CHECK(T.Sent == 3 && T.Replacements == 1 && Near(T.ReplacementNearest, 190.f) && GTPChase::Unaided(T, S));
		// A replacement made close to the player did not drive to him: the pursuit was helped.
		T.NoteSent(90.f, true);
		T.NoteSent(240.f, true);
		CHECK(T.Replacements == 3 && Near(T.ReplacementNearest, 90.f) && !GTPChase::Unaided(T, S));
		T.Reset();
		CHECK(T.Sent == 0 && T.Replacements == 0 && T.ReplacementNearest < 0.f);
		CHECK(T.Seconds == 0.0 && T.NearestMin < 0.f && T.MeanNearest() < 0.f);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
