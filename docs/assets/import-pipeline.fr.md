# Pipeline d'import Tripo vers Unreal

*English version: [import-pipeline.md](import-pipeline.md). Règles suivies : [bible artistique](../art/art-bible.fr.md) sections 5 à 8. Catalogue : [asset-catalog.fr.md](../../production/asset-catalog.fr.md).*

But : transformer un GLB master Tripo (environ 1 M de sommets, 55-88 Mo) en un maillage
statique Unreal prêt à jouer par objet, valable pour les deux profils de rendu, sans geste manuel.

État : la partie préparation (Python, hors Unreal) est testée en CI sur un échantillon
procédural. La partie Unreal (`import_props.py`) est écrite pour l'API Python 5.8 mais
**pas encore exécutée dans l'éditeur** : premier essai sur le Mac = prochaine étape, sur
feu vert du coordinateur (une seule session d'éditeur à la fois).

## Flux

```
assets/tripo/w1/W1-07*.glb            masters, hors Git
        |  tools/assets/prepare_props.py   (n'importe quelle machine, Python seul)
        v
assets/staging/w1/W1-07/              hors Git, régénérable
   asset.json  W1-07_ultra.glb  textures/T_*_BC|ORM|N.png
        |  tools/assets/import-props-mac.sh   (éditeur Unreal fermé)
        v
Content/Environment/Props/<Famille>/<Asset>/
   SM_<Asset>_A   T_<Asset>_BC / _N / _ORM   MI_<Asset>
```

## Ce que fait la préparation

1. **Normaliser** : échelle pour que la plus grande dimension vaille la taille du catalogue
   (`size_m`, le plus grand nombre de la colonne Taille), pivot au centre du bas
   (`back_center` pour les objets muraux : centré en X/Y, collé à la face arrière),
   `rotate_y_deg` en option. Conventions glTF conservées (Y haut, +Z face, mètres) :
   l'import Unreal convertit en Z haut et centimètres. La bible veut la face sur +X dans
   Unreal : à vérifier au premier import, puis corriger par un `rotate_y_deg` global.
2. **Plafonner la source Nanite** au budget 5.1 de la bible selon la taille (petit sous
   1 m : 300 k triangles ; moyen 1-3 m : 1 M ; grand : 2 M) par décimation quadrique
   sensible aux attributs (meshoptimizer ; UV et normales guident les fusions). Un master
   sous le plafond est gardé entier.
3. **Textures** à côté du maillage : `T_<Asset>_BC` (sRGB), `_ORM` (R occlusion, G rugosité,
   B métal, linéaire, même empaquetage que glTF) et `_N`. Une carte absente devient une
   constante 4x4 issue des facteurs du matériau. Jamais réduites (plafond 4096 px) : le
   profil M1 économise la mémoire par le streaming et les groupes de LOD de texture
   (ADR-0001, bible 5.2).
4. **Recette de collision** : aucune pour les objets plats (moins de 3 cm) ou par exception ;
   une boîte pour les petits objets ; sinon une décomposition convexe dont le nombre de
   coques (1, 4 ou 8) suit l'écart à la convexité.
5. **Plan de LOD** (bible 5.1 et 6) : petits objets 3 LOD, autres 4 ; budget LOD0 M1
   3 k / 10 k / 30 k triangles ; tailles d'écran 1,0 / 0,5 / 0,25 / 0,1 ; environ 50 % de
   triangles en moins par LOD.
6. Les noms viennent de `assets/manifests/wave-01.json` (famille, `Type_Descripteur`), généré
   depuis le catalogue par `catalog_to_manifest.py`.

Relancer est incrémental : un asset dont master et réglages n'ont pas bougé est sauté
(`--force` pour refaire).

## Ce que fait le script Unreal

Un `SM_<Asset>_A` par objet (bible 6.1) : importe les trois textures avec les bons
drapeaux, crée `MI_<Asset>` depuis `M_Master_Surface` (créé dans `/Game/Materials/Masters/`
s'il manque, paramètres `BaseColor`, `ORM`, `Normal`), importe le maillage, construit la
collision d'après la recette, active Nanite avec le repli réglé sur le budget LOD0 M1,
génère la chaîne de LOD du plan, pose le tag `CatalogId` et supprime les matériaux
provisoires de l'importeur. Relancer remplace les assets. Un asset en échec n'arrête pas le
lot ; le code de sortie l'indique.

## Utilisation

```bash
pip install -r tools/assets/requirements.txt
python3 tools/assets/prepare_props.py --manifest assets/manifests/wave-01.json \
    --masters assets/tripo/w1 --staging assets/staging/w1 [--ids W1-01 W1-07] [--force]
tools/assets/import-props-mac.sh w1 [W1-01 W1-07]      # éditeur Unreal fermé
```

Masters retrouvés par préfixe d'identifiant (`W1-07.glb`). Les absents sont listés dans
`assets/staging/w1/report.json` (`no_master_yet`).

## Points ouverts

- **À vérifier au premier passage sur le Mac** : axe de face (+X), que le profil M1 (Nanite
  coupé) retombe bien sur le repli LOD0 puis la chaîne, et les noms exacts de l'API Python
  5.8 (`StaticMeshEditorSubsystem`). Le script échoue bruyamment par asset.
- **Calibrage des textures** (bible 4) : normales converties en DirectX ; taille choisie
  d'après la densité cible (10,24 px/cm, puissance de deux, jamais agrandie, 4K max,
  densité mesurée dans `asset.json`) ; albédo non métallique étiré dans 30-240 s'il en
  sort ; rugosité plate rendue légèrement variable. Le vrai dé-éclairage et un re-UV
  demandent une passe Blender. Les objets à transparence découpée demandent un matériau
  maître masqué.
- Les `.uasset` et textures importés vont dans Git LFS ; surveiller le quota
  ([git-lfs.fr.md](../project/git-lfs.fr.md)).
- Pas de dépendance à Blender : trimesh et meshoptimizer tournent en CI comme sur le Mac.
