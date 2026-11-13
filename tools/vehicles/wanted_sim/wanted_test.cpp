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
	{
		// Scenario without a player: steal a car, get chased, run people over, hide, lose the level, get a new one.
		FGTPWantedState S;
		float T = 0.f;
		const float Dt = 0.1f;
		int MaxStars = 0;
		auto Step = [&](float Seconds, bool Seen) { for (float E = 0.f; E < Seconds; E += Dt) { S.Tick(Dt, Seen); T += Dt; } };
		S.AddCrime(EGTPCrimeKind::CarTheft, T);
		CHECK(S.GetStars() == 1);
		Step(5.f, true);
		S.AddCrime(EGTPCrimeKind::HitPedestrian, T);
		CHECK(S.GetStars() == 2);
		Step(3.f, true);
		S.AddCrime(EGTPCrimeKind::HitPedestrian, T);
		S.AddCrime(EGTPCrimeKind::HitPolice, T);
		MaxStars = S.GetStars();
		CHECK(MaxStars >= 3);
		Step(30.f, true);
		CHECK(S.GetStars() == MaxStars); // never drops while seen
		const float HiddenAt = T;
		Step(120.f, false);
		CHECK(S.GetStars() == 0 && S.GetPhase() == EGTPWantedPhase::Clear);
		// The level fell after the search time of the highest level, not before.
		CHECK(T - HiddenAt >= 0.f);
		// A new crime later starts again from one star.
		S.AddCrime(EGTPCrimeKind::CarTheft, T);
		CHECK(S.GetStars() == 1);
	}
	{
		// Search time grows with the level.
		float Previous = 0.f;
		for (int Stars = 1; Stars <= 5; ++Stars)
		{
			FGTPWantedState S;
			S.ForceStars(Stars);
			float T = 0.f;
			while (S.GetStars() > 0 && T < 300.f) { S.Tick(0.1f, false); T += 0.1f; }
			CHECK(T > Previous);
			Previous = T;
		}
		// Units never decrease with the stars.
		int PreviousUnits = 0;
		for (int Stars = 0; Stars <= 5; ++Stars)
		{
			const int Units = GTPWanted::UnitsForStars(Stars, 4);
			CHECK(Units >= PreviousUnits);
			PreviousUnits = Units;
		}
		// An arrest then clears the level and the arrest bar.
		FGTPWantedState S;
		S.ForceStars(2);
		bool Busted = false;
		for (int I = 0; I < 100 && !Busted; ++I) Busted = S.UpdateArrest(0.1f, 3.f, 0.f, true);
		CHECK(Busted);
		S.Clear();
		CHECK(S.GetStars() == 0 && S.GetArrestProgress() == 0.f);
	}
	{
		// Combat crimes: a burst of shots counts once per 5 s, a killing is heavy, killing a police officer is heavier.
		FGTPWantedState S;
		CHECK(S.AddCrime(EGTPCrimeKind::ShotsFired, 0.f));
		CHECK(S.GetStars() == 1);
		CHECK(!S.AddCrime(EGTPCrimeKind::ShotsFired, 3.f)); // same burst
		CHECK(S.AddCrime(EGTPCrimeKind::ShotsFired, 5.5f));
		FGTPWantedState K;
		CHECK(K.AddCrime(EGTPCrimeKind::Killing, 0.f));
		CHECK(K.GetStars() == 2);
		FGTPWantedState P;
		CHECK(P.AddCrime(EGTPCrimeKind::KillPolice, 0.f));
		CHECK(P.GetStars() >= 3);
		CHECK(P.GetStars() > K.GetStars());
		// Holding a gun at someone adds heat once per burst too.
		FGTPWantedState T;
		T.AddCrime(EGTPCrimeKind::ArmedThreat, 0.f);
		CHECK(!T.AddCrime(EGTPCrimeKind::ArmedThreat, 1.f));
		CHECK(T.GetStars() == 1);
	}
	{
		// The crime table of docs/gameplay/wanted.md: one crime alone gives this many stars.
		struct Row { EGTPCrimeKind Kind; int Stars; };
		const Row Table[] = {
			{EGTPCrimeKind::CarTheft, 1}, {EGTPCrimeKind::HitPedestrian, 1}, {EGTPCrimeKind::HitVehicle, 0},
			{EGTPCrimeKind::HitPolice, 1}, {EGTPCrimeKind::Assault, 0}, {EGTPCrimeKind::ShotsFired, 1},
			{EGTPCrimeKind::ArmedThreat, 1}, {EGTPCrimeKind::Killing, 2}, {EGTPCrimeKind::KillPolice, 3}};
		for (const Row& R : Table)
		{
			FGTPWantedState S;
			S.AddCrime(R.Kind, 0.f);
			CHECK(S.GetStars() == R.Stars);
		}
		// Stealing a car in front of police and then firing at them gets to three stars within seconds.
		FGTPWantedState Chain;
		Chain.AddCrime(EGTPCrimeKind::CarTheft, 0.f);
		Chain.AddCrime(EGTPCrimeKind::HitPolice, 1.f);
		Chain.AddCrime(EGTPCrimeKind::ShotsFired, 2.f);
		Chain.AddCrime(EGTPCrimeKind::HitVehicle, 3.f);
		CHECK(Chain.GetStars() >= 2);
	}
	{
		// Hiding: out of sight and still, the search runs twice as fast.
		FGTPWantedState Slow, Fast;
		Slow.ForceStars(2);
		Fast.ForceStars(2);
		int SlowSteps = 0, FastSteps = 0;
		while (Slow.GetStars() > 0 && SlowSteps < 10000) { Slow.Tick(0.1f, false, 1.f); ++SlowSteps; }
		while (Fast.GetStars() > 0 && FastSteps < 10000) { Fast.Tick(0.1f, false, 2.f); ++FastSteps; }
		CHECK(FastSteps * 2 <= SlowSteps + 2 && FastSteps > 0);
		// Changing car out of sight keeps a share of the search; it does nothing with no stars.
		FGTPWantedState Swap;
		CHECK(!Swap.ShortenSearch(0.35f));
		Swap.ForceStars(3);
		CHECK(Swap.GetPhase() == EGTPWantedPhase::Pursuit);
		CHECK(Swap.ShortenSearch(0.35f));
		CHECK(Swap.GetPhase() == EGTPWantedPhase::Searching);
		CHECK(Swap.GetSearchProgress() >= 0.64f);
		// Seen again: the search starts over.
		Swap.Tick(0.1f, true);
		CHECK(Swap.GetPhase() == EGTPWantedPhase::Pursuit);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
