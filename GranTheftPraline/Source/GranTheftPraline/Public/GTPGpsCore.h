#pragma once

// GPS route on the road lines of the map data, no Unreal types (tested by
// tools/ui/tests/cpp). Roads are the polylines of minimap.csv (road_major and
// road_minor); their ends are the junctions. Roads are two-way here: following
// the traffic directions is a later step (roadmap 0.10).
//
// Route(From, To): both points are snapped to the nearest road (the start and
// the destination can be anywhere), the shortest way is found with Dijkstra on
// the junction graph, and the answer is a polyline in Unreal cm that starts on
// the road nearest to From and ends on the road nearest to To.

#include "GTPMinimapCore.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <queue>
#include <utility>
#include <vector>

namespace GTPUI
{
/** Distance from a point to a polyline, cm (large when the polyline is empty). */
inline float DistanceToPolyline(const FVec2& P, const std::vector<FVec2>& Line)
{
	float Best = std::numeric_limits<float>::max();
	for (std::size_t i = 1; i < Line.size(); ++i)
	{
		const float Dx = Line[i].X - Line[i - 1].X;
		const float Dy = Line[i].Y - Line[i - 1].Y;
		const float L2 = Dx * Dx + Dy * Dy;
		float T = L2 > 0.f ? ((P.X - Line[i - 1].X) * Dx + (P.Y - Line[i - 1].Y) * Dy) / L2 : 0.f;
		T = std::clamp(T, 0.f, 1.f);
		Best = std::min(Best, std::hypot(P.X - (Line[i - 1].X + Dx * T), P.Y - (Line[i - 1].Y + Dy * T)));
	}
	return Best;
}

class FGpsGraph
{
public:
	/** Builds the junction graph from the road lines of the map data. */
	void Build(const FMapData& Data)
	{
		Roads.clear();
		NodePositions.clear();
		Adjacency.clear();
		std::map<std::pair<long, long>, int> NodeOf;
		auto NodeFor = [&](const FVec2& P) {
			const std::pair<long, long> Key(std::lround(P.X / SnapCm), std::lround(P.Y / SnapCm));
			auto It = NodeOf.find(Key);
			if (It != NodeOf.end())
			{
				return It->second;
			}
			const int Id = static_cast<int>(NodePositions.size());
			NodeOf.emplace(Key, Id);
			NodePositions.push_back(P);
			Adjacency.emplace_back();
			return Id;
		};
		for (const FMapLine& Line : Data.Lines)
		{
			if (Line.Kind != EMapKind::RoadMajor && Line.Kind != EMapKind::RoadMinor)
			{
				continue;
			}
			FRoad Road;
			Road.Points = Line.Points;
			Road.Cumulative.assign(Road.Points.size(), 0.f);
			for (std::size_t i = 1; i < Road.Points.size(); ++i)
			{
				Road.Cumulative[i] = Road.Cumulative[i - 1]
					+ std::hypot(Road.Points[i].X - Road.Points[i - 1].X, Road.Points[i].Y - Road.Points[i - 1].Y);
			}
			Road.NodeA = NodeFor(Road.Points.front());
			Road.NodeB = NodeFor(Road.Points.back());
			const int RoadId = static_cast<int>(Roads.size());
			Roads.push_back(std::move(Road));
			const float Length = Roads.back().Cumulative.back();
			if (Roads.back().NodeA != Roads.back().NodeB)
			{
				Adjacency[Roads.back().NodeA].push_back({Roads.back().NodeB, Length, RoadId, 0.f, Length});
				Adjacency[Roads.back().NodeB].push_back({Roads.back().NodeA, Length, RoadId, Length, 0.f});
			}
		}
	}

	bool IsEmpty() const { return Roads.empty(); }
	std::size_t RoadCount() const { return Roads.size(); }
	std::size_t JunctionCount() const { return NodePositions.size(); }

