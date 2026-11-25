// People get out of the way of a car that comes fast (GTPWalkLogic.h: InTheWay, Dodge, and the kerb rule of Step).
//
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/pedestrians/dodge_test.cpp -o dodge_test
//
// A car is driven straight through people who walk on small graphs made here, with the rule the game knocks them
// down with (GTPImpact::CarReaches). Each scene is played twice, with and without the new rule: the first run says
// what the scene costs when nobody moves (gate 93: 47 people run over in one flight), the second what is left.

#include "GTPImpactLogic.h"
#include "GTPWalkLogic.h"

#include <cstdio>
#include <vector>

using namespace GTPWalk;

static int Failures = 0;

static void Check(bool bOk, const char* What)
{
	std::printf("%s  %s\n", bOk ? "ok  " : "FAIL", What);
	Failures += bOk ? 0 : 1;
}

static FEdge Edge(int A, int B, EKind Kind, float Width, FPoint From, FPoint To)
{
	FEdge E;
	E.NodeA = A;
	E.NodeB = B;
	E.Kind = Kind;
	E.Width = Width;
	E.Points = {From, To};
	return E;
}

struct FScene
{
	FGraph G;
	std::vector<FWalker> People;
	FFastCar Car;
	float Seconds = 12.f;

	/** The car starts Before in front of x = 0 and the scene lasts until it is Past it, and 6 s more. */
	void Drive(float Kmh, float Before, float Past, float Y)
	{
		Car.Speed = Kmh / 3.6f * 100.f;
		Car.Location = {-Before, Y};
		Seconds = (Before + Past) / Car.Speed + 6.f;
	}
};

struct FOutcome
{
	int Hit = 0;
	int Leapt = 0;
	int Ran = 0;
	int Saw = 0;
	int StillAside = 0;
	float FastestStep = 0.f; // cm/s, the fastest anybody moved in one frame
	std::vector<FPoint> End;
};

static FOutcome Play(FScene Scene, bool bDodge, const FSettings& S)
{
	FOutcome Out;
	FRng Rng(11);
	const float Dt = 1.f / 60.f;
	std::vector<FPoint> Last(Scene.People.size());
	std::vector<bool> bSaw(Scene.People.size(), false);
	for (size_t I = 0; I < Scene.People.size(); ++I)
	{
		FPoint D;
		Locate(Scene.G, Scene.People[I], Last[I], D);
	}
	for (float T = 0.f; T < Scene.Seconds; T += Dt)
	{
		Scene.Car.Location.X += Scene.Car.Forward.X * Scene.Car.Speed * Dt;
		Scene.Car.Location.Y += Scene.Car.Forward.Y * Scene.Car.Speed * Dt;
		for (size_t I = 0; I < Scene.People.size(); ++I)
		{
			FWalker& W = Scene.People[I];
			if (W.Down > 0.f)
			{
				W.Down = 100.f;
				continue;
			}
			FPoint P, D;
			Locate(Scene.G, W, P, D);
			float Side = 0.f, Seconds = 0.f;
			if (bDodge && ComesAt(Scene.G, W, Scene.Car, S, Side, Seconds))
			{
				const FDodgeResult R = Dodge(Scene.G, W, Scene.Car, Side, Seconds, S, Rng, Dt);
				Out.Leapt += R.bStarted && R.What == EDodge::Sideways ? 1 : 0;
				Out.Ran += R.bStarted && R.What == EDodge::Along ? 1 : 0;
				if (R.What != EDodge::None && !bSaw[I])
				{
					bSaw[I] = true;
					++Out.Saw;
				}
			}
			else
			{
				W.DodgeReact = -1.f;
			}
			Step(Scene.G, W, Dt, Rng, S, false);
			Locate(Scene.G, W, P, D);
			Out.FastestStep = std::max(Out.FastestStep, Distance(P, Last[I]) / Dt);
			Last[I] = P;
			const float Dx = P.X - Scene.Car.Location.X, Dy = P.Y - Scene.Car.Location.Y;
			const float Along = Dx * Scene.Car.Forward.X + Dy * Scene.Car.Forward.Y;
			const float Across = Dy * Scene.Car.Forward.X - Dx * Scene.Car.Forward.Y;
			if (GTPImpact::CarReaches(Along, Across, Scene.Car.HalfLength, Scene.Car.HalfWidth, Scene.Car.Speed))
			{
				W.Down = 100.f;
				++Out.Hit;
			}
		}
	}
	for (FWalker& W : Scene.People)
	{
		FRng Unused(3);
		FWalker Copy = W;
		Out.StillAside += W.Down <= 0.f && Step(Scene.G, Copy, Dt, Unused, S, false).bStandsAside ? 1 : 0;
	}
	Out.End = Last;
	return Out;
}

