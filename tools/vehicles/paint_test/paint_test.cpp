// Checks of GTPCarPaint.h outside Unreal: g++ -std=c++17 -I <Public> paint_test.cpp
#include "GTPCarPaint.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <set>

static int Failures = 0;

static void Check(bool bOk, const char* What)
{
	std::printf("%s %s\n", bOk ? "ok  " : "FAIL", What);
	Failures += bOk ? 0 : 1;
}

int main()
{
	using namespace GTPCarPaint;
	Check(std::fabs(TotalShare() - 100.f) < 0.01f, "the shares make a hundred cars");
	bool bInRange = true;
	for (const FPaint& Each : Paints)
	{
		bInRange = bInRange && Each.Share > 0.f && Each.R >= 0.f && Each.R <= 1.f && Each.G >= 0.f && Each.G <= 1.f && Each.B >= 0.f && Each.B <= 1.f;
	}
	Check(bInRange, "every paint has a share and a colour between 0 and 1");
	Check(Draw(0.f) == 0 && Draw(1.f) == NumPaints - 1 && Draw(-3.f) == 0 && Draw(7.f) == NumPaints - 1, "the ends of the draw give the first and the last paint");
	Check(Draw(0.f, 0) == 1 && Draw(1.f, NumPaints - 1) == NumPaints - 2, "never the paint of the car made just before, at the ends either");

	// The shares hold over many cars, and no two in a row are alike.
	std::mt19937 Rng(20261126u);
	std::uniform_real_distribution<float> U(0.f, 1.f);
	int Counts[NumPaints] = {};
	int Last = -1, Twice = 0;
	const int Cars = 200000;
	for (int Car = 0; Car < Cars; ++Car)
	{
		const int Paint = Draw(U(Rng), Last);
		Twice += Paint == Last ? 1 : 0;
		++Counts[Paint];
		Last = Paint;
	}
	Check(Twice == 0, "200 000 cars: no two in a row of one colour");
	int Greys = Counts[0] + Counts[1] + Counts[2] + Counts[3] + Counts[4];
	std::printf("     greys, whites and blacks %.0f %%, the most common paint %.0f %%, the rarest %.1f %%\n", 100.0 * Greys / Cars, 100.0 * Counts[1] / Cars, 100.0 * Counts[12] / Cars);
	Check(Greys > 0.62 * Cars && Greys < 0.76 * Cars, "greys, whites and blacks are two cars in three to three in four");
	bool bAllSeen = true;
	for (const int Count : Counts)
	{
		bAllSeen = bAllSeen && Count > Cars / 100;
	}
	Check(bAllSeen, "every paint is seen, one car in a hundred at least");

	// What a street shows: the 28 cars the game wants around the player, a thousand times.
	int Fewest = NumPaints, MostAlike = 0;
	for (int Street = 0; Street < 1000; ++Street)
	{
		std::set<int> Seen;
		int PerPaint[NumPaints] = {};
		int Before = -1;
		for (int Car = 0; Car < 28; ++Car)
		{
			const int Paint = Draw(U(Rng), Before);
			Seen.insert(Paint);
			MostAlike = std::max(MostAlike, ++PerPaint[Paint]);
			Before = Paint;
		}
		Fewest = std::min(Fewest, static_cast<int>(Seen.size()));
	}
	std::printf("     28 cars, 1000 streets: %d colours at least, %d cars of one colour at most\n", Fewest, MostAlike);
	Check(Fewest >= 5, "28 cars show five colours at least");
	Check(MostAlike <= 14, "and never more than half of them one colour");

	std::printf(Failures == 0 ? "PASS\n" : "FAIL (%d)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
