// Work loops of a quick-service kitchen (docs/gameplay/reference-interior.md section 4, step 4).
// Customers order at a till or a kiosk; the fry station drops and lifts baskets and bags the
// fries, the grill loads and unloads patties into the holding cabinet, the assembly crew toasts
// and builds burgers from held patties, the counter crew takes orders, pours drinks and bags.
// Every task has a real duration and moves real objects (a basket, a batch of patties, a
// carton): the game plays the matching animation and promotes the matching instances while a
// task runs. Food held too long is thrown away, as in a real restaurant. Plain C++, seeded and
// stepped at a fixed rate, so tools/interiors/core_test checks it with g++.
#pragma once

#include "GTPCashDrawerModel.h"

#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

namespace GTPKitchen
{
	enum class ERole : uint8_t
	{
		Fries,
		Grill,
		Assembly,
		Counter,
	};

	enum class ETask : uint8_t
	{
		Idle,
		TakeOrder,     // at a till
		FryDrop,       // basket into the oil
		FryLift,       // basket out, into the dump
		FryBag,        // scoop a portion into a carton
		GrillLoad,     // patties onto the grill
		GrillUnload,   // patties into the holding cabinet
		Assemble,      // toast, build and wrap one burger
		PourDrink,     // cup, drink tower, lid
		BagOrder,      // tray or bag, call the number
	};

	struct FParams
	{
		double FryCookSeconds = 180.0;
		double FryHoldSeconds = 420.0;     // fries thrown away after 7 minutes
		int FryBatchPortions = 6;          // portions per basket
		int Baskets = 8;                   // 4 fryers x 2 baskets
		double GrillCookSeconds = 45.0;
		int GrillBatch = 8;                // patties per load
		int Grills = 2;
		double PattyHoldSeconds = 900.0;   // 15 minutes in the cabinet
		int PattyPar = 8;                  // held patties the grill keeps ahead of demand
		int FriesPar = 6;
		double TakeOrderSeconds = 40.0;
		double FryDropSeconds = 5.0;
		double FryLiftSeconds = 6.0;
		double FryBagSeconds = 8.0;
		double GrillLoadSeconds = 10.0;
		double GrillUnloadSeconds = 8.0;
		double AssembleSeconds = 30.0;   // bun toasted in parallel by the toaster
		double PourSeconds = 15.0;
		double BagSeconds = 10.0;
		/** Share of customers ordering at a kiosk (no till time). */
		double KioskShare = 0.6;
		int CustomersPerHour = 120;
	};

	struct FOrder
	{
		int Id = 0;
		double PlacedAt = 0.0;
		int Burgers = 0, Fries = 0, Drinks = 0;
		int BurgersDone = 0, FriesDone = 0, DrinksDone = 0;
		bool bReady() const { return BurgersDone >= Burgers && FriesDone >= Fries && DrinksDone >= Drinks; }
	};

	struct FStaff
	{
		ERole Role = ERole::Counter;
		ETask Task = ETask::Idle;
		double TimeLeft = 0.0;
		int Target = -1;   // basket, grill or order index the task acts on
		double BusySeconds = 0.0;
	};

	struct FTaskEvent
	{
		int Staff;
		ETask Task;
		double Time;
	};

	struct FKitchen
	{
		FParams Params;
		std::vector<FStaff> Staff;
		GTPCash::FRng Rng{1};
		double Time = 0.0;
		double NextArrival = 0.0;
		int WaitingAtTill = 0;
		std::deque<FOrder> Orders;
		std::vector<double> BasketDoneAt;   // < 0: basket free, else time its fries are done
		std::vector<double> GrillDoneAt;
		std::vector<double> FriesHeld;      // time each held portion was made
		std::vector<double> PattiesHeld;
		int NextOrderId = 1;
		int Served = 0;
		double TotalWait = 0.0;
		double MaxWait = 0.0;
		int FriesWasted = 0;
		int PattiesWasted = 0;
		std::vector<FTaskEvent> Events;

