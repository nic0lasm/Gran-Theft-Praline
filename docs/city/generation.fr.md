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
unreal/run_city_import.sh (Mac) -> import_city.py, place_facades.py, puis une partie -game : captures + temps d'image
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

## Kit de façades v0 et placement (ajouté le 2026-11-09)

Code : [`tools/city/facade_kit/`](../../tools/city/facade_kit/) (Blender 4.2+, appli ou module `bpy`)
et [`tools/city/unreal/place_facades.py`](../../tools/city/unreal/place_facades.py).

* **18 modules**, une travée × un étage, construits procéduralement dans
  Blender d'après la bible (section 3.1) : *haussmannien* (pierre de taille,
  socle gris, entresol à refends, portes-fenêtres à chambranles, entablements
  et garde-corps, balcons filants sur consoles, portes cochères à imposte,
  corniche à modillons, devantures alu à bandeau) et *lyonnais* (enduit
  coloré, encadrements et appuis en pierre dorée, volets persiennés, fenêtres
  barreaudées en rez-de-chaussée, devantures bois, génoise). 6,5 k triangles
  pour tout le kit (1 k pour le plus gros module) : très loin du budget M1 de
  50 k par module ; le détail viendra des trim sheets et des textures
  Megascans/photos (les emplacements `MI_*` suivent déjà la bible).
* `assemble.py` exporte `facade_kit.glb` + `kit_manifest.json` et assemble la
  rue depuis `facade_modules.csv` avec la même règle que dans Unreal : module
  à sa place, face vers l'extérieur, largeur et hauteur étirées à la travée,
  corniche/génoise sur chaque travée sur rue du dernier étage, **lettres en
  relief au vrai nom du commerce** sur chaque bandeau. Rendus Cycles de la
  rue de la République : 3 895 modules et 62 enseignes dans un rayon de
  160 m, environ 2 min par image 1600 × 900 sur 4 cœurs cloud.
* `place_facades.py` (éditeur, après `import_city.py`) : importe le kit dans
  `Content/Environment/Facades/LyonKit/`, vérifie la taille de chaque module
  et le côté extérieur (d'après l'emprise du balcon) avant de placer, puis un
  acteur par type de module avec un composant d'instances hiérarchiques
  (13 k instances pour la rue), la couleur d'enduit du bâtiment en données
  par instance, et les enseignes : le vrai logo si
  `/CityBrands/Logos/T_Brand_<QID Wikidata>` existe (hors Git), sinon un
  texte au vrai nom. `import_city.py` place maintenant les **coques** (toits,
  murs mitoyens) pour que le kit ne soit pas doublé.
* Pas encore fait : un graphe PCG n'est pas nécessaire pour ce placement (la
  liste des modules est déjà calculée) ; il vaudra la peine pour le streaming
  (partition PCG) et la variation en jeu, après le premier test éditeur.
  Suite : trim sheets et textures Megascans/photos, `M_Master_Facade` qui lit
  la couleur par instance, modules d'angle, lucarnes et brisis haussmannien.

## Ce que le joueur voit d'abord : devantures et enseignes réelles (ajouté le 2026-11-09)

Priorité fixée en mesurant ce qui donne sur la rue de la République :
2 801 modules côté rue ; à hauteur d'yeux, 672 travées de rez-de-chaussée sans
commerce connu et 185 travées de vrais commerces, puis les fenêtres
haussmanniennes (2 698) et les balcons (1 846). Cette passe ne traite donc
que ceux-là, pas le kit entier.

* **Devantures** (`kit.py`) : la travée commerciale haussmannienne a trois
  dessins (cadre aluminium, devanture en applique en bois peint devant les
  piédroits, vitrage sans cadre à cornière acier) et un rideau métallique
  baissé ; chaque devanture a une pièce éclairée derrière un verre
  transparent (fond, sol, plafond, deux tables de présentation) : les
  vitrines se lisent comme des boutiques, plus comme des miroirs noirs. Le
  bandeau d'enseigne fait 60 cm (35 avant). Les travées sans commerce connu
  prennent les mêmes dessins avec un bandeau nu ; un immeuble sur huit a ses
  rideaux baissés. Les boutiques à enduit lyonnais ont aussi la pièce éclairée.
* **Encadrements de fenêtres** : trois dessins haussmanniens (entablement,
  fronton triangulaire, corniche sur consoles avec clé), pour les étages
  courants et à balcon. Fenêtres lyonnaises à enduit (deuxième module le plus
  fréquent côté rue) : volets rabattus, garde-corps en fer forgé sans volets,
  ou un volet fermé.
