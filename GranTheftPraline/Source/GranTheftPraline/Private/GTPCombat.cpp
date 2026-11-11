#include "GTPCombat.h"

#include "GTPCombatInput.h"
#include "GTPCombatMelee.h"
#include "GTPHitReaction.h"
#include "GTPLog.h"
#include "GTPPedestrians.h"
#include "GTPSurfaceLogic.h"
#include "GTPThreat.h"
#include "GTPWeapon.h"
#include "Components/DecalComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraCommon.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "WheeledVehiclePawn.h"

DEFINE_LOG_CATEGORY(LogGTPCombat);

namespace
{
	const FName CombatHospitalTag(TEXT("GTPHospital"));
	constexpr float CombatHealthStepSeconds = 0.1f;

	uint8 CombatSurfaceOf(const FHitResult& Hit, bool& bGlass)
	{
		FString Name;
		if (const UPhysicalMaterial* Phys = Hit.PhysMaterial.Get())
		{
			Name = Phys->GetName();
		}
		if (const UPrimitiveComponent* Component = Hit.GetComponent())
		{
			if (const UMaterialInterface* Material = Component->GetMaterial(FMath::Max(0, int32(Hit.ElementIndex))))
			{
				Name += TEXT(" ") + Material->GetName();
			}
		}
		const FString Lower = Name.ToLower();
		bGlass = Lower.Contains(TEXT("glass")) || Lower.Contains(TEXT("vitre")) || Lower.Contains(TEXT("window"));
		const uint8 Id = GTPSurfaceFromName(TCHAR_TO_UTF8(*Lower));
		return Id == GTPSurfaceId::Unknown ? GTPSurfaceId::Asphalt : Id;
	}
}

bool UGTPCombatSubsystem::IsEnabled()
{
	int32 Value = 1;
	FParse::Value(FCommandLine::Get(), TEXT("-Combat="), Value);
	return Value != 0;
}

UGTPCombatSubsystem* UGTPCombatSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UGTPCombatSubsystem>() : nullptr;
}

bool UGTPCombatSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsEnabled() && Super::ShouldCreateSubsystem(Outer);
}

void UGTPCombatSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	GTP_TRACE(LogGTPCombat, TEXT("combat: on (-Combat=0 turns it off)"));
	InWorld.GetTimerManager().SetTimer(HealthTimer, FTimerDelegate::CreateUObject(this, &UGTPCombatSubsystem::StepHealth), CombatHealthStepSeconds, true);
	InWorld.GetTimerManager().SetTimer(EquipTimer, FTimerDelegate::CreateUObject(this, &UGTPCombatSubsystem::EquipPlayer), 0.5f, true, 0.2f);
	int32 Bench = 0;
	FParse::Value(FCommandLine::Get(), TEXT("-CombatBench="), Bench);
	if (Bench != 0)
	{
		GTP_TRACE(LogGTPCombat, TEXT("combat: firefight bench on (a shot every 0.45 s)"));
		InWorld.GetTimerManager().SetTimer(BenchTimer, FTimerDelegate::CreateUObject(this, &UGTPCombatSubsystem::BenchShot), 0.45f, true, 3.0f);
	}
	FTimerHandle WarmUp;
	InWorld.GetTimerManager().SetTimer(WarmUp, FTimerDelegate::CreateUObject(this, &UGTPCombatSubsystem::WarmUpEffects), 1.0f, false);
}

void UGTPCombatSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HealthTimer);
		World->GetTimerManager().ClearTimer(EquipTimer);
		World->GetTimerManager().ClearTimer(RespawnTimer);
		World->GetTimerManager().ClearTimer(BenchTimer);
	}
	Bodies.Reset();
	Super::Deinitialize();
}

APawn* UGTPCombatSubsystem::GetPlayerPawn() const
{
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPawn() : nullptr;
}

bool UGTPCombatSubsystem::IsPlayer(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsPlayerControlled();
}

void UGTPCombatSubsystem::RegisterHealth(UGTPHealthComponent* Health)
{
	if (Health)
	{
		Bodies.AddUnique(Health);
	}
}

void UGTPCombatSubsystem::UnregisterHealth(UGTPHealthComponent* Health)
{
	Bodies.Remove(Health);
}

void UGTPCombatSubsystem::StepHealth()
{
	for (int32 I = Bodies.Num() - 1; I >= 0; --I)
	{
		UGTPHealthComponent* Health = Bodies[I];
		if (!IsValid(Health))
		{
			Bodies.RemoveAtSwap(I);
			continue;
		}
		Health->Step(CombatHealthStepSeconds);
	}
}

