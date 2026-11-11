#include "GTPMissionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "GTPLog.h"
#include "GTPVehicle.h"
#include "GTPWanted.h"

DEFINE_LOG_CATEGORY(LogGTPMission);

namespace
{
FString ToFString(const std::string& Text)
{
	return FString(UTF8_TO_TCHAR(Text.c_str()));
}
std::string ToStd(const FString& Text)
{
	return std::string(TCHAR_TO_UTF8(*Text));
}
FString DataDir()
{
	return FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Missions"));
}
} // namespace

bool UGTPMissionSubsystem::IsEnabled()
{
	int32 Value = 1;
	FParse::Value(FCommandLine::Get(), TEXT("Missions="), Value);
	return Value != 0;
}

bool UGTPMissionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return IsEnabled() && World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGTPMissionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPMissionSubsystem, STATGROUP_Tickables);
}

UGTPMissionSubsystem* UGTPMissionSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UGTPMissionSubsystem>() : nullptr;
}

FString UGTPMissionSubsystem::SavePath()
{
	// Interim file until the SaveGame of ADR-0013 carries FSaveData.
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames/GTPMissions.txt"));
}

static void MissionConsole(const TArray<FString>& Args, UWorld* World, int32 Which)
{
	UGTPMissionSubsystem* Missions = UGTPMissionSubsystem::Get(World);
	if (!Missions)
	{
		UE_LOG(LogGTPMission, Warning, TEXT("mission: off (-Missions=0)"));
		return;
	}
	switch (Which)
	{
	case 0:
		if (Args.Num() > 0)
		{
			Missions->StartMission(FName(*Args[0]));
		}
		break;
	case 1:
		Missions->RetryMission();
		break;
	case 2:
		Missions->AbortMission();
		break;
	default:
		if (Args.Num() > 0)
		{
			if (AGTPVehicle* Car = Cast<AGTPVehicle>(UGameplayStatics::GetPlayerPawn(World, 0)))
			{
				UGTPMissionSubsystem::TagVehicle(Car, FName(*Args[0]));
			}
		}
		break;
	}
}

static FAutoConsoleCommandWithWorldAndArgs GTPMissionStartCmd(TEXT("GTPMission.Start"), TEXT("Start a mission by id"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { MissionConsole(A, W, 0); }));
static FAutoConsoleCommandWithWorldAndArgs GTPMissionRetryCmd(TEXT("GTPMission.Retry"), TEXT("Retry the failed mission"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { MissionConsole(A, W, 1); }));
static FAutoConsoleCommandWithWorldAndArgs GTPMissionAbortCmd(TEXT("GTPMission.Abort"), TEXT("Give up the mission"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { MissionConsole(A, W, 2); }));
static FAutoConsoleCommandWithWorldAndArgs GTPMissionTagCmd(TEXT("GTPMission.TagVehicle"), TEXT("Tag the vehicle the player drives"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& A, UWorld* W) { MissionConsole(A, W, 3); }));

bool UGTPMissionSubsystem::LoadTables()
{
	FString Missions, Objectives, Zones;
	const FString Dir = DataDir();
	if (!FFileHelper::LoadFileToString(Missions, *FPaths::Combine(Dir, TEXT("missions.csv"))) || !FFileHelper::LoadFileToString(Objectives, *FPaths::Combine(Dir, TEXT("objectives.csv"))) || !FFileHelper::LoadFileToString(Zones, *FPaths::Combine(Dir, TEXT("zones.csv"))))
	{
		UE_LOG(LogGTPMission, Warning, TEXT("mission: tables not found in %s, missions off"), *Dir);
		return false;
	}
	GTPMission::LoadTables(ToStd(Missions), ToStd(Objectives), ToStd(Zones), Tables);
	for (const std::string& Error : Tables.Errors)
	{
		UE_LOG(LogGTPMission, Error, TEXT("mission: table error: %s"), *ToFString(Error));
	}
	return Tables.Errors.empty();
}

void UGTPMissionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bAutoStart = FParse::Param(FCommandLine::Get(), TEXT("MissionAutoStart"));
	bTablesOk = LoadTables();
	if (!bTablesOk)
	{
		return;
	}
	for (const GTPMission::FMission& Mission : Tables.Missions)
	{
		for (const GTPMission::FObjective& Objective : Mission.Objectives)
		{
			if (Objective.Type == GTPMission::EObjectiveType::EnterVehicle)
			{
				VehicleTags.insert(Objective.Target);
			}
		}
	}
	Runner = MakeUnique<GTPMission::FRunner>(&Tables);
	LoadSave();
	UE_LOG(LogGTPMission, Log, TEXT("mission: %d missions, %d zones"), static_cast<int32>(Tables.Missions.size()), static_cast<int32>(Tables.Zones.size()));
}

void UGTPMissionSubsystem::Deinitialize()
{
	if (Runner && Runner->SaveDirty())
	{
		WriteSave();
	}
	Runner.Reset();
	Super::Deinitialize();
}

void UGTPMissionSubsystem::LoadSave()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *SavePath()))
	{
		return;
	}
	GTPMission::FSaveData Data;
	if (GTPMission::DeserializeSave(ToStd(Text), Data))
	{
		Runner->Restore(Data);
		Execute(Runner->TakeCommands());
	}
	else
	{
		UE_LOG(LogGTPMission, Warning, TEXT("mission: save file unreadable, starting clean"));
	}
}

void UGTPMissionSubsystem::WriteSave()
{
	const FString Path = SavePath();
	const FString Temp = Path + TEXT(".tmp");
	const FString Text = ToFString(GTPMission::SerializeSave(Runner->Save()));
	// Write beside, then move over: a crash never leaves half a file.
	if (FFileHelper::SaveStringToFile(Text, *Temp))
	{
		IFileManager::Get().Move(*Path, *Temp, true, true);
		Runner->ClearSaveDirty();
	}
}

bool UGTPMissionSubsystem::StartMission(FName MissionId)
{
	if (!Runner)
	{
		return false;
	}
	const std::string Id = ToStd(MissionId.ToString());
	const std::string Why = Runner->CannotStartReason(Id);
	if (!Why.empty())
	{
		UE_LOG(LogGTPMission, Log, TEXT("mission: cannot start %s: %s"), *MissionId.ToString(), *ToFString(Why));
		return false;
	}
	BoundTag = NAME_None;
	BoundVehicle.Reset();
	const bool bOk = Runner->Start(Id);
	Execute(Runner->TakeCommands());
	return bOk;
}

bool UGTPMissionSubsystem::RetryMission()
{
	if (!Runner)
	{
		return false;
	}
	const bool bOk = Runner->Retry();
	Execute(Runner->TakeCommands());
	return bOk;
}

void UGTPMissionSubsystem::AbortMission()
{
	if (Runner)
	{
		Runner->Abort();
		Execute(Runner->TakeCommands());
		CurrentHint = NAME_None;
		WriteSave();
	}
}

void UGTPMissionSubsystem::TagVehicle(AGTPVehicle* Vehicle, FName Tag)
{
	if (Vehicle)
	{
		Vehicle->Tags.AddUnique(Tag);
		UE_LOG(LogGTPMission, Log, TEXT("mission: vehicle tagged %s"), *Tag.ToString());
	}
}

void UGTPMissionSubsystem::ReportEvent(FName Name, FName Subject, float Value)
{
	if (Runner)
	{
		GTPMission::FEvent Event;
		Event.Name = ToStd(Name.ToString());
		Event.Subject = ToStd(Subject.ToString());
		Event.Value = Value;
		Runner->OnEvent(Event);
		Execute(Runner->TakeCommands());
	}
}

