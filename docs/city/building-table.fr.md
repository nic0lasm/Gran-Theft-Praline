# Table des bâtiments de Lyon (ajoutée le 2026-11-13)

Une ligne par bâtiment réel, de la tranche République (Bellecour → Jacobins) jusqu'à l'horizon
(2,5 km autour de l'axe de la rue), avec ce qu'il faut à tout générateur de bâtiments : notre kit
de façades, les primitives de bâtiment PCG (`Assign_CitySample_Buildings` de City Sample lit une
spline d'emprise et `@data.MaxHeight`), ou un LLM qui pilote l'éditeur par Unreal MCP.

```
python3 tools/city/far_decor.py fetch --name presqu-ile        # zips CityGML (2 à 4 Go chacun), une fois
python3 tools/city/far_decor.py prep  --name presqu-ile        # -> citygml/cache, les zips peuvent partir
python3 tools/city/building_table.py fetch --name presqu-ile   # export BDNB du Rhône (560 Mo), une fois
python3 tools/city/building_table.py build --name presqu-ile --focus rue-de-la-republique
```

Sortie (hors Git) : `assets/geo/processed/<zone>/buildings_lyon/`
`buildings_lyon.geojson` (géométrie = emprise), `buildings_lyon.csv` (mêmes champs plus
`footprint_wkt`), `summary.json` (comptes par type de toit, style, source).

## Coordonnées

Mètres locaux, x vers l'est, y vers le nord, depuis l'origine de la zone en Lambert-93
(EPSG:2154, dans le bloc `metadata` du fichier) ; hauteurs en mètres au-dessus de l'altitude
d'origine (NGF-IGN69). Monde Unreal de la carte de la rue : X = x·100, Y = −y·100, Z = z·100 (cm).
Le GeoJSON n'est donc pas en WGS84 : lire le bloc `metadata`.

## Champs

| Champ | Sens |
| ---- | ---- |
| `id` | identifiant CityGML (cadastral pour le bâti courant, UUID pour les remarquables) ou BD TOPO |
| `source` | `maquette-2018` (maquette de la Métropole) ou `bdtopo` (bâtiment de la zone absent de la maquette) |
| `arrondissement`, `remarkable` | zip d'origine ; édifice remarquable (basilique, églises… : garder son maillage) |
| `centroid_x_m`, `centroid_y_m`, `dist_axis_m`, `footprint_m2` | position, distance à l'axe de la rue de la République, surface au sol |
| `ground_z_m`, `eave_z_m`, `ridge_z_m` | point le plus bas, égout (médiane du point le plus bas de chaque pan de toit, pondérée par sa surface), point le plus haut (maquette 2018) |
| `height_eave_m`, `height_ridge_m` | hauteur des murs à l'égout (pour `MaxHeight`) et hauteur totale |
| `roof_type`, `roof_pitch_deg` | `flat` (plat), `pitched` (à pans), `mansard`, `mixed`, `unknown`, d'après les pentes des pans de toit de la maquette (lignes BD TOPO : toit de la carte de la rue, sinon `unknown`) ; pente médiane |
| `storeys`, `storeys_source` | niveaux hors sol : `carte-de-la-rue` (notre génération), `bdnb`, `bdtopo` ou `estime-hauteur` |
| `style`, `style_source` | `haussmann`, `ancien` (avant 1853), `xixe` (1853-1914 hors des rues haussmanniennes), `entre-deux-guerres`, `moderne`, `monument`, `inconnu` ; d'après la carte de la rue, l'année BDNB, ou un toit plat |
| `year_built`, `era` | année de construction BDNB et sa période (`avant-1800` … `depuis-1976`) |
| `use`, `wall_material`, `roof_material` | BDNB (FFO) ou BD TOPO |
| `ground_floor_commercial`, `ground_floor_shops`, `shop_names` | commerces OpenStreetMap à moins de 3 m de l'emprise (zone seulement, vide au-delà) |
| `in_slice`, `in_street_map`, `street_map_roof` | dans le cadre de la tranche ; déjà généré par la carte de la rue, avec son type de toit |
| `bdtopo_id`, `bdnb_id` | liens vers BD TOPO et BDNB |

## Mesures (13/11/2026, 7 arrondissements)

13 805 lignes : 13 169 de la maquette 2018, 636 de BD TOPO ; 13 534 reliées à la BDNB, 12 571
avec une année. Toits : 10 201 à pans, 1 953 plats, 866 mixtes, 183 mansardés, 602 inconnus.
Tranche : 283 lignes, 201 avec commerce au rez-de-chaussée, 258 avec une année. Face au LiDAR de
la carte de la rue (178 bâtiments communs), le faîtage est à 0,07 m et l'égout à 0,7 m (écarts
absolus médians).

## Sources et limites

- Métropole de Lyon, maquette 3D texturée 2018 (CityGML LOD2), Licence Ouverte v2 : emprises,
  hauteurs, toits. Les bâtiments construits depuis 2018 n'y sont pas ; dans la zone ils viennent
  de BD TOPO (`source = bdtopo`, hauteur BD TOPO, pas de toit).
- BDNB open (CSTB), millésime 2026-02-b, Licence Ouverte Etalab 2.0 : année, niveaux, matériaux,
  usage. Les tables de propriétaires de l'export ne sont jamais lues.
- BD TOPO et LiDAR HD (IGN), OpenStreetMap (ODbL) pour les commerces, notre carte de la rue pour
  la tranche.
- L'emprise est l'union des pans de toit et de sol de la maquette vus du ciel ; une cour
  intérieure reste un trou dans le polygone GeoJSON (une spline prend le contour extérieur).
- Les ponts sont exclus (ce ne sont pas des bâtiments). Les édifices remarquables sont des lignes
  comme les autres mais gagnent à rester des maillages (décor lointain).
