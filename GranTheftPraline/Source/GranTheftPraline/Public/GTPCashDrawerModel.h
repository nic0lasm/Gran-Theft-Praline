// Cash drawer of a till (docs/adr/adr-0012-interiors.md, docs/gameplay/reference-interior.md):
// euro notes and coins counted one by one, a standard opening float, a shift of
// sales where some customers pay cash and get their change back from the drawer,
// and the manager's skim of large notes to the safe. What a robber finds in a till
// therefore depends on the hour and the number of cash sales, never on a random
// total. Plain C++ with its own seeded generator (same result on clang and MSVC),
// so tools/interiors/core_test checks it with g++.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace GTPCash
{
	/** Euro denominations in cents, largest first. 500 euro notes are no longer issued. */
	constexpr int NumDenoms = 14;
	constexpr std::array<int64_t, NumDenoms> Denoms = {
		20000, 10000, 5000, 2000, 1000, 500,       // notes
		200, 100, 50, 20, 10, 5, 2, 1};            // coins
	constexpr int FirstCoin = 6;

	inline bool IsNote(int Index) { return Index < FirstCoin; }

	/** Small deterministic generator (splitmix64): no std distributions, whose output differs between compilers. */
	struct FRng
	{
		uint64_t State = 0x9E3779B97F4A7C15ull;
		explicit FRng(uint64_t Seed) : State(Seed ^ 0x9E3779B97F4A7C15ull) {}
		uint64_t Next()
		{
			uint64_t Z = (State += 0x9E3779B97F4A7C15ull);
			Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
			Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
			return Z ^ (Z >> 31);
		}
		/** Uniform in [0, 1). */
		double Unit() { return double(Next() >> 11) * (1.0 / 9007199254740992.0); }
		int Range(int Count) { return Count <= 0 ? 0 : int(Next() % uint64_t(Count)); }
	};

	struct FDrawer
	{
		/** Number of pieces per denomination, same order as Denoms. Index 0-1 (200, 100) sit under the tray. */
		std::array<int, NumDenoms> Count{};

		int64_t Total() const
		{
			int64_t Sum = 0;
			for (int Index = 0; Index < NumDenoms; ++Index)
			{
				Sum += Denoms[Index] * Count[Index];
			}
			return Sum;
		}
		int64_t NotesTotal() const
		{
			int64_t Sum = 0;
			for (int Index = 0; Index < FirstCoin; ++Index)
			{
				Sum += Denoms[Index] * Count[Index];
			}
			return Sum;
		}
		int Pieces() const
		{
			int Sum = 0;
			for (int Value : Count)
			{
				Sum += Value;
			}
			return Sum;
		}
	};

	/**
	 * Opening float of 150.00 euros, the usual amount for a quick service till in France:
	 * 1 x 20, 4 x 10, 6 x 5, 10 x 2, 20 x 1, 20 x 0.50, 25 x 0.20, 30 x 0.10,
	 * 20 x 0.05, 25 x 0.02, 50 x 0.01 (one roll of each small coin, partly used).
	 */
	inline FDrawer StandardFloat()
	{
		FDrawer Drawer;
		Drawer.Count = {0, 0, 0, 1, 4, 6, 10, 20, 20, 25, 30, 20, 25, 50};
		return Drawer;
	}

	/**
	 * Takes Amount cents out of the drawer as change, largest pieces first, within
	 * what the drawer holds. Returns false and leaves the drawer untouched when the
	 * exact change cannot be made.
	 */
	inline bool GiveChange(FDrawer& Drawer, int64_t Amount)
	{
		FDrawer Work = Drawer;
		for (int Index = 0; Index < NumDenoms && Amount > 0; ++Index)
		{
			while (Amount >= Denoms[Index] && Work.Count[Index] > 0)
			{
				Amount -= Denoms[Index];
				--Work.Count[Index];
			}
		}
		if (Amount != 0)
		{
			return false;
		}
		Drawer = Work;
		return true;
	}

	/** How one customer settles a bill of Price cents in cash: the pieces handed over. */
	inline std::array<int, NumDenoms> Tender(int64_t Price, FRng& Rng)
	{
		std::array<int, NumDenoms> Paid{};
		const double Roll = Rng.Unit();
		if (Roll < 0.25)
		{
			// Exact money, largest pieces first.
			int64_t Rest = Price;
			for (int Index = 2; Index < NumDenoms; ++Index)
			{
				while (Rest >= Denoms[Index])
				{
					Rest -= Denoms[Index];
					++Paid[Index];
				}
			}
			return Paid;
		}
		// Smallest single note that covers the bill (5, 10, 20, 50), or else twenties.
		for (int Index = 5; Index >= 2; --Index)
		{
			if (Denoms[Index] >= Price)
			{
				++Paid[Index];
				return Paid;
			}
		}
		const int Twenties = int((Price + 1999) / 2000);
		Paid[3] = Twenties;
		return Paid;
	}

	struct FShiftParams
	{
		/** Menu prices in cents a basket is drawn from (1 to 3 items). */
		std::vector<int64_t> Prices = {1190, 1050, 890, 1290, 1490, 590, 350, 290, 250, 450, 190, 690};
		/** Share of customers paying cash (card and kiosk take the rest). */
		double CashShare = 0.18;
		/** Customers served by this till per hour. */
		int CustomersPerHour = 45;
		/** Above this many cents in notes, the manager drops notes to the safe. */
		int64_t SkimAbove = 40000;
		/** Notes kept in the till after a skim, in cents (the float notes). */
		int64_t SkimKeep = 10000;
	};

	struct FShiftResult
	{
		FDrawer Drawer;
		int CashSales = 0;
		int CardSales = 0;
		int Skims = 0;
		int64_t Skimmed = 0;
		/** Times the manager brought change from the safe, and its value in cents. */
		int Restocks = 0;
		int64_t Restocked = 0;
		int ChangeFailures = 0;
	};

	/** Drops the largest notes to the safe until at most Keep cents of notes stay. */
	inline int64_t Skim(FDrawer& Drawer, int64_t Keep)
	{
		int64_t Dropped = 0;
		for (int Index = 0; Index < FirstCoin; ++Index)
		{
			while (Drawer.Count[Index] > 0 && Drawer.NotesTotal() - Denoms[Index] >= Keep)
			{
				--Drawer.Count[Index];
				Dropped += Denoms[Index];
			}
		}
		return Dropped;
	}

	/**
	 * Change brought from the safe when the till runs short: one roll of each coin
	 * (25 x 2, 25 x 1, 40 x 0.50, 40 x 0.20, 40 x 0.10, 50 x 0.05, 50 x 0.02, 50 x 0.01)
	 * and five 10 euro notes. Returns its value in cents.
	 */
	inline int64_t Restock(FDrawer& Drawer)
	{
		const std::array<int, NumDenoms> Bring = {0, 0, 0, 0, 5, 0, 25, 25, 40, 40, 40, 50, 50, 50};
		int64_t Value = 0;
		for (int Index = 0; Index < NumDenoms; ++Index)
		{
			Drawer.Count[size_t(Index)] += Bring[size_t(Index)];
			Value += Denoms[size_t(Index)] * Bring[size_t(Index)];
		}
		return Value;
	}

	/** Runs Hours of service on one till, starting from the standard float. */
	inline FShiftResult SimulateShift(uint64_t Seed, double Hours, const FShiftParams& Params = FShiftParams())
	{
		FRng Rng(Seed);
		FShiftResult Result;
		Result.Drawer = StandardFloat();
		const int Customers = int(Hours * Params.CustomersPerHour + 0.5);
		for (int Customer = 0; Customer < Customers; ++Customer)
		{
			int64_t Price = 0;
			const int Items = 1 + Rng.Range(3);
			for (int Item = 0; Item < Items; ++Item)
			{
				Price += Params.Prices[size_t(Rng.Range(int(Params.Prices.size())))];
			}
			if (Rng.Unit() >= Params.CashShare)
			{
				++Result.CardSales;
				continue;
			}
			const std::array<int, NumDenoms> Paid = Tender(Price, Rng);
			int64_t PaidTotal = 0;
			FDrawer After = Result.Drawer;
			for (int Index = 0; Index < NumDenoms; ++Index)
			{
				After.Count[Index] += Paid[Index];
				PaidTotal += Denoms[Index] * Paid[Index];
			}
			bool bChanged = GiveChange(After, PaidTotal - Price);
			if (!bChanged)
			{
				// Short of change: the manager brings a restock from the safe, then retry.
				const int64_t Value = Restock(After);
				++Result.Restocks;
				Result.Restocked += Value;
				bChanged = GiveChange(After, PaidTotal - Price);
			}
			if (!bChanged)
			{
				// No change left for this note: the customer pays by card instead.
				++Result.ChangeFailures;
				++Result.CardSales;
				continue;
			}
			Result.Drawer = After;
			++Result.CashSales;
			if (Result.Drawer.NotesTotal() > Params.SkimAbove)
			{
				Result.Skimmed += Skim(Result.Drawer, Params.SkimKeep);
				++Result.Skims;
			}
		}
		return Result;
	}

	/** What a robber gets: notes always, coins only when they take the time to scoop them. */
	inline int64_t Grab(FDrawer& Drawer, bool bTakeCoins)
	{
		int64_t Taken = 0;
		const int Last = bTakeCoins ? NumDenoms : FirstCoin;
		for (int Index = 0; Index < Last; ++Index)
		{
			Taken += Denoms[Index] * Drawer.Count[Index];
			Drawer.Count[Index] = 0;
		}
		return Taken;
	}
}
