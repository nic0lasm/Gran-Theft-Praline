// Checks GTPCityStreamingCore.h (which city cells to stream in, ADR-0010) without Unreal.
// g++ -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public
//   then tools/city/tests/city_streaming_test.cpp -o city_streaming_test && ./city_streaming_test
#include "GTPCityStreamingCore.h"

#include <algorithm>
#include <cstdio>

using namespace GTPCity;

static int Failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++Failures; } } while (0)

static bool Has(const std::vector<int>& V, int X) { return std::find(V.begin(), V.end(), X) != V.end(); }

// 3 x 3 grid of 250 m cells, index = row * 3 + col, cell 4 in the middle at [0, 25000)^2.
static std::vector<FCellBox> Grid()
{
	std::vector<FCellBox> G;
	for (int R = -1; R <= 1; ++R)
		for (int C = -1; C <= 1; ++C)
			G.push_back({C * 25000.f, R * 25000.f, (C + 1) * 25000.f, (R + 1) * 25000.f});
	return G;
}

int main()
{
	const auto G = Grid();
	FStreamingRules R;   // 60 m load, 90 m unload, 4 max, 1 load per update
	CHECK(DistanceToBox(G[4], 100.f, 100.f) == 0.f);
	CHECK(std::fabs(DistanceToBox(G[4], -300.f, 12500.f) - 300.f) < 1e-3f);

	// Centre of the middle cell: only that one is within 60 m.
	std::vector<bool> L(9, false);
	auto P = PlanStreaming(G, L, 12500.f, 12500.f, R);
	CHECK(P.Load.size() == 1 && P.Load[0] == 4 && P.Unload.empty());

	// Near the north-west corner of the middle cell: 4 cells wanted, one request per update, nearest first.
	P = PlanStreaming(G, L, 1000.f, 1000.f, R);
	CHECK(P.Load.size() == 1 && P.Load[0] == 4);
	L[4] = true;
	int Requested = 1;
	for (int Step = 0; Step < 5; ++Step)
	{
		P = PlanStreaming(G, L, 1000.f, 1000.f, R);
		for (int I : P.Load) { L[I] = true; ++Requested; }
		CHECK(P.Unload.empty());
	}
	CHECK(Requested == 4 && L[0] && L[1] && L[3] && L[4]);

	// Hysteresis: 70 m from the west edge and 20 m from the north edge, cells 3, 1 and 0 stay (< 90 m).
	P = PlanStreaming(G, L, 7000.f, 2000.f, R);
	CHECK(P.Unload.empty() && P.Load.empty());
	// 100 m from both edges: cells 0, 1 and 3 go.
	P = PlanStreaming(G, L, 10000.f, 10000.f, R);
	CHECK(P.Unload.size() == 3 && Has(P.Unload, 0) && Has(P.Unload, 1) && Has(P.Unload, 3) && Has(P.Keep, 4));

	// Cap: 1 m from the corner with a cap of 2, only the 2 nearest stay, the others are released.
	FStreamingRules Cap = R;
	Cap.MaxDetailed = 2;
	std::vector<bool> All4(9, false);
	All4[0] = All4[1] = All4[3] = All4[4] = true;
	P = PlanStreaming(G, All4, 100.f, 50.f, Cap);
	CHECK(P.Keep.size() == 2 && Has(P.Keep, 4) && P.Unload.size() == 2);

	// Teleport far away: everything loaded is released, nothing requested.
	P = PlanStreaming(G, All4, 500000.f, 500000.f, R);
	CHECK(P.Unload.size() == 4 && P.Load.empty());

	// MaxDetailed 0 loads nothing (proxies only).
	FStreamingRules None = R;
	None.MaxDetailed = 0;
	P = PlanStreaming(G, std::vector<bool>(9, false), 12500.f, 12500.f, None);
	CHECK(P.Load.empty());

	std::printf(Failures ? "city streaming: %d failure(s)\n" : "city streaming: all checks passed\n", Failures);
	return Failures ? 1 : 0;
}
