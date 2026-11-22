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
		E.FacadeSide = F.size() > 7 ? std::atoi(F[7].c_str()) : 0;
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

	// Panic spreads: one frightened walker in a crowd of 60 on the same street frightens some others,
	// never beyond PanicMaxGeneration, and nobody teleports. Gawking: those who take it face the event
	// while they stand, and they walk on afterwards.
	int PanicCaught = 0, PanicMaxGen = 0, GawkFacing = 0, GawkTaken = 0, GawkResumed = 0;
	{
		FRng R2(99);
		std::vector<FWalker> C(60);
		for (auto& w : C)
		{
			Place(G, w, R2, S);
		}
		FPoint Src, SrcD;
		Locate(G, C[0], Src, SrcD);
		Flee(G, C[0], {Src.X - SrcD.X * 200.f, Src.Y - SrcD.Y * 200.f}, S);
		for (float T = 0; T < 20.f; T += Dt)
		{
			for (size_t i = 0; i < C.size(); ++i)
			{
				Step(G, C[i], Dt, R2, S, false);
			}
			for (size_t i = 0; i < C.size(); ++i)
			{
				if (C[i].FleeTimeLeft <= 0.f)
				{
					continue;
				}
				FPoint Pi, Di;
				Locate(G, C[i], Pi, Di);
				for (size_t j = 0; j < C.size(); ++j)
				{
					FPoint Pj, Dj;
					Locate(G, C[j], Pj, Dj);
					if (i != j && Distance(Pi, Pj) < S.PanicRadius && CatchPanic(G, C[j], C[i].Generation, Pi, S, R2))
					{
						++PanicCaught;
						PanicMaxGen = std::max(PanicMaxGen, C[j].Generation);
					}
				}
			}
		}
		const FPoint Event = Src;
		for (size_t i = 1; i < C.size(); ++i)
		{
			C[i].FleeTimeLeft = 0.f;
			C[i].Pause = 0.f;
			FPoint Pi, Di;
			Locate(G, C[i], Pi, Di);
			if (Distance(Pi, Event) < 5.f || !Gawk(C[i], Event, R2, S))
			{
				continue;
			}
			++GawkTaken;
			const FPoint F = Facing(G, C[i]);
			const float Dx = Event.X - Pi.X, Dy = Event.Y - Pi.Y;
			const float Len = std::sqrt(Dx * Dx + Dy * Dy);
			GawkFacing += (F.X * Dx + F.Y * Dy) / Len > 0.95f ? 1 : 0;
			const float Wait = C[i].Pause;
			bool Moved = false;
			for (float T = 0; T < Wait + 2.f; T += Dt)
			{
				const FStepResult Rs = Step(G, C[i], Dt, R2, S, false);
				Moved |= Rs.bMoving && T > Wait;
			}
			GawkResumed += Moved ? 1 : 0;
		}
	}
	std::printf("panic: caught=%d max_generation=%d; gawk: taken=%d facing=%d resumed=%d\n", PanicCaught, PanicMaxGen, GawkTaken, GawkFacing, GawkResumed);

	// Held up: a running walker frozen facing the aggressor stays put and faces it until released.
	int HeldOk = 0, HeldTrials = 20;
	for (int k = 0; k < HeldTrials; ++k)
	{
		FWalker H;
		Place(G, H, Rng, S);
		FPoint P0, D0;
		Locate(G, H, P0, D0);
		const FPoint Aggressor = {P0.X + D0.Y * 400.f + 50.f, P0.Y - D0.X * 400.f};
		Flee(G, H, Aggressor, S);
		Freeze(H, Aggressor, 5.f, EPause::HandsUp);
		Locate(G, H, P0, D0);
		bool Still = true;
		for (float T = 0; T < 4.f; T += Dt)
		{
			Still &= !Step(G, H, Dt, Rng, S, false).bMoving;
		}
		FPoint P1, D1;
		Locate(G, H, P1, D1);
		const FPoint F = Facing(G, H);
		const float Dx = Aggressor.X - P1.X, Dy = Aggressor.Y - P1.Y;
		const float Len = std::sqrt(Dx * Dx + Dy * Dy);
		HeldOk += (Still && Distance(P0, P1) < 5.f && (F.X * Dx + F.Y * Dy) / Len > 0.95f) ? 1 : 0;
	}
	std::printf("held up: %d/%d still and facing the aggressor\n", HeldOk, HeldTrials);

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

	// A steady flow of people at a crossing where a car waits: one by one they never let it go; in groups, on the
	// walk phase of the crossing, the road is free for most of each cycle, and nobody waits more than a cycle.
	auto Flow = [&G, Dt](const FSettings& Rule, bool bWait, double& LongestBusy, double& FreeShare, double& LongestWait, int& Crossed) {
		int Crossing = -1;
		for (size_t e = 0; e < G.Edges.size(); ++e)
		{
			// The longest crossing of the graph: the widest road.
			if (G.Edges[e].Kind == EKind::Crossing && (Crossing < 0 || G.Edges[e].Length > G.Edges[Crossing].Length))
			{
				Crossing = static_cast<int>(e);
			}
		}
		LongestBusy = FreeShare = LongestWait = 0.0;
		Crossed = 0;
		if (Crossing < 0)
		{
			return;
		}
		FRng FlowRng(77u);
		std::vector<FWalker> People;
		double Busy = 0.0, Free = 0.0, NextArrival = 0.0;
		const double Seconds = 300.0;
		for (double T = 0.0; T < Seconds; T += Dt)
		{
			if (T >= NextArrival)
			{
				NextArrival += 2.0;
				FWalker W;
				W.Edge = Crossing;
				W.bForward = (People.size() % 2) == 0;
				W.S = 0.f;
				People.push_back(W);
			}
			bool bOn = false;
			for (FWalker& W : People)
			{
				if (W.Edge != Crossing)
				{
					continue;
				}
				const bool bUnsafe = bWait && W.S < 30.f && CrossingUnsafe(Crossing, T, true, false, Rule);
				const float Before = W.S;
				Step(G, W, Dt, FlowRng, Rule, bUnsafe);
				if (W.Edge != Crossing)
				{
					++Crossed;
				}
				else if (W.S >= 30.f || Before >= 30.f)
				{
					bOn = true;
				}
				LongestWait = std::max(LongestWait, static_cast<double>(W.Edge == Crossing ? W.CrossingWait : 0.f));
			}
			Busy = bOn ? Busy + Dt : 0.0;
			Free += bOn ? 0.0 : Dt;
			LongestBusy = std::max(LongestBusy, Busy);
		}
		FreeShare = Free / Seconds;
	};
	FSettings OneByOne = S;
	FSettings Phased = S;
	Phased.CrossingCycleSeconds = 30.f;
	Phased.CrossingWalkSeconds = 8.f;
	Phased.CrossingWaitMax = 45.f;
	double BusyA, FreeA, WaitA, BusyB, FreeB, WaitB;
	int CrossedA, CrossedB;
	Flow(OneByOne, false, BusyA, FreeA, WaitA, CrossedA);
	Flow(Phased, true, BusyB, FreeB, WaitB, CrossedB);
	std::printf("crossing flow, one person every 2 s for 300 s with a car waiting: one by one busy %.0f s in a row, free %.0f %%, %d crossed; "
		"in groups busy %.0f s in a row, free %.0f %%, longest wait %.0f s, %d crossed\n",
		BusyA, FreeA * 100.0, CrossedA, BusyB, FreeB * 100.0, WaitB, CrossedB);
	const bool FlowOk = CrossedA > 0 && BusyB <= 25.0 && FreeB >= 0.3 && WaitB <= 30.5 && CrossedB >= CrossedA * 8 / 10;

	// What a car is to a crossing (LookAtCar): a crossing 8 m long across a road along X, cars 2.5 m each side of their centre.
	int LookOk = 0, LookTrials = 0;
	{
		FEdge Zebra;
		Zebra.Kind = EKind::Crossing;
		Zebra.Points = {{0.f, -400.f}, {0.f, 400.f}};
		Zebra.Length = 800.f;
		struct FCase { float X, Y, Fx, Fy, Speed; bool bNear, bDangerous; const char* What; };
		const FCase Cases[] = {
			{-600.f, 0.f, 1.f, 0.f, 0.f, true, false, "stopped 3.5 m before it"},
			{-600.f, 0.f, 1.f, 0.f, 100.f, true, false, "creeping 3.5 m before it"},
			{-2000.f, 0.f, 1.f, 0.f, 0.f, false, false, "stopped 17.5 m before it"},
			{-2000.f, 0.f, 1.f, 0.f, 1000.f, true, true, "coming at 36 km/h, 1.75 s away"},
			{-6000.f, 0.f, 1.f, 0.f, 1000.f, false, false, "coming at 36 km/h, 5.75 s away"},
			{100.f, 0.f, 1.f, 0.f, 300.f, true, true, "rolling over it"},
			{100.f, 0.f, 1.f, 0.f, 0.f, true, false, "stopped on it"},
			{600.f, 0.f, 1.f, 0.f, 800.f, false, false, "past it, driving away"},
			{600.f, 0.f, -1.f, 0.f, 800.f, true, true, "the other way, coming"},
			{-600.f, 900.f, 1.f, 0.f, 800.f, false, false, "in the next street, 5 m past the end of the crossing"},
			{-300.f, -1500.f, 0.f, 1.f, 800.f, false, false, "driving along the crossing, 3 m beside it"},
			{-600.f, 500.f, 1.f, 0.f, 0.f, true, false, "stopped beside the kerb, 1 m past the end of the crossing"},
		};
		for (const FCase& C : Cases)
		{
			bool bNear = false, bDangerous = false;
			LookAtCar(Zebra, FPoint{C.X, C.Y}, FPoint{C.Fx, C.Fy}, C.Speed, 250.f, S, bNear, bDangerous);
			++LookTrials;
			if (bNear == C.bNear && bDangerous == C.bDangerous)
			{
				++LookOk;
			}
			else
			{
				std::printf("crossing look WRONG for a car %s: near %d (wanted %d), dangerous %d (wanted %d)\n", C.What, bNear, C.bNear, bDangerous, C.bDangerous);
			}
		}
		std::printf("crossing look: %d/%d cars read right\n", LookOk, LookTrials);
	}

	// Both sides get through: with cars coming one after the other for ever, a person still crosses once the longest
	// wait is over; with a car stopped at the crossing for ever, at the next walk phase.
	FWalker Patient;
	for (size_t e = 0; e < G.Edges.size(); ++e)
	{
		if (G.Edges[e].Kind == EKind::Crossing && (Patient.Edge < 0 || G.Edges[e].Length > G.Edges[Patient.Edge].Length))
		{
			Patient.Edge = static_cast<int>(e);
		}
	}
	const int PatientEdge = Patient.Edge;
	FRng PatientRng(5u);
	double EndlessCars = -1.0, StoppedCar = -1.0;
	for (double T = 0.0; T < 120.0 && EndlessCars < 0.0; T += Dt)
	{
		Step(G, Patient, Dt, PatientRng, Phased, Patient.S < 30.f && CrossingUnsafe(PatientEdge, T, true, true, Phased));
		EndlessCars = Patient.S >= 30.f || Patient.Edge != PatientEdge ? T : -1.0;
	}
	Patient = FWalker();
	Patient.Edge = PatientEdge;
	// Arrives just as the walk phase of this crossing ends: the longest wait there is.
	double Start = 0.0;
	while (!CrossingWalkPhase(PatientEdge, Start, Phased) || CrossingWalkPhase(PatientEdge, Start + Dt, Phased))
	{
		Start += Dt;
	}
	Start += Dt;
	for (double T = Start; T < Start + 120.0 && StoppedCar < 0.0; T += Dt)
	{
		Step(G, Patient, Dt, PatientRng, Phased, Patient.S < 30.f && CrossingUnsafe(PatientEdge, T, true, false, Phased));
		StoppedCar = Patient.S >= 30.f || Patient.Edge != PatientEdge ? T - Start : -1.0;
	}
	std::printf("crossing wait: with cars coming for ever a person steps off after %.0f s (longest wait %.0f s); with a car stopped there for ever, after %.0f s at worst (cycle %.0f s, walk %.0f s)\n",
		EndlessCars, Phased.CrossingWaitMax, StoppedCar, Phased.CrossingCycleSeconds, Phased.CrossingWalkSeconds);
	const bool WaitOk = PatientEdge >= 0 && EndlessCars >= Phased.CrossingWaitMax - 1.0 && EndlessCars <= Phased.CrossingWaitMax + 1.0
		&& StoppedCar >= 0.0 && StoppedCar <= Phased.CrossingCycleSeconds - Phased.CrossingWalkSeconds + 1.0;

	// A car standing on a walkway: nobody walks through its body. On a wide walkway people pass beside it; on a
	// narrow one they wait, then turn back. Without the avoidance they walk straight through.
	auto Street = [Dt](float Width, bool bAvoid, int& InsideFrames, int& Passed, int& TurnedBack, double& LongestStand) {
		FGraph Lane;
		FEdge E;
		E.NodeA = 0;
		E.NodeB = 1;
		E.Kind = EKind::Pedestrian;
		E.Width = Width;
		E.Points = {{0.f, 0.f}, {3000.f, 0.f}};
		Lane.Edges.push_back(E);
		Lane.Finalize();
		FSettings Rule;
		Rule.PauseChancePerSecond = 0.f;
		FCarBox Car;
		Car.Center = {1500.f, 0.f};
		Car.Forward = {1.f, 0.f};
		FRng LaneRng(31u);
		std::vector<FWalker> People(20);
		std::vector<double> Standing(People.size(), 0.0);
		for (size_t i = 0; i < People.size(); ++i)
		{
			People[i].Edge = 0;
			People[i].bForward = (i % 2) == 0;
			People[i].S = LaneRng.Range(0.f, 900.f);
			People[i].Lateral = People[i].LateralTarget = LaneRng.Range(-Width * 0.5f, Width * 0.5f);
		}
		InsideFrames = Passed = TurnedBack = 0;
		LongestStand = 0.0;
		for (double T = 0.0; T < 180.0; T += Dt)
		{
			for (size_t i = 0; i < People.size(); ++i)
			{
				FWalker& W = People[i];
				FPoint Before, Dir;
				Locate(Lane, W, Before, Dir);
				const bool bWasForward = W.bForward;
				const EAround Around = bAvoid ? AvoidBox(Lane, W, Car, Rule, Dt) : EAround::Clear;
				if (Around != EAround::Wait)
				{
					Step(Lane, W, Dt, LaneRng, Rule, false);
				}
				FPoint After;
				Locate(Lane, W, After, Dir);
				InsideFrames += InCarBox(Car, After, 0.f) ? 1 : 0;
				Passed += (Before.X - Car.Center.X) * (After.X - Car.Center.X) < 0.f ? 1 : 0;
				TurnedBack += Around == EAround::Wait && W.bForward != bWasForward ? 1 : 0;
				Standing[i] = Around == EAround::Wait ? Standing[i] + Dt : 0.0;
				LongestStand = std::max(LongestStand, Standing[i]);
			}
		}
	};
	int ThroughInside, ThroughPassed, ThroughBack, WideInside, WidePassed, WideBack, NarrowInside, NarrowPassed, NarrowBack;
	double ThroughStand, WideStand, NarrowStand;
	Street(700.f, false, ThroughInside, ThroughPassed, ThroughBack, ThroughStand);
	Street(700.f, true, WideInside, WidePassed, WideBack, WideStand);
	Street(200.f, true, NarrowInside, NarrowPassed, NarrowBack, NarrowStand);
	std::printf("car on a walkway, 20 people for 180 s: not avoided, %d frames inside its body, %d passages; avoided on a 7 m walkway, %d frames inside, %d passages, %d turned back, "
		"longest stand %.1f s; on a 2 m walkway, %d frames inside, %d passages, %d turned back, longest stand %.1f s\n",
		ThroughInside, ThroughPassed, WideInside, WidePassed, WideBack, WideStand, NarrowInside, NarrowPassed, NarrowBack, NarrowStand);
	const bool CarOk = ThroughInside > 0 && WideInside == 0 && WidePassed >= ThroughPassed / 2 && WideStand <= FSettings().CarWaitSeconds + 0.5
		&& NarrowInside == 0 && NarrowPassed == 0 && NarrowBack > 0 && NarrowStand <= FSettings().CarWaitSeconds + 0.5;

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
	const bool Pass = Teleports == 0 && Stepped == 0 && Avoiding <= Plain && Avoiding12 <= Plain12 + 0.0005 && PairSpread < 500.0 && Walking > 0.8 && FleeOk >= FleeTrials * 0.9 && PanicMaxGen <= S.PanicMaxGeneration && PanicCaught >= 1 && HeldOk == HeldTrials && GawkTaken > 5 && GawkFacing == GawkTaken && GawkResumed >= GawkTaken * 0.9 && Visited.size() * 2 >= G.Edges.size() && FlowOk && LookOk == LookTrials && WaitOk && CarOk;
	std::printf("%s\n", Pass ? "PASS" : "FAIL");
	return Pass ? 0 : 1;
}
