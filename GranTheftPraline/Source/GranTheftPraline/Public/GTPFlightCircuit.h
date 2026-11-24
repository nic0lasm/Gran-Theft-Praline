#pragma once

// The circuit of a flight: one closed way on the lanes of the map, chosen once outside the game and kept in Git
// (Content/Test/Circuits/<Map>.circuit.csv, tools/city/flight_circuit.py), that the chase script of -GTARun and
// -PoliceChaseDemo follows lap after lap (GTPDemoRoute.cpp). Written without engine types so it can be tested
// outside Unreal (tools/vehicles/wanted_sim/flight_circuit_test.cpp). Units: metres.
//
// Until gate 78 the flight was free on the 235 lanes of L_City_RueDeLaRepublique: at each junction the exit with the
// best way on. Every wall, post or pit of the whole centre could hold it up, and the script put the car back on a lane
// 4 to 8 times in the four filmed minutes of gates 76 to 78. A flight on a circuit drives lanes that were looked at
// beforehand, and what the camera sees from them is what gets dressed.
//
// A row of the file names a lane by where it starts and ends, its number in the lanes file being only a hint: the
// lanes file is made again by tools/city/traffic.py when the city changes, and its numbers move.
//
// A row also carries the line driven on its lane. The lanes are road centre lines pushed aside: their points wander
// by a metre and a lane starts a metre or two beside the end of the one before. The drivers read each such step as
// a bend (GTPDriveLogic.h: a step of 15 degrees is worth 42 km/h): three laps driven on the raw lanes with the game's
// decisions average 40 km/h and never pass 74, whatever speed is asked. The line is one closed curve over the lap,
// straights straightened and corners rounded (tools/city/flight_circuit.py); on it the same laps average 52 to 56
// over their first minute and reach 100 (flight_circuit_test.cpp, a model of a car, not the game's).
//
// The file may start with an approach: the lanes driven from where the flight starts to the lap, in order, under a
// line that reads "approach" (the lap's rows then follow a line that reads "lap"). Without one a flight that starts
// off the lap takes at each junction the exit that leads to the lap by the fewest metres: from the theft of -GTARun
// on the quai Jean Moulin that is a U-turn at the first junction, when the lane beside goes straight up the quay to
// the lap.

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

namespace GTPFlightCircuit
{
	/** A row matches a lane whose two ends are both this near the row's (m). */
	constexpr float MatchMetres = 3.f;

	/** A point of a driving line (m). */
	struct FLinePoint
	{
		float X = 0.f;
		float Y = 0.f;
	};

	/** One row of the circuit file. */
	struct FRow
	{
		int LaneHint = -1;
		float StartX = 0.f;
		float StartY = 0.f;
		float EndX = 0.f;
		float EndY = 0.f;
		/** The line driven on the lane; empty when the row has none. */
		std::vector<FLinePoint> Line;
		/** A row of the approach (the lanes driven to the lap), not of the lap. */
		bool bApproach = false;
	};

	/** A driving line starts and ends this near its lane's ends, and the next line starts this near its end (m). */
	constexpr float LineEndsMetres = 8.f;

	/** The two ends of a lane of the network (m). */
	struct FEnds
	{
		float StartX = 0.f;
		float StartY = 0.f;
		float EndX = 0.f;
		float EndY = 0.f;
	};

