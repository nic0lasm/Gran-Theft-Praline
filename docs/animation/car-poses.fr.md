# Poses d'entrée et de sortie de voiture (à la main, gratuit)

*Version anglaise : [car-poses.md](car-poses.md)*

Le pack d'animations payant de Fab a été écarté pour son prix (2026-11-10). Les
animations d'entrée et de sortie sont posées à la main à partir de données :

- `tools/animation/car_poses.json` : poses clés (tendre le bras, porte ouverte,
  pivoter et descendre, assis, fermer la porte, mains au volant) en rotations par
  os du mannequin UE. La sortie rejoue les clés d'entrée à l'envers, d'assis à debout.
- `tools/animation/build_car_poses.py` : construit les actions dans Blender et
  exporte `AM_EnterCar_Left.fbx` et `AM_ExitCar_Left.fbx` (30 i/s, 62 et 40
  images). Sans `--input`, il utilise un squelette de substitution avec les noms
  d'os du mannequin : l'exécution dans le cloud affiche PASS. Avec le FBX du
  mannequin exporté d'Unreal sur le Mac (`--input`), il pose les vrais os.

Les valeurs sont un premier jet : à regarder sur le vrai mannequin dans Blender et
à régler dans le JSON (sans toucher au code). Puis importer les FBX dans Unreal et
les affecter à `EnterMontage` et `ExitMontage` de `AGTPVehicle` ; la porte de
droite est le miroir de la gauche.
