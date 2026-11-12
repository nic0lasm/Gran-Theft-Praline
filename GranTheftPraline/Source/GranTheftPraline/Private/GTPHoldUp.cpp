#include "GTPHoldUp.h"

#include "Engine/World.h"
#include "GTPInteriorStreaming.h"
#include "GTPKitchen.h"
#include "GTPLog.h"
#include "GTPMissionSubsystem.h"
#include "GTPPlaceAudio.h"
#include "GTPThreat.h"
#include "GTPWallet.h"
#include "GTPWanted.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(HoldUp, Log, All);   // lines read "HoldUp: ..." in every build

namespace
{
constexpr float HoldUpStepSeconds = 0.1f;
/** Where staff run to: the back of the storage room, in the interior frame (cm). */
const FVector HoldUpBackExitLocal(1500.f, -1550.f, 0.f);

const TCHAR* HoldUpEventName(GTPHoldUp::EEvent Kind)
{
	using GTPHoldUp::EEvent;
	switch (Kind)
	{
	case EEvent::Alerted: return TEXT("started");
	case EEvent::TillOpened: return TEXT("till opened");
	case EEvent::CashHandedOver: return TEXT("cash handed over");
	case EEvent::AlarmPressed: return TEXT("silent alarm");
	case EEvent::StaffEscaped: return TEXT("staff escaped");
	case EEvent::CrimeReported: return TEXT("crime reported");
	default: return TEXT("event");
	}
}
} // namespace

bool UGTPHoldUpSubsystem::IsEnabled()
{
	int32 Value = 0;
	FParse::Value(FCommandLine::Get(), TEXT("HoldUp="), Value);
	return Value != 0 && UGTPThreatSubsystem::IsEnabled();
}

bool UGTPHoldUpSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && IsEnabled() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGTPHoldUpSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPHoldUpSubsystem, STATGROUP_Tickables);
}

void UGTPHoldUpSubsystem::BeginShop(const FString& InInteriorId, const FTransform& InAnchor, const TArray<FVector>& TillLocations,
                                    const TArray<AGTPStaffCharacter*>& Crew, const TArray<uint8>& Roles)
{
	EndShop();
	InteriorId = InInteriorId;
	Anchor = InAnchor;
	Tills = TillLocations;
	const int32 TillCount = Tills.Num();
	Run = GTPHoldUpTills::FRun();
	Run.Begin(TillCount, 1000u + uint64(Serial));
	Run.Staff.resize(size_t(Crew.Num()));
	Staff.Reset();
	const FVector BackExit = Anchor.TransformPosition(HoldUpBackExitLocal);
	int32 NextTill = 0;
	for (int32 Index = 0; Index < Crew.Num(); ++Index)
	{
		AGTPStaffCharacter* Character = Crew[Index];
		Staff.Add(Character);
		GTPHoldUpTills::FMember& Member = Run.Staff[size_t(Index)];
		const bool bCounter = Roles.IsValidIndex(Index) && Roles[Index] == static_cast<uint8>(GTPKitchen::ERole::Counter);
		if (bCounter && NextTill < TillCount)
		{
			Member.Till = NextTill++;
		}
		Member.bCanReachAlarm = bCounter;   // the button is under the counter
		UGTPThreatResponseComponent* Threat = Character ? Character->GetThreatResponse() : nullptr;
		if (!Threat)
		{
			continue;
		}
		Threat->SetFleeTarget(BackExit);
		// The first cashier stands for the manager: braver, the one most likely to press the alarm.
		Threat->SetTemperament(bCounter && Index == 0 ? EGTPTemperament::Brave : EGTPTemperament::Compliant);
		Threat->OnFledToTarget.AddWeakLambda(this, [this, Index](AActor*)
		{
			if (Run.Staff.size() > size_t(Index))
			{
				Run.Staff[size_t(Index)].bReachedExit = true;
			}
		});
	}
	if (UGTPInteriorStreaming* Streaming = GetWorld()->GetSubsystem<UGTPInteriorStreaming>())
	{
		InteriorHandle = Streaming->OnPlayerInteriorChanged.AddUObject(this, &UGTPHoldUpSubsystem::OnInteriorChanged);
	}
	if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
	{
		ArrestHandle = Wanted->OnArrested.AddUObject(this, &UGTPHoldUpSubsystem::OnArrested);
	}
	EventsSeen = 0;
	Accumulator = 0.f;
	bShop = true;
	GTP_TRACE(HoldUp, TEXT("shop %s ready: %d staff, %d tills, %.2f euros in the tills"), *InteriorId, Crew.Num(), TillCount,
	          Run.CashLeft() / 100.0);
}

void UGTPHoldUpSubsystem::EndShop()
{
	if (!bShop)
	{
		return;
	}
	Finish(false);
	if (UGTPInteriorStreaming* Streaming = GetWorld()->GetSubsystem<UGTPInteriorStreaming>())
	{
		Streaming->OnPlayerInteriorChanged.Remove(InteriorHandle);
	}
	if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
	{
		Wanted->OnArrested.Remove(ArrestHandle);
	}
	Staff.Reset();
	bShop = false;
}

void UGTPHoldUpSubsystem::OnInteriorChanged(const FString& OldId, const FString& NewId)
{
	if (OldId == InteriorId && NewId != InteriorId)
	{
		Finish(false);
	}
	else if (NewId == InteriorId && Run.Outcome != GTPHoldUpTills::EOutcome::None)
	{
		Rearm();   // back inside after a finished hold-up: another one can start, emptied tills stay empty
	}
}