// A street closed to cars, 80 m long in four edges, with people all over its width; the car drives along it.
static FScene PedestrianStreet(float CarKmh, float CarY, float Width, int People)
{
	FScene Scene;
	for (int I = 0; I < 4; ++I)
	{
		Scene.G.Edges.push_back(Edge(I, I + 1, EKind::Pedestrian, Width, {I * 2000.f, 0.f}, {(I + 1) * 2000.f, 0.f}));
	}
	Scene.G.Finalize();
	FRng Rng(5);
	for (int I = 0; I < People; ++I)
	{
		FWalker W;
		W.Edge = I % 4;
		W.bForward = (I / 4) % 2 == 0;
		W.S = Rng.Range(0.f, 2000.f);
		W.SpeedScale = Rng.Range(0.8f, 1.2f);
		W.Lateral = W.LateralTarget = Rng.Range(-Width * 0.5f, Width * 0.5f);
		Scene.People.push_back(W);
	}
	Scene.Drive(CarKmh, 4000.f, 9000.f, CarY);
	return Scene;
}

// A road along x with a crossing over it at x = 0, 10 m long, and a pavement on each side; people are on the crossing.
static FScene Crossing(float CarKmh, float CarY, int People)
{
	FScene Scene;
	Scene.G.Edges.push_back(Edge(0, 1, EKind::Sidewalk, 200.f, {-3000.f, -500.f}, {0.f, -500.f}));
	Scene.G.Edges.push_back(Edge(1, 2, EKind::Crossing, 300.f, {0.f, -500.f}, {0.f, 500.f}));
	Scene.G.Edges.push_back(Edge(2, 3, EKind::Sidewalk, 200.f, {0.f, 500.f}, {3000.f, 500.f}));
	Scene.G.Edges.push_back(Edge(1, 4, EKind::Sidewalk, 200.f, {0.f, -500.f}, {3000.f, -500.f}));
	Scene.G.Edges.push_back(Edge(2, 5, EKind::Sidewalk, 200.f, {0.f, 500.f}, {-3000.f, 500.f}));
	Scene.G.Finalize();
	for (int I = 0; I < People; ++I)
	{
		FWalker W;
		W.Edge = 1;
		W.bForward = I % 2 == 0;
		W.S = 60.f + 880.f * static_cast<float>(I) / static_cast<float>(People - 1);
		W.Lateral = W.LateralTarget = (I % 3 - 1) * 80.f;
		Scene.People.push_back(W);
	}
	// 2.5 s away: those who are on the crossing now are still on it when it comes.
	Scene.Drive(CarKmh, CarKmh / 3.6f * 100.f * 2.5f, 1000.f, CarY);
	return Scene;
}

// A pavement along a road, its line 3 m from the car's: the car passes people by, as cars do all day long.
static FScene Pavement(float CarKmh, int People)
{
	FScene Scene;
	Scene.G.Edges.push_back(Edge(0, 1, EKind::Sidewalk, 200.f, {0.f, 300.f}, {4000.f, 300.f}));
	Scene.G.Edges.push_back(Edge(1, 2, EKind::Sidewalk, 200.f, {4000.f, 300.f}, {8000.f, 300.f}));
	Scene.G.Finalize();
	FRng Rng(9);
	for (int I = 0; I < People; ++I)
	{
		FWalker W;
		W.Edge = I % 2;
		W.bForward = (I / 2) % 2 == 0;
		W.S = Rng.Range(0.f, 4000.f);
		W.Lateral = W.LateralTarget = Rng.Range(-100.f, 100.f);
		Scene.People.push_back(W);
	}
	Scene.Drive(CarKmh, 4000.f, 9000.f, 0.f);
	return Scene;
}

