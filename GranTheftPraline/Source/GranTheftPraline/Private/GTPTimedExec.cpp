#include "GTPTimedExec.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GTPLog.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(TimedExec, Log, All);

bool UGTPTimedExec::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

bool UGTPTimedExec::ParseList(const FString& List, TArray<TPair<float, FString>>& Out)
{
	TArray<FString> Entries;
	List.ParseIntoArray(Entries, TEXT("|"));
	for (const FString& Entry : Entries)
	{
		FString Time, Command;
		if (!Entry.Split(TEXT("="), &Time, &Command) || Command.TrimStartAndEnd().IsEmpty()
			|| !Time.TrimStartAndEnd().IsNumeric())
		{
			return false;
		}
		// '+' stands for a space: launch scripts split their game arguments on spaces.
		Out.Emplace(FCString::Atof(*Time.TrimStartAndEnd()), Command.TrimStartAndEnd().Replace(TEXT("+"), TEXT(" ")));
	}
	return Out.Num() > 0;
}

void UGTPTimedExec::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString List;
	if (!FParse::Value(FCommandLine::Get(), TEXT("ExecAt="), List, false))
	{
		return;
	}
	TArray<TPair<float, FString>> Entries;
	if (!ParseList(List, Entries))
	{
		GTP_TRACE(TimedExec, TEXT("TimedExec: -ExecAt=%s not understood (want seconds=command|...)"), *List);
		return;
	}
	TWeakObjectPtr<UWorld> WeakWorld(&InWorld);
	for (const TPair<float, FString>& Entry : Entries)
	{
		const float At = FMath::Max(0.1f, Entry.Key);
		const FString Command = Entry.Value;
		FTimerHandle& Handle = Timers.AddDefaulted_GetRef();
		InWorld.GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakWorld, Command, At]()
		{
			if (UWorld* World = WeakWorld.Get())
			{
				GTP_TRACE(TimedExec, TEXT("TimedExec: t=%.1f %s"), At, *Command);
				if (GEngine)
				{
					GEngine->Exec(World, *Command);
				}
			}
		}), At, false);
	}
}
