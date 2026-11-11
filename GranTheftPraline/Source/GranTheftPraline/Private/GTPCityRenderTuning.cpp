#include "GTPCityRenderTuning.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GTPLog.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(CityRenderTuning, Log, All);   // lines read "CityRenderTuning: ..." in every build

bool UGTPCityRenderTuning::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

bool UGTPCityRenderTuning::ParseCullList(const FString& List, TMap<FString, float>& Out)
{
	TArray<FString> Pairs;
	List.ParseIntoArray(Pairs, TEXT(","));
	for (const FString& Pair : Pairs)
	{
		FString Key, Value;
		if (!Pair.Split(TEXT("="), &Key, &Value) || Key.TrimStartAndEnd().IsEmpty())
		{
			return false;
		}
		Out.Add(Key.TrimStartAndEnd(), FCString::Atof(*Value));
	}
	return Out.Num() > 0;
}

void UGTPCityRenderTuning::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	FString List;
	if (FParse::Value(FCommandLine::Get(), TEXT("CityCull="), List, false) && !ParseCullList(List, CullM))
	{
		GTP_TRACE(CityRenderTuning, TEXT("-CityCull=%s not understood (want family=metres,...)"), *List);
		CullM.Reset();
	}
	float Below = -1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("CityPropShadowBelow="), Below))
	{
		ShadowBelowM = Below;
	}
	if (CullM.Num() == 0 && ShadowBelowM < 0.f)
	{
		return;
	}
	Apply(InWorld.PersistentLevel);
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UGTPCityRenderTuning::OnLevelAdded);
}

void UGTPCityRenderTuning::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	Super::Deinitialize();
}

void UGTPCityRenderTuning::OnLevelAdded(ULevel* Level, UWorld* World)
{
	if (World == GetWorld())
	{
		Apply(Level);
	}
}

void UGTPCityRenderTuning::Apply(ULevel* Level)
{
	if (!Level)
	{
		return;
	}
	int32 Changed = 0;
	for (AActor* Actor : Level->Actors)
	{
		if (!Actor)
		{
			continue;
		}
		TInlineComponentArray<UInstancedStaticMeshComponent*> Comps(Actor);
		for (UInstancedStaticMeshComponent* Comp : Comps)
		{
			bool bTouched = false;
			for (const FName& Tag : Comp->ComponentTags)
			{
				const FString S = Tag.ToString();
				if (S.StartsWith(TEXT("GTP_Cull_")))
				{
					if (const float* M = CullM.Find(S.RightChop(9)))
					{
						const int32 End = FMath::RoundToInt(*M * 100.f);
						Comp->SetCullDistances(FMath::RoundToInt(End * 0.8f), End);
						bTouched = true;
					}
				}
				else if (ShadowBelowM >= 0.f && S.StartsWith(TEXT("GTP_SizeCm=")))
				{
					if (FCString::Atof(*S.RightChop(11)) < ShadowBelowM * 100.f && Comp->CastShadow)
					{
						Comp->SetCastShadow(false);
						bTouched = true;
					}
				}
			}
			Changed += bTouched ? 1 : 0;
		}
	}
	GTP_TRACE(CityRenderTuning, TEXT("%s %d components"), *Level->GetOuter()->GetName(), Changed);
}
