#include "GTPRagdollTest.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GTPImpactLogic.h"
#include "GTPLog.h"
#include "GTPPlayerReaction.h"
#include "GTPRagdoll.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
constexpr float StepTimeout = 20.f;
}

bool UGTPRagdollTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FParse::Param(FCommandLine::Get(), TEXT("RagdollTest"));
}

TStatId UGTPRagdollTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPRagdollTestSubsystem, STATGROUP_Tickables);
}

ACharacter* UGTPRagdollTestSubsystem::SpawnBody(const FVector& Offset)
{
	ACharacter* Source = Player.Get();
	if (!Source)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Body = GetWorld()->SpawnActor<ACharacter>(Source->GetClass(), Source->GetActorLocation() + Offset, Source->GetActorRotation(), Params);
	if (Body)
	{
		Body->SpawnDefaultController();
		if (!Body->FindComponentByClass<UGTPRagdollComponent>())
		{
			UGTPRagdollComponent* R = NewObject<UGTPRagdollComponent>(Body, TEXT("GTPRagdoll"));
			R->RegisterComponent();
		}
	}
	return Body;
}

void UGTPRagdollTestSubsystem::Fail(const FString& Why)
{
	if (!UGTPRagdollSubsystem::IsEnabled())
	{
		return; // baseline run (-Ragdoll=0): the ragdolls are refused on purpose, only the frame times count
	}
	bFailed = true;
	GTP_TRACE(LogGTPRagdoll, TEXT("GTPRagdollTest: FAIL %s"), *Why);
}

