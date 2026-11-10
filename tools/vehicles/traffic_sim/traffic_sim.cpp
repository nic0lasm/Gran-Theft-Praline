// Simulates AI drivers (GTPDriveLogic.h) on the lanes made by tools/city/traffic.py,
// with a kinematic bicycle model close to the city car tuning, and reports how
// well they keep to their lane, how fast they go and how often they touch.
// Build: g++ -O2 -std=c++17 -I ../../../GranTheftPraline/Source/GranTheftPraline/Public traffic_sim.cpp -o traffic_sim
// Run:   ./traffic_sim lanes.csv [cars] [seconds] [lights 0|1]   (exit code 1 when a criterion fails)
#include "GTPDriveLogic.h"
#include "GTPTrafficLights.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

struct Lane
{
	std::vector<FGTPDrivePoint> Points; // metres
	std::vector<int> Next;
	float Speed = 8.3f;
	int ToJunction = -1;
};

struct Car
{
	float X = 0, Y = 0, Yaw = 0, V = 0, SteerAngle = 0;
	int LaneIndex = 0, NextLane = -1, Closest = 0;
	std::vector<int> Route; // current lane first, then the lanes chosen ahead
	FGTPJunctionClaims Junctions;
	int Id = 0;
	std::vector<FGTPDrivePoint> Path;
	int LaneLength = 0;
};

/** Length along Points from index A to index B (A <= B). */
static float PathLength(const std::vector<FGTPDrivePoint>& Points, int A, int B)
{
	float L = 0.f;
	for (int K = std::max(A, 0); K < B && K + 1 < static_cast<int>(Points.size()); ++K)
		L += std::hypot(Points[K + 1].X - Points[K].X, Points[K + 1].Y - Points[K].Y);
	return L;
}

static std::vector<std::string> Split(const std::string& S, char Sep)
{
	std::vector<std::string> Out;
	std::stringstream In(S);
	std::string Item;
	while (std::getline(In, Item, Sep)) Out.push_back(Item);
	return Out;
}

// Minimal CSV: fields may be quoted (names with commas).
static std::vector<std::string> CsvRow(const std::string& Line)
{
	std::vector<std::string> Out;
	std::string Cur;
	bool Quoted = false;
	for (char C : Line)
	{
		if (C == '"') Quoted = !Quoted;
		else if (C == ',' && !Quoted) { Out.push_back(Cur); Cur.clear(); }
		else Cur += C;
	}
	Out.push_back(Cur);
	return Out;
}

static float DistToSegment(float Px, float Py, const FGTPDrivePoint& A, const FGTPDrivePoint& B)
{
	const float Dx = B.X - A.X, Dy = B.Y - A.Y;
	const float L2 = Dx * Dx + Dy * Dy;
	float T = L2 > 0 ? ((Px - A.X) * Dx + (Py - A.Y) * Dy) / L2 : 0;
	T = std::max(0.f, std::min(1.f, T));
	return std::hypot(Px - (A.X + T * Dx), Py - (A.Y + T * Dy));
}