* **Variantes par immeuble** (`kit_layout.module_for`, hachage stable de
  l'identifiant) : une façade garde un seul dessin, les voisines diffèrent, et
  Blender comme Unreal choisissent la même.
* **Enseignes** (`brands.py`, `signs.py`) : pour chaque marque, Wikidata donne
  le nom, la couleur officielle et le fichier du logo ; le logo vient de
  Commons (vignettes 960 px, requêtes espacées : Commons freine les scripts).
  `signs.py` dessine une texture 2048 × 464 par commerce avec son vrai nom
  (nom de marque si la marque a un identifiant Wikidata, sinon le nom propre
  du commerce), fond à la couleur de la marque, sinon couleur dominante du
  logo, sinon palette par métier ; lettres blanches ou presque noires selon le
  meilleur contraste (ratio WCAG vérifié, 4,3 au minimum). Si un logo existe,
  une seconde texture le pose sur le bandeau. Distributeurs, consignes à
  colis, œuvres d'art et parkings n'ont pas de bandeau (13 sur 215).
* **Placement** : un panneau d'enseigne (`SM_Sign_Panel`) sur le bandeau de
  chaque travée du commerce, à la hauteur et à la profondeur de la variante
  posée. Dans Unreal, `place_facades.py` crée `M_Sign_Fascia` et une instance
  de matériau par enseigne ; les textures de lettrage vont sous
  `/Game/Environment/City/Signs/`, celles des logos seulement sous
  `/CityBrands/Signs/` quand le plugin hors Git est monté (aucun logo de
  marque dans Git).

Commandes : `python3 tools/city/brands.py --name presqu-ile --focus rue-de-la-republique`,
puis `python3 tools/city/signs.py --name presqu-ile`, puis `assemble.py`
(`--logos` pour des rendus locaux avec les logos).

## Map de test du matin et mobilier urbain (ajouté le 2026-11-09)

* `import_city.py` donne maintenant à la map de la ville **l'éclairage de la
  bible** issu de la scène de référence (`GTP_CITY_LIGHT` = Noon par défaut,
  GoldenHour, Rain ou Night : la map de référence est copiée et tout ce qui
  n'est pas lumière, ciel, brouillard ou post-traitement est retiré), des
  **collisions complexes** sur bâtiments et terrain (le personnage marche sur
  le sol LiDAR et bute sur les murs), un kill-Z, et le **PlayerStart côté
  Bellecour** de la rue de la République, 8 m dans la rue, regard vers le nord.
* `props.py` pose le mobilier de la vague 1 **uniquement à des positions
  relevées** : corbeilles Grand Lyon (cerceaux à sac transparent quand le
  registre le dit), arceaux vélo (un par arceau, seulement ceux construits),
  abribus et grilles d'arbres ; bancs, lampadaires, bornes incendie, boîtes
  aux lettres, fontaines, bornes Vélo'v et colonnes à verre d'OSM ; terrasses
  devant les 16 cafés et restaurants de la rue (tables, chaises, parasol,
  ardoise). 732 objets pour la rue, plus 362 détails de la vague 14 sur les
  mêmes ancrages réels (troncs de platanes sur les platanes du Grand Lyon,
  croix de pharmacie, mannequins des boutiques de vêtements, lauriers aux
  portes des cafés, chauffages et jardinières de terrasse, interphones et
  seuils aux entrées, consignes à colis), retrouvés dans l'éditeur par leur
  étiquette CatalogId une fois importés : 1 094 lignes au total.
  `unreal/place_props.py` les instancie (un
  composant instancié par asset, collisions actives), pose chacun au sol par
  un lancer de rayon et liste les assets pas encore importés.
* `unreal/run_city_import.sh` enchaîne import, façades, mobilier, puis parcourt
  la rue en `-game` avec le profil de rendu m1 à 5 m/s de Bellecour au bout de
  la rue, en relevant chaque temps d'image (p50, p95, p99, saccades de plus de
  100 ms) et cinq captures.

## Sol et coques : matériaux de la bible (ajouté le 2026-11-10)

`tools/city/ground_mask.py` dessine, à partir des axes de voies BD TOPO, un masque du sol
(2048 × 8192, ~15 cm par texel, hors Git) : chaussée en asphalte (largeur réelle), bordures en
granit, pavés de granit sur les rues fermées aux voitures (vitesse nulle : rue de la République,
sentiers, escaliers), lignes axiales pointillées sur les voies à double sens, dalles de calcaire
partout ailleurs. `tools/city/unreal/apply_city_materials.py` crée `M_City_Ground` (masque lu
en coordonnées monde, joints de dalles et de pavés et variation de teinte procéduraux, une seule
texture) et `M_City_Shell` (couleur du bâtiment écrite par le générateur dans les couleurs de
sommets, assises de pierre sur les murs, toits moins rugueux), les assigne aux tuiles de terrain
et aux coques, puis vérifie les instances de façade de la map. Il remplace `M_CityGreybox` sans
reconstruire la map.

Vieillissement, tout en calcul dans le shader (aucune texture, peu coûteux en profil M1) : le
kit de façades (`M_City_Kit`) et les murs des coques reçoivent une lente dérive de teinte sur
l'îlot, un soubassement plus sombre sur les 1 à 4 premiers mètres au-dessus du niveau de rue
du bâtiment (écrit par `place_facades.py` dans la donnée d'instance 3) et des coulures sous
chaque corniche et appui, dosés par emplacement avec `Weathering` (pierre 1, bois 0,5, verre
0,1-0,15, intérieurs de boutique 0). L'asphalte reçoit des reprises (tranchées, passages de
roues). Le masque est dessiné sur des texels carrés puis rééchantillonné : les rues gardent
leur largeur quel que soit le format de la texture.

### Teinte pierre de Lyon des façades (ajouté le 2026-11-23)

Le générateur donne à chaque bâtiment une couleur de sa typologie (`procgen.PALETTE`) : jaunes
paille, roses saumon et orangés brique à pleine saturation, certains plus clairs qu'aucun mur.
Côte à côte sur un quai, ils font une rangée de jouets peints (vidéo de la porte 69), alors que
les photos de référence de la tranche montrent du calcaire crème rue de la République et sur les
quais, des enduits pâles dans les rues voisines. `tools/city/facade_tone.py` corrige la couleur
dans les matériaux, donc les tables générées et la map restent telles quelles : la teinte est
tirée presque entièrement vers le crème du calcaire (0,3 de la teinte du générateur est gardé ; 0,5 jusqu'à la porte 77, dont la vidéo montrait encore des jaunes peints sur le quai) (les bâtiments diffèrent encore, comme les enduits
d'une même ville) et la luminance au-dessus de 0,38 est divisée par deux, si bien qu'aucun mur
n'est plus clair qu'un calcaire clair (bible artistique 4.2 : 150 à 210 en sRGB 8 bits, 110 à
200 pour les enduits) et que les plus clairs restent les plus clairs.
`apply_city_materials.py` construit les mêmes sommes en nœuds de matériau (`lyon_tone`) sur
les murs de `M_City_Shell` et de `M_City_Backdrop` (pas les toits) et sur la couleur de
bâtiment de `M_City_Kit` ; `tools/city/tests/test_facade_tone.py` tient chaque couleur de la
palette dans les plages de la bible. `materials_report.json` donne `facade_tone`.
`GTP_FACADE_TONE=0` rend les couleurs du générateur. Pas encore vu en jeu : la première porte
qui suit le montrera.

### Fenêtres en retrait dans les murs de fond, et aperçu des façades (ajouté le 23/11/2026)

Les immeubles hors du parcours sont en `M_City_Backdrop` : un polygone plat par mur, la façade
dessinée par un bloc de HLSL (`BACKDROP_HLSL` dans `apply_city_materials.py`). Des fenêtres
peintes au nu du mur faisaient lire les rues de la fuite comme des boîtes (vidéo de la porte
69). Chaque ouverture est maintenant reculée par parallaxe : le rayon de l'œil est suivi
derrière le plan du mur jusqu'à la profondeur de la menuiserie (18 cm pour une fenêtre, 4 cm
pour des persiennes fermées, 12 cm pour le vitrage d'une boutique, 25 cm pour une porte
cochère), et le pixel montre ce qu'il y rencontre, la vitre ou le tableau (sous-face à l'ombre,
appui à la lumière, jambages entre les deux). Derrière le vitrage d'une boutique, le mur du
fond est à 3 m, avec des rayonnages, des murs de côté, un sol et un plafond éclairé, et glisse
avec l'œil. Le glissement s'efface avec le même facteur de distance que le motif, donc rien ne
scintille au loin. Une dizaine d'opérations de plus par pixel, aucune texture.

`tools/city/facade_preview.py` rend ce même texte HLSL sans Unreal : compilé en C++ avec une
petite cale (`tools/city/facade_preview/hlsl_shim.h`), une rue vue du trottoir sous un soleil et
un ciel simples. Une faute de frappe ou un nom non déclaré dans le shader échoue dans la CI
(`tools/city/tests/test_facade_preview.py`) et non dans l'éditeur du PC de référence, et un
changement de façade se regarde avant une porte :

```
python tools/city/facade_preview.py --out facade.ppm [--source <un autre apply_city_materials.py>]
```

L'aperçu ne montre que la couleur du shader : ni détail scanné, ni éclairage du moteur, ni
vieillissement. Pas encore vu en jeu.

### Détail d'enduit sur les immeubles colorés (ajouté le 23/11/2026)

Tous les murs prenaient le détail de la pierre de taille, donc un immeuble enduit couleur pêche
montrait des blocs de pierre. L'entrée `render_ochre` du catalogue a maintenant un secours CC0
(Poly Haven `yellow_plaster`, un enduit taloché ; seul son détail sert, chaque immeuble garde
sa couleur), et `gtp_surfaces.shell` choisit entre pierre et enduit d'après la saturation de la
couleur du bâtiment (`facade_tone.render_share` : pierre sous 0,24, enduit au-dessus de 0,46,
mélange entre les deux ; les couleurs haussmanniennes, de monument et modernes de la palette
restent en pierre, les couleurs franches des rues anciennes passent en enduit). Sur le kit, la
façade enduite le prend comme avant. Sans l'enduit dans le projet, rien ne change.
`materials_report.json` liste `kit.render` et `shell.render` dans `surfaces.used` quand il y
est.

### Le lointain : horizon, rive d'en face et façades éloignées (ajouté le 24/11/2026)

Vidéo de la porte 76, à 60 s sur le quai : de l'autre côté du fleuve, le sol était une plaine
blanche et nue, les volumes lointains des blocs beiges lisses, et la vue finissait sur une bande
bleu sombre sous le ciel, comme une mer. Quatre causes, quatre changements.

1. **Rien au-delà de la carte.** Passé les volumes BD TOPO (600 m de l'axe de la rue), le monde
   était vide, donc le ciel se voyait sous l'horizon. `tools/city/horizon.py` compose un
   lointain sans aucune donnée géographique : un sol jusqu'à l'horizon réel (il prend la brume
   comme tout sol), quatre rangs de blocs simples de 720 m à 1,5 km de l'axe (cinq à huit
   étages, une rue entre deux blocs), les tours de la Part-Dieu à l'est, et deux dômes aplatis
   pour Fourvière (ouest, boisée) et la Croix-Rousse (nord, bâtie), les blocs qui tombent
   dessus étant posés sur leurs pentes. `tools/city/unreal/place_horizon.py` le place dans le
   dossier City/Horizon avec les seules formes de base du moteur (environ 240 acteurs de
   12 triangles, regroupés par matériau ; ni collision, ni ombre, hors de la scène de lancer de
   rayons), 1 m sous le terrain le plus bas. C'est l'étape `horizon` de `route_dressing.py`,
   après `materials` ; elle sauvegarde la carte ; `GTP_HORIZON=0` l'enlève. Rapport :
   `horizon_report.json`.
2. **Les façades lointaines devenaient lisses.** Le shader de fond effaçait tout son motif vers
   une moyenne dès que les traits fins (cadres, barreaux) approchaient du pixel : à partir de
   350 m de face, de 60 m le long d'une rue. Les traits fins partent toujours en premier, mais
   les ouvertures restent des rectangles sombres jusqu'à faire elles-mêmes environ un pixel.
   Les blocs lointains prennent la même façade (`M_City_Horizon_<n>`, un par couleur de mur,
   teintée comme le reste).
3. **Sol blanc au-delà du masque de sol.** La texture du masque est bornée, donc passé son bord
   ses derniers texels étaient étirés et le sol était en dalles claires de trottoir jusqu'au
   bout du terrain. `far_ground` fond le sol, sur 30 m hors du masque, vers le ton sombre
   unique du lointain. `GTP_FAR_GROUND=0` le coupe. `materials_report.json` donne `far_ground`
   et `horizon_materials`.
