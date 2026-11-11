#include "GTPAudioWorld.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GTPAmbienceBed.h"
#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "GTPEngineSoundComponent.h"
#include "GTPFootstepComponent.h"
#include "GTPLog.h"
#include "GTPSirenComponent.h"
#include "GTPStreetReactions.h"
#include "GTPVehicle.h"
#include "GTPVehicleAudioComponent.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "WheeledVehiclePawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPAudioWorld, Log, All);

bool UGTPAudioWorld::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return UGTPAudioMix::IsGameAudioEnabled() && World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) && Super::ShouldCreateSubsystem(Outer);
}

void UGTPAudioWorld::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bLogStats = GetDefault<UGTPAudioSettings>()->bLogStats || FParse::Param(FCommandLine::Get(), TEXT("AudioStats"));
	// Sound for the filmed runs: same delay and length as UGTPFrameCapture (-CaptureDelay, -CaptureVideo).
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("RecordAudio")) || FParse::Value(Cmd, TEXT("RecordAudio="), RecordSeconds))
	{
		float CaptureSeconds = 0.f;
		if (RecordSeconds <= 0.f && FParse::Value(Cmd, TEXT("CaptureVideo="), CaptureSeconds))
		{
			RecordSeconds = CaptureSeconds;
		}
		RecordSeconds = RecordSeconds > 0.f ? RecordSeconds : 60.f;
		RecordDelay = 0.f;
		FParse::Value(Cmd, TEXT("CaptureDelay="), RecordDelay);
		if (!FParse::Value(Cmd, TEXT("RecordAudioDir="), RecordDir))
		{
			RecordDir = FPaths::ProjectSavedDir() / TEXT("AudioCapture");
		}
	}
	bTestSiren = FParse::Param(Cmd, TEXT("TestSiren"));
	SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UGTPAudioWorld::OnActorSpawned));
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		Equip(*It);
	}
	if (UGTPAudioMix* Mix = UGTPAudioMix::Get(&InWorld))
	{
		UGameplayStatics::PushSoundMixModifier(&InWorld, Mix->GetUserMix());
	}
	// -AudioBed=0: no ambience bed (crash hunt A/B switch; its layers stop and restart during play).
	int32 BedSwitch = 1;
	FParse::Value(FCommandLine::Get(), TEXT("AudioBed="), BedSwitch);
	if (BedSwitch != 0)
	{
		Bed = NewObject<UGTPAmbienceBed>(this);
		Bed->Start(&InWorld);
	}
	Reactions = NewObject<UGTPStreetReactions>(this);
	Reactions->Start(&InWorld);
	GTP_TRACE(LogGTPAudioWorld, TEXT("[Audio] world %s: %d actors equipped at start; bed %s"), *InWorld.GetMapName(), Equipped, Bed ? *Bed->Describe() : TEXT("off"));
}

void UGTPAudioWorld::Deinitialize()
{
	if (Bed)
	{
		Bed->Stop();
		Bed = nullptr;
	}
	if (Reactions)
	{
		Reactions->Stop();
		Reactions = nullptr;
	}
	PendingEquip.Reset();
	Walkers.Reset();
	Engines.Reset();
	OnLoudSound.Clear();
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Super::Deinitialize();
}

TStatId UGTPAudioWorld::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPAudioWorld, STATGROUP_Tickables);
}

void UGTPAudioWorld::OnActorSpawned(AActor* Actor)
{
	// Spawn callbacks can come before construction and component registration end
	// (deferred spawns): adding components there can leave them half registered.
	if (Actor)
	{
		PendingEquip.Add(Actor);
	}
}