	/**
	 * The rows of a circuit file, in lap order: "lane_id,start_x_cm,start_y_cm,end_x_cm,end_y_cm[,name[,line_ue_cm]]"
	 * (Unreal cm, as the lanes file; the line as the lanes file writes points, "x y;x y;..."). Lines that start with '#'
	 * and lines whose first field is not a number (the header) are skipped. The rows after a line that reads
	 * "approach" are the approach's, those after a line that reads "lap" the lap's.
	 */
	inline std::vector<FRow> ParseRows(const std::string& Text)
	{
		std::vector<FRow> Rows;
		bool bInApproach = false;
		std::size_t At = 0;
		while (At < Text.size())
		{
			std::size_t End = Text.find('\n', At);
			End = End == std::string::npos ? Text.size() : End;
			const std::string Line = Text.substr(At, End - At);
			At = End + 1;
			std::vector<std::string> Fields(1);
			for (const char Char : Line)
			{
				if (Char == ',')
				{
					Fields.emplace_back();
				}
				else if (Char != '\r')
				{
					Fields.back().push_back(Char);
				}
			}
			if (Fields.size() == 1 && (Fields[0] == "approach" || Fields[0] == "lap"))
			{
				bInApproach = Fields[0] == "approach";
				continue;
			}
			if (Fields.size() < 5 || Fields[0].empty() || Fields[0][0] == '#')
			{
				continue;
			}
			char* Rest = nullptr;
			const long Hint = std::strtol(Fields[0].c_str(), &Rest, 10);
			while (Rest && (*Rest == ' ' || *Rest == '\t'))
			{
				++Rest;
			}
			if (Rest == Fields[0].c_str() || *Rest != '\0')
			{
				continue;
			}
			FRow Row;
			Row.bApproach = bInApproach;
			Row.LaneHint = static_cast<int>(Hint);
			Row.StartX = std::strtof(Fields[1].c_str(), nullptr) / 100.f;
			Row.StartY = std::strtof(Fields[2].c_str(), nullptr) / 100.f;
			Row.EndX = std::strtof(Fields[3].c_str(), nullptr) / 100.f;
			Row.EndY = std::strtof(Fields[4].c_str(), nullptr) / 100.f;
			if (Fields.size() >= 7)
			{
				const char* Cursor = Fields[6].c_str();
				while (*Cursor != '\0')
				{
					char* AfterX = nullptr;
					const float X = std::strtof(Cursor, &AfterX);
					if (AfterX == Cursor)
					{
						break;
					}
					char* AfterY = nullptr;
					const float Y = std::strtof(AfterX, &AfterY);
					if (AfterY == AfterX)
					{
						break;
					}
					Row.Line.push_back({X / 100.f, Y / 100.f});
					Cursor = AfterY;
					while (*Cursor == ' ' || *Cursor == ';' || *Cursor == '\t')
					{
						++Cursor;
					}
				}
			}
			Rows.push_back(Row);
		}
		return Rows;
	}

	/** How far the ends of a lane are from a row's: the larger of the two gaps (m). */
	inline float EndsGap(const FRow& Row, const FEnds& Lane)
	{
		const float Start = std::hypot(Lane.StartX - Row.StartX, Lane.StartY - Row.StartY);
		const float End = std::hypot(Lane.EndX - Row.EndX, Lane.EndY - Row.EndY);
		return Start > End ? Start : End;
	}

	/**
	 * The lane of each row: the hinted one when its ends match, else the lane whose ends are nearest, within
	 * MatchMetres. False, with the reason, when a row has no lane (the lanes file is another one, or was made again
	 * with other roads). OutRenumbered: rows whose lane is not the hinted one.
	 */
	inline bool Resolve(const std::vector<FRow>& Rows, const std::vector<FEnds>& Lanes, std::vector<int>& OutLanes, int& OutRenumbered, std::string& OutError)
	{
		OutLanes.clear();
		OutRenumbered = 0;
		for (std::size_t RowIndex = 0; RowIndex < Rows.size(); ++RowIndex)
		{
			const FRow& Row = Rows[RowIndex];
			int Found = -1;
			if (Row.LaneHint >= 0 && static_cast<std::size_t>(Row.LaneHint) < Lanes.size() && EndsGap(Row, Lanes[static_cast<std::size_t>(Row.LaneHint)]) <= MatchMetres)
			{
				Found = Row.LaneHint;
			}
			else
			{
				float Best = MatchMetres;
				for (std::size_t Lane = 0; Lane < Lanes.size(); ++Lane)
				{
					const float Gap = EndsGap(Row, Lanes[Lane]);
					if (Gap <= Best)
					{
						Best = Gap;
						Found = static_cast<int>(Lane);
					}
				}
				OutRenumbered += Found >= 0 ? 1 : 0;
			}
			if (Found < 0)
			{
				OutError = "row " + std::to_string(RowIndex + 1) + " (lane " + std::to_string(Row.LaneHint) + " of the file it was made from): no lane of the network starts and ends there";
				OutLanes.clear();
				return false;
			}
			OutLanes.push_back(Found);
		}
		return true;
	}

	struct FCircuit
	{
		/** The lanes of one lap, in the order driven; the last leads to the first. */
		std::vector<int> Lanes;
		/** For each lane of the network: its place in the lap, -1 off it. */
		std::vector<int> IndexOf;
		/** For each lane off the lap: the exit to take to reach the lap by the shortest way, -1 when no way leads to it. */
		std::vector<int> Toward;
		/** For each lane: metres to drive after its end before being on the lap; 0 on the lap, negative when no way. */
		std::vector<float> TowardMetres;
		float LapMetres = 0.f;
		/** The line driven on each lane of the lap, in lap order (SetLines); empty: the lanes' own points are driven. */
		std::vector<std::vector<FLinePoint>> Lines;
		/** The lanes driven to the lap from where the flight starts, in order (SetApproach); the last leads to a lane of the lap. */
		std::vector<int> Approach;
		/** The line driven on each lane of the approach (SetApproachLines); empty: the lanes' own points are driven. */
		std::vector<std::vector<FLinePoint>> ApproachLines;

