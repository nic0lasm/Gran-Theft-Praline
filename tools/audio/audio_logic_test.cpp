// Checks the engine-free audio rules (GTPSurfaceLogic.h) and renders the
// procedural sirens and tyre squeal (GTPAudioSynths.h) to WAV files for
// check_synths.py. Exit code 0 when every rule holds.
// Build: g++ -O2 -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/audio/audio_logic_test.cpp -o audio_logic_test
// Run:   ./audio_logic_test <output folder>
#include "GTPAudioSynths.h"
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

int main(int Argc, char** Argv)
{
	TestSurfaceMap();
	TestNames();
	TestFootPlant();
	if (Argc > 1)
	{
		RenderSynths(Argv[1]);
	}
	if (Failures)
	{
		std::printf("%d check(s) failed\n", Failures);
		return 1;
	}
	std::printf("audio logic: all checks passed\n");
	return 0;
}
