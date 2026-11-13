#include "GTPCombat.h"

#include "GTPCombatInput.h"
#include "GTPCombatMelee.h"
#include "GTPHitReaction.h"
#include "GTPLog.h"
#include "GTPPedestrians.h"
#include "GTPPoliceOfficer.h"
#include "GTPSurfaceLogic.h"
#include "GTPVehicle.h"
#include "GTPThreat.h"
#include "GTPWeapon.h"
#include "GTPWeaponPickup.h"
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
		BenchMode = Bench;
		GTP_TRACE(LogGTPCombat, TEXT("combat: firefight bench on (a shot every 0.45 s, %s)"),
			Bench == 2 ? TEXT("walking a circle, hip fire") : TEXT("standing, shoulder aim"));
		InWorld.GetTimerManager().SetTimer(BenchTimer, FTimerDelegate::CreateUObject(this, &UGTPCombatSubsystem::BenchShot), 0.45f, true, 3.0f);
		if (Bench == 2)
		{
			InWorld.GetTimerManager().SetTimer(BenchWalkTimer, FTimerDelegate::CreateUObject(this, &UGTPCombatSubsystem::BenchWalk), 0.02f, true, 3.0f);
		}
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
		World->GetTimerManager().ClearTimer(BenchWalkTimer);
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
	if (Pawn && Pawn->IsPlayerControlled())
	{
		return true;
	}
	// The player's body seated in the car the player drives (drive-by).
	const AGTPVehicle* Car = Cast<AGTPVehicle>(GetPlayerPawn());
	return Car && Car->GetDriver() == Actor;
}

AActor* UGTPCombatSubsystem::FindAimTarget(const FVector& Origin, const FVector& Direction, float ConeDeg, float MaxDistanceM, const AActor* Ignore) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}
	GTPAimAssist::FSettings Settings;
	Settings.SnapConeDeg = ConeDeg;
	Settings.MaxDistanceM = MaxDistanceM;
	const FVector Dir = Direction.GetSafeNormal();
	const double MaxCm = static_cast<double>(MaxDistanceM) * 100.0;
	std::vector<GTPAimAssist::FTarget> Targets;
	TArray<AActor*> Actors;
	auto Consider = [&](AActor* Body)
	{
		if (!Body || Body == Ignore || IsPlayer(Body) || Body->IsHidden())
		{
			return;
		}
		// Chest height: a body's location is its middle (character capsule or pedestrian root at the feet).
		const FVector Chest = Body->GetActorLocation() + FVector(0.f, 0.f, Cast<ACharacter>(Body) ? 30.f : 130.f);
		const FVector To = Chest - Origin;
		const double Distance = To.Size();
		if (Distance > MaxCm || Distance < 1.0)
		{
			return;
		}
		const float Angle = static_cast<float>(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(To / Distance, Dir), -1.0, 1.0))));
		if (Angle > ConeDeg)
		{
			return;
		}
		GTPAimAssist::FTarget Target;
		Target.AngleDeg = Angle;
		Target.DistanceM = static_cast<float>(Distance * 0.01);
		const UGTPHealthComponent* Health = Body->FindComponentByClass<UGTPHealthComponent>();
		Target.bExcluded = Health && Health->IsDead();
		Target.bHostile = Cast<AGTPPoliceOfficer>(Body) != nullptr;
		FHitResult Block;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPAimTarget), false, Ignore);
		Params.AddIgnoredActor(Body);
		Params.AddIgnoredActor(GetPlayerPawn());
		Target.bVisible = !World->LineTraceSingleByChannel(Block, Origin, Chest, ECC_Visibility, Params);
		Targets.push_back(Target);
		Actors.Add(Body);
	};
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		Consider(*It);
	}
	for (TActorIterator<AGTPPedestrian> It(World); It; ++It)
	{
		Consider(*It);
	}
	const int Best = GTPAimAssist::PickSnapTarget(Targets, Settings);
	return Best >= 0 ? Actors[Best] : nullptr;
}