void UGTPCombatSubsystem::EquipPlayer()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	if (!Input)
	{
		Input = NewObject<UGTPCombatInput>(this, TEXT("GTPCombatInput"), RF_Transient);
	}
	Input->Bind(PC);

	ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
	Input->UpdateContext(Character != nullptr && !DeadPawn.IsValid());
	if (!Character || EquippedPawn.Get() == Character)
	{
		return;
	}
	EquippedPawn = Character;
	UGTPHealthComponent* Health = UGTPHealthComponent::Find(Character, true);
	Health->OnDied.AddUObject(this, &UGTPCombatSubsystem::OnHealthDied);
	if (UGTPHitReactionComponent::IsEnabled() && !Character->FindComponentByClass<UGTPHitReactionComponent>())
	{
		UGTPHitReactionComponent* Reaction = NewObject<UGTPHitReactionComponent>(Character, TEXT("GTPHitReaction"));
		Reaction->RegisterComponent();
	}
	UGTPWeaponComponent* Weapon = Character->FindComponentByClass<UGTPWeaponComponent>();
	if (!Weapon)
	{
		Weapon = NewObject<UGTPWeaponComponent>(Character, TEXT("GTPWeapon"));
		Weapon->bPlayerControlled = true;
		Weapon->RegisterComponent();
	}
	if (!Character->FindComponentByClass<UGTPCombatMeleeComponent>())
	{
		UGTPCombatMeleeComponent* Fists = NewObject<UGTPCombatMeleeComponent>(Character, TEXT("GTPFists"));
		Fists->RegisterComponent();
	}
	GTP_TRACE(LogGTPCombat, TEXT("combat: player %s equipped (health %.0f, pistol %d/%d)"), *Character->GetName(), Health->GetHealth(),
		Weapon->GetMagazine(), Weapon->GetReserve());
}

FGTPDamageOutcome UGTPCombatSubsystem::ApplyHit(const FGTPCombatHit& Hit)
{
	FGTPDamageOutcome Outcome;
	Outcome.bIgnored = true;
	AActor* Victim = Hit.Victim.Get();
	AActor* Instigator = Hit.Instigator.Get();
	if (!Victim || Victim->IsActorBeingDestroyed())
	{
		return Outcome;
	}
	UGTPHealthComponent* Health = UGTPHealthComponent::Find(Victim, true);
	if (!Health || Health->IsDead())
	{
		return Outcome;
	}
	const bool bVictimIsPlayer = IsPlayer(Victim);
	const bool bFromPlayer = IsPlayer(Instigator);
	const GTPDamage::EZone Zone = GTPDamage::ZoneFromBone(TCHAR_TO_UTF8(*Hit.Bone.ToString()));
	GTPDamage::FDifficulty Difficulty;
	Difficulty.PlayerTakes = PlayerDamageTaken;
	const float Amount = Hit.FixedDamage >= 0.f ? Hit.FixedDamage
		: GTPDamage::ComputeDamage(Hit.Damage, Zone, Hit.DistanceM, bVictimIsPlayer && !bFromPlayer, Difficulty);
	Outcome = Health->ApplyDamage(Amount, Hit.bMelee ? EGTPDamageCause::Melee : EGTPDamageCause::Bullet, Instigator,
		Hit.bMelee || Hit.Damage.bIgnoresArmor);
	if (Outcome.bIgnored)
	{
		return Outcome;
	}

	// Reaction (ragdoll thread): flinch, stagger, knockdown or death.
	if (UGTPHitReactionComponent::IsEnabled() && Victim->FindComponentByClass<USkeletalMeshComponent>())
	{
		UGTPHitReactionComponent* Reaction = Victim->FindComponentByClass<UGTPHitReactionComponent>();
		if (!Reaction)
		{
			Reaction = NewObject<UGTPHitReactionComponent>(Victim, MakeUniqueObjectName(Victim, UGTPHitReactionComponent::StaticClass(), TEXT("GTPHitReaction")));
			Reaction->RegisterComponent();
		}
		FGTPHitInfo Info;
		// Armour softens the blow the body feels, it does not cancel it.
		Info.Damage = Outcome.HealthLost + 0.5f * Outcome.ArmorLost;
		Info.HealthAfter = Outcome.HealthAfter;
		Info.Direction = Hit.Direction;
		Info.Bone = Hit.Bone;
		Info.bMelee = Hit.bMelee;
		Info.bHeavy = Hit.bHeavy;
		Reaction->ReactToHit(Info);
	}

	if (!Hit.bMelee)
	{
		FHitResult Fake;
		Fake.ImpactPoint = Hit.Location;
		Fake.Location = Hit.Location;
		Fake.ImpactNormal = -Hit.Direction;
		Fake.HitObjectHandle = FActorInstanceHandle(Victim);
		SpawnImpact(Fake, Hit.Direction);
		AddBloodStain(Victim);
	}
	if (Outcome.bKilled)
	{
		SpawnBloodPool(Victim);
	}

	if (bFromPlayer)
	{
		LastPlayerHitTime = GetWorld()->GetTimeSeconds();
		bLastPlayerHitKilled = Outcome.bKilled;
	}
	OnDamageDealt.Broadcast(Instigator, Victim, Outcome.HealthLost + Outcome.ArmorLost, Zone, Hit.bMelee, Outcome.HealthAfter);
	if (Outcome.bKilled)
	{
		GTP_TRACE(LogGTPCombat, TEXT("combat: %s killed %s"), Instigator ? *Instigator->GetName() : TEXT("?"), *Victim->GetName());
		OnPawnKilled.Broadcast(Instigator, Victim, Hit.bMelee);
	}
	return Outcome;
}

