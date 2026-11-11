// Checks the engine-free car signal rules (GTPCarSignalLogic.h). Exit code 0 when they hold.
// Build: g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/environment/car_signals_test.cpp -o car_signals_test
#include "GTPCarSignalLogic.h"

#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #Cond); ++Failures; } } while (false)

int main()
{
	FGTPCarSignalState State;
	FGTPCarSignalInput In;

	// Cruising straight: nothing.
	In.SpeedCmS = 1000.f; In.Gear = 3; In.TimeSeconds = 1.0;
	FGTPCarSignalOutput Out = GTPCarSignals::Evaluate(In, State);
	CHECK(!Out.bBrake && !Out.bReverse && Out.IndicatorSide == 0 && !Out.bIndicatorLit);

	// Braking and handbrake.
	In.Brake = 0.5f;
	CHECK(GTPCarSignals::Evaluate(In, State).bBrake);
	In.Brake = 0.05f; In.bHandbrake = true;
	CHECK(GTPCarSignals::Evaluate(In, State).bBrake);
	In.Brake = 0.f; In.bHandbrake = false;
	CHECK(!GTPCarSignals::Evaluate(In, State).bBrake);

	// Reverse gear: reverse light, no indicator even when steering.
	In.Gear = -1; In.Steering = 0.8f; In.SpeedCmS = 200.f;
	Out = GTPCarSignals::Evaluate(In, State);
	CHECK(Out.bReverse && Out.IndicatorSide == 0);

	// Turning right in town: indicator right, blinks at 1.5 Hz (lit then dark).
	In.Gear = 2; In.Steering = 0.6f; In.SpeedCmS = 600.f; In.TimeSeconds = 10.0;
	Out = GTPCarSignals::Evaluate(In, State);
	CHECK(Out.IndicatorSide == 1);
	int Lit = 0, Dark = 0;
	for (int i = 0; i < 60; ++i)
	{
		In.TimeSeconds = 10.0 + i * 0.05;
		(GTPCarSignals::Evaluate(In, State).bIndicatorLit ? Lit : Dark)++;
	}
	CHECK(Lit > 20 && Dark > 20);

	// Wheel straightens: held 1.2 s, then cancelled.
	In.Steering = 0.f; In.TimeSeconds = 13.5;
	CHECK(GTPCarSignals::Evaluate(In, State).IndicatorSide == 1);
	In.TimeSeconds = 14.5;
	CHECK(GTPCarSignals::Evaluate(In, State).IndicatorSide == 0);

	// Left turn; no indicator on the motorway-like speed nor while parked.
	In.Steering = -0.5f; In.TimeSeconds = 20.0;
	CHECK(GTPCarSignals::Evaluate(In, State).IndicatorSide == -1);
	FGTPCarSignalState Fresh;
	In.SpeedCmS = 2500.f;
	CHECK(GTPCarSignals::Evaluate(In, Fresh).IndicatorSide == 0);
	In.SpeedCmS = 0.f;
	CHECK(GTPCarSignals::Evaluate(In, Fresh).IndicatorSide == 0);

	std::printf(Failures ? "car signals: %d failure(s)\n" : "car signals: all rules hold (%d failures)\n", Failures);
	return Failures ? 1 : 0;
}
