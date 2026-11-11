#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPInteriorStreamingCore.h"
#include "GTPInteriorStreaming.generated.h"

class AActor;
class ULevel;
class ULevelStreamingDynamic;

/**
 * Streams interiors without a loading screen (ADR-0012, docs/gameplay/reference-interior.md).
 *
 * An interior is placed in the street by an anchor actor at its door, tagged
 *   GTP_InteriorAnchor, GTP_InteriorLevel=<package, e.g. /Game/Interiors/L_Interior_fastfood_bellecour>
 * The interior level (tools/interiors/unreal/build_interior_level.py) is built in the
 * anchor's local frame; it is loaded at the anchor transform when the player comes within
 * the preload radius and released past the unload radius (GTPInteriorStreamingCore.h).
 * Once visible, components tagged GTP_MinDensity=<n> are hidden above the requested density,
 * and those tagged GTP_NoShadowM1 lose their dynamic shadow on the m1 profile. Components tagged
 * GTP_NoCollision (micro objects) never collide.
 *
 * Off by default until measured. Command line:
 *   -Interiors=1              turns streaming on (0 or absent: anchors are ignored)
 *   -InteriorPreload=<m>      preload radius (default 40), -InteriorUnload=<m> (default 70)
 *   -InteriorDensity=0..3     object density (default 2 on m1, 3 otherwise)
 * Log lines "InteriorStreaming: load|visible|unload <level> t=<s>" let the bench line up a hitch.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPInteriorStreaming : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	int32 GetInteriorCount() const { return Interiors.Num(); }
	int32 GetDensity() const { return Density; }

	/** Changes the density of every visible interior at once (settings, bench). */
	void SetDensity(int32 NewDensity);

protected:
	UPROPERTY(Config)
	float PreloadRadiusM = 40.f;

	UPROPERTY(Config)
	float UnloadRadiusM = 70.f;

	UPROPERTY(Config)
	int32 MaxLoaded = 2;

	UPROPERTY(Config)
	float UpdateSeconds = 0.25f;

private:
	struct FInterior
	{
		FString Level;
		FTransform Anchor;
		TWeakObjectPtr<ULevelStreamingDynamic> Streaming;
		bool bRequested = false;
		bool bWasVisible = false;
	};

	void Update();
	void RequestLoad(FInterior& Interior);
	void Release(FInterior& Interior);
	void ApplyToLevel(ULevel* Level) const;
	bool IsM1Profile() const;

	TArray<FInterior> Interiors;
	GTPInterior::FStreamingRules Rules;
	int32 Density = 2;
	bool bEnabled = false;
	float SinceUpdate = 0.f;
};