void UGTPAudioWorld::Equip(AActor* Actor)
{
	// Never equip an actor on its way out (or not in a world any more).
	if (!Actor || Actor->IsTemplate() || Actor->IsActorBeingDestroyed() || !IsValid(Actor) || Actor->GetWorld() != GetWorld())
	{
		return;
	}
	bool bAdded = false;
	const bool bPedestrian = UGTPAudioMix::IsPedestrian(Actor);
	if (bPedestrian)
	{
		Pedestrians.AddUnique(Actor);
	}
	if ((Cast<ACharacter>(Actor) || bPedestrian) && !Actor->FindComponentByClass<UGTPFootstepComponent>())
	{
		UGTPFootstepComponent* Feet = NewObject<UGTPFootstepComponent>(Actor, MakeUniqueObjectName(Actor, UGTPFootstepComponent::StaticClass(), TEXT("GTPFootsteps")), RF_Transient);
		Actor->AddInstanceComponent(Feet);
		Feet->RegisterComponent();
		Walkers.Add(Feet);
		bAdded = true;
	}
	if (Cast<AWheeledVehiclePawn>(Actor))
	{
		if (!Actor->FindComponentByClass<UGTPVehicleAudioComponent>())
		{
			UGTPVehicleAudioComponent* Body = NewObject<UGTPVehicleAudioComponent>(Actor, MakeUniqueObjectName(Actor, UGTPVehicleAudioComponent::StaticClass(), TEXT("GTPBodyAudio")), RF_Transient);
			Body->SetupAttachment(Actor->GetRootComponent());
			// An instance component: the actor owns it, saves it from GC and unregisters it with itself.
			Actor->AddInstanceComponent(Body);
			Body->RegisterComponent();
			bAdded = true;
		}
		if (UGTPAudioMix* Mix = UGTPAudioMix::Get(Actor))
		{
			TArray<UGTPEngineSoundComponent*> EngineComponents;
			Actor->GetComponents(EngineComponents);
			for (UGTPEngineSoundComponent* Engine : EngineComponents)
			{
				Engine->SoundClassOverride = Mix->GetClass(EGTPAudioCategory::Vehicles);
				Engines.AddUnique(Engine);
			}
		}
	}
	const FName SirenTag = GetDefault<UGTPAudioSettings>()->SirenActorTag;
	if (!SirenTag.IsNone() && Actor->ActorHasTag(SirenTag))
	{
		UGTPSirenComponent::FindOrAdd(Actor);
		bAdded = true;
	}
	Equipped += bAdded ? 1 : 0;
}

void UGTPAudioWorld::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	UGTPAudioMix* Mix = UGTPAudioMix::Get(World);
	if (!World || !Mix)
	{
		return;
	}
	const APlayerController* Player = World->GetFirstPlayerController();
	const bool bDriving = Player && Cast<AWheeledVehiclePawn>(Player->GetPawn()) != nullptr;
	if (bDriving != bDrivingMixOn)
	{
		bDrivingMixOn = bDriving;
		if (bDriving)
		{
			UGameplayStatics::PushSoundMixModifier(World, Mix->GetDrivingMix());
		}
		else
		{
			UGameplayStatics::PopSoundMixModifier(World, Mix->GetDrivingMix());
		}
	}
	if (PendingEquip.Num() > 0)
	{
		TArray<TWeakObjectPtr<AActor>> Ready = MoveTemp(PendingEquip);
		PendingEquip.Reset();
		for (const TWeakObjectPtr<AActor>& Weak : Ready)
		{
			AActor* Actor = Weak.Get();
			if (!Actor)
			{
				continue;
			}
			if (Actor->HasActorBegunPlay())
			{
				Equip(Actor);
			}
			else if (!Actor->IsActorBeingDestroyed())
			{
				PendingEquip.Add(Actor);
			}
		}
	}
	UpdateRecording();
	UpdateTestSiren(DeltaTime);
	UpdateBudget(DeltaTime);
	if (Bed)
	{
		Bed->Tick(DeltaTime, Pedestrians);
	}
	if (Reactions)
	{
		Reactions->Tick(DeltaTime);
	}
	if (bLogStats)
	{
		StatsTimer += DeltaTime;
		if (StatsTimer >= 1.f)
		{
			StatsTimer = 0.f;
			LogStats();
		}
	}
}

