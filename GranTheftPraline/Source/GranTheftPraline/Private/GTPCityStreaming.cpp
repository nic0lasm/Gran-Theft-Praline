#include "GTPCityStreaming.h"

#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GTPLog.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogGTPCityStreaming, Log, All);

namespace
{
const FName ProxyTag(TEXT("GTP_CellProxy"));

bool ParseBounds(const FString& Tag, GTPCity::FCellBox& Out)
{
	FString Values;
	if (!Tag.Split(TEXT("bounds:"), nullptr, &Values))
	{
		return false;
	}
	TArray<FString> Parts;
	Values.ParseIntoArray(Parts, TEXT(","));
	if (Parts.Num() != 4)
	{
		return false;
	}
	Out.X0 = FCString::Atof(*Parts[0]);
	Out.Y0 = FCString::Atof(*Parts[1]);
	Out.X1 = FCString::Atof(*Parts[2]);
	Out.Y1 = FCString::Atof(*Parts[3]);
	return Out.X1 > Out.X0 && Out.Y1 > Out.Y0;
}
} // namespace

bool UGTPCityStreaming::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGTPCityStreaming::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPCityStreaming, STATGROUP_Tickables);
}

void UGTPCityStreaming::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Cells.Reset();
	Boxes.clear();
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor->Tags.Contains(ProxyTag))
		{
			continue;
		}
		FCell Cell;
		GTPCity::FCellBox Box;
		bool bBox = false;
		for (const FName& Tag : Actor->Tags)
		{
			const FString S = Tag.ToString();
			if (S.StartsWith(TEXT("/Game/")))
			{
				Cell.Level = S;
				FString Left;
				S.Split(TEXT("/"), &Left, &Cell.Name, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			}
			else if (S.StartsWith(TEXT("bounds:")))
			{
				bBox = ParseBounds(S, Box);
			}
		}
		if (Cell.Level.IsEmpty() || !bBox)
		{
			GTP_TRACE(CityStreaming, TEXT("proxy %s has no level or bounds tag, ignored"), *Actor->GetName());
			continue;
		}
		Cell.Proxy = Actor;
		Cells.Add(MoveTemp(Cell));
		Boxes.push_back(Box);
	}
	int32 Flag = 1;
	FParse::Value(FCommandLine::Get(), TEXT("CityCells="), Flag);
	float M = 0.f;
	Rules.LoadRadius = (FParse::Value(FCommandLine::Get(), TEXT("CityCellLoad="), M) ? M : LoadRadiusM) * 100.f;
	Rules.UnloadRadius = FMath::Max(Rules.LoadRadius, (FParse::Value(FCommandLine::Get(), TEXT("CityCellUnload="), M) ? M : UnloadRadiusM) * 100.f);
	int32 Max = MaxDetailed;
	FParse::Value(FCommandLine::Get(), TEXT("CityCellMax="), Max);
	Rules.MaxDetailed = FMath::Max(0, Max);
	Rules.MaxLoadsPerUpdate = 1;
	bEnabled = Cells.Num() > 0 && Flag != 0;
	if (Cells.Num() > 0)
	{
		GTP_TRACE(CityStreaming, TEXT("%d cells, %s, load %.0f m, unload %.0f m, max %d"), Cells.Num(),
		          bEnabled ? TEXT("on") : TEXT("off (-CityCells=0)"), Rules.LoadRadius / 100.f, Rules.UnloadRadius / 100.f,
		          Rules.MaxDetailed);
	}
	SinceUpdate = UpdateSeconds;   // first plan on the first tick
}

int32 UGTPCityStreaming::GetVisibleDetailedCount() const
{
	int32 N = 0;
	for (const FCell& Cell : Cells)
	{
		N += Cell.Streaming.IsValid() && Cell.Streaming->IsLevelVisible() ? 1 : 0;
	}
	return N;
}

void UGTPCityStreaming::Tick(float DeltaTime)
{
	if (!bEnabled)
	{
		return;
	}
	// Proxy swap every frame: cheap (a handful of cells), and the proxy must go the frame the level shows.
	for (FCell& Cell : Cells)
	{
		const bool bVisible = Cell.bRequested && Cell.Streaming.IsValid() && Cell.Streaming->IsLevelVisible();
		if (bVisible != Cell.bWasVisible)
		{
			Cell.bWasVisible = bVisible;
			if (AActor* Proxy = Cell.Proxy.Get())
			{
				Proxy->SetActorHiddenInGame(bVisible);
			}
			if (bVisible)
			{
				GTP_TRACE(CityStreaming, TEXT("visible %s t=%.2f"), *Cell.Name, GetWorld()->GetTimeSeconds());
			}
		}
	}
	SinceUpdate += DeltaTime;
	if (SinceUpdate >= UpdateSeconds)
	{
		SinceUpdate = 0.f;
		Update();
	}
}

void UGTPCityStreaming::Update()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const FVector Where = Pawn ? Pawn->GetActorLocation() : (PC && PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector);
	if (!PC)
	{
		return;
	}
	std::vector<bool> Loaded(Cells.Num(), false);
	for (int32 I = 0; I < Cells.Num(); ++I)
	{
		Loaded[I] = Cells[I].bRequested;
	}
	const GTPCity::FStreamingPlan Plan = GTPCity::PlanStreaming(Boxes, Loaded, Where.X, Where.Y, Rules);
	for (const int I : Plan.Unload)
	{
		Release(Cells[I]);
	}
	for (const int I : Plan.Load)
	{
		RequestLoad(Cells[I]);
	}
}

void UGTPCityStreaming::RequestLoad(FCell& Cell)
{
	Cell.bRequested = true;
	if (ULevelStreamingDynamic* Streaming = Cell.Streaming.Get())
	{
		Streaming->SetShouldBeLoaded(true);
		Streaming->SetShouldBeVisible(true);
	}
	else
	{
		bool bOk = false;
		Streaming = ULevelStreamingDynamic::LoadLevelInstance(GetWorld(), Cell.Level, FVector::ZeroVector, FRotator::ZeroRotator, bOk);
		if (!bOk || !Streaming)
		{
			GTP_TRACE(CityStreaming, TEXT("cannot load %s: proxy kept"), *Cell.Level);
			return;   // stays requested so it is not retried every update; the proxy stays visible
		}
		Cell.Streaming = Streaming;
	}
	GTP_TRACE(CityStreaming, TEXT("load %s t=%.2f"), *Cell.Name, GetWorld()->GetTimeSeconds());
}

void UGTPCityStreaming::Release(FCell& Cell)
{
	Cell.bRequested = false;
	// The proxy comes back first, so the cell never shows a hole while its level unloads.
	if (AActor* Proxy = Cell.Proxy.Get())
	{
		Proxy->SetActorHiddenInGame(false);
	}
	Cell.bWasVisible = false;
	if (ULevelStreamingDynamic* Streaming = Cell.Streaming.Get())
	{
		Streaming->SetShouldBeVisible(false);
		Streaming->SetShouldBeLoaded(false);
	}
	GTP_TRACE(CityStreaming, TEXT("unload %s t=%.2f"), *Cell.Name, GetWorld()->GetTimeSeconds());
}
