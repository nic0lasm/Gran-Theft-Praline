// Checks the police car that waits in a side street and pulls out behind the player (GTPPoliceIntercept.h), and why
// one is needed: a replay of the flight of gates 93 and 102 with a car that follows, and with one that pulls out.
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/police_intercept_test.cpp -o /tmp/police_intercept_test && /tmp/police_intercept_test
#include "GTPPoliceIntercept.h"
#include "GTPPoliceTactics.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL line %d: %s\n", __LINE__, #Cond); ++Failures; } } while (0)

namespace
{
	struct FLane
	{
		std::vector<float> Xs, Ys;
		std::vector<int> NextLanes;
	};

	struct FLanes
	{
		std::vector<FLane> All;
		int Num() const { return static_cast<int>(All.size()); }
		int NumPoints(int Lane) const { return static_cast<int>(All[Lane].Xs.size()); }
		float X(int Lane, int Point) const { return All[Lane].Xs[Point]; }
		float Y(int Lane, int Point) const { return All[Lane].Ys[Point]; }
		int NumNext(int Lane) const { return static_cast<int>(All[Lane].NextLanes.size()); }
		int Next(int Lane, int Index) const { return All[Lane].NextLanes[Index]; }
	};

	// A lane from (X0, Y0) to (X1, Y1), a point every 5 m or so.
	FLane Straight(float X0, float Y0, float X1, float Y1, std::vector<int> Next)
	{
		FLane Lane;
		const float Length = std::sqrt((X1 - X0) * (X1 - X0) + (Y1 - Y0) * (Y1 - Y0));
		const int Steps = std::max(1, static_cast<int>(Length / 5.f));
		for (int Step = 0; Step <= Steps; ++Step)
		{
			Lane.Xs.push_back(X0 + (X1 - X0) * static_cast<float>(Step) / static_cast<float>(Steps));
			Lane.Ys.push_back(Y0 + (Y1 - Y0) * static_cast<float>(Step) / static_cast<float>(Steps));
		}
		Lane.NextLanes = std::move(Next);
		return Lane;
	}

	// --- The flight, second by second -----------------------------------------------------------------------------
	// Speed of the car of the flight (km/h) and whether a bend held it ("limit=bend" of its DemoDrive lines):
	// seconds 0 to 30 from the long take of gate 93 (the kerb, the quay at 99 km/h, the corner of the rue Childebert),
	// seconds 31 to 71 from the -GTARun pass of gate 102 (rue de la Republique to the place Bellecour).
	const int FlightSeconds = 72;
	const float FlightKmh[FlightSeconds] = {0, 24, 39, 22, 21, 19, 18, 23, 25, 31, 48, 69, 81, 94, 99, 99, 99, 99, 99, 99, 99, 90, 75, 61, 46, 21, 23, 25, 38, 54, 60,
		47, 39, 41, 44, 49, 50, 50, 50, 50, 47, 34, 37, 43, 50, 50, 50, 50, 50, 50, 50, 49, 50, 50, 41, 35, 44, 54, 72, 73, 74, 72, 58, 44, 35, 42, 59, 72, 74, 74, 74, 74};
	const int FlightBend[FlightSeconds] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0,
		1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
	// The level through the flight (gate 93: one star at 2 s, two at 10 s, three at 30 s) and the speed of each on the lanes.
	int StarsAt(float T) { return T < 2.f ? 0 : (T < 10.f ? 1 : (T < 30.f ? 2 : 3)); }
	float LevelKmh(int Stars) { return GTPPoliceTactics::Profile(Stars < 1 ? 1 : Stars, 4).ChaseKmh * 0.9f; }

	struct FFlight
	{
		static constexpr float Dt = 0.05f;
		std::vector<float> Place;
		FFlight()
		{
			Place.push_back(0.f);
			for (float T = 0.f; T < static_cast<float>(FlightSeconds - 1); T += Dt)
			{
				Place.push_back(Place.back() + Speed(T) * Dt);
			}
		}
		// m/s at T.
		static float Speed(float T)
		{
			const int At = std::min(static_cast<int>(T), FlightSeconds - 2);
			const float Share = std::min(T - static_cast<float>(At), 1.f);
			return (FlightKmh[At] * (1.f - Share) + FlightKmh[At + 1] * Share) / 3.6f;
		}
		float At(float T) const { return Place[std::min(static_cast<std::size_t>(T / Dt), Place.size() - 1)]; }
		// The speed a car may have at a place of the road: Bends of what the flight had there where a bend held it,
		// no limit where the lane's own speed did.
		float Limit(float Where, float Bends) const
		{
			const std::size_t Index = static_cast<std::size_t>(std::lower_bound(Place.begin(), Place.end(), Where) - Place.begin());
			const float T = static_cast<float>(Index) * Dt;
			return FlightBend[std::min(static_cast<int>(T), FlightSeconds - 1)] ? Speed(T) * Bends : 1.e9f;
		}
	};

