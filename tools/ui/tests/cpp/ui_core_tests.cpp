// Checks the Unreal-free core of the interface: string tables, settings and
// mini-map maths. Build and run: tools/ui/tests/run_cpp_tests.sh
#include "GTPControls.h"
#include "GTPGpsCore.h"
#include "GTPMinimapCore.h"
#include "GTPSettingsCore.h"
#include "GTPStringTable.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>

static int Failures = 0;
#define CHECK(cond)                                                                    \
	do                                                                                 \
	{                                                                                  \
		if (!(cond))                                                                   \
		{                                                                              \
			std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                \
			++Failures;                                                                \
		}                                                                              \
	} while (0)

using namespace GTPUI;

static std::set<std::string> Placeholders(const char* Text)
{
	std::set<std::string> Out;
	const std::string S(Text);
	for (std::size_t i = 0; i < S.size(); ++i)
	{
		if (S[i] == '{')
		{
			const std::size_t End = S.find('}', i);
			if (End != std::string::npos)
			{
				Out.insert(S.substr(i, End - i + 1));
			}
		}
	}
	return Out;
}

static void TestStrings()
{
	std::set<std::string> Seen;
	for (const FStringEntry& E : Entries)
	{
		CHECK(Seen.insert(E.Key).second); // no duplicate key
		CHECK(E.French && E.French[0] != '\0');
		CHECK(E.English && E.English[0] != '\0');
		CHECK(Placeholders(E.French) == Placeholders(E.English));
	}
	CHECK(std::string(Find("menu.play", ELanguage::French)) == "Jouer");
	CHECK(std::string(Find("menu.play", ELanguage::English)) == "Play");
	CHECK(std::string(Find("no.such.key", ELanguage::English)) == "no.such.key");
	CHECK(LanguageFromCode("fr-FR") == ELanguage::French);
	CHECK(LanguageFromCode("en_GB") == ELanguage::English);
	CHECK(LanguageFromCode("de") == ELanguage::French);
	// Every key the settings screen builds exists.
	for (const char* Key : {"settings.language", "settings.graphics", "settings.volume", "settings.sensitivity",
			 "settings.invert_y", "settings.keyboard", "settings.minimap", "settings.minimap_zoom", "value.gfx.m1",
			 "value.gfx.fidelity", "value.gfx.ultra", "value.kb.auto", "value.zoom.far"})
	{
		CHECK(FindEntry(Key) != nullptr);
	}
}

static void TestControls()
{
	for (std::size_t i = 0; i < ControlRowCount; ++i)
	{
		CHECK(FindEntry(ControlRows[i].ActionKey) != nullptr);
		CHECK(ControlRows[i].Azerty[0] != '\0' && ControlRows[i].Qwerty[0] != '\0' && ControlRows[i].Gamepad[0] != '\0');
	}
	CHECK(FindEntry("controls.heading") && FindEntry("menu.controls") && FindEntry("controls.keyboard_header"));
}

static void TestSettings()
{
	FUserSettings S;
	CHECK(S.Graphics == "m1" && S.VolumePercent == 100 && S.SensitivityPercent == 100);

	S.CycleGraphics();
	CHECK(S.Graphics == "fidelity");
	S.CycleGraphics();
	S.CycleGraphics();
	CHECK(S.Graphics == "m1"); // wraps around
	S.VolumePercent = 100;
	S.CycleVolume();
	CHECK(S.VolumePercent == 0);
	S.CycleVolume();
	CHECK(S.VolumePercent == 10);
	S.SensitivityPercent = 200;
	S.CycleSensitivity();
	CHECK(S.SensitivityPercent == 25);

	// Round trip.
	FUserSettings T;
	T.Language = ELanguage::English;
	T.Graphics = "ultra";
	T.VolumePercent = 40;
	T.SensitivityPercent = 150;
	T.bInvertY = true;
	T.Keyboard = EKeyboardLayout::Qwerty;
	T.bMinimap = false;
	T.MinimapZoom = EMinimapZoom::Far;
	CHECK(Parse(Serialize(T)) == T);

	// A hand-edited or damaged file never breaks the game.
	const FUserSettings Bad = Parse("volume=900\nsensitivity=-5\ngraphics=potato\nkeyboard=7\nnonsense\n=\nzzz=1\nminimap=abc\n");
	CHECK(Bad.VolumePercent == 100);
	CHECK(Bad.SensitivityPercent == FUserSettings::SensitivityMin);
	CHECK(Bad.Graphics == "m1");
	CHECK(Bad.Keyboard == EKeyboardLayout::Auto);
	CHECK(Bad.bMinimap);
	CHECK(Parse("").VolumePercent == 100);
	CHECK(Parse("# only a comment\r\nvolume = 30 \r\n").VolumePercent == 30);
}

