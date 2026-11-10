// Checks the engine-free audio rules (GTPSurfaceLogic.h) and renders the
// procedural sirens and tyre squeal (GTPAudioSynths.h) to WAV files for
// check_synths.py. Exit code 0 when every rule holds.
// Build: g++ -O2 -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/audio/audio_logic_test.cpp -o audio_logic_test
// Run:   ./audio_logic_test <output folder>
#include "GTPAudioSynths.h"
#include "GTPEngineSynth.h"
#include "GTPSurfaceLogic.h"

#include <cstdio>
#include <string>
#include <vector>

static int Failures = 0;

#define CHECK(Cond)                                                        \
	do                                                                     \
	{                                                                      \
		if (!(Cond))                                                       \
		{                                                                  \
			std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #Cond);     \
			++Failures;                                                    \
		}                                                                  \
	} while (false)

static void WriteWav(const std::string& Path, const std::vector<float>& Samples, int Rate)
{
	FILE* File = std::fopen(Path.c_str(), "wb");
	if (!File)
	{
		std::perror(Path.c_str());
		++Failures;
		return;
	}
	const uint32_t DataBytes = static_cast<uint32_t>(Samples.size() * 2);
	const uint32_t RiffSize = 36 + DataBytes;
	const uint16_t Format = 1, Channels = 1, Bits = 16, Align = 2;
	const uint32_t ByteRate = Rate * 2, FmtSize = 16, SampleRate = Rate;
	std::fwrite("RIFF", 1, 4, File);
	std::fwrite(&RiffSize, 4, 1, File);
	std::fwrite("WAVEfmt ", 1, 8, File);
	std::fwrite(&FmtSize, 4, 1, File);
	std::fwrite(&Format, 2, 1, File);
	std::fwrite(&Channels, 2, 1, File);
	std::fwrite(&SampleRate, 4, 1, File);
	std::fwrite(&ByteRate, 4, 1, File);
	std::fwrite(&Align, 2, 1, File);
	std::fwrite(&Bits, 2, 1, File);
	std::fwrite("data", 1, 4, File);
	std::fwrite(&DataBytes, 4, 1, File);
	for (float Sample : Samples)
	{
		const float Clamped = Sample < -1.f ? -1.f : (Sample > 1.f ? 1.f : Sample);
		const int16_t Value = static_cast<int16_t>(Clamped * 32767.f);
		std::fwrite(&Value, 2, 1, File);
	}
	std::fclose(File);
}

static void TestSurfaceMap()
{
	// 3 x 2 cells of 50 cm from (-100, 200).
	std::vector<uint8_t> Data(8 + 20 + 6);
	std::memcpy(Data.data(), "GTPSURF1", 8);
	const int32_t W = 3, H = 2;
	const float X0 = -100.f, Y0 = 200.f, Cell = 50.f;
	std::memcpy(Data.data() + 8, &W, 4);
	std::memcpy(Data.data() + 12, &H, 4);
	std::memcpy(Data.data() + 16, &X0, 4);
	std::memcpy(Data.data() + 20, &Y0, 4);
	std::memcpy(Data.data() + 24, &Cell, 4);
	const uint8_t Ids[6] = {0, 1, 2, 3, 4, 200};
	std::memcpy(Data.data() + 28, Ids, 6);
	FGTPSurfaceMap Map;
	CHECK(Map.Load(Data.data(), Data.size()));
	CHECK(Map.Sample(-99.f, 201.f) == GTPSurfaceId::Asphalt);
	CHECK(Map.Sample(-40.f, 249.f) == GTPSurfaceId::Stone);
	CHECK(Map.Sample(-99.f, 251.f) == GTPSurfaceId::Gravel);
	CHECK(Map.Sample(45.f, 299.f) == GTPSurfaceId::Unknown); // invalid id 200
	CHECK(Map.Sample(-101.f, 201.f) == GTPSurfaceId::Unknown);
	CHECK(Map.Sample(60.f, 201.f) == GTPSurfaceId::Unknown);
	CHECK(!Map.Load(Data.data(), 20));
	Data[0] = 'X';
	CHECK(!Map.Load(Data.data(), Data.size()));
}

static void TestNames()
{
	CHECK(GTPSurfaceFromName("MI_Shopfront_Wood") == GTPSurfaceId::Wood);
	CHECK(GTPSurfaceFromName("MI_Shutter_Metal") == GTPSurfaceId::Metal);
	CHECK(GTPSurfaceFromName("MI_Shopfront_Steel") == GTPSurfaceId::Metal);
	CHECK(GTPSurfaceFromName("PM_Gravel_Bellecour") == GTPSurfaceId::Gravel);
	CHECK(GTPSurfaceFromName("MI_Facade_LimestoneCream") == GTPSurfaceId::Stone);
	CHECK(GTPSurfaceFromName("M_Cobblestone") == GTPSurfaceId::Cobble);
	CHECK(GTPSurfaceFromName("MI_Shop_Floor") == GTPSurfaceId::Stone);
	CHECK(GTPSurfaceFromName("M_City_Ground") == GTPSurfaceId::Unknown);
	CHECK(GTPSurfaceFromName(nullptr) == GTPSurfaceId::Unknown);
}

