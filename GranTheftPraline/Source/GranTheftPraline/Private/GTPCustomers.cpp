#include "GTPCustomers.h"

#include "Engine/Level.h"
#include "Engine/World.h"
#include "GTPInteriorStreaming.h"
#include "GTPLog.h"
#include "GTPPedestrians.h"
#include "GTPThreat.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(CustomerQueue, Log, All);   // lines read "CustomerQueue: ..." in every build (not "Customers": a member has that name)

namespace
{
constexpr float CustomerStepSeconds = 0.1f;
constexpr float CustomerAcquireGap = 0.25f;   // seconds between two pedestrians lent
const TCHAR* CustomerSpotPrefix = TEXT("GTP_CustomerSpot=q");   // q<line>_<place>
}

bool UGTPCustomerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGTPCustomerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPCustomerSubsystem, STATGROUP_Tickables);
}

void UGTPCustomerSubsystem::OpenShop(ULevel* InteriorLevel, const FString& InteriorId)
{
	int32 Wanted = 6;
	FParse::Value(FCommandLine::Get(), TEXT("InteriorCustomers="), Wanted);
	if (!InteriorLevel || Wanted <= 0 || bOpen)
	{
		return;
	}
	// Spots: "q<line>_<place>", place 0 at the till.
	TMap<int32, TMap<int32, FTransform>> Found;
	for (AActor* Actor : InteriorLevel->Actors)
	{
		if (!Actor)
		{
			continue;
		}
		for (const FName& Tag : Actor->Tags)
		{
			const FString S = Tag.ToString();
			FString LinePart, PlacePart;
			if (S.StartsWith(CustomerSpotPrefix) && S.RightChop(FCString::Strlen(CustomerSpotPrefix)).Split(TEXT("_"), &LinePart, &PlacePart))
			{
				Found.FindOrAdd(FCString::Atoi(*LinePart)).Add(FCString::Atoi(*PlacePart), Actor->GetActorTransform());
			}
		}
	}
	Spots.Reset();
	TArray<int32> Lines;
	Found.GetKeys(Lines);
	Lines.Sort();
	std::vector<int> SpotsPerLine;
	int32 TotalSpots = 0;
	for (const int32 Line : Lines)
	{
		TArray<int32> Places;
		Found[Line].GetKeys(Places);
		Places.Sort();
		TArray<FTransform>& Out = Spots.AddDefaulted_GetRef();
		for (const int32 Place : Places)
		{
			Out.Add(Found[Line][Place]);
		}
		SpotsPerLine.push_back(Out.Num());
		TotalSpots += Out.Num();
	}
	if (Spots.Num() == 0)
	{
		GTP_TRACE(CustomerQueue, TEXT("no customer spot in %s (rebuild the level with build_interior_level.py)"), *InteriorId);
		return;
	}
	ShopId = InteriorId;
	Run = GTPCustomers::FRun();
	Run.Begin(SpotsPerLine, Wanted, 77u);
	EventsSeen = 0;
	Accumulator = 0.f;
	SinceLog = 0.f;
	Refused = 0;
	bOpen = true;
	SinceAcquire = CustomerAcquireGap;
	ApplyPending();   // the first customer now, the others over the next frames
	GTP_TRACE(CustomerQueue, TEXT("shop %s open: %d lines, %d spots, %d customers coming in, one every %.2f s"), *ShopId, Spots.Num(),
	          TotalSpots, Run.Count(), CustomerAcquireGap);
}

void UGTPCustomerSubsystem::CloseShop()
{
	if (!bOpen)
	{
		return;
	}
	UGTPPedestrianSubsystem* Pool = GetWorld()->GetSubsystem<UGTPPedestrianSubsystem>();
	for (const TPair<int32, TWeakObjectPtr<AGTPPedestrian>>& Pair : Customers)
	{
		if (Pool && Pair.Value.IsValid())
		{
			Pool->ReturnToPool(Pair.Value.Get());
		}
	}
	GTP_TRACE(CustomerQueue, TEXT("shop %s closed: %d served"), *ShopId, Run.ServedCount);
	Customers.Reset();
	Spots.Reset();
	bOpen = false;
}

