#pragma once

// Walking decisions of the pedestrians: a graph of walkway edges and the rules a
// walker follows on it. No Unreal types, so tools/pedestrians/walk_sim checks it
// on the real walkways outside the editor (same approach as GTPDriveLogic.h).
// Units: centimetres and seconds, x east and y south as in Unreal.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace GTPWalk
{
struct FPoint
{
	float X = 0.f;
	float Y = 0.f;
};

inline float Distance(const FPoint& A, const FPoint& B)
{
	return std::sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y));
}

/** Small deterministic generator, so a run can be repeated exactly. */
struct FRng
{
	uint32_t State = 2463534242u;

	explicit FRng(uint32_t Seed = 1u) : State(Seed ? Seed : 1u) {}

	uint32_t NextU32()
	{
		State ^= State << 13;
		State ^= State >> 17;
		State ^= State << 5;
		return State;
	}

	/** [0, 1) */
	float Next() { return (NextU32() >> 8) * (1.0f / 16777216.0f); }

	float Range(float Min, float Max) { return Min + (Max - Min) * Next(); }
};

enum class EKind : int
{
	Sidewalk = 0,
	Pedestrian = 1,
	Crossing = 2,
	Join = 3
};

struct FEdge
{
	int NodeA = -1;
	int NodeB = -1;
	EKind Kind = EKind::Sidewalk;
	/** Width the walker may spread over (cm). */
	float Width = 200.f;
	/** +1: the facade is on the left of the walking direction A to B, -1 on the right, 0 none. */
	int FacadeSide = 0;
	std::vector<FPoint> Points;
	std::vector<float> Cumulative; // distance from Points[0] to each point
	float Length = 0.f;
};

struct FGraph
{
	std::vector<FEdge> Edges;
	std::vector<std::vector<int>> NodeEdges;

	/** Computes lengths and the edges of each node; call once after filling Edges. */
	void Finalize()
	{
		int Nodes = 0;
		for (FEdge& E : Edges)
		{
			E.Cumulative.assign(E.Points.size(), 0.f);
			for (size_t i = 1; i < E.Points.size(); ++i)
			{
				E.Cumulative[i] = E.Cumulative[i - 1] + Distance(E.Points[i - 1], E.Points[i]);
			}
			E.Length = E.Cumulative.empty() ? 0.f : E.Cumulative.back();
			Nodes = std::max(Nodes, std::max(E.NodeA, E.NodeB) + 1);
		}
		NodeEdges.assign(Nodes, {});
		for (size_t i = 0; i < Edges.size(); ++i)
		{
			NodeEdges[Edges[i].NodeA].push_back(static_cast<int>(i));
			if (Edges[i].NodeB != Edges[i].NodeA)
			{
				NodeEdges[Edges[i].NodeB].push_back(static_cast<int>(i));
			}
		}
	}

	bool IsValidEdge(int Edge) const { return Edge >= 0 && Edge < static_cast<int>(Edges.size()) && Edges[Edge].Length > 1.f; }
};

/** Position and heading at distance S along an edge from its A end. */
inline void SampleEdge(const FEdge& E, float S, FPoint& OutPos, FPoint& OutDir)
{
	S = std::max(0.f, std::min(S, E.Length));
	size_t i = 1;
	while (i + 1 < E.Points.size() && E.Cumulative[i] < S)
	{
		++i;
	}
	const FPoint& A = E.Points[i - 1];
	const FPoint& B = E.Points[i];
	const float Seg = std::max(1e-3f, E.Cumulative[i] - E.Cumulative[i - 1]);
	const float T = std::max(0.f, std::min(1.f, (S - E.Cumulative[i - 1]) / Seg));
	OutPos = {A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T};
	OutDir = {(B.X - A.X) / Seg, (B.Y - A.Y) / Seg};
}

struct FSettings
{
	float WalkSpeed = 140.f;        // cm/s, a relaxed walk
	float WalkSpeedSpread = 0.2f;   // +/- fraction between people
	float FleeSpeedScale = 2.3f;    // a run
	float FleeSeconds = 4.f;
	float PauseChancePerSecond = 0.012f; // stops to look at a window, on walkways only
	float PauseMin = 2.f;
	float PauseMax = 6.f;
	float PhoneShare = 0.3f;        // part of the pauses spent on the phone, the rest looking at a window
	float CrossingWaitMax = 7.f;    // after that they cross anyway: cars give way to people
	float LateralSpeed = 60.f;      // cm/s sideways drift to the chosen line
	float CrossingWeight = 0.4f;    // relative chance to take a crossing at a junction
	float GroupChance = 0.25f;      // chance that a new person is half of a pair walking together
	float GroupGap = 70.f;          // the follower walks this far behind its partner (cm)
	float GroupSide = 80.f;         // ...and this far beside (cm)
	float SeparationDistance = 90.f; // people closer than this step sideways round each other (cm)
	float SeparationNudge = 70.f;   // how far sideways (cm)
	// Panic spreads from person to person (herd), a few steps only.
	float PanicRadius = 700.f;
	float PanicChance = 0.65f;
	int PanicMaxGeneration = 2;
	// Far from a shot or a crash people stop and look.
	float GawkChance = 0.6f;
	float GawkMin = 4.f;
	float GawkMax = 9.f;
};