static void TestFootPlant()
{
	// A foot swinging between 2 cm and 30 cm, 1.8 steps per second, for 10 s at 60 fps.
	FGTPFootPlantSettings Settings;
	FGTPFootPlantDetector Foot;
	Foot.Reset();
	int Steps = 0;
	const float Dt = 1.f / 60.f;
	for (int Frame = 0; Frame < 600; ++Frame)
	{
		const float T = Frame * Dt;
		const float Height = 16.f + 14.f * std::sin(6.2831853f * 0.9f * T);
		Steps += Foot.Update(Height, Dt, Settings) ? 1 : 0;
	}
	std::printf("foot plants in 10 s at 0.9 Hz: %d\n", Steps);
	CHECK(Steps == 9);

	// Jitter around the threshold must not fire again and again.
	FGTPFootPlantDetector Jitter;
	Jitter.Reset();
	int JitterSteps = 0;
	for (int Frame = 0; Frame < 600; ++Frame)
	{
		const float Height = Settings.PlantHeight + ((Frame % 2) ? 1.5f : -1.5f);
		JitterSteps += Jitter.Update(Height, Dt, Settings) ? 1 : 0;
	}
	CHECK(JitterSteps == 0);

	CHECK(GTPStepGain(0.f) > 0.34f && GTPStepGain(0.f) < 0.36f);
	CHECK(GTPStepGain(700.f) == 1.f);
	CHECK(GTPStepGain(300.f) < GTPStepGain(450.f));
	CHECK(GTPImpactTier(50.f) == 0 && GTPImpactTier(200.f) == 1 && GTPImpactTier(600.f) == 2 && GTPImpactTier(1500.f) == 3);
	CHECK(GTPDopplerFactor(-1500.f, 0.f) > 1.04f);  // approaching at 54 km/h
	CHECK(GTPDopplerFactor(1500.f, 0.f) < 0.96f);   // leaving
	CHECK(GTPDopplerFactor(-1e6f, 0.f) == 1.25f);   // clamped
}

static void RenderSynths(const std::string& Folder)
{
	const int Rate = 48000;
	FGTPSirenSynth Siren;
	Siren.Init(Rate, FGTPSirenSettings());
	std::vector<float> Out(Rate * 6);
	// 0.5 s off, 3 s two-tone, 2 s yelp, 0.5 s off.
	const int Block = 480;
	for (int Start = 0; Start < static_cast<int>(Out.size()); Start += Block)
	{
		const float T = static_cast<float>(Start) / Rate;
		Siren.SetMode(T < 0.5f ? 0 : (T < 3.5f ? 1 : (T < 5.5f ? 2 : 0)));
		Siren.Render(Out.data() + Start, Block);
	}
	WriteWav(Folder + "/siren_modes.wav", Out, Rate);

	// Doppler pass: a car at 15 m/s passing 5 m from the listener.
	FGTPSirenSynth Pass;
	Pass.Init(Rate, FGTPSirenSettings());
	Pass.SetMode(1);
	std::vector<float> PassOut(Rate * 6);
	for (int Start = 0; Start < static_cast<int>(PassOut.size()); Start += Block)
	{
		const float T = static_cast<float>(Start) / Rate - 3.f;
		const float X = 1500.f * T;
		const float Dist = std::sqrt(X * X + 500.f * 500.f);
		const float Away = 1500.f * X / Dist;
		Pass.SetPitchScale(GTPDopplerFactor(Away, 0.f));
		Pass.Render(PassOut.data() + Start, Block);
		const float Gain = 500.f / Dist;
		for (int I = 0; I < Block; ++I)
		{
			PassOut[Start + I] *= Gain;
		}
	}
	WriteWav(Folder + "/siren_pass.wav", PassOut, Rate);

	FGTPSkidSynth Skid;
	Skid.Init(Rate, FGTPSkidSettings());
	std::vector<float> SkidOut(Rate * 4);
	for (int Start = 0; Start < static_cast<int>(SkidOut.size()); Start += Block)
	{
		const float T = static_cast<float>(Start) / Rate;
		Skid.SetAmount(T < 3.f ? T / 3.f : 0.f);
		Skid.Render(SkidOut.data() + Start, Block);
	}
	WriteWav(Folder + "/skid_ramp.wav", SkidOut, Rate);
}

// Engine profiles of DefaultGame.ini (EngineProfiles), driven through a short
// scripted run: idle, a blip, three gears at full throttle with the gear-change
// cut, a lift-off (turbo hiss), a cruise and back to idle.
struct FProfileCase
{
	const char* Name;
	FGTPEngineSynthSettings Settings;
	float ShiftCut;
};