4. **Du moellon sur les murs.** Le scan derrière `limestone_ashlar` est un mur romain brut : sur
   un mur au soleil de la fuite, il se lisait comme une forteresse (43 s). Une nouvelle entrée
   `dressed_stone` du catalogue (Poly Haven `sandstone_blocks_08`, CC0 : blocs lisses, joints
   fins) passe en premier pour le détail des murs ; sans elle dans le projet, l'ancien scan
   reste.

5. **Des façades en couleur de toit.** Les toits ajustés au LiDAR des volumes de fond ont des
   faces verticales, peintes comme le toit : des façades entières du quai étaient rouge brique
   (tuiles) ou bleu-gris (zinc) à 9 s, et les pentes raides (mansardes) prenaient les fenêtres
   de la façade dans la couleur du toit. `M_City_Backdrop` reconnaît maintenant une couleur de
   toit à sa valeur (`facade_tone.ROOF_ON_WALLS`) : sur une face verticale elle devient pierre,
   sur une pente elle reste un toit et sort du masque de mur. `GTP_ROOF_WALLS=0` le coupe ;
   `materials_report.json` donne `roof_walls`.

Depuis la porte 86 (prise longue, 120 s : au bout du quai les blocs lointains étaient des boîtes
grises plates sous une seule ligne droite) chaque bloc lointain a un toit : un toit de tuiles
dont la dalle déborde du mur ou une mansarde de zinc en retrait, à deux hauteurs sur les blocs
larges, et des souches de cheminées (`horizon.roofs`, posés par `place_horizon.block_roofs`
en instances d'un seul cube, un acteur par matériau ; `GTP_HORIZON_ROOFS=0` les coupe ;
`horizon_report.json` donne `roofs`).

La porte 88 (prise longue, 13 s) les a montrés posés, et le lointain se lisait encore en blocs
gris : les toits étaient aussi clairs que les murs. Deux causes, corrigées dans les matériaux.
Les couleurs de toit du générateur arrivent au moteur telles qu'écrites et se lisent en pastel
(tuiles saumon, zinc gris pâle), et sur le PC tout toit de moins de 55 degrés prend l'unique
scan de toit de sa bibliothèque, un toit de métal peint teinté en zinc : aucun toit n'était
rouge. Les faces en pente de couleur toit prennent désormais la vraie teinte et la vraie
rugosité du toit (`facade_tone.ROOF_TRUE` : tuiles canal patinées 0,30 / 0,14 / 0,09, zinc
terni 0,20 / 0,215 / 0,235), et le scan ne garde que ses taches claires et sombres, posées sur
la teinte du zinc ou, là où le générateur dit tuiles et où la bibliothèque n'a pas de scan de
tuiles, sur la teinte des tuiles (`gtp_surfaces.shell`). `GTP_ROOF_TONES=0` coupe les deux ;
`materials_report.json` donne `roof_tones` et, sous `surfaces.used`, `shell.roof_tone`,
`shell.tile` et `shell.tile_tone`. La brume des temps Clear et Cloudy est aussi allégée
(densité 0,005 et 0,008, au lieu de 0,008 et 0,012) : à 700 m le bout du quai n'était qu'une
seule teinte pâle. Pas encore vu en jeu.

Les positions des collines et des tours sont approximatives exprès (un lointain, jamais un lieu
où aller). Pas encore vu en jeu.

### Place Bellecour : le sol rouge et Fourvière sur la ligne d'horizon (ajouté le 24/11/2026)

Bellecour est la vitrine de la ville, et rien dans la carte ne le disait : son sol était le
dallage clair de n'importe quel trottoir (passé le bord du masque de sol, le ton sombre du
lointain), et la colline derrière ne portait aucun monument.

1. **Le sol rouge.** `landmarks.py` enregistre déjà la place comme donnée (`place-bellecour`,
   revêtement `red_gravel`, contour OpenStreetMap). `tools/city/squares.py` le lit, ramène le
   contour à 12 sommets au plus (chacun se paie sur chaque pixel du sol) et écrit le texte
   d'une distance signée pour un nœud Custom de `M_City_Ground` (`square_ground` dans
   `apply_city_materials.py`) : sable stabilisé rouge, mat, nuancé sur 3 à 20 m, sans joints de
   dalles, derrière une bordure de pierre claire de 60 cm. Il est posé en coordonnées du
   monde, après le sol lointain, donc l'étendue du masque ne compte pas ; une chaussée que le
   masque connaît à l'intérieur du contour reste une chaussée. Le texte du shader est compilé
   en C++ dans les tests et comparé à la référence Python. `GTP_SQUARES=0` le coupe,
   `GTP_SQUARE_INSET_M=<m>` rentre le contour (si le contour OpenStreetMap va jusqu'aux
   façades) ; `materials_report.json` donne `squares` (surface, sommets, emprise, `laid`). Un
   échec de cette partie y est noté (`error`) et laisse le sol comme il était.
2. **Fourvière.** `horizon.py` place maintenant la basilique (nef, abside, quatre tours de
   48 m, clocher de l'ancienne chapelle) et la tour métallique à côté, à leurs coordonnées
   réelles, en huit formes du moteur et deux matériaux unis (`M_City_HorizonStone`,
   `M_City_HorizonIron`). Le dôme de la colline monte à 150 m pour que la terrasse de la
   basilique, sur son bord est, soit à ses 115 m réels au-dessus de la ville : depuis le côté
   est de la place elle passe au-dessus d'un toit de 25 m situé à 250 m. `horizon_report.json`
   donne `monuments` (8 attendus) et `monuments_skipped`.

3. **La place au-delà du bout de la carte.** Le sol de la carte est un rectangle qui s'arrête
   150 m avant le côté sud de la place : depuis la rue qui longe son côté nord (où passe le
   circuit de fuite) on voyait 30 à 65 m de sol, puis le bord du monde. Là où une place de
   `landmarks.json` dépasse le sol, `horizon.square_setting` compose, et l'étape `horizon`
   place dans City/Horizon : le sol prolongé au niveau de la place (médiane de traces sur la
   partie que la carte porte ; cubes d'1 m d'épaisseur avec `M_City_Ground`, qui dessine le
   rouge en coordonnées du monde, 2 m sous le bord du sol de la carte, 140 m autour de la
   place) ; les façades du tour de la place, à une rue du contour, en blocs simples de 21 à
   24,5 m avec la façade lyonnaise procédurale, seulement là où ils tiennent entièrement hors
   du sol et à l'écart de tout volume réel (dossiers City/Backdrop, City/Buildings,
   City/Landmarks) ; deux rangs de l'arbre feuillu du trajet le long du grand côté éloigné,
   aucun à moins de 5 m de la ligne que la fuite conduit (lue dans le fichier du circuit). Sol,
   façades et arbres arrêtent le joueur ; les façades portent ombre. Ce sont des doublures en
   attendant que la zone elle-même soit étendue au sud. `GTP_SQUARE_SETTING=0` les retire ;
   `horizon_report.json` donne `square_settings` (`level_cm`, `level_from`, `aprons`,
   `fronts`, `trees`) et `ground_box_m`. Le niveau de la place est la hauteur médiane du
   mobilier urbain posé dans son contour ou à moins de 60 m (`level_from` commence alors par
   `furniture`) : dans l'éditeur sans écran un tir de rayon traverse les tuiles du terrain et
   s'arrêterait sur le sol lointain, plusieurs mètres trop bas.

Pas encore vu en jeu. Le rouge est une couleur unie pour l'instant : une surface scannée pourra
suivre une fois le contour prouvé sur une image.

### Les quais : le sol hors du masque, les fleuves et les fosses (ajouté le 24/11/2026)