enum class EPause : int
{
	None = 0,
	Window = 1, // faces the facade
	Phone = 2,  // stands where it is
	Gawk = 3    // stands and looks at an event (a crash, a fight, a shot far away)
};

struct FWalker
{
	int Edge = -1;
	bool bForward = true; // true: from NodeA to NodeB
	float S = 0.f;        // distance walked on the edge in the walking direction
	float SpeedScale = 1.f;
	float Lateral = 0.f;  // sideways offset from the walkway line (cm, + = left of the direction)
	float LateralTarget = 0.f;
	float Pause = 0.f;
	EPause PauseKind = EPause::None;
	float CrossingWait = 0.f;
	float FleeTimeLeft = 0.f;
	FPoint Threat;
	/** 0 = frightened by the event itself, n = caught the panic from somebody of generation n - 1. */
	int Generation = 0;
};

inline int NodeAhead(const FGraph& G, const FWalker& W)
{
	const FEdge& E = G.Edges[W.Edge];
	return W.bForward ? E.NodeB : E.NodeA;
}

inline void PickLateral(const FGraph& G, FWalker& W, FRng& Rng)
{
	const float Half = G.Edges[W.Edge].Width * 0.5f;
	W.LateralTarget = Rng.Range(-Half, Half);
}

/** Edge to take at Node after arriving by FromEdge (turning back only at a dead end). */
inline int ChooseNext(const FGraph& G, int Node, int FromEdge, FRng& Rng, const FSettings& S,
	bool bFleeing = false, const FPoint& Threat = FPoint())
{
	const std::vector<int>& Options = G.NodeEdges[Node];
	float Total = 0.f;
	std::vector<float> Weights(Options.size(), 0.f);
	int Best = -1;
	float BestDistance = -1.f;
	for (size_t i = 0; i < Options.size(); ++i)
	{
		const int Id = Options[i];
		if (Id == FromEdge || !G.IsValidEdge(Id))
		{
			continue;
		}
		const FEdge& E = G.Edges[Id];
		const FPoint FarPoint = E.NodeA == Node ? E.Points.back() : E.Points.front();
		if (bFleeing)
		{
			const float D = Distance(FarPoint, Threat);
			if (D > BestDistance)
			{
				BestDistance = D;
				Best = Id;
			}
			continue;
		}
		Weights[i] = E.Kind == EKind::Crossing ? S.CrossingWeight : 1.f;
		Total += Weights[i];
	}
	if (bFleeing && Best >= 0)
	{
		return Best;
	}
	if (Total <= 0.f)
	{
		return G.IsValidEdge(FromEdge) ? FromEdge : -1;
	}
	float Pick = Rng.Next() * Total;
	for (size_t i = 0; i < Options.size(); ++i)
	{
		Pick -= Weights[i];
		if (Weights[i] > 0.f && Pick <= 0.f)
		{
			return Options[i];
		}
	}
	for (size_t i = Options.size(); i-- > 0;)
	{
		if (Weights[i] > 0.f)
		{
			return Options[i];
		}
	}
	return -1;
}

/** Puts a walker at a random place on a walkway (not a crossing). False if the graph has none. */
inline bool Place(const FGraph& G, FWalker& W, FRng& Rng, const FSettings& S)
{
	std::vector<int> Candidates;
	for (size_t i = 0; i < G.Edges.size(); ++i)
	{
		const EKind K = G.Edges[i].Kind;
		if (G.IsValidEdge(static_cast<int>(i)) && (K == EKind::Sidewalk || K == EKind::Pedestrian))
		{
			Candidates.push_back(static_cast<int>(i));
		}
	}
	if (Candidates.empty())
	{
		return false;
	}
	W = FWalker();
	W.Edge = Candidates[std::min<size_t>(Candidates.size() - 1, static_cast<size_t>(Rng.Next() * Candidates.size()))];
	W.bForward = Rng.Next() < 0.5f;
	W.S = Rng.Range(0.f, G.Edges[W.Edge].Length);
	W.SpeedScale = 1.f + Rng.Range(-S.WalkSpeedSpread, S.WalkSpeedSpread);
	PickLateral(G, W, Rng);
	W.Lateral = W.LateralTarget;
	return true;
}

