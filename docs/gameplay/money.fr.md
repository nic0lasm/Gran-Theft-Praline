# Argent

L'argent du joueur vit dans `UGTPWalletSubsystem` (instance de jeu, survit aux changements de
carte), règles dans `GTPWalletModel.h`, testées par `tools/interiors/core_test`.

- Un solde en centimes. `AddMoney(euros, raison, runKey)` pour les récompenses, `AddCents` pour
  l'argent des caisses, un montant négatif dépense. Le solde ne descend jamais sous zéro : un
  paiement qu'il ne couvre pas est refusé.
- Une clé (`act_holdup_shop#3`, `holdup#2/till1`) rend un paiement idempotent : la même clé n'est
  jamais payée deux fois, une mission rejouée ou une sauvegarde rechargée ne repaie pas.
- Le HUD lit `GetMoney()` et écoute `OnMoneyChanged` (natif, centimes) ou `OnMoneyChangedEuros`
  (Blueprint).
- Sauvegarde (ADR-0013) : `SaveToString` / `LoadFromString` gardent le solde et les clés payées.
- Tests : `-Money=N` solde de départ en euros, console `GTPMoney.Add <euros>`. Lignes de log `Wallet: ...`.
