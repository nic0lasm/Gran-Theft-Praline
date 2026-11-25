// What a car does with the steering it is given, and what the drivers that are not a hand on a stick give it
// (GTPSteeringCurve.h, GTPFlightPedals::SteeringSent).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/steering_input_test.cpp -o /tmp/steering_input_test && /tmp/steering_input_test
//
// Gate 93's long take is replayed (gate93_lines.h): the car is put where the film's log says it was, driven at the
// speeds the log gives, steered by the game's lane follower (GTPDrive::Decide) on the line it followed, and its
// place is set against the log's second after second. The car is a bicycle model: 2.97 m between the axles, 45
// degrees of lock, a steering input that moves at 2.5 a second towards more lock and 5 a second back (the rates of
// a Chaos vehicle), and front wheels that get what the model under test says of that input.
//
// What it shows:
// - wheels that get the square of the input times the share of the speed (GTPSteeringCurve::WheelsGet) put the car
//   where the game's car was, to 0.6 m over 24 seconds on the quay at 99 km/h and to 0.3 m over 10 seconds into the
//   rue du President Carnot;
// - wheels that get the input times the share of the speed do not: that car stays on its line where the game's car
//   was 2.2 and 4.4 m off it;
// - with the steering the flight now sends (GTPFlightPedals::SteeringSent) the first car holds the line.
// What it does not show: tyres. The model does not slide, and whether the game's car holds the line at 99 km/h
// with wheels that answer seven times more is for the gate to say (-DemoSteerSquared: as before).
#include "circuit_corners.h"
#include "gate93_lines.h"

namespace
{
	enum class EWheels
	{
		SquareAndShare,   // GTPSteeringCurve::WheelsGet: what the game's car does
		ShareOnly,        // what was believed until gate 93
	};

	enum class ESent
	{
		Gate93,   // GTPFlightPedals::SteeringShareMadeUp
		Now,      // GTPFlightPedals::SteeringSent
	};

	struct FReplay
	{
		float OffMost = 0.f;        // the furthest from the line, m, at the log's seconds
		float OffMostSettled = 0.f; // the same from the second of the log's seconds on: the car starts where the game's was, off its line
		float OffMean = 0.f;
		float OffErrorMost = 0.f;   // against the log's: metres from the line, steering asked, yaw
		float SteerErrorMost = 0.f;
		float YawErrorMost = 0.f;
		float LogOffMost = 0.f;
	};

	float Moved(float Input, float Target, float Dt)
	{
		const bool bMoreLock = std::fabs(Target) > std::fabs(Input) && Target * Input >= 0.f;
		const float Rate = (bMoreLock ? 2.5f : 5.f) * Dt;
		return Input + std::max(-Rate, std::min(Rate, Target - Input));
	}

	float SpeedAt(const std::vector<Gate93::FSecond>& Log, float Film)
	{
		for (size_t Index = 0; Index + 1 < Log.size(); ++Index)
		{
			if (Film <= Log[Index + 1].Film)
			{
				const float T = (Film - Log[Index].Film) / (Log[Index + 1].Film - Log[Index].Film);
				return (Log[Index].Kmh + (Log[Index + 1].Kmh - Log[Index].Kmh) * std::max(0.f, std::min(1.f, T))) / 3.6f;
			}
		}
		return Log.back().Kmh / 3.6f;
	}

