#include "GTPPoliceSenses.h"

#include "AIController.h"
#include "Engine/World.h"
#include "GTPLog.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPSenses, Log, All);

int32 UGTPPoliceSenses::GetMode()
{
	static int32 Mode = -1;
	if (Mode < 0)
	{
		Mode = 2;
		FParse::Value(FCommandLine::Get(), TEXT("PolicePerception="), Mode);
		Mode = FMath::Clamp(Mode, 0, 2);
	}
	return Mode;
}

void UGTPPoliceEye::Init(UGTPPoliceSenses* InHub, AAIController* Controller)
{
	Hub = InHub;
	Owner = Controller;
	Perception = NewObject<UAIPerceptionComponent>(Controller, TEXT("GTPPolicePerception"));
	Controller->SetPerceptionComponent(*Perception);

	UAISenseConfig_Sight* Sight = NewObject<UAISenseConfig_Sight>(Perception);
	Sight->SightRadius = InHub->SightRadius;
	Sight->LoseSightRadius = InHub->LoseSightRadius;
	Sight->PeripheralVisionAngleDegrees = InHub->SightHalfAngle;
	Sight->AutoSuccessRangeFromLastSeenLocation = 1200.f;
	Sight->SetMaxAge(InHub->MemorySeconds);
	// Nobody carries a team here: everything is neutral, and the player is picked out by the handler.
	Sight->DetectionByAffiliation.bDetectEnemies = true;
	Sight->DetectionByAffiliation.bDetectNeutrals = true;
	Sight->DetectionByAffiliation.bDetectFriendlies = true;
	Perception->ConfigureSense(*Sight);

	UAISenseConfig_Hearing* Hearing = NewObject<UAISenseConfig_Hearing>(Perception);
	Hearing->HearingRange = InHub->HearingRange;
	Hearing->SetMaxAge(InHub->MemorySeconds);
	Hearing->DetectionByAffiliation.bDetectEnemies = true;
	Hearing->DetectionByAffiliation.bDetectNeutrals = true;
	Hearing->DetectionByAffiliation.bDetectFriendlies = true;
	Perception->ConfigureSense(*Hearing);

	Perception->SetDominantSense(Sight->GetSenseImplementation());
	Perception->OnTargetPerceptionUpdated.AddDynamic(this, &UGTPPoliceEye::OnTargetUpdated);
	Perception->RegisterComponent();
}

void UGTPPoliceEye::OnTargetUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Hub || !Actor || Actor != Hub->GetTarget())
	{
		return;
	}
	const bool bSight = Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>();
	const bool bSensed = Stimulus.WasSuccessfullySensed();
	if (bSight)
	{
		bSees = bSensed;
	}
	Hub->NoteSensed(Stimulus.StimulusLocation, bSight, bSensed);
}

void UGTPPoliceSenses::SetTarget(APawn* Pawn)
{
	if (Target.Get() == Pawn)
	{
		return;
	}
	Target = Pawn;
	if (Pawn && GetWorld())
	{
		// Be sure the new body is a sight source whatever the project's auto-registration says.
		UAIPerceptionSystem::RegisterPerceptionStimuliSource(GetWorld(), UAISense_Sight::StaticClass(), Pawn);
	}
	for (UGTPPoliceEye* Eye : Eyes)
	{
		if (Eye)
		{
			Eye->bSees = false;
		}
	}
}

void UGTPPoliceSenses::Attach(AAIController* Controller)
{
	if (GetMode() == 0 || !Controller || !Controller->GetPawn())
	{
		return;
	}
	Eyes.RemoveAll([](const UGTPPoliceEye* Old) { return !Old || !Old->Owner.IsValid(); });
	UGTPPoliceEye* Eye = NewObject<UGTPPoliceEye>(this);
	Eye->Init(this, Controller);
	Eyes.Add(Eye);
	GTP_TRACE(LogGTPSenses, TEXT("senses: eyes on %s (%d)"), *Controller->GetPawn()->GetName(), Eyes.Num());
}

bool UGTPPoliceSenses::AnySees() const
{
	for (int32 Index = 0; Index < Eyes.Num(); ++Index)
	{
		const UGTPPoliceEye* Eye = Eyes[Index];
		if (Eye && Eye->bSees && Eye->Owner.IsValid() && Eye->Owner->GetPawn())
		{
			return true;
		}
	}
	return false;
}

void UGTPPoliceSenses::NoteSensed(const FVector& Location, bool bSight, bool bSensed)
{
	if (!bSensed || !GetWorld())
	{
		return;
	}
	SensedLocation = Location;
	SensedTime = static_cast<float>(GetWorld()->GetTimeSeconds());
	if (!bSight)
	{
		HeardTime = SensedTime;
	}
}

void UGTPPoliceSenses::ReportNoise(const UObject* WorldContext, const FVector& Location, AActor* Instigator, float Loudness)
{
	if (GetMode() == 0 || !WorldContext)
	{
		return;
	}
	UAISense_Hearing::ReportNoiseEvent(const_cast<UObject*>(WorldContext), Location, Loudness, Instigator, 0.f, TEXT("GTPNoise"));
}
