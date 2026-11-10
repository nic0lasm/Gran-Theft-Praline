// Runs the walking logic (GTPWalkLogic.h) on a walkways.csv and checks it.
// Build and run: tools/pedestrians/check_walk_logic.py (no Unreal needed).
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <string>

#include "../../GranTheftPraline/Source/GranTheftPraline/Public/GTPWalkLogic.h"

using namespace GTPWalk;

static bool Load(const char* Path, FGraph& G)
{
	std::ifstream In(Path);
	if (!In)
	{
		return false;
	}
	std::string Line;
	std::getline(In, Line); // header
	while (std::getline(In, Line))
	{
		std::vector<std::string> F;
		std::stringstream SS(Line);
		std::string Cell;
		while (std::getline(SS, Cell, ','))
		{
			F.push_back(Cell);
		}
		if (F.size() < 7)
		{
			continue;
		}
		FEdge E;
		E.Kind = static_cast<EKind>(std::atoi(F[2].c_str()));
		E.NodeA = std::atoi(F[3].c_str());
		E.NodeB = std::atoi(F[4].c_str());
		E.Width = std::atof(F[5].c_str()) * 100.f;
		std::stringstream Pts(F[6]);
		std::string P;
		while (std::getline(Pts, P, ';'))
		{
			FPoint Pt;
			std::sscanf(P.c_str(), "%f %f", &Pt.X, &Pt.Y);
			E.Points.push_back(Pt);
		}
		if (E.Points.size() >= 2)
		{
			G.Edges.push_back(E);
		}
	}
	G.Finalize();
	return !G.Edges.empty();
}

