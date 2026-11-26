// The traffic around a player who stands still, as the game runs it: cars that slide along the lanes
// (GTPTrafficGhost.h) under the game's own rules (GTPDriveLogic.h, GTPTrafficLights.h, GTPTrafficHeld.h), put on the
// road in a ring around him and taken away when they leave it or stand too long out of his sight.
//
// Written after gates 103 to 106: during the filmed flight 13 to 24 cars moved within 150 m of the player, and once
// he stood still 0 to 9 of 28 did, held by "the queue along the lanes". On the film of gate 104 the car he stole
// stands astride the middle of a two-lane street. This puts that car on the lanes ("his-car": the lane he stands on
// and the one of the other way beside it are shut where he stands, and no car can go round) and counts what moves
// around him under the rules of gate 106 and under those written since.
//
// Build: g++ -O2 -std=c++17 -I ../../../GranTheftPraline/Source/GranTheftPraline/Public standing_sim.cpp -o standing_sim
// Run:   ./standing_sim lanes.csv X Y [cars] [seconds] [options]     (X Y: where the player stands, metres)
//        ./standing_sim grid                                         (no file: a grid of two-way streets, and the
//                                                                     figures the rules have to keep; exit code 1 if not)
//   options, each a word:  his-car         the car he left stands astride the middle of his street
//                          one-at-a-time   a junction lets one car through at a time (the game until gate 106)
//                          hold-junction   a car stopped in a queue keeps the junction it asked for (until gate 106)
//                          no-keep-off     cars drive into a lane that is held (until gate 106)
//                          gate-106        the three above
//                          no-lights       no traffic light
//                          quiet           the summary line only
#include "GTPDriveLogic.h"
#include "GTPTrafficGhost.h"
#include "GTPTrafficHeld.h"
#include "GTPTrafficLights.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	struct FLane
	{
		std::vector<FGTPDrivePoint> Points; // metres
		std::vector<int> Next;
		float Speed = 8.3f;
		int ToJunction = -1;
	};

	struct FCar
	{
		int Id = 0;
		bool bOnRoad = false;
		float X = 0.f, Y = 0.f, Yaw = 0.f, V = 0.f, Along = 0.f, Stopped = 0.f;
		int Closest = 0, LaneLength = 0;
		std::vector<int> Route;
		std::vector<FGTPDrivePoint> Path;
		FGTPJunctionClaims Junctions;
		int Limit = 0; // 0 none, 1 queue, 2 red light, 3 junction, 4 his car
	};

	std::vector<std::string> Split(const std::string& S, char Sep)
	{
		std::vector<std::string> Out;
		std::stringstream In(S);
		std::string Item;
		while (std::getline(In, Item, Sep))
		{
			Out.push_back(Item);
		}
		return Out;
	}

	std::vector<std::string> CsvRow(const std::string& Line)
	{
		std::vector<std::string> Out;
		std::string Cur;
		bool bQuoted = false;
		for (const char C : Line)
		{
			if (C == '"')
			{
				bQuoted = !bQuoted;
			}
			else if (C == ',' && !bQuoted)
			{
				Out.push_back(Cur);
				Cur.clear();
			}
			else
			{
				Cur += C;
			}
		}
		Out.push_back(Cur);
		return Out;
	}

	float Length(const std::vector<FGTPDrivePoint>& P, int From, int To)
	{
		float Sum = 0.f;
		for (int K = std::max(From, 0); K < To && K + 1 < static_cast<int>(P.size()); ++K)
		{
			Sum += std::hypot(P[K + 1].X - P[K].X, P[K + 1].Y - P[K].Y);
		}
		return Sum;
	}

	struct FOptions
	{
		float PlayerX = 0.f, PlayerY = 0.f;
		int NumCars = 28;
		float Duration = 360.f;
		bool bOneAtATime = false, bHoldJunction = false, bKeepOff = true, bLights = true, bHisCar = false, bQuiet = false, bSummary = true;
	};

	/** Means over the samples taken every 10 s from t=90 s. */
	struct FResult
	{
		float Within100 = 0.f, Within150 = 0.f, Moving = 0.f, Kmh = 0.f, Over20s = 0.f;
		int FewestMoving = 0;
	};

	std::vector<FLane> ReadLanes(const char* Path)
	{
		std::vector<FLane> Lanes;
		std::ifstream File(Path);
		std::string Line;
		std::getline(File, Line);
		while (std::getline(File, Line))
		{
			if (!Line.empty() && Line.back() == '\r')
			{
				Line.pop_back();
			}
			const std::vector<std::string> F = CsvRow(Line);
			if (F.size() < 7)
			{
				continue;
			}
			FLane L;
			L.Speed = std::stof(F[3]) / 3.6f;
			for (const std::string& P : Split(F[5], ';'))
			{
				const std::vector<std::string> XY = Split(P, ' ');
				L.Points.push_back({std::stof(XY[0]) / 100.f, std::stof(XY[1]) / 100.f});
			}
			for (const std::string& N : Split(F[6], ' '))
			{
				if (!N.empty())
				{
					L.Next.push_back(std::stoi(N));
				}
			}
			if (F.size() >= 9)
			{
				L.ToJunction = std::stoi(F[8]);
			}
			Lanes.push_back(L);
		}
		return Lanes;
	}

	/**
	 * A town with no file: Size by Size crossroads Block metres apart, every street two-way, one lane each way
	 * 1.75 m right of the middle, a point every 4 m, 30 km/h. At a crossroads a car goes straight on, left or right
	 * (no way back), so the streets of the edge lead back in. Every crossroads gets lights (two crossing ways).
	 */
	std::vector<FLane> GridLanes(int Size, float Block)
	{
		struct FEdge
		{
			int FromX, FromY, ToX, ToY;
		};
		std::vector<FEdge> Edges;
		for (int Y = 0; Y < Size; ++Y)
		{
			for (int X = 0; X < Size; ++X)
			{
				if (X + 1 < Size)
				{
					Edges.push_back({X, Y, X + 1, Y});
					Edges.push_back({X + 1, Y, X, Y});
				}
				if (Y + 1 < Size)
				{
					Edges.push_back({X, Y, X, Y + 1});
					Edges.push_back({X, Y + 1, X, Y});
				}
			}
		}
		std::vector<FLane> Lanes;
		for (const FEdge& E : Edges)
		{
			FLane L;
			L.Speed = 30.f / 3.6f;
			L.ToJunction = E.ToY * Size + E.ToX;
			const float Dx = static_cast<float>(E.ToX - E.FromX), Dy = static_cast<float>(E.ToY - E.FromY);
			// 7 m of each end are the crossroads; the lane keeps to the right of its street (x to the right, y down: right of (dx, dy) is (-dy, dx)).
			for (float Along = 7.f; Along <= Block - 7.f + 0.01f; Along += 4.f)
			{
				L.Points.push_back({(static_cast<float>(E.FromX) * Block) + Dx * Along - Dy * 1.75f, (static_cast<float>(E.FromY) * Block) + Dy * Along + Dx * 1.75f});
			}
			Lanes.push_back(L);
		}
		for (size_t From = 0; From < Edges.size(); ++From)
		{
			for (size_t To = 0; To < Edges.size(); ++To)
			{
				const bool bJoins = Edges[To].FromX == Edges[From].ToX && Edges[To].FromY == Edges[From].ToY;
				const bool bBack = Edges[To].ToX == Edges[From].FromX && Edges[To].ToY == Edges[From].FromY;
				if (bJoins && !bBack)
				{
					Lanes[From].Next.push_back(static_cast<int>(To));
				}
			}
		}
		return Lanes;
	}

	int Run(const std::vector<FLane>& Lanes, const FOptions& Options, FResult& Out)
	{
		float PlayerX = Options.PlayerX, PlayerY = Options.PlayerY;
		const int NumCars = Options.NumCars;
		const float Duration = Options.Duration;
		const bool bOneAtATime = Options.bOneAtATime, bHoldJunction = Options.bHoldJunction, bKeepOff = Options.bKeepOff, bLights = Options.bLights,
			bHisCar = Options.bHisCar, bQuiet = Options.bQuiet;

		// He stands on the road: on the point of a lane nearest to where he was said to be. The car he left there shuts
		// that lane and the one of the other way beside it (within 6 m, heading against it).
		struct FShut
		{
			int Lane = -1;
			int Point = 0;
		};
		std::vector<FShut> Shut;
		{
			float Best = 1e9f;
			FShut On;
			for (int Index = 0; Index < static_cast<int>(Lanes.size()); ++Index)
			{
				const std::vector<FGTPDrivePoint>& P = Lanes[Index].Points;
				for (int K = 2; K + 2 < static_cast<int>(P.size()); ++K)
				{
					const float Away = std::hypot(P[K].X - PlayerX, P[K].Y - PlayerY);
					if (Away < Best)
					{
						Best = Away;
						On = {Index, K};
					}
				}
			}
			if (On.Lane < 0)
			{
				std::fprintf(stderr, "no lane long enough to stand on\n");
				return 2;
			}
			const std::vector<FGTPDrivePoint>& Mine = Lanes[On.Lane].Points;
			PlayerX = Mine[On.Point].X;
			PlayerY = Mine[On.Point].Y;
			Shut.push_back(On);
			const float Hx = Mine[On.Point + 1].X - Mine[On.Point].X, Hy = Mine[On.Point + 1].Y - Mine[On.Point].Y;
			float BestOther = 6.f;
			FShut Other;
			for (int Index = 0; Index < static_cast<int>(Lanes.size()); ++Index)
			{
				const std::vector<FGTPDrivePoint>& P = Lanes[Index].Points;
				for (int K = 1; K + 1 < static_cast<int>(P.size()); ++K)
				{
					const float Away = std::hypot(P[K].X - PlayerX, P[K].Y - PlayerY);
					const float Dot = (P[K + 1].X - P[K].X) * Hx + (P[K + 1].Y - P[K].Y) * Hy;
					if (Index != On.Lane && Away < BestOther && Dot < 0.f)
					{
						BestOther = Away;
						Other = {Index, K};
					}
				}
			}
			if (Other.Lane >= 0)
			{
				Shut.push_back(Other);
			}
			if (!bHisCar)
			{
				Shut.clear();
			}
		}

		// The game's figures (GTPTraffic.h): the ring cars are put in, how long a standing car is left, the path planned ahead.
		const float MinSpawn = 40.f, MaxCar = 250.f, StuckSeconds = 35.f, RouteAhead = 50.f, SpawnRoom = 15.f;
		const float SeenWithin = 60.f; // nearer than this the player sees the car: it is not taken away for standing
		const float NearWithin = 80.f; // nearer than this a car has its physics on and sees what stands on the road
		const float Dt = 0.05f;        // a far car decides twenty times a second
		FGTPDriveSettings S;
		std::mt19937 Rng(20261126u);

		FGTPTrafficLights Lights;
		if (bLights)
		{
			for (int Index = 0; Index < static_cast<int>(Lanes.size()); ++Index)
			{
				const std::vector<FGTPDrivePoint>& P = Lanes[Index].Points;
				if (P.size() >= 2)
				{
					Lights.AddApproach(Index, Lanes[Index].ToJunction, std::atan2(P.back().Y - P[P.size() - 2].Y, P.back().X - P[P.size() - 2].X));
				}
			}
		}
		Lights.Build(FGTPLightTimings());
		FGTPJunctionTable Table;
		Table.bPlatoons = !bOneAtATime;
		FGTPHeldLanes Held;
		Held.Reset(static_cast<int>(Lanes.size()));
		double Now = 0.0;
		auto ExitsOf = [&Lanes](int Lane, const auto& Visit)
		{
			for (const int Exit : Lanes[static_cast<size_t>(Lane)].Next)
			{
				Visit(Exit);
			}
		};
		auto IsShut = [&](int Lane) { return Held.IsShut(Lane, ExitsOf, Now); };
		auto PickNext = [&](int From)
		{
			const std::vector<int>& Next = Lanes[From].Next;
			if (!bKeepOff)
			{
				return Next[Rng() % Next.size()];
			}
			return Held.Pick(static_cast<int>(Next.size()), [&Next](int I) { return Next[static_cast<size_t>(I)]; }, IsShut,
				[&Rng](int N) { return static_cast<int>(Rng() % static_cast<unsigned>(N)); });
		};

		auto Plan = [&](FCar& C)
		{
			float Ahead = 0.f;
			for (size_t K = 1; K < C.Route.size(); ++K)
			{
				Ahead += Length(Lanes[C.Route[K]].Points, 0, 1 << 20);
			}
			while (Ahead < RouteAhead)
			{
				const FLane& Last = Lanes[C.Route.back()];
				if (Last.Next.empty())
				{
					break;
				}
				const int Pick = PickNext(C.Route.back());
				C.Route.push_back(Pick);
				Ahead += Length(Lanes[Pick].Points, 0, 1 << 20);
			}
			C.Path.clear();
			for (const int Index : C.Route)
			{
				C.Path.insert(C.Path.end(), Lanes[Index].Points.begin(), Lanes[Index].Points.end());
			}
			C.LaneLength = static_cast<int>(Lanes[C.Route[0]].Points.size());
		};

		std::vector<int> Ring;
		for (int Index = 0; Index < static_cast<int>(Lanes.size()); ++Index)
		{
			if (Lanes[Index].Points.size() >= 4 && !Lanes[Index].Next.empty())
			{
				Ring.push_back(Index);
			}
		}
		std::vector<FCar> Cars(static_cast<size_t>(NumCars));
		for (int Index = 0; Index < NumCars; ++Index)
		{
			Cars[static_cast<size_t>(Index)].Id = Index + 1;
		}
		auto PutOnRoad = [&](FCar& C)
		{
			for (int Try = 0; Try < 120 && !Ring.empty(); ++Try)
			{
				const int Lane = Ring[Rng() % Ring.size()];
				const std::vector<FGTPDrivePoint>& P = Lanes[Lane].Points;
				const int Point = 1 + static_cast<int>(Rng() % (P.size() - 3));
				const float Away = std::hypot(P[Point].X - PlayerX, P[Point].Y - PlayerY);
				if (Away < MinSpawn || Away > MaxCar || (bKeepOff && IsShut(Lane)))
				{
					continue;
				}
				bool bFree = true;
				for (const FCar& O : Cars)
				{
					bFree = bFree && !(O.bOnRoad && std::hypot(O.X - P[Point].X, O.Y - P[Point].Y) < SpawnRoom);
				}
				if (!bFree)
				{
					continue;
				}
				C.Junctions.Clear(Table, C.Id);
				C.bOnRoad = true;
				C.X = P[Point].X;
				C.Y = P[Point].Y;
				C.Yaw = std::atan2(P[Point + 1].Y - P[Point].Y, P[Point + 1].X - P[Point].X);
				C.Closest = Point;
				C.Along = 0.f;
				C.Stopped = 0.f;
				// A reused car comes back rolling (AGTPTrafficNetwork::SpawnCar).
				C.V = std::min(Lanes[Lane].Speed, 30.f / 3.6f);
				C.Route.assign(1, Lane);
				Plan(C);
				return true;
			}
			return false;
		};

		// The nearest car ahead on the same lane or the next one, as AGTPTrafficNetwork::GapAlongLanes counts it.
		auto GapAlongLanes = [&](const FCar& C, float MaxMetres)
		{
			const int Lane = C.Route[0];
			const int Next = C.Route.size() > 1 ? C.Route[1] : -1;
			float Best = -1.f;
			for (const FCar& O : Cars)
			{
				if (!O.bOnRoad || O.Id == C.Id)
				{
					continue;
				}
				float Along = -1.f;
				if (O.Route[0] == Lane && O.Closest > C.Closest)
				{
					Along = Length(Lanes[Lane].Points, C.Closest, O.Closest);
				}
				else if (Next >= 0 && O.Route[0] == Next)
				{
					Along = Length(Lanes[Lane].Points, C.Closest, static_cast<int>(Lanes[Lane].Points.size()) - 1) + Length(Lanes[Next].Points, 0, O.Closest);
				}
				if (Along >= 0.f && Along < MaxMetres)
				{
					const float Gap = std::max(0.f, Along - 4.2f);
					if (Best < 0.f || Gap < Best)
					{
						Best = Gap;
					}
				}
			}
			return Best;
		};

		int RecycledFar = 0, RecycledStuck = 0, Samples = 0, TurnedAway = 0;
		double SumNear = 0.0, SumMoving = 0.0, SumQueue = 0.0, SumRed = 0.0, SumJunction = 0.0, SumLong = 0.0, SumSpeed = 0.0, SumHisCar = 0.0, SumWithin100 = 0.0;
		int WorstMoving = 1 << 20;
		float NextSpawn = 0.f, NextLine = 10.f;
		for (float T = 0.f; T < Duration; T += Dt)
		{
			Now = static_cast<double>(T);
			// The network: twice a second, up to four cars from the pool.
			if (T >= NextSpawn)
			{
				NextSpawn = T + 0.5f;
				int Put = 0;
				for (FCar& C : Cars)
				{
					if (!C.bOnRoad && Put < 4)
					{
						Put += PutOnRoad(C) ? 1 : 0;
					}
				}
			}
			for (FCar& C : Cars)
			{
				if (!C.bOnRoad)
				{
					continue;
				}
				const FLane& Lane = Lanes[C.Route[0]];
				C.Stopped = C.V < 0.3f ? C.Stopped + Dt : 0.f;
				float Obstacle = -1.f;
				C.Limit = 0;
				const float Queue = GapAlongLanes(C, 40.f);
				if (Queue >= 0.f)
				{
					Obstacle = Queue;
					C.Limit = 1;
				}
				// The car he left: seen by a car with its physics on (within NearWithin of him), as the sweep ahead of it does.
				const float AwayNow = std::hypot(C.X - PlayerX, C.Y - PlayerY);
				for (const FShut& Each : Shut)
				{
					float Along = -1.f;
					if (C.Route[0] == Each.Lane && C.Closest < Each.Point)
					{
						Along = Length(Lane.Points, C.Closest, Each.Point);
					}
					else if (C.Route.size() > 1 && C.Route[1] == Each.Lane)
					{
						Along = Length(Lane.Points, C.Closest, static_cast<int>(Lane.Points.size()) - 1) + Length(Lanes[Each.Lane].Points, 0, Each.Point);
					}
					if (AwayNow < NearWithin && Along >= 0.f && Along < 25.f)
					{
						const float Gap = std::max(0.f, Along - 4.2f);
						if (Obstacle < 0.f || Gap < Obstacle)
						{
							Obstacle = Gap;
							C.Limit = 4;
						}
					}
				}
				const FGTPDrivePoint& End = Lane.Points.back();
				const FGTPDrivePoint& BeforeEnd = Lane.Points[Lane.Points.size() - 2];
				const float Ex = End.X - BeforeEnd.X, Ey = End.Y - BeforeEnd.Y, El = std::hypot(Ex, Ey);
				const float ToLaneEnd = El < 1e-4f ? std::hypot(End.X - C.X, End.Y - C.Y) : ((End.X - C.X) * Ex + (End.Y - C.Y) * Ey) / El;
				const float LightStop = Lights.StopDistance(C.Route[0], T, ToLaneEnd, C.V, S.ComfortDecel);
				if (LightStop >= 0.f && (Obstacle < 0.f || LightStop < Obstacle))
				{
					Obstacle = LightStop;
					C.Limit = 2;
				}
				if (LightStop >= 0.f)
				{
					C.Junctions.Drop(Table, C.Id, Lane.ToJunction);
				}
				const float StopLine = C.Junctions.Update(Table, C.Id, LightStop >= 0.f ? -1 : Lane.ToJunction, End, C.X, C.Y, C.V, S.ComfortDecel, C.Stopped, C.Route[0],
					!bHoldJunction && (C.Limit == 1 || C.Limit == 4));
				if (StopLine >= 0.f && (Obstacle < 0.f || StopLine < Obstacle))
				{
					Obstacle = StopLine;
					C.Limit = 3;
				}
				int DecideClosest = C.Closest;
				const FGTPDriveCommand Command = GTPDrive::Decide(C.Path, C.X, C.Y, C.Yaw, C.V, Lane.Speed, Obstacle, S, DecideClosest);
				C.V = GTPGhost::NextSpeed(C.V, Command.TargetSpeed, 2.f, 6.f, Dt);
				FGTPDrivePoint Position{C.X, C.Y};
				GTPGhost::Advance(C.Path, C.Closest, C.Along, C.V * Dt, Position, C.Yaw);
				C.X = Position.X;
				C.Y = Position.Y;
				// Nothing to do but wait: the lane is held, and a car that meant to take a held lane next turns away.
				if (bKeepOff)
				{
					if (Held.ShouldReport(C.Stopped, C.Limit == 1 || C.Limit == 4, C.Limit == 4))
					{
						Held.Report(C.Route[0], Now);
					}
					const std::vector<int>& Exits = Lanes[C.Route[0]].Next;
					if (C.Route.size() > 1 && (ToLaneEnd > 8.f || C.V < 0.3f)
						&& Held.ShouldTurnAway(C.Route[1], static_cast<int>(Exits.size()), [&Exits](int I) { return Exits[static_cast<size_t>(I)]; }, IsShut))
					{
						C.Route.resize(1);
						Plan(C);
						++TurnedAway;
					}
				}
				if (C.Closest >= C.LaneLength && C.Route.size() > 1)
				{
					C.Closest -= C.LaneLength;
					C.Route.erase(C.Route.begin());
					Plan(C);
				}
				const float Away = std::hypot(C.X - PlayerX, C.Y - PlayerY);
				const bool bDeadEnd = C.Route.size() == 1 && Lanes[C.Route[0]].Next.empty() && C.Closest >= C.LaneLength - 2 && C.V < 0.5f;
				const bool bFar = Away > MaxCar;
				if (bFar || ((bDeadEnd || C.Stopped > StuckSeconds) && Away > SeenWithin))
				{
					RecycledFar += bFar ? 1 : 0;
					RecycledStuck += bFar ? 0 : 1;
					C.Junctions.Clear(Table, C.Id);
					C.bOnRoad = false;
				}
			}
			if (T >= NextLine)
			{
				NextLine += 10.f;
				int Near = 0, Moving = 0, Stopped = 0, Queue = 0, Red = 0, Junction = 0, Long = 0, Within100 = 0, HisCar = 0;
				double Speed = 0.0;
				for (const FCar& C : Cars)
				{
					if (!C.bOnRoad)
					{
						continue;
					}
					const float Away = std::hypot(C.X - PlayerX, C.Y - PlayerY);
					Within100 += Away < 100.f ? 1 : 0;
					if (Away >= 150.f)
					{
						continue;
					}
					++Near;
					Speed += C.V * 3.6;
					if (C.V > 1.f)
					{
						++Moving;
						continue;
					}
					++Stopped;
					Queue += C.Limit == 1 ? 1 : 0;
					Red += C.Limit == 2 ? 1 : 0;
					Junction += C.Limit == 3 ? 1 : 0;
					HisCar += C.Limit == 4 ? 1 : 0;
					Long += C.Stopped > 20.f ? 1 : 0;
				}
				if (!bQuiet)
				{
					std::printf("t=%3.0fs: within 100 m=%d within 150 m=%d (moving %d, stopped %d) | stopped for: his car=%d, the queue=%d, a red light=%d, a junction in use=%d; over 20 s=%d | "
						"lanes held=%d, cars turned away=%d | recycled: stuck=%d far=%d\n",
						T, Within100, Near, Moving, Stopped, HisCar, Queue, Red, Junction, Long, Held.NumHeld(Now), TurnedAway, RecycledStuck, RecycledFar);
				}
				// Counted once the road has filled and the first lights have turned.
				if (T >= 90.f)
				{
					++Samples;
					SumNear += Near;
					SumMoving += Moving;
					SumQueue += Queue;
					SumRed += Red;
					SumJunction += Junction;
					SumHisCar += HisCar;
					SumWithin100 += Within100;
					SumLong += Long;
					SumSpeed += Near > 0 ? Speed / Near : 0.0;
					WorstMoving = std::min(WorstMoving, Moving);
				}
			}
		}
		if (Samples == 0)
		{
			std::fprintf(stderr, "too short to count: run it 100 s at least\n");
			return 2;
		}
		if (Options.bSummary)
		{
			std::printf("player at %.0f %.0f, %d cars, %.0f s, %s, %s, %s, %s, %s: within 100 m %.1f, within 150 m %.1f of which moving %.1f (fewest %d), mean %.1f km/h; "
				"stopped for his car %.1f, the queue %.1f, a red light %.1f, a junction in use %.1f, over 20 s %.1f; lanes held %d time(s), cars turned away %d; recycled stuck %d, far %d\n",
				PlayerX, PlayerY, NumCars, Duration, bOneAtATime ? "one car at a time through a junction" : "cars of one lane follow each other through a junction",
				bLights ? "lights" : "no lights",
				bHisCar ? "his car astride the street" : "nothing on the road", bKeepOff ? "cars keep off a held lane" : "cars drive into a held lane",
				bHoldJunction ? "a queue keeps its junction" : "a queue gives its junction back",
				SumWithin100 / Samples, SumNear / Samples, SumMoving / Samples, WorstMoving, SumSpeed / Samples, SumHisCar / Samples, SumQueue / Samples, SumRed / Samples,
				SumJunction / Samples, SumLong / Samples, Held.GetNumReports(), TurnedAway, RecycledStuck, RecycledFar);
		}
		Out.Within100 = static_cast<float>(SumWithin100 / Samples);
		Out.Within150 = static_cast<float>(SumNear / Samples);
		Out.Moving = static_cast<float>(SumMoving / Samples);
		Out.Kmh = static_cast<float>(SumSpeed / Samples);
		Out.Over20s = static_cast<float>(SumLong / Samples);
		Out.FewestMoving = WorstMoving;
		return 0;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc >= 2 && std::strcmp(Argv[1], "grid") == 0)
	{
		// Six by six crossroads 70 m apart, 28 cars as the game wants around the player, and the car he left astride
		// the street at five places. What the rules have to keep: around that car at least as much moves as on the
		// empty street under the rules of gate 106 less a quarter, and 15 % more than those rules left around it.
		const std::vector<FLane> Lanes = GridLanes(6, 70.f);
		const float Places[5][2] = {{175.f, 140.f}, {105.f, 210.f}, {210.f, 245.f}, {245.f, 105.f}, {140.f, 175.f}};
		struct FCase
		{
			const char* Name;
			bool bHisCar, bGate106;
			FResult Mean;
		};
		FCase Cases[4] = {{"the rules of gate 106, nothing on the road", false, true, {}}, {"the rules of gate 106, his car astride the street", true, true, {}},
			{"the rules of now, his car astride the street", true, false, {}}, {"the rules of now, nothing on the road", false, false, {}}};
		for (FCase& Case : Cases)
		{
			int Fewest = 1 << 20;
			for (const float* Place : Places)
			{
				FOptions Options;
				Options.PlayerX = Place[0];
				Options.PlayerY = Place[1];
				Options.bHisCar = Case.bHisCar;
				Options.bOneAtATime = Options.bHoldJunction = Case.bGate106;
				Options.bKeepOff = !Case.bGate106;
				Options.bQuiet = true;
				Options.bSummary = false;
				FResult One;
				if (Run(Lanes, Options, One) != 0)
				{
					return 2;
				}
				Case.Mean.Within100 += One.Within100 / 5.f;
				Case.Mean.Within150 += One.Within150 / 5.f;
				Case.Mean.Moving += One.Moving / 5.f;
				Case.Mean.Kmh += One.Kmh / 5.f;
				Case.Mean.Over20s += One.Over20s / 5.f;
				Fewest = std::min(Fewest, One.FewestMoving);
			}
			Case.Mean.FewestMoving = Fewest;
			std::printf("grid, 5 places, %s: within 100 m %.1f, within 150 m %.1f of which moving %.1f (fewest at a time %d), mean %.1f km/h, stopped over 20 s %.1f\n",
				Case.Name, Case.Mean.Within100, Case.Mean.Within150, Case.Mean.Moving, Case.Mean.FewestMoving, Case.Mean.Kmh, Case.Mean.Over20s);
		}
		const bool bHolds = Cases[2].Mean.Moving >= 0.75f * Cases[0].Mean.Moving;
		const bool bBetter = Cases[2].Mean.Moving >= 1.15f * Cases[1].Mean.Moving && Cases[2].Mean.Over20s <= 0.75f * Cases[1].Mean.Over20s;
		const bool bNoWorse = Cases[3].Mean.Moving >= 0.95f * Cases[0].Mean.Moving;
		std::printf("around his car as much moves as on the empty street of gate 106 less a quarter: %s; 15 %% more than gate 106 left around it, and a quarter fewer cars "
			"stopped over 20 s: %s; with nothing on the road no less than at gate 106: %s\n", bHolds ? "yes" : "NO", bBetter ? "yes" : "NO", bNoWorse ? "yes" : "NO");
		std::printf(bHolds && bBetter && bNoWorse ? "PASS\n" : "FAIL\n");
		return bHolds && bBetter && bNoWorse ? 0 : 1;
	}
	if (Argc < 4)
	{
		std::fprintf(stderr, "usage: standing_sim lanes.csv X Y [cars] [seconds] [his-car] [gate-106] [one-at-a-time] [hold-junction] [no-keep-off] [no-lights] [quiet]\n"
			"       standing_sim grid\n");
		return 2;
	}
	FOptions Options;
	Options.PlayerX = static_cast<float>(std::atof(Argv[2]));
	Options.PlayerY = static_cast<float>(std::atof(Argv[3]));
	Options.NumCars = Argc > 4 ? std::atoi(Argv[4]) : 28;
	Options.Duration = Argc > 5 ? static_cast<float>(std::atof(Argv[5])) : 360.f;
	for (int Index = 6; Index < Argc; ++Index)
	{
		const bool bGate106 = std::strcmp(Argv[Index], "gate-106") == 0;
		Options.bOneAtATime = Options.bOneAtATime || bGate106 || std::strcmp(Argv[Index], "one-at-a-time") == 0;
		Options.bHoldJunction = Options.bHoldJunction || bGate106 || std::strcmp(Argv[Index], "hold-junction") == 0;
		Options.bKeepOff = Options.bKeepOff && !bGate106 && std::strcmp(Argv[Index], "no-keep-off") != 0;
		Options.bLights = Options.bLights && std::strcmp(Argv[Index], "no-lights") != 0;
		Options.bHisCar = Options.bHisCar || std::strcmp(Argv[Index], "his-car") == 0;
		Options.bQuiet = Options.bQuiet || std::strcmp(Argv[Index], "quiet") == 0;
	}
	const std::vector<FLane> Lanes = ReadLanes(Argv[1]);
	if (Lanes.empty())
	{
		std::fprintf(stderr, "no lane read from %s\n", Argv[1]);
		return 2;
	}
	FResult Result;
	return Run(Lanes, Options, Result);
}
