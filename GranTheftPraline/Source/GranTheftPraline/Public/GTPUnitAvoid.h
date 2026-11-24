#pragma once

// How a police car, and an officer on foot, get round what stands in their way, written without engine types so
// it can be tested outside Unreal (tools/vehicles/wanted_sim/unit_avoid_test.cpp). Units: seconds, metres, m/s.
//
// Gate of candidate 75 (2026-11-24): the chasing units averaged 18 km/h and moved 62 % of their time. Their
// lines showed nine cars held "with the throttle down" in 68 s, eight of them with the nose against the street's
// furniture (lamp posts, lights, trees: instanced static meshes), on their lane as often as off it. A unit only
// looked ahead for cars and people; in a chase at three stars and more it did not look at all. And the one
// officer on foot stood 36 m from the stopped player for 25 s: he walks straight at his goal.
//
// So: a unit looks ahead for scenery (what does not move), steers round it on the side that is free, and slows
// to what it can stop from. An officer follows the wall he meets, the same hand on it, until his way is free.

#include <algorithm>
#include <cmath>

struct FGTPUnitAvoidSettings
{
	/** Scenery is looked for this far ahead: the speed for LookSeconds, and no less than the car needs to stop,
	 *  between LookMinM and LookMaxM. */
	float LookSeconds = 0.9f;
	float LookMinM = 5.f;
	float LookMaxM = 30.f;
	/** The look leans into the turn: this many radians at full lock (a car in a bend looks where it goes). */
	float LeanRadians = 0.35f;
	/** The two ways round are looked at this far (radians) either side of the look. */
	float SideRadians = 0.6f;
	/** The car is taken to lose this much speed (m/s2, its brakes give 9) and stops this far (m) from what it cannot get round. */
	float Decel = 6.f;
	float MarginM = 1.5f;
	/** A side chosen is kept this long (s): no change of mind half way round. */
	float HoldSeconds = 1.5f;
	/** The ball pushed along the look (m): narrower than the car (1.08 m to its side) while nothing is in the way, so
	 *  what stands beside the lane is left alone; wider than it while the car goes round something, until it is clear. */
	float LookRadiusM = 0.8f;
	float RoundRadiusM = 1.35f;
	/** The least steering round something, far away; full lock when it is at the bumper. */
	float MinSteer = 0.4f;
	/** A way round counts when it is free this much further (m) than the way ahead. */
	float BetterByM = 1.f;
};

struct FGTPUnitAvoid
{
	/** The side taken round what is ahead: -1 left, +1 right, 0 none. */
	int Side = 0;
	float HoldLeft = 0.f;
	/** Times a way round was taken, and times there was none (the car stood). */
	int Rounds = 0;
	int Blocked = 0;
	/** The last look (m, negative: nothing). */
	float LastAheadM = -1.f;
	bool bStood = false;

	static float Clamp(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }

	/** The radius (m) of the ball to push along the look this frame. */
	float LookRadius(const FGTPUnitAvoidSettings& S) const { return Side != 0 ? S.RoundRadiusM : S.LookRadiusM; }

	/** Metres looked ahead at Speed (m/s). */
	static float LookMetres(float Speed, const FGTPUnitAvoidSettings& S)
	{
		const float V = std::fabs(Speed);
		const float ToStop = V * V / (2.f * std::max(S.Decel, 0.1f)) + S.MarginM + 2.f;
		return Clamp(std::max(V * S.LookSeconds, ToStop), S.LookMinM, S.LookMaxM);
	}

	/**
	 * One frame. AheadM: metres from the bumper to the scenery on the car's way (negative: none within LookM).
	 * LateralM: where it was met, metres to the right of the line looked along (negative: to the left).
	 * LeftM, RightM: the same look on the two ways round (negative: free), only read when AheadM is not negative.
	 * The pedals and the steering of the driving come in and go out changed.
	 */
	void Step(float DeltaSeconds, float Speed, float LookM, float AheadM, float LateralM, float LeftM, float RightM,
		const FGTPUnitAvoidSettings& S, float& Throttle, float& Brake, float& Steering)
	{
		LastAheadM = AheadM;
		HoldLeft = std::max(0.f, HoldLeft - DeltaSeconds);
		if (AheadM < 0.f)
		{
			bStood = false;
			if (HoldLeft <= 0.f)
			{
				Side = 0;
			}
			else if (Side > 0)
			{
				// Just round it: the wheel does not turn back into it before the tail is past.
				Steering = std::max(Steering, 0.f);
			}
			else if (Side < 0)
			{
				Steering = std::min(Steering, 0.f);
			}
			return;
		}
		const float V = std::fabs(Speed);
		if (Side == 0 || HoldLeft <= 0.f)
		{
			const float Open = LookM * 2.f;
			const float FreeLeft = LeftM < 0.f ? Open : LeftM;
			const float FreeRight = RightM < 0.f ? Open : RightM;
			const int Before = Side;
			if (FreeLeft > AheadM + S.BetterByM || FreeRight > AheadM + S.BetterByM)
			{
				if (std::fabs(FreeLeft - FreeRight) < S.BetterByM)
				{
					// As free either side: away from where it was met, or the side the driving turns to already.
					Side = std::fabs(LateralM) > 0.1f ? (LateralM > 0.f ? -1 : 1) : (Steering < 0.f ? -1 : 1);
				}
				else
				{
					Side = FreeRight > FreeLeft ? 1 : -1;
				}
				HoldLeft = S.HoldSeconds;
				Rounds += Side != Before ? 1 : 0;
			}
			else
			{
				Side = 0;
			}
		}
		const float Near = Clamp(1.3f - AheadM / std::max(LookM, 1.f), S.MinSteer, 1.f);
		if (Side > 0)
		{
			Steering = std::max(Steering, Near);
		}
		else if (Side < 0)
		{
			Steering = std::min(Steering, -Near);
		}
		// With a way round the car only has to turn; with none it has to stop short.
		const float Margin = Side != 0 ? S.MarginM * 0.5f : S.MarginM;
		const float StopFrom = std::sqrt(2.f * S.Decel * std::max(AheadM - Margin, 0.f));
		if (V > StopFrom + 0.3f)
		{
			Throttle = 0.f;
			Brake = std::max(Brake, Clamp(V - StopFrom, 0.2f, 1.f));
			bStood = false;
		}
		else if (Side == 0 && AheadM < Margin)
		{
			// Nowhere to go: the car stands (held like this, its recovery backs it off and it looks again).
			Throttle = 0.f;
			Brake = 1.f;
			Blocked += bStood ? 0 : 1;
			bStood = true;
		}
		else
		{
			Throttle = std::min(Throttle, Clamp((StopFrom - V) * 0.5f + 0.25f, 0.25f, 1.f));
			bStood = false;
		}
	}
};

