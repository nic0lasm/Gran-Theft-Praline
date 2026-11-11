# Trajets de test

*English: [test-drives.md](test-drives.md)*

Un trajet de test est un itinéraire nommé que le jeu rejoue tout seul : une mesure, une vidéo ou
une vérification de gameplay parcourt les mêmes rues à la même vitesse sur chaque build.

## Éléments

| Élément | Rôle |
|---|---|
| `tools/test/scenarios.json` | trajets nommés : points de passage en mètres Unreal (x, y) |
| `tools/test/drive_route.py` | accroche les points aux voies de la carte, les relie le long des voies (sens uniques, pas de demi-tour), ajoute les vitesses de virage, d'accélération et de freinage, écrit un chemin horodaté `t_s,x_cm,y_cm,speed_kmh` |
| `GranTheftPraline/Content/Test/Drives/<nom>.csv` | les chemins horodatés, empaquetés avec le jeu |
| `UGTPTestDrive` (`-TestDrive=<nom>`) | pose la voiture au premier point, y fait monter le joueur, puis suit le chemin |
| `tools/test/run-drive.sh` | lance un trajet dans une build empaquetée et vérifie qu'il va au bout |

## Modes

- `-TestDriveMode=rail` (défaut) : la voiture est portée le long du chemin selon le temps de jeu,
  physique coupée. Même trajectoire de caméra à chaque passe, quels que soient la cadence, le
  trafic ou la build : à utiliser pour toute mesure A/B.
- `-TestDriveMode=physics` : vraies pédales et vrai volant (`GTPDriveLogic.h`, comme les
  conducteurs IA), arrêt devant un obstacle, recul en cas de blocage. Pour tester la conduite.

Autres arguments : `-TestDriveLoop`, `-TestDriveQuit`, `-TestDriveDelay=<s>` (3 par défaut).
Ajouter `-TestVehicle` si la carte n'a pas de voiture garée. Ne se combine pas avec `-DemoRoute`.

## Usage courant

```sh
# vérifier tous les trajets contre les voies de la carte (code 2 si un point est inatteignable)
python3 tools/test/drive_route.py --lanes GranTheftPraline/Content/City/Traffic/L_City_RueDeLaRepublique.lanes.csv --list
# ajouter un trajet : l'ajouter à scenarios.json, puis écrire son chemin et une vue de dessus
python3 tools/test/drive_route.py --lanes <lanes.csv> --scenario <nom> \
  --out GranTheftPraline/Content/Test/Drives/<nom>.csv --svg /tmp/<nom>.svg
# le jouer dans une build (rail), avec vidéo, ou en vraie conduite
tools/test/run-drive.sh Builds/0.3 hopital-bellecour
VIDEO=1 tools/test/run-drive.sh Builds/0.3 hopital-bellecour
tools/test/run-drive.sh Builds/0.3 grolee-cordeliers physics
# dans un banc : le trajet remplace -DemoRoute
EXTRA_ARGS="-TestVehicle -TestDrive=hopital-bellecour -TestDriveLoop" tools/perf/bench.sh <build>
```

Le jeu écrit `DemoPos t=... phase=testdrive` toutes les 5 s de conduite et `TestDrive end` à la
fin ; `tools/perf/compare_demo_positions.py` compare deux passes. En mode rail, deux passes doivent
coïncider à quelques centimètres près (à confirmer au premier passage sur le Mac). Les chemins versionnés viennent des voies de `tools/city/traffic.py`
(focus rue-de-la-republique) ; si les voies changent, `--list` signale le trajet qui ne passe plus.

## Trajets livrés

| Nom | Longueur | Durée | Contenu |
|---|---|---|---|
| `grolee-cordeliers` | 172 m | 38 s | rue droite, un carrefour |
| `gentil-bourse` | 166 m | 35 s | court, un virage à droite |
| `hopital-bellecour` | 576 m | 110 s | virages, quatre carrefours, place Bellecour |
| `longest` | 737 m | 136 s | plus longue chaîne de voies de la carte (`--longest`) |

Limites : en mode rail, la voiture roule sans rotation des roues ni charge moteur ; le mode
physique dépend du trafic et n'est pas identique au bit près.
