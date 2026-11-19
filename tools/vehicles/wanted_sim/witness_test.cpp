// Checks who counts as a witness (GTPWitnessLogic.h).
//   g++ -O2 -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/witness_test.cpp -o /tmp/witness_test && /tmp/witness_test
#include "GTPWitnessLogic.h"

#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

int main()
{
	const FGTPWitnessSettings S;
	{
		// Nobody around: not reported.
		CHECK(!GTPWitness::Evaluate(0.f, 0.f, false, false, {}, S).bReported);
	}
	{
		// A person in sight within 25 m: reported after the call delay.
		const auto R = GTPWitness::Evaluate(0.f, 0.f, false, false, {{10.f, 0.f, false, true}}, S);
		CHECK(R.bReported && R.Delay == S.CallDelay);
		// Too far, or no line of sight: not reported.
		CHECK(!GTPWitness::Evaluate(0.f, 0.f, false, false, {{40.f, 0.f, false, true}}, S).bReported);
		CHECK(!GTPWitness::Evaluate(0.f, 0.f, false, false, {{10.f, 0.f, false, false}}, S).bReported);
	}
	{
		// Police in sight within 60 m: at once, even when a person also saw it.
		const auto R = GTPWitness::Evaluate(0.f, 0.f, false, false, {{10.f, 0.f, false, true}, {50.f, 0.f, true, true}}, S);
		CHECK(R.bReported && R.Delay == 0.f);
		CHECK(!GTPWitness::Evaluate(0.f, 0.f, false, false, {{80.f, 0.f, true, true}}, S).bReported);
	}
	{
		// A shot is heard without sight: by police within 120 m, by a person within 40 m.
		CHECK(GTPWitness::Evaluate(0.f, 0.f, true, false, {{100.f, 0.f, true, false}}, S).bReported);
		CHECK(GTPWitness::Evaluate(0.f, 0.f, true, false, {{30.f, 0.f, false, false}}, S).bReported);
		CHECK(!GTPWitness::Evaluate(0.f, 0.f, true, false, {{150.f, 0.f, true, false}}, S).bReported);
		CHECK(!GTPWitness::Evaluate(0.f, 0.f, false, false, {{100.f, 0.f, true, false}}, S).bReported); // not loud: no
	}
	{
		// The victim of a carjack tells the police even on an empty street.
		const auto R = GTPWitness::Evaluate(0.f, 0.f, false, true, {}, S);
		CHECK(R.bReported && R.Delay == S.CallDelay);
	}
	{
		// Who told the police: the run of the packaged game reads it (a star after a theft must come from a call).
		const auto Call = GTPWitness::Evaluate(0.f, 0.f, false, true, {{10.f, 0.f, false, true}, {12.f, 0.f, false, true}, {40.f, 0.f, false, true}}, S);
		CHECK(Call.bVictim && Call.Persons == 2 && !Call.bByPolice && Call.Delay == S.CallDelay);
		const auto Seen = GTPWitness::Evaluate(0.f, 0.f, false, false, {{10.f, 0.f, false, true}, {50.f, 0.f, true, true}}, S);
		CHECK(Seen.bByPolice && Seen.Persons == 1 && !Seen.bVictim && Seen.Delay == 0.f);
		const auto Nobody = GTPWitness::Evaluate(0.f, 0.f, false, false, {{40.f, 0.f, false, true}}, S);
		CHECK(!Nobody.bReported && Nobody.Persons == 0 && Nobody.Delay == 0.f);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
