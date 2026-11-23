// Street-level picture of the backdrop facade shader, without Unreal: two rows of walls along
// a street, a pinhole camera at eye height, the shader's colour under a plain sun and sky.
// Usage: facade_preview <out.ppm> [width height] ; prints a few figures on stdout.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "hlsl_shim.h"

namespace hlsl
{
static float4 LyonFacade(float3 Pw, float3 Nw, float3 Vw, float Depth, float3 WallC)
{
#include "facade_body.inc"
}
}   // namespace hlsl

using hlsl::float3;

namespace
{
const float StreetHalfWidth = 600.0f;   // cm: a 12 m street of the Presqu'ile
const float GroundZ = -80.0f;           // the shader takes the street at -0.8 m
const float WallTop = GroundZ + 2100.0f;
const float3 Sun = hlsl::normalize(float3(-0.35, -0.55, 0.75));   // lights the wall at +Y

float Encode(float Linear)
{
	const float V = Linear < 0.0f ? 0.0f : Linear > 1.0f ? 1.0f : Linear;
	return V <= 0.0031308f ? 12.92f * V : 1.055f * std::pow(V, 1.0f / 2.4f) - 0.055f;
}

// One building colour per 18 m of street, as the generator gives one colour per building.
float3 BuildingColour(float X, float Side)
{
	static const float3 Colours[] = {float3(0.52, 0.47, 0.37), float3(0.47, 0.40, 0.31),
	                                 float3(0.56, 0.51, 0.42), float3(0.43, 0.39, 0.33)};
	const int Index = int(std::floor(X / 1800.0f) + (Side > 0.0f ? 1.0f : 0.0f));
	return Colours[((Index % 4) + 4) % 4];
}
}   // namespace

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "usage: facade_preview <out.ppm> [width height]\n");
		return 2;
	}
	const int Width = Argc > 3 ? std::atoi(Argv[2]) : 1280;
	const int Height = Argc > 3 ? std::atoi(Argv[3]) : 720;
	const float3 Eye(0.0, -150.0, GroundZ + 170.0);
	const float3 Forward = hlsl::normalize(float3(1.0, 0.16, 0.10));
	const float3 Right = hlsl::normalize(float3(Forward.y, -Forward.x, 0.0));
	const float3 Up(Right.y * Forward.z - Right.z * Forward.y, Right.z * Forward.x - Right.x * Forward.z,
	                Right.x * Forward.y - Right.y * Forward.x);
	const float Tan = 0.7f;   // about 70 degrees across, the game's camera
	std::vector<unsigned char> Image(size_t(Width) * Height * 3);
	long Glass = 0;
	long Wall = 0;
	double Sum = 0.0;
	double SumSquares = 0.0;
	for (int Y = 0; Y < Height; ++Y)
	{
		for (int X = 0; X < Width; ++X)
		{
			const float Sx = (2.0f * (X + 0.5f) / Width - 1.0f) * Tan;
			const float Sy = (1.0f - 2.0f * (Y + 0.5f) / Height) * Tan * Height / Width;
			const float3 Dir = hlsl::normalize(Forward + Right * Sx + Up * Sy);
			float3 Colour(0.45f + 0.25f * Dir.z, 0.60f + 0.20f * Dir.z, 0.85f);   // sky
			float Best = 1.0e9f;
			if (Dir.z < 0.0f)
			{
				Best = (GroundZ - Eye.z) / Dir.z;
				Colour = float3(0.07, 0.07, 0.07);
			}
			for (float Side = -1.0f; Side <= 1.0f; Side += 2.0f)
			{
				if (Dir.y * Side <= 0.0f)
				{
					continue;
				}
				const float T = (Side * StreetHalfWidth - Eye.y) / Dir.y;
				const float3 P = Eye + Dir * T;
				if (T <= 0.0f || T >= Best || P.z > WallTop || P.z < GroundZ)
				{
					continue;
				}
				Best = T;
				const float3 Normal(0.0, -Side, 0.0);
				const hlsl::float4 Out = hlsl::LyonFacade(P, Normal, Dir * -1.0f, T * hlsl::dot(Dir, Forward),
				                                          BuildingColour(P.x, Side));
				const float Lit = Side > 0.0f ? 0.25f + 0.75f * hlsl::max(hlsl::dot(Normal, Sun), 0.0) : 0.30f;
				Colour = float3(Out.x, Out.y, Out.z) * Lit;
				if (Out.w > 0.5f)
				{
					Colour = Colour + float3(0.10, 0.13, 0.18) * (0.3f + 0.7f * hlsl::saturate(Dir.z * -2.0 + 0.6));
					++Glass;
				}
				++Wall;
				const float Grey = hlsl::dot(float3(Out.x, Out.y, Out.z), float3(0.2126, 0.7152, 0.0722));
				Sum += Grey;
				SumSquares += double(Grey) * Grey;
			}
			unsigned char* Pixel = &Image[(size_t(Y) * Width + X) * 3];
			Pixel[0] = static_cast<unsigned char>(Encode(Colour.x) * 255.0f + 0.5f);
			Pixel[1] = static_cast<unsigned char>(Encode(Colour.y) * 255.0f + 0.5f);
			Pixel[2] = static_cast<unsigned char>(Encode(Colour.z) * 255.0f + 0.5f);
		}
	}
	std::FILE* File = std::fopen(Argv[1], "wb");
	if (!File)
	{
		std::fprintf(stderr, "cannot write %s\n", Argv[1]);
		return 2;
	}
	std::fprintf(File, "P6\n%d %d\n255\n", Width, Height);
	std::fwrite(Image.data(), 1, Image.size(), File);
	std::fclose(File);
	const double Mean = Wall ? Sum / Wall : 0.0;
	const double Deviation = Wall ? std::sqrt(hlsl::max(SumSquares / Wall - Mean * Mean, 0.0)) : 0.0;
	std::printf("wall_pixels %ld glass_share %.4f mean %.4f deviation %.4f\n", Wall,
	            Wall ? double(Glass) / Wall : 0.0, Mean, Deviation);
	return 0;
}