		int LinePoints() const
		{
			std::size_t Count = 0;
			for (const std::vector<FLinePoint>& Line : Lines)
			{
				Count += Line.size();
			}
			for (const std::vector<FLinePoint>& Line : ApproachLines)
			{
				Count += Line.size();
			}
			return static_cast<int>(Count);
		}

		bool IsValid() const { return !Lanes.empty(); }

		bool IsOn(int Lane) const
		{
			return Lane >= 0 && static_cast<std::size_t>(Lane) < IndexOf.size() && IndexOf[static_cast<std::size_t>(Lane)] >= 0;
		}

		/** Place of Lane in the approach, -1 when it is not of it. */
		int ApproachPlace(int Lane) const
		{
			for (std::size_t Place = 0; Place < Approach.size(); ++Place)
			{
				if (Approach[Place] == Lane)
				{
					return static_cast<int>(Place);
				}
			}
			return -1;
		}

		/** Lane is of the lap or of the approach: a lane that was looked at beforehand. */
		bool IsKnown(int Lane) const { return IsOn(Lane) || ApproachPlace(Lane) >= 0; }

		/** The line driven on Lane (lap or approach); null when it has none: its own points are driven. */
		const std::vector<FLinePoint>* LineOf(int Lane) const
		{
			if (IsOn(Lane))
			{
				const std::size_t Place = static_cast<std::size_t>(IndexOf[static_cast<std::size_t>(Lane)]);
				return Place < Lines.size() ? &Lines[Place] : nullptr;
			}
			const int Place = ApproachPlace(Lane);
			return Place >= 0 && static_cast<std::size_t>(Place) < ApproachLines.size() ? &ApproachLines[static_cast<std::size_t>(Place)] : nullptr;
		}

		/** The lane to take after Lane: the next of the lap, or off the lap the exit that leads to it; -1 when there is none. */
		int Next(int Lane) const
		{
			if (Lane < 0 || static_cast<std::size_t>(Lane) >= IndexOf.size() || Lanes.empty())
			{
				return -1;
			}
			const int Place = IndexOf[static_cast<std::size_t>(Lane)];
			if (Place >= 0)
			{
				return Lanes[(static_cast<std::size_t>(Place) + 1) % Lanes.size()];
			}
			return Toward[static_cast<std::size_t>(Lane)];
		}

		/** Lanes of the network from which the lap is reached (those of the lap included). */
		int Reaching() const
		{
			int Count = 0;
			for (const float Metres : TowardMetres)
			{
				Count += Metres >= 0.f ? 1 : 0;
			}
			return Count;
		}
	};

