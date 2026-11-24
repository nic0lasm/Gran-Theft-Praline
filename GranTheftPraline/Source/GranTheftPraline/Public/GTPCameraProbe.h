#pragma once

#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;
class UWorld;

/**
 * Camera collision shared by the on-foot follow camera and the car's chase camera, as the genre does it: only the
 * world's geometry and cars pull the camera in (walls, ground, buildings, a car). People and small props (poles, bins,
 * bollards, signs, anything whose bounds are under SmallRadius) do not: the camera passes them, and the people and
 * props between the camera and the player are hidden by UGTPCameraOccluders instead.
 */
namespace GTPCameraProbe
{
	/** Share (0..1) of the way From -> To the camera can travel; 1 when nothing large and static is in the way. */
	GRANTHEFTPRALINE_API float Sweep(UWorld* World, const FVector& From, const FVector& To, float Radius, float SmallRadius,
		const TArray<const AActor*>& Ignored, FString* HitName = nullptr);
}

/**
 * Hides the people and small props between the camera and the player for as long as they are in the way, and
 * shows them again after. Rendering only: their collision, AI and movement are untouched.
 */
struct GRANTHEFTPRALINE_API FGTPCameraOccluders
{
	/** Sweeps from the camera to the target; returns how many things are hidden now. */
	int32 Update(UWorld* World, const FVector& Camera, const FVector& Target, float Radius, float SmallRadius, const TArray<const AActor*>& Ignored);
	/** Shows everything again (camera handed over, end of play). */
	void Clear();

private:
	TArray<TWeakObjectPtr<UPrimitiveComponent>> Hidden;
};
