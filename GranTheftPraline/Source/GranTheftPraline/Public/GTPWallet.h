#pragma once

#include "CoreMinimal.h"
#include "GTPWalletModel.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GTPWallet.generated.h"

class IConsoleObject;

/** Balance after the change (cents), the change (cents, negative when spent), and why. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGTPMoneyChanged, int64 /*BalanceCents*/, int64 /*DeltaCents*/, FName /*Reason*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGTPMoneyChangedDynamic, int32, BalanceEuros, int32, DeltaEuros);

/**
 * The player's money (rules in GTPWalletModel.h). Lives on the game instance, so it survives
 * the change of map. Missions pay rewards with AddMoney, the restaurant hold-up credits each
 * till emptied, shops will spend with a negative amount; the HUD reads GetMoney and listens to
 * OnMoneyChanged. A run key makes a payment idempotent: the same key is never paid twice.
 *
 * Command line: -Money=N sets the starting balance in euros (tests). Console: GTPMoney.Add <euros>.
 * Log lines "Wallet: ..." on every change.
 * Save (ADR-0013): SaveToString / LoadFromString, to be stored by the SaveGame when it lands.
 */
UCLASS()
class GRANTHEFTPRALINE_API UGTPWalletSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The wallet of the game instance of WorldContext, null outside a game. */
	static UGTPWalletSubsystem* Get(const UObject* WorldContext);

	/** Adds whole euros (negative spends). False when RunKey was already paid or the balance would go below zero. */
	UFUNCTION(BlueprintCallable, Category = "Money")
	bool AddMoney(int32 Euros, FName Reason, FName RunKey);

	/** Same in cents (till cash: notes and coins). */
	bool AddCents(int64 Cents, FName Reason, FName RunKey = NAME_None);

	UFUNCTION(BlueprintPure, Category = "Money")
	int32 GetMoney() const { return static_cast<int32>(Money.Cents / 100); }

	int64 GetCents() const { return Money.Cents; }

	UFUNCTION(BlueprintPure, Category = "Money")
	bool WasPaid(FName RunKey) const { return Money.WasPaid(TCHAR_TO_UTF8(*RunKey.ToString())); }

	FString SaveToString() const;
	bool LoadFromString(const FString& Text);

	FGTPMoneyChanged OnMoneyChanged;

	UPROPERTY(BlueprintAssignable, Category = "Money")
	FGTPMoneyChangedDynamic OnMoneyChangedEuros;

private:
	GTPWallet::FWallet Money;
	IConsoleObject* AddCommand = nullptr;
};