void UGTPAudioWorld::UpdateBudget(float DeltaTime)
{
	BudgetTimer -= DeltaTime;
	if (BudgetTimer > 0.f)
	{
		return;
	}
	const UGTPAudioSettings* Settings = GetDefault<UGTPAudioSettings>();
	BudgetTimer = FMath::Max(0.05f, Settings->BudgetInterval);
	UWorld* World = GetWorld();
	const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	if (!Player)
	{
		return;
	}
	FVector Listener, Front, Right;
	Player->GetAudioListenerPosition(Listener, Front, Right);
	const AActor* PlayerPawn = Player->GetPawn();

	// Nearest first; the player's own pawn or car ranks before everything.
	auto Rank = [&Listener, PlayerPawn](const UActorComponent* Component)
	{
		const AActor* Owner = Component->GetOwner();
		return Owner == PlayerPawn ? -1.f : FVector::DistSquared(Owner->GetActorLocation(), Listener);
	};

	{
		Engines.RemoveAll([](const TWeakObjectPtr<UGTPEngineSoundComponent>& Weak) { return !Weak.IsValid(); });
		TArray<TPair<float, UGTPEngineSoundComponent*>> Order;
		Order.Reserve(Engines.Num());
		for (const TWeakObjectPtr<UGTPEngineSoundComponent>& Weak : Engines)
		{
			UGTPEngineSoundComponent* Engine = Weak.Get();
			const AGTPVehicle* Car = Cast<AGTPVehicle>(Engine->GetOwner());
			if (Car && Car->IsEngineRunning())
			{
				Order.Emplace(Rank(Engine), Engine);
			}
			else
			{
				// Off engines keep the right to start: their own tick decides.
				Engine->SetAudible(true);
			}
		}
		Order.Sort([](const TPair<float, UGTPEngineSoundComponent*>& A, const TPair<float, UGTPEngineSoundComponent*>& B) { return A.Key < B.Key; });
		EnginesAudible = 0;
		for (const TPair<float, UGTPEngineSoundComponent*>& Entry : Order)
		{
			const float Silent = Entry.Value->SilentDistance;
			const bool bAudible = EnginesAudible < Settings->MaxEngineVoices && Entry.Key < FMath::Square(Silent);
			Entry.Value->SetAudible(bAudible);
			EnginesAudible += bAudible ? 1 : 0;
		}
	}
	{
		Walkers.RemoveAll([](const TWeakObjectPtr<UGTPFootstepComponent>& Weak) { return !Weak.IsValid(); });
		TArray<TPair<float, UGTPFootstepComponent*>> Order;
		Order.Reserve(Walkers.Num());
		for (const TWeakObjectPtr<UGTPFootstepComponent>& Weak : Walkers)
		{
			UGTPFootstepComponent* Feet = Weak.Get();
			if (Feet->GetOwner()->IsHidden())
			{
				Feet->SetInBudget(false);
				continue;
			}
			Order.Emplace(Rank(Feet), Feet);
		}
		Order.Sort([](const TPair<float, UGTPFootstepComponent*>& A, const TPair<float, UGTPFootstepComponent*>& B) { return A.Key < B.Key; });
		WalkersHeard = 0;
		for (const TPair<float, UGTPFootstepComponent*>& Entry : Order)
		{
			const bool bHeard = WalkersHeard < Settings->MaxStepWalkers && Entry.Key < FMath::Square(Settings->FootstepMaxDistance);
			Entry.Value->SetInBudget(bHeard);
			WalkersHeard += bHeard ? 1 : 0;
		}
	}
}

