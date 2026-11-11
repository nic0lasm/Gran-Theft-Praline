// Hold-up of several employees (docs/gameplay/reference-interior.md section 6):
// each employee has a fear level fed by what the player does (aim at them, show a
// weapon, shout), and moves through Working -> Alerted -> HandsUp -> OpeningTill ->
// HandingOver. Those the player is not covering may cower, slip out of the back
// door or press the silent alarm under the counter; the alarm and any escape end
// up as a reported crime, which the game sends to UGTPWantedComponent::ReportCrime.
// Plain C++, seeded and stepped at a fixed rate, so tools/interiors/core_test
// replays a hold-up without the engine and the same inputs always give the same story.
#pragma once

#include "GTPCashDrawerModel.h"

#include <cstdint>
#include <vector>

namespace GTPHoldUp
{
	enum class EState : uint8_t
	{
		Working,
		Alerted,
		HandsUp,
		OpeningTill,
		HandingOver,
		Cowering,
		Fleeing,
		Gone,
	};

	enum class ERole : uint8_t
	{
		Crew,     // kitchen and assembly
		Cashier,  // owns a till
		Manager,  // owns the safe key, more likely to press the alarm
	};

	enum class EEvent : uint8_t
	{
		Alerted,
		HandsUp,
		TillOpened,
		CashHandedOver,
		AlarmPressed,
		StaffEscaped,
		CrimeReported,
	};

	struct FEventRecord
	{
		EEvent Kind;
		int Staff;        // -1 when not about one employee
		double Time;
		int64_t Cents;    // cash handed over, else 0
	};

	struct FStaff
	{
		ERole Role = ERole::Crew;
		EState State = EState::Working;
		/** 0 = calm, 1 = terrified. */
		double Fear = 0.0;
		/** 0..1, drawn from the seed: brave staff act when not covered. */
		double Courage = 0.3;
		/** Seconds left in the current timed state. */
		double Timer = 0.0;
		/** Index of the till this employee works, -1 for none. */
		int Till = -1;
		/** Metres to the back exit (Fleeing takes this long at 3 m/s). */
		double ExitDistance = 8.0;
		bool bCanReachAlarm = false;
		double UncoveredTime = 0.0;
	};

	/** What the player does to one employee this step. */
	struct FThreat
	{
		bool bAimedAt = false;        // weapon pointed at this employee
		bool bWeaponSeen = false;     // a weapon is out and this employee can see it
		bool bOrderedCash = false;    // player orders this employee to empty the till
	};

	struct FParams
	{
		double FearFromAim = 1.2;        // per second while aimed at
		double FearFromWeapon = 0.35;    // per second while a weapon is seen
		double FearFromShout = 0.3;      // one-off, everyone in earshot
		double FearDecay = 0.08;         // per second without threat
		double AlertAt = 0.15;
		double HandsUpAt = 0.5;
		double OpenTillSeconds = 2.5;
		double HandOverSeconds = 4.0;
		double FleeSpeed = 3.0;          // m/s to the back exit
		double ActAfterUncovered = 3.0;  // seconds out of cover before a brave employee acts
		double AlarmToReportSeconds = 20.0;  // monitoring centre calls the police
		bool bRobberTakesCoins = false;
	};

	struct FHoldUp
	{
		std::vector<FStaff> Staff;
		std::vector<GTPCash::FDrawer> Tills;
		FParams Params;
		GTPCash::FRng Rng{1};
		double Time = 0.0;
		double AlarmTime = -1.0;
		bool bCrimeReported = false;
		int64_t Loot = 0;
		std::vector<FEventRecord> Events;

		void Emit(EEvent Kind, int StaffIndex, int64_t Cents = 0) { Events.push_back({Kind, StaffIndex, Time, Cents}); }

		void ReportCrime()
		{
			if (!bCrimeReported)
			{
				bCrimeReported = true;
				Emit(EEvent::CrimeReported, -1);
			}
		}

		/** Seeds courage per employee; managers are braver on average. */
		void Begin(uint64_t Seed)
		{
			Rng = GTPCash::FRng(Seed);
			for (FStaff& Member : Staff)
			{
				const double Base = Member.Role == ERole::Manager ? 0.45 : 0.15;
				Member.Courage = Base + 0.5 * Rng.Unit();
			}
		}

		/** Player shouts "everybody down": fear for everyone still inside. */
		void Shout()
		{
			for (FStaff& Member : Staff)
			{
				if (Member.State != EState::Gone && Member.State != EState::Fleeing)
				{
					Member.Fear += Params.FearFromShout;
					Member.UncoveredTime = 0.0;
				}
			}
		}

