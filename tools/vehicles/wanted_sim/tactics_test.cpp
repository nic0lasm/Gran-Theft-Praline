// Checks the police tactics rules (GTPPoliceTactics.h): deploying officers, shooting, searching, foot chase.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/tactics_test.cpp -o /tmp/tactics_test && /tmp/tactics_test
#include "GTPPoliceTactics.h"
#include "GTPDriveLogic.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

int main()
{
	using namespace GTPPoliceTactics;
	const FSettings S;

	// Officers by level.
	CHECK(OfficersForStars(0, 4) == 0 && OfficersForStars(1, 4) == 1 && OfficersForStars(5, 4) == 4);
	CHECK(OfficersForStars(5, 2) == 2 && OfficersForStars(9, 9) == 4);

	// Deploy: only for a player on foot and close, or a stuck car; never without stars or above the quota.
	CHECK(ShouldDeploy(S, 2, true, 3000.f, false, 0, 4));
	CHECK(!ShouldDeploy(S, 2, false, 3000.f, false, 0, 4));
	CHECK(!ShouldDeploy(S, 2, true, 5000.f, false, 0, 4));
	CHECK(ShouldDeploy(S, 2, false, 5000.f, true, 0, 4));
	CHECK(!ShouldDeploy(S, 0, true, 100.f, false, 0, 4));
	CHECK(!ShouldDeploy(S, 2, true, 3000.f, false, 2, 4));
	CHECK(!ShouldDeploy(S, 5, true, 3000.f, false, 0, 0));

	// Shooting: never at one star, only when provoked at two, always from three.
	CHECK(!ShootsOnSight(S, 1, true) && !ShootsOnSight(S, 2, false) && ShootsOnSight(S, 2, true));
	CHECK(ShootsOnSight(S, 3, false) && ShootsOnSight(S, 5, false));
	CHECK(MayFire(S, 3, false, 2000.f, true, 0.f));
	CHECK(!MayFire(S, 3, false, 2000.f, false, 0.f));
	CHECK(!MayFire(S, 3, false, 4000.f, true, 0.f));
	CHECK(!MayFire(S, 3, false, 2000.f, true, 0.4f));
	CHECK(!MayFire(S, 1, true, 2000.f, true, 0.f));
	// Aim gets better and fire faster with the level.
	CHECK(SpreadFactor(S, 3) == S.SpreadAtThree && SpreadFactor(S, 5) < SpreadFactor(S, 3) && SpreadFactor(S, 5) >= 1.f);
	CHECK(FireInterval(S, 5) < FireInterval(S, 3) && FireInterval(S, 5) >= S.FireIntervalMin);

	// Search: officers spread over distinct points, the ring widens up to its cap.
	{
		float X0, Y0, X1, Y1, X2, Y2;
		SearchOffset(S, 0, 3, 0.f, X0, Y0);
		SearchOffset(S, 1, 3, 0.f, X1, Y1);
		SearchOffset(S, 2, 3, 0.f, X2, Y2);
		CHECK(std::hypot(X0 - X1, Y0 - Y1) > 1000.f && std::hypot(X1 - X2, Y1 - Y2) > 1000.f && std::hypot(X0 - X2, Y0 - Y2) > 1000.f);
		CHECK(std::fabs(std::hypot(X0, Y0) - S.SearchRadius) < 1.f);
		float Xa, Ya;
		SearchOffset(S, 0, 3, 40.f, Xa, Ya);
		CHECK(std::hypot(Xa, Ya) > S.SearchRadius);
		SearchOffset(S, 0, 3, 5000.f, Xa, Ya);
		CHECK(std::hypot(Xa, Ya) <= S.SearchMaxRadius + 1.f);
	}

	// Foot chase against a player who stands still: the officer arrives from 30 m in about 7 s.
	{
		float X = 0.f, Y = 0.f;
		float T = 0.f;
		while (!StepToward(S, X, Y, 3000.f, 0.f, 1.f / 30.f) && T < 30.f)
		{
			T += 1.f / 30.f;
		}
		CHECK(T > 5.f && T < 8.f);
		CHECK(std::fabs(3000.f - X) <= S.ArriveRadius + 1.f);
	}
	// Against a player who runs faster than an officer, the gap never closes: foot officers alone do not catch a sprinter.
	{
		float X = 0.f, Y = 0.f, Px = 1000.f;
		const float PlayerSpeed = 600.f;
		for (float T = 0.f; T < 20.f; T += 1.f / 30.f)
		{
			Px += PlayerSpeed / 30.f;
			StepToward(S, X, Y, Px, 0.f, 1.f / 30.f);
		}
		CHECK(Px - X > 1000.f);
	}

	// Escalation: more cars, faster, rams and roadblocks as the level rises.
	{
		for (int Stars = 1; Stars < 5; ++Stars)
		{
			CHECK(Profile(Stars + 1, 9).ChaseKmh >= Profile(Stars, 9).ChaseKmh);
			CHECK(Profile(Stars + 1, 9).Cars >= Profile(Stars, 9).Cars);
		}
		CHECK(Profile(0, 9).Cars == 0);
		CHECK(!Profile(2, 9).bRam && Profile(3, 9).bRam);
		CHECK(Profile(2, 9).RoadblockCars == 0 && Profile(3, 9).RoadblockCars == 2);
		CHECK(!Profile(1, 9).bFlank && Profile(2, 9).bFlank);
		CHECK(Profile(5, 2).Cars == 2);
		CHECK(Profile(1, 9).ChaseKmh >= 70.f && Profile(5, 9).ChaseKmh >= 130.f);
		// Ahead of the player: in front yes, behind and beside no.
		CHECK(IsAhead(100.f, 5.f, 1.f, 0.f, 0.8f));
		CHECK(!IsAhead(-100.f, 0.f, 1.f, 0.f, 0.8f));
		CHECK(!IsAhead(0.f, 100.f, 1.f, 0.f, 0.8f));
	}

	// What a unit does with what its probe meets (gate 93: at three stars and more it looked at nothing, four units
	// off their wheels in a minute).
	{
		// Below the ramming levels everything ahead is braked for, the car of the man it is after too.
		for (const EMet Met : {EMet::Quarry, EMet::Police, EMet::Person, EMet::Other})
		{
			CHECK(ObstacleFor(8.f, Met, false) == 8.f);
		}
		// A unit that rams: only the man it is after and his car are not braked for.
		CHECK(ObstacleFor(8.f, EMet::Quarry, true) < 0.f);
		CHECK(ObstacleFor(8.f, EMet::Police, true) == 8.f && ObstacleFor(8.f, EMet::Person, true) == 8.f && ObstacleFor(8.f, EMet::Other, true) == 8.f);
		CHECK(ObstacleFor(0.f, EMet::Other, true) == 0.f);
		// Nothing met stays nothing.
		CHECK(ObstacleFor(-1.f, EMet::Nothing, true) < 0.f && ObstacleFor(-1.f, EMet::Nothing, false) < 0.f);
		// Held up: pushed through after a moment. A car, yes; a colleague or a person, no.
		CHECK(ShovesThrough(EMet::Other) && ShovesThrough(EMet::Quarry) && !ShovesThrough(EMet::Police) && !ShovesThrough(EMet::Person) && !ShovesThrough(EMet::Nothing));
		// Two stars: at a crawl only, as before. One star: never.
		CHECK(IsHeldUp(10.f, 6.f, EMet::Other, 2, false) && !IsHeldUp(30.f, 6.f, EMet::Other, 2, false) && !IsHeldUp(10.f, 6.f, EMet::Other, 1, false));
		CHECK(IsHeldUp(10.f, 6.f, EMet::Quarry, 2, false));
		// Three stars: behind a car of the traffic at 30 km/h too; not behind a colleague, a person, or nothing.
		CHECK(IsHeldUp(30.f, 6.f, EMet::Other, 3, true) && IsHeldUp(80.f, 11.f, EMet::Other, 4, true));
		CHECK(!IsHeldUp(30.f, 6.f, EMet::Police, 3, true) && !IsHeldUp(5.f, 6.f, EMet::Person, 3, true) && !IsHeldUp(5.f, -1.f, EMet::Nothing, 3, true));
		// Far ahead is not held up.
		CHECK(!IsHeldUp(5.f, 12.f, EMet::Other, 3, true) && !IsHeldUp(5.f, 30.f, EMet::Other, 2, false));
		for (const EMet Met : {EMet::Nothing, EMet::Quarry, EMet::Police, EMet::Person, EMet::Other})
		{
			CHECK(MetName(Met) != nullptr && MetName(Met)[0] != 0);
		}
	}

	// What drives the unit's own way is followed, not braked for as a wall (gate 102: "speed=66 km/h obstacle=29.8 m
	// met=police brake=1.00", three units one behind the other on the way of the flight, 40 km/h on average).
	{
		// A colleague, a car of the traffic, the car of the man it is after: followed from 2 m/s. A person never.
		CHECK(LeaderSpeed(EMet::Police, 18.f) == 18.f && LeaderSpeed(EMet::Other, 9.f) == 9.f && LeaderSpeed(EMet::Quarry, 14.f) == 14.f);
		CHECK(LeaderSpeed(EMet::Person, 3.f) < 0.f && LeaderSpeed(EMet::Nothing, 18.f) < 0.f);
		// One that stands, creeps or comes the other way is a wall, as before.
		CHECK(LeaderSpeed(EMet::Police, 0.f) < 0.f && LeaderSpeed(EMet::Other, 1.9f) < 0.f && LeaderSpeed(EMet::Other, -12.f) < 0.f);
		// The line of gate 102, on a straight road with a unit's own settings: the whole brake without the speed of
		// the colleague, the throttle with it; and still the brake when the colleague is 6 m ahead.
		FGTPDriveSettings Unit;
		GTPDrive::SetChaseDriving(Unit);
		Unit.SafeGap = 3.f;
		Unit.IdmTimeGap = 0.7f;
		Unit.IdmMaxAccel = 3.f;
		const std::vector<FGTPDrivePoint> Road = {{0.f, 0.f}, {400.f, 0.f}};
		int Closest = 0;
		const float Speed = 66.f / 3.6f;
		const FGTPDriveCommand Wall = GTPDrive::Decide(Road, 10.f, 0.f, 0.f, Speed, 94.f / 3.6f, 29.8f, Unit, Closest);
		CHECK(Wall.Brake >= 0.99f && Wall.Throttle == 0.f);
		const FGTPDriveCommand Followed = GTPDrive::Decide(Road, 10.f, 0.f, 0.f, Speed, 94.f / 3.6f, 29.8f, Unit, Closest, LeaderSpeed(EMet::Police, Speed));
		CHECK(Followed.Brake == 0.f && Followed.Throttle > 0.3f);
		const FGTPDriveCommand Close = GTPDrive::Decide(Road, 10.f, 0.f, 0.f, Speed, 94.f / 3.6f, 6.f, Unit, Closest, LeaderSpeed(EMet::Police, Speed));
		CHECK(Close.Brake > 0.5f && Close.Throttle == 0.f);
		// A colleague who stands 29.8 m ahead: braked for, as before.
		const FGTPDriveCommand Stands = GTPDrive::Decide(Road, 10.f, 0.f, 0.f, Speed, 94.f / 3.6f, 29.8f, Unit, Closest, LeaderSpeed(EMet::Police, 0.f));
		CHECK(Stands.Brake >= 0.99f);
	}

	// The speed a unit left behind may reach (CatchUpKmh).
	{
		const FCatchUp C;
		// Near him, at one star, and with nothing to catch up: its level's speed.
		CHECK(CatchUpKmh(81.f, 99.f, 60.f, 2, C) == 81.f && CatchUpKmh(81.f, 99.f, 20.f, 3, C) == 81.f);
		CHECK(CatchUpKmh(63.f, 99.f, 250.f, 1, C) == 63.f);
		// 150 m behind and more: its top speed, whatever he does.
		CHECK(CatchUpKmh(81.f, 99.f, 150.f, 2, C) == C.TopKmh && CatchUpKmh(94.5f, 20.f, 290.f, 3, C) == C.TopKmh);
		// Between the two it rises with the distance, never under its level's speed, never over the top.
		float Before = 81.f;
		for (float Metres = 60.f; Metres <= 300.f; Metres += 5.f)
		{
			const float Kmh = CatchUpKmh(81.f, 50.f, Metres, 2, C);
			CHECK(Kmh >= Before && Kmh >= 81.f && Kmh <= C.TopKmh);
			Before = Kmh;
		}
		// Just past 60 m behind a car faster than its level: at least his speed, he does not leave it behind again.
		CHECK(CatchUpKmh(81.f, 99.f, 61.f, 2, C) >= 99.f && CatchUpKmh(81.f, 99.f, 61.f, 2, C) < 102.f);
		// A level already faster than the top keeps its speed.
		CHECK(CatchUpKmh(150.f, 99.f, 250.f, 5, C) == 150.f);
	}
	{
		// Gate 103: the player gave up 17 m from a roadblock car, which stood there and was taken away 12 s later.
		const FBlockRelease R;
		float Near = 0.f;
		// He drives past it: it stands.
		for (int Frame = 0; Frame < 600; ++Frame)
		{
			Near = BlockNearSeconds(Near, 1.f / 60.f, 14.f, 74.f, false, 3, R);
		}
		CHECK(Near == 0.f && !LeavesBlock(Near, R));
		// He has stopped 17 m from it: it leaves its place after a second and a half, not at the first frame.
		Near = BlockNearSeconds(Near, 1.f / 60.f, 17.f, 0.f, false, 3, R);
		CHECK(Near > 0.f && !LeavesBlock(Near, R));
		for (int Frame = 0; Frame < 95; ++Frame)
		{
			Near = BlockNearSeconds(Near, 1.f / 60.f, 17.f, 0.f, false, 3, R);
		}
		CHECK(LeavesBlock(Near, R));
		// He stopped far from it, or nobody is wanted: it stands.
		CHECK(BlockNearSeconds(1.f, 0.1f, 61.f, 0.f, false, 3, R) == 0.f);
		CHECK(BlockNearSeconds(1.f, 0.1f, 17.f, 0.f, false, 0, R) == 0.f);
		// A slow corner in front of it is not a stop; on foot he can no longer flee whatever his speed.
		CHECK(BlockNearSeconds(1.f, 0.1f, 17.f, 13.f, false, 3, R) == 0.f);
		CHECK(BlockNearSeconds(1.f, 0.1f, 17.f, 13.f, true, 3, R) > 1.f);
		// A unit that stands far from him pushes through a traffic car, never through a colleague.
		CHECK(PushesThrough(4.1f, EMet::Other) && PushesThrough(4.1f, EMet::Nothing) && !PushesThrough(3.9f, EMet::Other));
		CHECK(!PushesThrough(30.f, EMet::Police));
	}

	if (Failures == 0)
	{
		std::printf("tactics_test: all checks passed\n");
	}
	return Failures == 0 ? 0 : 1;
}