Porte 81, prise longue. La fuite part du quai Jules Courmont, 60 m à l'est du masque du sol
(il s'arrête 60 m après les immeubles générés) : ni chaussée ni bordure, une plaine grise striée
jusqu'à la rive d'en face, le Rhône dallé comme un trottoir. À 50 s la voiture volée a basculé
dans une rampe de parking.

1. **Masque lointain** (`tools/city/ground_mask.py --far`) : les mêmes canaux sur tout le
   terrain, 4096 texels de côté (34 cm), dessiné deux fois plus fin puis moyenné, écrit avec
   son json dans `assets/geo/processed/presqu-ile/ground_far/` (non versionné, comme le masque
   de la rue : `route_dressing.py` le fabrique avec les données de la machine avant son étape
   `materials`, et sans lui le sol reste comme avant). `M_City_Ground` lit le masque de la rue dans son étendue et le masque lointain au-delà ; le
   ton sombre du lointain commence alors à 150 m au lieu du bord. `GTP_FAR_MASK=0` revient au
   seul masque de la rue.
2. **Fleuves** : le terrain LiDAR est plat sur l'eau, 5,8 m sous les quais. `ground_far.json`
   porte un contour grossier du Rhône et de la Saône (10 et 22 coins) et leur niveau ; le
   matériau peint de l'eau (couleur et brillant de `M_City_Water`, à plat) à moins de 15 m d'un
   contour partout où le terrain est sous ce niveau plus 40 cm : la berge est là où le terrain
   tombe. `GTP_RIVERS=0` laisse le sol comme avant. Depuis la porte 93 l'eau est plus
   rugueuse (0,4 au lieu de 0,06) et gris-vert plutôt que noire : vue du quai sous deux degrés,
   le quasi-miroir ne montrait que la brume de l'horizon et le Rhône était une dalle pâle sous
   la rive d'en face. `GTP_WATER_ROUGHNESS=0.06` et `GTP_WATER_TONE=0` rendent l'ancienne eau ;
   `materials_report.json` donne `water`. Pas encore vu en jeu.
3. **Fosses** (`cover_pits.py`, étape `pits`) : de la porte 78 à la porte 81 l'étape tirait des
   rayons sur le terrain, n'obtenait aucune réponse dans l'éditeur sans écran et rendait
   « aucune fosse ». Elle lit maintenant les coins des tuiles du terrain
   (`tools/city/terrain_file.py` : dans `terrain/terrain.glb`, gardé seulement si ses tuiles
   ont les boîtes de celles de la carte, sinon dans les maillages des tuiles), couvre chaque
   fosse fermée avec des couvercles qui vont jusqu'aux coins secs autour, et échoue si aucune
   hauteur n'est lue. Sur le terrain du cache : 68 fosses, 258 couvercles, moins d'une seconde.
   À la porte 83 les tuiles de la carte portaient des coins hors du treillis de 4 m (256 hauteurs de plus qu'il n'en tient) : le plus
   petit écart donnait une grille de 2 m vide aux trois quarts, où chaque cellule s'écoule, et
   l'étape a lu 123 104 hauteurs sans trouver de fosse. Le pas est maintenant celui du treillis
   qui porte 95 % des coins (`terrain_file.grid`), les autres sont comptés (`off_grid`), et une
   grille vide à plus d'un cinquième est une erreur.

4. **La ligne conduite** (`clear_lane_instances.py`, étape `lanes`) : l'étape dégageait 1,5 m
   autour de l'axe des voies, mais la fuite roule sur la ligne du fichier de circuit
   (`line_ue_cm`, approche et tour), jusqu'à 3,9 m de cet axe : à la porte 83 la voiture a
   touché un arbre du quai à 96 km/h. Les mêmes jeux d'objets sont maintenant aussi dégagés à
   2,5 m de part et d'autre de chaque ligne de ce fichier (`GTP_CIRCUIT_CLEAR_M`) ; le rapport
   donne `circuit_lines`, `circuit_clear_cm`, `circuit_removed`, et une telle instance porte
   `"lane": "circuit"`.

   Les acteurs simples comptent comme les instances depuis la porte 86 (le kiosque octogonal de
   la rue de la République, un acteur à 0,1 m de la ligne, a arrêté la voiture aux portes 85 et
   86) : celui de ces jeux d'objets, large de 30 m au plus, est détruit (`removed_actors`) ; un
   monument est seulement signalé (le manège de la place de la République est sur le tour : la
   ligne le contourne), et les immeubles, façades et sols ne sont pas regardés ainsi, une boîte
   n'étant pas leur plan. Et l'étape `collision`
   (`backdrop_collision.py`) finit par un relevé : ce qui tient debout sur 2,5 m ou plus et
   n'arrête rien, par dossier, et par nom à moins de 60 m de la ligne conduite
   (`walls_without_collision`) : à 171 s de cette prise le joueur a traversé un mur de pierre
   aveugle et l'image est restée noire 18 s.

5. **Le masque du trajet** (`ground_mask.py --route <Carte>.circuit.csv`, fait par l'étape
   `materials` après le masque lointain) : à 34 cm le texel, le masque lointain ne peut pas
   porter une bordure ni une ligne de voie, et dans la prise longue de la porte 83 le quai est
   une plaine d'asphalte. Le masque du trajet est aussi fin que le masque de la zone (4096
   texels sur son grand côté : 11 à 16 cm) sur la boîte de la ligne conduite que le masque de
   la zone ne tient pas, élargie de 40 m (aujourd'hui 161 m sur 460 m des quais Jules Courmont
   et Jean Moulin) ; le matériau du sol le lit là, le masque lointain au-delà
   (`GTP_ROUTE_MASK=0` le coupe). Les rues à sens unique de deux voies ou plus reçoivent une
   ligne discontinue entre leurs voies (trois sur les quatre voies du quai), large de deux texels
   au moins dans ce masque (31 cm : un texel ne se voyait pas à la porte 86). Non versionné,
   comme le masque lointain ; `materials_report.json` donne `route_mask` (`texture`,
   `read_on_the_route`).

6. **Le masque de la zone refait** (étape `materials`, `route_dressing.ground_masks`) : sur le
   PC le masque de la zone va jusqu'au quai (4096 x 8192 : le masque du trajet n'y est pas
   nécessaire et n'est pas écrit) et datait de la fabrication de la ville : pas de ligne de
   voie, et à la porte 86 la chaussée que roule la fuite était claire et lisse. L'étape refait
   maintenant le masque de la zone avec les données de la machine, puis les masques lointain
   et du trajet, et sa note dit ce que chaque masque portait sous la ligne conduite avant et
   après (`ground_mask.py --probe` : asphalte, marquage, pavage, bordure, trottoir).

`materials_report.json` donne `far_mask` (`texture`, `read_past_the_mask`, `rivers`, `laid`) ;
`pit_cover_report.json` donne `heights_from`, `cells_read`, `covered`. Pas encore vu en jeu.

### Arbres des rues : l'érable plane à la place de l'aulne (ajouté le 24/11/2026)

Le trajet était planté avec la forme « Field » de l'aulne glutineux, branchue dès le sol : dans
la prise longue de la porte 81 (85 à 100 s) ses branches bouchaient la caméra. Le pack érable
plane de Fab (Megaplants, 15 maillages SimpleWind, tous Nanite, de 0,3 à 25 m ; dans le Content
du jeu sur le PC, hors Git) a des formes « Forest » au tronc dégagé d'un arbre d'alignement, la
doublure gratuite la plus proche des platanes de Lyon. La carte de base est figée et garde ses
aulnes : à chaque candidat l'étape `horizon` (`place_horizon.py`, `route_maples`) donne aux
composants d'arbres du dossier City/Route l'érable de leur sorte (`tools/city/route_trees.py` :
adulte `Forest_04`, 15,9 m ; jeune `Forest_07`, 6 m), mêmes places et mêmes orientations, la
hauteur moyenne gardée entre 13 et 17 m et entre 5 et 7 m. Les rangs du fond de la place
Bellecour prennent le `Field_02` de plein vent, ramené à 16,5 m, à 8 m au moins de la ligne
conduite. Une machine sans le pack garde les aulnes ; `GTP_ROUTE_MAPLES=0` aussi.
`horizon_report.json` donne `route_maples` (`swapped`, `missing`, `trees`) et, sous
`square_settings`, `tree_mesh`. L'étape des voies reconnaît un érable comme un arbre (son tronc
compte, pas son houppier). Les arbres des rues remplacés au lancement (`GTPStreetTrees`)
restent des aulnes.

Vu dans le jeu empaqueté à la porte 83 (prise longue, 10 à 30 s) : l'échange a tourné (1 870
arbres) et les feuilles sont dessinées, mais « Forest » veut dire poussé en forêt, une perche
mince avec quelques feuilles en haut, et un quai bordé de perches se lit comme des tiges nues.
L'échange est donc **coupé par défaut** (`GTP_ROUTE_MAPLES=1` l'active) et le trajet garde les
aulnes de la carte de base, les rangs de Bellecour aussi, tant qu'un érable n'a pas été regardé
sur une image avant d'être planté.