static bool Near(float A, float B, float Eps = 1e-3f) { return std::fabs(A - B) <= Eps; }

static void TestMinimap()
{
	FMapView View;
	View.Center = {1000.f, 2000.f};
	View.RadiusPixels = 100.f;
	View.WorldRadiusCm = 10000.f;

	// Facing east (yaw 0): a point 5000 cm east is straight ahead, half way to the edge.
	View.HeadingRadians = 0.f;
	FVec2 P = View.ToScreen({6000.f, 2000.f});
	CHECK(Near(P.X, 0.f) && Near(P.Y, -50.f));
	// South is the player's right when facing east (Y is south in the map data).
	P = View.ToScreen({1000.f, 7000.f});
	CHECK(Near(P.X, 50.f) && Near(P.Y, 0.f));
	// Facing south (yaw +90 deg): south is ahead, west is on the right.
	View.HeadingRadians = 3.14159265f / 2.f;
	P = View.ToScreen({1000.f, 7000.f});
	CHECK(Near(P.X, 0.f) && Near(P.Y, -50.f));
	P = View.ToScreen({-4000.f, 2000.f});
	CHECK(Near(P.X, 50.f) && Near(P.Y, 0.f));

	// Disc clipping.
	FVec2 A{-200.f, 0.f}, B{200.f, 0.f};
	CHECK(ClipSegmentToDisc(A, B, 100.f) && Near(A.X, -100.f) && Near(B.X, 100.f));
	A = {-50.f, 0.f};
	B = {50.f, 0.f};
	CHECK(ClipSegmentToDisc(A, B, 100.f) && Near(A.X, -50.f) && Near(B.X, 50.f));
	A = {150.f, 150.f};
	B = {300.f, 150.f};
	CHECK(!ClipSegmentToDisc(A, B, 100.f));
	A = {0.f, 0.f};
	B = {500.f, 0.f};
	CHECK(ClipSegmentToDisc(A, B, 100.f) && Near(A.X, 0.f) && Near(B.X, 100.f));

	// CSV: kinds, closed ring, bad rows skipped, culling by box.
	FMapData Data;
	const int Kept = Data.ParseCsv(
		"kind,width_m,points_ue_cm,name\n"
		"road_major,6,0 0;1000 0,Rue Test\n"
		"road_minor,4,0 100;500 100,Rue Test\n"
		"road_minor,4,0 5000;1000 5000,\n"
		"water,0,0 0;100 0;100 100;0 0\n"
		"mystery,1,0 0;1 1\n"
		"path,2,5 5\r\n"
		"building,0,-100 -100;100 -100;100 100;-100 100;-100 -100\n");
	CHECK(Kept == 5);
	CHECK(Data.Lines[0].Kind == EMapKind::RoadMajor && Data.Lines[0].WidthM == 6.f);
	CHECK(Data.Lines[3].Kind == EMapKind::Water);
	CHECK(Data.Lines[0].Name == "Rue Test" && Data.Lines[2].Name.empty());
	// One label per name, at the middle of the longest polyline (the 1000 cm one).
	const std::vector<FMapLabel> Labels = Data.BuildStreetLabels();
	CHECK(Labels.size() == 1 && Labels[0].Text == "Rue Test" && Near(Labels[0].Position.X, 500.f) && Near(Labels[0].Position.Y, 0.f));

	FMapView Near2;
	Near2.Center = {500.f, 0.f};
	Near2.HeadingRadians = 0.f;
	Near2.RadiusPixels = 100.f;
	Near2.WorldRadiusCm = 1500.f;
	int Roads = 0, Others = 0;
	const int Segments = ForEachVisibleSegment(Data, Near2, [&](EMapKind K, float, const FVec2&, const FVec2&) {
		(K == EMapKind::RoadMajor || K == EMapKind::RoadMinor) ? ++Roads : ++Others;
	});
	CHECK(Roads == 2); // the two near roads; the minor road 50 m away is outside
	CHECK(Others > 0);
	CHECK(Segments == Roads + Others);
}

