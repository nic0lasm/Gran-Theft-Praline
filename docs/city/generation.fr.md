# Générer la Presqu'île en 3D : LiDAR, données Grand Lyon et façades procédurales

*English version: [generation.md](generation.md)*
*Rédigé le 2026-11-09. Étape 2 du [plan ville](plan.fr.md). Code : [`tools/city/`](../../tools/city/).*
*Budgets et nommage : [bible artistique](../art/art-bible.fr.md). Tout ce qui est généré reste hors Git et se régénère.*

## Résultat en un paragraphe

À partir des seules données ouvertes, sans clé ni éditeur, le pipeline
reconstruit chaque bâtiment de la rue de la République (Terreaux → Bellecour,
45 m de part et d'autre) en greybox qui a déjà son vrai volume, son vrai toit,
ses cours, ses étages, ses travées, ses portes, ses balcons et **les vraies
enseignes dans les bonnes travées**. 175 bâtiments BD TOPO deviennent 281
volumes (cours séparées), 13,2 k modules de façade, 215 enseignes dont 106
marques, 288 k triangles, en 16 s. Les toits collent au LiDAR IGN récent à
**0,39 m en médiane** (90 % des cellules de 0,5 m à moins de 2,2 m). La même
liste de modules alimentera ensuite le kit de façades du moteur.

Les aperçus sont rendus par le pipeline lui-même (`render.py`, sans moteur) dans
`assets/geo/processed/presqu-ile/gen/rue-de-la-republique/preview_*.png`.

## Sources ajoutées (toutes testées le 2026-11-09)

| Source | Accès | Licence | Sert à |
|---|---|---|---|
| IGN **LiDAR HD** MNT / MNS / MNH, 0,5 m | WMS-Raster Géoplateforme `data.geopf.fr/wms-r`, grilles float32, 40 s pour 1,4 × 1,4 km | Licence Ouverte Etalab 2.0 | Terrain, sol de chaque bâtiment, égouts de toit, surface des toits, cours, contrôle de chaque toit |
| **Lignes de faîtage 3D** Grand Lyon (2012) | WFS `download.data.grandlyon.com/wfs/grandlyon`, couche `fpc_fond_plan_communaut.fpclignefaitage` | Licence Ouverte | Faîtages et arêtiers (gardés seulement si le LiDAR les confirme) |
| **Arbres d'alignement** Grand Lyon | `abr_arbres_alignement.abrarbre` | Licence Ouverte | Essence, hauteur, fût, rayon de couronne (2 153 arbres, mieux qu'OSM) |
| **Bordures, îlots, points cotés au sol** Grand Lyon | `fpcbordchaussee`, `fpcilot`, `fpcpointcotesol` | Licence Ouverte | Bords de trottoir et contrôle du sol (étape suivante : voirie) |
| **Corbeilles, arceaux vélo, abris** Grand Lyon | `gincorbeille`, `pvostationnementvelo`, `pvomobilierurbain` | Licence Ouverte | Mobilier positionné *avec son modèle* (ex. « Arceaux Wilmotte ») |
| **Bancs, fontaines ornementales** Grand Lyon | `adrbanc_latest`, `adrfontaineornem_latest` | ODbL (repris d'OSM) | Positions du mobilier ; hors Git comme toute table issue d'OSM |
| **Maquette CityGML** texturée Grand Lyon | zips par arrondissement, **2012** | Licence Ouverte | Non utilisée : 14 ans d'âge ; façades par le kit, toits par le LiDAR |

Licences lues sur le portail (champ « Licence » de chaque jeu). Crédits à
ajouter au jeu : « IGN, LiDAR HD », « Métropole de Lyon ».

## Pipeline

```
fetch.py     -> raw/<zone>/       + lidar_<mnt|mns|mnh>.f32, lidar.json, grandlyon_<couche>.geojson
build.py     -> processed/<zone>/ (inchangé : bâtiments, commerces, arbres, mobilier, routes...)
generate.py  -> processed/<zone>/gen/<rue>/   buildings_gen.csv, facade_modules.csv, greybox.glb, report.json
terrain.py   -> processed/<zone>/terrain/     heightmap.png (Landscape), terrain.glb (tuiles), terrain.json
render.py    -> processed/<zone>/gen/<rue>/preview_<vue>.png
unreal/import_city.py (éditeur, sans interface) -> /Game/Maps/L_City_<Rue>
```

Commandes (Python 3 + `pip install -r tools/city/requirements.txt`) :

```
python3 tools/city/fetch.py --name presqu-ile --bbox 45.7570,4.8250,45.7690,4.8420
python3 tools/city/build.py --name presqu-ile
python3 tools/city/generate.py --name presqu-ile --focus "Rue de la République" --buffer 45
python3 tools/city/terrain.py --name presqu-ile
python3 tools/city/render.py --name presqu-ile --view street:176,-60,1.8,179,150,9
```

`--focus all` génère toute la zone (2 601 bâtiments).

## Comment un bâtiment est construit

1. **Emprise** (BD TOPO), découpée par le LiDAR : les zones connexes d'au moins
   6 m² au niveau du sol sont des **cours ouvertes** (retirées) ; celles à au
   moins 4 m sous le toit principal sont des **cours couvertes** (toitures des
   commerces au rez-de-chaussée, très fréquentes ici) et reviennent comme
   volumes bas à toit plat (« socles »). Rue de la République : 3 118 m² de
   cours couvertes trouvées.
2. **Sol** : point de terrain LiDAR le plus bas le long du contour.
3. **Égout du toit** : quantile bas de la surface LiDAR dans une bande de
   0,4 à 1,6 m à l'intérieur du contour, limité pour qu'aucun toit ne dépasse
   environ 45° sur le petit côté.
4. **Toit** : faîtages découpés à l'emprise et vérifiés par le LiDAR (écartés
   au-delà de 2,5 m d'écart : toits démolis ou refaits), puis triangulation
   contrainte contour + faîtages : pignons et croupes en sortent seuls. Toit
   plat là où le LiDAR est plat ; sinon grille LiDAR de 3 m.
5. **Murs** : chaque mur est une façade *sur rue* (une voie à moins de sa
   demi-largeur + 7 m devant), *sur cour*, ou un *mur mitoyen* (un autre
   bâtiment 0,6 m derrière : sans ouverture, comme en vrai).
6. **Typologie** (règles, à confronter aux photos) :
   * `haussmann` : mur ≥ 14 m sur une rue Second Empire (République,
     Édouard-Herriot, Bourse, Jacobins, Cordeliers, Carnot...) : rez-de-chaussée
     4,2 m + entresol 2,6 m, étages 3,3 m, travées 3,3 m, balcons aux 2e et
     5e étages (bible), pierre crème, toits zinc ;
   * `ancien` : rez-de-chaussée 3,8 m, étages 3 m, travées 2,8 m, enduits
     lyonnais (ocre, saumon, jaune paille, ocre rouge), tuiles ;
   * `moderne` : murs en béton selon la BD TOPO ; `monument` : églises (faites à la main plus tard).
7. **Travées et modules** : les étages sont étirés pour finir pile à l'égout ;
   chaque façade sur rue ou cour est découpée en travées entières. Types :
   `shop`, `shopfront_closed`, `entrance` (une par bâtiment, au milieu),
   `gf_window`, `entresol_window`, `window`, `window_balcony`, `window_small`
   (dernier étage), `court_window`, `monument_bay`, `blank` (murs < 1,8 m).
8. **Enseignes** : chaque commerce OSM du bâtiment va à la travée sur rue la
   plus proche (deux travées si la façade en compte six ou plus). `name`,
   `brand` et `brand:wikidata` sont recopiés sur le module : c'est la clé qui
   permet à l'acteur d'enseigne de choisir le vrai logo dans `Plugins/CityBrands/` (hors Git).

## Sorties

* `facade_modules.csv` : une ligne par module : bâtiment, typologie, côté, rue,
  type, étage, travée, position (mètres locaux et cm Unreal), lacet Unreal
  (vers l'extérieur), largeur, hauteur, couleur, nom/marque/Wikidata de
  l'enseigne. C'est l'entrée du kit de façades
  (`Content/Environment/Facades/<Kit>/`, grille de 50 cm) et du graphe PCG qui l'instanciera.
* `greybox.glb` : un nœud par volume (`SM_Building_<id>`), pivot au centroïde
  de l'emprise au sol, couleurs par sommet, axes glTF X est, Y haut, Z sud
  (même convention que la scène témoin, déjà validée dans UE).
* `buildings_gen.csv` : typologie, niveaux, type de toit, hauteurs, contrôles
  LiDAR, position Unreal et emprise attendue de chaque maillage.
* `terrain/heightmap.png` 2017 × 2017, pas de 0,70 m, réglages Landscape dans
  `terrain.json` (échelle 69,64 / 69,64 / 17,22, pas vertical 1,3 mm).

## Mesures (rue de la République, 2026-11-09, machine cloud)

| Mesure | Valeur |
|---|---|
| Bâtiments BD TOPO / volumes générés | 175 / 281 (106 socles de cours couvertes) |
| Toits | 142 avec faîtages vérifiés LiDAR, 118 plats, 21 grille LiDAR |
| Toit vs LiDAR, 169 742 cellules de 0,5 m | médiane 0,39 m, p90 2,23 m (lucarnes, cheminées, verrières non modélisées) |
| Hauteur BD TOPO vs LiDAR (toute la zone) | médiane 0,85 m, p90 3,8 m |
| Typologies | 70 haussmann, 211 ancien (socles compris) |
| Modules de façade | 13 182, dont 215 commerces avec enseigne (106 marques) |
| Greybox | 288 k triangles, GLB 28 Mo (faces plates, non indexé) |
| Durée | 16 s génération, 30 s terrain, environ 1 min par aperçu |
| Zone entière (`--focus all`, 1,4 × 1,4 km) | 2 601 bâtiments → 3 617 volumes, 205 k modules, 2 486 enseignes (499 marques), 3,8 M triangles, toit vs LiDAR médiane 0,52 m / p90 3,2 m, 3 min 44 s |

La greybox sert à vérifier la mise en page, ce n'est pas un asset : les façades
finales sont des modules de kit (≤ 50 k triangles chacun, ADR-0002), instanciés ;
c'est la liste des modules qui est conservée.

## Limites et suite

1. **Test éditeur** : `tools/city/unreal/import_city.py` importe greybox et
   terrain, vérifie chaque maillage par rapport à son emprise attendue avant de
   placer quoi que ce soit, et construit `/Game/Maps/L_City_RueDeLaRepublique`.
   Pas encore lancé : l'éditeur est occupé (rendu puis import Tripo) ; il faut un créneau.
2. Les règles de typologie sont des heuristiques ; Opéra, Hôtel de Ville,
   Palais de la Bourse et églises demandent des monuments faits main (0.14+).
3. Kit de façades : modéliser les modules des deux familles principales
   (pierre haussmannienne, enduit lyonnais) dans Blender, puis un graphe PCG
   qui lit `facade_modules.csv`.
4. Voirie, trottoirs et bordures depuis les lignes de bordure Grand Lyon ;
   mobilier depuis les tables Grand Lyon (avec vrais modèles) plutôt qu'OSM
   quand les deux existent.
5. Quelques faîtages 2012 encore validés par le LiDAR donnent des pignons
   raides sur des bâtiments étroits ; à vérifier sur photos de rue.
