// Checks the wanted level rules (GTPWantedLogic.h, the header the game compiles).
//   g++ -O2 -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/wanted_test.cpp -o /tmp/wanted_test && /tmp/wanted_test
#include "GTPWantedLogic.h"

#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

int main()
{
	{
		FGTPWantedState S;
		CHECK(S.GetStars() == 0 && S.GetPhase() == EGTPWantedPhase::Clear);
		CHECK(S.AddCrime(EGTPCrimeKind::CarTheft, 0.f));
		CHECK(S.GetStars() == 1 && S.GetPhase() == EGTPWantedPhase::Pursuit);
		// Same crime within the cooldown does not count again.
		CHECK(!S.AddCrime(EGTPCrimeKind::CarTheft, 1.f));
		CHECK(S.AddCrime(EGTPCrimeKind::HitPedestrian, 1.f));
		CHECK(S.GetStars() == 2);
	}
	{
		// Out of sight: the level is lost after the search time, longer at higher levels.
		FGTPWantedState S;
		S.AddCrime(EGTPCrimeKind::CarTheft, 0.f);
		float T = 0.f;
		while (S.GetStars() > 0 && T < 100.f) { S.Tick(0.1f, false); T += 0.1f; }
		CHECK(T > 17.f && T < 19.f); // 14 + 4 x 1 star

		FGTPWantedState S3;
		for (int I = 0; I < 4; ++I) S3.AddCrime(EGTPCrimeKind::HitPolice, 10.f * I);
		const int Stars = S3.GetStars();
		CHECK(Stars >= 3);
		float T3 = 0.f;
		while (S3.GetStars() > 0 && T3 < 200.f) { S3.Tick(0.1f, false); T3 += 0.1f; }
		CHECK(T3 > T + 4.f);
	}
	{
		// Seen: the timer never runs out.
		FGTPWantedState S;
		S.AddCrime(EGTPCrimeKind::HitPolice, 0.f);
		for (int I = 0; I < 3000; ++I) S.Tick(0.1f, true);
		CHECK(S.GetStars() >= 1 && S.GetPhase() == EGTPWantedPhase::Pursuit);
		// Then hidden: searching, progress grows.
		for (int I = 0; I < 50; ++I) S.Tick(0.1f, false);
		CHECK(S.GetPhase() == EGTPWantedPhase::Searching && S.GetSearchProgress() > 0.1f);
		// Seen again: back to pursuit.
		S.Tick(0.1f, true);
		CHECK(S.GetPhase() == EGTPWantedPhase::Pursuit);
	}
	{
		// Arrest only when close and slow (or on foot) for long enough.
		FGTPWantedState S;
		S.AddCrime(EGTPCrimeKind::HitPolice, 0.f);
		bool Busted = false;
		for (int I = 0; I < 100 && !Busted; ++I) Busted = S.UpdateArrest(0.1f, 5.f, 60.f, false);
		CHECK(!Busted); // fast car: no arrest
		for (int I = 0; I < 100 && !Busted; ++I) Busted = S.UpdateArrest(0.1f, 5.f, 3.f, false);
		CHECK(Busted);
		FGTPWantedState Free;
		for (int I = 0; I < 100; ++I) CHECK(!Free.UpdateArrest(0.1f, 2.f, 0.f, true)); // no stars: never
	}
	{
		CHECK(GTPWanted::UnitsForStars(0, 3) == 0);
		CHECK(GTPWanted::UnitsForStars(5, 3) == 3);
		CHECK(GTPWanted::UnitsForStars(1, 3) == 1);
		FGTPWantedSettings Cap; Cap.MaxStars = 3;
		FGTPWantedState S(Cap);
		for (int I = 0; I < 10; ++I) S.AddCrime(EGTPCrimeKind::HitPolice, 5.f * I);
		CHECK(S.GetStars() == 3);
	}
	{
		FGTPWantedState S;
		S.ForceStars(3);
		CHECK(S.GetStars() == 3 && S.GetPhase() == EGTPWantedPhase::Pursuit);
		S.ForceStars(0);
		CHECK(S.GetStars() == 0);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