Les aulnes du trajet sont des arbres de plein champ : leurs tiges penchent d'un mètre et plus
sur ce qui est à côté, à hauteur de voiture, et le maillage porte la collision de sa propre
forme. À la porte 91 la voiture volée, à 0,3 à 0,9 m de sa ligne, a été arrêtée six fois par des
arbres dont le tronc était à l'écart de cette ligne (quai, 22 s ; rue du Président Carnot, 140 à
182 s). Depuis, un arbre du trajet est son tronc, comme l'étaient déjà les arbres des rues
échangés au lancement : à la fin de l'étape `lanes` les arbres feuillus des jeux n'arrêtent plus
rien et un poteau caché par arbre tient lieu de tronc (`route_trees.trunk_post`, acteur
`Route_TreeTrunks`). `GTP_TREE_TRUNKS=0` laisse les arbres tels quels ; `lane_clear_report.json`
donne `tree_trunks` (arbres, poteaux, acteurs qui les portaient). Pas encore vu en jeu.

### Chaussées recalées sur les bordures et régénération en une commande

Les axes BD TOPO sont à 0,8 m du milieu de la chaussée (2,2 m au 90e centile) et les voitures
sur des voies tirées de ces axes roulaient sur les bordures du terrain LiDAR.
`tools/city/carriageway.py` recale chaque voie carrossable sur les bordures Grand Lyon (rayons
tous les 2 m, lissés ; 79 % des échantillons trouvent une vraie bordure) et écrit
`processed/<zone>/carriageway.geojson` : un polygone, un axe corrigé et les demi-largeurs par
voie. `ground_mask.py` y dessine l'asphalte et les bordures ; `traffic.py` peut y poser ses voies.

`tools/city/regen_city.sh` régénère tout en une commande (données géo, génération avec la boîte
du Rhône, enseignes, chaussées, mobilier, détails, Rhône, import Unreal, façades, mobilier,
détails, matériaux, voiture garée, voies de trafic) : `caffeinate -dimsu tools/city/regen_city.sh`.

## Quai du Rhône et pont Lafayette (ajouté le 2026-11-10, pour la 0.3)

L'emprise générée s'étend vers l'est pour la boucle en voiture de la 0.3 :
`generate.py --extra-bbox 250,-60,700,220` ajoute les bâtiments du quai Jules-Courmont, de la tête
du pont et d'un bout de rive gauche (233 bâtiments BD TOPO, 378 parties, au lieu de 175 / 281).
`tools/city/riverside.py` construit en greybox (couleurs de sommets) l'eau du Rhône (plan au
niveau LiDAR de la surface, sans collision), le tablier du pont Lafayette (emprise Grand Lyon,
gid 41, ~20 m × 190 m, parapets ; l'emprise s'arrête au-dessus des bas-ports, donc chaque bout
est prolongé dans l'axe du pont jusqu'à rejoindre la rue à moins de 12 cm, puis posé sur la
hauteur médiane de la rue : marche mesurée de 7 cm au plus, collision active) et un parapet de
quai le long de l'eau (519 m dans la boîte, coupé sous le pont). `import_city.py` le pose s'il existe
(dossier City/Riverside) ; `apply_city_materials.py` y met `M_City_Shell` et `M_City_Water`.
La surface du fleuve est notre propre maillage avec `M_City_Water` (petites vagues mobiles,
opaque), pas le plugin Water du moteur : il exige un paysage et des passes en plus que le budget
M1 ne permet pas.
Chute dans le Rhône : un lit avec collision 2,5 m sous la surface (rien ne traverse le monde),
des boîtes de déclenchement de 25 m marquées `GTP_RiverRecovery` sur l'eau et des sorties de
quai au niveau de la rue tous les 25 m, marquées `GTP_RiverExit` (points tournés vers la terre,
jamais sur les bas-ports) : le jeu s'en sert pour faire nager ou repêcher le joueur et les
voitures.
`build.py` garde maintenant les noms de voies `nom_collaboratif_*` et `nom_voie_ban_*` de la
BD TOPO actuelle (les anciens `nom_1_*` n'existent plus).

## Détails de façade et de rue : deuxième passe d'habillage (ajouté le 2026-11-10)

L'audit en jeu de la 0.2 et de la 0.3 montre une rue « maquette » : sur environ 344 objets
importés, seuls 24 types étaient posés. `tools/city/details.py` ajoute les petits détails, toujours
accrochés à quelque chose de réel :

* **Façades** (`facade_modules.csv`) : une descente d'eau par immeuble côté mitoyen, sur toute la
  hauteur ; soupiraux sous les fenêtres du rez-de-chaussée haussmannien ; coffret de compteurs,
  plaque de numéro et mascaron sur la clé des portes cochères (positions calculées sur le kit) ;
  stores bannes déroulés sur les cafés, restaurants et commerces de bouche, repliés sur un commerce
  sur deux ailleurs, étirés à la largeur de la travée ; distributeur sur les banques ; cendrier aux
  portes des bars et tabacs ; paillassons ; jardinières sur l'appui des fenêtres lyonnaises,
  quelques paraboles et climatiseurs sur les immeubles anciens ; un présentoir contre le fond de chaque
  vitrine ouverte de la rue (rien à voir derrière la vitre sinon).
* **Angles** : plaques de rue sur chaque immeuble d'angle, côté de l'angle.
* **OpenStreetMap** : potelets (sauf ceux posés sur une chaussée : une borne d'accès au milieu d'une
  rue où roulent les voitures du jeu est écartée, et listée dans `details_report.json` ;
  `tools/city/drop_carriageway_bollards.py` les retire d'une table existante sur une machine sans les
  données géographiques brutes), feux piétons des deux côtés des traversées à feux (largeur réelle de
  la chaussée BD TOPO), feux de circulation sur la bordure de droite, panneaux par code français
  (B1 sens interdit, AB3a cédez le passage, B54 zone piétonne...) ramenés sur la bordure Grand Lyon,
  armoires techniques, colonnes d'affichage.
* **Arbres, bancs et corbeilles** : feuilles mortes de platane autour d'un arbre sur deux (automne,
  saison de la bible), déchets et journaux près de certaines corbeilles, pigeons près des bancs.
* **Toits** : souches de cheminée sur les murs mitoyens, à la hauteur du faîtage.

**Voies de circulation dégagées** (`tools/city/clear_lanes.py`). Dans les rues fermées aux voitures
dans la réalité, les bancs, arceaux à vélos, scooters garés et panneaux relevés sont au milieu de la
rue, là où passent les voies de circulation du jeu. `props.py` et `details.py` écartent tout objet
qu'une voiture peut heurter (collision active) à moins de 1,5 m de l'axe d'une voie ou du lien entre
une voie et la suivante, et les comptent dans `left_out_on_lanes` de leur rapport (rue de la
République : 17 objets de mobilier et 24 détails). Un objet gênant est retiré, pas déplacé. Sur une
machine sans les données géographiques brutes,
`python3 tools/city/clear_lanes.py --name presqu-ile --focus rue-de-la-republique [--lanes <le
lanes.csv de la carte>] [--dry-run]` filtre les tables existantes (il garde
`<table>.before-lanes.csv` et écrit une ligne, `decor off the lanes: {...}`) ; `place_props.py`
repose ensuite chaque jeu.

Les immeubles suivent la même règle dans `generate.py` : une emprise générée à moins de 1,5 m de
l'axe d'une voie est reculée le long de la voie, et un immeuble qui y perdrait plus de 40 % de sa
surface (ou garderait moins de 25 m²) est sur la chaussée et n'est pas généré ; `backdrop.py` ne le
remet pas. `report.json` liste les deux cas sous `off_lanes`. Rue de la République : trois emprises
reculées (rue Claudia, un vrai passage de 2 m porté à 3 m, 8,1 et 1,5 m² ; un angle de la rue
Thomassin, 0,9 m²) et un immeuble BD TOPO de 53 m² écarté, rue Champier, que la maquette 2018 place
1,2 m plus à l'est. Le mur de son voisin de ce côté devient une façade sur rue ; tous les autres immeubles sortent à l'identique. `GTP_CITY_CLEAR_LANES=0` garde
les emprises relevées. `regen_city.sh` lance maintenant `carriageway.py` avant la génération,
puisque les voies suivent les chaussées mesurées.

Le choix d'une travée est un hachage stable de l'immeuble et de la travée : chaque passage donne la
même rue. Le tableau a les colonnes de `props_placement.csv` plus `snap` (`keep` garde la hauteur des
objets muraux et de toit), `scale_x/y/z` et `collision` (aucune sur les petits objets). Plafond de
3 000 instances (budget du fil optimisation, 1 ms GPU sur m1), par priorité de visibilité ;
environ 2 100 pour la rue de la République. `run_city_import.sh` rejoue `details.py` puis le pose avec
`GTP_PROPS_SET=details` dans le dossier City/Details.

