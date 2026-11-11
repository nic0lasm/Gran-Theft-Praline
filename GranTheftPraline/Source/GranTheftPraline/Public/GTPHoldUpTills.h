// Hold-up of a restaurant in the game (step 7 of docs/gameplay/reference-interior.md):
// the fear, hands up, obeying and fleeing of each employee come from the one threat
// model of the game (UGTPThreatResponseComponent, GTPThreatLogic.h); this file only
// adds what belongs to the shop: tills opened by an employee who obeys, cash handed
// over, the silent alarm under the counter, escapes through the back, and the crime
// report. GTPHoldUpModel.h keeps its own fear model for offline stories; this one is
// what the game runs. Plain C++, tested by tools/interiors/core_test without the engine.
#pragma once

#include "GTPCashDrawerModel.h"
#include "GTPHoldUpModel.h"

#include <cstdint>
#include <string>
#include <vector>

namespace GTPHoldUpTills
{
	/** Same order as EGTPThreatState (GTPThreat.h). */
	enum class EThreat : uint8_t
	{
		Calm,
		Alarmed,
		HandsUp,
		Complying,
		Backing,
		Fleeing,
		Down,
	};

	enum class ETillPhase : uint8_t
	{
		Closed,
		Opening,
		HandingOver,
		Emptied,
	};

	enum class EOutcome : uint8_t
	{
		None,
		Escaped,  // the player left with the cash, or without being caught
		Caught,   // arrested before leaving
		Aborted,  // left or gave up without any cash
	};

	inline const char* OutcomeName(EOutcome Outcome)
	{
		switch (Outcome)
		{
		case EOutcome::Escaped: return "escaped";
		case EOutcome::Caught: return "caught";
		case EOutcome::Aborted: return "aborted";
		default: return "none";
		}
	}

	struct FMember
	{
		/** Till this employee works, -1 for none. */
		int Till = -1;
		/** The silent alarm is under the counter: only counter staff reach it. */
		bool bCanReachAlarm = false;
		/** Inputs refreshed by the game before each step. */
		EThreat State = EThreat::Calm;
		double SecondsSinceAimed = 1e9;
		bool bReachedExit = false;
		/** Kept by the run. */
		bool bEscaped = false;
	};

	struct FTill
	{
		GTPCash::FDrawer Drawer;
		ETillPhase Phase = ETillPhase::Closed;
		double Timer = 0.0;
		int Staff = -1;
	};

	struct FParams
	{
		double OpenTillSeconds = 2.5;
		double HandOverSeconds = 4.0;
		double ActAfterUncovered = 3.0;      // seconds not covered before an employee at the counter presses the alarm
		double AlarmToReportSeconds = 20.0;  // monitoring centre calls the police
		bool bRobberTakesCoins = false;
	};

	struct FRun
	{
		std::vector<FMember> Staff;
		std::vector<FTill> Tills;
		FParams Params;
		double Time = 0.0;
		double AlarmTime = -1.0;
		bool bActive = false;
		bool bCrimeReported = false;
		EOutcome Outcome = EOutcome::None;
		int64_t Loot = 0;
		int TillsEmptied = 0;
		std::vector<GTPHoldUp::FEventRecord> Events;

		/** Tills filled like a shop in the middle of a service (seeded, so the same day gives the same drawers). */
		void Begin(int TillCount, uint64_t Seed, double ServiceHours = 4.0)
		{
			Tills.assign(size_t(TillCount < 0 ? 0 : TillCount), FTill());
			for (int Index = 0; Index < int(Tills.size()); ++Index)
			{
				Tills[size_t(Index)].Drawer = GTPCash::SimulateShift(Seed + uint64_t(Index) * 7919u, ServiceHours, GTPCash::FShiftParams()).Drawer;
			}
		}

		/** One step. bThreat: someone armed threatens the people of this shop right now. */
		void Step(double Dt, bool bThreat)
		{
			Time += Dt;
			if (Outcome != EOutcome::None)
			{
				return;
			}
			if (!bActive && bThreat)
			{
				for (const FMember& Member : Staff)
				{
					if (Member.State != EThreat::Calm)
					{
						bActive = true;
						Emit(GTPHoldUp::EEvent::Alerted, -1);
						break;
					}
				}
			}
			if (!bActive)
			{
				return;
			}
			for (int Index = 0; Index < int(Staff.size()); ++Index)
			{
				StepMember(Index, Dt);
			}
			for (FTill& Till : Tills)
			{
				StepTill(Till, Dt);
			}
			if (AlarmTime >= 0.0 && Time - AlarmTime >= Params.AlarmToReportSeconds)
			{
				ReportCrime();
			}
		}