void UGTPCombatSubsystem::NotifyShot(AActor* Shooter, const FVector& Origin, const FVector& Direction, FName Weapon, const FHitResult& Hit, bool bHit)
{
	if (UGTPThreatSubsystem* Threat = GetWorld() ? GetWorld()->GetSubsystem<UGTPThreatSubsystem>() : nullptr)
	{
		Threat->NotifyShot(Origin, bHit ? Hit.GetActor() : nullptr);
	}
	OnShotFired.Broadcast(Shooter, Origin, Direction, Weapon, Hit, bHit);
}

void UGTPCombatSubsystem::NotifyAim(AActor* Source, const FVector& Origin, const FVector& Aim, bool bAiming)
{
	UGTPThreatSubsystem* Threat = GetWorld() ? GetWorld()->GetSubsystem<UGTPThreatSubsystem>() : nullptr;
	if (bAiming)
	{
		if (Threat)
		{
			Threat->SetThreat(Source, true, Origin, Aim);
		}
		if (!bThreatActive)
		{
			bThreatActive = true;
			OnArmedThreat.Broadcast(Source, Origin, Aim);
		}
	}
	else if (bThreatActive)
	{
		bThreatActive = false;
		if (Threat && Threat->GetSource() == Source)
		{
			Threat->ClearThreat();
		}
	}
}

void UGTPCombatSubsystem::SpawnImpact(const FHitResult& Hit, const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	GTPDamage::FImpactContext Context;
	const AActor* HitActor = Hit.GetActor();
	Context.bHitPawn = Cast<ACharacter>(HitActor) != nullptr || Cast<AGTPPedestrian>(HitActor) != nullptr;
	Context.bHitVehicle = Cast<AWheeledVehiclePawn>(HitActor) != nullptr;
	bool bGlass = false;
	Context.SurfaceId = Context.bHitPawn ? GTPSurfaceId::Asphalt : CombatSurfaceOf(Hit, bGlass);
	Context.bGlass = bGlass;
	const int32 Kind = static_cast<int32>(GTPDamage::ImpactFor(Context));
	UNiagaraSystem* System = LoadedImpacts.IsValidIndex(Kind) ? LoadedImpacts[Kind].Get() : nullptr;
	if (!System)
	{
		return; // no effect assigned yet: the hit still counts
	}

	// Hard cap: forget the effects that ended; over the cap, the oldest ends now.
	const double Now = World->GetTimeSeconds();
	LiveImpactEnds.RemoveAll([Now](double End) { return End <= Now; });
	if (LiveImpactEnds.Num() >= FMath::Max(1, MaxLiveImpacts))
	{
		return; // the budget is spent for this instant; nothing waits
	}
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Hit.ImpactPoint, Hit.ImpactNormal.Rotation(), FVector::OneVector, true, true,
		ENCPoolMethod::AutoRelease, true);
	LiveImpactEnds.Add(Now + ImpactLifeSeconds);
}

