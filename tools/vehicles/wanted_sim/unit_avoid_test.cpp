// Checks how a police car and an officer on foot get round what stands in their way (GTPUnitAvoid.h).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/unit_avoid_test.cpp -o /tmp/unit_avoid_test && /tmp/unit_avoid_test
#include "GTPDriveLogic.h"
#include "GTPUnitAvoid.h"
#include "GTPUnitRecovery.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

struct FPost
{
	float X, Y, Radius;
};

// What the game's sweep gives: a ball of BallRadius pushed from (X, Y) along (Dx, Dy) for Length metres.
// Returns the metres pushed before it meets a post (negative: none), and where that post is, right (+) of the line.
static float Sweep(const std::vector<FPost>& Posts, float X, float Y, float Dx, float Dy, float Length, float BallRadius, float& OutLateral)
{
	float Best = -1.f;
	OutLateral = 0.f;
	for (const FPost& Post : Posts)
	{
		const float Rx = Post.X - X;
		const float Ry = Post.Y - Y;
		const float Along = Rx * Dx + Ry * Dy;
		const float Aside = -Rx * Dy + Ry * Dx; // to the right (the game's Y is to the right of its X)
		const float Reach = Post.Radius + BallRadius;
		if (std::fabs(Aside) >= Reach)
		{
			continue;
		}
		const float Hit = Along - std::sqrt(Reach * Reach - Aside * Aside);
		if (Hit > Length || Along < 0.f)
		{
			continue;
		}
		const float Metres = Hit > 0.f ? Hit : 0.f;
		if (Best < 0.f || Metres < Best)
		{
			Best = Metres;
			OutLateral = Aside;
		}
	}
	return Best;
}

struct FRun
{
	bool bTouched = false;
	float FarthestAside = 0.f;
	float EndX = 0.f;
	float EndY = 0.f;
	float EndSpeed = 0.f;
	float Seconds = 0.f;
	FGTPUnitAvoid Avoid;
};

