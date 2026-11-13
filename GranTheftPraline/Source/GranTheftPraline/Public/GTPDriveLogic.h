#pragma once

// Driving decisions of a non-player driver, written without engine types so
// they can be simulated outside Unreal (tools/vehicles/traffic_sim).
// Units: metres, seconds, radians; x east, y north is fine, any right-handed
// or left-handed plane works as long as yaw is measured the same way.
//
// - Steering: pure pursuit towards a point LookAhead metres along the path.
// - Speed: the lane's limit, lowered before bends (lateral acceleration cap)
//   and to stop SafeGap metres behind whatever is ahead. Behind a moving car whose
//   speed is known, the Intelligent Driver Model (Treiber, Hennecke, Helbing 2000,
//   the car-following law also used by CARLA-style traffic managers) sets the speed:
//   a time gap instead of a stop at every car, so queues move off together.
// - Pedals: proportional throttle, brake when too fast; never both.
// - Junctions: one car at a time crosses a junction (FGTPJunctionTable);
//   the others wait at a stop line. Lights and priorities come in 0.9.

#include <cmath>
#include <unordered_map>
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
	float IdmTimeGap = 1.4f;             // seconds kept behind a moving car (IDM T)
	float IdmMaxAccel = 1.6f;            // m/s2 (IDM a)
	float IdmHorizon = 1.f;              // seconds: the IDM acceleration becomes a target speed
	float ThrottleGain = 0.35f;
	float BrakeGain = 0.4f;
};

struct FGTPDriveCommand
{
	float Throttle = 0.f;
	float Brake = 0.f;
	float Steering = 0.f;    // -1 left, +1 right (positive yaw rate)
	float TargetSpeed = 0.f; // m/s, for debugging
	/** What set TargetSpeed (logs of chases): see GTPDrive::ELimit. */
	int Limit = 0;
};

namespace GTPDrive
{
	/** Why the car wants the speed it wants: the posted or given limit, a bend, a car it follows, an obstacle, the end of the known path. */
	enum ELimit { LimitLane = 0, LimitBend = 1, LimitFollow = 2, LimitObstacle = 3, LimitPathEnd = 4, LimitCount = 5 };

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