/** Position of a walker in the world, sideways drift included; Dir is the walking direction. */
inline void Locate(const FGraph& G, const FWalker& W, FPoint& OutPos, FPoint& OutDir)
{
	const FEdge& E = G.Edges[W.Edge];
	FPoint P, D;
	SampleEdge(E, W.bForward ? W.S : E.Length - W.S, P, D);
	if (!W.bForward)
	{
		D = {-D.X, -D.Y};
	}
	// The sideways drift fades out over the last metres of an edge, so walkers meet at the nodes
	// whatever the angle between two edges (no jump at a corner).
	const float Ramp = 300.f;
	const float Fade = std::max(0.f, std::min(1.f, std::min(W.S, E.Length - W.S) / Ramp));
	const float Side = W.Lateral * Fade;
	// Unreal: x forward (east), y right (south). The left of (dx, dy) is (dy, -dx).
	OutPos = {P.X + D.Y * Side, P.Y - D.X * Side};
	OutDir = D;
}

/** Direction the walker faces: the facade while looking at a window, else the walking direction. */
inline FPoint Facing(const FGraph& G, const FWalker& W)
{
	FPoint Pos, Dir;
	Locate(G, W, Pos, Dir);
	if (W.PauseKind == EPause::Window && W.Pause > 0.f)
	{
		const int Side = G.Edges[W.Edge].FacadeSide * (W.bForward ? 1 : -1);
		return {Dir.Y * Side, -Dir.X * Side}; // left of (dx, dy) is (dy, -dx)
	}
	if (W.PauseKind == EPause::Gawk && W.Pause > 0.f)
	{
		const float Dx = W.Threat.X - Pos.X;
		const float Dy = W.Threat.Y - Pos.Y;
		const float Len = std::sqrt(Dx * Dx + Dy * Dy);
		if (Len > 1.f)
		{
			return {Dx / Len, Dy / Len};
		}
	}
	return Dir;
}

/** Makes the walker run away from Threat along its edge (turns round if it walks towards it). */
inline void Flee(const FGraph& G, FWalker& W, const FPoint& Threat, const FSettings& S)
{
	FPoint Pos, Dir;
	Locate(G, W, Pos, Dir);
	const float Away = (Pos.X - Threat.X) * Dir.X + (Pos.Y - Threat.Y) * Dir.Y;
	if (Away < 0.f)
	{
		W.bForward = !W.bForward;
		W.S = G.Edges[W.Edge].Length - W.S;
	}
	W.FleeTimeLeft = S.FleeSeconds;
	W.Threat = Threat;
	W.Pause = 0.f;
	W.Generation = 0;
}

/**
 * Somebody near flees: this walker may catch the panic (fresh sources of generation Generation, at
 * SourcePos). Not when already running, when the chain is long enough, or by chance. Returns true when caught.
 */
inline bool CatchPanic(const FGraph& G, FWalker& W, int SourceGeneration, const FPoint& SourcePos, const FSettings& S, FRng& Rng)
{
	if (W.FleeTimeLeft > 0.f || SourceGeneration + 1 > S.PanicMaxGeneration || Rng.Next() >= S.PanicChance)
	{
		return false;
	}
	Flee(G, W, SourcePos, S);
	W.Generation = SourceGeneration + 1;
	return true;
}

/** Stops and looks at Point (a crash, a fight, a distant shot) for a few seconds; not while running. */
inline bool Gawk(FWalker& W, const FPoint& Point, FRng& Rng, const FSettings& S)
{
	if (W.FleeTimeLeft > 0.f || Rng.Next() >= S.GawkChance)
	{
		return false;
	}
	W.Pause = Rng.Range(S.GawkMin, S.GawkMax);
	W.PauseKind = EPause::Gawk;
	W.Threat = Point;
	return true;
}

/** Puts Follower beside and a little behind Leader on the same edge (a pair walking together). */
inline void Follow(const FGraph& G, FWalker& Follower, const FWalker& Leader, const FSettings& S)
{
	Follower.Edge = Leader.Edge;
	Follower.bForward = Leader.bForward;
	Follower.S = std::max(0.f, Leader.S - S.GroupGap);
	Follower.SpeedScale = Leader.SpeedScale;
	const float Half = G.Edges[Leader.Edge].Width * 0.5f;
	Follower.Lateral = Follower.LateralTarget = std::max(-Half, std::min(Half, Leader.Lateral + (Leader.Lateral < 0.f ? S.GroupSide : -S.GroupSide)));
	Follower.Pause = Leader.Pause;
	Follower.PauseKind = Leader.PauseKind;
	Follower.FleeTimeLeft = Leader.FleeTimeLeft;
	Follower.Threat = Leader.Threat;
}

