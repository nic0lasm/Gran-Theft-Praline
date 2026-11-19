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

namespace GTPDrive
{
	/**
	 * Chases and demos (the police units, the car the chase demo drives): a flight, not a drive.
	 *
	 * The car is the game's own (2.97 m between the axles, 45 degrees at full lock), bends are taken at 6 m/s2.
	 * The braking planned (4 m/s2) is well under what the car gives, and the brake answers the whole error:
	 * with 6 or 7 m/s2 planned and a brake at 0.4 of the error, the pedals ran behind the plan and the car
	 * reached a corner worth 19 km/h at 26 to 54 km/h (tools/vehicles/wanted_sim/corner_drive_test.cpp, brakes
	 * of 6 and 9 m/s2). The point aimed at is nearer (3 m + 0.5 s) so a right angle is cut by 1.5 m, not 2.4 m.
	 */
	inline void SetChaseDriving(FGTPDriveSettings& S)
	{
		S.WheelBase = 2.97f;
		S.MaxSteerAngle = 0.785f;
		S.MaxLateralAccel = 6.f;
		S.ComfortDecel = 4.f;
		S.ThrottleGain = 0.8f;
		S.BrakeGain = 1.f;
		S.LookAheadMin = 3.f;
		S.LookAheadPerSpeed = 0.5f;
	}
}

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

	/** The point Distance metres along Path ahead of a car at (X, Y); also returns the index of the path point closest to it. */
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
		// The distance counts from where the car is along the path, not from its closest point: with lane
		// points 20 m apart that point is up to 10 m behind or ahead of the car, and the point aimed at was
		// then beside the car or behind it (the car swerved, or turned on itself, near the corners of such lanes).
		int Start = InOutClosest;
		FGTPDrivePoint From = Path[InOutClosest];
		{
			float BestOff = 1e30f;
			for (int Seg = InOutClosest - 1; Seg <= InOutClosest; ++Seg)
			{
				if (Seg < 0 || Seg + 1 >= N)
				{
					continue;
				}
				const float Dx = Path[Seg + 1].X - Path[Seg].X, Dy = Path[Seg + 1].Y - Path[Seg].Y;
				const float L2 = Dx * Dx + Dy * Dy;
				if (L2 < 1e-8f)
				{
					continue;
				}
				const float T = Clamp(((X - Path[Seg].X) * Dx + (Y - Path[Seg].Y) * Dy) / L2, 0.f, 1.f);
				const FGTPDrivePoint Foot = {Path[Seg].X + Dx * T, Path[Seg].Y + Dy * T};
				const float Off = (Foot.X - X) * (Foot.X - X) + (Foot.Y - Y) * (Foot.Y - Y);
				// At equal distance (on a corner) the segment ahead is the one the car is turning into.
				if (Off <= BestOff)
				{
					BestOff = Off;
					From = Foot;
					Start = Seg;
				}
			}
		}
		float Left = Distance;
		for (int I = Start + 1; I < N; ++I)
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
	 * The point Distance metres of path before (bForward false) or after vertex I, between the points when it
	 * falls there; the end of the path when the path is shorter. OutLength: metres of path really covered.
	 */
	inline FGTPDrivePoint PointFromVertex(const std::vector<FGTPDrivePoint>& Path, int I, float Distance, bool bForward, float& OutLength)
	{
		const int N = static_cast<int>(Path.size());
		const int Step = bForward ? 1 : -1;
		FGTPDrivePoint From = Path[I];
		OutLength = 0.f;
		for (int K = I + Step; K >= 0 && K < N; K += Step)
		{
			const float Seg = std::hypot(Path[K].X - From.X, Path[K].Y - From.Y);
			if (OutLength + Seg >= Distance && Seg > 1e-4f)
			{
				const float T = (Distance - OutLength) / Seg;
				OutLength = Distance;
				return {From.X + (Path[K].X - From.X) * T, From.Y + (Path[K].Y - From.Y) * T};
			}
			OutLength += Seg;
			From = Path[K];
		}
		return From;
	}

	/**
	 * Highest speed for the sharpest bend within Horizon metres ahead of a car at (X, Y) whose closest path
	 * point is From (m/s). The bend at a vertex is measured between the points BendBaseline metres of path
	 * before and after it, and the distance to it from the car itself.
	 *
	 * Not between neighbouring points: lane points a metre apart carry a few degrees of noise that, taken as
	 * bends, held the car under 40 km/h on a straight road. And not between whole segments either, nor from the
	 * point behind the car: where two lanes meet at a right angle with their points 10 or 20 m apart, the corner
	 * read as a wide bend one segment further away, 46 to 65 km/h wanted at a corner worth 20. The chase demo's
	 * car and the police units went into the walls of four such corners at 45 to 55 km/h (gate of candidate 49).
	 */
	inline float BendSpeedAt(const std::vector<FGTPDrivePoint>& Path, int From, float X, float Y, float Horizon, const FGTPDriveSettings& S, float LaneSpeed)
	{
		constexpr float BendBaseline = 6.f;
		// A corner this far behind the car no longer holds it (the closest point stays the corner's for half the next segment).
		constexpr float PassedMetres = 3.f;
		const int N = static_cast<int>(Path.size());
		if (N < 3)
		{
			return LaneSpeed;
		}
		const int C = From < 0 ? 0 : (From >= N ? N - 1 : From);
		// Metres of path from the car to its closest point: positive when that point is still ahead.
		float ToClosest = 0.f;
		{
			float Before = 0.f, After = 0.f;
			if (C >= 1)
			{
				const float Dx = Path[C].X - Path[C - 1].X, Dy = Path[C].Y - Path[C - 1].Y;
				const float L = std::hypot(Dx, Dy);
				Before = L > 1e-4f ? ((Path[C].X - X) * Dx + (Path[C].Y - Y) * Dy) / L : 0.f;
			}
			if (C + 1 < N)
			{
				const float Dx = Path[C + 1].X - Path[C].X, Dy = Path[C + 1].Y - Path[C].Y;
				const float L = std::hypot(Dx, Dy);
				After = L > 1e-4f ? ((X - Path[C].X) * Dx + (Y - Path[C].Y) * Dy) / L : 0.f;
			}
			ToClosest = Before > After ? (Before > 0.f ? Before : 0.f) : -(After > 0.f ? After : 0.f);
		}
		float Along = 0.f; // metres of path from the closest point to vertex I
		float Speed = LaneSpeed;
		for (int I = C; I + 1 < N && Along + ToClosest < Horizon; ++I)
		{
			if (I > C)
			{
				Along += std::hypot(Path[I].X - Path[I - 1].X, Path[I].Y - Path[I - 1].Y);
			}
			if (I < 1)
			{
				continue;
			}
			const float ToVertex = Along + ToClosest;
			if (ToVertex < -PassedMetres)
			{
				continue;
			}
			float BackLen = 0.f, FwdLen = 0.f;
			const FGTPDrivePoint Back = PointFromVertex(Path, I, BendBaseline, false, BackLen);
			const FGTPDrivePoint Fwd = PointFromVertex(Path, I, BendBaseline, true, FwdLen);
			if (BackLen < 0.6f * BendBaseline || FwdLen < 0.6f * BendBaseline)
			{
				continue; // too close to either end of the known path to tell a bend from noise
			}
			const float Ax = Path[I].X - Back.X, Ay = Path[I].Y - Back.Y;
			const float Bx = Fwd.X - Path[I].X, By = Fwd.Y - Path[I].Y;
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
			const float Distance = ToVertex > 0.f ? ToVertex : 0.f;
			const float VHere = std::sqrt(VBend * VBend + 2.f * S.ComfortDecel * Distance);
			if (VHere < Speed)
			{
				Speed = VHere;
			}
		}
		return Speed;
	}

	/** The same for a car standing on the point before From (the way it was first written; kept for its tests). */
	inline float BendSpeed(const std::vector<FGTPDrivePoint>& Path, int From, float Horizon, const FGTPDriveSettings& S, float LaneSpeed)
	{
		const int N = static_cast<int>(Path.size());
		if (N == 0)
		{
			return LaneSpeed;
		}
		const int C = From < 1 ? 1 : (From >= N ? N - 1 : From);
		const FGTPDrivePoint& At = Path[C >= 1 && C - 1 < N ? C - 1 : 0];
		return BendSpeedAt(Path, C, At.X, At.Y, Horizon, S, LaneSpeed);
	}

	/**
	 * Metres of path a driver must know ahead to hold SpeedLimit (m/s): Decide() brakes for the end of the
	 * known path (v^2 = 2 a d), so a path shorter than the braking distance caps the speed. With the calm
	 * settings and the lane follower's 50 m, that cap is 67 km/h whatever the speed asked (police chases).
	 */
	inline float PathNeeded(float SpeedLimit, const FGTPDriveSettings& S)
	{
		const float Decel = S.ComfortDecel > 0.1f ? S.ComfortDecel : 0.1f;
		return SpeedLimit * SpeedLimit / (2.f * Decel) + 20.f;
	}

	/**
	 * Metres of road open after entering Lane before every way ends, searched Depth junctions deep and no further
	 * than Budget (a lane that still leads somewhere at that depth counts as open: Budget). A flight picks its exits
	 * with it: the chase demo of the packaged game stood still from its 8th second in a lane whose way ends on the
	 * quay (door 41). NextOf(Lane): the lanes it leads to; LengthOf(Lane): metres.
	 */
	template <typename FNextOf, typename FLengthOf>
	float OpenRoad(int Lane, int Depth, float Budget, const FNextOf& NextOf, const FLengthOf& LengthOf)
	{
		const float Own = LengthOf(Lane);
		if (Own >= Budget)
		{
			return Budget;
		}
		float Best = 0.f;
		for (const int Next : NextOf(Lane))
		{
			if (Depth <= 0)
			{
				return Budget;
			}
			const float Open = OpenRoad(Next, Depth - 1, Budget - Own, NextOf, LengthOf);
			if (Open > Best)
			{
				Best = Open;
			}
			if (Own + Best >= Budget)
			{
				return Budget;
			}
		}
		return Own + Best;
	}

	/** Metres of path watched for bends at Speed (m/s): the braking distance, never under the 40 m of city speeds. */
	inline float BendHorizon(float Speed, const FGTPDriveSettings& S)
	{
		const float Decel = S.ComfortDecel > 0.1f ? S.ComfortDecel : 0.1f;
		const float Braking = Speed > 0.f ? Speed * Speed / (2.f * Decel) + 10.f : 0.f;
		return Braking > 40.f ? Braking : 40.f;
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

		float Want = BendSpeedAt(Path, InOutClosest, X, Y, BendHorizon(Speed, S), S, LaneSpeed);
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