// The car of the game on a straight lane along X, the driving of a chase, the pedals tamed as the director does.
static FRun Drive(const std::vector<FPost>& Posts, bool bAvoid, float UntilX, float MaxSeconds)
{
	const float HalfWidth = 1.075f, Nose = 2.3f, Tail = 2.3f, Dt = 1.f / 60.f;
	FGTPDriveSettings Driving;
	GTPDrive::SetChaseDriving(Driving);
	const FGTPUnitRecoverySettings Recovery;
	const FGTPUnitAvoidSettings S;
	const std::vector<FGTPDrivePoint> Path = {{0.f, 0.f}, {400.f, 0.f}};
	float X = 0.f, Y = 0.f, Yaw = 0.f, Speed = 0.f;
	int Closest = 0;
	FRun Run;
	while (X < UntilX && Run.Seconds < MaxSeconds)
	{
		const FGTPDriveCommand Command = GTPDrive::Decide(Path, X, Y, Yaw, Speed, 60.f / 3.6f, -1.f, Driving, Closest);
		float Throttle = Command.Throttle, Brake = Command.Brake, Steering = Command.Steering;
		if (bAvoid)
		{
			const float Look = FGTPUnitAvoid::LookMetres(Speed, S);
			const float Lean = Yaw + Steering * S.LeanRadians;
			// As in the game: the ball starts 1 m ahead of the car's middle, the bumper is taken 1.3 m further.
			const float StartX = X + std::cos(Yaw), StartY = Y + std::sin(Yaw);
			float Lateral = 0.f, Unused = 0.f;
			const float Raw = Sweep(Posts, StartX, StartY, std::cos(Lean), std::sin(Lean), Look + 1.3f, Run.Avoid.LookRadius(S), Lateral);
			const float Ahead = Raw < 0.f ? -1.f : std::max(0.f, Raw - 1.3f);
			float Left = -1.f, Right = -1.f;
			if (Ahead >= 0.f)
			{
				const float RawLeft = Sweep(Posts, StartX, StartY, std::cos(Lean - S.SideRadians), std::sin(Lean - S.SideRadians), Look + 1.3f, S.LookRadiusM, Unused);
				const float RawRight = Sweep(Posts, StartX, StartY, std::cos(Lean + S.SideRadians), std::sin(Lean + S.SideRadians), Look + 1.3f, S.LookRadiusM, Unused);
				Left = RawLeft < 0.f ? -1.f : std::max(0.f, RawLeft - 1.3f);
				Right = RawRight < 0.f ? -1.f : std::max(0.f, RawRight - 1.3f);
			}
			Run.Avoid.Step(Dt, Speed, Look, Ahead, Lateral, Left, Right, S, Throttle, Brake, Steering);
		}
		GTPUnitRecovery::Tame(Throttle, Brake, Steering, Speed, Driving.MaxLateralAccel, Recovery);
		// A plain car: 3 m/s2 on the throttle, 8 on the brake, the wheels at 45 degrees at full lock.
		Speed = std::max(0.f, Speed + (Throttle * 3.f - Brake * 8.f) * Dt);
		Yaw += Speed / 2.97f * std::tan(Steering * 0.785f) * Dt;
		X += Speed * std::cos(Yaw) * Dt;
		Y += Speed * std::sin(Yaw) * Dt;
		Run.Seconds += Dt;
		Run.FarthestAside = std::max(Run.FarthestAside, std::fabs(Y));
		for (const FPost& Post : Posts)
		{
			// The post in the car's own frame, against the car's rectangle.
			const float Rx = Post.X - X, Ry = Post.Y - Y;
			const float Along = Rx * std::cos(Yaw) + Ry * std::sin(Yaw);
			const float Aside = -Rx * std::sin(Yaw) + Ry * std::cos(Yaw);
			const float Dx = std::max(std::max(-Tail - Along, Along - Nose), 0.f);
			const float Dy = std::max(std::fabs(Aside) - HalfWidth, 0.f);
			if (std::hypot(Dx, Dy) < Post.Radius)
			{
				Run.bTouched = true;
			}
		}
	}
	Run.EndX = X;
	Run.EndY = Y;
	Run.EndSpeed = Speed;
	return Run;
}

// An officer walking at 4.3 m/s from (X, Y) to the goal, boxes in his way. Returns the seconds taken (negative: not there in MaxSeconds).
struct FBox
{
	float MinX, MinY, MaxX, MaxY;
};

static bool InBox(const FBox& B, float X, float Y, float Grow)
{
	return X > B.MinX - Grow && X < B.MaxX + Grow && Y > B.MinY - Grow && Y < B.MaxY + Grow;
}

// A step of 1.5 m looked along (Dx, Dy): true when a box is met, with the face met turned (Nx, Ny).
static bool Look(const std::vector<FBox>& Boxes, float X, float Y, float Dx, float Dy, float& Nx, float& Ny)
{
	const float Body = 0.3f;
	for (float Along = 0.05f; Along <= 1.5f; Along += 0.05f)
	{
		const float Px = X + Dx * Along, Py = Y + Dy * Along;
		for (const FBox& B : Boxes)
		{
			if (!InBox(B, Px, Py, Body))
			{
				continue;
			}
			// The face nearest the point before it went in.
			const float Qx = Px - Dx * 0.05f, Qy = Py - Dy * 0.05f;
			const float Left = B.MinX - Body - Qx, Right = Qx - (B.MaxX + Body), Down = B.MinY - Body - Qy, Up = Qy - (B.MaxY + Body);
			const float Most = std::max(std::max(Left, Right), std::max(Down, Up));
			Nx = Most == Left ? -1.f : (Most == Right ? 1.f : 0.f);
			Ny = Nx != 0.f ? 0.f : (Most == Down ? -1.f : 1.f);
			return true;
		}
	}
	return false;
}