	/**
	 * The circuit of a lap on a network of LaneCount lanes. NextOf(Lane): the lanes it leads to; LengthOf(Lane): metres.
	 * False, with the reason, when the lap is not one: under three lanes, a lane that is not of the network or comes
	 * twice, a lane that does not lead to the next one (the last to the first).
	 */
	template <typename FNextOf, typename FLengthOf>
	bool Build(int LaneCount, const std::vector<int>& Lap, const FNextOf& NextOf, const FLengthOf& LengthOf, FCircuit& Out, std::string& OutError)
	{
		Out = FCircuit();
		const std::size_t Count = LaneCount > 0 ? static_cast<std::size_t>(LaneCount) : 0;
		if (Lap.size() < 3)
		{
			OutError = "a lap needs three lanes or more, this one has " + std::to_string(Lap.size());
			return false;
		}
		std::vector<int> IndexOf(Count, -1);
		for (std::size_t Place = 0; Place < Lap.size(); ++Place)
		{
			const int Lane = Lap[Place];
			if (Lane < 0 || static_cast<std::size_t>(Lane) >= Count)
			{
				OutError = "lane " + std::to_string(Lane) + " is not of the network";
				return false;
			}
			if (IndexOf[static_cast<std::size_t>(Lane)] >= 0)
			{
				OutError = "lane " + std::to_string(Lane) + " comes twice in the lap";
				return false;
			}
			IndexOf[static_cast<std::size_t>(Lane)] = static_cast<int>(Place);
		}
		float LapMetres = 0.f;
		for (std::size_t Place = 0; Place < Lap.size(); ++Place)
		{
			const int Lane = Lap[Place];
			const int After = Lap[(Place + 1) % Lap.size()];
			bool bLeads = false;
			for (const int Next : NextOf(Lane))
			{
				bLeads = bLeads || Next == After;
			}
			if (!bLeads)
			{
				OutError = "lane " + std::to_string(Lane) + " does not lead to lane " + std::to_string(After);
				return false;
			}
			LapMetres += LengthOf(Lane);
		}
		// The shortest way to the lap from every other lane: metres after the lane's end, relaxed until nothing changes
		// (a few hundred lanes).
		std::vector<int> Toward(Count, -1);
		std::vector<float> Metres(Count, -1.f);
		for (const int Lane : Lap)
		{
			Metres[static_cast<std::size_t>(Lane)] = 0.f;
		}
		bool bChanged = true;
		for (std::size_t Pass = 0; bChanged && Pass <= Count; ++Pass)
		{
			bChanged = false;
			for (std::size_t Lane = 0; Lane < Count; ++Lane)
			{
				if (IndexOf[Lane] >= 0)
				{
					continue;
				}
				for (const int Next : NextOf(static_cast<int>(Lane)))
				{
					if (Next < 0 || static_cast<std::size_t>(Next) >= Count || Metres[static_cast<std::size_t>(Next)] < 0.f)
					{
						continue;
					}
					const float Via = IndexOf[static_cast<std::size_t>(Next)] >= 0 ? 0.f : LengthOf(Next) + Metres[static_cast<std::size_t>(Next)];
					if (Metres[Lane] < 0.f || Via < Metres[Lane] - 0.01f)
					{
						Metres[Lane] = Via;
						Toward[Lane] = Next;
						bChanged = true;
					}
				}
			}
		}
		Out.Lanes = Lap;
		Out.IndexOf = IndexOf;
		Out.Toward = Toward;
		Out.TowardMetres = Metres;
		Out.LapMetres = LapMetres;
		return true;
	}

	/**
	 * Gives the circuit the driving lines of its rows (Rows in lap order, as Resolve read them; Ends: the lanes of the
	 * network). False, with the reason, and no line kept, when a row has none of two points or more, when a line does
	 * not start and end within LineEndsMetres of its lane's ends, or when the next line does not start within
	 * LineEndsMetres of its end: the lanes' own points are then driven.
	 */
	inline bool SetLines(FCircuit& Circuit, const std::vector<FRow>& Rows, const std::vector<FEnds>& Ends, std::string& OutError)
	{
		Circuit.Lines.clear();
		if (Rows.size() != Circuit.Lanes.size())
		{
			OutError = "as many driving lines as lanes are needed";
			return false;
		}
		for (std::size_t Place = 0; Place < Rows.size(); ++Place)
		{
			const std::vector<FLinePoint>& Line = Rows[Place].Line;
			const std::vector<FLinePoint>& After = Rows[(Place + 1) % Rows.size()].Line;
			const int Lane = Circuit.Lanes[Place];
			if (Line.size() < 2 || After.size() < 2)
			{
				OutError = "lane " + std::to_string(Lane) + " or the next has no driving line of two points or more";
				return false;
			}
			const FEnds& Of = Ends[static_cast<std::size_t>(Lane)];
			if (std::hypot(Line.front().X - Of.StartX, Line.front().Y - Of.StartY) > LineEndsMetres
				|| std::hypot(Line.back().X - Of.EndX, Line.back().Y - Of.EndY) > LineEndsMetres)
			{
				OutError = "the driving line of lane " + std::to_string(Lane) + " does not start and end where the lane does";
				return false;
			}
			if (std::hypot(After.front().X - Line.back().X, After.front().Y - Line.back().Y) > LineEndsMetres)
			{
				OutError = "the driving line after lane " + std::to_string(Lane) + " does not start where that one ends";
				return false;
			}
		}
		for (const FRow& Row : Rows)
		{
			Circuit.Lines.push_back(Row.Line);
		}
		return true;
	}