		void Begin(uint64_t Seed)
		{
			Rng = GTPCash::FRng(Seed);
			BasketDoneAt.assign(size_t(Params.Baskets), -1.0);
			GrillDoneAt.assign(size_t(Params.Grills), -1.0);
			NextArrival = NextGap();
		}

		/** A typical evening crew: 2 fries, 1 grill, 2 assembly, 3 counter (8, the M1 default). */
		static std::vector<FStaff> DefaultCrew(int Size = 8)
		{
			static const ERole Order[] = {ERole::Counter, ERole::Assembly, ERole::Fries, ERole::Grill,
			                              ERole::Counter, ERole::Assembly, ERole::Counter, ERole::Fries,
			                              ERole::Counter, ERole::Assembly};
			std::vector<FStaff> Crew;
			for (int Index = 0; Index < Size && Index < 10; ++Index)
			{
				FStaff Member;
				Member.Role = Order[Index];
				Crew.push_back(Member);
			}
			return Crew;
		}

		double AverageWait() const { return Served > 0 ? TotalWait / Served : 0.0; }

		void Step(double Dt)
		{
			Time += Dt;
			Arrivals();
			Expire(FriesHeld, Params.FryHoldSeconds, FriesWasted);
			Expire(PattiesHeld, Params.PattyHoldSeconds, PattiesWasted);
			for (int Index = 0; Index < int(Staff.size()); ++Index)
			{
				FStaff& Member = Staff[size_t(Index)];
				if (Member.Task != ETask::Idle)
				{
					Member.BusySeconds += Dt;
					Member.TimeLeft -= Dt;
					if (Member.TimeLeft > 0.0)
					{
						continue;
					}
					Finish(Member);
				}
				Choose(Index, Member);
			}
		}

	private:
		double NextGap()
		{
			// Exponential gaps between customers (Poisson arrivals) at the hourly rate.
			const double Rate = Params.CustomersPerHour / 3600.0;
			double U = Rng.Unit();
			U = U < 1e-9 ? 1e-9 : U;
			return Time - std::log(U) / Rate;
		}

		void Arrivals()
		{
			while (Time >= NextArrival)
			{
				if (Rng.Unit() < Params.KioskShare)
				{
					PlaceOrder();
				}
				else
				{
					++WaitingAtTill;
				}
				NextArrival = NextGap();
			}
		}

		void PlaceOrder()
		{
			FOrder Order;
			Order.Id = NextOrderId++;
			Order.PlacedAt = Time;
			Order.Burgers = 1 + Rng.Range(2);
			Order.Fries = Rng.Unit() < 0.85 ? 1 + Rng.Range(2) : 0;
			Order.Drinks = Rng.Unit() < 0.8 ? 1 + Rng.Range(2) : 0;
			Orders.push_back(Order);
		}

		/** Held food is kept oldest first: drop what passed its holding time. */
		void Expire(std::vector<double>& Held, double HoldSeconds, int& Wasted)
		{
			size_t Old = 0;
			while (Old < Held.size() && Time - Held[Old] > HoldSeconds)
			{
				++Old;
			}
			if (Old > 0)
			{
				Held.erase(Held.begin(), Held.begin() + long(Old));
				Wasted += int(Old);
			}
		}

		int Demand(int FOrder::*Want, int FOrder::*Done) const
		{
			int Sum = 0;
			for (const FOrder& Order : Orders)
			{
				Sum += Order.*Want - Order.*Done;
			}
			return Sum;
		}

		int Cooking(const std::vector<double>& DoneAt) const
		{
			int Count = 0;
			for (double At : DoneAt)
			{
				Count += At >= 0.0 ? 1 : 0;
			}
			return Count;
		}

		int Reserved(ETask Task) const
		{
			int Count = 0;
			for (const FStaff& Member : Staff)
			{
				Count += Member.Task == Task ? 1 : 0;
			}
			return Count;
		}

