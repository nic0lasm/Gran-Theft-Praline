// The player's money (docs/gameplay/money.md): one balance in cents, payments that can be
// keyed so that the same reward or the same till is never paid twice (a mission retried, a
// save reloaded), and a text form for the save (ADR-0013). Plain C++, tested by
// tools/interiors/core_test without the engine.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <set>
#include <sstream>
#include <string>

namespace GTPWallet
{
	struct FWallet
	{
		int64_t Cents = 0;
		/** Run keys already paid ("act_holdup_shop#3", "holdup#2/till1"...). */
		std::set<std::string> Paid;

		/**
		 * Adds (or takes, when negative) money. With a run key, a second call with the same key
		 * does nothing and returns false. The balance never goes below zero: a payment larger
		 * than the balance is refused (false) and changes nothing.
		 */
		bool Add(int64_t Amount, const std::string& RunKey = std::string())
		{
			if (!RunKey.empty() && Paid.count(RunKey) != 0)
			{
				return false;
			}
			if (Cents + Amount < 0)
			{
				return false;
			}
			Cents += Amount;
			if (!RunKey.empty())
			{
				Paid.insert(RunKey);
			}
			return true;
		}

		bool WasPaid(const std::string& RunKey) const { return Paid.count(RunKey) != 0; }

		/** "cents\nkey\nkey\n...": keys never hold a line break. */
		std::string Save() const
		{
			std::ostringstream Out;
			Out << Cents << '\n';
			for (const std::string& Key : Paid)
			{
				Out << Key << '\n';
			}
			return Out.str();
		}

		bool Load(const std::string& Text)
		{
			std::istringstream In(Text);
			std::string Line;
			if (!std::getline(In, Line) || Line.empty())
			{
				return false;
			}
			char* End = nullptr;
			const long long Value = std::strtoll(Line.c_str(), &End, 10);
			if (End == Line.c_str() || *End != '\0' || Value < 0)
			{
				return false;
			}
			Cents = Value;
			Paid.clear();
			while (std::getline(In, Line))
			{
				if (!Line.empty())
				{
					Paid.insert(Line);
				}
			}
			return true;
		}
	};
}
