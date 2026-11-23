// Checks the engine-free lens rules (GTPPictureLens.h) against the art bible, section 9.
// Exit code 0 when they hold.
// Build: g++ -O2 -std=c++17 -Wall -Wextra -Werror -I GranTheftPraline/Source/GranTheftPraline/Public tools/environment/picture_lens_test.cpp -o picture_lens_test
#include "GTPPictureLens.h"

#include <cmath>
#include <cstdio>

static int Failures = 0;
#define CHECK(Cond) do { if (!(Cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #Cond); ++Failures; } } while (false)

static bool Near(float A, float B, float Tolerance = 1e-4f)
{
	return std::fabs(A - B) <= Tolerance;
}

int main()
{
	const FGTPLensSettings Settings;

	// Art bible 9, starting values: bloom about 0.3, motion blur about 0.3 and 2 % at most, vignette about 0.2.
	FGTPLensState Day = GTPPictureLens::Evaluate(Settings, 0.f, 5.f);
	CHECK(Near(Day.Bloom, 0.3f) && Near(Day.Vignette, 0.2f));
	CHECK(Near(Day.MotionBlur, 0.3f) && Near(Day.MotionBlurMaxPercent, 2.f));
	CHECK(Day.Sharpen > 0.f && Day.Sharpen <= 0.6f);

	// A sprint (about 25 km/h) and town traffic are not "driving fast".
	CHECK(Near(GTPPictureLens::Evaluate(Settings, 0.f, 25.f).MotionBlur, Day.MotionBlur));
	CHECK(Near(GTPPictureLens::Evaluate(Settings, 0.f, 50.f).MotionBlur, Day.MotionBlur));

	// Driving fast: more blur, never more than the engine's own default (0.5 and 5 %), and it grows with the speed.
	float Last = Day.MotionBlur;
	for (float Kmh = 50.f; Kmh <= 200.f; Kmh += 10.f)
	{
		const FGTPLensState S = GTPPictureLens::Evaluate(Settings, 0.f, Kmh);
		CHECK(S.MotionBlur >= Last && S.MotionBlur <= 0.5f && S.MotionBlurMaxPercent <= 5.f);
		Last = S.MotionBlur;
	}
	CHECK(Near(GTPPictureLens::Evaluate(Settings, 0.f, 110.f).MotionBlur, Settings.DrivingMotionBlur));
	CHECK(Near(GTPPictureLens::Evaluate(Settings, 0.f, 300.f).MotionBlur, Settings.DrivingMotionBlur));

	// Night: the lamps glow more, the rest does not move; values outside 0..1 are clamped.
	const FGTPLensState Night = GTPPictureLens::Evaluate(Settings, 1.f, 5.f);
	CHECK(Night.Bloom > Day.Bloom && Night.Bloom <= 0.5f);
	CHECK(Near(Night.MotionBlur, Day.MotionBlur) && Near(Night.Vignette, Day.Vignette));
	CHECK(Near(GTPPictureLens::Evaluate(Settings, 2.f, 5.f).Bloom, Night.Bloom));
	CHECK(Near(GTPPictureLens::Evaluate(Settings, -1.f, 5.f).Bloom, Day.Bloom));
	const float Dusk = GTPPictureLens::Evaluate(Settings, 0.5f, 5.f).Bloom;
	CHECK(Dusk > Day.Bloom && Dusk < Night.Bloom);

	// Camera speed: 10 cm in a 60th of a second is 21.6 km/h; the smoothed value gets there in a few seconds.
	float Speed = 0.f;
	for (int Frame = 0; Frame < 30; ++Frame)
	{
		Speed = GTPPictureLens::FollowSpeed(Speed, 10.f, 1.f / 60.f);
	}
	CHECK(Speed > 10.f && Speed < 21.6f);
	for (int Frame = 0; Frame < 240; ++Frame)
	{
		Speed = GTPPictureLens::FollowSpeed(Speed, 10.f, 1.f / 60.f);
	}
	CHECK(Near(Speed, 21.6f, 0.1f));

	// A respawn or a camera cut (50 m in one frame) is not a speed; neither is a frame without duration.
	const float Before = Speed;
	CHECK(Near(GTPPictureLens::FollowSpeed(Speed, 5000.f, 1.f / 60.f), Before));
	CHECK(Near(GTPPictureLens::FollowSpeed(Speed, 10.f, 0.f), Before));
	// A long frame (hitch) does not overshoot.
	CHECK(Near(GTPPictureLens::FollowSpeed(0.f, 3000.f, 2.f), 54.f, 0.01f));

	if (Failures == 0)
	{
		std::printf("picture lens rules: ok\n");
	}
	return Failures == 0 ? 0 : 1;
}
