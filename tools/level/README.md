# Playable street level / Niveau jouable de la rue

*Roadmap: version 0.1, task 5 ([production/roadmap.md](../../production/roadmap.md)).*

## EN

`L_RueRepublique` is the level of version 0.1: rue de la République (greybox
v0, 334 IGN buildings, 367 m of street) made walkable. It is built from the
reference street scene, so its lighting is exactly the measured `Noon` state.

1. Prerequisite: `tools/rendering/reference_scene/build_reference_scene.py`
   has run (it imports the greybox into `/Game/ReferenceScene/Geo` and builds
   `ReferenceStreet_Noon`).
2. `build_playable_street.py` (Unreal Editor Python) rebuilds
   `/Game/Maps/L_RueRepublique` from scratch. Without the UI (UE 5.8 on Mac
   has no `UnrealEditor-Cmd`), from a copy in a folder without spaces:
   `UnrealEditor GranTheftPraline.uproject -unattended -nosplash -ExecutePythonScript=/tmp/build_playable_street.py`.
   - collisions: buildings and ground block (complex as simple, so courtyards
     and L-shaped blocks are exact), road ribbons do not;
   - camera path actor removed, `PlayerStart` 5 m into the street, facing it;
   - four invisible walls (`Gameplay/Bounds`, tag `PlayBounds`) 60 m high
     along the ground edges, kill-Z at -30 m;
   - line-trace checks (spawn on the ground and under open sky, facades block
     on both sides with complex and with simple collision, at least 70 % of
     the samples since side streets open the street, walls block, kill-Z),
     report in
     `GranTheftPraline/Saved/PlayableStreet/report.json`, `RESULT PASS` in
     the log.
3. `playtest_street.sh` plays the level in `-game` mode and runs
   `playtest_street.py`: screenshot at spawn, then the pawn is pushed across
   the street towards the nearest facade, and above the roofs towards the
   north wall; the distance to each obstacle is measured first by a trace,
   and the pawn must stop before it. Report `playtest.json` and screenshots
   `spawn.png`, `facade.png`, `bounds.png` in the same folder.

Lighting and rendering settings belong to the reference scene: change them
there, then rebuild this level.

## FR

`L_RueRepublique` est le niveau de la version 0.1 : la rue de la République
(greybox v0, 334 bâtiments IGN, 367 m de rue) rendue praticable à pied. Il
part de la scène témoin, donc son éclairage est exactement l'état `Noon`
mesuré.

1. Prérequis : `tools/rendering/reference_scene/build_reference_scene.py` a
   tourné (import du greybox dans `/Game/ReferenceScene/Geo` et niveau
   `ReferenceStreet_Noon`).
2. `build_playable_street.py` (Python de l'éditeur Unreal) reconstruit
   `/Game/Maps/L_RueRepublique` de zéro. Sans interface (UE 5.8 sur Mac n'a
   pas `UnrealEditor-Cmd`), depuis une copie dans un dossier sans espace :
   `UnrealEditor GranTheftPraline.uproject -unattended -nosplash -ExecutePythonScript=/tmp/build_playable_street.py`.
   - collisions : bâtiments et sol bloquent (collision complexe utilisée
     comme simple, donc cours intérieures et îlots en L exacts), les rubans
     de route non ;
   - trajet caméra retiré, `PlayerStart` à 5 m dans la rue, tourné vers elle ;
   - quatre murs invisibles (`Gameplay/Bounds`, tag `PlayBounds`) de 60 m
     le long des bords du sol, kill-Z à -30 m ;
   - contrôles par lancer de rayons (apparition au sol et à ciel ouvert,
     façades bloquantes des deux côtés en collision complexe et simple, sur
     au moins 70 % des points car les rues transversales ouvrent la rue,
     murs bloquants, kill-Z), rapport dans
     `GranTheftPraline/Saved/PlayableStreet/report.json`, `RESULT PASS` dans
     le journal.
3. `playtest_street.sh` joue le niveau en mode `-game` avec
   `playtest_street.py` : capture au point d'apparition, puis le pion est
   poussé en travers de la rue vers la façade la plus proche, et au-dessus
   des toits vers le mur nord ; la distance à chaque obstacle est d'abord
   mesurée par un rayon, et le pion doit s'arrêter avant. Rapport
   `playtest.json` et captures `spawn.png`, `facade.png`, `bounds.png` dans
   le même dossier.

L'éclairage et les réglages de rendu appartiennent à la scène témoin : les
changer là-bas, puis reconstruire ce niveau.
