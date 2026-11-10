#pragma once

// Mini-map maths and data without Unreal types (checked by tools/ui/tests/cpp):
// reading minimap.csv (tools/ui/minimap.py), the rotating round view and the
// clipping of road segments to the disc. AGTPHUD draws what this produces
// through UGTPMinimap (GTPMinimap.h).
//
// Coordinates: world = Unreal cm (X east, Y south); screen = pixels relative to
// the disc centre, x to the right, y down. The view turns with the camera so
// "up" on the map is where the player looks.

#include <cmath>
#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace GTPUI
{
enum class EMapKind
{
	RoadMajor,
	RoadMinor,
	Path,
	Tunnel,
	Water,
	Building,
};

struct FVec2
{
	float X = 0.f;
	float Y = 0.f;
};

struct FMapLine
{
	EMapKind Kind = EMapKind::RoadMinor;
	float WidthM = 0.f;
	std::string Name;          // street name, empty when none
	std::vector<FVec2> Points; // Unreal cm
	float MinX = 0.f, MinY = 0.f, MaxX = 0.f, MaxY = 0.f;
};

/** A street name and where to write it (full-screen map). */
struct FMapLabel
{
	std::string Text;
	FVec2 Position; // Unreal cm
};

struct FMapData
{
	std::vector<FMapLine> Lines;

	/** One label per street name, at the middle of its longest polyline. */
	std::vector<FMapLabel> BuildStreetLabels() const;

	/** Parses the file written by tools/ui/minimap.py; returns the number of polylines kept. */
	int ParseCsv(std::string_view Text);
};

namespace Detail
{
inline bool KindFromName(std::string_view Name, EMapKind& Out)
{
	struct Row { const char* Name; EMapKind Kind; };
	static const Row Rows[] = {
		{"road_major", EMapKind::RoadMajor}, {"road_minor", EMapKind::RoadMinor}, {"path", EMapKind::Path},
		{"tunnel", EMapKind::Tunnel}, {"water", EMapKind::Water}, {"building", EMapKind::Building},
	};
	for (const Row& R : Rows)
	{
		if (Name == R.Name)
		{
			Out = R.Kind;
			return true;
		}
	}
	return false;
}
} // namespace Detail

inline int FMapData::ParseCsv(std::string_view Text)
{
	int Kept = 0;
	bool bHeader = true;
	while (!Text.empty())
	{
		const std::size_t Newline = Text.find('\n');
		std::string_view Line = Text.substr(0, Newline);
		Text = Newline == std::string_view::npos ? std::string_view() : Text.substr(Newline + 1);
		if (!Line.empty() && Line.back() == '\r')
		{
			Line.remove_suffix(1);
		}
		if (bHeader)
		{
			bHeader = false;
			continue;
		}
		const std::size_t C1 = Line.find(',');
		const std::size_t C2 = C1 == std::string_view::npos ? C1 : Line.find(',', C1 + 1);
		if (C2 == std::string_view::npos)
		{
			continue;
		}
		FMapLine Entry;
		if (!Detail::KindFromName(Line.substr(0, C1), Entry.Kind))
		{
			continue;
		}
		Entry.WidthM = static_cast<float>(std::atof(std::string(Line.substr(C1 + 1, C2 - C1 - 1)).c_str()));
		std::string_view Points = Line.substr(C2 + 1);
		const std::size_t C3 = Points.find(',');
		if (C3 != std::string_view::npos)
		{
			Entry.Name = std::string(Points.substr(C3 + 1));
			Points = Points.substr(0, C3);
		}
		while (!Points.empty())
		{
			const std::size_t Semi = Points.find(';');
			const std::string Pair(Points.substr(0, Semi));
			Points = Semi == std::string_view::npos ? std::string_view() : Points.substr(Semi + 1);
			char* End = nullptr;
			const float X = std::strtof(Pair.c_str(), &End);
			if (End == Pair.c_str())
			{
				continue;
			}
			char* End2 = nullptr;
			const float Y = std::strtof(End, &End2);
			if (End2 == End)
			{
				continue;
			}
			Entry.Points.push_back({X, Y});
		}
		if (Entry.Points.size() < 2)
		{
			continue;
		}
		Entry.MinX = Entry.MaxX = Entry.Points[0].X;
		Entry.MinY = Entry.MaxY = Entry.Points[0].Y;
		for (const FVec2& P : Entry.Points)
		{
			Entry.MinX = std::fmin(Entry.MinX, P.X);
			Entry.MaxX = std::fmax(Entry.MaxX, P.X);
			Entry.MinY = std::fmin(Entry.MinY, P.Y);
			Entry.MaxY = std::fmax(Entry.MaxY, P.Y);
		}
		Lines.push_back(std::move(Entry));
		++Kept;
	}
	return Kept;
}

inline std::vector<FMapLabel> FMapData::BuildStreetLabels() const
{
	struct FBest
	{
		float Length = 0.f;
		FVec2 Middle;
	};
	std::unordered_map<std::string, FBest> Best;
	std::vector<std::string> Order; // stable output order
	for (const FMapLine& Line : Lines)
	{
		if (Line.Name.empty() || (Line.Kind != EMapKind::RoadMajor && Line.Kind != EMapKind::RoadMinor))
		{
			continue;
		}
		float Total = 0.f;
		for (std::size_t i = 1; i < Line.Points.size(); ++i)
		{
			Total += std::hypot(Line.Points[i].X - Line.Points[i - 1].X, Line.Points[i].Y - Line.Points[i - 1].Y);
		}
		auto It = Best.find(Line.Name);
		if (It == Best.end())
		{
			Order.push_back(Line.Name);
			It = Best.emplace(Line.Name, FBest()).first;
		}
		if (Total <= It->second.Length)
		{
			continue;
		}
		// Point at half the length.
		float Walked = 0.f;
		FVec2 Middle = Line.Points.front();
		for (std::size_t i = 1; i < Line.Points.size(); ++i)
		{
			const float Step = std::hypot(Line.Points[i].X - Line.Points[i - 1].X, Line.Points[i].Y - Line.Points[i - 1].Y);
			if (Walked + Step >= Total * 0.5f && Step > 0.f)
			{
				const float T = (Total * 0.5f - Walked) / Step;
				Middle = {Line.Points[i - 1].X + (Line.Points[i].X - Line.Points[i - 1].X) * T,
					Line.Points[i - 1].Y + (Line.Points[i].Y - Line.Points[i - 1].Y) * T};
				break;
			}
			Walked += Step;
		}
		It->second = {Total, Middle};
	}
	std::vector<FMapLabel> Out;
	for (const std::string& Name : Order)
	{
		Out.push_back({Name, Best[Name].Middle});
	}
	return Out;
}

/** Round view of the map: where the player is, where they look, how much street the disc shows. */
struct FMapView
{
	FVec2 Center;              // world position shown at the centre, cm
	float HeadingRadians = 0.f; // view yaw in the Unreal plane: atan2(forward.Y, forward.X)
	float RadiusPixels = 100.f; // disc radius on screen
	float WorldRadiusCm = 22000.f; // world distance that reaches the disc edge

	float PixelsPerCm() const { return RadiusPixels / WorldRadiusCm; }

	/** World point to disc-relative pixels: forward is up, the player's right is right. */
	FVec2 ToScreen(const FVec2& World) const
	{
		const float Dx = World.X - Center.X;
		const float Dy = World.Y - Center.Y;
		const float Fx = std::cos(HeadingRadians);
		const float Fy = std::sin(HeadingRadians);
		// Unreal: X forward, Y right when the yaw is zero, so right = (-sin, cos).
		const float Right = Dx * -Fy + Dy * Fx;
		const float Ahead = Dx * Fx + Dy * Fy;
		const float K = PixelsPerCm();
		return {Right * K, -Ahead * K};
	}
};

/** Zoom steps: world radius shown, in cm. */
inline float WorldRadiusForZoom(int Zoom)
{
	switch (Zoom)
	{
	case 0: return 12000.f; // near: 120 m
	case 2: return 40000.f; // far: 400 m
	default: return 22000.f; // medium: 220 m
	}
}

/** Clips the segment A-B (disc-relative) to the disc of the given radius; false when nothing is inside. */
inline bool ClipSegmentToDisc(FVec2& A, FVec2& B, float Radius)
{
	const float Dx = B.X - A.X;
	const float Dy = B.Y - A.Y;
	const float QA = Dx * Dx + Dy * Dy;
	const float R2 = Radius * Radius;
	if (QA < 1e-6f)
	{
		return A.X * A.X + A.Y * A.Y <= R2;
	}
	const float QB = 2.f * (A.X * Dx + A.Y * Dy);
	const float QC = A.X * A.X + A.Y * A.Y - R2;
	const float Disc = QB * QB - 4.f * QA * QC;
	if (Disc <= 0.f)
	{
		return false;
	}
	const float Root = std::sqrt(Disc);
	float T0 = (-QB - Root) / (2.f * QA);
	float T1 = (-QB + Root) / (2.f * QA);
	if (T1 < 0.f || T0 > 1.f)
	{
		return false;
	}
	T0 = std::fmax(T0, 0.f);
	T1 = std::fmin(T1, 1.f);
	const FVec2 NewA{A.X + Dx * T0, A.Y + Dy * T0};
	const FVec2 NewB{A.X + Dx * T1, A.Y + Dy * T1};
	A = NewA;
	B = NewB;
	return true;
}

/**
 * Calls Emit(Kind, WidthM, A, B) for every segment that shows in the disc,
 * clipped to it. Lines whose box lies outside the disc are skipped without
 * looking at their points (the box test is done in world space around the centre).
 */
template <typename EmitFn>
inline int ForEachVisibleSegment(const FMapData& Data, const FMapView& View, EmitFn&& Emit)
{
	const float R = View.WorldRadiusCm;
	const float MinX = View.Center.X - R, MaxX = View.Center.X + R;
	const float MinY = View.Center.Y - R, MaxY = View.Center.Y + R;
	int Count = 0;
	for (const FMapLine& Line : Data.Lines)
	{
		if (Line.MaxX < MinX || Line.MinX > MaxX || Line.MaxY < MinY || Line.MinY > MaxY)
		{
			continue;
		}
		FVec2 Prev = View.ToScreen(Line.Points[0]);
		for (std::size_t i = 1; i < Line.Points.size(); ++i)
		{
			const FVec2 Next = View.ToScreen(Line.Points[i]);
			FVec2 A = Prev, B = Next;
			Prev = Next;
			if (ClipSegmentToDisc(A, B, View.RadiusPixels))
			{
				Emit(Line.Kind, Line.WidthM, A, B);
				++Count;
			}
		}
	}
	return Count;
}
} // namespace GTPUI