		/** First order still missing an item of this kind and not already being served by someone else. */
		int OrderNeeding(int FOrder::*Want, int FOrder::*Done, ETask Task) const
		{
			for (size_t Index = 0; Index < Orders.size(); ++Index)
			{
				int Pending = Orders[Index].*Want - Orders[Index].*Done;
				for (const FStaff& Member : Staff)
				{
					Pending -= (Member.Task == Task && Member.Target == int(Index)) ? 1 : 0;
				}
				if (Pending > 0)
				{
					return int(Index);
				}
			}
			return -1;
		}

		void Start(int Index, FStaff& Member, ETask Task, double Seconds, int Target)
		{
			Member.Task = Task;
			Member.TimeLeft = Seconds;
			Member.Target = Target;
			Events.push_back({Index, Task, Time});
		}

		/** Own station first; with nothing to do there, a crew member helps the others (small crews). */
		void Choose(int Index, FStaff& Member)
		{
			static const ERole Help[] = {ERole::Counter, ERole::Assembly, ERole::Fries, ERole::Grill};
			ChooseFor(Index, Member, Member.Role);
			for (ERole Role : Help)
			{
				if (Member.Task != ETask::Idle)
				{
					return;
				}
				if (Role != Member.Role)
				{
					ChooseFor(Index, Member, Role);
				}
			}
		}

		void ChooseFor(int Index, FStaff& Member, ERole Role)
		{
			Member.Task = ETask::Idle;
			Member.Target = -1;
			switch (Role)
			{
			case ERole::Fries:
			{
				for (size_t Basket = 0; Basket < BasketDoneAt.size(); ++Basket)
				{
					if (BasketDoneAt[Basket] >= 0.0 && BasketDoneAt[Basket] <= Time && !Targeted(ETask::FryLift, int(Basket)))
					{
						return Start(Index, Member, ETask::FryLift, Params.FryLiftSeconds, int(Basket));
					}
				}
				const int Need = Demand(&FOrder::Fries, &FOrder::FriesDone) + Params.FriesPar;
				const int Have = int(FriesHeld.size()) + (Cooking(BasketDoneAt) + Reserved(ETask::FryDrop)) * Params.FryBatchPortions;
				if (Have < Need)
				{
					for (size_t Basket = 0; Basket < BasketDoneAt.size(); ++Basket)
					{
						if (BasketDoneAt[Basket] < 0.0 && !Targeted(ETask::FryDrop, int(Basket)))
						{
							return Start(Index, Member, ETask::FryDrop, Params.FryDropSeconds, int(Basket));
						}
					}
				}
				const int Order = OrderNeeding(&FOrder::Fries, &FOrder::FriesDone, ETask::FryBag);
				if (Order >= 0 && int(FriesHeld.size()) > Reserved(ETask::FryBag))
				{
					return Start(Index, Member, ETask::FryBag, Params.FryBagSeconds, Order);
				}
				return;
			}
			case ERole::Grill:
			{
				for (size_t Grill = 0; Grill < GrillDoneAt.size(); ++Grill)
				{
					if (GrillDoneAt[Grill] >= 0.0 && GrillDoneAt[Grill] <= Time && !Targeted(ETask::GrillUnload, int(Grill)))
					{
						return Start(Index, Member, ETask::GrillUnload, Params.GrillUnloadSeconds, int(Grill));
					}
				}
				const int Need = Demand(&FOrder::Burgers, &FOrder::BurgersDone) + Params.PattyPar;
				const int Have = int(PattiesHeld.size()) + (Cooking(GrillDoneAt) + Reserved(ETask::GrillLoad)) * Params.GrillBatch;
				if (Have < Need)
				{
					for (size_t Grill = 0; Grill < GrillDoneAt.size(); ++Grill)
					{
						if (GrillDoneAt[Grill] < 0.0 && !Targeted(ETask::GrillLoad, int(Grill)))
						{
							return Start(Index, Member, ETask::GrillLoad, Params.GrillLoadSeconds, int(Grill));
						}
					}
				}
				return;
			}
			case ERole::Assembly:
			{
				const int Order = OrderNeeding(&FOrder::Burgers, &FOrder::BurgersDone, ETask::Assemble);
				if (Order >= 0 && !PattiesHeld.empty())
				{
					PattiesHeld.erase(PattiesHeld.begin());   // oldest patty first
					return Start(Index, Member, ETask::Assemble, Params.AssembleSeconds, Order);
				}
				return;
			}
			case ERole::Counter:
			{
				for (size_t Order = 0; Order < Orders.size(); ++Order)
				{
					if (Orders[Order].bReady() && !Targeted(ETask::BagOrder, int(Order)))
					{
						return Start(Index, Member, ETask::BagOrder, Params.BagSeconds, int(Order));
					}
				}
				if (WaitingAtTill > Reserved(ETask::TakeOrder))
				{
					return Start(Index, Member, ETask::TakeOrder, Params.TakeOrderSeconds, -1);
				}
				const int Order = OrderNeeding(&FOrder::Drinks, &FOrder::DrinksDone, ETask::PourDrink);
				if (Order >= 0)
				{
					return Start(Index, Member, ETask::PourDrink, Params.PourSeconds, Order);
				}
				return;
			}
			}
		}

