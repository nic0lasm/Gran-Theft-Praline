# Reproduire Lyon fidèlement : sources et pipeline

*English version: [plan.md](plan.md)*
*Rédigé le 2026-11-09. Entrées : [concept](../../design/gdd/game-concept.fr.md), [roadmap](../../production/roadmap.fr.md) (versions 0.7, 0.12, 0.14), [recensement des assets](../sourcing/asset-sourcing.fr.md).*
*Code : [`tools/city/`](../../tools/city/). Les données vont dans `assets/geo/`, elles sont régénérées et non versionnées.*

## Décision en un paragraphe

On **génère la ville nous-mêmes à partir de données ouvertes** : emprises,
hauteurs, routes, eau, végétation et positions des commerces viennent de la
BD TOPO de l'IGN et d'OpenStreetMap, deviennent des tables et du GeoJSON en
mètres locaux, sont importés dans Unreal par un script d'éditeur, puis chaque
bâtiment est construit par un kit de façades procédural (PCG) et habillé avec
des assets Tripo et les vraies enseignes. On ne diffuse **pas** un maillage de
photogrammétrie (Google 3D Tiles / Cesium) comme monde du jeu : il n'est pas
modifiable, n'a pas d'intérieurs, ne sert pas de géométrie de jeu (collisions,
éclairage) et les conditions de Google interdisent son extraction. Cesium for
Unreal reste utile comme *calque de référence* dans l'éditeur pour contrôler
notre résultat.

## Comparatif des sources

| Source | Apporte | Licence | Verdict |
|---|---|---|---|
| IGN **BD TOPO V3** (WFS `data.geopf.fr`) | 2,6 k emprises de bâtiments avec hauteur, étages, usages, matériaux, altitudes sur la zone test ; routes avec largeur, voies, sens, vitesse ; végétation ; eau ; ouvrages ; ERP | Licence Ouverte Etalab 2.0 (crédit IGN) | **Socle.** Testé, sans clé |
| **OpenStreetMap** (API standard, par tuiles) | Commerces et restaurants avec `name`, `brand`, `brand:wikidata`, horaires, adresses ; arbres ; lampadaires, bancs, poubelles, arrêts de bus, bouches d'incendie, feux | ODbL 1.0 (crédit, partage à l'identique de la base) | **Socle pour enseignes et mobilier.** Testé. Overpass n'est pas joignable depuis notre cloud, l'API marche pour des zones de la taille d'un quartier |
| IGN **LiDAR HD** (MNT / MNS) | Modèle de terrain et de surface 0,5 m, formes de toits, hauteurs d'arbres | Licence Ouverte Etalab 2.0 | **Utilisé** (étape 2) : terrain, égouts, toits, cours. Testé, sans clé ([génération](generation.fr.md)) |
| **Données ouvertes Grand Lyon** (data.grandlyon.com : maquette 3D, imagerie, arbres, mobilier) | Arbres d'alignement, faîtages 3D, bordures, corbeilles, arceaux, abris, bancs ; maquette CityGML 2012 | Licence Ouverte (bancs et fontaines : ODbL, repris d'OSM) | **Utilisé** (étape 2) : faîtages, arbres, mobilier. CityGML 2012 non utilisée ([génération](generation.fr.md)) |
| **Google Photorealistic 3D Tiles** via Cesium for Unreal | Maillage photogrammétrique texturé | Conditions Google Maps Platform : en ligne seulement, attribution, pas d'extraction ni d'usage hors ligne | **Référence seulement**, jamais livré |
| **Photogrammétrie** maison (Apple Object Capture) | Façades et détails de quelques monuments | À nous | Plus tard, pour une poignée de monuments (0.14+) |

## Pipeline