Les vrais logos d'enseignes ne s'affichaient pas : le plugin hors Git `Plugins/CityBrands` n'existait
pas sur le Mac (`citybrands_mounted: false`). `tools/city/unreal/mount_citybrands.sh`, lancé par
`run_city_import.sh`, crée le plugin de contenu vide s'il manque et vérifie qu'il est ignoré par Git.

**Enseignes drapeau** (`place_facades.py`) : un commerce sur deux reçoit, sur sa première travée,
une enseigne perpendiculaire à la façade au-dessus du bandeau (1,1 m, même texture que le bandeau,
deux panneaux dos à dos, sans ombre).

**Preuve avant/après** : `tools/city/evidence/before_after.sh <builds avant> <builds après> <version>`
lance le build « après » sur le parcours de démonstration (profil m1, le jeu se filme), puis écrit
des images côte à côte aux mêmes instants (`compare-stills.swift`, AVFoundation, sans ffmpeg) et les
deux résumés de temps d'image.

## Monuments repères et poids des cellules (ajouté le 2026-11-10)

`tools/city/landmarks.py` remplace le kit de façades générique par un volume mesuré pour les
monuments qu'on reconnaît : l'Opéra (voûte de verre) et l'Hôtel de Ville (pavillons, beffroi,
cours). Emprise OpenStreetMap (identifiant Wikidata), cours retirées là où le LiDAR voit le sol,
murs sur le vrai contour, toit = champ de hauteur du MNS LiDAR raccordé au haut des murs.
Opéra : 2 072 m², 45 m, 4 261 triangles ; Hôtel de Ville : 3 764 m² dont 1 604 m² de cours
retirés, 52 m, 7 856 triangles. Les 6 bâtiments BD TOPO couverts sortent de `generate.py`
(`left_to_landmarks` dans le rapport) et `import_city.py` pose `gen/<focus>/landmarks.glb`
avec collision. `GTP_CITY_LANDMARKS=0` revient aux coques génériques.

Places Bellecour et Jacobins : données seulement (contour, revêtement, monuments). La statue
de Louis XIV et les fontaines des Jacobins et Bartholdi n'ont pas de volume : le MNS LiDAR
est égal au terrain sur ces places (sursol filtré), il faudra des modèles. Limite connue.

`tools/city/cell_budget.py` estime le poids de chaque cellule (instances de façade, enseignes,
mobilier) calibré sur la rue mesurée (21 299 + 3 400 instances = 14 ms). Pire cellule
p0000-m0500 : 8,4 ms ; pire bloc 2 × 2 : 26,1 ms ; 3 × 3 : 50,4 ms. Bornes hautes (ni
distance de coupe ni occlusion) : 2 × 2 dépasse les 14 ms de l'ADR-0010, d'où la variété du
kit à coût constant et la coupe à 250 m à mesurer.

Créneau Mac en une commande : `caffeinate -dimsu tools/city/city_slot_mac.sh <builds_avant> 0.3`
(régénération, package, test M1 60 s filmé, avant/après du fil réalisme).

**Coût GPU de l'habillage** : `tools/city/unreal/profile_details.sh` mesure, en `-game` et sans
reconstruire la map, le temps d'image à quatre points fixes de la rue, avec tout visible puis en
masquant les détails, le mobilier, ou un groupe de détails à la fois (tags d'acteur `GTP_Set_*` et
`GTP_Cat_*` posés par `place_props.py`). Résultat : `Saved/CityImport/details_profile.json`. Les détails
plaqués au mur, les présentoirs derrière les vitres et les petits objets ne projettent plus d'ombre
(colonne `shadow`) ; seuls les stores et les cheminées gardent la leur.

**Arbres en feuilles** : `GTP_CITY_LEAFY=1` (ou `props.py --leafy`) remplace, aux mêmes ancrages
Grand Lyon, les troncs nus de platane par le hêtre Megascans gratuit (`SM_EuropeanBeech_Field_01`,
`_Sapling_01` pour les jeunes arbres). Désactivé par défaut tant que son coût M1 n'est pas mesuré.

**Vitrines à fausse profondeur** (défaut, `GTP_SHOP_GLASS=interior`) : `M_City_ShopInterior` est un verre
opaque qui dessine une boutique éclairée par « interior mapping » (un test rayon-boîte par pixel dans
le repère du module : fond garni d'étagères aux couleurs variées, sol, plafond lumineux, murs),
sans translucidité ni surcoût de remplissage ; reflets et Fresnel par-dessus, émissif multiplié par
`EmissiveScale` pour la nuit. `translucent` (vraie vitre et présentoirs W14-32) et `opaque` restent
disponibles. Les détails ont des distances d'affichage de 20 à 40 m (colonne `cull_m`, 60 m pour
cheminées et feux).
Les détails sans ombre (petits ou plaqués au mur) sortent aussi des champs de distance de Lumen,
de la scène de lancer de rayons et des décalques : rien ne change à l'image, et Lumen logiciel a moins
de champs de distance à parcourir (`GTP_DETAILS_LEAN=0` le désactive pour comparer).

