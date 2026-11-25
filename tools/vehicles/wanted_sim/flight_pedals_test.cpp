// The chase script's car through the three corners of the Bellecour circuit where gate 83's car left its lane,
// driven the way the game drives them: the lane follower drops the points of a lane the moment the car's closest
// point is on the next one (GTPFlightPedals.h).
//   g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/vehicles/wanted_sim/flight_pedals_test.cpp -o /tmp/flight_pedals_test && /tmp/flight_pedals_test
//
// The lanes and the car: circuit_corners.h.
//
// What it shows: the speed wanted jumps as a lane is left in a corner, the throttle then comes with the wheels
// turned, and the rule takes that throttle away at little cost in time. What it does not show: the car of the
// model takes these corners either way, where the game's left its lane by 13 to 27 m. So the rule is also
// checked on the very lines of gate 83, and whether the game's car now holds these corners is for the gate to say.
// The lane is dropped whole here, as the lane follower did at gate 83 (lane_change_corner_test.cpp: what it does since).
#include "circuit_corners.h"

int main(int ArgCount, char**)
{
	const bool bTrace = ArgCount > 1;
	float ThrottleTurnedBefore = 0.f;
	int Jumps = 0;
	for (const FCorner& Corner : Corners())
	{
		const int Points = static_cast<int>(Corner.Lanes[0].Line.size());
		// From the start of the lane before the corner, from its middle, and from 4 points (about 11 m) before its end.
		for (const int From : {0, Points / 2, Points - 5})
		{
			const FRun Before = Drive(Corner, From, false, ELeave::Drop, bTrace);
			const FRun After = Drive(Corner, From, true, ELeave::Drop, bTrace);
			std::printf("%s, from point %d of %d: pedals alone %.1f m off the line (%.0f km/h with the wheels turned, %.1f s of throttle past what they hold)%s; following the steering %.1f m (%.0f km/h), %.1f s against %.1f s\n",
				Corner.Name, From, Points, static_cast<double>(Before.Off), static_cast<double>(Before.TurnedKmh), static_cast<double>(Before.ThrottleTurned), Before.bWantJumped ? ", speed wanted jumped" : "",
				static_cast<double>(After.Off), static_cast<double>(After.TurnedKmh), static_cast<double>(After.Seconds), static_cast<double>(Before.Seconds));
			ThrottleTurnedBefore += Before.ThrottleTurned;
			Jumps += Before.bWantJumped ? 1 : 0;
			CHECK(Before.bArrived && After.bArrived);
			// Never the throttle past what the wheels hold, no further from the line, and a second lost at most.
			CHECK(After.ThrottleTurned == 0.f);
			CHECK(After.Off < Before.Off + 0.2f);
			CHECK(After.TurnedKmh < 30.f);
			CHECK(After.Seconds < Before.Seconds + 1.f);
		}
	}
	// What gate 83 showed and the follower does: the speed wanted jumps as the lane is left, and the throttle comes
	// with the wheels turned.
	CHECK(Jumps >= 6);
	CHECK(ThrottleTurnedBefore > 1.f);
	{
		FGTPDriveSettings Settings;
		GTPDrive::SetChaseDriving(Settings);
		// A straight is not slowed: the firm throttle with the wheels straight, the whole of it when pulling away,
		// wheels turned or not.
		float Throttle = 0.2f, Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 0.02f, 20.f, 27.8f, Settings);
		CHECK(Throttle == 0.7f && Brake == 0.f);
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 1.f, 0.f, 27.8f, Settings);
		CHECK(Throttle == 1.f && Brake == 0.f);
		// The lines of gate 83. The quay at 98 km/h with 0.14 of lock asked: left alone.
		Throttle = 0.52f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, -0.14f, 98.f / 3.6f, 27.8f, Settings);
		CHECK(Throttle == 0.52f && Brake == 0.f);
		// Rue Childebert, kmh=36 throttle=1.00 steer=1.00, then kmh=52: no throttle, the brake.
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 1.f, 36.f / 3.6f, 16.7f, Settings);
		CHECK(Throttle == 0.f && Brake == 0.8f);
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 1.f, 52.f / 3.6f, 16.7f, Settings);
		CHECK(Throttle == 0.f && Brake == 0.8f);
		// Place de la Republique, kmh=37 throttle=1.00 steer=-0.41: the same.
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, -0.41f, 37.f / 3.6f, 12.5f, Settings);
		CHECK(Throttle == 0.f && Brake > 0.5f);
		// Under the speed held the throttle tapers: rue Jean de Tournes, 0.39 of lock, here at 20 km/h for the 27 held.
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 0.39f, 20.f / 3.6f, 16.7f, Settings);
		CHECK(Throttle > 0.7f && Throttle <= 1.f && Brake == 0.f);
		// The brake asked for an obstacle is never eased.
		Throttle = 0.f; Brake = 1.f;
		GTPFlightPedals::Apply(Throttle, Brake, 0.52f, 31.f / 3.6f, 20.8f, Settings);
		CHECK(Throttle == 0.f && Brake == 1.f);
		// Full lock is held at 15 km/h, a third of lock at 28.
		CHECK(std::fabs(GTPFlightPedals::HoldSpeed(1.f, Settings) * 3.6f - 15.2f) < 0.5f);
		CHECK(std::fabs(GTPFlightPedals::HoldSpeed(0.35f, Settings) * 3.6f - 28.3f) < 0.5f);
		// People ahead (gate 86). 98 km/h, a person at 0 m, or 9 m ahead at the end of the probe: no brake, the line is kept.
		CHECK(!GTPFlightPedals::StopsForPerson(0.f, 98.f / 3.6f));
		CHECK(!GTPFlightPedals::StopsForPerson(9.f, 98.f / 3.6f));
		// 36 km/h and a person 9 m ahead: 7.1 m to stop and 1 m of room, it brakes. 6 m ahead: it does not.
		CHECK(GTPFlightPedals::StopsForPerson(9.f, 36.f / 3.6f));
		CHECK(!GTPFlightPedals::StopsForPerson(6.f, 36.f / 3.6f));
		// 43 km/h is the fastest it brakes at with 9 m of probe.
		CHECK(GTPFlightPedals::StopsForPerson(9.f, 37.f / 3.6f) && !GTPFlightPedals::StopsForPerson(9.f, 39.f / 3.6f));
		// At walking pace it waits for anybody, against the bumper or not (the script pushes through after its wait).
		CHECK(GTPFlightPedals::StopsForPerson(2.f, 5.f / 3.6f) && GTPFlightPedals::StopsForPerson(0.f, 0.f));
		// 20 km/h, a person 2 m ahead: too late. 4 m ahead: it stops.
		CHECK(!GTPFlightPedals::StopsForPerson(2.f, 20.f / 3.6f) && GTPFlightPedals::StopsForPerson(4.f, 20.f / 3.6f));
		// -DemoPedalsAlone: as before.
		Throttle = 1.f; Brake = 0.f;
		GTPFlightPedals::Apply(Throttle, Brake, 1.f, 10.f, 16.7f, Settings, false);
		CHECK(Throttle == 1.f && Brake == 0.f);
	}
	std::printf(Failures ? "FAILED (%d)\n" : "OK\n", Failures);
	return Failures ? 1 : 0;
}
