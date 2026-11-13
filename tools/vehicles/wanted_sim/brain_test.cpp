// Checks the police unit states (GTPPoliceBrainLogic.h).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/brain_test.cpp -o /tmp/brain_test && /tmp/brain_test
#include "GTPPoliceBrainLogic.h"

#include <cstdio>

using namespace GTPPoliceBrain;

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

int main()
{
	FInput In;
	CHECK(Decide(In, EState::Pursue) == EState::Idle);
	In.Stars = 2;
	In.bSensed = true;
	CHECK(Decide(In, EState::Idle) == EState::Pursue);
	// A corner is not a loss: still pursuing within the grace time, searching after it.
	In.bSensed = false;
	In.SecondsSinceSensed = 0.8f;
	CHECK(Decide(In, EState::Pursue) == EState::Pursue);
	In.SecondsSinceSensed = 2.f;
	CHECK(Decide(In, EState::Pursue) == EState::Search);
	// Sensed again: back to the chase.
	In.bSensed = true;
	In.SecondsSinceSensed = 0.f;
	CHECK(Decide(In, EState::Search) == EState::Pursue);
	// Arrest needs a helpless target within reach; leaving it has hysteresis.
	In.bTargetHelpless = true;
	In.DistanceM = 6.f;
	CHECK(Decide(In, EState::Pursue) == EState::Arrest);
	In.DistanceM = 9.f;
	CHECK(Decide(In, EState::Arrest) == EState::Arrest);
	CHECK(Decide(In, EState::Pursue) == EState::Pursue);
	In.DistanceM = 12.f;
	CHECK(Decide(In, EState::Arrest) == EState::Pursue);
	// Not helpless: never arrest.
	In.bTargetHelpless = false;
	In.DistanceM = 3.f;
	CHECK(Decide(In, EState::Pursue) == EState::Pursue);
	// Roadblock cars stay roadblocks while there are stars; the level gone, idle.
	In.bIsRoadblock = true;
	CHECK(Decide(In, EState::Pursue) == EState::Block);
	In.Stars = 0;
	CHECK(Decide(In, EState::Block) == EState::Idle);
	CHECK(WantsYelp(EState::Pursue) && WantsYelp(EState::Arrest) && !WantsYelp(EState::Search) && !WantsYelp(EState::Idle));
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