void UGTPAudioWorld::UpdateRecording()
{
	if (RecordDelay < 0.f)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float Now = World->GetRealTimeSeconds();
	if (!bRecording && Now >= RecordDelay)
	{
		bRecording = true;
		RecordStartedAt = Now;
		UAudioMixerBlueprintLibrary::StartRecordingOutput(World, RecordSeconds);
		GTP_TRACE(LogGTPAudioWorld, TEXT("[Audio] recording the output for %.0f s from %.1f s"), RecordSeconds, Now);
	}
	else if (bRecording && Now - RecordStartedAt >= RecordSeconds)
	{
		bRecording = false;
		RecordDelay = -1.f;
		IFileManager::Get().MakeDirectory(*RecordDir, true);
		UAudioMixerBlueprintLibrary::StopRecordingOutput(World, EAudioRecordingExportType::WavFile, TEXT("game_audio"), RecordDir);
		GTP_TRACE(LogGTPAudioWorld, TEXT("[Audio] recording written to %s/game_audio.wav"), *RecordDir);
	}
}

void UGTPAudioWorld::UpdateTestSiren(float DeltaTime)
{
	if (!bTestSiren)
	{
		return;
	}
	TestSirenTimer += DeltaTime;
	AActor* Car = TestSirenCar.Get();
	if (!Car)
	{
		// Every 2 s, look for the nearest car nobody plays.
		if (TestSirenTimer < 2.f)
		{
			return;
		}
		TestSirenTimer = 0.f;
		const APlayerController* Player = GetWorld()->GetFirstPlayerController();
		const APawn* PlayerPawn = Player ? Player->GetPawn() : nullptr;
		if (!PlayerPawn)
		{
			return;
		}
		float Best = TNumericLimits<float>::Max();
		for (TActorIterator<AWheeledVehiclePawn> It(GetWorld()); It; ++It)
		{
			if (*It == PlayerPawn || It->IsPlayerControlled())
			{
				continue;
			}
			const float Dist = FVector::DistSquared(It->GetActorLocation(), PlayerPawn->GetActorLocation());
			if (Dist < Best)
			{
				Best = Dist;
				Car = *It;
			}
		}
		if (Car)
		{
			TestSirenCar = Car;
			UGTPSirenComponent::SetSiren(Car, EGTPSirenMode::TwoTone);
			GTP_TRACE(LogGTPAudioWorld, TEXT("[Audio] test siren on %s, %.0f m away"), *Car->GetName(), FMath::Sqrt(Best) / 100.f);
		}
		return;
	}
	// 12 s of two-tone, 4 s of yelp, repeat.
	const float Phase = FMath::Fmod(TestSirenTimer, 16.f);
	UGTPSirenComponent::SetSiren(Car, Phase < 12.f ? EGTPSirenMode::TwoTone : EGTPSirenMode::Yelp);
}

void UGTPAudioWorld::LogStats()
{
	UGTPAudioMix* Mix = UGTPAudioMix::Get(GetWorld());
	int32 Sirens = 0;
	int32 Skids = 0;
	int32 Feet = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (const UGTPSirenComponent* Siren = It->FindComponentByClass<UGTPSirenComponent>())
		{
			Sirens += Siren->IsPlaying() ? 1 : 0;
		}
		if (const UGTPVehicleAudioComponent* Body = It->FindComponentByClass<UGTPVehicleAudioComponent>())
		{
			Skids += Body->IsPlaying() ? 1 : 0;
		}
		Feet += It->FindComponentByClass<UGTPFootstepComponent>() ? 1 : 0;
	}
	GTP_TRACE(LogGTPAudioWorld, TEXT("[Audio] steps=%d impacts=%d sirens_on=%d skids_on=%d walkers=%d heard=%d engines=%d/%d driving=%d bank=%d voices=%d radio=%d %s; %s"),
		Mix ? Mix->FootstepsPlayed : 0, Mix ? Mix->ImpactsPlayed : 0, Sirens, Skids, Feet, WalkersHeard, EnginesAudible, Engines.Num(), bDrivingMixOn ? 1 : 0, Mix ? Mix->GetBankSize() : 0,
		Mix ? Mix->VoicesPlayed : 0, Mix ? Mix->RadioPlayed : 0, Bed ? *Bed->Describe() : TEXT("bed=off"),
		Reactions ? *Reactions->Describe() : TEXT("reactions=off"));
}
