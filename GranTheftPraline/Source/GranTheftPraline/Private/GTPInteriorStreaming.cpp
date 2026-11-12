#include "GTPInteriorStreaming.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/BoxComponent.h"
#include "Components/PostProcessComponent.h"
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
const TCHAR* SizePrefix = TEXT("GTP_InteriorSize=");   // metres, "19x16x7.2"
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
			else if (S.StartsWith(SizePrefix))
			{
				TArray<FString> Parts;
				S.RightChop(FCString::Strlen(SizePrefix)).ParseIntoArray(Parts, TEXT("x"));
				if (Parts.Num() == 3)
				{
					Interior.Size = FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2])) * 100.f;
				}
			}
		}
		if (Interior.Level.IsEmpty())
		{
			GTP_TRACE(InteriorStreaming, TEXT("anchor %s has no level tag, ignored"), *Actor->GetName());
			continue;
		}
		Interior.Anchor = Actor->GetActorTransform();
		Interior.AnchorActor = Actor;
		Interior.Id = UTF8_TO_TCHAR(GTPInterior::IdFromLevel(TCHAR_TO_UTF8(*Interior.Level)).c_str());
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
	int32 Start = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("InteriorStart="), Start) && Start != 0)
	{
		Rules.LoadRadius = Rules.UnloadRadius = 1.0e7f;   // loaded from the start, wherever the player is
	}
	bStartDone = false;
	LeaveIn = -1.f;
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
			AddExposure(Interior);
			if (UGTPKitchenSubsystem* Kitchen = GetWorld()->GetSubsystem<UGTPKitchenSubsystem>())
			{
				Kitchen->StartService(Streaming->GetLoadedLevel(), Interior.Id, Interior.Anchor);   // inert without -InteriorStaff=
			}
			GTP_TRACE(InteriorStreaming, TEXT("visible %s t=%.2f"), *Interior.Level, GetWorld()->GetTimeSeconds());
		}
		if (bVisible && !bStartDone)
		{
			PlaceAtCounter(Interior);   // retried until the pawn exists
		}
		Interior.bWasVisible = bVisible;
	}
	if (LeaveIn >= 0.f)
	{
		LeaveIn -= DeltaTime;
		APawn* Pawn = GetWorld()->GetFirstPlayerController() ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
		if (LeaveIn < 0.f && Pawn)
		{
			// Back on the pavement, 3 m in front of the shop front: the hold-up ends as an escape or an abort.
			Pawn->TeleportTo(StartAnchor.TransformPosition(FVector(950.f, 300.f, 100.f)), StartAnchor.Rotator());
			GTP_TRACE(InteriorStreaming, TEXT("test start: player taken outside t=%.2f"), GetWorld()->GetTimeSeconds());
		}
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
		if (!bLoggedFirstPlan)
		{
			GTP_TRACE(InteriorStreaming, TEXT("waiting for a player controller t=%.2f"), GetWorld()->GetTimeSeconds());
			bLoggedFirstPlan = true;   // logged once; the first real plan logs again below
			bWaitingLogged = true;
		}
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
	FString Inside;
	for (const FInterior& Interior : Interiors)
	{
		const FVector Local = Interior.Anchor.InverseTransformPosition(Where);
		if (GTPInterior::Inside(Local.X, Local.Y, Local.Z, Interior.Size.X, Interior.Size.Y, Interior.Size.Z))
		{
			Inside = Interior.Id;
			break;
		}
	}
	if (Inside != PlayerInteriorId)
	{
		const FString Old = PlayerInteriorId;
		PlayerInteriorId = Inside;
		GTP_TRACE(InteriorStreaming, TEXT("player %s %s t=%.2f"), Inside.IsEmpty() ? TEXT("left") : TEXT("entered"),
		          Inside.IsEmpty() ? *Old : *Inside, GetWorld()->GetTimeSeconds());
		OnPlayerInteriorChanged.Broadcast(Old, Inside);
	}
	const std::vector<bool> Wanted = GTPInterior::Plan(Sites, static_cast<float>(Where.X), static_cast<float>(Where.Y), static_cast<float>(Where.Z), Rules);
	if (!bLoggedFirstPlan || bWaitingLogged)
	{
		bLoggedFirstPlan = true;
		bWaitingLogged = false;
		float Nearest = 1e30f;
		for (const GTPInterior::FSite& Site : Sites)
		{
			Nearest = FMath::Min(Nearest, GTPInterior::Distance(Site, static_cast<float>(Where.X), static_cast<float>(Where.Y), static_cast<float>(Where.Z)));
		}
		int32 Count = 0;
		for (const bool bWanted : Wanted)
		{
			Count += bWanted ? 1 : 0;
		}
		GTP_TRACE(InteriorStreaming, TEXT("first plan: player at %s (%s), nearest interior %.0f m, %d wanted, t=%.2f"), *Where.ToCompactString(),
		          Pawn ? TEXT("pawn") : TEXT("camera"), Nearest / 100.f, Count, GetWorld()->GetTimeSeconds());
	}
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
	RemoveExposure(Interior);
	if (ULevelStreamingDynamic* Streaming = Interior.Streaming.Get())
	{
		Streaming->SetShouldBeVisible(false);
		Streaming->SetShouldBeLoaded(false);
	}
	GTP_TRACE(InteriorStreaming, TEXT("unload %s t=%.2f"), *Interior.Level, GetWorld()->GetTimeSeconds());
}

