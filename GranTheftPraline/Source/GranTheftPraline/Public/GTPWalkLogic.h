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
	// People cross a road with cars on it in groups, as at a pedestrian light: they step onto a crossing during the
	// first CrossingWalkSeconds of every CrossingCycleSeconds, and wait at the kerb for the rest (each crossing has
	// its own clock). 0 = no such phases. One by one, a steady flow of people never lets a waiting car go.
	float CrossingCycleSeconds = 0.f;
	float CrossingWalkSeconds = 8.f;
	// What a car is to a crossing (LookAtCar): one whose way goes over it and whose front is within CrossingCarNear
	// of it is "near", stopped or not; one that will be on it within CrossingCarSeconds at its speed, or that rolls
	// over it, is "dangerous". Slower than CrossingCarFastSpeed it is seen as one that can still stop.
	float CrossingCarNear = 1200.f;
	float CrossingCarSeconds = 3.f;
	float CrossingCarFastSpeed = 150.f;
	float CrossingCarSide = 150.f;  // the car's way may miss the ends of the crossing by this much and still count (cm)
	float LateralSpeed = 60.f;      // cm/s sideways drift to the chosen line
	// A car standing in the way (AvoidBox): seen CarLookAhead in front, passed CarMargin clear of its body; with no
	// room on either side the walker waits CarWaitSeconds and turns back.
	float CarLookAhead = 350.f;
	float CarMargin = 45.f;
	float CarWaitSeconds = 4.f;
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
	Gawk = 3,   // stands and looks at an event (a crash, a fight, a shot far away)
	HandsUp = 4 // stands, hands up, facing an armed threat (until released)
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
	/** Seconds spent standing in front of a car with no way round it (AvoidBox). */
	float BlockedTime = 0.f;
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
	if ((W.PauseKind == EPause::Gawk || W.PauseKind == EPause::HandsUp) && W.Pause > 0.f)
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
		// Left and right swap with the direction: keep the same spot on the pavement.
		W.Lateral = -W.Lateral;
		W.LateralTarget = -W.LateralTarget;
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
	W.LateralTarget = W.Lateral;
	return true;
}

