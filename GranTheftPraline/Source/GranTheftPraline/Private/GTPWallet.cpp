#include "GTPWallet.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GTPLog.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(Wallet, Log, All);   // lines read "Wallet: ..." in every build

void UGTPWalletSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	int32 Start = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("Money="), Start) && Start > 0)
	{
		Money.Cents = static_cast<int64>(Start) * 100;
	}
	AddCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("GTPMoney.Add"), TEXT("GTPMoney.Add <euros>: adds (or takes) money, for tests"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			if (Args.Num() > 0)
			{
				AddMoney(FCString::Atoi(*Args[0]), TEXT("console"), NAME_None);
			}
		}),
		ECVF_Cheat);
}

void UGTPWalletSubsystem::Deinitialize()
{
	if (AddCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(AddCommand);
		AddCommand = nullptr;
	}
	Super::Deinitialize();
}

UGTPWalletSubsystem* UGTPWalletSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGTPWalletSubsystem>() : nullptr;
}

bool UGTPWalletSubsystem::AddMoney(int32 Euros, FName Reason, FName RunKey)
{
	return AddCents(static_cast<int64>(Euros) * 100, Reason, RunKey);
}

bool UGTPWalletSubsystem::AddCents(int64 Cents, FName Reason, FName RunKey)
{
	const std::string Key = RunKey.IsNone() ? std::string() : std::string(TCHAR_TO_UTF8(*RunKey.ToString()));
	if (!Money.Add(Cents, Key))
	{
		GTP_TRACE(Wallet, TEXT("refused %+lld c (%s, key %s): %s"), static_cast<long long>(Cents), *Reason.ToString(), *RunKey.ToString(),
		          Money.WasPaid(Key) ? TEXT("already paid") : TEXT("not enough money"));
		return false;
	}
	GTP_TRACE(Wallet, TEXT("%+lld c (%s, key %s), balance %lld c"), static_cast<long long>(Cents), *Reason.ToString(), *RunKey.ToString(),
	          static_cast<long long>(Money.Cents));
	OnMoneyChanged.Broadcast(Money.Cents, Cents, Reason);
	OnMoneyChangedEuros.Broadcast(GetMoney(), static_cast<int32>(Cents / 100));
	return true;
}

FString UGTPWalletSubsystem::SaveToString() const
{
	return UTF8_TO_TCHAR(Money.Save().c_str());
}

bool UGTPWalletSubsystem::LoadFromString(const FString& Text)
{
	const int64 Before = Money.Cents;
	if (!Money.Load(TCHAR_TO_UTF8(*Text)))
	{
		return false;
	}
	OnMoneyChanged.Broadcast(Money.Cents, Money.Cents - Before, TEXT("load"));
	OnMoneyChangedEuros.Broadcast(GetMoney(), static_cast<int32>((Money.Cents - Before) / 100));
	return true;
}