void UGTPCustomerSubsystem::ApplyPending()
{
	// One pedestrian lent per CustomerAcquireGap at most: lending switches a pedestrian to full
	// detail, and six at once on the frame the shop opens gave a 63 ms frame on the M1.
	for (; EventsSeen < Run.Events.size(); ++EventsSeen)
	{
		const GTPCustomers::FEvent& Event = Run.Events[EventsSeen];
		if (Event.Kind == GTPCustomers::ECustomerEvent::Arrived)
		{
			if (SinceAcquire < CustomerAcquireGap)
			{
				return;   // the rest waits, in order
			}
			SinceAcquire = 0.f;
		}
		Apply(Event);
	}
}

void UGTPCustomerSubsystem::Apply(const GTPCustomers::FEvent& Event)
{
	using GTPCustomers::ECustomerEvent;
	UGTPPedestrianSubsystem* Pool = GetWorld()->GetSubsystem<UGTPPedestrianSubsystem>();
	if (!Spots.IsValidIndex(Event.Line) || !Spots[Event.Line].IsValidIndex(Event.Index))
	{
		return;
	}
	const FTransform& Spot = Spots[Event.Line][Event.Index];
	switch (Event.Kind)
	{
	case ECustomerEvent::Arrived:
	{
		AGTPPedestrian* Person = Pool ? Pool->AcquireFromPool(Spot.GetLocation(), static_cast<float>(Spot.Rotator().Yaw)) : nullptr;
		if (Person)
		{
			Customers.Add(Event.Id, Person);
		}
		else
		{
			++Refused;   // pedestrians off or none to lend: the spot stays empty, the queue goes on
		}
		break;
	}
	case ECustomerEvent::Moved:
		if (const TWeakObjectPtr<AGTPPedestrian>* Person = Customers.Find(Event.Id))
		{
			if (Person->IsValid())
			{
				(*Person)->SetActorLocationAndRotation(Spot.GetLocation(), FRotator(0.f, Spot.Rotator().Yaw, 0.f), false, nullptr,
				                                        ETeleportType::TeleportPhysics);
			}
		}
		break;
	case ECustomerEvent::Served:
	{
		TWeakObjectPtr<AGTPPedestrian> Person;
		if (Customers.RemoveAndCopyValue(Event.Id, Person) && Pool && Person.IsValid())
		{
			Pool->ReturnToPool(Person.Get());
		}
		break;
	}
	default:
		break;
	}
}

bool UGTPCustomerSubsystem::IsFrozen() const
{
	const UGTPThreatSubsystem* Threats = GetWorld()->GetSubsystem<UGTPThreatSubsystem>();
	const UGTPInteriorStreaming* Streaming = GetWorld()->GetSubsystem<UGTPInteriorStreaming>();
	return Threats && Threats->HasThreat() && Threats->IsArmed() && Streaming && Streaming->GetPlayerInteriorId() == ShopId;
}

void UGTPCustomerSubsystem::Tick(float DeltaTime)
{
	if (!bOpen)
	{
		return;
	}
	Accumulator = FMath::Min(Accumulator + DeltaTime, 0.5f);
	const bool bFrozen = IsFrozen();
	while (Accumulator >= CustomerStepSeconds)
	{
		Accumulator -= CustomerStepSeconds;
		Run.Step(CustomerStepSeconds, bFrozen);
	}
	SinceAcquire += DeltaTime;
	ApplyPending();
	SinceLog += DeltaTime;
	if (SinceLog >= 60.f)
	{
		SinceLog = 0.f;
		GTP_TRACE(CustomerQueue, TEXT("%s: %d in the shop, %d served, %d refused by the pool%s"), *ShopId, Customers.Num(), Run.ServedCount,
		          Refused, bFrozen ? TEXT(", frozen by a threat") : TEXT(""));
	}
}
