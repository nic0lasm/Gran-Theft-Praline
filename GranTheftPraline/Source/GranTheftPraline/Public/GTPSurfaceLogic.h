#pragma once

// Footstep and surface rules, written without engine types so they can be
// built and checked outside Unreal (tools/audio/audio_logic_test.cpp).
//
// - FGTPSurfaceMap: the ground of the generated city is one material driven by
//   a mask (tools/city/ground_mask.py), so the material alone cannot tell
//   asphalt from a sidewalk. tools/audio/surface_map.py turns that mask, plus
//   vegetation and hand-drawn areas, into a small grid of surface ids that is
//   read here at the hit point.
// - GTPSurfaceFromName: keywords in a material or physical material name.
// - FGTPFootPlantDetector: one foot; a step is the moment the foot comes down
//   below a height above the ground, with hysteresis so it fires once.

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

/** Byte values of the surface map, same order as EGTPSurface. */
namespace GTPSurfaceId
{
	constexpr uint8_t Asphalt = 0;
	constexpr uint8_t Stone = 1;
	constexpr uint8_t Cobble = 2;
	constexpr uint8_t Gravel = 3;
	constexpr uint8_t Grass = 4;
	constexpr uint8_t Metal = 5;
	constexpr uint8_t Wood = 6;
	constexpr uint8_t Water = 7;
	constexpr uint8_t Count = 8;
	/** No information at this point (outside the map). */
	constexpr uint8_t Unknown = 255;
}

/**
 * Grid of surface ids over the ground of a focus street, in Unreal world
 * centimetres. File layout (little endian), written by surface_map.py:
 *   char[8]  "GTPSURF1"
 *   int32    width, height (cells)
 *   float32  x0, y0 (world cm of the corner of cell 0,0), cell size (cm)
 *   uint8    width * height ids, row by row (row = +Y)
 */
class FGTPSurfaceMap
{
public:
	bool Load(const uint8_t* Data, size_t Size)
	{
		Cells.clear();
		Width = Height = 0;
		const size_t Header = 8 + 4 * 2 + 4 * 3;
		if (!Data || Size < Header || std::memcmp(Data, "GTPSURF1", 8) != 0)
		{
			return false;
		}
		int32_t W = 0, H = 0;
		float X = 0.f, Y = 0.f, Cell = 0.f;
		std::memcpy(&W, Data + 8, 4);
		std::memcpy(&H, Data + 12, 4);
		std::memcpy(&X, Data + 16, 4);
		std::memcpy(&Y, Data + 20, 4);
		std::memcpy(&Cell, Data + 24, 4);
		if (W <= 0 || H <= 0 || !(Cell > 0.f) || Size < Header + static_cast<size_t>(W) * static_cast<size_t>(H))
		{
			return false;
		}
		Width = W;
		Height = H;
		X0 = X;
		Y0 = Y;
		CellCm = Cell;
		Cells.assign(Data + Header, Data + Header + static_cast<size_t>(W) * static_cast<size_t>(H));
		return true;
	}

	bool IsLoaded() const { return !Cells.empty(); }

	/** Surface id at a world point, or GTPSurfaceId::Unknown outside the map. */
	uint8_t Sample(float WorldX, float WorldY) const
	{
		if (Cells.empty())
		{
			return GTPSurfaceId::Unknown;
		}
		const int Cx = static_cast<int>(std::floor((WorldX - X0) / CellCm));
		const int Cy = static_cast<int>(std::floor((WorldY - Y0) / CellCm));
		if (Cx < 0 || Cy < 0 || Cx >= Width || Cy >= Height)
		{
			return GTPSurfaceId::Unknown;
		}
		const uint8_t Id = Cells[static_cast<size_t>(Cy) * Width + Cx];
		return Id < GTPSurfaceId::Count ? Id : GTPSurfaceId::Unknown;
	}

	int GetWidth() const { return Width; }
	int GetHeight() const { return Height; }
	float GetCellCm() const { return CellCm; }

private:
	std::vector<uint8_t> Cells;
	int Width = 0;
	int Height = 0;
	float X0 = 0.f;
	float Y0 = 0.f;
	float CellCm = 50.f;
};

/**
 * Surface named by keywords in a material or physical material name
 * ("MI_Shopfront_Wood" -> Wood, "PM_Gravel" -> Gravel), or Unknown.
 * Matching ignores case; the first keyword found in the list wins, so the
 * more specific words come first.
 */