int main(int Argc, char** Argv)
{
	if (Argc < 2) { std::fprintf(stderr, "usage: traffic_sim lanes.csv [cars] [seconds]\n"); return 2; }
	const int NumCars = Argc > 2 ? std::atoi(Argv[2]) : 12;
	const float Duration = Argc > 3 ? static_cast<float>(std::atof(Argv[3])) : 300.f;
	const bool bLights = Argc > 4 ? std::atoi(Argv[4]) != 0 : true;
	// Same as AGTPTrafficNetwork::StuckSeconds: longer than a red light plus its queue.
	const float StuckSeconds = 35.f;

	std::vector<Lane> Lanes;
	std::ifstream File(Argv[1]);
	std::string Line;
	std::getline(File, Line);
	while (std::getline(File, Line))
	{
		if (!Line.empty() && Line.back() == '\r') Line.pop_back();
		const auto F = CsvRow(Line);
		if (F.size() < 7) continue;
		Lane L;
		L.Speed = std::stof(F[3]) / 3.6f;
		for (const auto& P : Split(F[5], ';'))
		{
			const auto XY = Split(P, ' ');
			// Unreal cm, Y south -> metres in the same (left-handed) frame: keep Unreal axes.
			L.Points.push_back({std::stof(XY[0]) / 100.f, std::stof(XY[1]) / 100.f});
		}
		for (const auto& N : Split(F[6], ' ')) if (!N.empty()) L.Next.push_back(std::stoi(N));
		if (F.size() >= 9) L.ToJunction = std::stoi(F[8]);
		Lanes.push_back(L);
	}
	std::printf("%zu lanes\n", Lanes.size());
	std::mt19937 Rng(7);
	FGTPDriveSettings S;
	const float Dt = 1.f / 30.f, MaxAccel = 2.6f, MaxDecel = 8.f, SteerRate = 1.2f;

	// Longer lanes first, so spawns are spread out.
	std::vector<int> Spawnable;
	for (int I = 0; I < static_cast<int>(Lanes.size()); ++I) if (Lanes[I].Points.size() >= 6 && !Lanes[I].Next.empty()) Spawnable.push_back(I);

	// Lanes are chosen ahead until the path covers RouteAhead metres past the current lane,
	// so a bend made of several short lanes is seen in time (the game does the same).
	const float RouteAhead = 50.f;
	auto LaneLen = [&](int Index) {
		float Len = 0;
		const auto& P = Lanes[Index].Points;
		for (size_t K = 1; K < P.size(); ++K) Len += std::hypot(P[K].X - P[K - 1].X, P[K].Y - P[K - 1].Y);
		return Len;
	};
	auto Plan = [&](Car& C) {
		if (C.Route.empty() || C.Route.front() != C.LaneIndex) C.Route.assign(1, C.LaneIndex);
		float Ahead = 0;
		for (size_t K = 1; K < C.Route.size(); ++K) Ahead += LaneLen(C.Route[K]);
		while (Ahead < RouteAhead)
		{
			const Lane& Last = Lanes[C.Route.back()];
			if (Last.Next.empty()) break;
			const int Pick = Last.Next[Rng() % Last.Next.size()];
			C.Route.push_back(Pick);
			Ahead += LaneLen(Pick);
		}
		C.NextLane = C.Route.size() > 1 ? C.Route[1] : -1;
		C.Path.clear();
		for (int Index : C.Route) C.Path.insert(C.Path.end(), Lanes[Index].Points.begin(), Lanes[Index].Points.end());
		C.LaneLength = static_cast<int>(Lanes[C.LaneIndex].Points.size());
	};
	int Respawns = 0;
	FGTPJunctionTable JunctionTable;
	FGTPTrafficLights Lights;
	if (bLights)
	{
		for (int Index = 0; Index < static_cast<int>(Lanes.size()); ++Index)
		{
			const auto& P = Lanes[Index].Points;
			if (P.size() >= 2)
				Lights.AddApproach(Index, Lanes[Index].ToJunction, std::atan2(P.back().Y - P[P.size() - 2].Y, P.back().X - P[P.size() - 2].X));
		}
	}
	Lights.Build(FGTPLightTimings());
	int RedRuns = 0;
	auto Spawn = [&](Car& C, int LaneIndex, int PointIndex) {
		C.LaneIndex = LaneIndex;
		const auto& P = Lanes[LaneIndex].Points;
		C.X = P[PointIndex].X; C.Y = P[PointIndex].Y;
		C.Yaw = std::atan2(P[PointIndex + 1].Y - P[PointIndex].Y, P[PointIndex + 1].X - P[PointIndex].X);
		C.V = 0; C.SteerAngle = 0; C.Closest = PointIndex;
		C.Route.clear();
		C.Junctions.Clear(JunctionTable, C.Id);
		Plan(C);
	};
	std::vector<Car> Cars(NumCars);
	for (int I = 0; I < NumCars; ++I) Cars[I].Id = I;
	for (int I = 0; I < NumCars; ++I) Spawn(Cars[I], Spawnable[(I * 7919) % Spawnable.size()], 1);

	std::vector<float> CrossTrack;
	double SpeedSum = 0; long SpeedCount = 0; int Touches = 0; int StuckEvents = 0;
	std::vector<float> Stopped(NumCars, 0.f);
	for (float T = 0; T < Duration; T += Dt)
	{
		for (int I = 0; I < NumCars; ++I)
		{
			Car& C = Cars[I];
			// Nearest car ahead: in front, within 2 m of our line of travel.
			float Obstacle = -1.f;
			const float Hx = std::cos(C.Yaw), Hy = std::sin(C.Yaw);
			for (int J = 0; J < NumCars; ++J)
			{
				if (J == I) continue;
				const float Dx = Cars[J].X - C.X, Dy = Cars[J].Y - C.Y;
				const float Along = Dx * Hx + Dy * Hy, Side = -Dx * Hy + Dy * Hx;
				if (Along > 0 && Along < 40 && std::fabs(Side) < 2.f)
				{
					const float Gap = Along - 4.2f;
					if (Obstacle < 0 || Gap < Obstacle) Obstacle = std::max(0.f, Gap);
				}
			}
			// Cars on our own lane or the next one, measured along the lanes: on a bend
			// the straight look-ahead above misses a queue waiting at a light.
			for (int J = 0; J < NumCars; ++J)
			{
				if (J == I) continue;
				const Car& O = Cars[J];
				float Along = -1.f;
				if (O.LaneIndex == C.LaneIndex && O.Closest > C.Closest)
					Along = PathLength(Lanes[C.LaneIndex].Points, C.Closest, O.Closest);
				else if (C.Route.size() > 1 && O.LaneIndex == C.Route[1])
					Along = PathLength(Lanes[C.LaneIndex].Points, C.Closest, static_cast<int>(Lanes[C.LaneIndex].Points.size()) - 1)
						+ PathLength(Lanes[O.LaneIndex].Points, 0, O.Closest);
				if (Along >= 0.f && Along < 40.f)
				{
					const float Gap = std::max(0.f, Along - 4.2f);
					if (Obstacle < 0 || Gap < Obstacle) Obstacle = Gap;
				}
			}
			const Lane& Current = Lanes[C.LaneIndex];
			// A red light first: a car waiting at it does not claim the junction.
			const float ToEnd = std::hypot(Current.Points.back().X - C.X, Current.Points.back().Y - C.Y);
			const float LightStop = Lights.StopDistance(C.LaneIndex, T, ToEnd, C.V, S.ComfortDecel);
			if (LightStop >= 0.f && (Obstacle < 0.f || LightStop < Obstacle)) Obstacle = LightStop;
			const float StopLine = C.Junctions.Update(JunctionTable, I, LightStop >= 0.f ? -1 : Current.ToJunction, Current.Points.back(), C.X, C.Y, C.V, S.ComfortDecel, Stopped[I]);
			if (StopLine >= 0.f && (Obstacle < 0.f || StopLine < Obstacle)) Obstacle = StopLine;
			const FGTPDriveCommand Cmd = GTPDrive::Decide(C.Path, C.X, C.Y, C.Yaw, C.V, Lanes[C.LaneIndex].Speed, Obstacle, S, C.Closest);
			const float Accel = Cmd.Throttle * MaxAccel - Cmd.Brake * MaxDecel - 0.2f;
			C.V = std::max(0.f, C.V + Accel * Dt);
			const float WantSteer = Cmd.Steering * S.MaxSteerAngle;
			C.SteerAngle += GTPDrive::Clamp(WantSteer - C.SteerAngle, -SteerRate * Dt, SteerRate * Dt);
			C.Yaw = GTPDrive::WrapAngle(C.Yaw + C.V / S.WheelBase * std::tan(C.SteerAngle) * Dt);
			C.X += C.V * std::cos(C.Yaw) * Dt;
			C.Y += C.V * std::sin(C.Yaw) * Dt;

			// Distance to the path near the closest point.
			float Best = 1e9f;
			for (int K = std::max(0, C.Closest - 3); K + 1 < static_cast<int>(C.Path.size()) && K < C.Closest + 3; ++K)
				Best = std::min(Best, DistToSegment(C.X, C.Y, C.Path[K], C.Path[K + 1]));
			if (Best < 1e8f) CrossTrack.push_back(Best);
			SpeedSum += C.V; ++SpeedCount;

			Stopped[I] = C.V < 0.3f ? Stopped[I] + Dt : 0.f;
			bool bStuck = false;
			if (Stopped[I] > StuckSeconds) { ++StuckEvents; Stopped[I] = 0; bStuck = true; }

			// Into the next lane: shift the path.
			if (C.Closest >= C.LaneLength && C.NextLane >= 0)
			{
				// Crossed the line on red (amber is allowed when too close to stop).
				if (Lights.LightFor(C.LaneIndex, T) == EGTPLight::Red) ++RedRuns;
				C.Closest -= C.LaneLength;
				C.Route.erase(C.Route.begin());
				C.LaneIndex = C.NextLane;
				Plan(C);
			}
			// Dead end: the game respawns such a car out of sight.
			const auto& End = C.Path.back();
			if (bStuck || (C.NextLane < 0 && std::hypot(End.X - C.X, End.Y - C.Y) < 2.f && C.V < 0.5f))
			{
				int Pick = 0;
				for (int Try = 0; Try < 50; ++Try)
				{
					Pick = Spawnable[Rng() % Spawnable.size()];
					const auto& P = Lanes[Pick].Points[1];
					bool Free = true;
					for (const Car& O : Cars) if (std::hypot(O.X - P.X, O.Y - P.Y) < 12.f) Free = false;
					if (Free) break;
				}
				Spawn(C, Pick, 1);
				++Respawns;
			}
		}
		for (int I = 0; I < NumCars; ++I)
			for (int J = I + 1; J < NumCars; ++J)
				if (std::hypot(Cars[I].X - Cars[J].X, Cars[I].Y - Cars[J].Y) < 2.0f) ++Touches;
	}
	std::sort(CrossTrack.begin(), CrossTrack.end());
	const float P50 = CrossTrack[CrossTrack.size() / 2], P95 = CrossTrack[CrossTrack.size() * 95 / 100], P99 = CrossTrack[CrossTrack.size() * 99 / 100];
	const float MeanKmh = static_cast<float>(SpeedSum / SpeedCount * 3.6);
	std::printf("lights: %d signalled junctions, red lights run %d\n", Lights.NumSignalledJunctions(), RedRuns);
	std::printf("cars %d, %.0f s: lane offset p50 %.2f m, p95 %.2f m, p99 %.2f m; mean speed %.1f km/h; touching frames %d; stuck %.0f s %d; respawns %d\n",
		NumCars, Duration, P50, P95, P99, MeanKmh, Touches, StuckSeconds, StuckEvents, Respawns);
	// Lanes are about 3 m wide: 95 % of the time within half a lane. The worst 1 % is at
	// junction corners, measured against the straight jump between two lane ends (no
	// drawn curve there yet): under 4 m. No contact at all, few cars stuck.
	bool Ok = P95 < 1.5f && P99 < 4.0f && MeanKmh > (bLights ? 10.f : 12.f) && StuckEvents <= NumCars / 4 && Touches == 0;
	std::printf(Ok ? "PASS\n" : "FAIL\n");
	return Ok ? 0 : 1;
}
