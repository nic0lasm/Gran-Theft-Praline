// Customers queuing at the tills of a restaurant (step 6 of docs/gameplay/reference-interior.md).
// One line per till, front first. Every few seconds the front customer of a line is served and
// leaves, the others step forward, and a newcomer joins the shortest line every few seconds
// while the shop holds fewer than MaxPeople. While frozen (a hold-up), nobody is served and
// nobody comes in. Plain C++, tested by tools/interiors/core_test without the engine; UGTPCustomerSubsystem lends pool pedestrians
// for the customers and moves them between the spots of the level.
#pragma once

#include <cstdint>
#include <vector>

namespace GTPCustomers
{
	enum class ECustomerEvent : uint8_t
	{
		Arrived,   // Id joins Line at Index
		Moved,     // Id steps forward to Index
		Served,    // Id leaves (served)
	};

	struct FEvent
	{
		ECustomerEvent Kind = ECustomerEvent::Arrived;
		int Id = 0;
		int Line = 0;
		int Index = 0;
	};

	struct FParams
	{
		double ServeMin = 12.0;   // seconds at the till, front customer
		double ServeMax = 24.0;
		double ArriveMin = 3.0;   // seconds between two newcomers while there is room
		double ArriveMax = 7.0;
	};

	struct FLine
	{
		int Spots = 0;
		std::vector<int> People;   // ids, front first
		double Timer = 0.0;
	};

	struct FRun
	{
		FParams Params;
		std::vector<FLine> Lines;
		std::vector<FEvent> Events;
		int MaxPeople = 0;
		int NextId = 1;
		int ServedCount = 0;
		double ArriveTimer = 0.0;
		uint64_t Rng = 1;

		double Random()
		{
			Rng = Rng * 6364136223846793005ull + 1442695040888963407ull;
			return double(Rng >> 11) * (1.0 / 9007199254740992.0);
		}

		int Count() const
		{
			int Total = 0;
			for (const FLine& Line : Lines)
			{
				Total += int(Line.People.size());
			}
			return Total;
		}

		/** Lines of the given number of spots; MaxPeople customers placed front first, spread over the lines. */
		void Begin(const std::vector<int>& SpotsPerLine, int InMaxPeople, uint64_t Seed)
		{
			Lines.clear();
			Events.clear();
			Rng = Seed * 2654435761ull + 1;
			int Capacity = 0;
			for (int Spots : SpotsPerLine)
			{
				FLine Line;
				Line.Spots = Spots > 0 ? Spots : 0;
				Lines.push_back(Line);
				Capacity += Line.Spots;
			}
			MaxPeople = InMaxPeople < Capacity ? InMaxPeople : Capacity;
			if (MaxPeople < 0)
			{
				MaxPeople = 0;
			}
			for (FLine& Line : Lines)
			{
				Line.Timer = Params.ServeMin + Random() * (Params.ServeMax - Params.ServeMin);
			}
			while (Count() < MaxPeople && Join())
			{
			}
			ArriveTimer = Params.ArriveMin + Random() * (Params.ArriveMax - Params.ArriveMin);
		}

		/** A newcomer at the back of the shortest line with room; false when every line is full. */
		bool Join()
		{
			int Best = -1;
			for (int Index = 0; Index < int(Lines.size()); ++Index)
			{
				const FLine& Line = Lines[size_t(Index)];
				if (int(Line.People.size()) < Line.Spots && (Best < 0 || Line.People.size() < Lines[size_t(Best)].People.size()))
				{
					Best = Index;
				}
			}
			if (Best < 0)
			{
				return false;
			}
			FLine& Line = Lines[size_t(Best)];
			Line.People.push_back(NextId);
			Events.push_back({ECustomerEvent::Arrived, NextId, Best, int(Line.People.size()) - 1});
			++NextId;
			return true;
		}

		void Step(double Dt, bool bFrozen)
		{
			if (bFrozen)
			{
				return;
			}
			for (int Index = 0; Index < int(Lines.size()); ++Index)
			{
				FLine& Line = Lines[size_t(Index)];
				Line.Timer -= Dt;
				if (Line.Timer > 0.0)
				{
					continue;
				}
				Line.Timer = Params.ServeMin + Random() * (Params.ServeMax - Params.ServeMin);
				if (Line.People.empty())
				{
					continue;
				}
				Events.push_back({ECustomerEvent::Served, Line.People.front(), Index, 0});
				Line.People.erase(Line.People.begin());
				++ServedCount;
				for (int Place = 0; Place < int(Line.People.size()); ++Place)
				{
					Events.push_back({ECustomerEvent::Moved, Line.People[size_t(Place)], Index, Place});
				}
			}
			ArriveTimer -= Dt;
			if (ArriveTimer <= 0.0)
			{
				ArriveTimer = Params.ArriveMin + Random() * (Params.ArriveMax - Params.ArriveMin);
				if (Count() < MaxPeople)
				{
					Join();
				}
			}
		}
	};
}
