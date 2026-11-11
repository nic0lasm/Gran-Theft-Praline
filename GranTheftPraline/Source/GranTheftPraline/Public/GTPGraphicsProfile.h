#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPGraphicsProfile.generated.h"

class FViewport;

/**
 * Applies a rendering profile from ADR-0002 when the game starts, so the
 * packaged build runs with the same settings that were measured.
 *
 * Profiles (tools/rendering/reference_scene/profiles.json has the same values):
 *   ultra     Epic scalability at 100 % resolution, set explicitly so the
 *             auto-detected settings saved on the machine cannot lower it
 *   fidelity  ADR-0002 fidelity mode: TSR 67 %, Lumen GI and reflections High
 *   m1        fidelity + Lumen Lite + shadows Medium, internal resolution of
 *             a 1080p image at 50 % whatever the display (default)
 *
 * Command line: -GfxProfile=ultra|fidelity|m1 (default: DefaultProfile in
 * DefaultGame.ini). Values are set with code priority, so saved user
 * settings cannot silently override what is being measured.
 */
UCLASS(Config = Game)
class GRANTHEFTPRALINE_API UGTPGraphicsProfile : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Switches to another profile while the game runs (settings screen): puts
	 * back the values found at start, then applies the named profile.
	 * Names as for -GfxProfile. The cvar values stay those of the profiles.
	 */
	void SetProfile(const FString& Name);

	/** Active profile name, for logs and the frame time summary. */
	const FString& GetActiveProfile() const { return ActiveProfile; }

	/** Screen percentage last applied (0 when the profile leaves it alone). */
	float GetAppliedScreenPercentage() const { return AppliedScreenPercentage; }

	/** Last known viewport size in pixels. */
	FIntPoint GetViewportSize() const { return ViewportSize; }

private:
	void ApplyProfile(FString Requested);
	void SetCVar(const FString& Name, const FString& Value);
	void ApplyCVars(const TArray<FString>& Pairs);
	void HandleViewportResized(FViewport* Viewport, uint32 Unused);
	void UpdateScreenPercentage(int32 ViewportHeight);

	/** Profile used when the command line names none. */
	UPROPERTY(Config)
	FString DefaultProfile = TEXT("m1");

	/** "cvar=value" pairs of the ultra tier. */
	UPROPERTY(Config)
	TArray<FString> UltraCVars;

	/** "cvar=value" pairs of the fidelity mode. */
	UPROPERTY(Config)
	TArray<FString> FidelityCVars;

	/** "cvar=value" pairs applied on top of the fidelity mode for m1. */
	UPROPERTY(Config)
	TArray<FString> M1CVars;

	/** m1: output height the 50 % internal resolution was measured at. */
	UPROPERTY(Config)
	float M1ReferenceHeight = 1080.f;

	/** m1: screen percentage at the reference height. */
	UPROPERTY(Config)
	float M1ScreenPercentage = 50.f;

	/** Value of each cvar before the profile first changed it. */
	TMap<FString, FString> OriginalValues;

	FString ActiveProfile;
	float AppliedScreenPercentage = 0.f;
	FIntPoint ViewportSize = FIntPoint::ZeroValue;
	FDelegateHandle ResizeHandle;
};
