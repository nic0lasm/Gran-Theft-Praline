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
   `/Game/Maps/L_RueRepublique` from scratch:
   - collisions: buildings and ground block (complex as simple, so courtyards
     and L-shaped blocks are exact), road ribbons do not;
   - camera path actor removed, `PlayerStart` 5 m into the street, facing it;
   - four invisible walls (`Gameplay/Bounds`, tag `PlayBounds`) 60 m high
     along the ground edges, kill-Z at -30 m;
   - line-trace checks (spawn on the ground and under open sky, facades block
     on both sides, walls block, kill-Z), report in
     `GranTheftPraline/Saved/PlayableStreet/report.json`, `RESULT PASS` in
     the log.
3. `playtest_street.sh` plays the level in `-game` mode and runs
   `playtest_street.py`: screenshot at spawn, pawn driven into a facade and
   into a wall, both must stop it. Report `playtest.json` and screenshots
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
   `/Game/Maps/L_RueRepublique` de zéro :
   - collisions : bâtiments et sol bloquent (collision complexe utilisée
     comme simple, donc cours intérieures et îlots en L exacts), les rubans
     de route non ;
   - trajet caméra retiré, `PlayerStart` à 5 m dans la rue, tourné vers elle ;
   - quatre murs invisibles (`Gameplay/Bounds`, tag `PlayBounds`) de 60 m
     le long des bords du sol, kill-Z à -30 m ;
   - contrôles par lancer de rayons (apparition au sol et à ciel ouvert,
     façades bloquantes des deux côtés, murs bloquants, kill-Z), rapport dans
     `GranTheftPraline/Saved/PlayableStreet/report.json`, `RESULT PASS` dans
     le journal.
3. `playtest_street.sh` joue le niveau en mode `-game` avec
   `playtest_street.py` : capture au point d'apparition, pion poussé contre
   une façade puis contre un mur, les deux doivent l'arrêter. Rapport
   `playtest.json` et captures `spawn.png`, `facade.png`, `bounds.png` dans
   le même dossier.

L'éclairage et les réglages de rendu appartiennent à la scène témoin : les
changer là-bas, puis reconstruire ce niveau.