		/** One fixed step. Threats has one entry per employee. */
		void Step(double Dt, const std::vector<FThreat>& Threats)
		{
			Time += Dt;
			for (int Index = 0; Index < int(Staff.size()); ++Index)
			{
				FStaff& Member = Staff[size_t(Index)];
				const FThreat Threat = Index < int(Threats.size()) ? Threats[size_t(Index)] : FThreat();
				StepOne(Index, Member, Threat, Dt);
			}
			if (AlarmTime >= 0.0 && Time - AlarmTime >= Params.AlarmToReportSeconds)
			{
				ReportCrime();
			}
		}

		int CountIn(EState State) const
		{
			int Count = 0;
			for (const FStaff& Member : Staff)
			{
				Count += Member.State == State ? 1 : 0;
			}
			return Count;
		}

	private:
		void StepOne(int Index, FStaff& Member, const FThreat& Threat, double Dt)
		{
			if (Member.State == EState::Gone)
			{
				return;
			}
			const bool bThreatened = Threat.bAimedAt || Threat.bWeaponSeen;
			if (Threat.bAimedAt)
			{
				Member.Fear += Params.FearFromAim * Dt;
			}
			else if (Threat.bWeaponSeen)
			{
				Member.Fear += Params.FearFromWeapon * Dt;
			}
			else
			{
				Member.Fear -= Params.FearDecay * Dt;
			}
			Member.Fear = Member.Fear < 0.0 ? 0.0 : (Member.Fear > 1.0 ? 1.0 : Member.Fear);
			Member.UncoveredTime = Threat.bAimedAt ? 0.0 : Member.UncoveredTime + Dt;

			switch (Member.State)
			{
			case EState::Working:
				if (Member.Fear >= Params.AlertAt)
				{
					Member.State = EState::Alerted;
					Emit(EEvent::Alerted, Index);
				}
				break;
			case EState::Alerted:
			case EState::Cowering:
				if (Threat.bAimedAt && Member.Fear >= Params.HandsUpAt)
				{
					Member.State = EState::HandsUp;
					Emit(EEvent::HandsUp, Index);
				}
				else if (!Threat.bAimedAt)
				{
					TryAct(Index, Member);
				}
				break;
			case EState::HandsUp:
				if (Threat.bAimedAt && Threat.bOrderedCash && Member.Till >= 0)
				{
					Member.State = EState::OpeningTill;
					Member.Timer = Params.OpenTillSeconds;
				}
				else if (!bThreatened)
				{
					TryAct(Index, Member);
				}
				break;
			case EState::OpeningTill:
				Member.Timer -= Dt;
				if (Member.Timer <= 0.0)
				{
					Member.State = EState::HandingOver;
					Member.Timer = Params.HandOverSeconds;
					Emit(EEvent::TillOpened, Index);
				}
				break;
			case EState::HandingOver:
				Member.Timer -= Dt;
				if (Member.Timer <= 0.0)
				{
					const int64_t Cash = GTPCash::Grab(Tills[size_t(Member.Till)], Params.bRobberTakesCoins);
					Loot += Cash;
					Member.State = EState::HandsUp;
					Emit(EEvent::CashHandedOver, Index, Cash);
				}
				break;
			case EState::Fleeing:
				Member.Timer -= Dt;
				if (Member.Timer <= 0.0)
				{
					Member.State = EState::Gone;
					Emit(EEvent::StaffEscaped, Index);
					ReportCrime();  // they call the police from the street
				}
				break;
			case EState::Gone:
				break;
			}
		}

		/** An employee nobody covers: a brave one acts after a while, the others cower. */
		void TryAct(int Index, FStaff& Member)
		{
			if (Member.UncoveredTime < Params.ActAfterUncovered || Member.Courage <= Member.Fear)
			{
				if (Member.State == EState::Alerted && Member.Fear >= Params.HandsUpAt)
				{
					Member.State = EState::Cowering;
				}
				return;
			}
			if (Member.bCanReachAlarm && AlarmTime < 0.0)
			{
				AlarmTime = Time;
				Emit(EEvent::AlarmPressed, Index);
				Member.State = EState::Cowering;
				Member.UncoveredTime = -1e9;  // has done their part
				return;
			}
			if (Member.UncoveredTime > 0.0)
			{
				Member.State = EState::Fleeing;
				Member.Timer = Member.ExitDistance / Params.FleeSpeed;
			}
		}
	};
}