	FReplay Replay(const std::vector<FGTPDrivePoint>& Line, const std::vector<Gate93::FSecond>& Log, EWheels Wheels, ESent Sent, bool bTrace)
	{
		FGTPDriveSettings Settings;
		GTPDrive::SetChaseDriving(Settings);
		float X = Log[0].X, Y = Log[0].Y, Yaw = Log[0].Yaw * 3.14159265f / 180.f;
		int Closest = 0;
		{
			float Best = 1e30f;
			for (size_t Index = 0; Index < Line.size(); ++Index)
			{
				const float D = std::hypot(Line[Index].X - X, Line[Index].Y - Y);
				if (D < Best)
				{
					Best = D;
					Closest = static_cast<int>(Index);
				}
			}
		}
		float Input = Log[0].Sent;
		float Asked = Log[0].Steer;
		size_t Next = 1;
		FReplay Out;
		int Seconds = 0;
		const float Dt = 1.f / 60.f;
		for (float Film = Log[0].Film; Next < Log.size(); Film += Dt)
		{
			const float V = SpeedAt(Log, Film);
			const float Kmh = V * 3.6f;
			Asked = GTPDrive::Decide(Line, X, Y, Yaw, V, V, -1.f, Settings, Closest).Steering;
			const float Target = Sent == ESent::Gate93 ? GTPFlightPedals::SteeringShareMadeUp(Asked, Kmh) : GTPFlightPedals::SteeringSent(Asked, Kmh);
			Input = Moved(Input, Target, Dt);
			const float Lock = Wheels == EWheels::SquareAndShare ? GTPSteeringCurve::WheelsGet(Input, Kmh) : Input * GTPSteeringCurve::ShareAtKmh(Kmh);
			Yaw += V * std::tan(Lock * Settings.MaxSteerAngle) / Settings.WheelBase * Dt;
			X += V * std::cos(Yaw) * Dt;
			Y += V * std::sin(Yaw) * Dt;
			if (Film + Dt >= Log[Next].Film)
			{
				const Gate93::FSecond& Row = Log[Next];
				const float Off = Stray(Line, X, Y);
				const float YawError = std::fabs(GTPDrive::WrapAngle(Yaw - Row.Yaw * 3.14159265f / 180.f)) * 180.f / 3.14159265f;
				Out.OffMost = std::max(Out.OffMost, Off);
				Out.OffMostSettled = Next >= 2 ? std::max(Out.OffMostSettled, Off) : Out.OffMostSettled;
				Out.OffMean += Off;
				Out.LogOffMost = std::max(Out.LogOffMost, Row.Off);
				Out.OffErrorMost = std::max(Out.OffErrorMost, std::fabs(Off - Row.Off));
				Out.SteerErrorMost = std::max(Out.SteerErrorMost, std::fabs(Asked - Row.Steer));
				Out.YawErrorMost = std::max(Out.YawErrorMost, YawError);
				++Seconds;
				if (bTrace)
				{
					std::printf("    film %.1f s, %.0f km/h: %.1f m off the line (the game's car %.1f), steering asked %+.2f (%+.2f), sent %+.2f, wheels %+.3f of lock, yaw %.0f (%.0f)\n",
						static_cast<double>(Row.Film), static_cast<double>(Row.Kmh), static_cast<double>(Off), static_cast<double>(Row.Off), static_cast<double>(Asked), static_cast<double>(Row.Steer),
						static_cast<double>(Input), static_cast<double>(Lock), static_cast<double>(Yaw * 180.f / 3.14159265f), static_cast<double>(Row.Yaw));
				}
				++Next;
			}
		}
		Out.OffMean /= static_cast<float>(std::max(Seconds, 1));
		return Out;
	}

	/** A straight line driven at Kmh from Beside metres off it: the seconds until the car stays within 0.5 m, and how far it goes past. */
	void BackToLine(float Kmh, float Beside, float& OutSeconds, float& OutPast)
	{
		FGTPDriveSettings Settings;
		GTPDrive::SetChaseDriving(Settings);
		std::vector<FGTPDrivePoint> Line;
		for (int Index = 0; Index < 400; ++Index)
		{
			Line.push_back({static_cast<float>(Index) * 3.f, 0.f});
		}
		float X = 6.f, Y = Beside, Yaw = 0.f, Input = 0.f;
		int Closest = 0;
		const float V = Kmh / 3.6f, Dt = 1.f / 60.f;
		OutSeconds = 0.f;
		OutPast = 0.f;
		for (float T = 0.f; T < 10.f; T += Dt)
		{
			const float Asked = GTPDrive::Decide(Line, X, Y, Yaw, V, V, -1.f, Settings, Closest).Steering;
			Input = Moved(Input, GTPFlightPedals::SteeringSent(Asked, Kmh), Dt);
			Yaw += V * std::tan(GTPSteeringCurve::WheelsGet(Input, Kmh) * Settings.MaxSteerAngle) / Settings.WheelBase * Dt;
			X += V * std::cos(Yaw) * Dt;
			Y += V * std::sin(Yaw) * Dt;
			if (std::fabs(Y) > 0.5f)
			{
				OutSeconds = T + Dt;
			}
			OutPast = std::max(OutPast, -Y * (Beside > 0.f ? 1.f : -1.f));
		}
	}
}

