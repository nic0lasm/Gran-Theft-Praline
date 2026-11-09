# Pipeline d'import Tripo vers Unreal

*English version: [import-pipeline.md](import-pipeline.md). Catalogue : [asset-catalog.fr.md](../../production/asset-catalog.fr.md).*

But : transformer un GLB master Tripo (environ 1 M de sommets, 64-74 Mo) en
assets Unreal prêts à jouer pour les deux profils de rendu, sans geste manuel.

État : la partie préparation (Python, hors Unreal) est testée en CI sur un
échantillon procédural. La partie Unreal (`import_props.py`) est écrite pour l'API
Python 5.8 mais **pas encore exécutée dans l'éditeur** : premier essai sur le Mac
= prochaine étape, sur feu vert du coordinateur (une seule session d'éditeur à la fois).

## Flux

```
assets/tripo/w1/W1-07*.glb            masters, hors Git
        |  tools/assets/prepare_props.py   (n'importe quelle machine, Python seul, ~1 s par asset)
        v
assets/staging/w1/W1-07/              hors Git, régénérable
   asset.json  W1-07_ultra.glb  W1-07_m1.glb  textures/*.png
        |  tools/assets/import-props-mac.sh   (éditeur Unreal fermé)
        v
Content/Props/W1/W1-07/  SM_W1_07 (M1, LODs)   MI_*   Textures/
Content/Props/W1/Ultra/  SM_W1_07_Ultra (Nanite, local seulement)
```

## Ce que fait la préparation

1. **Normaliser** : mise à l'échelle pour que la plus grande dimension vaille la
   taille du catalogue (`size_m`, le plus grand nombre de la colonne Taille),
   pivot au centre du bas (`back_center` pour les objets muraux : centré en X/Y,
   collé à la face arrière), `rotate_y_deg` en option. Les conventions glTF sont
   conservées (Y haut, +Z face, mètres) : l'import Unreal convertit en Z haut et centimètres.
2. **Deux géométries** :
   - `*_ultra.glb` : le master complet, normalisé (profil Ultra, Nanite).
   - `*_m1.glb` : maillage de base décimé pour le profil M1. Décimation quadrique
     sensible aux attributs (meshoptimizer ; UV et normales pèsent dans le calcul,
     donc coutures et ombrage tiennent). Budget selon la taille : moins de 1 m
     6 000 triangles, moins de 3 m 15 000, au-delà 30 000 ; `m1_tris` pour forcer par objet.
3. **Textures** : extraites à côté des maillages en `BaseColor` (sRGB), `ORM`
   (R occlusion, G rugosité, B métal, linéaire, même empaquetage que glTF) et
   `Normal`. Une carte absente devient une constante 4x4 issue des facteurs du
   matériau. **Les textures ne sont jamais réduites** (plafond 4096 px) : le profil
   M1 économise la mémoire par le streaming de textures et les réglages
   graphiques, pas en dégradant les assets (ADR-0001).
4. **Recette de collision** dans `asset.json` : `none` pour les objets plats (moins
   de 3 cm : plaque d'égout, grille) ou par exception, sinon décomposition convexe
   dont le nombre de coques (1, 4 ou 8) suit l'écart à la convexité.
5. **Plan de LOD** : 100 / 35 / 12 / 4 % des triangles M1 aux tailles d'écran
   1,0 / 0,45 / 0,20 / 0,08.

Relancer est incrémental : un asset dont le master et les réglages n'ont pas bougé
est sauté (`--force` pour refaire).

## Ce que fait le script Unreal

Par asset : importe les trois textures avec les bons drapeaux (sRGB, masques,
normal map), crée une instance de `/Game/Props/M_Prop_PBR` (créé au premier passage :
paramètres BaseColor, ORM, Normal), importe le maillage M1 (Nanite coupé, LODs
générés dans le moteur depuis le maillage de base, collision construite d'après la
recette) et le maillage Ultra (Nanite actif). Les matériaux provisoires de
l'importeur sont supprimés. Remplace les assets existants : relançable sans risque.
Un asset en échec n'arrête pas le lot ; le code de sortie l'indique.

## Utilisation

```bash
pip install -r tools/assets/requirements.txt
python3 tools/assets/catalog_to_manifest.py --out assets/manifests/wave-01.json   # seulement si le catalogue change
python3 tools/assets/prepare_props.py --manifest assets/manifests/wave-01.json \
    --masters assets/tripo/w1 --staging assets/staging/w1 [--ids W1-01 W1-07] [--force]
tools/assets/import-props-mac.sh w1 [W1-01 W1-07]      # éditeur Unreal fermé
```

Les masters sont retrouvés par préfixe d'identifiant (`W1-07.glb`, `W1-07_bin.glb`).
Les masters absents sont listés dans `assets/staging/w1/report.json` (`no_master_yet`).

## Choix et points ouverts

- **Pas de dépendance à Blender** : trimesh + meshoptimizer tournent en CI comme sur
  le Mac. Blender reste optionnel pour le nettoyage à la main (étape « nettoyé » du catalogue).
- **Deux maillages par objet** (`SM_x` et `SM_x_Ultra`) : les niveaux utilisent `SM_x` ;
  une build Ultra remplace par `_Ultra` via script. Les maillages Ultra restent hors
  Git (`.gitignore`) : plusieurs dizaines de Mo chacun, régénérables depuis les masters.
  L'outil de bascule viendra avec le travail sur le changement de profil.
- **Opaque seulement** : les objets à transparence découpée (grilles vues à travers,
  feuillage) demandent un matériau maître masqué ; à ajouter quand la vague 1 montrera lesquels.
- **Orientation** : suppose que Tripo regarde vers +Z. Si un modèle sort tourné,
  renseigner `rotate_y_deg` dans le manifeste.
- Textures et maillages M1 vont dans Git LFS ; surveiller le quota ([git-lfs.fr.md](../project/git-lfs.fr.md)).