static float Walk(const std::vector<FBox>& Boxes, float X, float Y, float GoalX, float GoalY, bool bDetour, float MaxSeconds, FGTPFootDetour& Detour)
{
	const float Dt = 1.f / 30.f, Speed = 4.3f;
	float Seconds = 0.f;
	while (Seconds < MaxSeconds)
	{
		const float ToGoal = std::hypot(GoalX - X, GoalY - Y);
		if (ToGoal < 2.f)
		{
			return Seconds;
		}
		const float WantX = (GoalX - X) / ToGoal, WantY = (GoalY - Y) / ToGoal;
		float DirX = WantX, DirY = WantY;
		if (bDetour)
		{
			float Nx = 0.f, Ny = 0.f, N2x = 0.f, N2y = 0.f;
			const bool bBlocked = Look(Boxes, X, Y, WantX, WantY, Nx, Ny);
			bool bAlongBlocked = false;
			if (bBlocked)
			{
				float Ax = 0.f, Ay = 0.f;
				Detour.Along(WantX, WantY, Nx, Ny, Ax, Ay);
				bAlongBlocked = Look(Boxes, X, Y, Ax, Ay, N2x, N2y);
			}
			Detour.Step(Dt, X, Y, WantX, WantY, bBlocked, Nx, Ny, bAlongBlocked, N2x, N2y, DirX, DirY);
		}
		// He slides along what he walks into, as a character does: the part of the step that goes in is lost.
		float Nx = X + DirX * Speed * Dt, Ny = Y + DirY * Speed * Dt;
		for (const FBox& B : Boxes)
		{
			if (InBox(B, Nx, Ny, 0.3f))
			{
				if (!InBox(B, Nx, Y, 0.3f))
				{
					Ny = Y;
				}
				else if (!InBox(B, X, Ny, 0.3f))
				{
					Nx = X;
				}
				else
				{
					Nx = X;
					Ny = Y;
				}
			}
		}
		X = Nx;
		Y = Ny;
		Seconds += Dt;
	}
	return -1.f;
}