/** Stands still facing Point for Seconds, even if it was running (held up, ordered to stop). */
inline void Freeze(FWalker& W, const FPoint& Point, float Seconds, EPause Kind)
{
	W.FleeTimeLeft = 0.f;
	W.Pause = Seconds;
	W.PauseKind = Kind;
	W.Threat = Point;
	W.LateralTarget = W.Lateral; // no sliding sideways while standing
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

/**
 * Which of the surfaces found under a person is the ground he walks on. Heights: the surfaces met from above (cm),
 * Reference: the ground he stood on (or the player's for somebody just placed), StepUp: what a walking person
 * climbs in one step. The highest surface he can step onto is his ground: the top of a tree, a parasol, a signal
 * pole, a bench or a car above it is not. When every surface is higher than a step (stairs or a ramp taken fast),
 * the least high. Returns the index, -1 without any surface.
 */
inline int PickGround(const float* Heights, int Count, float Reference, float StepUp)
{
	int Best = -1;
	for (int I = 0; I < Count; ++I)
	{
		if (Heights[I] <= Reference + StepUp && (Best < 0 || Heights[I] > Heights[Best]))
		{
			Best = I;
		}
	}
	if (Best >= 0)
	{
		return Best;
	}
	for (int I = 0; I < Count; ++I)
	{
		if (Best < 0 || Heights[I] < Heights[Best])
		{
			Best = I;
		}
	}
	return Best;
}

/** The body of a standing car seen from above: centre, heading (unit), half its length and half its width (cm). */
struct FCarBox
{
	FPoint Center;
	FPoint Forward{1.f, 0.f};
	float HalfLength = 230.f;
	float HalfWidth = 95.f;
};

inline bool InCarBox(const FCarBox& Box, const FPoint& P, float Margin)
{
	const float Dx = P.X - Box.Center.X;
	const float Dy = P.Y - Box.Center.Y;
	const float Along = Dx * Box.Forward.X + Dy * Box.Forward.Y;
	const float Side = Dy * Box.Forward.X - Dx * Box.Forward.Y;
	return std::fabs(Along) <= Box.HalfLength + Margin && std::fabs(Side) <= Box.HalfWidth + Margin;
}

/** Walks the same edge the other way from where the walker stands. */
inline void TurnBack(const FGraph& G, FWalker& W)
{
	if (!G.IsValidEdge(W.Edge))
	{
		return;
	}
	W.S = G.Edges[W.Edge].Length - W.S;
	W.bForward = !W.bForward;
	W.Lateral = -W.Lateral;
	W.LateralTarget = -W.LateralTarget;
}

enum class EAround : int
{
	Clear = 0,  // the car is not in the way
	Around = 1, // in the way further on: walk on, a line to its side is chosen
	Wait = 2,   // right in front: stand (the caller does not Step), still moving sideways to the chosen line
	Inside = 3  // already within its body (it stopped on him): walk on out
};

/**
 * A car standing in a walker's way: he walks round it on his walkway, never through it nor over it. Call it before
 * Step for each standing car near; on Wait do not Step. With no room on either side (a narrow walkway, the end of an
 * edge where everybody is on the line) he waits, and after CarWaitSeconds turns back.
 */
inline EAround AvoidBox(const FGraph& G, FWalker& W, const FCarBox& Box, const FSettings& S, float Dt)
{
	if (!G.IsValidEdge(W.Edge))
	{
		return EAround::Clear;
	}
	FPoint Pos, Dir;
	Locate(G, W, Pos, Dir);
	if (InCarBox(Box, Pos, 0.f))
	{
		return EAround::Inside;
	}
	bool bAhead = false, bClose = false;
	for (float Ahead = 35.f; Ahead <= S.CarLookAhead; Ahead += 35.f)
	{
		if (InCarBox(Box, {Pos.X + Dir.X * Ahead, Pos.Y + Dir.Y * Ahead}, S.CarMargin))
		{
			bAhead = true;
			bClose = Ahead <= 70.f;
			break;
		}
	}
	if (!bAhead)
	{
		return EAround::Clear;
	}
	// The car's body with its margin, on the walker's sideways axis (his left is (dy, -dx)), from where he is.
	float MostLeft = -1e9f, MostRight = 1e9f;
	for (int Corner = 0; Corner < 4; ++Corner)
	{
		const float A = (Corner & 1 ? 1.f : -1.f) * (Box.HalfLength + S.CarMargin);
		const float B = (Corner & 2 ? 1.f : -1.f) * (Box.HalfWidth + S.CarMargin);
		const float Cx = Box.Center.X + Box.Forward.X * A - Box.Forward.Y * B - Pos.X;
		const float Cy = Box.Center.Y + Box.Forward.Y * A + Box.Forward.X * B - Pos.Y;
		const float Left = Cx * Dir.Y - Cy * Dir.X;
		MostLeft = std::max(MostLeft, Left);
		MostRight = std::min(MostRight, Left);
	}
	const float Half = G.Edges[W.Edge].Width * 0.5f;
	const float PassLeft = W.Lateral + MostLeft + 10.f;
	const float PassRight = W.Lateral + MostRight - 10.f;
	const bool bLeft = PassLeft <= Half;
	const bool bRight = PassRight >= -Half;
	if (bLeft || bRight)
	{
		W.LateralTarget = bLeft && (!bRight || PassLeft - W.Lateral <= W.Lateral - PassRight) ? PassLeft : PassRight;
	}
	if (!bClose && (bLeft || bRight))
	{
		W.BlockedTime = 0.f;
		return EAround::Around;
	}
	// Right in front of it: stands, steps sideways if there is a way, and does not wait for ever.
	W.Lateral += std::max(-S.LateralSpeed * 2.f * Dt, std::min(S.LateralSpeed * 2.f * Dt, W.LateralTarget - W.Lateral));
	W.BlockedTime += Dt;
	if (W.BlockedTime >= S.CarWaitSeconds)
	{
		W.BlockedTime = 0.f;
		TurnBack(G, W);
	}
	return EAround::Wait;
}

struct FStepResult
{
	bool bMoving = false;
	bool bFleeing = false;
	bool bWaitingToCross = false;
	/** Walking speed after this step (cm/s). */
	float Speed = 0.f;
};

/** Is this crossing in its "walk" phase at Time (s)? Always when there are no phases. */
inline bool CrossingWalkPhase(int Edge, double Time, const FSettings& S)
{
	if (S.CrossingCycleSeconds <= 0.f)
	{
		return true;
	}
	const double Cycle = S.CrossingCycleSeconds;
	// Each crossing its own offset, so that the whole town does not cross at once.
	const double Offset = static_cast<double>((static_cast<unsigned>(Edge) * 2654435761u) % 1000u) / 1000.0 * Cycle;
	return std::fmod(Time + Offset, Cycle) < S.CrossingWalkSeconds;
}

/**
 * Must a walker stay at the kerb of this crossing now? bCarNear: a car is at the crossing or coming to it, stopped
 * or not; bCarDangerous: one comes fast and is about to pass. An empty road is crossed at once; with cars about,
 * people go together on the walk phase and wait for the rest, which is when the cars pass.
 */
inline bool CrossingUnsafe(int Edge, double Time, bool bCarNear, bool bCarDangerous, const FSettings& S)
{
	return bCarDangerous || (bCarNear && !CrossingWalkPhase(Edge, Time, S));
}

/**
 * What one car means for one crossing: sets bNear and bDangerous when it does (never clears them, so that a caller
 * can pass every car in turn). The car is at CarLocation, heads along CarForward (unit) at CarSpeed (cm/s) and is
 * CarRadius long on each side of its centre. Its way is taken straight: only a car whose line goes over the crossing
 * counts, not one that drives along it or passes in the next street.
 */
inline void LookAtCar(const FEdge& Crossing, const FPoint& CarLocation, const FPoint& CarForward, float CarSpeed, float CarRadius,
	const FSettings& S, bool& bNear, bool& bDangerous)
{
	if (Crossing.Points.size() < 2 || Crossing.Length <= 1.f)
	{
		return;
	}
	const FPoint& A = Crossing.Points.front();
	const FPoint& B = Crossing.Points.back();
	const float Dx = B.X - A.X, Dy = B.Y - A.Y;
	const float Chord = std::sqrt(Dx * Dx + Dy * Dy);
	if (Chord <= 1.f)
	{
		return;
	}
	// Where the car's line meets the crossing's: Car + Along * Forward = A + Across * (B - A).
	const float Denominator = CarForward.X * Dy - CarForward.Y * Dx;
	if (std::fabs(Denominator) < 0.3f * Chord)
	{
		return; // drives along the crossing (less than about 17 degrees from it): it does not go over it
	}
	const float Ax = A.X - CarLocation.X, Ay = A.Y - CarLocation.Y;
	const float Along = (Ax * Dy - Ay * Dx) / Denominator;
	const float Across = (Ax * CarForward.Y - Ay * CarForward.X) / Denominator;
	const float Side = S.CrossingCarSide / Chord;
	if (Across < -Side || Across > 1.f + Side)
	{
		return;
	}
	// From the car's front to the crossing; negative while the car is on it, and past it once its back has left.
	const float Gap = Along - CarRadius;
	if (Gap < -2.f * CarRadius - 100.f)
	{
		return;
	}
	const bool bRollsOver = Gap < 0.f && CarSpeed > 20.f;
	const bool bComesFast = CarSpeed > S.CrossingCarFastSpeed && Gap <= CarSpeed * S.CrossingCarSeconds;
	if (bRollsOver || bComesFast)
	{
		bNear = true;
		bDangerous = true;
	}
	else if (Gap <= S.CrossingCarNear)
	{
		bNear = true;
	}
}

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
			if (W.CrossingWait <= 0.f)
			{
				// Those who wait stand side by side along the kerb, not all on the line of the crossing.
				W.LateralTarget = Rng.Range(-1.f, 1.f) * (Current.Width * 0.5f + 60.f);
			}
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
