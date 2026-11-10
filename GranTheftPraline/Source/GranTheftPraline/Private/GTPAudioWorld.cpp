#include "GTPAudioWorld.h"

#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GTPAudioMix.h"
#include "GTPAudioSettings.h"
#include "GTPEngineSoundComponent.h"
#include "GTPFootstepComponent.h"
#include "GTPLog.h"
#include "GTPSirenComponent.h"
#include "GTPVehicleAudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
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
	SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UGTPAudioWorld::OnActorSpawned));
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		Equip(*It);
	}
	if (UGTPAudioMix* Mix = UGTPAudioMix::Get(&InWorld))
	{
		UGameplayStatics::PushSoundMixModifier(&InWorld, Mix->GetUserMix());
	}
	GTP_TRACE(LogGTPAudioWorld, TEXT("[Audio] world %s: %d actors equipped at start"), *InWorld.GetMapName(), Equipped);
}

void UGTPAudioWorld::Deinitialize()
{
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
	Equip(Actor);
}

void UGTPAudioWorld::Equip(AActor* Actor)
{
	if (!Actor || Actor->IsTemplate())
	{
		return;
	}
	bool bAdded = false;
	if ((Cast<ACharacter>(Actor) || UGTPAudioMix::IsPedestrian(Actor)) && !Actor->FindComponentByClass<UGTPFootstepComponent>())
	{
		UGTPFootstepComponent* Feet = NewObject<UGTPFootstepComponent>(Actor, MakeUniqueObjectName(Actor, UGTPFootstepComponent::StaticClass(), TEXT("GTPFootsteps")));
		Feet->RegisterComponent();
		bAdded = true;
	}
	if (Cast<AWheeledVehiclePawn>(Actor))
	{
		if (!Actor->FindComponentByClass<UGTPVehicleAudioComponent>())
		{
			UGTPVehicleAudioComponent* Body = NewObject<UGTPVehicleAudioComponent>(Actor, MakeUniqueObjectName(Actor, UGTPVehicleAudioComponent::StaticClass(), TEXT("GTPBodyAudio")));
			Body->SetupAttachment(Actor->GetRootComponent());
			Body->RegisterComponent();
			bAdded = true;
		}
		if (UGTPAudioMix* Mix = UGTPAudioMix::Get(Actor))
		{
			TArray<UGTPEngineSoundComponent*> Engines;
			Actor->GetComponents(Engines);
			for (UGTPEngineSoundComponent* Engine : Engines)
			{
				Engine->SoundClassOverride = Mix->GetClass(EGTPAudioCategory::Vehicles);
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
	GTP_TRACE(LogGTPAudioWorld, TEXT("[Audio] steps=%d impacts=%d sirens_on=%d skids_on=%d walkers=%d driving=%d bank=%d"),
		Mix ? Mix->FootstepsPlayed : 0, Mix ? Mix->ImpactsPlayed : 0, Sirens, Skids, Feet, bDrivingMixOn ? 1 : 0, Mix ? Mix->GetBankSize() : 0);
}
