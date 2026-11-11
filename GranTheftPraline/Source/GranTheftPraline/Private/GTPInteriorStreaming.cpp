#include "GTPInteriorStreaming.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GTPGraphicsProfile.h"
#include "GTPKitchen.h"
#include "GTPLog.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(InteriorStreaming, Log, All);   // lines read "InteriorStreaming: ..." in every build

namespace
{
const FName AnchorTag(TEXT("GTP_InteriorAnchor"));
const TCHAR* LevelPrefix = TEXT("GTP_InteriorLevel=");
const TCHAR* MinDensityPrefix = TEXT("GTP_MinDensity=");
const FName NoShadowM1Tag(TEXT("GTP_NoShadowM1"));
const FName NoCollisionTag(TEXT("GTP_NoCollision"));   // micro objects: never collide, even when shown
} // namespace

bool UGTPInteriorStreaming::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

TStatId UGTPInteriorStreaming::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGTPInteriorStreaming, STATGROUP_Tickables);
}

bool UGTPInteriorStreaming::IsM1Profile() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UGTPGraphicsProfile* Profile = GameInstance ? GameInstance->GetSubsystem<UGTPGraphicsProfile>() : nullptr;
	return !Profile || Profile->GetActiveProfile().Equals(TEXT("m1"), ESearchCase::IgnoreCase);
}

void UGTPInteriorStreaming::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	Interiors.Reset();
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor->Tags.Contains(AnchorTag))
		{
			continue;
		}
		FInterior Interior;
		for (const FName& Tag : Actor->Tags)
		{
			const FString S = Tag.ToString();
			if (S.StartsWith(LevelPrefix))
			{
				Interior.Level = S.RightChop(FCString::Strlen(LevelPrefix));
			}
		}
		if (Interior.Level.IsEmpty())
		{
			GTP_TRACE(InteriorStreaming, TEXT("anchor %s has no level tag, ignored"), *Actor->GetName());
			continue;
		}
		Interior.Anchor = Actor->GetActorTransform();
		Interiors.Add(MoveTemp(Interior));
	}

	int32 Flag = 0;
	FParse::Value(FCommandLine::Get(), TEXT("Interiors="), Flag);
	float M = 0.f;
	Rules.LoadRadius = (FParse::Value(FCommandLine::Get(), TEXT("InteriorPreload="), M) ? M : PreloadRadiusM) * 100.f;
	Rules.UnloadRadius = FMath::Max(Rules.LoadRadius, (FParse::Value(FCommandLine::Get(), TEXT("InteriorUnload="), M) ? M : UnloadRadiusM) * 100.f);
	Rules.MaxLoaded = FMath::Max(0, MaxLoaded);
	int32 RequestedDensity = GTPInterior::DefaultDensity(IsM1Profile());
	FParse::Value(FCommandLine::Get(), TEXT("InteriorDensity="), RequestedDensity);
	Density = GTPInterior::ClampDensity(RequestedDensity);
	bEnabled = Interiors.Num() > 0 && Flag != 0;
	if (Interiors.Num() > 0)
	{
		GTP_TRACE(InteriorStreaming, TEXT("%d interiors, %s, preload %.0f m, unload %.0f m, density %d"), Interiors.Num(),
		          bEnabled ? TEXT("on") : TEXT("off (-Interiors=1 to turn on)"), Rules.LoadRadius / 100.f,
		          Rules.UnloadRadius / 100.f, Density);
	}
	SinceUpdate = UpdateSeconds;   // first plan on the first tick
}

void UGTPInteriorStreaming::Tick(float DeltaTime)
{
	if (!bEnabled)
	{
		return;
	}
	for (FInterior& Interior : Interiors)
	{
		const ULevelStreamingDynamic* Streaming = Interior.Streaming.Get();
		const bool bVisible = Interior.bRequested && Streaming && Streaming->IsLevelVisible();
		if (bVisible && !Interior.bWasVisible)
		{
			ApplyToLevel(Streaming->GetLoadedLevel());
			if (UGTPKitchenSubsystem* Kitchen = GetWorld()->GetSubsystem<UGTPKitchenSubsystem>())
			{
				Kitchen->StartService(Streaming->GetLoadedLevel());   // inert without -InteriorStaff=
			}
			GTP_TRACE(InteriorStreaming, TEXT("visible %s t=%.2f"), *Interior.Level, GetWorld()->GetTimeSeconds());
		}
		Interior.bWasVisible = bVisible;
	}
	SinceUpdate += DeltaTime;
	if (SinceUpdate >= UpdateSeconds)
	{
		SinceUpdate = 0.f;
		Update();
	}
}