void UGTPHoldUpSubsystem::Rearm()
{
	GTPHoldUpTills::FRun Next;
	Next.Tills = Run.Tills;
	Next.Staff = Run.Staff;
	for (GTPHoldUpTills::FMember& Member : Next.Staff)
	{
		Member.bReachedExit = false;   // those who escaped stay escaped (bEscaped)
	}
	Run = MoveTemp(Next);
	EventsSeen = 0;
}

void UGTPHoldUpSubsystem::Finish(bool bArrested)
{
	const GTPHoldUpTills::EOutcome Outcome = Run.End(bArrested);
	if (Outcome == GTPHoldUpTills::EOutcome::None)
	{
		return;
	}
	const FName Name(GTPHoldUpTills::OutcomeName(Outcome));
	GTP_TRACE(HoldUp, TEXT("hold-up #%d at %s ended: %s, loot %.2f euros, %d tills emptied"), Serial, *InteriorId, *Name.ToString(),
	          Run.Loot / 100.0, Run.TillsEmptied);
	if (UGTPMissionSubsystem* Missions = GetWorld()->GetSubsystem<UGTPMissionSubsystem>())
	{
		Missions->ReportEvent(TEXT("holdup.ended"), Name, static_cast<float>(Run.Loot / 100.0));
	}
	OnEnded.Broadcast(InteriorId, Name, Run.Loot);
}

void UGTPHoldUpSubsystem::HandleEvent(const GTPHoldUp::FEventRecord& Event)
{
	using GTPHoldUp::EEvent;
	const int32 TillIndex = Run.Staff.size() > size_t(Event.Staff) && Event.Staff >= 0 ? Run.Staff[size_t(Event.Staff)].Till : -1;
	const FVector TillAt = Tills.IsValidIndex(TillIndex) ? Tills[TillIndex] + FVector(0.f, 0.f, 95.f) : Anchor.GetLocation();
	GTP_TRACE(HoldUp, TEXT("%s, staff %d, %.2f euros, t=%.1f"), HoldUpEventName(Event.Kind), Event.Staff, Event.Cents / 100.0, Run.Time);
	switch (Event.Kind)
	{
	case EEvent::Alerted:
		++Serial;
		break;
	case EEvent::TillOpened:
		UGTPPlaceAudio::PlayShop(this, TillAt, EGTPShopSound::TillOpen);
		break;
	case EEvent::StaffEscaped:
		UGTPPlaceAudio::PlayShop(this, Anchor.TransformPosition(HoldUpBackExitLocal), EGTPShopSound::AlarmBuzzer);   // the back door
		break;
	case EEvent::CashHandedOver:
	{
		const int32 Till = TillIndex;
		UGTPPlaceAudio::PlayShop(this, TillAt, EGTPShopSound::Notes);
		if (Run.Params.bRobberTakesCoins)
		{
			UGTPPlaceAudio::PlayShop(this, TillAt, EGTPShopSound::Coins);
		}
		if (UGTPWalletSubsystem* Purse = UGTPWalletSubsystem::Get(this))
		{
			Purse->AddCents(Event.Cents, TEXT("holdup_till"), FName(*FString::Printf(TEXT("holdup#%d/till%d"), Serial, Till)));
		}
		if (UGTPMissionSubsystem* Missions = GetWorld()->GetSubsystem<UGTPMissionSubsystem>())
		{
			Missions->ReportEvent(TEXT("holdup.cash_taken"), FName(*InteriorId), static_cast<float>(Event.Cents / 100.0));
		}
		OnCashTaken.Broadcast(InteriorId, Till, Event.Cents);
		break;
	}
	case EEvent::CrimeReported:
		if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
		{
			Wanted->ReportCrime(EGTPCrimeKind::ArmedThreat, Anchor.GetLocation());
		}
		break;
	default:
		break;
	}
}

void UGTPHoldUpSubsystem::Tick(float DeltaTime)
{
	if (!bShop)
	{
		return;
	}
	Accumulator = FMath::Min(Accumulator + DeltaTime, 0.5f);
	if (Accumulator < HoldUpStepSeconds)
	{
		return;
	}
	const UGTPThreatSubsystem* Threats = GetWorld()->GetSubsystem<UGTPThreatSubsystem>();
	const bool bThreat = Threats && Threats->HasThreat() && Threats->IsArmed();
	for (int32 Index = 0; Index < Staff.Num(); ++Index)
	{
		GTPHoldUpTills::FMember& Member = Run.Staff[size_t(Index)];
		const AGTPStaffCharacter* Character = Staff[Index].Get();
		const UGTPThreatResponseComponent* Threat = Character ? Character->GetThreatResponse() : nullptr;
		if (!Threat)
		{
			Member.State = GTPHoldUpTills::EThreat::Down;   // gone from the world: counts as out
			continue;
		}
		Member.State = static_cast<GTPHoldUpTills::EThreat>(static_cast<uint8>(Threat->GetState()));
		Member.SecondsSinceAimed = Threat->GetSecondsSinceAimed();
	}
	while (Accumulator >= HoldUpStepSeconds)
	{
		Accumulator -= HoldUpStepSeconds;
		Run.Step(HoldUpStepSeconds, bThreat);
	}
	for (; EventsSeen < Run.Events.size(); ++EventsSeen)
	{
		HandleEvent(Run.Events[EventsSeen]);
	}
}