	/**
	 * Shortest route between two world points (Unreal cm). False when either
	 * point is farther than MaxSnapCm from every road or no way links them.
	 */
	bool Route(const FVec2& From, const FVec2& To, std::vector<FVec2>& OutPoints, float& OutLengthCm, float MaxSnapCm = 20000.f) const
	{
		OutPoints.clear();
		OutLengthCm = 0.f;
		FSnap Start, End;
		if (!Snap(From, Start) || !Snap(To, End) || Start.Distance > MaxSnapCm || End.Distance > MaxSnapCm)
		{
			return false;
		}
		// Graph copy with two temporary nodes, S (start) and T (end), cut into their roads.
		const int NodeCount = static_cast<int>(NodePositions.size());
		const int S = NodeCount;
		const int T = NodeCount + 1;
		std::vector<std::vector<FEdge>> Adj = Adjacency;
		Adj.resize(NodeCount + 2);
		auto Attach = [&](int Temp, const FSnap& At) {
			const FRoad& Road = Roads[At.Road];
			const float Total = Road.Cumulative.back();
			Adj[Temp].push_back({Road.NodeA, At.Arc, At.Road, At.Arc, 0.f});
			Adj[Temp].push_back({Road.NodeB, Total - At.Arc, At.Road, At.Arc, Total});
			Adj[Road.NodeA].push_back({Temp, At.Arc, At.Road, 0.f, At.Arc});
			Adj[Road.NodeB].push_back({Temp, Total - At.Arc, At.Road, Total, At.Arc});
		};
		Attach(S, Start);
		Attach(T, End);
		if (Start.Road == End.Road)
		{
			const float Gap = std::fabs(Start.Arc - End.Arc);
			Adj[S].push_back({T, Gap, Start.Road, Start.Arc, End.Arc});
			Adj[T].push_back({S, Gap, Start.Road, End.Arc, Start.Arc});
		}

		std::vector<float> Dist(NodeCount + 2, std::numeric_limits<float>::max());
		std::vector<const FEdge*> Via(NodeCount + 2, nullptr);
		std::vector<int> From_(NodeCount + 2, -1);
		using Item = std::pair<float, int>;
		std::priority_queue<Item, std::vector<Item>, std::greater<Item>> Queue;
		Dist[S] = 0.f;
		Queue.push({0.f, S});
		while (!Queue.empty())
		{
			const auto [D, N] = Queue.top();
			Queue.pop();
			if (D > Dist[N])
			{
				continue;
			}
			if (N == T)
			{
				break;
			}
			for (const FEdge& E : Adj[N])
			{
				if (D + E.Cost < Dist[E.To])
				{
					Dist[E.To] = D + E.Cost;
					Via[E.To] = &E;
					From_[E.To] = N;
					Queue.push({Dist[E.To], E.To});
				}
			}
		}
		if (Dist[T] == std::numeric_limits<float>::max())
		{
			return false;
		}
		// Walk back from T, then reverse; each edge contributes the part of its road it covers.
		std::vector<const FEdge*> Edges;
		for (int N = T; N != S; N = From_[N])
		{
			Edges.push_back(Via[N]);
		}
		std::reverse(Edges.begin(), Edges.end());
		OutPoints.push_back(Start.Point);
		for (const FEdge* E : Edges)
		{
			AppendArc(Roads[E->Road], E->ArcFrom, E->ArcTo, OutPoints);
		}
		OutPoints.push_back(End.Point);
		// Drop repeated points (an edge starts where the previous one ended).
		OutPoints.erase(std::unique(OutPoints.begin(), OutPoints.end(), [](const FVec2& A, const FVec2& B) {
			return std::hypot(A.X - B.X, A.Y - B.Y) < 1.f;
		}), OutPoints.end());
		OutLengthCm = Dist[T] + Start.Distance + End.Distance;
		return true;
	}

private:
	static constexpr float SnapCm = 50.f; // road ends closer than this are one junction

	struct FRoad
	{
		std::vector<FVec2> Points;
		std::vector<float> Cumulative;
		int NodeA = 0;
		int NodeB = 0;
	};
	struct FEdge
	{
		int To;
		float Cost;
		int Road;
		float ArcFrom;
		float ArcTo;
	};
	struct FSnap
	{
		int Road = -1;
		float Arc = 0.f;
		float Distance = 0.f;
		FVec2 Point;
	};

	bool Snap(const FVec2& P, FSnap& Out) const
	{
		float Best = std::numeric_limits<float>::max();
		for (std::size_t r = 0; r < Roads.size(); ++r)
		{
			const FRoad& Road = Roads[r];
			for (std::size_t i = 1; i < Road.Points.size(); ++i)
			{
				const float Dx = Road.Points[i].X - Road.Points[i - 1].X;
				const float Dy = Road.Points[i].Y - Road.Points[i - 1].Y;
				const float L2 = Dx * Dx + Dy * Dy;
				float T = L2 > 0.f ? ((P.X - Road.Points[i - 1].X) * Dx + (P.Y - Road.Points[i - 1].Y) * Dy) / L2 : 0.f;
				T = std::clamp(T, 0.f, 1.f);
				const FVec2 Q{Road.Points[i - 1].X + Dx * T, Road.Points[i - 1].Y + Dy * T};
				const float D = std::hypot(P.X - Q.X, P.Y - Q.Y);
				if (D < Best)
				{
					Best = D;
					Out.Road = static_cast<int>(r);
					Out.Arc = Road.Cumulative[i - 1] + std::sqrt(L2) * T;
					Out.Distance = D;
					Out.Point = Q;
				}
			}
		}
		return Out.Road >= 0;
	}

	/** Appends the points of Road between two arc lengths, in the direction of travel. */
	static void AppendArc(const FRoad& Road, float ArcFrom, float ArcTo, std::vector<FVec2>& Out)
	{
		auto PointAt = [&](float Arc) {
			Arc = std::clamp(Arc, 0.f, Road.Cumulative.back());
			for (std::size_t i = 1; i < Road.Points.size(); ++i)
			{
				if (Arc <= Road.Cumulative[i])
				{
					const float Span = Road.Cumulative[i] - Road.Cumulative[i - 1];
					const float T = Span > 0.f ? (Arc - Road.Cumulative[i - 1]) / Span : 0.f;
					return FVec2{Road.Points[i - 1].X + (Road.Points[i].X - Road.Points[i - 1].X) * T,
						Road.Points[i - 1].Y + (Road.Points[i].Y - Road.Points[i - 1].Y) * T};
				}
			}
			return Road.Points.back();
		};
		const bool bForward = ArcTo >= ArcFrom;
		const float Lo = std::min(ArcFrom, ArcTo);
		const float Hi = std::max(ArcFrom, ArcTo);
		std::vector<FVec2> Part;
		Part.push_back(PointAt(Lo));
		for (std::size_t i = 0; i < Road.Points.size(); ++i)
		{
			if (Road.Cumulative[i] > Lo && Road.Cumulative[i] < Hi)
			{
				Part.push_back(Road.Points[i]);
			}
		}
		Part.push_back(PointAt(Hi));
		if (!bForward)
		{
			std::reverse(Part.begin(), Part.end());
		}
		Out.insert(Out.end(), Part.begin(), Part.end());
	}

	std::vector<FRoad> Roads;
	std::vector<FVec2> NodePositions;
	std::vector<std::vector<FEdge>> Adjacency;
};
} // namespace GTPUI
