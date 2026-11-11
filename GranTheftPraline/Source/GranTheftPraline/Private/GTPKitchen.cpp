#include "GTPKitchen.h"

#include "Animation/AnimMontage.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GTPHoldUp.h"
#include "GTPLog.h"
#include "GTPThreat.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(KitchenService, Log, All);   // lines read "KitchenService: ..." in every build

namespace
{
const TCHAR* StationPrefix = TEXT("GTP_Station=");
constexpr float StepSeconds = 1.f / 30.f;
constexpr float ArriveCm = 40.f;

/** Station family a task happens at (names of tools/interiors/fastfood_bellecour.json). */
const TCHAR* StationFor(GTPKitchen::ETask Task)
{
	using GTPKitchen::ETask;
	switch (Task)
	{
	case ETask::TakeOrder: return TEXT("till");
	case ETask::FryDrop:
	case ETask::FryLift: return TEXT("fryer");
	case ETask::FryBag: return TEXT("fry_dump");
	case ETask::GrillLoad:
	case ETask::GrillUnload: return TEXT("grill");
	case ETask::Assemble: return TEXT("assembly");
	case ETask::PourDrink: return TEXT("drinks");
	case ETask::BagOrder: return TEXT("pickup");
	default: return nullptr;
	}
}
} // namespace

AGTPStaffCharacter::AGTPStaffCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;   // walking to a slot does not need every frame
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	ThreatResponse = CreateDefaultSubobject<UGTPThreatResponseComponent>(TEXT("ThreatResponse"));
}

bool AGTPStaffCharacter::IsThreatened() const
{
	return ThreatResponse && UGTPThreatSubsystem::IsEnabled() && ThreatResponse->GetState() != EGTPThreatState::Calm;
}

void AGTPStaffCharacter::BeginTask(uint8 InTask, const FTransform& InSlot)
{
	Task = InTask;
	Slot = InSlot;
	bAtSlot = false;
}

void AGTPStaffCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (IsThreatened())
	{
		bAtSlot = false;   // walks back to its task once calm again
		return;
	}
	if (bAtSlot)
	{
		return;
	}
	FVector ToSlot = Slot.GetLocation() - GetActorLocation();
	ToSlot.Z = 0.f;
	if (ToSlot.Size() > ArriveCm)
	{
		AddMovementInput(ToSlot.GetSafeNormal(), 1.f);
		return;
	}
	bAtSlot = true;
	// Slots stand 70 cm in front of their station, on the counter side (-Y of the interior frame).
	SetActorRotation(FRotator(0.f, Slot.GetRotation().RotateVector(FVector(0.f, -1.f, 0.f)).Rotation().Yaw, 0.f));
	if (const TObjectPtr<UAnimMontage>* Montage = TaskMontages.Find(Task))
	{
		PlayAnimMontage(Montage->Get());
	}
	OnTaskStarted.Broadcast(this, Task);
}

bool UGTPKitchenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGTPKitchenSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPKitchenSubsystem, STATGROUP_Tickables);
}

