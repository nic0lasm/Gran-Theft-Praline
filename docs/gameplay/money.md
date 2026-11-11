# Money

The player's money lives in `UGTPWalletSubsystem` (game instance, survives map changes), rules in
`GTPWalletModel.h`, tested by `tools/interiors/core_test`.

- One balance in cents. `AddMoney(euros, reason, runKey)` for rewards, `AddCents` for till cash,
  a negative amount spends. The balance never goes below zero: a payment it cannot cover is refused.
- A run key (`act_holdup_shop#3`, `holdup#2/till1`) makes a payment idempotent: the same key is
  never paid twice, so a retried mission or a reloaded save cannot pay again.
- The HUD reads `GetMoney()` and listens to `OnMoneyChanged` (native, cents) or
  `OnMoneyChangedEuros` (Blueprint).
- Save (ADR-0013): `SaveToString` / `LoadFromString` keep the balance and the paid keys.
- Tests: `-Money=N` starting balance in euros, console `GTPMoney.Add <euros>`. Log lines `Wallet: ...`.