void UGTPCombatSubsystem::SnapPlayerAim(UGTPWeaponComponent* Weapon)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !Weapon || !bAimAssist)
	{
		return;
	}
	FVector Eye;
	FRotator View;
	PC->GetPlayerViewPoint(Eye, View);
	AActor* Target = FindAimTarget(Eye, View.Vector(), AimSnapConeDeg, AimSnapDistanceM, Weapon->GetOwner());
	if (!Target)
	{
		return;
	}
	const FVector Chest = Target->GetActorLocation() + FVector(0.f, 0.f, Cast<ACharacter>(Target) ? 30.f : 130.f);
	FRotator Snap = (Chest - Eye).Rotation();
	Snap.Roll = 0.f;
	PC->SetControlRotation(Snap);
	GTP_TRACE(LogGTPCombat, TEXT("combat: aim snapped to %s"), *Target->GetName());
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
	// At the wheel: the seated driver keeps the gun for drive-by (one-handed weapons only).
	AGTPVehicle* Car = Cast<AGTPVehicle>(PC->GetPawn());
	ACharacter* Driver = Car ? Car->GetDriver() : nullptr;
	const bool bDriving = Driver != nullptr && Driver == EquippedPawn.Get();
	Input->UpdateContext(DeadPawn.IsValid() ? UGTPCombatInput::EMode::None
		: (Character ? UGTPCombatInput::EMode::OnFoot : (bDriving ? UGTPCombatInput::EMode::Driving : UGTPCombatInput::EMode::None)));
	if (APawn* Equipped = EquippedPawn.Get())
	{
		if (UGTPWeaponComponent* Held = Equipped->FindComponentByClass<UGTPWeaponComponent>())
		{
			Held->SetDriveBy(bDriving ? Car : nullptr);
		}
	}
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
	// Disarmed: an armed body that dies, or takes a heavy blow or a takedown gun in hand, drops its gun.
	if (!bVictimIsPlayer)
	{
		if (UGTPWeaponComponent* VictimGun = Victim->FindComponentByClass<UGTPWeaponComponent>())
		{
			// HasAmmo: a gun already dropped (stripped) does not drop a second time.
			if (GTPPickup::Disarms(Hit.bMelee, Hit.bHeavy, Outcome.bKilled, VictimGun->HasAmmo(), VictimGun->IsDrawn()))
			{
				DropWeapon(Victim, VictimGun);
			}
		}
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
		bThreatActive = true;
		// OnArmedThreat means someone is in the player's sights (the police crime table turns it into
		// an armed threat): aiming at an empty street is not one. Looked for twice a second, once per aim.
		const double Now = GetWorld()->GetTimeSeconds();
		if (!bThreatBroadcast && Now >= NextThreatCheckTime)
		{
			NextThreatCheckTime = Now + 0.5;
			if (!IsPlayer(Source) || FindAimTarget(Origin, Aim, ThreatConeDeg, ThreatDistanceM, Source))
			{
				bThreatBroadcast = true;
				OnArmedThreat.Broadcast(Source, Origin, Aim);
			}
		}
	}
	else if (bThreatActive)
	{
		bThreatActive = false;
		bThreatBroadcast = false;
		NextThreatCheckTime = 0.0;
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
	if (!Context.bHitPawn && !Context.bHitVehicle && Context.SurfaceId != GTPSurfaceId::Water)
	{
		SpawnBulletHole(Hit, bGlass);
	}
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
	LoadedBulletHole = BulletHoleMaterial.IsNull() ? nullptr : Cast<UMaterialInterface>(BulletHoleMaterial.TryLoad());
	LoadedGlassHole = GlassHoleMaterial.IsNull() ? nullptr : Cast<UMaterialInterface>(GlassHoleMaterial.TryLoad());
	for (UMaterialInterface* Decal : {LoadedBulletHole.Get(), LoadedGlassHole.Get(), LoadedBloodPool.Get()})
	{
		if (Decal)
		{
			// Far below the street for a moment: the decal shaders are ready before the first shot.
			UGameplayStatics::SpawnDecalAtLocation(World, Decal, FVector(4.f, 4.f, 4.f), FVector(0.f, 0.f, -100000.f), FRotator::ZeroRotator, 0.2f);
		}
	}
	GTP_TRACE(LogGTPCombat, TEXT("combat: bullet holes %s, glass holes %s"), LoadedBulletHole ? TEXT("on") : TEXT("off"),
		LoadedGlassHole ? TEXT("on") : TEXT("off"));
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
		Input->UpdateContext(UGTPCombatInput::EMode::None);
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

void UGTPCombatSubsystem::DropWeapon(AActor* Body, UGTPWeaponComponent* Gun)
{
	UWorld* World = GetWorld();
	if (!World || !Body || !Gun || MaxPickups <= 0)
	{
		return;
	}
	const GTPArsenal::EKind Kind = Gun->GetKind();
	const int32 Rounds = GTPPickup::DropRounds(Gun->GetMagazine(), Gun->GetReserve(), Gun->GetSpec().MagazineSize);
	USkeletalMesh* Mesh = Gun->GetMeshFor(Kind);
	// Out of the hand for good: holstered (the mesh hides) and empty.
	Gun->SetDrawn(false);
	Gun->StripAmmo();

	// On the ground beside the body, toward where the blow or the bullet came from.
	const FVector Side = Body->GetActorRightVector() * 70.f;
	const FVector From = Body->GetActorLocation() + Side;
	FHitResult Ground;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GTPWeaponDrop), false, Body);
	const FVector Floor = World->LineTraceSingleByChannel(Ground, From, From - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params)
		? FVector(Ground.ImpactPoint) : From - FVector(0.f, 0.f, 90.f);
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGTPWeaponPickup* Pickup = World->SpawnActor<AGTPWeaponPickup>(Floor + FVector(0.f, 0.f, 80.f), FRotator(0.f, Body->GetActorRotation().Yaw, 0.f), SpawnParams);
	if (!Pickup)
	{
		return;
	}
	Pickup->Setup(Kind, Rounds, Mesh, PickupLifeSeconds);
	Pickups.RemoveAll([](const TWeakObjectPtr<AGTPWeaponPickup>& P) { return !P.IsValid(); });
	while (Pickups.Num() >= MaxPickups)
	{
		if (AGTPWeaponPickup* Oldest = Pickups[0].Get())
		{
			Oldest->Destroy();
		}
		Pickups.RemoveAt(0);
	}
	Pickups.Add(Pickup);
	GTP_TRACE(LogGTPCombat, TEXT("combat: %s dropped weapon %d with %d rounds"), *Body->GetName(), static_cast<int32>(Kind), Rounds);
}

