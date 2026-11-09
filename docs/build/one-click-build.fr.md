# Build macOS en un clic

*Version anglaise : [one-click-build.md](one-click-build.md).*

Objectif de la version 0.1 : double-cliquer sur
`Builds/0.1/GranTheftPraline.app` et jouer, sans éditeur ni terminal.

## Empaqueter

```sh
tools/build/package-mac.sh            # version lue dans DefaultGame.ini (0.1), Shipping
tools/build/package-mac.sh 0.1 Development
```

- Lance `RunUAT BuildCookRun` (compilation, cuisson, pak, IoStore,
  archive) pour Mac.
- Range l'application dans `Builds/<version>/GranTheftPraline.app`, la
  signe en ad hoc (`codesign --sign -`) pour qu'elle s'ouvre sur ce Mac, et
  écrit `build-info.txt` (commit, durée, taille) et `package.log`.
- `UE_ROOT` change le chemin du moteur (par défaut
  `/Users/Shared/Epic Games/UE_5.8`). `Builds/` est ignoré par Git.

## Tester

```sh
tools/build/test-build-mac.sh          # 0.1, 10 minutes, vidéo de 2 minutes
tools/build/test-build-mac.sh 0.1 120 0
```

Lance le jeu empaqueté directement en jeu, enregistre chaque image et
vérifie qu'il se ferme proprement au bout du temps demandé, sans rapport de
plantage. Les preuves vont dans `Builds/<version>/evidence/<horodatage>/` :
`frametimes.csv`, `frametimes.summary.txt` (p50, p95, p99, part des images
sous 33,3 ms, délai avant la première image), `capture.mov`, `result.txt`.
Les preuves retenues pour une version sont copiées dans
`production/evidence/<version>/` (vidéo via Git LFS).

## Options de ligne de commande du jeu

| Option | Effet |
| ---- | ---- |
| `-SkipTitle` | Démarre en jeu, sans l'écran titre |
| `-FrameLog` | Enregistre les temps d'image dans `Saved/Profiling/frametimes-<date>.csv` |
| `-FrameLogFile=<chemin>` | Enregistre les temps d'image dans ce fichier |
| `-FrameLogWarmup=<s>` | Ignore les premières secondes (5 par défaut) |
| `-ExitAfter=<s>` | Quitte proprement après ce nombre de secondes |

L'enregistreur fonctionne aussi en Shipping : il lit les temps des fils du
moteur (`frame`, `game`, `render`, `gpu`), pas le profileur.

## Organisation du code

`GranTheftPraline/Source/GranTheftPraline/` :

| Classe | Rôle |
| ---- | ---- |
| `UGTPGameInstance` | Vit toute la partie ; retient que l'écran titre a été passé |
| `AGTPGameMode` | Mode de jeu par défaut ; pion jouable choisi par `PlayerPawnClass` dans `DefaultGame.ini` |
| `AGTPPlayerController` | Écran titre et menu pause (Échap, bouton Menu de la manette) |
| `UGTPTitleMenuWidget`, `UGTPPauseMenuWidget` | Menus construits en C++ avec UMG, sans asset de widget |
| `UGTPFrameTimeRecorder` | CSV de temps d'image et arrêt des tests d'endurance |

La classe du personnage jouable doit aussi être cuite : mettre son dossier
dans `DirectoriesToAlwaysCook` ou la référencer depuis un asset cuit.
