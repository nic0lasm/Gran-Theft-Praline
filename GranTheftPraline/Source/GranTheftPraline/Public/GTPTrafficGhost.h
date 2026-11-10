// Ghost mode of far AI cars (docs/performance/vehicle-cost.md): physics off, the
// car slides along its planned path at a speed that obeys the same limits as a
// driven car (bends, the car ahead, red lights, junction stop lines). Plain C++,
// no engine types, so tools/vehicles/ghost_test checks it with g++.
#pragma once

#include "GTPDriveLogic.h"

#include <cmath>
#include <vector>

namespace GTPGhost
{
	/**
	 * Moves Distance metres forward along Path from point InOutIndex plus
	 * InOutAlong metres. Writes the new position and heading (radians); false
	 * at the end of the path (the car stops on its last point).
	 */
	inline bool Advance(const std::vector<FGTPDrivePoint>& Path, int& InOutIndex, float& InOutAlong, float Distance,
		FGTPDrivePoint& OutPosition, float& OutYaw)
	{
		const int N = static_cast<int>(Path.size());
		if (N < 2)
		{
			if (N == 1)
			{
				OutPosition = Path[0];
			}
			return false;
		}
		int Index = InOutIndex < 0 ? 0 : (InOutIndex > N - 2 ? N - 2 : InOutIndex);
		float Along = InOutAlong + (Distance > 0.f ? Distance : 0.f);
		bool bMoving = true;
		for (;;)
		{
			const FGTPDrivePoint& A = Path[Index];
			const FGTPDrivePoint& B = Path[Index + 1];
			const float Length = std::hypot(B.X - A.X, B.Y - A.Y);
			if (Along <= Length || Index + 2 >= N)
			{
				if (Along > Length)
				{
					Along = Length;
					bMoving = false;
				}
				const float T = Length > 1e-4f ? Along / Length : 0.f;
				OutPosition.X = A.X + (B.X - A.X) * T;
				OutPosition.Y = A.Y + (B.Y - A.Y) * T;
				if (Length > 1e-4f)
				{
					OutYaw = std::atan2(B.Y - A.Y, B.X - A.X);
				}
				break;
			}
			Along -= Length;
			++Index;
		}
		InOutIndex = Index;
		InOutAlong = Along;
		return bMoving;
	}

	/** Speed after Dt seconds towards Want (m/s): gentle acceleration, firmer braking, never below 0. */
	inline float NextSpeed(float Speed, float Want, float Accel, float Decel, float Dt)
	{
		if (Want > Speed)
		{
			const float Up = Speed + Accel * Dt;
			return Up < Want ? Up : Want;
		}
		const float Down = Speed - Decel * Dt;
		const float Next = Down > Want ? Down : Want;
		return Next > 0.f ? Next : 0.f;
	}

	/**
	 * Hysteresis between the two modes: a driven car turns into a ghost past
	 * GhostDistance from the player, a ghost wakes up again inside WakeDistance
	 * (WakeDistance < GhostDistance, so a car on the border does not flicker).
	 */
	inline bool WantsGhost(bool bGhostNow, float DistanceToPlayer, float GhostDistance, float WakeDistance)
	{
		return bGhostNow ? DistanceToPlayer > WakeDistance : DistanceToPlayer > GhostDistance;
	}

}