	/**
	 * Highest speed for the sharpest bend within Horizon metres ahead (m/s). The bend at a vertex is measured
	 * over a baseline of BendBaseline metres each side, not between neighbouring points: lane points a metre
	 * apart carry a few degrees of noise that, taken as bends, held the car under 40 km/h on a straight road.
	 */
	inline float BendSpeed(const std::vector<FGTPDrivePoint>& Path, int From, float Horizon, const FGTPDriveSettings& S, float LaneSpeed)
	{
		constexpr float BendBaseline = 6.f;
		const int N = static_cast<int>(Path.size());
		float Travelled = 0.f;
		float Speed = LaneSpeed;
		for (int I = (From < 1 ? 1 : From); I + 1 < N && Travelled < Horizon; ++I)
		{
			Travelled += std::hypot(Path[I].X - Path[I - 1].X, Path[I].Y - Path[I - 1].Y);
			// Back and forward until BendBaseline metres of path (or the end of the known path).
			int Back = I;
			float BackLen = 0.f;
			while (Back > 0 && BackLen < BendBaseline)
			{
				BackLen += std::hypot(Path[Back].X - Path[Back - 1].X, Path[Back].Y - Path[Back - 1].Y);
				--Back;
			}
			int Fwd = I;
			float FwdLen = 0.f;
			while (Fwd + 1 < N && FwdLen < BendBaseline)
			{
				FwdLen += std::hypot(Path[Fwd + 1].X - Path[Fwd].X, Path[Fwd + 1].Y - Path[Fwd].Y);
				++Fwd;
			}
			const float Ax = Path[I].X - Path[Back].X, Ay = Path[I].Y - Path[Back].Y;
			const float Bx = Path[Fwd].X - Path[I].X, By = Path[Fwd].Y - Path[I].Y;
			const float La = std::hypot(Ax, Ay), Lb = std::hypot(Bx, By);
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
			const float Distance = Travelled > 0.f ? Travelled : 0.f;
			const float VHere = std::sqrt(VBend * VBend + 2.f * S.ComfortDecel * Distance);
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
	/**
	 * Intelligent Driver Model: speed to aim for behind a leader Gap metres ahead
	 * (bumper to bumper) going LeaderSpeed (m/s), for a car at Speed wanting Desired.
	 */
	inline float IdmSpeed(float Speed, float Desired, float Gap, float LeaderSpeed, const FGTPDriveSettings& S)
	{
		if (Desired < 0.1f)
		{
			return 0.f;
		}
		const float V = Speed > 0.f ? Speed : 0.f;
		const float Approach = V - LeaderSpeed;
		const float Dynamic = V * S.IdmTimeGap + V * Approach / (2.f * std::sqrt(S.IdmMaxAccel * S.ComfortDecel));
		const float Wanted = S.SafeGap + (Dynamic > 0.f ? Dynamic : 0.f);
		const float Free = Gap > 0.1f ? Gap : 0.1f;
		const float Ratio = V / Desired;
		const float Accel = S.IdmMaxAccel * (1.f - Ratio * Ratio * Ratio * Ratio - (Wanted / Free) * (Wanted / Free));
		const float Next = V + Accel * S.IdmHorizon;
		return Clamp(Next, 0.f, Desired);
	}

	inline FGTPDriveCommand Decide(const std::vector<FGTPDrivePoint>& Path, float X, float Y, float Yaw, float Speed,
		float LaneSpeed, float ObstacleDistance, const FGTPDriveSettings& S, int& InOutClosest, float LeaderSpeed = -1.f)
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
		Out.Limit = Want < LaneSpeed - 0.3f ? LimitBend : LimitLane;
		if (ObstacleDistance >= 0.f && LeaderSpeed > 0.5f && ObstacleDistance > S.SafeGap * 0.5f)
		{
			// A moving car ahead: follow it at a time gap (IDM).
			const float VFollow = IdmSpeed(Speed, Want, ObstacleDistance, LeaderSpeed, S);
			if (VFollow < Want)
			{
				Want = VFollow;
				Out.Limit = LimitFollow;
			}
		}
		else if (ObstacleDistance >= 0.f)
		{
			const float Free = ObstacleDistance - S.SafeGap;
			const float VStop = Free > 0.f ? std::sqrt(2.f * S.ComfortDecel * Free) : 0.f;
			if (VStop < Want)
			{
				Want = VStop;
				Out.Limit = LimitObstacle;
			}
		}
		// End of the known path: stop there.
		const FGTPDrivePoint& Last = Path.back();
		const float ToEnd = std::hypot(Last.X - X, Last.Y - Y);
		const float VEnd = std::sqrt(2.f * S.ComfortDecel * (ToEnd > 1.f ? ToEnd - 1.f : 0.f));
		if (VEnd < Want)
		{
			Out.Limit = LimitPathEnd;
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

/** Who is crossing each junction now (shared by all the drivers of a network). */
class FGTPJunctionTable
{
public:
	/** True when Car may cross Junction (free, or already its own). Junction < 0 is always free. */
	bool TryClaim(int Junction, int Car)
	{
		if (Junction < 0)
		{
			return true;
		}
		const auto Found = Holder.find(Junction);
		if (Found == Holder.end() || Found->second == Car)
		{
			Holder[Junction] = Car;
			return true;
		}
		return false;
	}

	void Release(int Junction, int Car)
	{
		const auto Found = Holder.find(Junction);
		if (Found != Holder.end() && Found->second == Car)
		{
			Holder.erase(Found);
		}
	}

	/** Frees everything Car holds (car taken away, stolen or wrecked). */
	void ReleaseAll(int Car)
	{
		for (auto It = Holder.begin(); It != Holder.end();)
		{
			It = It->second == Car ? Holder.erase(It) : std::next(It);
		}
	}

private:
	std::unordered_map<int, int> Holder;
};

/**
 * Junction handling of one car. Each tick: Update() with the junction at the
 * end of the current lane and the distance to it; it returns the distance to
 * stop at (stop line) when that junction is taken, else a negative value.
 * Claimed junctions are released once the car is ReleaseDistance past them.
 */
struct FGTPJunctionClaims
{
	struct FClaim
	{
		int Junction;
		FGTPDrivePoint Point;
	};

	float ApproachDistance = 12.f;   // claim this far before the junction (plus braking distance)
	float ReleaseDistance = 10.f;    // release this far past it
	float StopLineBack = 3.f;        // stop this far before the end of the lane
	float GiveUpSeconds = 6.f;       // stopped this long past a junction: let the others go (gridlock)
	std::vector<FClaim> Claims;

	/** StoppedSeconds: how long the car has been standing still. */
	float Update(FGTPJunctionTable& Table, int Car, int NextJunction, const FGTPDrivePoint& LaneEnd,
		float X, float Y, float Speed, float ComfortDecel, float StoppedSeconds = 0.f)
	{
		for (size_t Index = 0; Index < Claims.size();)
		{
			const FClaim& Claim = Claims[Index];
			const bool bAhead = Claim.Junction == NextJunction;
			if (!bAhead && (std::hypot(Claim.Point.X - X, Claim.Point.Y - Y) > ReleaseDistance || StoppedSeconds > GiveUpSeconds))
			{
				Table.Release(Claim.Junction, Car);
				Claims.erase(Claims.begin() + static_cast<long>(Index));
				continue;
			}
			++Index;
		}
		if (NextJunction < 0)
		{
			return -1.f;
		}
		const float ToEnd = std::hypot(LaneEnd.X - X, LaneEnd.Y - Y);
		const float Braking = Speed * Speed / (2.f * ComfortDecel);
		if (ToEnd > ApproachDistance + Braking)
		{
			return -1.f;
		}
		if (Table.TryClaim(NextJunction, Car))
		{
			bool bKnown = false;
			for (const FClaim& Claim : Claims)
			{
				bKnown = bKnown || Claim.Junction == NextJunction;
			}
			if (!bKnown)
			{
				Claims.push_back({NextJunction, LaneEnd});
			}
			return -1.f;
		}
		const float Stop = ToEnd - StopLineBack;
		return Stop > 0.f ? Stop : 0.f;
	}

	/** Gives back one junction (the car now waits at a red light before it). */
	void Drop(FGTPJunctionTable& Table, int Car, int Junction)
	{
		if (Junction < 0)
		{
			return;
		}
		Table.Release(Junction, Car);
		for (size_t Index = 0; Index < Claims.size(); ++Index)
		{
			if (Claims[Index].Junction == Junction)
			{
				Claims.erase(Claims.begin() + static_cast<long>(Index));
				break;
			}
		}
	}

	void Clear(FGTPJunctionTable& Table, int Car)
	{
		Table.ReleaseAll(Car);
		Claims.clear();
	}
};

