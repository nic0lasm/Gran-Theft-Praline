# ADR-0004 : Niveau de recherche et poursuite de police

*English version: [adr-0004-wanted-police.md](adr-0004-wanted-police.md)*

## Statut

Proposé le 10/11/2026 pour la version après la 0.3 (code écrit dans le
cloud, pas encore compilé ni mesuré sur le Mac).

## Décideur

nic0lasm (chef de projet)

## Résumé

Un système de recherche façon GTA en quatre petites pièces, bâti sur le code
de trafic de la 0.3 (ADR-0003) plutôt que sur une nouvelle IA :

1. **Règles sans moteur** (`GTPWantedLogic.h`) : les délits ajoutent de la
   « chaleur », la chaleur donne 1 à 5 étoiles, être vu par la police
   maintient le niveau, se cacher le fait perdre, une voiture de police
   proche et un joueur à l'arrêt donnent l'arrestation. C++ pur, testé hors
   d'Unreal (`tools/vehicles/wanted_sim/wanted_test.cpp`, lancé par la CI).
2. **`UGTPWantedComponent`** sur le contrôleur du joueur : point d'entrée
   unique `ReportCrime(type, position)`, test de vue, arrestation, état lu
   par le HUD. Tout système peut déclarer un délit par là (piétons, armes
   plus tard).
3. **`AGTPPoliceDirector`** : fait apparaître des voitures de police sur les
   voies du trafic hors de vue, les conduit (droit sur le joueur quand il
   est vu, par les voies vers la dernière position connue sinon), les retire
   hors de vue quand le niveau est perdu.
4. **HUD** : cinq étoiles en haut à droite, fixes en poursuite, clignotantes
   pendant la recherche, barre bleue du temps de recherche restant, barre
   rouge de l'arrestation, bandeau « ARRÊTÉ ». Ajouté à `AGTPHUD` sans
   toucher au panneau de conduite ni aux aides.

## Règles (valeurs dans `FGTPWantedSettings`)

| Délit | Chaleur | Déclaré par |
|---|---|---|
| Voiture prise à son conducteur, vitre brisée | 25 | `AGTPVehicle::BeginEnter` |
| Personne percutée par la voiture du joueur (> 12 km/h) | 35 | `AGTPVehicle::OnBodyHit` |
| Voiture percutée (> 12 km/h) | 12 | idem |
| Voiture de police percutée | 45 | idem (tag `GTPPolice`) |

Étoiles à 20, 60, 120, 220, 360 de chaleur. Un même type de délit compte une
fois par 2 s. Temps de recherche après la dernière vue : 14 s + 4 s par
étoile. Arrestation : police à moins de 7 m pendant 3 s alors que le joueur
est à pied ou sous 12 km/h. L'arrestation efface le niveau, fait descendre
le joueur et le fige 3 s. Amendes, perte des armes et retour au commissariat
viendront avec l'argent et les armes.

## Budget de performance (M1, ADR-0002)

Une voiture de police coûte comme une voiture de trafic (même mode économe :
pas d'ombres lointaines, LOD lointain, lumières sans ombres). Unités par
niveau : 1, 2, 2, 3, 4 étoiles, jamais plus de `MaxUnits` (2 sur M1 tant que le coût du trafic n'est pas mesuré, 
`-PoliceMax=N` jusqu'à 3 en test). Ligne de commande : `-Police=0` coupe tout, `-WantedStars=N` force un niveau après 5 s (`-WantedDelay=S`), `-WantedHold` le garde ; une ligne `wanted:` toutes les 2 s dans le journal. Rampe lumineuse : deux lumières ponctuelles sans ombre par
voiture, commutées seulement au changement de phase. À zéro étoile, rien
d'autre qu'un test léger. Cible : p95 ≤ 42 ms avec trois voitures de police
et le trafic ; à mesurer.

## Limites de cette première étape

- Poursuite en ligne droite quand le joueur est vu, sinon par les voies :
  pas de recherche de chemin dans la ville, ni barrages ni herses.
- Les policiers restent dans leur voiture : arrestation par proximité.
- Pas de sirène, voiture générique (voitures du trafic, ou
  `PoliceVehicleClass` quand une livrée existera ; livrée réelle et logos
  hors Git).
- Tout délit est déclaré ; témoins et radio de police viendront plus tard.
- Pas d'hélicoptère à 4-5 étoiles.

## Coordination

- Voies, pilotes PNJ et HUD appartiennent au fil 0.3 : ce travail n'ajoute
  que `FGTPLaneFollower::SetGoal`, `AGTPTrafficNetwork::PickNextToward`, une
  déclaration friend et une méthode de HUD.
- Piéton heurté : le fil piétons appelle
  `UGTPWantedComponent::ReportCrime(EGTPCrimeKind::HitPedestrian, Position)`.
- Aucune modification de la carte : tout apparaît au runtime.

## Correction de l'à-coup d'apparition (11/11/2026)

Le banc M1 a montré un à-coup (pic CPU de 47 ms) à chaque apparition d'unités de police : une unité était un personnage joueur complet (avec son système de caméra de gameplay) plus une voiture, créés dans la même image, puis détruits à la perte du niveau, environ toutes les 110 s. Maintenant : la voiture, le conducteur et la rampe sont créés sur trois images successives ; les unités sont gardées `IdleLifetime` (150 s) après la perte du niveau, à rouler sur les voies lumières éteintes, et une unité gardée loin d'une nouvelle poursuite est déplacée près du joueur hors de vue au lieu d'être recréée ; `PoliceDriverClass` permet de remplacer le personnage du joueur par un personnage léger sans caméra.