void UGTPInteriorStreaming::PlaceAtCounter(const FInterior& Interior)
{
	// Tests without input: -InteriorStart=1 puts the player in the queue facing the first till, the
	// way a robber walks in (with -ThreatDemo the player's view is the aim);
	// -InteriorStartLeaveAfter=S takes them back outside S seconds later.
	int32 Start = 0;
	FParse::Value(FCommandLine::Get(), TEXT("InteriorStart="), Start);
	if (Start == 0)
	{
		bStartDone = true;   // not a test start: never looked at again
		return;
	}
	const ULevelStreamingDynamic* Streaming = Interior.Streaming.Get();
	if (bStartDone || !Streaming || !Streaming->GetLoadedLevel())
	{
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return;
	}
	// The first till slot by name (the builder numbers stations across the counter: till_3, till_4...).
	const AActor* First = nullptr;
	FString FirstName;
	for (const AActor* Actor : Streaming->GetLoadedLevel()->Actors)
	{
		if (!Actor)
		{
			continue;
		}
		for (const FName& Tag : Actor->Tags)
		{
			const FString S = Tag.ToString();
			if (S.StartsWith(TEXT("GTP_Station=till_")) && (!First || S < FirstName))
			{
				First = Actor;
				FirstName = S;
			}
		}
	}
	if (First)
	{
		const FVector Till = First->GetActorLocation();
		const FVector Front = Interior.Anchor.GetRotation().RotateVector(FVector(0.f, 250.f, 0.f));   // towards the shop front
		const FVector Where = Till + Front + FVector(0.f, 0.f, 100.f);
		const FRotator Look = (Till + FVector(0.f, 0.f, 150.f) - (Where + FVector(0.f, 0.f, 60.f))).Rotation();
		Pawn->TeleportTo(Where, FRotator(0.f, Look.Yaw, 0.f));
		PC->SetControlRotation(Look);
		bStartDone = true;
		StartAnchor = Interior.Anchor;
		float After = -1.f;
		FParse::Value(FCommandLine::Get(), TEXT("InteriorStartLeaveAfter="), After);
		LeaveIn = After;
		GTP_TRACE(InteriorStreaming, TEXT("test start: player at the counter of %s (%s) t=%.2f"), *Interior.Id, *FirstName,
		          GetWorld()->GetTimeSeconds());
		return;
	}
	bStartDone = true;
	GTP_TRACE(InteriorStreaming, TEXT("test start: no till slot in %s"), *Interior.Level);
}

void UGTPInteriorStreaming::AddExposure(FInterior& Interior)
{
	int32 Flag = 1;
	FParse::Value(FCommandLine::Get(), TEXT("InteriorExposure="), Flag);
	AActor* Holder = Interior.AnchorActor.Get();
	if (Flag == 0 || !Holder || Interior.Exposure.IsValid())
	{
		return;
	}
	// The box covers the interior frame (x along the front, -y to the back, z up from the anchor).
	UBoxComponent* Box = NewObject<UBoxComponent>(Holder, NAME_None, RF_Transient);
	Box->SetMobility(EComponentMobility::Movable);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetGenerateOverlapEvents(false);
	Box->SetHiddenInGame(true);
	Box->SetBoxExtent(Interior.Size * 0.5f, false);
	Box->SetWorldTransform(FTransform(Interior.Anchor.GetRotation(),
	                                  Interior.Anchor.TransformPosition(FVector(Interior.Size.X * 0.5f, -Interior.Size.Y * 0.5f, Interior.Size.Z * 0.5f))));
	Box->RegisterComponent();
	UPostProcessComponent* PostProcess = NewObject<UPostProcessComponent>(Holder, NAME_None, RF_Transient);
	PostProcess->SetMobility(EComponentMobility::Movable);
	PostProcess->bUnbound = false;          // bounded by its parent box
	PostProcess->Priority = 1001.f;         // above the environment's unbound volume (1000)
	PostProcess->BlendRadius = 60.f;        // the doorway blends street and shop
	PostProcess->BlendWeight = 1.f;
	FPostProcessSettings& S = PostProcess->Settings;
	S.bOverride_AutoExposureMinBrightness = true;
	S.bOverride_AutoExposureMaxBrightness = true;
	S.AutoExposureMinBrightness = InteriorExposureMinEV;
	S.AutoExposureMaxBrightness = FMath::Max(InteriorExposureMinEV, InteriorExposureMaxEV);
	PostProcess->SetupAttachment(Box);
	PostProcess->RegisterComponent();
	Interior.ExposureBox = Box;
	Interior.Exposure = PostProcess;
	GTP_TRACE(InteriorStreaming, TEXT("exposure EV %.1f-%.1f inside %s"), InteriorExposureMinEV, InteriorExposureMaxEV, *Interior.Id);
}

void UGTPInteriorStreaming::RemoveExposure(FInterior& Interior)
{
	if (UPostProcessComponent* PostProcess = Interior.Exposure.Get())
	{
		PostProcess->DestroyComponent();
	}
	if (UBoxComponent* Box = Interior.ExposureBox.Get())
	{
		Box->DestroyComponent();
	}
	Interior.Exposure.Reset();
	Interior.ExposureBox.Reset();
}

FTransform UGTPInteriorStreaming::GetInteriorAnchor(const FString& Id) const
{
	for (const FInterior& Interior : Interiors)
	{
		if (Interior.Id == Id)
		{
			return Interior.Anchor;
		}
	}
	return FTransform::Identity;
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