int main()
{
	// The game's settings, and the same with nobody slow to see the car (one in eight is, in the game): the scenes
	// are first played with everybody quick, where the rule must save everybody, then as the game plays them.
	const FSettings Game;
	FSettings S;
	S.DodgeSlowShare = 0.f;
	// The fastest a person may be seen to move: a leap sideways while the offset kept at the end of an edge comes
	// back, or a run with the usual drift to his line.
	const float Leap = 2.f * S.DodgeSideSpeed + 5.f;
	const float RunDrift = S.WalkSpeed * 1.2f * S.FleeSpeedScale + S.LateralSpeed + 5.f;
	const float Fastest = std::max(Leap, RunDrift);

	std::printf("-- what is in a car's way\n");
	{
		FFastCar Car;
		Car.Location = {0.f, 0.f};
		Car.Speed = 2000.f;
		float Side = 0.f, Seconds = 0.f;
		Check(InTheWay(Car, {1500.f, 50.f}, S, Side, Seconds) && Side < 0.f && std::fabs(Seconds - (1500.f - 240.f) / 2000.f) < 0.01f,
			"15 m in front of a car at 72 km/h, half a metre to its right: in its way, 0.63 s before its front");
		Check(!InTheWay(Car, {1500.f, 180.f}, S, Side, Seconds), "1.8 m to the side of its line (0.8 m from its flank): not in its way");
		Check(InTheWay(Car, {1500.f, -160.f}, S, Side, Seconds) && Side > 0.f, "1.6 m to its left: in its way, on its left");
		Check(!InTheWay(Car, {3300.f, 0.f}, S, Side, Seconds), "33 m in front, more than 1.5 s away: not yet");
		Check(!InTheWay(Car, {-300.f, 0.f}, S, Side, Seconds), "behind it: not in its way");
		Check(InTheWay(Car, {100.f, 150.f}, S, Side, Seconds) && Seconds == 0.f, "beside its body, half a metre from its flank: in its way now");
		Car.Speed = 2750.f;
		Check(InTheWay(Car, {3600.f, 0.f}, S, Side, Seconds) && !InTheWay(Car, {3800.f, 0.f}, S, Side, Seconds), "at 99 km/h: seen 35 m in front and no further");
		Car.Speed = 0.f;
		Check(!InTheWay(Car, {600.f, 0.f}, S, Side, Seconds), "a car that stands is in nobody's way");
	}

	std::printf("-- a street closed to cars, the car drives along its middle\n");
	for (const float Kmh : {26.f, 50.f, 73.f, 99.f})
	{
		const FScene Scene = PedestrianStreet(Kmh, 0.f, 600.f, 80);
		const FOutcome Before = Play(Scene, false, S);
		const FOutcome After = Play(Scene, true, S);
		std::printf("     %2.0f km/h: %d of 80 run over when nobody moves; with the rule %d (%d leapt aside, %d ran along, fastest move %.0f cm/s, %d still aside at the end)\n",
			Kmh, Before.Hit, After.Hit, After.Leapt, After.Ran, After.FastestStep, After.StillAside);
		Check(Before.Hit >= 10, "when nobody moves, ten people or more are run over");
		Check(After.Hit == 0, "with the rule nobody is");
		Check(After.Leapt >= Before.Hit, "at least those who were run over leapt aside");
		Check(After.FastestStep <= Fastest, "nobody moves faster than a leap or a run (no jump from one place to another)");
		Check(After.StillAside == 0, "once the car has passed everybody walks on");
	}

	std::printf("-- the same street, the car 1.5 m off its middle and 2 m off the next line of walkers\n");
	{
		const FScene Scene = PedestrianStreet(73.f, 150.f, 600.f, 80);
		const FOutcome Before = Play(Scene, false, S);
		const FOutcome After = Play(Scene, true, S);
		std::printf("     %d run over when nobody moves; with the rule %d (%d leapt, %d ran)\n", Before.Hit, After.Hit, After.Leapt, After.Ran);
		Check(Before.Hit >= 10 && After.Hit == 0, "ten or more run over before, nobody with the rule");
		Check(After.FastestStep <= Fastest && After.StillAside == 0, "no jump, and everybody walks on afterwards");
	}

	std::printf("-- the same street as the game plays it: one in eight sees the car late\n");
	{
		int Before = 0, After = 0;
		for (const float Kmh : {26.f, 50.f, 73.f, 99.f})
		{
			for (const float CarY : {-150.f, 0.f, 150.f})
			{
				const FScene Scene = PedestrianStreet(Kmh, CarY, 600.f, 80);
				const FOutcome Still = Play(Scene, false, Game);
				const FOutcome Moving = Play(Scene, true, Game);
				std::printf("     %2.0f km/h, %+.1f m: %d run over when nobody moves, %d with the rule\n", Kmh, CarY / 100.f, Still.Hit, Moving.Hit);
				Before += Still.Hit;
				After += Moving.Hit;
				Check(Moving.FastestStep <= Fastest && Moving.StillAside == 0, "no jump, and everybody walks on afterwards");
			}
		}
		std::printf("     in all: %d run over when nobody moves, %d with the rule\n", Before, After);
		Check(After >= 1 && After * 10 <= Before, "a few are still run over, fewer than one in ten of those who were");
	}

	std::printf("-- a crossing, the car drives over it\n");
	for (const float Kmh : {26.f, 60.f, 99.f})
	{
		for (const float CarY : {-150.f, 0.f, 150.f})
		{
			const FScene Scene = Crossing(Kmh, CarY, 12);
			const FOutcome Before = Play(Scene, false, S);
			const FOutcome After = Play(Scene, true, S);
			std::printf("     %2.0f km/h, %+.1f m from the middle of the road: %d of 12 run over when nobody moves; with the rule %d (%d ran along the crossing, %d leapt)\n",
				Kmh, CarY / 100.f, Before.Hit, After.Hit, After.Ran, After.Leapt);
			Check(Before.Hit >= 1 && After.Hit == 0, "somebody run over before, nobody with the rule");
			Check(After.Ran >= Before.Hit && After.Leapt == 0, "they run off along the crossing, and nobody leaps");
			Check(After.FastestStep <= Fastest && After.StillAside == 0, "no jump, and everybody walks on afterwards");
		}
	}

	std::printf("-- a pavement the car passes by, 3 m from its line\n");
	for (const float Kmh : {50.f, 99.f})
	{
		const FScene Scene = Pavement(Kmh, 40);
		const FOutcome Before = Play(Scene, false, S);
		const FOutcome After = Play(Scene, true, S);
		bool bSame = Before.End.size() == After.End.size();
		for (size_t I = 0; bSame && I < Before.End.size(); ++I)
		{
			bSame = Distance(Before.End[I], After.End[I]) < 0.01f;
		}
		Check(Before.Hit == 0 && After.Saw == 0 && bSame, "nobody is in its way and nobody does anything he would not have done");
	}

	std::printf("-- a narrow pavement the car drives along: no room to leap to\n");
	{
		FScene Scene = Pavement(60.f, 40);
		Scene.Car.Location.Y = 300.f;
		const FOutcome After = Play(Scene, true, S);
		std::printf("     %d of 40 run over, %d leapt as far aside as the pavement goes\n", After.Hit, After.Leapt);
		Check(After.Leapt >= 30 && After.FastestStep <= Fastest && After.StillAside == 0, "they go as far aside as they can, without a jump, and walk on afterwards if they still can");
	}

	std::printf("-- who does not leap\n");
	{
		FScene Scene = PedestrianStreet(73.f, 0.f, 600.f, 1);
		FWalker& W = Scene.People[0];
		W.S = 1000.f;
		W.Lateral = W.LateralTarget = 20.f;
		FFastCar Car;
		Car.Location = {W.Edge * 2000.f + (W.bForward ? 1000.f : 1000.f) - 1500.f, 0.f};
		Car.Speed = 2000.f;
		FRng Rng(2);
		FPoint P, D;
		Locate(Scene.G, W, P, D);
		float Side = 0.f, Seconds = 0.f;
		Check(InTheWay(Car, P, S, Side, Seconds), "(the car is coming at him)");
		FWalker Down = W;
		Down.Down = 1.f;
		Check(Dodge(Scene.G, Down, Car, Side, Seconds, S, Rng, 0.5f).What == EDodge::None && Down.DodgeTimeLeft <= 0.f, "somebody lying on the ground, run over, stays there");
		FWalker Held = W;
		Freeze(Held, {0.f, 0.f}, 60.f, EPause::HandsUp);
		Check(Dodge(Scene.G, Held, Car, Side, Seconds, S, Rng, 0.5f).What == EDodge::None && Held.Pause > 59.f, "somebody held at gunpoint keeps his hands up");
		FWalker Phone = W;
		Phone.Pause = 5.f;
		Phone.PauseKind = EPause::Phone;
		Check(Dodge(Scene.G, Phone, Car, Side, Seconds, S, Rng, 0.01f).What == EDodge::Sees && Phone.Pause > 4.f, "somebody on his phone takes a moment to see it");
		const FDodgeResult R = Dodge(Scene.G, Phone, Car, Side, Seconds, S, Rng, 0.5f);
		Check(R.What == EDodge::Sideways && R.bStarted && Phone.Pause <= 0.f && Phone.DodgeTimeLeft > Seconds, "then drops it and leaps, and stays aside longer than the car takes to reach him");
		Check(Phone.LateralTarget > 150.f, "to the side he was already on");
	}

	std::printf("-- somebody who runs from a siren and comes to a crossing\n");
	{
		const FScene Scene = Crossing(60.f, 0.f, 2);
		FRng Rng(4);
		FWalker W;
		W.Edge = 1;
		W.S = 5.f;
		W.FleeTimeLeft = 4.f;
		FWalker Before = W;
		const FStepResult Old = Step(Scene.G, Before, 0.1f, Rng, S, true);
		Check(Old.bMoving && !Old.bWaitingToCross, "before: he ran onto it whatever came (the car is only 'unsafe' to him)");
		FWalker After = W;
		const FStepResult New = Step(Scene.G, After, 0.1f, Rng, S, true, true);
		Check(!New.bMoving && New.bWaitingToCross && New.bFleeing && After.S == W.S, "now: a car about to pass keeps him at the kerb");
		FWalker Free = W;
		Check(Step(Scene.G, Free, 0.1f, Rng, S, false, false).bMoving, "no car: he runs across");
		FWalker Calm = W;
		Calm.FleeTimeLeft = 0.f;
		Check(Step(Scene.G, Calm, 0.1f, Rng, S, true, true).bWaitingToCross, "somebody calm waits as before");
		// At the kerb the way of the car is 1.3 m in front of him: he stays there, he does not run back from it.
		FFastCar Car;
		Car.Location = {-1500.f, -200.f};
		Car.Speed = 1667.f;
		float Side = 0.f, Seconds = 0.f;
		FWalker Kerb = W;
		Kerb.FleeTimeLeft = 0.f;
		Check(!ComesAt(Scene.G, Kerb, Car, S, Side, Seconds), "at the kerb, a car about to pass in front of him does not make him run back");
		Kerb.S = 60.f;
		Check(ComesAt(Scene.G, Kerb, Car, S, Side, Seconds) && std::fabs(Side) > 100.f + S.DodgeClear, "once he has stepped off, it does: his walk takes him into its way");
		FWalker Long = W;
		Long.CrossingWait = S.CrossingWaitMax;
		Check(Step(Scene.G, Long, 0.1f, Rng, S, true, true).bMoving, "and nobody waits for ever");
	}

	std::printf("%s\n", Failures == 0 ? "PASS" : "FAIL");
	return Failures == 0 ? 0 : 1;
}
