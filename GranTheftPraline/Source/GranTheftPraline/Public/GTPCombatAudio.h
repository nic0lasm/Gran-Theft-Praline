#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GTPAudioTypes.h"
#include "GTPCombatAudio.generated.h"

class AActor;
struct FHitResult;

/**
 * Combat sounds (ADR-0009, spec in docs/gameplay/combat.md "Sounds"). The combat
 * code calls these, or the audio world binds them to UGTPCombatSubsystem's
 * delegates once it exists (OnShotFired -> PlayShot, OnDamageDealt -> PlayHit):
 * - a shot is dry and short near the listener, a crack and a rolling tail far
 *   away, late by the speed of sound; street reflections follow with a delay set
 *   by the zone (narrow street, open quay, bridge); a bullet passing near the
 *   listener whizzes; the street ambience and the traffic duck for 0.5 s; people
 *   around run away (UGTPAudioWorld::OnLoudSound);
 * - bullet impacts per kind of surface, melee blows, weapon handling.
 * Every call does nothing with -Audio=0 or an empty bank.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPCombatAudio : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** A shot from Origin towards Direction (Range in cm: where the bullet stops, for the whizz). */
	UFUNCTION(BlueprintCallable, Category = "Audio|Combat", meta = (WorldContext = "WorldContext"))
	static void PlayShot(const UObject* WorldContext, AActor* Shooter, FVector Origin, FVector Direction, float Range = 10000.f);

	UFUNCTION(BlueprintCallable, Category = "Audio|Combat", meta = (WorldContext = "WorldContext"))
	static void PlayBulletImpact(const UObject* WorldContext, FVector Location, EGTPBulletImpact Kind);

	/** Kind of bullet impact for a trace hit: people, cars, then the surface (physical material, material name). */
	static EGTPBulletImpact ImpactKindOf(const UObject* WorldContext, const FHitResult& Hit);

	/** A hit taken by Victim: flesh for a bullet, a light or heavy blow for melee (heavy from HeavyDamage). */
	UFUNCTION(BlueprintCallable, Category = "Audio|Combat", meta = (WorldContext = "WorldContext"))
	static void PlayHit(const UObject* WorldContext, AActor* Victim, float Damage, bool bMelee);

	UFUNCTION(BlueprintCallable, Category = "Audio|Combat", meta = (WorldContext = "WorldContext"))
	static void PlayMelee(const UObject* WorldContext, FVector Location, EGTPMeleeSound Sound);

	/** Dry fire, magazine out and in, slide: at the weapon holder. */
	UFUNCTION(BlueprintCallable, Category = "Audio|Combat", meta = (WorldContext = "WorldContext"))
	static void PlayWeaponHandling(const UObject* WorldContext, AActor* Holder, EGTPWeaponSound Sound);

	/** Melee damage from which a blow sounds heavy (the third blow of a combo is 18). */
	static constexpr float HeavyDamage = 15.f;
};
