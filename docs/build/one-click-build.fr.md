# Build macOS en un clic

*Version anglaise : [one-click-build.md](one-click-build.md).*

Objectif : double-cliquer sur `Builds/<version>/GranTheftPraline.app` et
jouer, sans éditeur ni terminal (depuis la version 0.1).

## Empaqueter

```sh
tools/build/package-mac.sh            # version lue dans DefaultGame.ini (0.2.0 -> 0.2), Shipping
tools/build/package-mac.sh 0.2 Development
```

- Lance `RunUAT BuildCookRun` (compilation, cuisson, pak, IoStore,
  archive) pour Mac.
- Range l'application dans `Builds/<version>/GranTheftPraline.app`, la
  signe en ad hoc (`codesign --sign -`) pour qu'elle s'ouvre sur ce Mac, et
  écrit `build-info.txt` (commit, durée, taille) et `package.log`.
- `UE_ROOT` change le chemin du moteur (par défaut
  `/Users/Shared/Epic Games/UE_5.8`). `Builds/` est ignoré par Git.
- Chien de garde : la cuisson peut se figer tout à la fin quand le serveur
  Zen local s'arrête. BuildCookRun est arrêté si son journal n'a pas grandi
  depuis `STALL_MINUTES` (25 par défaut) ou au bout de `MAX_MINUTES` (240
  par défaut), puis relancé `RETRIES` fois (1 par défaut) ; la cuisson est
  incrémentale, une relance reprend l'essentiel du travail.
- Identité de l'application : identifiant `io.github.nic0lasm.GranTheftPraline`
  (`[/Script/MacTargetPlatform.XcodeProjectSettings]` dans
  `DefaultEngine.ini`) et version tirée de `ProjectVersion`. Le script les
  écrit dans l'`Info.plist` empaqueté et refuse de livrer l'identifiant
  `com.YourCompany` du modèle.

## Tester

```sh
tools/build/test-build-mac.sh          # version courante, 10 minutes, vidéo de 2 minutes
tools/build/test-build-mac.sh 0.2 120 0
GAME_ARGS="-GfxProfile=ultra" tools/build/test-build-mac.sh 0.2 300 0
```

Lance le jeu empaqueté directement en jeu, enregistre chaque image et
vérifie qu'il se ferme proprement au bout du temps demandé, sans rapport de
plantage. Par défaut, les passes vidéo et endurance jouent le parcours
automatique (`-TestVehicle -DemoRoute`) pour filmer et mesurer marche et
conduite ; `DEMO_ROUTE=0` laisse le joueur immobile. Les preuves vont dans `Builds/<version>/evidence/<horodatage>/` :
`frametimes.csv`, `frametimes.summary.txt` (p50, p95, p99, part des images
sous 33,3 ms, délai avant la première image), `title.jpg`, `capture.mov`,
`result.txt`. La vidéo et l'image de l'écran titre sont filmées par le jeu
lui-même, puis assemblées par `tools/build/frames-to-movie.swift`
(frameworks système uniquement).
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
| `-CaptureVideo=<s>` | Enregistre les images rendues en JPEG pendant ce nombre de secondes (`-CaptureFps`, 10 par défaut ; `-CaptureDelay`, 3 par défaut ; `-CaptureDir`) |
| `-GfxProfile=<nom>` | Profil de rendu de l'ADR-0002 : `m1` (par défaut), `fidelity` ou `ultra` (réglages du projet seuls) |
| `-DemoRoute` | Joue seul un parcours fixe (regard circulaire, marche, montée dans la voiture de test, conduite, descente) pour filmer et mesurer sans joueur ; avec `-SkipTitle -TestVehicle` |

L'enregistreur fonctionne aussi en Shipping : il lit les temps des fils du
moteur (`frame`, `game`, `render`, `gpu`), pas le profileur. La minuterie GPU
de Metal renvoie parfois des valeurs absurdes (environ 89 000 ms) : ces
échantillons restent dans le CSV mais sont exclus des statistiques GPU, et
le résumé les compte. Le résumé indique aussi le profil graphique, la taille
de la fenêtre de rendu et le pourcentage d'écran.

## Profils graphiques

`UGTPGraphicsProfile` applique le profil au démarrage, avec les valeurs de
`tools/rendering/reference_scene/profiles.json`, reprises dans
`[/Script/GranTheftPraline.GTPGraphicsProfile]` de `DefaultGame.ini`.
`m1` (TSR, Lumen Lite, ombres Medium) garde la résolution interne mesurée,
50 % de 1080p, quel que soit l'écran : le pourcentage d'écran est recalculé
d'après la hauteur de la fenêtre de rendu, pour qu'un écran Retina ne rende
pas trois fois plus de pixels. Les valeurs sont posées en priorité « code »,
les réglages enregistrés du joueur ne peuvent donc pas fausser la mesure.

## Organisation du code

`GranTheftPraline/Source/GranTheftPraline/` :

| Classe | Rôle |
| ---- | ---- |
| `UGTPGameInstance` | Vit toute la partie ; retient que l'écran titre a été passé |
| `AGTPGameMode` | Mode de jeu par défaut ; pion jouable choisi par `PlayerPawnClass` dans `DefaultGame.ini` |
| `AGTPPlayerController` | Écran titre et menu pause (Échap, bouton Menu de la manette) |
| `UGTPTitleMenuWidget`, `UGTPPauseMenuWidget` | Menus construits en C++ avec UMG, sans asset de widget |
| `UGTPFrameTimeRecorder` | CSV de temps d'image et arrêt des tests d'endurance |
| `UGTPFrameCapture` | Filme le jeu de l'intérieur (sans autorisation d'enregistrement d'écran) |
| `UGTPGraphicsProfile` | Applique le profil de rendu de l'ADR-0002 (`-GfxProfile`) |
| `UGTPDemoRoute` | Parcours automatique pour les tests filmés et mesurés (`-DemoRoute`) |

La classe du personnage jouable doit aussi être cuite : mettre son dossier
dans `DirectoriesToAlwaysCook` ou la référencer depuis un asset cuit.
