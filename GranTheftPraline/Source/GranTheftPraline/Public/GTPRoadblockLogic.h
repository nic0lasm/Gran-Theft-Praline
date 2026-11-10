#pragma once

// Police roadblock placement, written without engine types so it can be
// tested outside Unreal (tools/vehicles/wanted_sim/roadblock_test.cpp).
// Units: metres. Not wired into the game yet (ADR-0004, next stage).
//
// A roadblock goes on the route the player is most likely to take: the lane
// path ahead of the player. Two cars stand across the road, nose to tail
// at an angle, at a point far enough that the player sees it coming.

#include <cmath>
#include <vector>

struct FGTPRoadPoint
{
	float X = 0.f;
	float Y = 0.f;
};

struct FGTPRoadblockSettings
{
	/** Distance ahead of the player along the lane path (m). */
	float AheadMetres = 220.f;
	/** Never closer than this straight-line distance (m): the player must see it. */
	float MinStraightMetres = 120.f;
	/** Lateral spacing of the two cars across the lane (m). */
	float Spacing = 2.6f;
	/** The cars stand at this angle to the road direction (degrees, 90 = across). */
	float AngleDegrees = 60.f;
};

struct FGTPRoadblockCar
{
	float X = 0.f;
	float Y = 0.f;
	/** Heading in radians, same convention as the lane path. */
	float Yaw = 0.f;
};

struct FGTPRoadblockPlan
{
	bool bValid = false;
	FGTPRoadblockCar Cars[2];
	/** Index of the path point the block sits on. */
	int PathIndex = 0;
};

namespace GTPRoadblock
{
	/**
	 * Plans a roadblock on Path (the lane points ahead of the player, in
	 * driving order) starting at index From. Returns an invalid plan when
	 * the path is shorter than the wanted distance.
	 */
	inline FGTPRoadblockPlan Plan(const std::vector<FGTPRoadPoint>& Path, int From, float PlayerX, float PlayerY, const FGTPRoadblockSettings& S)
	{
		FGTPRoadblockPlan Out;
		const int N = static_cast<int>(Path.size());
		if (N < 2 || From < 0 || From >= N - 1)
		{
			return Out;
		}
		float Travelled = 0.f;
		for (int I = From + 1; I < N; ++I)
		{
			Travelled += std::hypot(Path[I].X - Path[I - 1].X, Path[I].Y - Path[I - 1].Y);
			if (Travelled < S.AheadMetres)
			{
				continue;
			}
			if (std::hypot(Path[I].X - PlayerX, Path[I].Y - PlayerY) < S.MinStraightMetres)
			{
				continue; // the path doubles back: look further
			}
			const float Dx = Path[I].X - Path[I - 1].X, Dy = Path[I].Y - Path[I - 1].Y;
			const float Length = std::hypot(Dx, Dy);
			if (Length < 1e-4f)
			{
				continue;
			}
			const float Ux = Dx / Length, Uy = Dy / Length;
			// Left of the direction of travel.
			const float Lx = -Uy, Ly = Ux;
			const float Road = std::atan2(Uy, Ux);
			const float Angle = S.AngleDegrees * 3.14159265f / 180.f;
			for (int Car = 0; Car < 2; ++Car)
			{
				const float Side = Car == 0 ? -0.5f : 0.5f;
				Out.Cars[Car].X = Path[I].X + Lx * S.Spacing * Side;
				Out.Cars[Car].Y = Path[I].Y + Ly * S.Spacing * Side;
				// Both noses point to the same side, angled across the road.
				Out.Cars[Car].Yaw = Road + (Car == 0 ? Angle : -Angle);
			}
			Out.bValid = true;
			Out.PathIndex = I;
			return Out;
		}
		return Out;
	}

	/** True when the player has passed the block (it is behind them): time to take it away. */
	inline bool IsBehind(const FGTPRoadblockPlan& Plan, float PlayerX, float PlayerY, float PlayerYaw)
	{
		if (!Plan.bValid)
		{
			return true;
		}
		const float Dx = Plan.Cars[0].X - PlayerX, Dy = Plan.Cars[0].Y - PlayerY;
		return Dx * std::cos(PlayerYaw) + Dy * std::sin(PlayerYaw) < -20.f;
	}
}
