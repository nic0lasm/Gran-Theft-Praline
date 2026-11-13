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

## Délits de combat (11/11/2026)

Ajoutés pour le combat, déclarés par `ReportCrime` (chaleur) : agression 15, coups de feu 20, menace armée 25, meurtre 60 (2 étoiles), meurtre d'un policier 130 (3 étoiles). Coups de feu et menaces comptent une fois par 5 s (une rafale), les autres délits une fois par 2 s. Pas encore de témoins (tout délit compte).

## Témoins (11/11/2026)

Un délit ne compte que si quelqu'un l'a vu ou entendu (`GTPWitnessLogic.h`, testé en CI) : une voiture de police en vue à moins de 60 m le signale aussitôt ; une personne en vue à moins de 25 m appelle la police 4 s plus tard ; les coups de feu et les meurtres s'entendent sans vue (police 120 m, passants 40 m) ; la personne directement concernée (le conducteur sorti de force, un piéton percuté) compte comme une personne qui a vu. Personne autour : aucune chaleur. `-Witnesses=0` fait compter tout délit aussitôt (tests, démos scriptées) ; `bRequireWitnesses=False` dans la config aussi. Les passants sont cherchés parmi les acteurs piétons proches du délit ; les silhouettes de foule lointaine ne sont pas des témoins.

## Coût d'une unité de police et `-PoliceLite=1` (12/11/2026)

M1 en Shipping, scène fixe, `-WantedStars=2 -WantedHold -PoliceMax=6`, 600 s : GPU p50 18,1 ms et p95 29,2 ms contre 13,2 / 20,2 sans poursuite, soit environ 0,8 ms (p50) et 1,5 ms (p95) par unité. Estimation par élément (non mesurée par élément) : la rampe est faite de deux lumières ponctuelles sans ombre dont une seule est allumée à la fois, environ 0,05 à 0,1 ms par voiture ; le matériau émissif et le conducteur assis ne coûtent presque rien ; le reste (environ 0,6 à 1,3 ms) est la caisse en plein détail avec son ombre mobile, car une poursuite garde chaque unité à moins de 25 m du joueur : aucune n'a le LOD lointain ni l'ombre coupée. `-PoliceLite=1` (désactivé par défaut, pour l'A/B) : une lumière par voiture dont la couleur alterne, lumières seulement à moins de 30 m, ombre et plein détail seulement à moins de 15 m (au lieu de 25 m), et 3 unités au plus sauf si `-PoliceMax` est donné. Attendu : environ 0,1 ms par unité côté lumières et un tiers à la moitié du coût de caisse au-delà de 15 m ; à mesurer avec une scène de poursuite à 3 et 6 unités, Lite coupé et activé.

## Policiers à pied, tirs et recherche (12/11/2026)

Règles dans `GTPPoliceTactics.h` (C++ pur, testé en CI par `tactics_test`), comportement dans `AGTPPoliceOfficer` (corps léger, pas encore de maillage de navigation : il se dirige en ligne droite et glisse le long des murs).

- **Sortie de voiture** : joueur à pied à moins de 35 m d'une unité, ou à tout moment pour une unité bloquée à moins de 60 m ; quota par niveau 1/2/2/3/4, au plus `MaxOfficers` (3, `-PoliceOfficers=N`, 0 = voitures seules ; 2 avec `-PoliceLite=1`). Ils comptent pour la vue, les témoins et l'arrestation comme les voitures.
- **À pied** : ils courent (4,3 m/s, moins vite qu'un joueur qui sprinte : les policiers seuls n'attrapent pas un coureur, ce sont les voitures et les étoiles) vers le joueur tant qu'il est vu, sinon vers le dernier point connu, puis balaient un cercle autour qui s'élargit de 0,6 m/s de 15 m à 60 m, un secteur par policier.
- **Tirs** : jamais à une étoile ; à deux seulement 20 s après que le joueur a tiré, menacé ou frappé ; dès trois étoiles à vue à moins de 35 m avec ligne dégagée, par `UGTPWeaponComponent::FireAt`. La visée s'améliore et la cadence monte avec le niveau (dispersion 2,5 à trois étoiles, 0,35 de moins par étoile ; 1,1 s puis jusqu'à 0,5 s entre deux tirs).
- **Fin de service** : 15 s après la perte du niveau hors de vue du joueur, tous après 60 s.
- Pas fait : barrages et encerclement (logique dans `feat/police-roadblocks`, gelés jusqu'au scénario des traboules), animation de course (config `RunAnimation`, `-PoliceRunAnim=`), vrais chemins dans les traboules.
- Démo et preuve : `-WantedStars=3 -WantedHold -PoliceOfficers=3` avec le joueur à pied ; la trace `wanted:` donne `officers=N`, le journal `police: officer out`.

## Chasse façon GTA quand le joueur conduit (13/11/2026)

`GTPPoliceTactics::Profile(étoiles)` est la table d'escalade, testée en CI. Niveau 1 : une voiture à 70 km/h, sans percussion. Niveau 2 : deux voitures à 90 km/h, une sur deux arrive par l'avant d'un joueur en mouvement (point d'apparition à 100-170 m devant, dans 35 degrés du sens de marche, hors de vue). Niveau 3 : trois voitures à 105 km/h qui ignorent la sonde d'obstacle (elles percutent le trafic et la voiture du joueur) et deux voitures de barrage (un V en travers d'une route que le joueur suit, à 140-240 m devant, frein à main et gyrophares ; elles partent quand le joueur les a dépassées, ou 8 s après la perte du niveau). Niveaux 4 et 5 : quatre voitures à 120 et 135 km/h. Une unité plus lente que 15 km/h avec quelque chose à moins de 12 m devant pendant 1,5 s force le passage 3 s (dès le niveau 2). Les voitures suivent les voies à 90 % de la vitesse de chasse, vitesse affichée de la voie ignorée, avec une conduite plus dure (latéral 6 m/s2, freinage 7 m/s2, distance 3 m, gain d'accélérateur 0,8). `MaxUnits` vaut 4 (3 avec `-PoliceLite=1`). Policiers à pied, arrestation et tirs : comme décrit plus haut. Non compilé à l'écriture ; StateTree, AI Perception et EQS ne sont pas activés dans le projet, ces règles restent donc de la logique simple en attendant.