bool UGTPMissionSubsystem::GetWaypoint(FVector& OutLocationCm, float& OutRadiusCm) const
{
	const GTPMission::FZone* Zone = WaypointZone.IsEmpty() ? nullptr : Tables.FindZone(ToStd(WaypointZone));
	if (!Zone)
	{
		return false;
	}
	OutLocationCm = ToCm(Zone->X, Zone->Y);
	OutRadiusCm = Zone->RadiusM * 100.f;
	return true;
}

TArray<UGTPMissionSubsystem::FStartMarker> UGTPMissionSubsystem::GetStartMarkers() const
{
	TArray<FStartMarker> Markers;
	if (!Runner)
	{
		return Markers;
	}
	for (const GTPMission::FMission& Mission : Tables.Missions)
	{
		const GTPMission::FZone* Zone = Tables.FindZone(Mission.StartZone);
		if (!Zone)
		{
			continue;
		}
		FStartMarker Marker;
		Marker.MissionId = FName(*ToFString(Mission.Id));
		Marker.TitleKey = FName(*ToFString(Mission.TitleKey));
		Marker.LocationCm = ToCm(Zone->X, Zone->Y);
		Marker.RadiusCm = Zone->RadiusM * 100.f;
		Marker.bAvailable = Runner->CannotStartReason(Mission.Id).empty();
		Markers.Add(Marker);
	}
	return Markers;
}

GTPMission::FContext UGTPMissionSubsystem::BuildContext()
{
	GTPMission::FContext Ctx;
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn)
	{
		return Ctx;
	}
	const FVector Location = Pawn->GetActorLocation();
	Ctx.X = Location.X / 100.f;
	Ctx.Y = -Location.Y / 100.f;
	if (const UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
	{
		Ctx.Stars = Wanted->GetStars();
	}
	if (const AGTPVehicle* Car = Cast<AGTPVehicle>(Pawn))
	{
		Ctx.bInVehicle = true;
		Ctx.VehicleHealth = Car->GetHealthRatio();
		for (const FName& Tag : Car->Tags)
		{
			if (VehicleTags.count(ToStd(Tag.ToString())))
			{
				Ctx.VehicleTag = ToStd(Tag.ToString());
				BoundVehicle = const_cast<AGTPVehicle*>(Car);
				BoundTag = Tag;
				break;
			}
		}
	}
	if (BoundVehicle.IsValid())
	{
		Ctx.BoundVehicleDistM = FVector::Dist(Location, BoundVehicle->GetActorLocation()) / 100.f;
	}
	return Ctx;
}

void UGTPMissionSubsystem::PollWorldEvents(const GTPMission::FContext& Ctx)
{
	// Arrest: the wanted component shows its banner while the player is taken; no delegate on next yet
	// (feat/wanted-api adds OnArrested, then this poll goes away).
	if (const UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
	{
		const bool bBusted = Wanted->GetBustedBannerTime() > 0.f;
		if (bBusted && !bWasBusted)
		{
			ReportEvent(TEXT("player.arrested"));
		}
		bWasBusted = bBusted;
	}
	if (Runner->GetStatus() == GTPMission::EStatus::Active && BoundVehicle.IsValid() && BoundVehicle->IsWrecked())
	{
		ReportEvent(TEXT("vehicle.destroyed"), BoundTag);
	}
	(void)Ctx;
}

void UGTPMissionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Runner)
	{
		return;
	}
	const GTPMission::FContext Ctx = BuildContext();
	PollWorldEvents(Ctx);
	Runner->Tick(DeltaTime, Ctx);
	Execute(Runner->TakeCommands());

	if (Runner->GetStatus() != GTPMission::EStatus::Active)
	{
		// Offer the mission whose start zone the player stands in (once per entry).
		FName Offer = NAME_None;
		for (const GTPMission::FMission& Mission : Tables.Missions)
		{
			const GTPMission::FZone* Zone = Tables.FindZone(Mission.StartZone);
			if (Zone && Runner->GetStatus() != GTPMission::EStatus::Failed && Runner->CannotStartReason(Mission.Id).empty())
			{
				const float DX = Ctx.X - Zone->X, DY = Ctx.Y - Zone->Y;
				if (DX * DX + DY * DY <= Zone->RadiusM * Zone->RadiusM)
				{
					Offer = FName(*ToFString(Mission.Id));
					break;
				}
			}
		}
		if (Offer != OfferedMission)
		{
			OfferedMission = Offer;
			if (!Offer.IsNone())
			{
				OnStartAvailable.Broadcast(Offer);
				if (bAutoStart)
				{
					StartMission(Offer);
				}
			}
		}
	}
	else
	{
		OfferedMission = NAME_None;
	}

	LogTimer += DeltaTime;
	if (Runner->GetStatus() == GTPMission::EStatus::Active && LogTimer >= 5.f)
	{
		LogTimer = 0.f;
		UE_LOG(LogGTPMission, Log, TEXT("mission: %s objective %d stars %d"), *ToFString(Runner->GetActiveId()), Runner->GetObjectiveIndex(), Ctx.Stars);
	}
	if (Runner->SaveDirty())
	{
		WriteSave();
	}
}