void UGTPCombatSubsystem::SpawnBulletHole(const FHitResult& Hit, bool bGlass)
{
	UWorld* World = GetWorld();
	UMaterialInterface* Material = bGlass && LoadedGlassHole ? LoadedGlassHole.Get() : LoadedBulletHole.Get();
	if (!World || !Material || MaxBulletHoles <= 0)
	{
		return; // loaded once in WarmUpEffects; missing asset = no holes
	}
	BulletHoles.RemoveAll([](const TWeakObjectPtr<UDecalComponent>& D) { return !D.IsValid(); });
	while (BulletHoles.Num() >= MaxBulletHoles)
	{
		if (UDecalComponent* Oldest = BulletHoles[0].Get())
		{
			Oldest->DestroyComponent();
		}
		BulletHoles.RemoveAt(0);
	}
	const float Size = BulletHoleSizeCm * (bGlass ? 2.5f : 1.f);
	FRotator Rotation = (-Hit.ImpactNormal).Rotation();
	Rotation.Roll = FMath::FRandRange(0.f, 360.f);
	// Attached to what was hit (a door, a bin that moves later), thin so it does not wrap round corners.
	UDecalComponent* Decal = nullptr;
	if (USceneComponent* Surface = Hit.GetComponent())
	{
		Decal = UGameplayStatics::SpawnDecalAttached(Material, FVector(4.f, Size, Size), Surface, NAME_None, Hit.ImpactPoint, Rotation,
			EAttachLocation::KeepWorldPosition, BulletHoleLifeSeconds);
	}
	if (Decal)
	{
		Decal->SetFadeOut(FMath::Max(0.f, BulletHoleLifeSeconds - 5.f), 5.f, false);
		Decal->SetFadeScreenSize(0.002f);
		BulletHoles.Add(Decal);
	}
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

void UGTPCombatSubsystem::BenchWalk()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !Cast<ACharacter>(Pawn))
	{
		return;
	}
	// Half speed on a slow turn (15 degrees a second): a circle of about 6 m, seen from every side by the camera.
	BenchWalkSeconds += 0.02f;
	const FRotator Heading(0.f, 15.f * BenchWalkSeconds, 0.f);
	Pawn->AddMovementInput(Heading.Vector(), 0.5f);
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
	// Standing: shoulder aim held, as a player fires in GTA (aim pose, aim camera). Walking: from the hip, so
	// the wide camera shows the legs walking while the reload plays on the upper body.
	const bool bAim = BenchMode != 2;
	if (Weapon->IsAiming() != bAim)
	{
		Weapon->SetAiming(bAim);
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