	struct FResult
	{
		float NearShare = 0.f;
		float MeanMetres = 0.f;
		float MeanKmh = 0.f;
	};

	// A car on the road of the flight, Behind metres behind it at From seconds, at Kmh, until the flight ends: the
	// share of that time it is within 80 m of the flight's car, how far on average, how fast. It takes the bends at
	// Bends of the speed of the flight, picks up at Accel (half of it over 80 km/h) and brakes at 4 m/s2 for what it sees.
	FResult Follow(const FFlight& Flight, float From, float Behind, float Kmh, bool bCatchUp, float Bends, float Accel)
	{
		const GTPPoliceTactics::FCatchUp CatchUp;
		float Where = Flight.At(From) - Behind;
		float Speed = Kmh / 3.6f;
		double Near = 0.0, Metres = 0.0, Speeds = 0.0, Count = 0.0;
		for (float T = From; T < static_cast<float>(FlightSeconds - 1); T += FFlight::Dt)
		{
			const int Stars = StarsAt(T);
			const float Away = Flight.At(T) - Where;
			const float Level = LevelKmh(Stars);
			float Want = (bCatchUp ? GTPPoliceTactics::CatchUpKmh(Level, FFlight::Speed(T) * 3.6f, Away, Stars, CatchUp) : Level) / 3.6f;
			for (float Look = 0.f; Look < 200.f; Look += 2.f)
			{
				const float Limit = Flight.Limit(Where + Look, Bends);
				if (Limit < 1.e8f)
				{
					Want = std::min(Want, std::sqrt(Limit * Limit + 2.f * 4.f * Look));
				}
			}
			// It does not drive through him: behind his bumper at 0.6 s.
			if (Away < 8.f + Speed * 0.6f)
			{
				Want = std::min(Want, FFlight::Speed(T));
			}
			Speed = Want > Speed ? std::min(Want, Speed + (Speed < 22.f ? Accel : Accel * 0.5f) * FFlight::Dt) : std::max(Want, Speed - 7.f * FFlight::Dt);
			Where += Speed * FFlight::Dt;
			Near += Away < 80.f ? 1.0 : 0.0;
			Metres += Away;
			Speeds += Speed * 3.6f;
			Count += 1.0;
		}
		FResult Out;
		Out.NearShare = static_cast<float>(Near / Count);
		Out.MeanMetres = static_cast<float>(Metres / Count);
		Out.MeanKmh = static_cast<float>(Speeds / Count);
		return Out;
	}
}

