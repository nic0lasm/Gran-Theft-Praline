#include "GTPCityRenderTuning.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

FString UGTPCityRenderTuning::ImpliedFamily(const FString& MeshPath)
{
	// Persistent level of the quarter (import_city.py, GTP_CITY_LEVEL=quarter): plain static mesh
	// actors, recognised by their mesh folder so a map built before the tags still answers.
	if (MeshPath.Contains(TEXT("/CellProxies/")))
	{
		return TEXT("proxy");
	}
	if (MeshPath.Contains(TEXT("/Terrain/")))
	{
		return TEXT("terrain");
	}
	return FString();
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
	int32 Proxies = 1;
	if (FParse::Value(FCommandLine::Get(), TEXT("CityProxies="), Proxies))
	{
		bHideProxies = Proxies == 0;
	}
	int32 Report = 0;
	FParse::Value(FCommandLine::Get(), TEXT("CityTuneReport="), Report);
	if (CullM.Num() == 0 && ShadowBelowM < 0.f && !bHideProxies && Report == 0)
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
	// Per family: components found (tagged or implied) and components this pass changed.
	TMap<FString, FIntPoint> Counts;
	for (AActor* Actor : Level->Actors)
	{
		if (!Actor)
		{
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> Comps(Actor);
		for (UStaticMeshComponent* Comp : Comps)
		{
			UInstancedStaticMeshComponent* Ism = Cast<UInstancedStaticMeshComponent>(Comp);
			TArray<FString> Families;
			float SizeCm = -1.f;
			for (const FName& Tag : Comp->ComponentTags)
			{
				const FString S = Tag.ToString();
				if (S.StartsWith(TEXT("GTP_Cull_")))
				{
					Families.AddUnique(S.RightChop(9));
				}
				else if (S.StartsWith(TEXT("GTP_SizeCm=")))
				{
					SizeCm = FCString::Atof(*S.RightChop(11));
				}
			}
			if (Families.Num() == 0 && !Ism && Comp->GetStaticMesh())
			{
				const FString Implied = ImpliedFamily(Comp->GetStaticMesh()->GetPathName());
				if (!Implied.IsEmpty())
				{
					Families.Add(Implied);
				}
			}
			if (Families.Num() == 0)
			{
				continue;
			}
			bool bTouched = false;
			for (const FString& Family : Families)
			{
				if (const float* M = CullM.Find(Family))
				{
					const int32 End = FMath::RoundToInt(*M * 100.f);
					if (Ism)
					{
						Ism->SetCullDistances(FMath::RoundToInt(End * 0.8f), End);
					}
					else
					{
						Comp->SetCullDistance(float(End));
					}
					bTouched = true;
				}
				if (bHideProxies && Family == TEXT("proxy") && Comp->IsVisible())
				{
					// Component visibility, not the actor's: UGTPCityStreaming hides and shows the
					// proxy actor as cells load, and both must be visible to draw.
					Comp->SetVisibility(false);
					bTouched = true;
				}
			}
			if (ShadowBelowM >= 0.f && SizeCm >= 0.f && SizeCm < ShadowBelowM * 100.f && Comp->CastShadow)
			{
				Comp->SetCastShadow(false);
				bTouched = true;
			}
			FIntPoint& C = Counts.FindOrAdd(Families[0]);
			C.X += 1;
			C.Y += bTouched ? 1 : 0;
		}
	}
	Counts.KeySort([](const FString& A, const FString& B) { return A < B; });
	FString Line;
	int32 Changed = 0;
	for (const TPair<FString, FIntPoint>& It : Counts)
	{
		Line += FString::Printf(TEXT(" %s=%d/%d"), *It.Key, It.Value.Y, It.Value.X);
		Changed += It.Value.Y;
	}
	// "<level> <changed> components changed; family=changed/found ..."
	GTP_TRACE(CityRenderTuning, TEXT("%s %d components changed;%s"), *Level->GetOuter()->GetName(), Changed,
		Line.IsEmpty() ? TEXT(" no tagged component") : *Line);
}
