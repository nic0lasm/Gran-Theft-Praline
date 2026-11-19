# Recherche et police (jeu)

Code : `GTPWantedLogic.h` (règles, testées en CI), `GTPWanted.*` (composant et directeur), `GTPPoliceTactics.h`, `GTPPoliceOfficer.*`. Décisions : ADR-0004.

## Ce qui donne des étoiles

Chaque délit ajoute de la « chaleur » ; les étoiles sont un palier de la chaleur (1 à 20, 2 à 60, 3 à 120, 4 à 220, 5 à 360). Un délit ne compte que si un témoin a pu le voir ou l'entendre (`GTPWitnessLogic.h`) ; `-Witnesses=0` fait tout compter.

| Ce que fait le joueur | Délit | Chaleur | Étoiles seul | Signalé par |
|---|---|---|---|---|
| Prend une voiture (vu) | CarTheft | 25 | 1 | `AGTPVehicle` |
| Renverse un piéton | HitPedestrian | 35 | 1 | `AGTPVehicle` |
| Percute une autre voiture | HitVehicle | 12 | 0 | `AGTPVehicle` |
| Percute une voiture de police | HitPolice | 45 | 1 | `AGTPVehicle` |
| Blesse quelqu'un (arme ou poings) | Assault | 15 | 0 | événements de combat |
| Blesse un policier | HitPolice | 45 | 1 | événements de combat |
| Tire (une fois par rafale de 5 s) | ShotsFired | 20 | 1 | événements de combat |
| Met quelqu'un en joue | ArmedThreat | 25 | 1 | événements de combat |
| Tue une personne | Killing | 60 | 2 | événements de combat |
| Tue un policier | KillPolice | 130 | 3 | événements de combat |

Les délits s'additionnent : vol de voiture, un tir, un choc avec une voiture de police font déjà 3 étoiles.

## Escalade

Voir ADR-0004 : niveau 1 une voiture à 70 km/h ; 2 deux voitures à 90, une arrive par l'avant ; 3 trois voitures à 105 qui percutent, deux voitures de barrage ; 4 et 5 quatre voitures à 120 et 135 km/h. Les policiers descendent quand le joueur est à pied ou immobile en voiture depuis 2 s, et tirent dès 2 étoiles si le joueur est agressif, dès 3 sinon.

## Faire tomber les étoiles

- Hors de vue, le compte à rebours de recherche tourne (14 s plus 4 s par étoile). Revu : il repart de zéro.
- Se cacher : hors de vue, immobile (moins de 1,5 m/s) et à plus de 25 m de toute unité, la recherche va deux fois plus vite.
- Changer de voiture (ou en sortir) hors de vue ne garde que 35 % du temps de recherche.

## Arrestation

Une unité à moins de 7 m d'un joueur à pied ou sous 12 km/h, trois secondes de suite. À la première seconde les policiers crient la sommation (`OnArrestWarning`, pour le son et le sous-titre). Ensuite : bandeau, arme rangée et munitions confisquées, étoiles effacées, puis après `BustedSeconds` le joueur réapparaît sur un TargetPoint étiqueté `GTPPoliceStation`, sinon à `ArrestReturnLocation`, sinon au point de départ.

## Mort

À la mort du joueur (`OnPlayerDied`), les signalements en attente sont abandonnés ; à la réapparition à l'hôpital, les étoiles sont effacées.

## Preuve dans le jeu packagé (`-GTARun`, 19/11/2026)

Quatre étapes du parcours automatique couvrent la recherche, comptées sur leur propre ligne (`GTARun police: N/4 ok`) ; les sept autres gardent la leur (`GTARun done: N/7 ok`).

| Étape | Ce qui se passe | Réussie quand |
| ---- | ---- | ---- |
| `witness` (2) | Juste après le vol de la voiture | Un délit signalé par téléphone (la victime, un passant) est arrivé à la police et a donné une étoile. Une étoile forcée par le script de poursuite ne compte pas. Échoue avec `-Witnesses=0`. |
| `pursuit` (4) | La fuite continue 40 s, le niveau livré à lui-même (plus d'étoiles forcées, arrestation possible) | La police suit : sur le temps où une voiture de poursuite était sur la route, les voitures font 30 km/h de moyenne (ou 60 % de la moyenne du joueur s'il est plus lent), et une voiture est à moins de 80 m un quart du temps. Les voitures de barrage ne comptent pas. |
| `outcome` (5) | Le joueur renonce : la voiture s'arrête et attend | Arrêté puis relâché à pied, capable de marcher, sans étoile ; ou tué puis revenu ; ou semée. Tout ce qui a mis fin à la poursuite depuis son début compte. Échoue si le joueur est encore recherché 60 s après l'arrêt. |
| `wasted` (11) | Le joueur reçoit un coup mortel par le composant de santé | Mort, de nouveau debout (point hôpital, sinon point de départ) avec toute sa santé, capable de marcher, sans étoile. |

Les chiffres de la poursuite viennent du directeur de la police (`FGTPChaseTally`, `GTPChaseTally.h`, testé dans `tools/vehicles/wanted_sim/chase_tally_test.cpp`) : à chaque image tant que le joueur est recherché, la vitesse de chaque voiture de poursuite et la distance de la plus proche. Il les écrit toutes les 10 s (`police chase: units mean=... km/h ... nearest mean=... m`), et la démo de poursuite ajoute `police mean=` et `nearest mean=` à sa ligne `DemoChase`. Une unité laissée à plus de 180 m est replacée près du joueur, hors de vue : ces déplacements sont comptés (`put back near the player`), et un déplacement que la voiture n'a pas gardé une demi-seconde plus tard est écrit (`the move did not hold`).

Ce que ces étapes ne prouvent pas : qu'un joueur peut semer la police en conduisant (la voiture du test est conduite par un script, et renonce), ni qui peut tuer le joueur (le coup mortel est celui du test). Après l'issue, le parcours efface ce qui reste du niveau et coupe l'arrestation jusqu'à sa fin, pour que les étapes à pied testent le vol et le braquage, pas la police.

## Interrupteurs

`-PolicePerception=0|1|2` (coupée, perception seule, les deux = défaut), `-PoliceEqs=0` (les policiers balaient de nouveau un cercle fixe), `-Police=0`, `-WantedStars=N`, `-WantedHold`, `-Witnesses=0`.