// Fraction of walker-pair samples closer than OverlapCm, with or without the sideways avoidance.
static double Overlap(const FGraph& G, int Count, bool bAvoid, bool bGroups, double* OutPairSpread)
{
	FSettings S;
	FRng Rng(11);
	std::vector<FWalker> W(Count);
	std::vector<int> Leader(Count, -1);
	for (int i = 0; i < Count; ++i)
	{
		Place(G, W[i], Rng, S);
		if (bGroups && i > 0 && Leader[i - 1] < 0 && Rng.Next() < S.GroupChance * 2.f)
		{
			Leader[i] = i - 1;
			Follow(G, W[i], W[i - 1], S);
		}
	}
	const float Dt = 1.f / 15.f;
	long Close = 0, Total = 0;
	double Spread = 0;
	long SpreadN = 0;
	std::vector<FPoint> P(Count), D(Count);
	for (float T = 0; T < 300.f; T += Dt)
	{
		for (int i = 0; i < Count; ++i)
		{
			if (Leader[i] < 0)
			{
				Step(G, W[i], Dt, Rng, S, false);
			}
		}
		for (int i = 0; i < Count; ++i)
		{
			if (Leader[i] >= 0)
			{
				Follow(G, W[i], W[Leader[i]], S);
			}
			Locate(G, W[i], P[i], D[i]);
		}
		for (int i = 0; i < Count; ++i)
		{
			for (int j = 0; j < Count; ++j)
			{
				if (i == j || Leader[i] == j || Leader[j] == i)
				{
					continue;
				}
				if (bAvoid && Leader[i] < 0)
				{
					Separate(G, W[i], P[i], D[i], P[j], S);
				}
			}
		}
		for (int i = 0; i < Count; ++i)
		{
			for (int j = i + 1; j < Count; ++j)
			{
				if (Leader[i] == j || Leader[j] == i)
				{
					if (OutPairSpread)
					{
						Spread = std::max(Spread, static_cast<double>(Distance(P[i], P[j])));
						++SpreadN;
					}
					continue;
				}
				++Total;
				Close += Distance(P[i], P[j]) < 40.f ? 1 : 0;
			}
		}
	}
	if (OutPairSpread)
	{
		*OutPairSpread = SpreadN ? Spread : 0.0;
	}
	return Total ? static_cast<double>(Close) / Total : 0.0;
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: walk_sim walkways.csv [walkers] [seconds]\n");
		return 2;
	}
	FGraph G;
	if (!Load(Argv[1], G))
	{
		std::fprintf(stderr, "cannot read %s\n", Argv[1]);
		return 2;
	}
	const int Walkers = Argc > 2 ? std::atoi(Argv[2]) : 200;
	const float Seconds = Argc > 3 ? static_cast<float>(std::atof(Argv[3])) : 600.f;
	const float Dt = 1.f / 30.f;
	FSettings S;
	FRng Rng(7);
	std::vector<FWalker> W(Walkers);
	std::vector<FPoint> Last(Walkers);
	for (int i = 0; i < Walkers; ++i)
	{
		if (!Place(G, W[i], Rng, S))
		{
			std::fprintf(stderr, "no walkway to place on\n");
			return 1;
		}
		FPoint D;
		Locate(G, W[i], Last[i], D);
	}

	std::set<int> Visited;
	double MovingTime = 0, TotalTime = 0, WaitTime = 0;
	float MaxJump = 0.f;
	int Teleports = 0;
	for (float T = 0; T < Seconds; T += Dt)
	{
		for (int i = 0; i < Walkers; ++i)
		{
			// Every 10th walker meets a "car" at each crossing for the first 3 s: they must wait, then go.
			const bool bUnsafe = (i % 10 == 0) && W[i].CrossingWait < 3.f;
			const FStepResult R = Step(G, W[i], Dt, Rng, S, bUnsafe);
			FPoint P, D;
			Locate(G, W[i], P, D);
			const float Jump = Distance(P, Last[i]);
			// Lateral drift (60 cm/s) and a speed up to 2.3 x 168 cm/s, plus a move to a new
			// edge's chosen line, which is at most half a width away (smoothed by LateralSpeed).
			if (Jump > (S.WalkSpeed * 1.2f * S.FleeSpeedScale + S.LateralSpeed) * Dt + 400.f)
			{
				++Teleports;
			}
			MaxJump = std::max(MaxJump, Jump);
			Last[i] = P;
			Visited.insert(W[i].Edge);
			MovingTime += R.bMoving ? Dt : 0;
			WaitTime += R.bWaitingToCross ? Dt : 0;
			TotalTime += Dt;
		}
	}

	// Fleeing: put a walker on a walkway, a threat behind it, and it must get farther away.
	int FleeOk = 0, FleeTrials = 50;
	for (int k = 0; k < FleeTrials; ++k)
	{
		FWalker F;
		Place(G, F, Rng, S);
		FPoint P0, D0;
		Locate(G, F, P0, D0);
		const FPoint Threat = {P0.X - D0.X * 300.f, P0.Y - D0.Y * 300.f};
		Flee(G, F, Threat, S);
		for (float T = 0; T < 3.f; T += Dt)
		{
			Step(G, F, Dt, Rng, S, false);
		}
		FPoint P1, D1;
		Locate(G, F, P1, D1);
		if (Distance(P1, Threat) > Distance(P0, Threat) + 300.f)
		{
			++FleeOk;
		}
	}

	// Waiting at a crossing: while a car is near (and under CrossingWaitMax) nobody steps onto it.
	int Stepped = 0, Waited = 0;
	for (size_t e = 0; e < G.Edges.size(); ++e)
	{
		if (G.Edges[e].Kind != EKind::Crossing)
		{
			continue;
		}
		FWalker C;
		C.Edge = static_cast<int>(e);
		C.bForward = true;
		C.S = 0.f;
		for (float T = 0; T < S.CrossingWaitMax - 1.f; T += Dt)
		{
			const FStepResult R = Step(G, C, Dt, Rng, S, true);
			Waited += R.bWaitingToCross ? 1 : 0;
		}
		Stepped += C.S > 30.f ? 1 : 0;
	}
	(void)Waited;

	// Density of the M1 profile (12 people) on the real graph, with and without avoidance, and pairs.
	// A crowd five times the density too, where overlaps are frequent enough to compare.
	double PairSpread = 0;
	const double Plain = Overlap(G, 60, false, true, nullptr);
	const double Avoiding = Overlap(G, 60, true, true, &PairSpread);
	const double Plain12 = Overlap(G, 12, false, true, nullptr);
	const double Avoiding12 = Overlap(G, 12, true, true, nullptr);
	std::printf("overlap: 60 people plain=%.5f avoiding=%.5f; 12 people plain=%.5f avoiding=%.5f; max_pair_distance_cm=%.0f\n",
		Plain, Avoiding, Plain12, Avoiding12, PairSpread);

	const double Walking = MovingTime / TotalTime;
	std::printf("edges=%zu nodes=%zu walkers=%d seconds=%.0f visited=%zu walking=%.2f waiting=%.3f max_jump_cm=%.0f teleports=%d flee_ok=%d/%d crossing_breaches=%d\n",
		G.Edges.size(), G.NodeEdges.size(), Walkers, Seconds, Visited.size(), Walking, WaitTime / TotalTime,
		MaxJump, Teleports, FleeOk, FleeTrials, Stepped);
	const bool Pass = Teleports == 0 && Stepped == 0 && Avoiding <= Plain && Avoiding12 <= Plain12 + 0.0005 && PairSpread < 500.0 && Walking > 0.8 && FleeOk >= FleeTrials * 0.9 && Visited.size() * 2 >= G.Edges.size();
	std::printf("%s\n", Pass ? "PASS" : "FAIL");
	return Pass ? 0 : 1;
}