```
fetch.py  ->  assets/geo/raw/<zone>/        GeoJSON BD TOPO (Lambert-93) + tuiles OSM XML
build.py  ->  assets/geo/processed/<zone>/  buildings.csv|geojson, shops.csv, trees.csv,
                                            furniture.csv, roads/vegetation/water.geojson,
                                            manifest.json
generate.py / terrain.py -> gen/, terrain/  toits, cours, modules de façade, greybox, heightmap
(éditeur) ->  unreal/import_city.py         niveau greybox ; kit de façades / PCG lit facade_modules.csv
```

* Coordonnées : mètres Lambert-93 recentrés sur la zone, `x` est, `y` nord.
  Unreal : `X = est`, `Y = sud`, `Z = haut`, en cm ; les colonnes
  `ue_x/ue_y/ue_z` sont déjà converties (`Z` relatif à l'altitude médiane du sol).
  Notre projection colle à la sortie EPSG:2154 de l'IGN à moins de 5 cm.
* Hauteurs : `hauteur` BD TOPO, sinon toit moins sol, sinon étages × 3 m,
  sinon 15 m (colonne `height_source`). Sur la zone test, 2 596 bâtiments sur 2 601 ont la hauteur IGN.
* Commerces : chaque commerce OSM nommé est rattaché à son bâtiment (dans
  l'emprise, sinon le plus proche à moins de 15 m) : c'est la liste d'enseignes par façade.

## Zone test : Presqu'île (bbox `45.7570,4.8250,45.7690,4.8420`)

Commande : `python3 tools/city/fetch.py --name presqu-ile --bbox 45.7570,4.8250,45.7690,4.8420`
puis `python3 tools/city/build.py --name presqu-ile`. Environ une minute, sans clé.
La boîte couvre Bellecour → Terreaux, les deux fleuves et une partie des quais.

| Sortie | Nombre |
|---|---|
| Bâtiments | 2 601 |
| Commerces / lieux nommés | 2 692, dont 521 avec une marque et 2 615 rattachés à un bâtiment |
| Arbres | 1 466 |
| Mobilier urbain | 3 604 |
| Tronçons de route | 1 194 |
| Zones de végétation / surfaces d'eau | 74 / 3 |

Les trois McDonald's de la zone sont retrouvés avec marque, horaires et
bâtiment, dont celui du côté Bellecour de la rue de la République.

## Enseignes réelles

Décision de Nico (2026-11-09) : usage non commercial, donc enseignes et marques
réelles reproduites le plus fidèlement possible, sans masquage. `shops.csv`
porte `name`, `brand`, `brand_wikidata`, `cuisine`, `opening_hours` et
l'identifiant du bâtiment : un acteur d'enseigne choisit le bon logo et la
bonne typographie par façade. Risque à garder en tête (une phrase) : les logos
sont des marques déposées que la licence MIT ne couvre pas ; il vaut mieux garder
les fichiers de logos hors de Git (stockage local, comme les masters Tripo) et
afficher l'intention non commerciale dans le README.

## Gestion des licences

* Données IGN : crédit « IGN, BD TOPO » dans `assets/SOURCES.md` et aux crédits du jeu.
* OSM est sous ODbL : on ne versionne **pas** les tables dérivées (seulement le code et
  le manifeste) ; chacun les régénère. Crédit « © les contributeurs d'OpenStreetMap ».
* `assets/geo/raw/` est déjà ignoré par Git ; `processed/` est régénéré et ne
  doit pas être commité, hors `manifest.json`.

## Limites de fidélité et suite

1. La couverture OSM est inégale (19 % des lieux ont une marque) : on complète les
   commerces clés à la main avec des photos de rue.
2. La BD TOPO n'a aucun détail de façade : rythme des fenêtres, balcons et
   rez-de-chaussée viennent du kit de façades, alimenté par `usage_*`, étages, matériau et photos.
3. Fait à l'étape 2 ([génération](generation.fr.md)) : terrain et toits LiDAR HD, façades, script d'import de la greybox.
   Reste : ponts et quais (`construction_*` BD TOPO), voies et feux.
4. Tests : `python3 -I -m unittest discover -s tools/city/tests` (job CI « City tools tests »).