static std::vector<float> DriveRun(const FProfileCase& Case, int Rate)
{
	FGTPEngineSynth Synth;
	Synth.Init(Rate, Case.Settings);
	const float Idle = Case.Settings.IdleRpm;
	const float Max = Case.Settings.MaxRpm;
	std::vector<float> Out;
	const int Block = Rate / 100;
	std::vector<float> Buffer(Block);
	float Rpm = Idle;
	float T = 0.f;
	int Gear = 0;
	float Cut = 0.f;
	const float Total = 20.f;
	while (T < Total)
	{
		float Load = 0.05f;
		if (T < 2.f)
		{
			Rpm = Idle;
		}
		else if (T < 3.f)
		{
			// Blip: up to half the range and back.
			const float A = (T - 2.f) < 0.3f ? (T - 2.f) / 0.3f : 1.f - (T - 2.3f) / 0.7f;
			Rpm = Idle + (Max * 0.5f - Idle) * (A > 0.f ? A : 0.f);
			Load = T < 2.3f ? 1.f : 0.f;
		}
		else if (T < 12.f)
		{
			if (Gear == 0)
			{
				Gear = 1;
				Rpm = Idle * 1.4f;
			}
			if (Cut > 0.f)
			{
				Cut -= 0.01f;
				Load = 0.f;
				Rpm -= (Rpm - Max * 0.55f) * 0.08f;
			}
			else
			{
				Load = 1.f;
				Rpm += (Max - Idle) * 0.01f / (1.2f + 0.9f * Gear);
				if (Rpm > Max * 0.88f && Gear < 3)
				{
					++Gear;
					Cut = Case.ShiftCut;
				}
				Rpm = Rpm > Max * 0.95f ? Max * 0.95f : Rpm;
			}
		}
		else if (T < 15.f)
		{
			Load = 0.f;
			Rpm += (Idle * 1.8f - Rpm) * 0.02f;
		}
		else if (T < 18.f)
		{
			Load = 0.3f;
			Rpm += (Idle * 2.6f - Rpm) * 0.05f;
		}
		else
		{
			Rpm += (Idle - Rpm) * 0.05f;
		}
		Synth.SetTarget(Rpm, Load);
		Synth.Render(Buffer.data(), Block);
		Out.insert(Out.end(), Buffer.begin(), Buffer.end());
		T += 0.01f;
	}
	return Out;
}

static void RenderEngines(const std::string& Folder)
{
	const int Rate = 48000;
	FProfileCase Cases[3];
	Cases[0].Name = "engine_clio";
	Cases[0].Settings.Cylinders = 3; Cases[0].Settings.IdleRpm = 850.f; Cases[0].Settings.MaxRpm = 6000.f;
	Cases[0].Settings.IdleGain = 0.22f; Cases[0].Settings.FullLoadGain = 0.6f; Cases[0].Settings.NoiseAmount = 0.18f;
	Cases[0].Settings.Jitter = 0.12f; Cases[0].Settings.RingHarmonic = 4.f; Cases[0].Settings.Turbo = 0.6f; Cases[0].Settings.TurboPitchHz = 5200.f;
	Cases[0].ShiftCut = 0.2f;
	Cases[1].Name = "engine_208";
	Cases[1].Settings.Cylinders = 3; Cases[1].Settings.IdleRpm = 750.f; Cases[1].Settings.MaxRpm = 6200.f;
	Cases[1].Settings.IdleGain = 0.2f; Cases[1].Settings.FullLoadGain = 0.62f; Cases[1].Settings.NoiseAmount = 0.2f;
	Cases[1].Settings.Jitter = 0.16f; Cases[1].Settings.RingHarmonic = 3.5f; Cases[1].Settings.Turbo = 0.5f; Cases[1].Settings.TurboPitchHz = 4600.f;
	Cases[1].ShiftCut = 0.18f;
	Cases[2].Name = "engine_kangoo";
	Cases[2].Settings.Cylinders = 4; Cases[2].Settings.IdleRpm = 800.f; Cases[2].Settings.MaxRpm = 4500.f;
	Cases[2].Settings.IdleGain = 0.28f; Cases[2].Settings.FullLoadGain = 0.58f; Cases[2].Settings.NoiseAmount = 0.24f;
	Cases[2].Settings.Jitter = 0.1f; Cases[2].Settings.RingHarmonic = 5.f; Cases[2].Settings.Diesel = 0.8f; Cases[2].Settings.Turbo = 0.7f;
	Cases[2].Settings.TurboPitchHz = 4000.f;
	Cases[2].ShiftCut = 0.3f;
	for (const FProfileCase& Case : Cases)
	{
		const std::vector<float> Out = DriveRun(Case, Rate);
		for (float V : Out)
		{
			CHECK(V == V && V >= -1.f && V <= 1.f);
		}
		WriteWav(Folder + "/" + Case.Name + ".wav", Out, Rate);
	}
}

int main(int Argc, char** Argv)
{
	TestSurfaceMap();
	TestNames();
	TestFootPlant();
	if (Argc > 1)
	{
		RenderSynths(Argv[1]);
		RenderEngines(Argv[1]);
	}
	if (Failures)
	{
		std::printf("%d check(s) failed\n", Failures);
		return 1;
	}
	std::printf("audio logic: all checks passed\n");
	return 0;
}
