#include "GTPMassTrafficCars.h"

namespace GTPMassTrafficCarsLocal
{
	GTPMassTrafficCars::FQuery& StoredQuery()
	{
		static GTPMassTrafficCars::FQuery Query;
		return Query;
	}
}

void GTPMassTrafficCars::SetQuery(FQuery Query)
{
	GTPMassTrafficCarsLocal::StoredQuery() = MoveTemp(Query);
}

bool GTPMassTrafficCars::Find(UWorld& World, const FVector& Near, float RadiusCm, TArray<FGTPMassTrafficCar>& Out)
{
	Out.Reset();
	const FQuery& Query = GTPMassTrafficCarsLocal::StoredQuery();
	if (!Query)
	{
		return false;
	}
	Query(World, Near, RadiusCm, Out);
	return true;
}