int main()
{
	using namespace GTPPoliceIntercept;
	// The lanes' unit is the metre here: the settings of the game, in metres.
	FSettings S;
	S.Back = 25.f;
	S.MinLength = 30.f;
	const auto NoWay = [](int) { return -1; };
	const auto Always = [](int, int) { return true; };

	// --- Where it waits -------------------------------------------------------------------------------------------
	// His road heads east, four lanes end to end, 100 m each (0, 1, 2, 3), then lane 4 that leads nowhere.
	// 5: a street from the north into lane 2 (junction at x = 200). 6: a street from the south into lane 3 (x = 300).
	// 7: another lane of his own road into lane 2, a metre beside lane 1. 8: the lane that comes the other way, turned
	// into lane 2. 9: a street from the north into lane 3, 20 m long. 13: a street that merges into lane 3 at 30 degrees. 10: a way off his road to the south from lane 1,
	// into lane 11; 12: a street from the east into lane 11.
	FLanes Town;
	Town.All.push_back(Straight(0.f, 0.f, 100.f, 0.f, {1}));
	Town.All.push_back(Straight(100.f, 0.f, 200.f, 0.f, {2, 10}));
	Town.All.push_back(Straight(200.f, 0.f, 300.f, 0.f, {3}));
	Town.All.push_back(Straight(300.f, 0.f, 400.f, 0.f, {4}));
	Town.All.push_back(Straight(400.f, 0.f, 500.f, 0.f, {}));
	Town.All.push_back(Straight(200.f, -80.f, 200.f, 0.f, {2}));
	Town.All.push_back(Straight(300.f, 90.f, 300.f, 0.f, {3}));
	Town.All.push_back(Straight(100.f, 1.f, 200.f, 1.f, {2}));
	Town.All.push_back(Straight(300.f, -3.f, 200.f, -3.f, {2}));
	Town.All.push_back(Straight(300.f, -20.f, 300.f, 0.f, {3}));
	Town.All.push_back(Straight(200.f, 0.f, 200.f, 60.f, {11}));
	Town.All.push_back(Straight(200.f, 60.f, 200.f, 160.f, {}));
	Town.All.push_back(Straight(280.f, 60.f, 200.f, 60.f, {11}));
	Town.All.push_back(Straight(230.72f, 40.f, 300.f, 0.f, {3}));
	{
		// He is 20 m along lane 0. The first side street between 80 and 300 m from him: lane 5, 25 m before its end.
		FPlace Place;
		CHECK(Find(Town, 0, 4, 20.f, 0.f, 80.f, 300.f, S, 8, NoWay, Always, Place));
		CHECK(Place.Lane == 5 && Place.Joins == 2);
		CHECK(std::fabs(Town.X(Place.Lane, Place.Point) - 200.f) < 0.1f);
		CHECK(Town.Y(Place.Lane, Place.Point) <= -25.f && Town.Y(Place.Lane, Place.Point) > -31.f);
		// His way to the junction: 180 m. The place is not at an end of its lane: a car is headed along the next point.
		CHECK(std::fabs(Place.Along - 180.f) < 0.5f);
		CHECK(Place.Point >= 1 && Place.Point <= Town.NumPoints(Place.Lane) - 3);
		CHECK(Place.Straight >= 80.f && Place.Straight <= 300.f);
	}
	{
		// A replacement is made 150 m away and more: from x = 60 the street of lane 5 is 142 m away, the next one is taken.
		FPlace Place;
		CHECK(Find(Town, 0, 12, 60.f, 0.f, 150.f, 300.f, S, 8, NoWay, Always, Place));
		CHECK(Place.Lane == 6 && Place.Joins == 3);
		CHECK(Town.Y(Place.Lane, Place.Point) >= 25.f && Town.Y(Place.Lane, Place.Point) < 31.f);
		CHECK(std::fabs(Place.Along - 240.f) < 0.5f);
	}
	{
		// Lane 5 is taken (a car stands there, or he sees it): lane 6. Neither: nothing, and never the other lane of
		// his road (7), the lane that comes the other way (8) nor the street too short to come out behind him (9).
		FPlace Place;
		std::vector<int> Asked;
		const auto NotFive = [&Asked](int Lane, int) { Asked.push_back(Lane); return Lane != 5; };
		CHECK(Find(Town, 0, 4, 20.f, 0.f, 80.f, 300.f, S, 8, NoWay, NotFive, Place) && Place.Lane == 6);
		Asked.clear();
		const auto Never = [&Asked](int Lane, int) { Asked.push_back(Lane); return false; };
		CHECK(!Find(Town, 0, 4, 20.f, 0.f, 80.f, 300.f, S, 8, NoWay, Never, Place));
		// The street that merges at 30 degrees is one too (cosine 0.87): asked after lane 6, at the same junction.
		CHECK(Asked.size() == 3 && Asked[0] == 5 && Asked[1] == 6 && Asked[2] == 13);
		// One look only: lane 5 and no more.
		Asked.clear();
		CHECK(!Find(Town, 0, 4, 20.f, 0.f, 80.f, 300.f, S, 1, NoWay, Never, Place) && Asked.size() == 1);
	}
	{
		// The flight's circuit says he turns south after lane 1: the street that leads into lane 11 then, lane 12.
		FPlace Place;
		const auto South = [](int Lane) { return Lane == 1 ? 10 : -1; };
		CHECK(Find(Town, 0, 4, 20.f, 0.f, 80.f, 300.f, S, 8, South, Always, Place));
		CHECK(Place.Lane == 12 && Place.Joins == 11);
		CHECK(Town.X(Place.Lane, Place.Point) >= 225.f && Town.X(Place.Lane, Place.Point) < 231.f);
		CHECK(std::fabs(Place.Along - 240.f) < 0.5f);
		// A way named that is no exit of his lane counts for nothing: straight on.
		const auto Wrong = [](int) { return 12; };
		CHECK(Find(Town, 0, 4, 20.f, 0.f, 80.f, 300.f, S, 8, Wrong, Always, Place) && Place.Lane == 5);
	}
	{
		// Nothing nearer than asked nor farther; nothing past the end of his road; nothing from a lane that is not one.
		FPlace Place;
		CHECK(!Find(Town, 0, 4, 20.f, 0.f, 400.f, 500.f, S, 8, NoWay, Always, Place));
		CHECK(!Find(Town, 4, 2, 410.f, 0.f, 20.f, 300.f, S, 8, NoWay, Always, Place));
		CHECK(!Find(Town, -1, 0, 0.f, 0.f, 80.f, 300.f, S, 8, NoWay, Always, Place));
		CHECK(!Find(Town, 0, 4, 20.f, 0.f, 300.f, 80.f, S, 8, NoWay, Always, Place));
		// From lane 3 on, no side street is left ahead of him.
		CHECK(!Find(Town, 3, 2, 310.f, 0.f, 20.f, 300.f, S, 8, NoWay, Always, Place));
	}

	// --- When it pulls out ----------------------------------------------------------------------------------------
	{
		const FSettings Game;
		// He is 120 m away and comes at 60 km/h: it waits. At 2.5 s of him (41.7 m): it goes.
		CHECK(Joins(120.f, 120.f, 60.f, 3.f, 3, Game) == EJoin::Waits);
		CHECK(Joins(43.f, 43.f, 60.f, 8.f, 3, Game) == EJoin::Waits);
		CHECK(Joins(41.f, 41.f, 60.f, 8.f, 3, Game) == EJoin::Comes);
		// A slow player: never before he is 27 m away, the car's own 25 m and a little.
		CHECK(Joins(29.f, 29.f, 20.f, 8.f, 3, Game) == EJoin::Waits);
		CHECK(Joins(26.f, 26.f, 20.f, 8.f, 3, Game) == EJoin::Comes);
		// He was 90 m away and is 121 m away: he turned off before its street.
		CHECK(Joins(119.f, 90.f, 60.f, 8.f, 3, Game) == EJoin::Waits);
		CHECK(Joins(121.f, 90.f, 60.f, 8.f, 3, Game) == EJoin::Passed);
		// Twenty seconds and he has not come; and a level that is gone.
		CHECK(Joins(120.f, 120.f, 60.f, 19.9f, 3, Game) == EJoin::Waits);
		CHECK(Joins(120.f, 120.f, 60.f, 20.f, 3, Game) == EJoin::Late);
		CHECK(Joins(120.f, 120.f, 60.f, 1.f, 0, Game) == EJoin::Over);

		// It comes out behind him, never in front: its street meets his road at a right angle, it stands 25 m from
		// the junction, picks up at Accel and takes the corner at 25 km/h. From 15 to 99 km/h and for a slow and a
		// brisk police car, he is past the junction when it gets there, by 0.4 s at least and 3.2 s at most.
		float Soonest = 100.f, Latest = 0.f;
		for (const float Accel : {3.5f, 5.5f, 7.f})
		{
			for (float Kmh = 15.f; Kmh <= 99.f; Kmh += 6.f)
			{
				const float His = Kmh / 3.6f;
				const float Lead = std::max(Game.JoinMinM, Game.JoinSeconds * His);
				const float ToJunction = std::sqrt(std::max(0.f, Lead * Lead - 25.f * 25.f));
				// Its 25 m: up to a peak, then down to the corner's speed at 4 m/s2.
				const float Corner = 25.f / 3.6f;
				const float Up = (Corner * Corner + 2.f * 4.f * 25.f) / (2.f * Accel + 2.f * 4.f);
				const float Peak = std::sqrt(2.f * Accel * Up);
				const float Its = Peak / Accel + (Peak - Corner) / 4.f;
				const float Margin = Its - ToJunction / His;
				Soonest = std::min(Soonest, Margin);
				Latest = std::max(Latest, Margin);
			}
		}
		std::printf("pulls out: at its junction %.1f to %.1f s after he has passed it (15 to 99 km/h, police car picking up at 3.5 to 7 m/s2)\n", Soonest, Latest);
		CHECK(Soonest >= 0.4f && Latest <= 3.2f);
	}

	// --- A unit left behind ---------------------------------------------------------------------------------------
	{
		const FLeftBehind L;
		float Seconds = 0.f;
		for (int Frame = 0; Frame < 119; ++Frame)
		{
			Seconds = LeftBehindSeconds(Seconds, 0.05f, 220.f, 60.f, 3, L);
		}
		CHECK(Seconds < L.Seconds && Seconds > 5.9f);
		CHECK(LeftBehindSeconds(Seconds, 0.05f, 220.f, 60.f, 3, L) >= L.Seconds);
		// Not while it is near, not behind a player who hardly moves, not at one star.
		CHECK(LeftBehindSeconds(Seconds, 0.05f, 170.f, 60.f, 3, L) == 0.f);
		CHECK(LeftBehindSeconds(Seconds, 0.05f, 220.f, 20.f, 3, L) == 0.f);
		CHECK(LeftBehindSeconds(Seconds, 0.05f, 220.f, 60.f, 1, L) == 0.f);
		// Taken away after 6 s of it, while another unit drives after him, none waits, and not a unit just made.
		CHECK(TakenAway(6.f, 30.f, 1, 0, L) && TakenAway(9.f, 12.f, 2, 0, L));
		CHECK(!TakenAway(5.9f, 30.f, 1, 0, L) && !TakenAway(6.f, 11.f, 1, 0, L) && !TakenAway(6.f, 30.f, 0, 0, L) && !TakenAway(6.f, 30.f, 1, 1, L));
	}

	// --- The flight of gates 93 and 102, replayed --------------------------------------------------------------------
	{
		const FFlight Flight;
		// A car made 100 m behind him 3 s after he took the wheel, rolling at his speed, that takes the bends as he
		// does or a little slower: the gates measured 5 to 10 % of the time within 80 m and a nearest unit 146 to
		// 169 m away. The replay gives the same: the model is not kinder than the game.
		const FResult Slower = Follow(Flight, 3.f, 100.f, 22.f, false, 0.88f, 5.5f);
		const FResult Same = Follow(Flight, 3.f, 100.f, 22.f, false, 1.f, 5.5f);
		std::printf("follows from 100 m behind, bends at 0.88 of his: within 80 m %.0f %% of the time, %.0f m away, %.0f km/h on average\n",
			Slower.NearShare * 100.f, Slower.MeanMetres, Slower.MeanKmh);
		std::printf("follows from 100 m behind, bends as fast as his:  within 80 m %.0f %% of the time, %.0f m away, %.0f km/h on average\n",
			Same.NearShare * 100.f, Same.MeanMetres, Same.MeanKmh);
		CHECK(Slower.NearShare < 0.15f && Slower.MeanMetres > 120.f && Slower.MeanMetres < 180.f);
		// A replacement is made 150 m behind him: it never comes within 80 m, whenever it is made and however fast it
		// may drive on the straights. Seconds behind do not turn into metres made up between two corners.
		for (const float From : {3.f, 12.f, 40.f})
		{
			const FResult Before = Follow(Flight, From, 150.f, 50.f, false, 0.88f, 5.5f);
			const FResult After = Follow(Flight, From, 150.f, 50.f, true, 0.88f, 5.5f);
			std::printf("replacement 150 m behind at %2.0f s: within 80 m %.0f %% at its level's speed, %.0f %% with the catch-up speed (%.0f m then %.0f m away)\n",
				From, Before.NearShare * 100.f, After.NearShare * 100.f, Before.MeanMetres, After.MeanMetres);
			CHECK(Before.NearShare < 0.05f && After.NearShare < 0.05f);
			// The catch-up speed never makes it worse.
			CHECK(After.MeanMetres <= Before.MeanMetres + 0.5f);
		}
		// With the catch-up speed the first car, made 100 m behind, is nearer on average.
		const FResult Faster = Follow(Flight, 3.f, 100.f, 22.f, true, 0.88f, 5.5f);
		std::printf("follows from 100 m behind with the catch-up speed: within 80 m %.0f %% of the time, %.0f m away\n", Faster.NearShare * 100.f, Faster.MeanMetres);
		CHECK(Faster.MeanMetres < Slower.MeanMetres - 10.f && Faster.NearShare >= Slower.NearShare);
		// A car that pulls out of a side street as he passes: from a standstill, 25 to 45 m behind him. Wherever on
		// the circuit past the quay, it is within 80 m of him 60 % of the time and more until the flight ends.
		float Worst = 1.f;
		for (const float From : {22.f, 32.f, 36.f, 44.f, 52.f, 58.f})
		{
			for (const float Behind : {25.f, 45.f})
			{
				const FResult Out = Follow(Flight, From, Behind, 0.f, true, 0.88f, 5.5f);
				Worst = std::min(Worst, Out.NearShare);
			}
		}
		std::printf("pulls out 25 to 45 m behind him between 22 and 58 s: within 80 m %.0f %% of the time at worst\n", Worst * 100.f);
		CHECK(Worst >= 0.6f);
	}

	std::printf(Failures == 0 ? "PASS police intercept\n" : "%d FAILURE(S)\n", Failures);
	return Failures == 0 ? 0 : 1;
}