	/**
	 * Gives the circuit its approach: lanes off the lap, each leading to the next and the last to a lane of the lap
	 * (NextOf as for Build). From them the way to the lap is the approach, not the shortest one. False, with the reason,
	 * and no approach kept, when it is not one.
	 */
	template <typename FNextOf>
	bool SetApproach(FCircuit& Circuit, const std::vector<int>& Approach, const FNextOf& NextOf, std::string& OutError)
	{
		Circuit.Approach.clear();
		Circuit.ApproachLines.clear();
		std::vector<int> Exits;
		for (std::size_t Place = 0; Place < Approach.size(); ++Place)
		{
			const int Lane = Approach[Place];
			if (Lane < 0 || static_cast<std::size_t>(Lane) >= Circuit.IndexOf.size() || Circuit.IsOn(Lane))
			{
				OutError = "lane " + std::to_string(Lane) + " of the approach is of the lap or not of the network";
				return false;
			}
			for (std::size_t Before = 0; Before < Place; ++Before)
			{
				if (Approach[Before] == Lane)
				{
					OutError = "lane " + std::to_string(Lane) + " comes twice in the approach";
					return false;
				}
			}
			int Exit = -1;
			for (const int Next : NextOf(Lane))
			{
				const bool bWanted = Place + 1 < Approach.size() ? Next == Approach[Place + 1] : Circuit.IsOn(Next);
				Exit = Exit < 0 && bWanted ? Next : Exit;
			}
			if (Exit < 0)
			{
				OutError = "lane " + std::to_string(Lane) + " of the approach does not lead to " + (Place + 1 < Approach.size() ? "lane " + std::to_string(Approach[Place + 1]) : std::string("the lap"));
				return false;
			}
			Exits.push_back(Exit);
		}
		for (std::size_t Place = 0; Place < Approach.size(); ++Place)
		{
			Circuit.Toward[static_cast<std::size_t>(Approach[Place])] = Exits[Place];
		}
		Circuit.Approach = Approach;
		return true;
	}

	/**
	 * Gives the approach its driving lines (Rows: the approach's, in order), as SetLines gives the lap its own: the
	 * last one ends where the line of the lap's lane it leads to starts. False, with the reason, and no line kept, when
	 * the lap has no lines or a line does not fit.
	 */
	inline bool SetApproachLines(FCircuit& Circuit, const std::vector<FRow>& Rows, const std::vector<FEnds>& Ends, std::string& OutError)
	{
		Circuit.ApproachLines.clear();
		if (Rows.size() != Circuit.Approach.size() || Rows.empty() || Circuit.Lines.empty())
		{
			OutError = "the approach has lines only with the lap, one a lane";
			return false;
		}
		for (std::size_t Place = 0; Place < Rows.size(); ++Place)
		{
			const int Lane = Circuit.Approach[Place];
			const std::vector<FLinePoint>& Line = Rows[Place].Line;
			const std::vector<FLinePoint>* After = Place + 1 < Rows.size() ? &Rows[Place + 1].Line : Circuit.LineOf(Circuit.Toward[static_cast<std::size_t>(Lane)]);
			if (Line.size() < 2 || !After || After->size() < 2)
			{
				OutError = "lane " + std::to_string(Lane) + " of the approach or the next has no driving line of two points or more";
				return false;
			}
			const FEnds& Of = Ends[static_cast<std::size_t>(Lane)];
			if (std::hypot(Line.front().X - Of.StartX, Line.front().Y - Of.StartY) > LineEndsMetres
				|| std::hypot(Line.back().X - Of.EndX, Line.back().Y - Of.EndY) > LineEndsMetres)
			{
				OutError = "the driving line of lane " + std::to_string(Lane) + " of the approach does not start and end where the lane does";
				return false;
			}
			if (std::hypot(After->front().X - Line.back().X, After->front().Y - Line.back().Y) > LineEndsMetres)
			{
				OutError = "the driving line after lane " + std::to_string(Lane) + " of the approach does not start where that one ends";
				return false;
			}
		}
		for (const FRow& Row : Rows)
		{
			Circuit.ApproachLines.push_back(Row.Line);
		}
		return true;
	}

	/** A carriageway this wide or more is a quay or a boulevard: several lanes each way, long sight lines (m). */
	constexpr float WideMetres = 12.f;

	/**
	 * Speed of a flight on a clear lane of the lap or of the approach (km/h). Those lanes were looked at beforehand: 100 on a quay or a
	 * boulevard (WideMetres and more), 75 on another main axis (7 m and more), 60 in a middle street or on a road of
	 * unknown width, 45 in a narrow street. Off the lap a flight keeps GTPRoadWidth::FlightKmh (70, 60, 45). Bends,
	 * obstacles and the end of the known path lower it further (GTPDriveLogic.h).
	 */
	inline float CruiseKmh(float CarriagewayMetres)
	{
		if (!(CarriagewayMetres > 0.f))
		{
			return 60.f;
		}
		return CarriagewayMetres >= WideMetres ? 100.f : (CarriagewayMetres >= 7.f ? 75.f : (CarriagewayMetres >= 5.f ? 60.f : 45.f));
	}
}