		/** The hold-up ends: the player left the shop (bArrested false) or was arrested. */
		EOutcome End(bool bArrested)
		{
			if (!bActive || Outcome != EOutcome::None)
			{
				return EOutcome::None;
			}
			Outcome = bArrested ? EOutcome::Caught : (Loot > 0 ? EOutcome::Escaped : EOutcome::Aborted);
			return Outcome;
		}

		int64_t CashLeft() const
		{
			int64_t Total = 0;
			for (const FTill& Till : Tills)
			{
				Total += Till.Drawer.Total();
			}
			return Total;
		}

	private:
		void Emit(GTPHoldUp::EEvent Kind, int StaffIndex, int64_t Cents = 0) { Events.push_back({Kind, StaffIndex, Time, Cents}); }

		void ReportCrime()
		{
			if (!bCrimeReported)
			{
				bCrimeReported = true;
				Emit(GTPHoldUp::EEvent::CrimeReported, -1);
			}
		}

		static bool Obeys(EThreat State) { return State == EThreat::Complying; }
		static bool Inside(EThreat State) { return State != EThreat::Fleeing && State != EThreat::Down; }

		void StepMember(int Index, double /*Dt*/)
		{
			FMember& Member = Staff[size_t(Index)];
			if (Member.bEscaped)
			{
				return;
			}
			if (Member.bReachedExit)
			{
				Member.bEscaped = true;
				Emit(GTPHoldUp::EEvent::StaffEscaped, Index);
				ReportCrime();  // they call the police from the street
				return;
			}
			if (Member.Till >= 0 && Member.Till < int(Tills.size()) && Obeys(Member.State))
			{
				FTill& Till = Tills[size_t(Member.Till)];
				if (Till.Phase == ETillPhase::Closed)
				{
					Till.Phase = ETillPhase::Opening;
					Till.Timer = Params.OpenTillSeconds;
					Till.Staff = Index;
				}
			}
			// Nobody has covered them for a while and they are still at the counter: the alarm.
			if (Member.bCanReachAlarm && AlarmTime < 0.0 && Inside(Member.State) && Member.State != EThreat::Calm &&
			    Member.SecondsSinceAimed >= Params.ActAfterUncovered)
			{
				AlarmTime = Time;
				Emit(GTPHoldUp::EEvent::AlarmPressed, Index);
			}
		}

		void StepTill(FTill& Till, double Dt)
		{
			if (Till.Phase != ETillPhase::Opening && Till.Phase != ETillPhase::HandingOver)
			{
				return;
			}
			const FMember& Member = Staff[size_t(Till.Staff)];
			if (!Obeys(Member.State) && Member.State != EThreat::HandsUp)
			{
				// stopped obeying (fled, fell, or calmed down): the till stays as it is
				Till.Phase = Till.Phase == ETillPhase::Opening ? ETillPhase::Closed : ETillPhase::HandingOver;
				if (Till.Phase == ETillPhase::Closed)
				{
					Till.Staff = -1;
					return;
				}
				if (!Inside(Member.State))
				{
					return;  // open till, nobody hands the cash: the player can come back to it later (step 8)
				}
			}
			Till.Timer -= Dt;
			if (Till.Timer > 0.0)
			{
				return;
			}
			if (Till.Phase == ETillPhase::Opening)
			{
				Till.Phase = ETillPhase::HandingOver;
				Till.Timer = Params.HandOverSeconds;
				Emit(GTPHoldUp::EEvent::TillOpened, Till.Staff);
				return;
			}
			const int64_t Cash = GTPCash::Grab(Till.Drawer, Params.bRobberTakesCoins);
			Loot += Cash;
			++TillsEmptied;
			Till.Phase = ETillPhase::Emptied;
			Emit(GTPHoldUp::EEvent::CashHandedOver, Till.Staff, Cash);
		}
	};
}