// An officer on foot: he walks at his goal; when something stands in the way he follows it, the same hand on
// it, until the way to the goal is free; when he has not moved for a while he tries the other hand.
struct FGTPFootDetour
{
	/** The hand kept on the wall: -1, +1, 0 when walking free. */
	int Hand = 0;
	float HandLeft = 0.f;
	/** Where he was at the last look at his progress, and the time since. */
	float MarkX = 0.f;
	float MarkY = 0.f;
	float MarkTime = 0.f;
	bool bHasMark = false;
	int Detours = 0;
	int Flips = 0;

	/**
	 * The direction to walk (OutX, OutY, unit length). (WantX, WantY): unit vector to the goal. bBlocked: something
	 * within a step that way, its face turned (NormalX, NormalY) towards him. bAlongBlocked, (Normal2X, Normal2Y): the
	 * same along the wall, looked at by the caller with Along() when bBlocked. (X, Y): where he is.
	 */
	void Step(float DeltaSeconds, float X, float Y, float WantX, float WantY, bool bBlocked, float NormalX, float NormalY,
		bool bAlongBlocked, float Normal2X, float Normal2Y, float& OutX, float& OutY)
	{
		HandLeft = std::max(0.f, HandLeft - DeltaSeconds);
		// Progress: under 0.6 m in 2 s while something is in the way, the other hand, kept for a while.
		if (!bHasMark)
		{
			bHasMark = true;
			MarkX = X;
			MarkY = Y;
			MarkTime = 0.f;
		}
		MarkTime += DeltaSeconds;
		bool bFlipped = false;
		if (MarkTime >= 2.f)
		{
			if (bBlocked && Hand != 0 && std::hypot(X - MarkX, Y - MarkY) < 0.6f)
			{
				Hand = -Hand;
				HandLeft = 4.f;
				++Flips;
				bFlipped = true;
			}
			MarkX = X;
			MarkY = Y;
			MarkTime = 0.f;
		}
		if (!bBlocked)
		{
			if (HandLeft <= 0.f)
			{
				Hand = 0;
			}
			OutX = WantX;
			OutY = WantY;
			return;
		}
		if (Hand == 0)
		{
			// The way along the wall that goes more towards the goal.
			Hand = (-NormalY * WantX + NormalX * WantY) >= 0.f ? 1 : -1;
			++Detours;
		}
		// The hand is kept 3 s after the wall is left: the next face of the same building is followed the same way.
		HandLeft = std::max(HandLeft, 3.f);
		// What the caller saw along the wall was for the hand he had: not after a change of hand.
		const bool bSecondWall = bAlongBlocked && !bFlipped;
		AlongWall(Hand, bSecondWall ? Normal2X : NormalX, bSecondWall ? Normal2Y : NormalY, OutX, OutY);
	}

	/** Along a wall whose face is turned (NormalX, NormalY), the hand given on it, a little away from it. */
	static void AlongWall(int InHand, float NormalX, float NormalY, float& OutX, float& OutY)
	{
		const float Side = InHand >= 0 ? 1.f : -1.f;
		float Dx = -NormalY * Side + NormalX * 0.2f;
		float Dy = NormalX * Side + NormalY * 0.2f;
		const float Length = std::hypot(Dx, Dy);
		if (Length > 1.e-4f)
		{
			Dx /= Length;
			Dy /= Length;
		}
		OutX = Dx;
		OutY = Dy;
	}

	/** The direction along the wall for the hand he would take: what the caller looks along before Step when blocked. */
	void Along(float WantX, float WantY, float NormalX, float NormalY, float& OutX, float& OutY) const
	{
		const int Would = Hand != 0 ? Hand : ((-NormalY * WantX + NormalX * WantY) >= 0.f ? 1 : -1);
		AlongWall(Would, NormalX, NormalY, OutX, OutY);
	}
};
