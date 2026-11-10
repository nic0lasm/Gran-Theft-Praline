// Renders the procedural engine sound outside Unreal and writes WAV files
// plus a CSV of the steady test points, for check_engine_synth.py.
// Build: g++ -O2 -std=c++17 -I ../../../GranTheftPraline/Source/GranTheftPraline/Public engine_synth_check.cpp -o engine_synth_check
#include "GTPEngineSynth.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void WriteWav(const std::string& Path, const std::vector<float>& Samples, int Rate)
{
	FILE* File = std::fopen(Path.c_str(), "wb");
	if (!File)
	{
		std::perror(Path.c_str());
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
		const int16_t Value = static_cast<int16_t>(Sample * 32767.f);
		std::fwrite(&Value, 2, 1, File);
	}
	std::fclose(File);
}

int main(int Argc, char** Argv)
{
	const std::string OutDir = Argc > 1 ? Argv[1] : ".";
	const int Rate = 48000;
	const int Block = 512; // like an audio callback

	// Steady points: rpm, load, 2 s each.
	{
		FILE* Csv = std::fopen((OutDir + "/steady.csv").c_str(), "w");
		std::fprintf(Csv, "rpm,load,file\n");
		const float Points[][2] = {{850, 0}, {2000, 0.3f}, {3000, 1}, {4500, 1}, {6000, 1}, {3000, 0}};
		for (const auto& Point : Points)
		{
			FGTPEngineSynth Synth;
			Synth.Init(Rate, FGTPEngineSynthSettings());
			Synth.SetTarget(Point[0], Point[1]);
			std::vector<float> Samples(Rate * 2);
			for (size_t Start = 0; Start < Samples.size(); Start += Block)
			{
				Synth.Render(Samples.data() + Start, static_cast<int>(std::min<size_t>(Block, Samples.size() - Start)));
			}
			char Name[64];
			std::snprintf(Name, sizeof(Name), "steady_%04d_%03d.wav", static_cast<int>(Point[0]), static_cast<int>(Point[1] * 100));
			WriteWav(OutDir + "/" + Name, Samples, Rate);
			std::fprintf(Csv, "%g,%g,%s\n", Point[0], Point[1], Name);
		}
		std::fclose(Csv);
	}

	// Drive: idle, full throttle through three gears (rpm drops at each shift), lift off, idle. 12 s.
	{
		FGTPEngineSynth Synth;
		Synth.Init(Rate, FGTPEngineSynthSettings());
		std::vector<float> Samples(Rate * 12);
		for (size_t Start = 0; Start < Samples.size(); Start += Block)
		{
			const float T = static_cast<float>(Start) / Rate;
			float Rpm = 850.f, Load = 0.f;
			if (T >= 1.f && T < 9.f)
			{
				const float GearTime = std::fmod(T - 1.f, 2.66f) / 2.66f;
				Rpm = 2000.f + 2800.f * GearTime;
				Load = 1.f;
			}
			else if (T >= 9.f && T < 10.5f)
			{
				Rpm = 2600.f - (T - 9.f) * 1000.f;
			}
			Synth.SetTarget(Rpm, Load);
			Synth.Render(Samples.data() + Start, static_cast<int>(std::min<size_t>(Block, Samples.size() - Start)));
		}
		WriteWav(OutDir + "/drive_demo.wav", Samples, Rate);
	}
	std::printf("ok\n");
	return 0;
}