void UGTPRagdollTestSubsystem::Finish()
{
	const double Avg = FrameCount > 0 ? FrameMsSum / FrameCount : 0.0;
	FrameMs.Sort();
	const float P95 = FrameMs.Num() > 0 ? FrameMs[FMath::Min(FrameMs.Num() - 1, static_cast<int32>(FrameMs.Num() * 0.95f))] : 0.f;
	const float Max = FrameMs.Num() > 0 ? FrameMs.Last() : 0.f;
	GTP_TRACE(LogGTPRagdoll, TEXT("GTPRagdollTest: scenario frames %d, mean %.2f ms, p95 %.2f ms, max %.2f ms (lite %d, enabled %d)"), FrameCount, Avg, P95, Max, UGTPRagdollSubsystem::IsLite() ? 1 : 0, UGTPRagdollSubsystem::IsEnabled() ? 1 : 0);
	GTP_TRACE(LogGTPRagdoll, TEXT("GTPRagdollTest: %s"), bFailed ? TEXT("FAIL") : TEXT("PASS"));
	Step = 99;
	if (FParse::Param(FCommandLine::Get(), TEXT("RagdollTestQuit")))
	{
		UKismetSystemLibrary::QuitGame(GetWorld(), GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
	}
}

void UGTPRagdollTestSubsystem::Tick(float DeltaTime)
{
	if (Step >= 99 || !GetWorld() || !GetWorld()->HasBegunPlay())
	{
		return;
	}
	StepTime += DeltaTime;
	if (Step > 0)
	{
		FrameMsSum += DeltaTime * 1000.0;
		++FrameCount;
		FrameMs.Add(DeltaTime * 1000.f);
	}
	if (!Player.IsValid())
	{
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			Player = Cast<ACharacter>(PC->GetPawn());
		}
		if (!Player.IsValid())
		{
			if (StepTime > StepTimeout)
			{
				Fail(TEXT("no player character"));
				Finish();
			}
			return;
		}
		StepTime = 0.f;
	}
	if (StepTime < 2.f && Step == 0)
	{
		return; // let the level settle
	}

	// Steps 1-3: one car hit each on a fresh body; step 4: falls; step 5: the cap.
	static const float CarKmh[3] = {20.f, 40.f, 70.f};
	if (Step >= 0 && Step < 3)
	{
		if (StepTime < 0.f) return;
		ACharacter* Body = SpawnBody(FVector(300.f * (Step + 1), 300.f, 0.f));
		UGTPRagdollComponent* R = Body ? Body->FindComponentByClass<UGTPRagdollComponent>() : nullptr;
		if (!R)
		{
			Fail(TEXT("cannot spawn a body"));
			Finish();
			return;
		}
		GTPImpact::FCarHitInput In;
		In.CarVelocity = {GTPImpact::CmSFromKmh(CarKmh[Step]), 0.f, 0.f};
		In.ToCar = {-100.f, 0.f, 0.f};
		const GTPImpact::FCarHitResult Hit = GTPImpact::ClassifyCarHit(In, GTPImpact::FSettings());
		const bool bOk = R->BeginRagdoll(FVector(Hit.LaunchVelocity.X, Hit.LaunchVelocity.Y, Hit.LaunchVelocity.Z), static_cast<EGTPImpactSeverity>(static_cast<int32>(Hit.Severity)), 100.f - Hit.Damage, Hit.SpinRadS);
		GTP_TRACE(LogGTPRagdoll, TEXT("GTPRagdollTest: car %.0f km/h -> severity %d, damage %.0f, ragdoll started %d"), CarKmh[Step], static_cast<int32>(Hit.Severity), Hit.Damage, bOk ? 1 : 0);
		if (!bOk)
		{
			Fail(FString::Printf(TEXT("ragdoll refused at %.0f km/h"), CarKmh[Step]));
		}
		Bodies.Add(Body);
		++Step;
		StepTime = 0.f;
		return;
	}
	if (Step == 3)
	{
		// Wait for the three hits to resolve: knocked and launched get up, the heavy one stays down.
		bool bBusy = false;
		for (const TWeakObjectPtr<ACharacter>& B : Bodies)
		{
			if (const UGTPRagdollComponent* R = B.IsValid() ? B->FindComponentByClass<UGTPRagdollComponent>() : nullptr)
			{
				bBusy |= R->IsRagdoll();
			}
		}
		// The 70 km/h body freezes at the pause end (StayDown): all three finish.
		if (!bBusy)
		{
			GTP_TRACE(LogGTPRagdoll, TEXT("GTPRagdollTest: all three car hits resolved in %.1f s"), StepTime);
			++Step;
			StepTime = 0.f;
		}
		else if (StepTime > StepTimeout)
		{
			Fail(TEXT("a ragdoll never ended"));
			Finish();
		}
		return;
	}
	if (Step == 4 || Step == 5)
	{
		// Falls: drop a fresh body from 3 m then 8 m, the landing must give a stumble (3 m) or a ragdoll (8 m).
		if (WaitBudget == 0.f)
		{
			ACharacter* Body = SpawnBody(FVector(-300.f, 300.f * (Step - 3), (Step == 4 ? 300.f : 800.f)));
			if (!Body)
			{
				Fail(TEXT("cannot spawn a falling body"));
				Finish();
				return;
			}
			if (!Body->FindComponentByClass<UGTPPlayerReactionComponent>())
			{
				UGTPPlayerReactionComponent* Reaction = NewObject<UGTPPlayerReactionComponent>(Body, TEXT("GTPReaction"));
				Reaction->RegisterComponent();
			}
			Bodies.Add(Body);
			WaitBudget = 1.f;
			StepTime = 0.f;
			return;
		}
		ACharacter* Body = Bodies.Last().Get();
		const UGTPPlayerReactionComponent* Reaction = Body ? Body->FindComponentByClass<UGTPPlayerReactionComponent>() : nullptr;
		const UGTPRagdollComponent* R = Body ? Body->FindComponentByClass<UGTPRagdollComponent>() : nullptr;
		const bool bLanded = Body && Body->GetCharacterMovement()->IsMovingOnGround();
		const bool bRagdoll = R && R->IsRagdoll();
		if (bLanded || bRagdoll || StepTime > StepTimeout)
		{
			const float Health = Reaction ? Reaction->GetHealth() : 100.f;
			GTP_TRACE(LogGTPRagdoll, TEXT("GTPRagdollTest: fall of %d m -> health %.0f, ragdoll %d"), Step == 4 ? 3 : 8, Health, bRagdoll ? 1 : 0);
			if (Step == 4 && bRagdoll)
			{
				Fail(TEXT("3 m fall must not ragdoll"));
			}
			if (Step == 5 && !bRagdoll && Health >= 100.f)
			{
				Fail(TEXT("8 m fall had no effect"));
			}
			WaitBudget = 0.f;
			++Step;
			StepTime = 0.f;
		}
		return;
	}
	if (Step == 6)
	{
		// Four at once: the hard limit refuses the ones over the cap.
		int32 Started = 0;
		int32 Refused = 0;
		for (int32 I = 0; I < 4; ++I)
		{
			ACharacter* Body = SpawnBody(FVector(-600.f, 300.f * I, 0.f));
			UGTPRagdollComponent* R = Body ? Body->FindComponentByClass<UGTPRagdollComponent>() : nullptr;
			if (R && R->BeginRagdoll(FVector(300.f, 0.f, 150.f), EGTPImpactSeverity::Knock, 80.f))
			{
				++Started;
				Bodies.Add(Body);
			}
			else
			{
				++Refused;
			}
		}
		const UGTPRagdollSubsystem* Sub = GetWorld()->GetSubsystem<UGTPRagdollSubsystem>();
		GTP_TRACE(LogGTPRagdoll, TEXT("GTPRagdollTest: four at once -> started %d, refused %d, simulating %d"), Started, Refused, Sub ? Sub->GetActiveCount() : -1);
		if (Started + Refused != 4 || (Sub && Sub->GetActiveCount() > Started))
		{
			Fail(TEXT("cap accounting wrong"));
		}
		if (Started < 1)
		{
			Fail(TEXT("no ragdoll started"));
		}
		++Step;
		StepTime = 0.f;
		return;
	}
	if (Step == 7 && StepTime > 6.f)
	{
		Finish();
	}
}