void UGTPKitchenSubsystem::StartService(ULevel* InteriorLevel, const FString& InteriorId, const FTransform& Anchor)
{
	int32 CrewSize = 0;
	FParse::Value(FCommandLine::Get(), TEXT("InteriorStaff="), CrewSize);
	CrewSize = FMath::Clamp(CrewSize, 0, 10);
	if (!InteriorLevel || CrewSize == 0 || bRunning)
	{
		return;
	}
	Slots.Reset();
	for (AActor* Actor : InteriorLevel->Actors)
	{
		if (!Actor)
		{
			continue;
		}
		for (const FName& Tag : Actor->Tags)
		{
			const FString S = Tag.ToString();
			if (S.StartsWith(StationPrefix))
			{
				Slots.Add(S.RightChop(FCString::Strlen(StationPrefix)), Actor->GetActorTransform());
			}
		}
	}
	Kitchen = GTPKitchen::FKitchen();
	int32 PerHour = Kitchen.Params.CustomersPerHour;
	FParse::Value(FCommandLine::Get(), TEXT("InteriorCustomersPerHour="), PerHour);
	Kitchen.Params.CustomersPerHour = FMath::Max(1, PerHour);
	Kitchen.Staff = GTPKitchen::FKitchen::DefaultCrew(CrewSize);
	Kitchen.Begin(1);
	EventsSent = 0;
	Crew.Reset();
	for (int32 Index = 0; Index < CrewSize; ++Index)
	{
		FTransform Start;
		if (!FindSlot(GTPKitchen::ETask::TakeOrder, Index, -1, Start))
		{
			Start = InteriorLevel->Actors.Num() > 0 && InteriorLevel->Actors[0] ? InteriorLevel->Actors[0]->GetActorTransform() : FTransform::Identity;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Params.OverrideLevel = InteriorLevel;
		Crew.Add(GetWorld()->SpawnActor<AGTPStaffCharacter>(AGTPStaffCharacter::StaticClass(), Start, Params));
	}
	bRunning = true;
	if (UGTPHoldUpSubsystem* HoldUp = GetWorld()->GetSubsystem<UGTPHoldUpSubsystem>())
	{
		TArray<AGTPStaffCharacter*> Characters;
		TArray<uint8> Roles;
		for (int32 Index = 0; Index < Crew.Num(); ++Index)
		{
			Characters.Add(Crew[Index].Get());
			Roles.Add(static_cast<uint8>(Kitchen.Staff[size_t(Index)].Role));
		}
		TArray<FString> TillNames;
		for (const TPair<FString, FTransform>& Pair : Slots)
		{
			if (Pair.Key.StartsWith(TEXT("till_")))
			{
				TillNames.Add(Pair.Key);
			}
		}
		TillNames.Sort();   // counter staff k works till k, in name order
		TArray<FVector> Tills;
		for (const FString& Name : TillNames)
		{
			Tills.Add(Slots[Name].GetLocation());
		}
		HoldUp->BeginShop(InteriorId, Anchor, Tills, Characters, Roles);
	}
	GTP_TRACE(KitchenService, TEXT("service started: %d staff, %d station slots, %d customers/h"), CrewSize, Slots.Num(), Kitchen.Params.CustomersPerHour);
}

void UGTPKitchenSubsystem::StopService()
{
	if (UGTPHoldUpSubsystem* HoldUp = GetWorld()->GetSubsystem<UGTPHoldUpSubsystem>())
	{
		HoldUp->EndShop();
	}
	for (const TWeakObjectPtr<AGTPStaffCharacter>& Staff : Crew)
	{
		if (AGTPStaffCharacter* Character = Staff.Get())
		{
			Character->Destroy();
		}
	}
	Crew.Reset();
	bRunning = false;
}

bool UGTPKitchenSubsystem::FindSlot(GTPKitchen::ETask Task, int32 StaffIndex, int32 Target, FTransform& Out) const
{
	const TCHAR* Family = StationFor(Task);
	if (!Family)
	{
		return false;
	}
	TArray<FString> Names;
	for (const TPair<FString, FTransform>& Pair : Slots)
	{
		FString Left, Right;
		if (Pair.Key.Split(TEXT("_"), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd) && Left == Family)
		{
			Names.Add(Pair.Key);
		}
	}
	if (Names.Num() == 0)
	{
		return false;
	}
	Names.Sort();
	// Baskets and grills come in pairs per station; other tasks spread the crew over the stations.
	const bool bPerTarget = Task == GTPKitchen::ETask::FryDrop || Task == GTPKitchen::ETask::FryLift ||
	                        Task == GTPKitchen::ETask::GrillLoad || Task == GTPKitchen::ETask::GrillUnload;
	const int32 Pick = bPerTarget && Target >= 0 ? (Task <= GTPKitchen::ETask::FryLift ? Target / 2 : Target) : StaffIndex;
	Out = Slots[Names[Pick % Names.Num()]];
	return true;
}

void UGTPKitchenSubsystem::Dispatch(const GTPKitchen::FTaskEvent& Event)
{
	AGTPStaffCharacter* Character = Crew.IsValidIndex(Event.Staff) ? Crew[Event.Staff].Get() : nullptr;
	if (!Character)
	{
		return;
	}
	const GTPKitchen::FStaff& Member = Kitchen.Staff[size_t(Event.Staff)];
	FTransform Slot;
	if (FindSlot(Event.Task, Event.Staff, Member.Target, Slot))
	{
		Character->BeginTask(static_cast<uint8>(Event.Task), Slot);
	}
}

void UGTPKitchenSubsystem::Tick(float DeltaTime)
{
	if (!bRunning)
	{
		return;
	}
	Accumulator = FMath::Min(Accumulator + DeltaTime, 0.5f);   // never catch up more than half a second after a hitch
	while (Accumulator >= StepSeconds)
	{
		Accumulator -= StepSeconds;
		Kitchen.Step(StepSeconds);
	}
	for (; EventsSent < Kitchen.Events.size(); ++EventsSent)
	{
		Dispatch(Kitchen.Events[EventsSent]);
	}
	if (Kitchen.Events.size() > 4096)
	{
		Kitchen.Events.clear();   // events are only a hand-off to the characters
		EventsSent = 0;
	}
	SinceLog += DeltaTime;
	if (SinceLog >= 60.f)
	{
		SinceLog = 0.f;
		GTP_TRACE(KitchenService, TEXT("served %d, wait %.0f s avg, %.0f s max, %d orders open, waste %d fries %d patties"), Kitchen.Served,
		          Kitchen.AverageWait(), Kitchen.MaxWait, static_cast<int32>(Kitchen.Orders.size()), Kitchen.FriesWasted, Kitchen.PattiesWasted);
	}
}