int main()
{
	const FGTPUnitAvoidSettings S;
	{
		// Nothing ahead: the driving is left alone.
		FGTPUnitAvoid A;
		float Throttle = 0.8f, Brake = 0.f, Steering = 0.3f;
		A.Step(0.016f, 12.f, FGTPUnitAvoid::LookMetres(12.f, S), -1.f, 0.f, -1.f, -1.f, S, Throttle, Brake, Steering);
		CHECK(Throttle == 0.8f && Brake == 0.f && Steering == 0.3f && A.Side == 0 && A.Rounds == 0);
	}
	{
		// A lamp post 8 m ahead at 43 km/h, a little to the right of the line, both ways round free: round it by the
		// left, and slower (from 43 km/h the car cannot be sure to turn in 8 m).
		FGTPUnitAvoid A;
		float Throttle = 1.f, Brake = 0.f, Steering = 0.f;
		A.Step(0.016f, 12.f, 10.8f, 8.f, 0.3f, -1.f, -1.f, S, Throttle, Brake, Steering);
		CHECK(A.Side == -1 && Steering <= -S.MinSteer && Throttle == 0.f && Brake > 0.f && A.Rounds == 1);
		// The side is kept while the hold lasts, even when the other one looks better for a frame.
		Steering = 0.2f;
		A.Step(0.016f, 11.f, 9.9f, 6.f, 0.3f, 2.f, -1.f, S, Throttle, Brake, Steering);
		CHECK(A.Side == -1 && Steering < 0.f && A.Rounds == 1);
		// Past it: for the rest of the hold the wheel does not turn back to the right.
		Steering = 0.5f;
		Throttle = 1.f;
		Brake = 0.f;
		A.Step(0.016f, 9.f, 8.1f, -1.f, 0.f, -1.f, -1.f, S, Throttle, Brake, Steering);
		CHECK(Steering == 0.f && Throttle == 1.f && Brake == 0.f);
		// The hold over: the driving has the wheel again.
		Steering = 0.5f;
		A.Step(2.f, 9.f, 8.1f, -1.f, 0.f, -1.f, -1.f, S, Throttle, Brake, Steering);
		CHECK(Steering == 0.5f && A.Side == 0);
	}
	{
		// A wall to the left of what is ahead: round it by the right.
		FGTPUnitAvoid A;
		float Throttle = 1.f, Brake = 0.f, Steering = -0.1f;
		A.Step(0.016f, 6.f, 5.4f, 4.f, -0.2f, 3.f, -1.f, S, Throttle, Brake, Steering);
		CHECK(A.Side == 1 && Steering >= S.MinSteer);
	}
	{
		// A wall across the way, 1 m from the bumper, no better either side: the car stands, counted once.
		FGTPUnitAvoid A;
		float Throttle = 1.f, Brake = 0.f, Steering = 0.f;
		A.Step(0.016f, 0.2f, 5.f, 1.f, 0.f, 1.2f, 0.8f, S, Throttle, Brake, Steering);
		CHECK(A.Side == 0 && Throttle == 0.f && Brake == 1.f && A.Blocked == 1);
		Throttle = 1.f;
		Brake = 0.f;
		A.Step(0.016f, 0.f, 5.f, 1.f, 0.f, 1.2f, 0.8f, S, Throttle, Brake, Steering);
		CHECK(Throttle == 0.f && Brake == 1.f && A.Blocked == 1);
	}
	{
		// The whole thing driven: a post on the lane, 30 cm right of its middle, 60 m from the start. Without the
		// look the car runs into it; with it the car goes round, does not touch it, and is back on its lane.
		const std::vector<FPost> Posts = {{60.f, 0.3f, 0.2f}};
		const FRun Blind = Drive(Posts, false, 120.f, 60.f);
		CHECK(Blind.bTouched);
		const FRun Seeing = Drive(Posts, true, 120.f, 60.f);
		std::printf("post on the lane: touched %d, %.1f m aside at most, %.1f s to 120 m (%.1f s blind), %d way(s) round, ends %.2f m from the lane at %.0f km/h\n",
			Seeing.bTouched ? 1 : 0, Seeing.FarthestAside, Seeing.Seconds, Blind.Seconds, Seeing.Avoid.Rounds, Seeing.EndY, Seeing.EndSpeed * 3.6f);
		CHECK(!Seeing.bTouched && Seeing.EndX >= 120.f && std::fabs(Seeing.EndY) < 1.f && Seeing.FarthestAside < 4.f);
		CHECK(Seeing.Seconds < Blind.Seconds + 4.f && Seeing.Avoid.Rounds >= 1 && Seeing.Avoid.Blocked == 0);
	}
	{
		// The same post dead on the middle of the lane.
		const std::vector<FPost> Posts = {{60.f, 0.f, 0.2f}};
		const FRun Seeing = Drive(Posts, true, 120.f, 60.f);
		std::printf("post dead ahead: touched %d, %.1f m aside at most, %.1f s to 120 m\n", Seeing.bTouched ? 1 : 0, Seeing.FarthestAside, Seeing.Seconds);
		CHECK(!Seeing.bTouched && Seeing.EndX >= 120.f && std::fabs(Seeing.EndY) < 1.f);
	}
	{
		// Parked cars along the right of the lane, their sides 1.4 m from its middle (the car's own side is 1.08 m
		// from it): nothing is in the way, the unit must not be slowed.
		std::vector<FPost> Posts;
		for (float X = 20.f; X <= 110.f; X += 1.f)
		{
			Posts.push_back({X, 2.3f, 0.9f});
		}
		const FRun Free = Drive({}, true, 120.f, 60.f);
		const FRun Seeing = Drive(Posts, true, 120.f, 60.f);
		std::printf("parked cars beside the lane: touched %d, %.1f s to 120 m (%.1f s with nothing there), %d way(s) round\n", Seeing.bTouched ? 1 : 0,
			Seeing.Seconds, Free.Seconds, Seeing.Avoid.Rounds);
		CHECK(!Seeing.bTouched && Seeing.Seconds < Free.Seconds + 0.5f && Seeing.Avoid.Rounds == 0);
	}
	{
		// A wall across the road 60 m away, 30 m to each side, as at the end of a street: the car turns along it and
		// goes round its end, without touching it.
		std::vector<FPost> Posts;
		for (float Y = -30.f; Y <= 30.f; Y += 0.5f)
		{
			Posts.push_back({60.f, Y, 0.5f});
		}
		const FRun Seeing = Drive(Posts, true, 59.f, 12.f);
		std::printf("wall across: touched %d, at %.1f m and %.1f m aside after %.0f s, %.0f km/h, %d way(s) round\n", Seeing.bTouched ? 1 : 0, Seeing.EndX, Seeing.EndY,
			Seeing.Seconds, Seeing.EndSpeed * 3.6f, Seeing.Avoid.Rounds);
		CHECK(!Seeing.bTouched && Seeing.Avoid.Rounds >= 1 && Seeing.FarthestAside > 30.f);
	}
	{
		// A dead end: the same wall and one each side of the road, 4 m from its middle. The car stops short and stands.
		std::vector<FPost> Posts;
		for (float Y = -4.f; Y <= 4.f; Y += 0.5f)
		{
			Posts.push_back({60.f, Y, 0.5f});
		}
		for (float X = 20.f; X <= 60.f; X += 0.5f)
		{
			Posts.push_back({X, -4.f, 0.5f});
			Posts.push_back({X, 4.f, 0.5f});
		}
		const FRun Seeing = Drive(Posts, true, 120.f, 20.f);
		std::printf("dead end: touched %d, stands at %.1f m, %.0f km/h, stood %d time(s)\n", Seeing.bTouched ? 1 : 0, Seeing.EndX, Seeing.EndSpeed * 3.6f,
			Seeing.Avoid.Blocked);
		CHECK(!Seeing.bTouched && Seeing.EndSpeed < 1.f && Seeing.EndX < 60.f && Seeing.Avoid.Blocked >= 1);
	}
	{
		// The officer of gate 75: a building between him and the player, 36 m away. Walking straight he never gets
		// there; following the wall he does, in about the time the way round takes at his pace.
		const std::vector<FBox> Boxes = {{10.f, -12.f, 22.f, 9.f}};
		FGTPFootDetour None, Detour;
		CHECK(Walk(Boxes, 0.f, 0.f, 36.f, 0.f, false, 60.f, None) < 0.f);
		const float Seconds = Walk(Boxes, 0.f, 0.f, 36.f, 0.f, true, 60.f, Detour);
		std::printf("officer round a building: %.1f s, %d detour(s), %d change(s) of hand\n", Seconds, Detour.Detours, Detour.Flips);
		CHECK(Seconds > 0.f && Seconds < 20.f && Detour.Detours >= 1);
	}
	{
		// A corner he walks into (two walls, an L open towards him): he gets out of it and round.
		const std::vector<FBox> Boxes = {{10.f, -6.f, 12.f, 6.f}, {4.f, 6.f, 12.f, 8.f}};
		FGTPFootDetour Detour;
		const float Seconds = Walk(Boxes, 0.f, 3.f, 30.f, 3.f, true, 60.f, Detour);
		std::printf("officer out of a corner: %.1f s, %d detour(s), %d change(s) of hand\n", Seconds, Detour.Detours, Detour.Flips);
		CHECK(Seconds > 0.f && Seconds < 25.f);
	}
	{
		// Nothing in the way: straight there.
		FGTPFootDetour Detour;
		const float Seconds = Walk({}, 0.f, 0.f, 36.f, 0.f, true, 60.f, Detour);
		CHECK(Seconds > 7.f && Seconds < 9.f && Detour.Detours == 0);
	}
	std::printf(Failures == 0 ? "unit avoid: all checks passed\n" : "unit avoid: %d check(s) failed\n", Failures);
	return Failures == 0 ? 0 : 1;
}