static void TestGps()
{
	// A square block with a diagonal shortcut missing:
	//   A(0,0) --1000-- B(1000,0)
	//     |                |
	//   2000             1000        D is (0,2000), C (1000,1000), E (1000,2000)
	//     |                |
	//   D(0,2000)--1000--E(1000,2000)   and B-C-E on the right
	FMapData Data;
	Data.ParseCsv(
		"kind,width_m,points_ue_cm,name\n"
		"road_major,6,0 0;1000 0,Top\n"
		"road_major,6,1000 0;1000 1000,Right1\n"
		"road_major,6,1000 1000;1000 2000,Right2\n"
		"road_minor,4,0 0;0 2000,Left\n"
		"road_minor,4,0 2000;1000 2000,Bottom\n"
		"water,0,5000 5000;5100 5000\n"
		"road_minor,4,9000 9000;9500 9000,Island\n");
	FGpsGraph Graph;
	Graph.Build(Data);
	CHECK(Graph.RoadCount() == 6);

	std::vector<FVec2> Route;
	float Length = 0.f;
	// Start at the middle of the left road, go to the middle of Right1: via the top, 1000 + 1000 + 500 + 500...
	CHECK(Graph.Route({0.f, 1000.f}, {1000.f, 500.f}, Route, Length));
	// Shortest: up the left road (1000), across the top (1000), down Right1 (500) = 2500;
	// the other way: down 1000, across the bottom 1000, up Right2 1000, up Right1 500 = 3500.
	CHECK(Near(Length, 2500.f, 1.f));
	CHECK(Route.size() >= 4);
	CHECK(Near(Route.front().X, 0.f) && Near(Route.front().Y, 1000.f));
	CHECK(Near(Route.back().X, 1000.f) && Near(Route.back().Y, 500.f));
	CHECK(Near(Route[1].X, 0.f) && Near(Route[1].Y, 0.f)); // goes through the top left corner

	// Same road: straight along it.
	CHECK(Graph.Route({100.f, 0.f}, {800.f, 0.f}, Route, Length));
	CHECK(Near(Length, 700.f, 1.f));

	// A point 30 m off the road is snapped to it and the detour counts in the length.
	CHECK(Graph.Route({-3000.f, 1000.f}, {1000.f, 500.f}, Route, Length));
	CHECK(Near(Length, 2500.f + 3000.f, 1.f));

	// No way between the island road and the block; and nothing near the water.
	CHECK(!Graph.Route({9200.f, 9000.f}, {1000.f, 500.f}, Route, Length, 1e9f));
	CHECK(!Graph.Route({90000.f, 90000.f}, {1000.f, 500.f}, Route, Length));

	FGpsGraph Empty;
	CHECK(!Empty.Route({0, 0}, {1, 1}, Route, Length));
	CHECK(Near(DistanceToPolyline({500.f, 300.f}, {{0.f, 0.f}, {1000.f, 0.f}}), 300.f));
}

int main()
{
	TestStrings();
	TestControls();
	TestSettings();
	TestMinimap();
	TestGps();
	if (Failures == 0)
	{
		std::printf("ui_core_tests: PASS\n");
		return 0;
	}
	std::printf("ui_core_tests: %d failure(s)\n", Failures);
	return 1;
}
