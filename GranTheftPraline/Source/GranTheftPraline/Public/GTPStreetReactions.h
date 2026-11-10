#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GTPStreetReactions.generated.h"

class AActor;
class UWorld;

/**
 * Sounds that answer what happens in the street (ADR-0009), owned by UGTPAudioWorld:
 * - people: a cry when hit by a car (UGTPPedestrianSubsystem::OnPedestrianHit),
 *   a scream or a gasp when frightened (OnPedestrianScared), at their place,
 *   rate-limited so a fleeing crowd does not become a wall of screams;
 * - police radio (2D) while the player is wanted (UGTPWantedComponent): alert
 *   tones and a call for each star gained, a "clear" call when the pursuit
 *   ends, squelched chatter in between; plus the StarUp/StarDown HUD sounds.
 * Both sides are only listened to: the pedestrian and police code know nothing of it.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPStreetReactions : public UObject
{
	GENERATED_BODY()

public:
	void Start(UWorld* World);
	void Stop();
	void Tick(float DeltaTime);

	/** "voices 3 (hit 1, scared 2), radio 4, stars 2" for the stats line. */
	FString Describe() const;

private:
	void OnPedestrianHit(const FVector& Location, AActor* Cause);
	void OnPedestrianScared(const FVector& Location, AActor* Cause);
	void UpdatePoliceRadio(float DeltaTime);

	TWeakObjectPtr<UWorld> WorldPtr;
	FDelegateHandle HitHandle;
	FDelegateHandle ScaredHandle;
	double LastScreamTime = -1e9;
	int32 HitCount = 0;
	int32 ScaredCount = 0;

	int32 LastStars = 0;
	/** Delay before the call that follows the alert tones (s), -1 when none is due. */
	float PendingAlert = -1.f;
	float ChatterTimer = 0.f;
};
