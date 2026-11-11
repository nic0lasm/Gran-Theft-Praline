# Missions et activités

*Version anglaise : [missions.md](missions.md)*

Décision et alternatives : [ADR-0014](../adr/adr-0014-missions-framework.fr.md). Aucun scénario, aucun
personnage nommé : une activité est un petit travail avec un but, un échec et un paiement.

## Déroulement

- **Départ** : un marqueur (carte, dès la 0.9) ou un point dans la rue. Dans la zone de départ,
  l'invite « Commencer : <titre> » s'affiche. Le départ est refusé pendant une autre mission,
  pendant le délai de reprise, une fois fait (non répétable) ou s'il manque un prérequis.
- **Objectifs** : joués dans l'ordre. Le HUD montre l'indice, la carte le point de passage. Les
  événements qui arrivent avant leur objectif sont ignorés.
- **Échec** : arrêté, mort, limite de temps, ou règle de l'objectif (voiture détruite, voiture
  abandonnée). Une carte propose Réessayer ou Abandonner. Réessayer reprend au dernier point de
  reprise ou recommence (selon la mission) ; le chrono de l'essai repart ; rien n'a été payé.
- **Succès** : la récompense est versée une seule fois, le passage est compté, le jeu sauvegarde.
- **Sauvegarde** : compteurs de passages, délais, mission en cours et son point de reprise. Charger
  reprend la mission en cours à son point de reprise.

## Données

`GranTheftPraline/Content/Data/Missions/` : `missions.csv`, `objectives.csv`, `zones.csv`,
`strings.tsv`. Types d'objectif : `GoTo` (zone, `max_stars`), `EnterInterior` (id d'intérieur),
`WaitEvent` (événement, `min`), `EnterVehicle` (étiquette de véhicule, `search_zone`),
`DeliverVehicle` (zone, `min_health`, `abandon_m`, `abandon_s`), `LoseWanted` (`hold_s`). Règles
vérifiées en CI (`tools/missions/mission_test`) : voir l'[ADR-0014](../adr/adr-0014-missions-framework.fr.md).

```
g++ -std=c++17 -I GranTheftPraline/Source/GranTheftPraline/Public tools/missions/mission_test/mission_test.cpp -o mission_test
./mission_test GranTheftPraline/Content/Data/Missions tools/missions/mission_test/fixtures
```

## Ce que le cadre lit et envoie

| Lu chez | Quoi | Sert à |
|---|---|---|
| Police (`UGTPWantedComponent`) | étoiles, arrestation, `player.arrested` | LoseWanted, GoTo `max_stars`, échec |
| Intérieur de référence | id de l'intérieur courant, `holdup.cash_taken` (valeur = euros) | EnterInterior, WaitEvent |
| Trafic et véhicules | étiquette du véhicule conduit, santé, distance à la voiture de mission, `vehicle.destroyed` | EnterVehicle, DeliverVehicle |
| Joueur | position, `player.died` | zones, échec |

| Envoyé à | Commande |
|---|---|
| Économie (fil Boutiques) | `GiveReward(montant, clé)`, versé une fois par clé |
| SaveGame (ADR-0013) | données de mission, écrites quand la sauvegarde est sale |
| Police | `EnsureStars` (activité de fuite), via `ReportCrime`, sans toucher au code police |
| Interface | clé d'indice, zone du point de passage, résultat, carte Réessayer ou Abandonner |
| Trafic / voitures garées | `SpawnVehicle` (étiquette près d'une zone) si aucune voiture ne convient |

## Activité 1 : braquer la boutique (`act_holdup_shop`)

- **Pitch** : entrer dans le fast-food de la place Bellecour, faire vider la caisse sous la menace,
  échapper à la police.
- **Départ** : dans la rue devant le restaurant (zone `z_start_holdup`).
- **Objectifs** : 1. entrer dans `fastfood_bellecour`. 2. attendre `holdup.cash_taken` (tout montant
  > 0 ; le butin va à l'argent du joueur par la caisse, pas par cette mission). 3. semer la police :
  étoiles à 0 pendant 2 s, en 180 s.
- **Échec** : arrêté, mort, 180 s écoulées. Réessayer recommence à l'entrée. L'argent déjà pris
  reste au joueur ou se perd selon les règles de l'économie (la police peut le récupérer), non décidé ici.
- **Récompense** : 150 € pour une fuite réussie (en plus du butin). Répétable, délai de 60 s.
- **Faux danger** : l'alarme (`holdup.alarm_raised`) n'est pas un objectif : la réponse du fil Police
  au délit fait la pression.
- **Dépend de** : intérieur de référence (caisse, script de braquage, `ReportCrime`), combat (menace).

## Activité 2 : voler une voiture sur commande (`act_order_car`)

- **Pitch** : un client veut une berline bleue. La trouver, la voler, la livrer à un garage fictif
  sans l'abîmer.
- **Départ** : un point de la rue (zone `z_start_order`) ; la zone de recherche `z_order_search`
  (90 m) est marquée sur la carte. Une voiture étiquetée `order_sedan_blue` est prise parmi les
  voitures garées de la zone, ou créée si elle manque.
- **Objectifs** : 1. conduire `order_sedan_blue` (la voiture est alors liée à la mission).
  2. l'amener à la zone du garage `z_garage_fictif` avec au moins 50 % de santé.
- **Échec** : la voiture liée est détruite ; vous la quittez à plus de 150 m pendant 20 s ; 600 s
  écoulées ; arrêté ou mort. Réessayer recommence à la recherche. Les étoiles du vol sont la
  conséquence normale, pas un échec.
- **Récompense** : 400 €. Répétable, délai de 60 s.
- **Dépend de** : voitures garées et étiquettes (trafic), recherche (délit de vol), économie.

## Activité 3 : semer la police (`act_lose_cops`)

- **Pitch** : la police vous a repéré. Lui échapper, puis rejoindre le refuge près des Terreaux
  sans être vu.
- **Départ** : un point de la rue (zone `z_start_flee`) ; le cadre monte le joueur à 2 étoiles au
  départ (`ensure_stars=2`) par l'API police.
- **Objectifs** : 1. étoiles revenues à 0 pendant 2 s. 2. atteindre `z_refuge_terreaux` à 0 étoile.
  Être vu dans la zone ne valide pas l'objectif.
- **Échec** : arrêté, mort, 300 s écoulées. Réessayer remonte les étoiles et recommence.
- **Récompense** : 250 €. Répétable, délai de 60 s.
- **Dépend de** : recherche (étoiles, état de fouille), trafic (voitures qui poursuivent).

## Fait quand (0.9)

Les trois activités jouées de bout en bout, trois fois chacune, en build packagée sans bloquant ;
échec et reprise fonctionnels ; récompense versée et sauvegardée ; départ depuis la carte ou un
point de la rue ; porte de fusion tenue. Coût sans mission active : 0 ms.

## Suite

Adaptateur Unreal `UGTPMissionSubsystem` (demande les API police, économie et intérieur sur `next`),
invite de départ et point de passage avec le fil Interface, interrupteur `-Missions=0`, carte
Réessayer/Abandonner, puis créneau machine pour l'A/B.