int main(int ArgCount, char**)
{
	const bool bTrace = ArgCount > 1;
	using namespace GTPSteeringCurve;

	// The car: the square of what it is given, its sign kept, times the share of its speed.
	CHECK(Squared(0.5f) == 0.25f && Squared(-0.5f) == -0.25f && Squared(1.f) == 1.f && Squared(0.f) == 0.f);
	CHECK(std::fabs(WheelsGet(0.1f, 0.f) - 0.01f) < 1e-6f && std::fabs(WheelsGet(-0.1f, 0.f) + 0.01f) < 1e-6f);
	CHECK(std::fabs(WheelsGet(1.f, 100.f) - ShareAtKmh(100.f)) < 1e-6f && WheelsGet(3.f, 0.f) == 1.f);
	// Gate 93, the quay at 99 km/h: 0.07 asked, 0.15 sent, 0.010 of lock at the wheels, a seventh of what was asked.
	CHECK(std::fabs(GTPFlightPedals::SteeringShareMadeUp(0.07f, 99.f) - 0.15f) < 0.005f);
	CHECK(std::fabs(WheelsGet(GTPFlightPedals::SteeringShareMadeUp(0.07f, 99.f), 99.f) - 0.0105f) < 0.001f);

	// The square undone: what is asked is what the car makes of it, either side, at every size.
	for (float Asked = -1.f; Asked <= 1.f; Asked += 0.01f)
	{
		CHECK(std::fabs(Squared(InputFor(Asked)) - Asked) < 1e-5f);
		CHECK(InputFor(-Asked) == -InputFor(Asked));
		CHECK(std::fabs(InputFor(Asked)) >= std::fabs(Asked) - 1e-6f && std::fabs(InputFor(Asked)) <= 1.f);
		CHECK(InputFor(Asked + 0.01f) >= InputFor(Asked));
	}
	CHECK(InputFor(0.f) == 0.f && InputFor(1.f) == 1.f && InputFor(-1.f) == -1.f && InputFor(4.f) == 1.f && InputFor(-4.f) == -1.f);
	CHECK(std::fabs(InputFor(0.09f) - 0.3f) < 1e-5f && std::fabs(InputFor(0.25f) - 0.5f) < 1e-6f);
	// The police units and the traffic: the wheels get what was asked times the share of the speed, no more.
	for (const float Kmh : {0.f, 30.f, 50.f, 80.f, 105.f})
	{
		CHECK(std::fabs(WheelsGet(InputFor(0.3f), Kmh) - 0.3f * ShareAtKmh(Kmh)) < 1e-5f);
	}

	// The flight: the wheels get what was asked, at every speed, up to what the car gives at that speed.
	for (float Kmh = 0.f; Kmh <= 130.f; Kmh += 5.f)
	{
		for (float Asked = -1.f; Asked <= 1.f; Asked += 0.05f)
		{
			const float Sent = GTPFlightPedals::SteeringSent(Asked, Kmh);
			const float Got = WheelsGet(Sent, Kmh);
			CHECK(std::fabs(Sent) <= 1.f);
			if (std::fabs(Asked) <= ShareAtKmh(Kmh))
			{
				CHECK(std::fabs(Got - Asked) < 1e-4f);
			}
			else
			{
				CHECK(std::fabs(Sent) == 1.f && Sent * Asked > 0.f);
			}
			CHECK(GTPFlightPedals::SteeringSent(-Asked, Kmh) == -Sent && GTPFlightPedals::SteeringSent(Asked, -Kmh) == Sent);
		}
	}
	// The quay again: 0.07 asked at 99 km/h now goes as 0.39 and is 0.07 at the wheels.
	CHECK(std::fabs(GTPFlightPedals::SteeringSent(0.07f, 99.f) - 0.39f) < 0.01f);
	CHECK(std::fabs(WheelsGet(GTPFlightPedals::SteeringSent(0.07f, 99.f), 99.f) - 0.07f) < 1e-4f);

	// Gate 93 replayed.
	struct FStretch
	{
		const char* Name;
		const std::vector<FGTPDrivePoint>& Line;
		const std::vector<Gate93::FSecond>& Log;
	};
	const FStretch Stretches[] = {{"quay Jules Courmont and rue Childebert", Gate93::QuayLine, Gate93::Quay}, {"rue Grolee and rue du President Carnot", Gate93::CarnotLine, Gate93::Carnot}};
	for (const FStretch& Stretch : Stretches)
	{
		const FReplay Game = Replay(Stretch.Line, Stretch.Log, EWheels::SquareAndShare, ESent::Gate93, bTrace);
		const FReplay Believed = Replay(Stretch.Line, Stretch.Log, EWheels::ShareOnly, ESent::Gate93, false);
		const FReplay Now = Replay(Stretch.Line, Stretch.Log, EWheels::SquareAndShare, ESent::Now, bTrace);
		std::printf("%s, %d s of gate 93: the game's car up to %.1f m off its line. Wheels that get the square of the steering sent: %.1f m (never more than %.2f m from where the game's car was, "
			"%.2f of steering, %.0f degrees). Wheels that get the steering sent: %.1f m (up to %.1f m from the game's). With the steering sent now: %.1f m after the first second, %.2f m on average (%.2f m before).\n",
			Stretch.Name, static_cast<int>(Stretch.Log.size()) - 1, static_cast<double>(Game.LogOffMost), static_cast<double>(Game.OffMost), static_cast<double>(Game.OffErrorMost),
			static_cast<double>(Game.SteerErrorMost), static_cast<double>(Game.YawErrorMost), static_cast<double>(Believed.OffMost), static_cast<double>(Believed.OffErrorMost),
			static_cast<double>(Now.OffMostSettled), static_cast<double>(Now.OffMean), static_cast<double>(Game.OffMean));
		// The square is what the game's car does: its place second after second, its steering, where it points.
		CHECK(Game.OffErrorMost < 0.8f);
		CHECK(Game.SteerErrorMost < 0.08f);
		CHECK(Game.YawErrorMost < 6.f);
		CHECK(std::fabs(Game.OffMost - Game.LogOffMost) < 0.5f);
		// Without it the model's car is not the game's: on its line where the game's was metres off it.
		CHECK(Believed.OffMost < Game.LogOffMost - 1.f);
		CHECK(Believed.OffErrorMost > 1.5f);
		// The steering sent now: back from where the stretch starts (1.7 and 0.8 m off the line) in a second, then the
		// line is held, corners included.
		CHECK(Now.OffMost < 1.f);
		CHECK(Now.OffMostSettled < 0.5f);
		CHECK(Now.OffMean < 0.35f && Now.OffMean < Game.OffMean * 0.3f);
	}

	// Put 2 m beside a straight line, the flight's car is back within half a metre of it in under two seconds and
	// does not go a metre past it: at town speed, at the quay's.
	for (const float Kmh : {30.f, 50.f, 75.f, 99.f})
	{
		for (const float Beside : {2.f, -2.f})
		{
			float Seconds = 0.f, Past = 0.f;
			BackToLine(Kmh, Beside, Seconds, Past);
			if (Beside > 0.f)
			{
				std::printf("2 m beside a straight line at %.0f km/h: within 0.5 m of it for good after %.1f s, %.2f m past it at most\n", static_cast<double>(Kmh), static_cast<double>(Seconds), static_cast<double>(Past));
			}
			CHECK(Seconds < 2.f);
			CHECK(Past < 1.f);
		}
	}

	// The three corners where gate 83's car left its lane, with the script's own pedals: the game's car as the
	// replay above found it, with the steering of gate 93 and with the steering sent now.
	for (const FCorner& Corner : Corners())
	{
		const FRun Before = Drive(Corner, 0, true, ELeave::KeepEnd, false, ECar::Gate93);
		const FRun After = Drive(Corner, 0, true, ELeave::KeepEnd, bTrace, ECar::Game);
		const FRun Asked = Drive(Corner, 0, true, ELeave::KeepEnd, false, ECar::Model);
		std::printf("%s: with the steering of gate 93 %.1f m off the line in %.1f s; with the steering sent now %.1f m in %.1f s (%.0f km/h at most with over half of the lock on the wheels); "
			"a car whose wheels get the steering asked %.1f m in %.1f s\n", Corner.Name, static_cast<double>(Before.Off), static_cast<double>(Before.Seconds), static_cast<double>(After.Off),
			static_cast<double>(After.Seconds), static_cast<double>(After.TurnedKmh), static_cast<double>(Asked.Off), static_cast<double>(Asked.Seconds));
		CHECK(Before.bArrived && After.bArrived && Asked.bArrived);
		// Nearer the line and no slower than with the steering of gate 93. What is left is the corner the lane follower
		// cuts (the point it aims at is ahead of the car): the same as for a car whose wheels get the steering asked.
		CHECK(After.Off < Before.Off - 0.4f);
		CHECK(std::fabs(After.Off - Asked.Off) < 0.3f);
		CHECK(After.Seconds < Before.Seconds + 0.5f && std::fabs(After.Seconds - Asked.Seconds) < 0.5f);
		CHECK(After.ThrottleTurned == 0.f);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