		bool Targeted(ETask Task, int Target) const
		{
			for (const FStaff& Member : Staff)
			{
				if (Member.Task == Task && Member.Target == Target)
				{
					return true;
				}
			}
			return false;
		}

		FOrder* OrderAt(int Index) { return Index >= 0 && Index < int(Orders.size()) ? &Orders[size_t(Index)] : nullptr; }

		void Finish(FStaff& Member)
		{
			switch (Member.Task)
			{
			case ETask::TakeOrder:
				if (WaitingAtTill > 0)
				{
					--WaitingAtTill;
					PlaceOrder();
				}
				break;
			case ETask::FryDrop:
				BasketDoneAt[size_t(Member.Target)] = Time + Params.FryCookSeconds;
				break;
			case ETask::FryLift:
				BasketDoneAt[size_t(Member.Target)] = -1.0;
				FriesHeld.insert(FriesHeld.end(), size_t(Params.FryBatchPortions), Time);
				break;
			case ETask::FryBag:
				if (!FriesHeld.empty())
				{
					FriesHeld.erase(FriesHeld.begin());
					if (FOrder* Order = OrderAt(Member.Target))
					{
						++Order->FriesDone;
					}
				}
				break;
			case ETask::GrillLoad:
				GrillDoneAt[size_t(Member.Target)] = Time + Params.GrillCookSeconds;
				break;
			case ETask::GrillUnload:
				GrillDoneAt[size_t(Member.Target)] = -1.0;
				PattiesHeld.insert(PattiesHeld.end(), size_t(Params.GrillBatch), Time);
				break;
			case ETask::Assemble:
				if (FOrder* Order = OrderAt(Member.Target))
				{
					++Order->BurgersDone;
				}
				break;
			case ETask::PourDrink:
				if (FOrder* Order = OrderAt(Member.Target))
				{
					++Order->DrinksDone;
				}
				break;
			case ETask::BagOrder:
				if (OrderAt(Member.Target))
				{
					const double Wait = Time - Orders[size_t(Member.Target)].PlacedAt;
					TotalWait += Wait;
					MaxWait = Wait > MaxWait ? Wait : MaxWait;
					++Served;
					Orders.erase(Orders.begin() + Member.Target);
					// Indices after the served order shift down by one for everyone still working on them.
					for (FStaff& Other : Staff)
					{
						if (&Other != &Member && Other.Target > Member.Target && TargetsOrder(Other.Task))
						{
							--Other.Target;
						}
					}
				}
				break;
			case ETask::Idle:
				break;
			}
			Member.Task = ETask::Idle;
			Member.Target = -1;
		}

		static bool TargetsOrder(ETask Task)
		{
			return Task == ETask::FryBag || Task == ETask::Assemble || Task == ETask::PourDrink || Task == ETask::BagOrder;
		}
	};
}
