#pragma once

#include "CoreMinimal.h"
#include "GTPHoldUpTills.h"
#include "Subsystems/WorldSubsystem.h"
#include "GTPHoldUp.generated.h"

class AGTPStaffCharacter;
class ULevel;

/** Cash handed over at one till: interior id, till index, amount in cents. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPHoldUpCashTaken, const FString& /*InteriorId*/, int32 /*Till*/, int64 /*Cents*/);
/** The hold-up ended: interior id, outcome ("escaped", "caught", "aborted"), total loot in cents. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPHoldUpEnded, const FString& /*InteriorId*/, FName /*Outcome*/, int64 /*LootCents*/);

/**
 * Hold-up of a restaurant (docs/gameplay/reference-interior.md, step 7). The staff carry the
 * shared threat response (fear, hands up, obeying, fleeing: ragdoll thread, GTPThreat.h); this
 * subsystem adds the shop: tills opened by a cashier who obeys, cash handed over and credited to
 * the wallet, the silent alarm under the counter, escapes through the back, the crime report to
 * the wanted level, and the events the missions read (holdup.cash_taken, holdup.ended).
 * Rules in GTPHoldUpTills.h, checked by tools/interiors/core_test.
 *
 * Off by default. Command line: -HoldUp=1, which needs -ThreatResponse=1 (the staff react
 * through it), -Interiors=1 and -InteriorStaff=N. Log lines "HoldUp: ...".
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPHoldUpSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static bool IsEnabled();

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** The kitchen spawned its crew in a visible interior. Roles are GTPKitchen::ERole values. */
	void BeginShop(const FString& InteriorId, const FTransform& Anchor, int32 TillCount, const TArray<AGTPStaffCharacter*>& Crew,
	               const TArray<uint8>& Roles);
	/** The interior is unloaded: a running hold-up ends as if the player left. */
	void EndShop();

	bool IsActive() const { return bShop && Run.bActive && Run.Outcome == GTPHoldUpTills::EOutcome::None; }
	int64 GetLootCents() const { return Run.Loot; }
	int64 GetCashLeftCents() const { return Run.CashLeft(); }

	FGTPHoldUpCashTaken OnCashTaken;
	FGTPHoldUpEnded OnEnded;

private:
	void Finish(bool bArrested);
	void Rearm();
	void HandleEvent(const GTPHoldUp::FEventRecord& Event);
	void OnInteriorChanged(const FString& OldId, const FString& NewId);
	void OnArrested() { Finish(true); }

	GTPHoldUpTills::FRun Run;
	TArray<TWeakObjectPtr<AGTPStaffCharacter>> Staff;
	FString InteriorId;
	FTransform Anchor;
	FDelegateHandle InteriorHandle;
	FDelegateHandle ArrestHandle;
	size_t EventsSeen = 0;
	int32 Serial = 0;
	float Accumulator = 0.f;
	bool bShop = false;
};
