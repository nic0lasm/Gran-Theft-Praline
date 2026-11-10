#pragma once

// Driving decisions of a non-player driver, written without engine types so
// they can be simulated outside Unreal (tools/vehicles/traffic_sim).
// Units: metres, seconds, radians; x east, y north is fine, any right-handed
// or left-handed plane works as long as yaw is measured the same way.
//
// - Steering: pure pursuit towards a point LookAhead metres along the path.
// - Speed: the lane's limit, lowered before bends (lateral acceleration cap)
//   and to stop SafeGap metres behind whatever is ahead.
// - Pedals: proportional throttle, brake when too fast; never both.

#include <cmath>
#include <vector>

struct FGTPDrivePoint
{
	float X = 0.f;
	float Y = 0.f;
};

struct FGTPDriveSettings
{
	float WheelBase = 2.6f;
	float MaxSteerAngle = 0.6f;          // about 35 degrees at full lock
	float LookAheadMin = 5.f;
	float LookAheadPerSpeed = 0.6f;      // extra metres per m/s
	float MaxLateralAccel = 2.2f;        // calm city driving
	float ComfortDecel = 3.5f;
	float SafeGap = 4.f;                 // metres kept to the obstacle ahead, bumper to bumper
	float ThrottleGain = 0.35f;
	float BrakeGain = 0.4f;
};

struct FGTPDriveCommand
{
	float Throttle = 0.f;
	float Brake = 0.f;
	float Steering = 0.f;    // -1 left, +1 right (positive yaw rate)
	float TargetSpeed = 0.f; // m/s, for debugging
};

namespace GTPDrive
{
	inline float WrapAngle(float A)
	{
		while (A > 3.14159265f) A -= 6.2831853f;
		while (A < -3.14159265f) A += 6.2831853f;
		return A;
	}

	inline float Clamp(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

	/** Point Distance metres along Path after the point closest to (X, Y); also returns that closest index. */
	inline FGTPDrivePoint PointAhead(const std::vector<FGTPDrivePoint>& Path, float X, float Y, float Distance, int& InOutClosest)
	{
		const int N = static_cast<int>(Path.size());
		if (N == 0)
		{
			return {X, Y};
		}
		// Search forward from the last closest point only: a lane never goes back.
		int Best = InOutClosest < 0 ? 0 : (InOutClosest >= N ? N - 1 : InOutClosest);
		float BestD = 1e30f;
		for (int I = Best; I < N && I < Best + 12; ++I)
		{
			const float D = (Path[I].X - X) * (Path[I].X - X) + (Path[I].Y - Y) * (Path[I].Y - Y);
			if (D < BestD)
			{
				BestD = D;
				InOutClosest = I;
			}
		}
		float Left = Distance;
		FGTPDrivePoint From = Path[InOutClosest];
		for (int I = InOutClosest + 1; I < N; ++I)
		{
			const float Seg = std::hypot(Path[I].X - From.X, Path[I].Y - From.Y);
			if (Seg >= Left && Seg > 1e-4f)
			{
				const float T = Left / Seg;
				return {From.X + (Path[I].X - From.X) * T, From.Y + (Path[I].Y - From.Y) * T};
			}
			Left -= Seg;
			From = Path[I];
		}
		return Path[N - 1];
	}

	/** Highest speed for the sharpest bend within Horizon metres ahead (m/s). */
	inline float BendSpeed(const std::vector<FGTPDrivePoint>& Path, int From, float Horizon, const FGTPDriveSettings& S, float LaneSpeed)
	{
		const int N = static_cast<int>(Path.size());
		float Travelled = 0.f;
		float Speed = LaneSpeed;
		for (int I = (From < 1 ? 1 : From); I + 1 < N && Travelled < Horizon; ++I)
		{
			const float Ax = Path[I].X - Path[I - 1].X, Ay = Path[I].Y - Path[I - 1].Y;
			const float Bx = Path[I + 1].X - Path[I].X, By = Path[I + 1].Y - Path[I].Y;
			const float La = std::hypot(Ax, Ay), Lb = std::hypot(Bx, By);
			Travelled += La;
			if (La < 1e-3f || Lb < 1e-3f)
			{
				continue;
			}
			const float Turn = std::fabs(WrapAngle(std::atan2(By, Bx) - std::atan2(Ay, Ax)));
			if (Turn < 1e-3f)
			{
				continue;
			}
			const float Radius = 0.5f * (La + Lb) / Turn;
			const float VBend = std::sqrt(S.MaxLateralAccel * Radius);
			// Slow down in time: v^2 = vBend^2 + 2 a d.
			const float VHere = std::sqrt(VBend * VBend + 2.f * S.ComfortDecel * (Travelled > 0.f ? Travelled - La : 0.f));
			if (VHere < Speed)
			{
				Speed = VHere;
			}
		}
		return Speed;
	}

	/**
	 * One decision. Path: points of the current lane followed by the next one(s).
	 * Yaw: heading of the car. Speed: forward speed (m/s). ObstacleDistance:
	 * free distance ahead along the path (negative = nothing seen).
	 */
	inline FGTPDriveCommand Decide(const std::vector<FGTPDrivePoint>& Path, float X, float Y, float Yaw, float Speed,
		float LaneSpeed, float ObstacleDistance, const FGTPDriveSettings& S, int& InOutClosest)
	{
		FGTPDriveCommand Out;
		if (Path.size() < 2)
		{
			Out.Brake = 1.f;
			return Out;
		}
		const float LookAhead = S.LookAheadMin + S.LookAheadPerSpeed * (Speed > 0.f ? Speed : 0.f);
		const FGTPDrivePoint Target = PointAhead(Path, X, Y, LookAhead, InOutClosest);
		const float Alpha = WrapAngle(std::atan2(Target.Y - Y, Target.X - X) - Yaw);
		const float Dist = std::hypot(Target.X - X, Target.Y - Y);
		const float Steer = std::atan2(2.f * S.WheelBase * std::sin(Alpha), Dist > 0.5f ? Dist : 0.5f);
		// Target behind (a hairpin): pure pursuit fades out there, so turn at full lock towards it.
		Out.Steering = std::fabs(Alpha) > 1.5708f ? (Alpha > 0.f ? 1.f : -1.f) : Clamp(Steer / S.MaxSteerAngle, -1.f, 1.f);

		float Want = BendSpeed(Path, InOutClosest, 40.f, S, LaneSpeed);
		if (ObstacleDistance >= 0.f)
		{
			const float Free = ObstacleDistance - S.SafeGap;
			const float VStop = Free > 0.f ? std::sqrt(2.f * S.ComfortDecel * Free) : 0.f;
			if (VStop < Want)
			{
				Want = VStop;
			}
		}
		// End of the known path: stop there.
		const FGTPDrivePoint& Last = Path.back();
		const float ToEnd = std::hypot(Last.X - X, Last.Y - Y);
		const float VEnd = std::sqrt(2.f * S.ComfortDecel * (ToEnd > 1.f ? ToEnd - 1.f : 0.f));
		if (VEnd < Want)
		{
			Want = VEnd;
		}
		Out.TargetSpeed = Want;

		const float Error = Want - Speed;
		if (Want < 0.3f && Speed < 0.5f)
		{
			Out.Brake = 1.f; // hold still
		}
		else if (Error >= 0.f)
		{
			Out.Throttle = Clamp(Error * S.ThrottleGain + 0.15f, 0.f, 1.f);
		}
		else if (Error < -0.5f)
		{
			Out.Brake = Clamp(-Error * S.BrakeGain, 0.f, 1.f);
		}
		return Out;
	}
}