inline uint8_t GTPSurfaceFromName(const char* Name)
{
	if (!Name)
	{
		return GTPSurfaceId::Unknown;
	}
	std::string Lower(Name);
	for (char& C : Lower)
	{
		C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
	}
	struct FKeyword
	{
		const char* Word;
		uint8_t Id;
	};
	static const FKeyword Keywords[] = {
		{"puddle", GTPSurfaceId::Water}, {"water", GTPSurfaceId::Water},
		{"cobble", GTPSurfaceId::Cobble}, {"pave_", GTPSurfaceId::Cobble}, {"sett", GTPSurfaceId::Cobble},
		{"gravel", GTPSurfaceId::Gravel}, {"sand", GTPSurfaceId::Gravel}, {"dirt", GTPSurfaceId::Gravel},
		{"grass", GTPSurfaceId::Grass}, {"lawn", GTPSurfaceId::Grass}, {"moss", GTPSurfaceId::Grass},
		{"metal", GTPSurfaceId::Metal}, {"steel", GTPSurfaceId::Metal}, {"iron", GTPSurfaceId::Metal},
		{"alumin", GTPSurfaceId::Metal}, {"grate", GTPSurfaceId::Metal}, {"manhole", GTPSurfaceId::Metal},
		{"wood", GTPSurfaceId::Wood}, {"plank", GTPSurfaceId::Wood}, {"parquet", GTPSurfaceId::Wood},
		{"asphalt", GTPSurfaceId::Asphalt}, {"tarmac", GTPSurfaceId::Asphalt}, {"road", GTPSurfaceId::Asphalt},
		{"limestone", GTPSurfaceId::Stone}, {"granite", GTPSurfaceId::Stone}, {"stone", GTPSurfaceId::Stone},
		{"concrete", GTPSurfaceId::Stone}, {"tile", GTPSurfaceId::Stone}, {"slab", GTPSurfaceId::Stone},
		{"marble", GTPSurfaceId::Stone}, {"kerb", GTPSurfaceId::Stone}, {"floor", GTPSurfaceId::Stone},
	};
	for (const FKeyword& Keyword : Keywords)
	{
		if (Lower.find(Keyword.Word) != std::string::npos)
		{
			return Keyword.Id;
		}
	}
	return GTPSurfaceId::Unknown;
}

struct FGTPFootPlantSettings
{
	/** Height of the foot bone above the ground below which the foot is down (cm). */
	float PlantHeight = 13.f;
	/** The foot must rise this much above PlantHeight before the next step counts (cm). */
	float LiftHysteresis = 5.f;
	/** A foot cannot step twice within this time (s): filters jitter and shuffles. */
	float MinInterval = 0.22f;
};

/** One foot. Update every frame with the foot height above the ground. */
class FGTPFootPlantDetector
{
public:
	void Reset()
	{
		bDown = true;
		SinceStep = 1000.f;
		LastHeight = 0.f;
		bHasLast = false;
	}

	/**
	 * Returns true on the frame the foot touches down. Downward speed (cm/s,
	 * positive when falling) is returned through OutDownSpeed for loudness.
	 */
	bool Update(float HeightCm, float DeltaSeconds, const FGTPFootPlantSettings& Settings, float* OutDownSpeed = nullptr)
	{
		SinceStep += DeltaSeconds;
		const float DownSpeed = (bHasLast && DeltaSeconds > 0.f) ? (LastHeight - HeightCm) / DeltaSeconds : 0.f;
		LastHeight = HeightCm;
		bHasLast = true;
		if (OutDownSpeed)
		{
			*OutDownSpeed = DownSpeed;
		}
		if (bDown)
		{
			if (HeightCm > Settings.PlantHeight + Settings.LiftHysteresis)
			{
				bDown = false;
			}
			return false;
		}
		if (HeightCm <= Settings.PlantHeight)
		{
			bDown = true;
			if (SinceStep >= Settings.MinInterval)
			{
				SinceStep = 0.f;
				return true;
			}
		}
		return false;
	}

	bool IsDown() const { return bDown; }

private:
	bool bDown = true;
	bool bHasLast = false;
	float SinceStep = 1000.f;
	float LastHeight = 0.f;
};

/**
 * Loudness of a step from the ground speed of the character (cm/s):
 * a slow walk is quiet, a sprint is full. Returned in [MinGain, 1].
 */
inline float GTPStepGain(float GroundSpeed, float WalkSpeed = 160.f, float SprintSpeed = 600.f, float MinGain = 0.35f)
{
	if (SprintSpeed <= WalkSpeed)
	{
		return 1.f;
	}
	float T = (GroundSpeed - WalkSpeed) / (SprintSpeed - WalkSpeed);
	T = T < 0.f ? 0.f : (T > 1.f ? 1.f : T);
	return MinGain + (1.f - MinGain) * std::sqrt(T);
}

/**
 * Impact tier from the change of speed of a hit (cm/s): 0 = too light to
 * be heard, 1 light, 2 medium, 3 heavy.
 */
inline int GTPImpactTier(float DeltaSpeed, float Light = 120.f, float Medium = 450.f, float Heavy = 1000.f)
{
	if (DeltaSpeed >= Heavy)
	{
		return 3;
	}
	if (DeltaSpeed >= Medium)
	{
		return 2;
	}
	return DeltaSpeed >= Light ? 1 : 0;
}

/**
 * Doppler factor of a source heard by a listener (speeds in cm/s, projected
 * on the line from the listener to the source, positive when moving away
 * from the listener). Clamped so a wrong velocity cannot make a squeal.
 */
inline float GTPDopplerFactor(float SourceAwaySpeed, float ListenerTowardSpeed, float SoundSpeed = 34300.f)
{
	// A source "approaching faster than sound" is a bad velocity: keep the denominator sane.
	const float Denominator = SoundSpeed + SourceAwaySpeed;
	float Factor = (SoundSpeed + ListenerTowardSpeed) / (Denominator > 0.5f * SoundSpeed ? Denominator : 0.5f * SoundSpeed);
	return Factor < 0.8f ? 0.8f : (Factor > 1.25f ? 1.25f : Factor);
}