void UGTPCombatSubsystem::WarmUpEffects()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	LoadedImpacts.SetNum(static_cast<int32>(GTPDamage::EImpact::Count));
	int32 Loaded = 0;
	for (int32 I = 0; I < ImpactSystems.Num() && I < LoadedImpacts.Num(); ++I)
	{
		UNiagaraSystem* System = ImpactSystems[I].IsNull() ? nullptr : Cast<UNiagaraSystem>(ImpactSystems[I].TryLoad());
		LoadedImpacts[I] = System;
		if (System)
		{
			// One hidden spawn far below the street compiles the shaders and pipeline states before the first real shot.
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, FVector::OneVector,
				true, true, ENCPoolMethod::AutoRelease, false);
			++Loaded;
		}
	}
	// Loaded here, never during a fight (a missing package would stall the game thread).
	LoadedBloodPool = BloodPoolMaterial.IsNull() ? nullptr : Cast<UMaterialInterface>(BloodPoolMaterial.TryLoad());
	GTP_TRACE(LogGTPCombat, TEXT("combat: %d impact effects loaded and warmed up, blood pool %s"), Loaded, LoadedBloodPool ? TEXT("on") : TEXT("off"));
}

void UGTPCombatSubsystem::OnHealthDied(UGTPHealthComponent* Health, EGTPDamageCause Cause, AActor* Killer)
{
	APawn* Pawn = Health ? Cast<APawn>(Health->GetOwner()) : nullptr;
	if (!Pawn || !IsPlayer(Pawn) || DeadPawn.IsValid())
	{
		return;
	}
	DeadPawn = Pawn;
	DeathLocation = Pawn->GetActorLocation();
	if (UGTPWeaponComponent* Weapon = Pawn->FindComponentByClass<UGTPWeaponComponent>())
	{
		Weapon->SetDrawn(false);
	}
	if (Input)
	{
		Input->UpdateContext(false);
	}
	GTP_TRACE(LogGTPCombat, TEXT("combat: player died (cause %d)"), static_cast<int32>(Cause));
	OnPlayerDied.Broadcast(DeathLocation);
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer, FTimerDelegate::CreateUObject(this, &UGTPCombatSubsystem::RespawnPlayer),
		FMath::Max(0.5f, Health->GetSettings().RespawnDelaySeconds), false);
}

void UGTPCombatSubsystem::RespawnPlayer()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!PC || !GameMode)
	{
		return;
	}
	// Nearest fictional hospital, else the player start.
	std::vector<GTPHealth::FHospital> Hospitals;
	TArray<ATargetPoint*> Points;
	for (TActorIterator<ATargetPoint> It(World); It; ++It)
	{
		if (It->ActorHasTag(CombatHospitalTag))
		{
			const FVector L = It->GetActorLocation();
			Hospitals.push_back({static_cast<float>(L.X), static_cast<float>(L.Y), static_cast<float>(L.Z), static_cast<float>(It->GetActorRotation().Yaw)});
			Points.Add(*It);
		}
	}
	const int32 Index = GTPHealth::PickHospital(Hospitals, static_cast<float>(DeathLocation.X), static_cast<float>(DeathLocation.Y));
	FTransform Where;
	FName HospitalName = NAME_None;
	if (Points.IsValidIndex(Index))
	{
		Where = Points[Index]->GetActorTransform();
		HospitalName = Points[Index]->GetFName();
	}
	else if (const AActor* Start = GameMode->FindPlayerStart(PC))
	{
		Where = Start->GetActorTransform();
	}

	APawn* Old = DeadPawn.Get();
	DeadPawn.Reset();
	EquippedPawn.Reset();
	PC->UnPossess();
	GameMode->RestartPlayerAtTransform(PC, Where);
	if (Old)
	{
		Old->Destroy();
	}
	EquipPlayer();
	if (APawn* Fresh = PC->GetPawn())
	{
		if (UGTPHealthComponent* Health = UGTPHealthComponent::Find(Fresh, true))
		{
			Health->Revive(RespawnGraceSeconds);
		}
	}
	GTP_TRACE(LogGTPCombat, TEXT("combat: player respawned at %s"), *HospitalName.ToString());
	OnPlayerRespawned.Broadcast(Where.GetLocation(), HospitalName);
}

float UGTPCombatSubsystem::GetSecondsSincePlayerHit(bool& bKilled) const
{
	bKilled = bLastPlayerHitKilled;
	return GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - LastPlayerHitTime) : 1000.f;
}