**Interrupteurs par groupe** (passes de perf, tous vides par défaut = habillage complet), lus par
`tools/city/details.py`, par id de catalogue ou famille : `GTP_DETAILS_ONLY=W14-42,Shopfront` ne
garde que ces groupes, `GTP_DETAILS_DROP=W14-03` en retire, `GTP_DETAILS_THIN=W14-42:0.5` garde une
part stable d'un groupe (les mêmes instances à chaque passe), `GTP_DETAILS_CULL=W14-42:25,*:0.8`
fixe une distance d'affichage en mètres ou les multiplie toutes. `details_report.json` liste ce qui
a été appliqué. **Fusion de matériaux** : avec `GTP_DETAILS_MERGED=1`, `run_city_import.sh` lance
d'abord `tools/city/unreal/merge_detail_materials.py`, qui cuit chaque détail à plusieurs
matériaux en `SM_<Asset>_Merged` (un seul matériau atlas, 512 px par défaut, `GTP_MERGE_TEXTURE` ;
chaîne de LOD d'origine refaite), puis `place_props.py` utilise ces maillages : sans Nanite, chaque
emplacement de matériau coûte un appel de dessin de plus par passe. Le script écrit de nouveaux
assets dans Content, il reste donc coupé tant qu'un créneau ne l'a pas validé.

### LOD du kit de façades (performance, 2026-11-10)

Les 21 242 modules de la map de rue faisaient 10,2 M de triangles en LOD0 jusqu'à 250 m, redessinés
par les ombres virtuelles, sans Nanite sur M1 : c'est la première cause des 44,5 ms de p95 en
package. `place_facades.py` génère maintenant quatre LOD par module (100 %, 50 % sous 0,3 de taille
d'écran, 15 % sous 0,1, 5 % sous 0,035, soit environ 10, 30 et 85 m) ; rien de visible n'est coupé.
`GTP_KIT_LODS=0` pour l'avant, `GTP_FACADE_CULL="court=120"` pour tester une distance au banc.
Banc en une commande : `caffeinate -dimsu tools/city/facade_lods_mac.sh` (rejoue seulement les
façades et les matériaux, `FACADES_ONLY=1`, puis package dans Builds/facade-lods/).

### Atlas des enseignes (performance, 2026-11-10)

Chaque enseigne était son propre acteur avec son matériau et sa texture : environ 600 draw calls
pour 341 bandeaux et les enseignes drapeau. `tools/city/sign_atlas.py` range les textures dans
des pages 4096 × 4096 (64 enseignes par page, 1008 × 228 px chacune, bords répétés contre le
débordement des mips) : 5 pages de lettrage et 1 page de logos (hors Git, plugin CityBrands).
`place_facades.py` pose une composante instanciée par page (plus une sans ombre pour les drapeaux)
avec `M_Sign_Atlas`, la case de chaque enseigne en données d'instance. `GTP_SIGN_ATLAS=0` revient
aux acteurs séparés (l'avant du banc).

### Coques fusionnées par cellule (performance, 2026-11-10)

`generate.py` écrit aussi `shell_cells.glb` : les coques fusionnées par cellule de 250 m (grille de
l'ADR-0010), 376 coques → 8 maillages sur la map de rue. `GTP_SHELL_MERGE=1` les fait poser par
`import_city.py` à la place des coques séparées (collision complexe conservée). Désactivé par défaut.

## Quartier en streaming : substituts lointains, niveaux de cellule, chargement (ajouté le 2026-11-11, ADR-0010)

Première mise en œuvre de l'option B de l'ADR-0010, préparée et essayée hors Mac ; pas encore mesurée.

- `tools/city/cell_proxies.py` fusionne, par cellule, toutes les coques et les volumes de
  monuments de `gen/<cellule>/` en un substitut lointain (`gen/quarter/cell_proxies.glb` et
  `.json` : niveau, boîte de la cellule en cm Unreal, triangles). Quartier de 30 cellules :
  29 substituts (une cellule est vide), 29 draws, 242 k triangles ; `--check` échoue au-delà
  de 100 draws ou 400 k triangles. `--near-street rue-de-la-republique --within 60` donne les
  cellules qu'un trajet dans la rue charge : p0000-m0250, p0000-p0000, p0000-p0250,
  p0000-p0500, p0000-m0500.
- `import_city.py` prend `GTP_CITY_LEVEL` : `quarter` construit le niveau persistant
  `L_City_Quarter` (éclairage de la bible, terrain réutilisé, substituts sans collision, ombre
  ni champ de distance, étiquetés `GTP_CellProxy`, niveau et `bounds:`), `cell` construit un
  niveau de streaming vide `L_City_Cell<X><Y>` avec les seuls bâtiments et monuments de la
  cellule. `place_facades.py` réutilise le kit de la rue (`GTP_KIT_FOCUS`, `GTP_KIT_REUSE=1`),
  sans le réimporter à chaque cellule ; `apply_city_materials.py` ne fait que donner aux
  coques de la cellule le matériau de coque existant.
- `UGTPCityStreaming` (C++) lit les substituts étiquetés au démarrage, planifie avec
  `GTPCityStreamingCore.h` (chargement à 60 m d'une cellule, conservation jusqu'à 90 m,
  4 cellules au plus, une demande par mise à jour, la plus proche d'abord), charge par
  `ULevelStreamingDynamic`, cache un substitut dès que son niveau est visible et le remontre
  avant de libérer le niveau. `-CityCells=0`, `-CityCellLoad=`, `-CityCellUnload=`,
  `-CityCellMax=`. Sans effet sur une map sans substituts. Règles vérifiées par
  `tools/city/tests/city_streaming_test.cpp` (CI).
- Créneau Mac : `tools/city/evidence/quarter_slot_mac.sh` (compilation, cellules, substituts,
  quartier, 5 cellules, puis le parcours démo streaming actif et substituts seuls ; garde-temps
  par étape, signe de vie, contenus restaurés par md5 sauf `KEEP=1`). Environ 2 h 10.
- Quartier entier (2026-11-11, 0.9) : `cells.py` pose aussi le mobilier relevé par cellule (`props.py --cell`, 20 460 objets) et, sur un passage complet, écrit les substituts, les voies du quartier (`traffic.py --focus quarter` : 1 043 voies, 45,5 km) et le budget par cellule avec une estimation mémoire (`cell_budget.py` : pire 2 × 2 + partagé ≈ 446 Mo pour 1,5 Go). `tools/city/quarter_build.py` construit tous les niveaux sans écran, sur le Mac ou le PC Windows (Python seul, limite de temps par étape, arrêt forcé d'un éditeur bloqué, `--dry-run`).
- Leviers sur un package, sans régénérer (`UGTPCityRenderTuning`) : `-CityCull=<famille>=<m>,...`
  fixe la distance de disparition par famille d'étiquette, sur les composants instanciés des
  cellules (`ground`, `upper`, `eave`, `court`, `sign`, `props_<taille>`, `details_<taille>`) et sur
  le niveau persistant (`proxy`, `terrain` ; une map construite avant ces étiquettes est reconnue
  par le dossier du maillage) ; `-CityProxies=0` n'affiche jamais les substituts lointains ;
  `-CityPropShadowBelow=<m>` ; `-CityTuneReport=1` journalise seulement. Une ligne par niveau :
  `CityRenderTuning: <niveau> <n> components changed; <famille>=<changés>/<trouvés> ...`.
- Cellules v2 (2026-11-11, 0.9) : `cells.py` écrit aussi les détails de rue de chaque cellule le
  long de la rue de la République (`details.py --focus <cellule> --cell=x0,y0,x1,y1` : la zone de
  la rue coupée à la boîte de la cellule, rien en double à une couture ; 1 909 dans les cinq
  cellules de la rue, à peu près les 1 890 de la map de rue). `quarter_slot_mac.sh` et
  `quarter_build.py` posent le mobilier et les détails dans chaque niveau de cellule
  (`place_props.py`, jeux `props` et `details`). Compensation du budget, active par défaut dans
  les seuls niveaux de cellule (`UGTPCityRenderTuning`) : façades de cour et enseignes jusqu'à
  60 m et 45 m, distances du mobilier et des détails à 60 % (`-CityCellDetailPct=`, de 10 à 100), pas d'ombre sous
  1 m ; `-CityCellDefaults=0` la coupe, `-CityCull` l'emporte, une distance de 0 masque une famille.
- Pas encore : mobilier et détails dans les cellules loin de la rue de la République, masque de sol à l'échelle du quartier,
  niveaux de cellule dans la liste de cuisson pour un package.

## Toits à la Mansart et tranche République (ajouté le 2026-11-12)

Les immeubles Second Empire de la rue de la République se terminent par un comble en zinc à la
Mansart, pas par des étages de pierre en plus. Le LiDAR voit le brisis presque vertical : le
générateur empilait donc 7 ou 8 étages de pierre jusqu'à l'égout. `procgen.levels_for()` termine
désormais un mur haussmannien de 19 m ou plus par un niveau de comble de 3,2 m : les étages de
pierre s'arrêtent dessous et portent la corniche, puis un module de toit par travée :

- `dormer` (côté rue, travées de 2,2 m ou plus) : `SM_Roof_Haussmann_Dormer`, brisis en zinc avec
  une lucarne en pierre (fenêtre à battants, appui, jambages, linteau, fronton cintré, joues en zinc).
- `mansard_blank` (côté cour, travées étroites) : `SM_Roof_Haussmann_MansardBlank`, brisis seul.

Les modules sont construits par `tools/city/facade_kit/roof_kit.py` (branché à la fin des
définitions de `kit.py` ; il ajoute l'emplacement `MI_Roof_Zinc`). Le brisis recule de 0,8 m à
l'égout pour que le terrasson du toit généré le rejoigne. `place_facades.py` pose la corniche sur
le dernier étage de pierre, sous le comble. La hauteur du comble varie de 2,8 à 3,8 m pour que les
étages de pierre restent proches des 3,3 m nominaux : sur la tranche, étages courants p10 / médiane /
p90 = 3,07 / 3,30 / 3,53 m.

Une tranche plus courte pour la cible visuelle des deux semaines (Bellecour aux Jacobins) :

```
python3 tools/city/generate.py --name presqu-ile --focus "Rue de la République" \
  --clip=-80,-570,300,-180 --out republique-tranche
```

`--clip X0,Y0,X1,Y1` garde les bâtiments dont le centre est dans la boîte et coupe l'axe de la
rue ; `--out` nomme le dossier de sortie. Résultat (2026-11-12, machine cloud) : 58 bâtiments,
85 volumes, 3 964 modules dont 270 lucarnes et 78 travées de comble aveugles, 86 k triangles,
5,7 s. Côté rue, 26 bâtiments sur 45 ont 7 niveaux (rez-de-chaussée, entresol, 4 étages, comble).

Intérieurs visitables : `place_facades.py` ne pose pas les travées du rez-de-chaussée sur la vitrine
d'un intérieur de `tools/interiors/*.json` (pivot à moins de 1 m de la ligne y = 0 du cadre, sur sa
largeur plus 0,5 m ; les autres côtés sont des murs pleins et gardent la façade de ville) : la
vitrine et la porte de l'intérieur se voient (`tools/city/interior_cutouts.py` ;
`GTP_INTERIOR_CUTOUTS=0` les garde). La coque n'a pas de mur derrière les modules de façade, rien
d'autre n'est ouvert. Fast-food Bellecour : 9 modules sur la façade place Bellecour (4 travées et 5 bandes) ;
cette façade est aujourd'hui générée comme un côté cour, car la place n'est pas une rue nommée.

Places et quais : une façade sans rue devant était classée côté cour. Désormais, trois rayons (normale
et ±35°) qui parcourent 24 m sans croiser d'emprise en font une façade sur rue, nommée d'après la rue
la plus proche (`procgen.faces_open_space`). Sur le focus République, 353 modules (7 bâtiments)
deviennent des façades sur rue, place des Cordeliers, place de la Bourse et place Le Viste ; la
façade du fast-food sur la place Bellecour reçoit des devantures.

## Fond de carte et axe de rue complet (ajouté le 2026-11-12, tranche République)

Les vues larges de la tranche (place Bellecour, Pathé, place de la République) finissaient sur
l'horizon derrière la zone de 45 m. `tools/city/backdrop.py` donne à tous les autres bâtiments
BD TOPO à moins de 600 m de l'axe un volume fermé en basse définition (murs jusqu'à l'égout LiDAR
dans la couleur de la typologie, toit ajusté au LiDAR, sans modules de façade), fusionné par
cellule de 250 m. `regen_city.sh` le lance après la génération et `import_city.py` le pose dans
la carte de rue (dossier City/Backdrop, matériau des coques, sans collision ni champ de distance,
étiquette de coupe `proxy` ; `GTP_CITY_BACKDROP=0` le saute). République (2026-11-12, machine
cloud) : 1 589 bâtiments, 102 k triangles, 30 maillages, 16 s.

L'axe de la rue n'était que le plus long morceau de la rue. La rue de la République est coupée
par la place de la République : l'axe s'arrêtait là, le départ du joueur (« côté Bellecour ») était
sur la place et le tronçon Bellecour (Y Unreal de 24 000 à 53 000) n'était jamais sur l'axe.
`generate.chain_axis` raccorde maintenant les morceaux par-dessus des trous de 150 m au plus : l'axe
va de Y -50 050 à +52 361 cm. Le masque du sol est en dalles (G) sur toute la rue ; l'asphalte (R)
n'apparaît qu'aux croisements avec des rues ouvertes aux voitures.

Deux repères pour la tranche (`landmarks.py`) : l'immeuble du Pathé Bellecour (voie OSM 82377010)
prend son volume LiDAR, tour d'angle comprise (35 m ; le toit générique la coupait), et le bassin de la
place de la République (voie 129172257) un nouveau type `basin` : margelle en pierre de 0,45 m sur le
contour OSM et un plan d'eau, car le LiDAR n'y voit aucun volume. Le carrousel n'a pas d'emprise
OSM : un volume `carousel` (12 m, 6,5 m de haut) est posé au bord nord du bassin, position estimée
sur la photo V4 (±10 m, à recaler sur la vue de dessus V0).
Depuis la porte 91 il est plus au nord, entièrement sur la place (12 m à la porte 91, local
172,-239 ; 15 m depuis, local 172,-236 : le coin de sa boîte arrivait à 1,46 m d'une voie de
circulation) : à sa
première position il couvrait la chaussée de la rue Jean de Tournes, la voiture volée s'y est
arrêtée aux portes 85 et 86 et la circulation roulait dedans. La carte de base figée l'a encore à
l'ancienne place : l'étape `lanes` de la préparation déplace son acteur
(`lane_clear.LANDMARK_MOVES`, `landmarks_moved` dans le rapport) et retire le mobilier qui se
trouvait là où il est maintenant.

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

## Surfaces scannées (réalisme de la rue)

`tools/city/surfaces.json` liste les surfaces scannées de la rue : Megascans gratuits sur Fab, en 2K, hors Git. Il couvre les dalles de granit clair, la pierre de taille, l'enduit, la bordure, le zinc, l'ardoise et l'asphalte, puis les pavés, les trottoirs, les tuiles, les grilles et les décals.

`python tools/city/surfaces_build.py [--ab]` (PC Windows ou Mac, sans bash ; `--ab` filme aussi la rue sans puis avec les surfaces dans `Saved/CityImport/surfaces_ab/`) ou `tools/city/unreal/surfaces_mac.sh` enchaîne deux lancements sans écran :
1. `import_surfaces.py` trouve chaque surface dans le projet, par un dossier au nom de son id ou par son nom Megascans. Il la plafonne à 2K et écrit `Saved/CityImport/surfaces.json`.
2. `apply_city_materials.py` reconstruit ensuite les matériaux de la ville. Ses crochets de `gtp_surfaces.py` posent les textures dans :
   - le sol, avec une couche par canal du masque, les marquages restant procéduraux ;
   - la pierre et l'enduit des façades, sous forme d'un détail qui garde la teinte de chaque immeuble ;
   - les toits.

Sans le manifeste, ou avec `GTP_SURFACES=0`, les matériaux restent procéduraux.

Budget M1 : au plus 15 lectures de texture pour le sol (la saleté comprise, en texture plutôt qu’en acteurs décals), 4 pour le kit et 6 pour les coques, toutes avec des samplers partagés. `GTP_SURFACES_LITE=1` ne garde que les cartes de couleur.

## Toute la place de la République (ajouté le 2026-11-12, tranche République)

La zone de 45 m autour de la rue ne prenait que les façades de la place de la République proches
de l'axe de la rue de la République. Le côté est de la place, le fond de son bord nord et les
angles sur la rue Childebert restaient des volumes du fond de carte, gris dans la vue large V4.
`generate.py --also-street NOM[:TAMPON_M]` ajoute tout bâtiment dont l'emprise est à moins de
TAMPON_M (8 m par défaut) des tronçons BD TOPO nommés NOM. Il s'applique après `--clip`, si bien
que le fond de la place est généré même quand son centroïde sort de la boîte de la tranche. À
8 m, on ne prend que les façades sur la place ; dès 12 m, les bâtiments en fond d'îlot et le
Grand Hôtel-Dieu entrent. `faces_open_space` fait ensuite des côtés de la place des façades sur
rue : devantures, balcons et mansarde.

`regen_city.sh` passe `--also-street "Place de la République"` par défaut
(`GTP_CITY_ALSO_STREET`, vide pour s'en passer). Focus République (2026-11-12, machine cloud) :
4 bâtiments de plus, 235 en tout, 19 341 modules (+337), 417 k triangles (+8 k) ; le fond de carte
passe à 1 585 volumes. Tranche (`--clip=-80,-570,300,-180`) : 6 bâtiments de plus, 63 en tout,
4 267 modules.

## Façades sur rue par l'emprise, et départ dégagé (ajouté le 2026-11-13)

Le package 18d montrait un mur aveugle au départ. Un bâtiment profond, dont la seule façade donne
sur la rue de la République (BD TOPO 240874130, à 12 m à l'est du départ), avait son centroïde
hors de la zone de 45 m. Il restait donc sur la rue comme volume du fond de carte, sans fenêtres
et sans collision ; la marche de démo et la caméra entraient dedans. `generate.py --front` (14 m
par défaut) prend aussi tout bâtiment dont l'emprise est à moins de 14 m des tronçons de la rue :
3 sur le focus République. L'angle de la place Le Viste, vu au demi-tour (240874625), entre par
`--also-street "Place Le Viste:15"`, que `regen_city.sh` passe par défaut avec la place de la
République (`GTP_CITY_ALSO_STREET`, éléments séparés par « ; »). Contrôle par rayons depuis le
départ (machine cloud) : aucun volume du fond de carte à moins de 150 m, dans toutes les
directions. Focus République : 239 bâtiments, 20 669 modules, 446 k triangles ; fond de carte
1 581 volumes.

Le départ du joueur passe de 8 m à 30 m le long de l'axe (`SPAWN_ALONG_AXIS_CM`, mesuré le long de
la polyligne). Le panoramique de la démo (±60°) voit alors des façades du kit des deux côtés, et
la rue est dégagée sur 150 m devant.
## ZoneGraph pour Mass (ajouté le 12/11/2026, ADR-0015)

`tools/city/zonegraph.py` transforme les voies de circulation (`traffic.py`) et les cheminements piétons (`pedestrians.py`) en `gen/<focus>/zonegraph/zonegraph.json` : des splines « Car » à sens unique d'un carrefour au suivant (les bouts d'une rue enchaînés, les carrefours reliés par une voie de moins de 22 m fondus en un, comme les routes de City Sample), coupées 6 m avant chaque carrefour, un polygone par carrefour (tags Car et Intersection) dont les points sont ces extrémités coupées, et des splines « Pedestrian » à double sens pour les cheminements (tag Crossing sur les passages). `--lanes` et `--walkways` prennent à la place les tables découpées de `npc_data.py`. `run_city_import.sh` appelle ensuite `tools/city/unreal/place_zonegraph.py`, qui confie la table à l'importeur C++ `UGTPZoneGraphImport` (module GTPMass) puis sauvegarde la map : le ZoneGraph n'est jamais dessiné à la main. `GTP_ZONEGRAPH=0` saute l'étape ; ses échecs sont consignés dans `Saved/CityImport/zonegraph_report.json` sans arrêter l'import, tant que les premiers passages dans l'éditeur ne sont pas propres. Pour vérifier sur la map : affichage du ZoneGraph dans la vue de l'éditeur (Show > ZoneGraph).