/**
 * Two people too close to each other: A steps sideways, away from B, on its own line
 * (its left is (dy, -dx) for the walking direction DirA). Call it for both ways round.
 */
inline void Separate(const FGraph& G, FWalker& A, const FPoint& PosA, const FPoint& DirA, const FPoint& PosB, const FSettings& S, float Distance = -1.f)
{
	const float Limit = Distance > 0.f ? Distance : S.SeparationDistance;
	const float Dx = PosA.X - PosB.X;
	const float Dy = PosA.Y - PosB.Y;
	const float Dist = std::sqrt(Dx * Dx + Dy * Dy);
	if (Dist >= Limit || !G.IsValidEdge(A.Edge))
	{
		return;
	}
	// Exactly on top of each other: pick a side from the position, so the two do not pick the same.
	const float Side = Dist < 1.f ? ((static_cast<int>(PosA.X) & 1) ? 1.f : -1.f) : ((Dx * DirA.Y - Dy * DirA.X) >= 0.f ? 1.f : -1.f);
	const float Half = G.Edges[A.Edge].Width * 0.5f;
	A.LateralTarget = std::max(-Half, std::min(Half, A.Lateral + Side * S.SeparationNudge));
}

struct FStepResult
{
	bool bMoving = false;
	bool bFleeing = false;
	bool bWaitingToCross = false;
	/** Walking speed after this step (cm/s). */
	float Speed = 0.f;
};

/**
 * One decision. bCrossingUnsafe: a car is coming at the crossing the walker is
 * about to use (the caller looks, the graph does not know about cars).
 */
inline FStepResult Step(const FGraph& G, FWalker& W, float Dt, FRng& Rng, const FSettings& S, bool bCrossingUnsafe)
{
	FStepResult Result;
	if (!G.IsValidEdge(W.Edge))
	{
		return Result;
	}
	W.Lateral += std::max(-S.LateralSpeed * Dt, std::min(S.LateralSpeed * Dt, W.LateralTarget - W.Lateral));
	if (W.FleeTimeLeft > 0.f)
	{
		W.FleeTimeLeft -= Dt;
		Result.bFleeing = true;
	}
	else if (W.Pause > 0.f)
	{
		W.Pause -= Dt;
		if (W.Pause <= 0.f)
		{
			W.PauseKind = EPause::None;
		}
		return Result;
	}

	const FEdge& Current = G.Edges[W.Edge];
	const bool bFleeing = W.FleeTimeLeft > 0.f;
	if (Current.Kind == EKind::Crossing && W.S < 30.f && !bFleeing)
	{
		if (bCrossingUnsafe && W.CrossingWait < S.CrossingWaitMax)
		{
			W.CrossingWait += Dt;
			Result.bWaitingToCross = true;
			return Result;
		}
	}
	else if (!bFleeing && Current.Kind != EKind::Crossing && Rng.Next() < S.PauseChancePerSecond * Dt)
	{
		W.Pause = Rng.Range(S.PauseMin, S.PauseMax);
		const bool bWindow = Current.FacadeSide != 0 && Rng.Next() >= S.PhoneShare;
		W.PauseKind = bWindow ? EPause::Window : EPause::Phone;
		if (bWindow)
		{
			// Walk over to the facade: left is positive, and on a reversed walk the sides swap.
			const float Side = static_cast<float>(Current.FacadeSide) * (W.bForward ? 1.f : -1.f);
			W.LateralTarget = Side * Current.Width * 0.45f;
		}
		return Result;
	}

	const float Speed = S.WalkSpeed * W.SpeedScale * (bFleeing ? S.FleeSpeedScale : 1.f);
	W.S += Speed * Dt;
	Result.bMoving = true;
	Result.Speed = Speed;
	while (W.Edge >= 0 && W.S >= G.Edges[W.Edge].Length)
	{
		const float Over = W.S - G.Edges[W.Edge].Length;
		const int Node = NodeAhead(G, W);
		const int Next = ChooseNext(G, Node, W.Edge, Rng, S, bFleeing, W.Threat);
		if (Next < 0)
		{
			W.S = G.Edges[W.Edge].Length;
			break;
		}
		const bool bTurningBack = Next == W.Edge;
		W.bForward = bTurningBack ? !W.bForward : (G.Edges[Next].NodeA == Node);
		W.Edge = Next;
		W.S = std::min(Over, G.Edges[Next].Length);
		W.CrossingWait = 0.f;
		PickLateral(G, W, Rng);
		if (G.Edges[Next].Kind == EKind::Crossing)
		{
			W.LateralTarget = 0.f;
		}
	}
	return Result;
}
} // namespace GTPWalk