void UGTPMissionSubsystem::Execute(const std::vector<GTPMission::FCommand>& Commands)
{
	using GTPMission::ECommand;
	for (const GTPMission::FCommand& Command : Commands)
	{
		const FName MissionId(*ToFString(Command.MissionId));
		switch (Command.Kind)
		{
		case ECommand::MissionStarted:
			UE_LOG(LogGTPMission, Log, TEXT("mission: started %s"), *MissionId.ToString());
			break;
		case ECommand::ShowObjective:
			CurrentHint = FName(*ToFString(Command.Key));
			OnObjective.Broadcast(MissionId, CurrentHint);
			UE_LOG(LogGTPMission, Log, TEXT("mission: %s objective %d (%s)"), *MissionId.ToString(), Command.Amount, *CurrentHint.ToString());
			break;
		case ECommand::SetWaypoint:
			WaypointZone = ToFString(Command.Key);
			break;
		case ECommand::ClearWaypoint:
			WaypointZone.Reset();
			break;
		case ECommand::EnsureStars:
			// Never lowers the level. EnsureMinStars(n) of feat/wanted-api replaces this once on next.
			if (UGTPWantedComponent* Wanted = UGTPWantedComponent::Find(this))
			{
				if (Wanted->GetStars() < Command.Amount)
				{
					Wanted->ForceStars(Command.Amount);
				}
			}
			break;
		case ECommand::SpawnVehicle:
			// Needs AGTPTrafficNetwork::SpawnParkedVehicle (traffic thread proposal). Until then a car
			// carrying the tag must exist in the level, or be tagged with GTPMission.TagVehicle.
			UE_LOG(LogGTPMission, Log, TEXT("mission: needs a vehicle tagged %s near zone %s"), *ToFString(Command.Key), *ToFString(Command.Extra));
			break;
		case ECommand::Checkpoint:
			WriteSave();
			break;
		case ECommand::GiveReward:
			UE_LOG(LogGTPMission, Log, TEXT("mission: reward %d (%s)"), Command.Amount, *ToFString(Command.RunKey));
			OnReward.Broadcast(Command.Amount, FName(*ToFString(Command.RunKey)), MissionId);
			break;
		case ECommand::MissionSucceeded:
			UE_LOG(LogGTPMission, Log, TEXT("mission: succeeded %s"), *MissionId.ToString());
			OnSucceeded.Broadcast(MissionId, Command.Amount);
			if (Runner)
			{
				Runner->Acknowledge();
			}
			CurrentHint = NAME_None;
			break;
		case ECommand::MissionFailed:
			UE_LOG(LogGTPMission, Log, TEXT("mission: failed %s: %s"), *MissionId.ToString(), *ToFString(Command.Key));
			OnFailed.Broadcast(FName(*ToFString(Command.Key)));
			break;
		}
	}
}
