#pragma once

// Taking cover against walls and cars, written without engine types (checked by
// tools/combat/combat_logic_test.cpp). ADR-0011.
//
// Unreal measures, these rules decide:
// - FProbe: three horizontal traces toward the wall at knee, chest and head
//   height, plus a side trace on each side for the edges. Classify() says
//   whether there is cover and whether it is low (crouch behind, stand up to
//   aim) or high (lean out at an edge to aim).
// - Cover to cover: among candidate points (from the same traces, or from the
//   placed cover data of the city), the best one in the direction the stick
//   points, within reach, not behind the player.
// - Protection: is a body in cover safe from a shooter at a given direction?
//   For NPC decisions (police flanking, "is the player exposed?"); bullets
//   themselves are physical traces.
// Units: centimetres, degrees. Directions are horizontal (x, y), not normalised.

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <vector>

namespace GTPCover
{
enum class EType : int
{
	None = 0,
	/** Crouch behind it; aiming stands up over it. Car bonnets, low walls, planters. */
	Low = 1,
	/** Stand behind it; aiming leans out at an edge. Walls, pillars, vans. */
	High = 2
};

struct FSettings
{
	/** A wall farther than this from the capsule is not taken (cm). */
	float MaxEnterDistanceCm = 120.f;
	/** Heights of the three probes (cm above the feet). */
	float KneeHeightCm = 50.f;
	float ChestHeightCm = 110.f;
	float HeadHeightCm = 165.f;
	/** The wall must face the player within this angle (degrees). */
	float MaxFacingAngleDeg = 50.f;
	/** Edge: the side trace finds no wall within this distance (cm). */
	float EdgeClearCm = 60.f;

	/** Cover to cover: farthest jump (cm), cone around the stick direction (degrees). */
	float MaxMoveCm = 1200.f;
	float MoveConeDeg = 40.f;
	/** Points nearer than this are the cover the player is already in (cm). */
	float MinMoveCm = 150.f;

	/** Protection: half-angle of the arc a cover protects around its normal (degrees). */
	float ProtectedHalfAngleDeg = 75.f;
};

/** Results of the traces of one cover query; a distance < 0 means nothing hit. */
struct FProbe
{
	float KneeDistCm = -1.f;
	float ChestDistCm = -1.f;
	float HeadDistCm = -1.f;
	/** Angle between the wall normal (pointing to the player) and the direction from the wall to the player (degrees). */
	float FacingAngleDeg = 0.f;
	/** Side traces along the wall, left and right (cm, < 0 = open). */
	float LeftSideDistCm = -1.f;
	float RightSideDistCm = -1.f;
};

inline bool Hits(float Dist, float Max) { return Dist >= 0.f && Dist <= Max; }

inline EType Classify(const FProbe& P, const FSettings& S = FSettings())
{
	if (std::fabs(P.FacingAngleDeg) > S.MaxFacingAngleDeg)
	{
		return EType::None;
	}
	const bool bKnee = Hits(P.KneeDistCm, S.MaxEnterDistanceCm);
	const bool bChest = Hits(P.ChestDistCm, S.MaxEnterDistanceCm);
	const bool bHead = Hits(P.HeadDistCm, S.MaxEnterDistanceCm);
	if (bKnee && bChest && bHead)
	{
		return EType::High;
	}
	if (bKnee && !bHead)
	{
		// Knee only (a kerb planter) still hides a crouching body; chest without head is a car or a low wall.
		return EType::Low;
	}
	return EType::None;
}

struct FEdges
{
	bool bLeft = false;
	bool bRight = false;
};

/** Edges where the player can peek or lean out (a high cover needs one to aim). */
inline FEdges FindEdges(const FProbe& P, const FSettings& S = FSettings())
{
	FEdges E;
	E.bLeft = !Hits(P.LeftSideDistCm, S.EdgeClearCm);
	E.bRight = !Hits(P.RightSideDistCm, S.EdgeClearCm);
	return E;
}

enum class EAimFrom : int
{
	/** Cannot aim from here: a high cover with no edge nearby. Blind fire only. */
	BlindOnly = 0,
	/** Stands up over a low cover. */
	OverTop = 1,
	LeanLeft = 2,
	LeanRight = 3
};

/** How the player aims from cover. PreferRight: the camera shoulder. */
inline EAimFrom AimFrom(EType Type, const FEdges& Edges, bool bPreferRight)
{
	if (Type == EType::Low)
	{
		return EAimFrom::OverTop;
	}
	if (Type == EType::High)
	{
		if (Edges.bRight && (bPreferRight || !Edges.bLeft))
		{
			return EAimFrom::LeanRight;
		}
		if (Edges.bLeft)
		{
			return EAimFrom::LeanLeft;
		}
	}
	return EAimFrom::BlindOnly;
}

struct FPoint
{
	float X = 0.f;
	float Y = 0.f;
	/** Normal of the cover, pointing away from the wall toward where the body stands. */
	float NormalX = 1.f;
	float NormalY = 0.f;
	EType Type = EType::High;
};

inline float AngleBetweenDeg(float Ax, float Ay, float Bx, float By)
{
	const float La = std::sqrt(Ax * Ax + Ay * Ay);
	const float Lb = std::sqrt(Bx * Bx + By * By);
	if (La <= 1e-4f || Lb <= 1e-4f)
	{
		return 180.f;
	}
	const float C = std::clamp((Ax * Bx + Ay * By) / (La * Lb), -1.f, 1.f);
	return std::acos(C) * 57.2957795f;
}

/**
 * Next cover in the stick direction (world x, y) from the player at (Px, Py). Ranks by
 * angle off the stick plus distance; -1 when nothing qualifies.
 */
inline int PickNextCover(const std::vector<FPoint>& Points, float Px, float Py, float StickX, float StickY, const FSettings& S = FSettings())
{
	int Best = -1;
	float BestScore = 0.f;
	for (size_t I = 0; I < Points.size(); ++I)
	{
		const float Dx = Points[I].X - Px;
		const float Dy = Points[I].Y - Py;
		const float D = std::sqrt(Dx * Dx + Dy * Dy);
		if (D < S.MinMoveCm || D > S.MaxMoveCm || Points[I].Type == EType::None)
		{
			continue;
		}
		const float Angle = AngleBetweenDeg(Dx, Dy, StickX, StickY);
		if (Angle > S.MoveConeDeg)
		{
			continue;
		}
		// One degree off the stick weighs like 15 cm of extra distance.
		const float Score = D + Angle * 15.f;
		if (Best < 0 || Score < BestScore)
		{
			Best = static_cast<int>(I);
			BestScore = Score;
		}
	}
	return Best;
}

/**
 * Is a body in cover Point safe from a shooter at (Sx, Sy)? bExposed: the body is
 * aiming out (standing over a low cover or leaning), which always exposes it.
 */
inline bool IsProtected(const FPoint& Point, float Sx, float Sy, bool bExposed, const FSettings& S = FSettings())
{
	if (bExposed || Point.Type == EType::None)
	{
		return false;
	}
	// The cover protects against shooters on the wall side, i.e. opposite the normal.
	const float Angle = AngleBetweenDeg(Sx - Point.X, Sy - Point.Y, -Point.NormalX, -Point.NormalY);
	return Angle <= S.ProtectedHalfAngleDeg;
}
} // namespace GTPCover