void UGTPInteriorStreaming::Update()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	const APawn* Pawn = PC->GetPawn();
	const FVector Where = Pawn ? Pawn->GetActorLocation() : (PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector);
	std::vector<GTPInterior::FSite> Sites;
	Sites.reserve(Interiors.Num());
	for (const FInterior& Interior : Interiors)
	{
		const FVector Door = Interior.Anchor.GetLocation();
		Sites.push_back({static_cast<float>(Door.X), static_cast<float>(Door.Y), static_cast<float>(Door.Z), Interior.bRequested});
	}
	const std::vector<bool> Wanted = GTPInterior::Plan(Sites, static_cast<float>(Where.X), static_cast<float>(Where.Y), static_cast<float>(Where.Z), Rules);
	for (int32 Index = 0; Index < Interiors.Num(); ++Index)
	{
		FInterior& Interior = Interiors[Index];
		if (Wanted[Index] && !Interior.bRequested)
		{
			RequestLoad(Interior);
		}
		else if (!Wanted[Index] && Interior.bRequested)
		{
			Release(Interior);
		}
	}
}

void UGTPInteriorStreaming::RequestLoad(FInterior& Interior)
{
	Interior.bRequested = true;
	if (ULevelStreamingDynamic* Streaming = Interior.Streaming.Get())
	{
		Streaming->SetShouldBeLoaded(true);
		Streaming->SetShouldBeVisible(true);
	}
	else
	{
		bool bOk = false;
		Streaming = ULevelStreamingDynamic::LoadLevelInstance(GetWorld(), Interior.Level, Interior.Anchor.GetLocation(),
		                                                      Interior.Anchor.Rotator(), bOk);
		if (!bOk || !Streaming)
		{
			GTP_TRACE(InteriorStreaming, TEXT("cannot load %s: shop window card kept"), *Interior.Level);
			return;   // stays requested so it is not retried every update
		}
		Interior.Streaming = Streaming;
	}
	GTP_TRACE(InteriorStreaming, TEXT("load %s t=%.2f"), *Interior.Level, GetWorld()->GetTimeSeconds());
}

void UGTPInteriorStreaming::Release(FInterior& Interior)
{
	Interior.bRequested = false;
	if (Interior.bWasVisible)
	{
		if (UGTPKitchenSubsystem* Kitchen = GetWorld()->GetSubsystem<UGTPKitchenSubsystem>())
		{
			Kitchen->StopService();
		}
	}
	Interior.bWasVisible = false;
	if (ULevelStreamingDynamic* Streaming = Interior.Streaming.Get())
	{
		Streaming->SetShouldBeVisible(false);
		Streaming->SetShouldBeLoaded(false);
	}
	GTP_TRACE(InteriorStreaming, TEXT("unload %s t=%.2f"), *Interior.Level, GetWorld()->GetTimeSeconds());
}

void UGTPInteriorStreaming::SetDensity(int32 NewDensity)
{
	Density = GTPInterior::ClampDensity(NewDensity);
	for (const FInterior& Interior : Interiors)
	{
		const ULevelStreamingDynamic* Streaming = Interior.Streaming.Get();
		if (Streaming && Streaming->IsLevelVisible())
		{
			ApplyToLevel(Streaming->GetLoadedLevel());
		}
	}
}

void UGTPInteriorStreaming::ApplyToLevel(ULevel* Level) const
{
	if (!Level)
	{
		return;
	}
	const bool bM1 = IsM1Profile();
	int32 Hidden = 0;
	for (AActor* Actor : Level->Actors)
	{
		if (!Actor)
		{
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> Components(Actor);
		for (UPrimitiveComponent* Component : Components)
		{
			int32 MinDensity = 0;
			for (const FName& Tag : Component->ComponentTags)
			{
				const FString S = Tag.ToString();
				if (S.StartsWith(MinDensityPrefix))
				{
					MinDensity = FCString::Atoi(*S.RightChop(FCString::Strlen(MinDensityPrefix)));
				}
			}
			const bool bShown = GTPInterior::ShownAt(MinDensity, Density);
			Component->SetVisibility(bShown);
			const bool bCollides = bShown && !Component->ComponentHasTag(NoCollisionTag);
			Component->SetCollisionEnabled(bCollides ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			Hidden += bShown ? 0 : 1;
			if (bM1 && Component->ComponentHasTag(NoShadowM1Tag))
			{
				Component->SetCastShadow(false);
			}
		}
	}
	GTP_TRACE(InteriorStreaming, TEXT("density %d applied, %d components hidden"), Density, Hidden);
}