void UGTPCombatSubsystem::AddBloodStain(AActor* Body)
{
	USkeletalMeshComponent* Mesh = Body ? Body->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	if (!Mesh || BloodStainPerHit <= 0.f)
	{
		return;
	}
	if (BloodStains.Num() > 64)
	{
		for (auto It = BloodStains.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}
	float& Amount = BloodStains.FindOrAdd(Body);
	Amount = FMath::Min(1.f, Amount + BloodStainPerHit);
	// Materials without the parameter ignore it (the realism thread adds it to the clothes).
	Mesh->SetScalarParameterValueOnMaterials(TEXT("GTPBlood"), Amount);
}

void UGTPCombatSubsystem::SpawnBloodPool(AActor* Body)
{
	UWorld* World = GetWorld();
	if (!World || !Body || BloodPoolMaterial.IsNull() || MaxBloodPools <= 0)
	{
		return;
	}
	if (!LoadedBloodPool)
	{
		return; // loaded once in WarmUpEffects; missing asset = no pools
	}
	// On the ground under the body.
	const FVector From = Body->GetActorLocation();
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPBloodPool), false, Body);
	if (!World->LineTraceSingleByChannel(Ground, From, From - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		return;
	}
	BloodPools.RemoveAll([](const TWeakObjectPtr<UDecalComponent>& D) { return !D.IsValid(); });
	while (BloodPools.Num() >= MaxBloodPools)
	{
		if (UDecalComponent* Oldest = BloodPools[0].Get())
		{
			Oldest->DestroyComponent();
		}
		BloodPools.RemoveAt(0);
	}
	const float Size = 70.f + 30.f * static_cast<float>((GetTypeHash(Body->GetName()) % 100u)) / 100.f;
	// A decal projects along its X axis: into the ground.
	FRotator Rotation = (-Ground.ImpactNormal).Rotation();
	Rotation.Roll = static_cast<double>(GetTypeHash(Body->GetName()) % 360u);
	if (UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(World, LoadedBloodPool, FVector(20.f, Size, Size), Ground.ImpactPoint,
			Rotation, BloodPoolLifeSeconds))
	{
		Decal->SetFadeOut(BloodPoolLifeSeconds - 10.f, 10.f, false);
		BloodPools.Add(Decal);
	}
}

void UGTPCombatSubsystem::BenchShot()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	UGTPWeaponComponent* Weapon = Pawn ? Pawn->FindComponentByClass<UGTPWeaponComponent>() : nullptr;
	if (!Weapon)
	{
		return;
	}
	if (!Weapon->IsDrawn())
	{
		Weapon->SetDrawn(true);
	}
	if (Weapon->GetReserve() < 24)
	{
		Weapon->AddAmmo(96);
	}
	++BenchShots;
	// A fixed sweep: left-right across the street, down at the ground and up at the facades.
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	const float T = static_cast<float>(BenchShots);
	const FRotator Aim(View.Pitch - 6.0 + 9.0 * FMath::Sin(T * 0.61f), View.Yaw + 40.0 * FMath::Sin(T * 0.37f), 0.0);
	Weapon->FireAt(Eye + Aim.Vector() * 3000.f, 1.f);

	if (BenchShots % 10 != 0)
	{
		return;
	}
	// Every tenth shot: the nearest living passer-by within 40 m falls (reaction, ragdoll, blood).
	AActor* Nearest = nullptr;
	double NearestD2 = FMath::Square(4000.0);
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Other = *It;
		if (Other == Pawn || !(Cast<AGTPPedestrian>(Other) || (Cast<ACharacter>(Other) && !Cast<APawn>(Other)->IsPlayerControlled())))
		{
			continue;
		}
		const UGTPHealthComponent* Health = Other->FindComponentByClass<UGTPHealthComponent>();
		if (Health && Health->IsDead())
		{
			continue;
		}
		const double D2 = FVector::DistSquared(Other->GetActorLocation(), Pawn->GetActorLocation());
		if (D2 < NearestD2)
		{
			Nearest = Other;
			NearestD2 = D2;
		}
	}
	if (Nearest)
	{
		FGTPCombatHit Hit;
		Hit.Instigator = Pawn;
		Hit.Victim = Nearest;
		Hit.Location = Nearest->GetActorLocation();
		Hit.Direction = (Nearest->GetActorLocation() - Pawn->GetActorLocation()).GetSafeNormal();
		Hit.FixedDamage = 1000.f;
		ApplyHit(Hit);
	}
}
