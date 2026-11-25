// Checks what becomes of a traffic car its driver left in the street (GTPLeftCars.h).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/left_cars_test.cpp -o /tmp/left_cars_test && /tmp/left_cars_test
#include "GTPLeftCars.h"

#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

int main()
{
	using namespace GTPLeftCars;
	const float RemoveM = 80.f;
	{
		// Its driver is still getting out, 200 m away and out of sight: kept until he is out.
		FCar Car;
		Car.ToPlayerM = 200.f;
		CHECK(Fate(Car, RemoveM) == EFate::Keep);
		// Empty now: taken away.
		Car.bEmpty = true;
		CHECK(Fate(Car, RemoveM) == EFate::TakeAway);
		// Not while the player can see it, nor near him.
		Car.bSeenByPlayer = true;
		CHECK(Fate(Car, RemoveM) == EFate::Keep);
		Car.bSeenByPlayer = false;
		Car.ToPlayerM = 79.f;
		CHECK(Fate(Car, RemoveM) == EFate::Keep);
	}
	{
		// The player took it (he is getting in, or drives it), its driver came back, or it burnt: no longer a car
		// left in the street, wherever it is.
		FCar Car;
		Car.bEmpty = true;
		Car.ToPlayerM = 200.f;
		Car.bPlayers = true;
		CHECK(Fate(Car, RemoveM) == EFate::Forget);
		Car.bPlayers = false;
		Car.bDrivenAgain = true;
		CHECK(Fate(Car, RemoveM) == EFate::Forget);
		Car.bDrivenAgain = false;
		Car.bWrecked = true;
		CHECK(Fate(Car, RemoveM) == EFate::Forget);
	}
	{
		// The way of the filmed flight: the car in its lane ahead, to 35 m; not the one behind, beside it in the next
		// lane (3 m), or parked at the kerb.
		CHECK(InTheWay(8.f, 0.4f) && InTheWay(34.f, -2.4f) && InTheWay(1.f, 2.4f));
		CHECK(!InTheWay(-3.f, 0.f) && !InTheWay(36.f, 0.f) && !InTheWay(10.f, 3.f) && !InTheWay(10.f, -2.6f));
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
