# Recherche et police (jeu)

Code : `GTPWantedLogic.h` (règles, testées en CI), `GTPWanted.*` (composant et directeur), `GTPPoliceTactics.h`, `GTPPoliceOfficer.*`. Décisions : ADR-0004.

## Ce qui donne des étoiles

Chaque délit ajoute de la « chaleur » ; les étoiles sont un palier de la chaleur (1 à 20, 2 à 60, 3 à 120, 4 à 220, 5 à 360). Un délit ne compte que si un témoin a pu le voir ou l'entendre (`GTPWitnessLogic.h`) ; `-Witnesses=0` fait tout compter. La ligne de vue d'un témoin finit 150 cm au-dessus du lieu du délit ; ce que vise le délit (la voiture volée, la voiture qui a heurté quelqu'un) et le joueur ne la coupent jamais. Quand personne n'a rien vu, la ligne de log `crime N not witnessed` dit combien de gens étaient près et ce qui a coupé la vue du plus proche.

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
| `pursuit` (4) | La fuite continue 40 s, le niveau livré à lui-même (plus d'étoiles forcées, arrestation possible) | La police suit, et rien ne l'a aidée : sur le temps où une voiture de poursuite était sur la route, les voitures font 60 % de la moyenne du joueur et jamais moins de 25 km/h (sauf si elles restent à moins de 30 m de lui en moyenne : un joueur coincé), et une voiture est à moins de 80 m un quart du temps ; aucune voiture de remplacement n'a été créée à moins de 150 m du joueur, et le script n'a jamais remis la voiture du joueur sur une voie. Les voitures de barrage ne comptent pas. |
| `outcome` (5) | Le joueur renonce : la voiture s'arrête et attend | Arrêté puis relâché à pied, capable de marcher, sans étoile ; ou tué puis revenu ; ou semée. Tout ce qui a mis fin à la poursuite depuis son début compte. Une arrestation ne compte que si une voiture de police ou un agent était à moins de 10 m au moment où elle a été faite, et si le script de poursuite n'avait pas déplacé la voiture du joueur dans les 8 s précédentes (le détail dit qui a arrêté et de quelle distance). Échoue si le joueur est encore recherché 60 s après l'arrêt. |
| `wasted` (11) | Le joueur reçoit un coup mortel par le composant de santé | Mort, de nouveau debout (point hôpital, sinon point de départ) avec toute sa santé, capable de marcher, sans étoile. |

Les chiffres de la poursuite viennent du directeur de la police (`FGTPChaseTally`, `GTPChaseTally.h`, testé dans `tools/vehicles/wanted_sim/chase_tally_test.cpp`) : à chaque image tant que le joueur est recherché, la vitesse de chaque voiture de poursuite et la distance de la plus proche. Il les écrit toutes les 10 s (`police chase: units mean=... km/h ... nearest mean=... m`), et la démo de poursuite ajoute `police mean=` et `nearest mean=` à sa ligne `DemoChase`. Aucune unité n'est déplacée à la main. Les voitures que demande un niveau sont créées dans l'anneau d'apparition (50 à 120 m, hors de vue) ; une fois qu'il les a eues, une voiture envoyée en remplace une perdue (laissée à plus de 300 m, abandonnée, détruite) et elle est créée entre 150 et 255 m, devant un joueur qui roule quand une place s'y trouve : elle doit venir jusqu'à lui en roulant. Les lignes les comptent (`N sent, N of them replacements (nearest made N m from the player)`), et chaque voiture dit où elle a été créée (`police: <name> on lane N at ..., N m from the player (replacement)`).

L'image de la poursuite : à la fin de l'étape `chase`, le test met la vue pendant une seconde sur une caméra à 9 m devant la voiture du joueur, tournée vers elle et vers la voiture de police la plus proche, de jour (l'horloge est mise à 14 h s'il est avant 9 h ou après 16 h), flou de mouvement coupé, et écrit `gtarun-chase.png` dans le dossier des captures. Une ligne dit ce qu'elle montre (`GTARun: picture of the chase asked: ... N police car(s) on the road, N of them in the frame with nothing in front`) : la caméra du joueur regarde devant et ne montre jamais les voitures qui suivent.

Ce que ces étapes ne prouvent pas : qu'un joueur peut semer la police en conduisant (la voiture du test est conduite par un script, et renonce), ni qui peut tuer le joueur (le coup mortel est celui du test). Après l'issue, le parcours efface ce qui reste du niveau et coupe l'arrestation jusqu'à sa fin, pour que les étapes à pied testent le vol et le braquage, pas la police.

## Ce que la première porte a montré, et ce que fait une unité quand sa conduite tourne mal (19/11/2026)

Candidat 48, packagé : `pursuit` a échoué. Les voitures de poursuite roulaient à 13 km/h de moyenne (24 voulus, joueur 39) et bougeaient 29 % du temps. Leurs lignes donnent un accélérateur à 1,00 dans chaque relevé à l'arrêt et aucun obstacle vu : les conducteurs demandaient à avancer. Deux choses retenaient les voitures :

- une voiture à 0 km/h pendant 24 s sur ses quatre roues, en première, 2086 N.m sur chaque roue arrière et aucune roue qui tourne : elle poussait contre quelque chose. Une unité ne reculait jamais ; seule la voiture de la démo de poursuite savait le faire. Et la règle « stopped, pushing through » remettait le temps d'arrêt à zéro toutes les 4 s, si bien que la règle qui retire une unité bloquée après 12 s ne s'appliquait jamais ;
- des voitures avec les deux roues intérieures en l'air trois secondes après le départ, puis sans aucune roue au sol : la conduite demandait le braquage à fond pied au plancher (cible derrière la voiture, voie prise de biais), sur des pneus qui accrochent plus (frottement 0,7 x 3) qu'il n'en faut pour retourner la voiture (centre de gravité à 83 cm ; le côté véhicule de ce défaut est au fil Trafic).

L'étape `outcome` passait sur une arrestation que le test avait fabriquée lui-même : le script de poursuite venait de poser la voiture du joueur à 11 m d'une voiture de barrage. Le script laisse maintenant 60 m à toute voiture de police quand il déplace le joueur, l'arrestation est coupée 8 s après un tel déplacement, et l'étape vérifie qui a arrêté.

Depuis (`GTPUnitRecovery.h`, testé dans `tools/vehicles/wanted_sim/unit_recovery_test.cpp`) :

| Cas | Ce que fait l'unité |
| ---- | ---- |
| Toute conduite | Les pédales suivent le volant : plus d'accélérateur, et du frein, au-dessus de la vitesse que le braquage demandé permet de tenir (6 m/s2 : 15 km/h braqué à fond) ; le volant ne demande jamais plus de 7,5 m/s2 à la vitesse de la voiture ; pas plus de 0,6 d'accélérateur sous 20 km/h. |
| Immobile 1,5 s avec l'accélérateur tenu | Recule 1,8 s, roues braquées de l'autre côté, puis reprend sa voie. Pas à moins de 12 m du joueur recherché : là, une unité arrêtée l'encadre. |
| Retenue au même endroit (12 m) après trois marches arrière, ou aucune roue au sol (ou couchée) pendant 5 s | Abandonnée sur place : freins serrés, ses agents descendent si le joueur est à moins de 60 m, elle est retirée dès qu'elle n'est plus en vue, et une autre voiture est envoyée aussitôt (deux épaves au plus). Elle n'est jamais replacée sur une voie. Les 5 s laissent son tour à la règle du véhicule : une voiture couchée ou sur le toit est remise sur ses roues après 2,5 s, celle du joueur comme celles de l'IA, et une unité relevée ainsi repart. |

Une voiture abandonnée ne compte plus parmi les voitures de poursuite, ni dans la vitesse moyenne ; le compte est dans les mêmes lignes (`... N back-off(s), N unit(s) given up`), et une ligne par événement dit où est la voiture, comment elle repose et ce qu'elle a devant elle (`police unit: <nom> held with the throttle down, backing off at ... ahead: ...`). Rien de cela n'a tourné dans le moteur au moment où ces lignes sont écrites : la logique n'est testée qu'en dehors.

## Ce que la deuxième porte a montré : virages, impasses et un test qui aidait (19/11/2026)

Candidat 49, packagé : `pursuit` est passée (22 km/h pour 20 voulus, une voiture à moins de 80 m 56 % du temps), avec de l'aide : 4 unités replacées entre 50 et 120 m du joueur, et la voiture du joueur remise sur une voie 4 fois par le script. Trois causes, chacune reproduite hors du moteur.

**La vitesse d'un virage.** Là où deux voies se rejoignent à angle droit avec des points espacés de 10 à 20 m, l'angle était lu comme une grande courbe, un segment plus loin : 46 à 65 km/h voulus à un angle qui en vaut 19 (`GTPDrive::BendSpeed` mesurait le virage sur des segments entiers et sa distance depuis le point derrière la voiture). La voiture de la démo de poursuite et les unités partagent ce code : les deux sortaient de la route à ces angles. Le virage est maintenant mesuré sur 6 m de trajet de chaque côté du sommet, et sa distance depuis la voiture elle-même : 19 km/h quel que soit l'espacement (`bend_test.cpp`). Le point que vise le volant est compté depuis la voiture le long du trajet, et non depuis son point le plus proche. Et les réglages de poursuite (`GTPDrive::SetChaseDriving`, communs aux unités et à la voiture de la démo) prévoient un freinage de 4 m/s2 avec un frein qui répond à tout l'écart : avec 6 ou 7 m/s2 prévus et un frein à 0,4 de l'écart, les pédales étaient en retard sur le plan. Un modèle bicyclette conduit autour d'un angle droit y arrive entre 21 et 25 km/h avec des freins de 6 à 12 m/s2, là où les réglages du candidat 49 donnaient 26 à 54 (`corner_drive_test.cpp`). Le trafic calme utilise les mêmes décisions : sur les voies du quartier, la simulation de trafic passe de 974 images de contact à aucune (`tools/vehicles/traffic_sim`, un passage de 25 voitures, 300 s).

**Le chemin vers le joueur.** À chaque carrefour, une unité prenait la sortie dont le bout était le plus proche du joueur. Sur les voies du quartier (1043 voies, 137 impasses), ce parcours finit dans une impasse dans 193 poursuites sur 341 lancées entre 60 et 300 m de la cible. Et une unité arrêtée au bout d'une impasse repartait sur la même voie, pour de bon. Le chemin est maintenant le plus court sur les voies (`GTPLaneRoute.h`, Dijkstra, testé dans `lane_route_test.cpp`), recherché toutes les 2 s à mesure que le joueur bouge, en gardant les voies que la voiture ne peut plus éviter à sa vitesse. Au bout d'une impasse, l'unité fait demi-tour : elle prend la voie de retour, braquée à fond, en reculant quand elle bute contre le trottoir (un demi-tour en trois temps), et cette impasse est écartée de son chemin pendant une demi-minute (`police unit: <name> at the end of lane N, a dead end, at ...: turning round`).

**L'aide.** Les unités ne sont plus replacées près du joueur, les remplaçantes sont créées loin (ci-dessus), et l'étape `pursuit` échoue quand une remplaçante a été créée à moins de 150 m ou quand le script a déplacé la voiture du joueur, quels que soient les autres chiffres. Son critère de vitesse a un plancher fixe : 60 % de la moyenne du joueur rendait l'étape plus facile avec un joueur plus lent (20 km/h voulus pour un joueur à 33) ; il n'est plus jamais sous 25 km/h, la vitesse du trafic ordinaire dans des rues limitées à 20 ou 30 km/h. Le candidat 49 échouerait sur les deux points.

Rien de tout cela n'a tourné dans le moteur au moment où ces lignes sont écrites. Les voies utilisées hors du moteur sont celles du 11/11/2026, pas la carte de la porte ; le freinage réel des voitures n'est pas connu (les réglages lui laissent une marge, ils ne le mesurent pas).

## Interrupteurs

`-PolicePerception=0|1|2` (coupée, perception seule, les deux = défaut), `-PoliceEqs=0` (les policiers balaient de nouveau un cercle fixe), `-Police=0`, `-WantedStars=N`, `-WantedHold`, `-Witnesses=0`.
