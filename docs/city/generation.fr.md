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
* **OpenStreetMap** : potelets, feux piétons des deux côtés des traversées à feux (largeur réelle de
  la chaussée BD TOPO), feux de circulation sur la bordure de droite, panneaux par code français
  (B1 sens interdit, AB3a cédez le passage, B54 zone piétonne...) ramenés sur la bordure Grand Lyon,
  armoires techniques, colonnes d'affichage.
* **Arbres, bancs et corbeilles** : feuilles mortes de platane autour d'un arbre sur deux (automne,
  saison de la bible), déchets et journaux près de certaines corbeilles, pigeons près des bancs.
* **Toits** : souches de cheminée sur les murs mitoyens, à la hauteur du faîtage.

Le choix d'une travée est un hachage stable de l'immeuble et de la travée : chaque passage donne la
même rue. Le tableau a les colonnes de `props_placement.csv` plus `snap` (`keep` garde la hauteur des
objets muraux et de toit), `scale_x/y/z` et `collision` (aucune sur les petits objets). Plafond de
3 000 instances (budget du fil optimisation, 1 ms GPU sur m1), par priorité de visibilité ;
environ 2 100 pour la rue de la République. `run_city_import.sh` rejoue `details.py` puis le pose avec
`GTP_PROPS_SET=details` dans le dossier City/Details.

Les vrais logos d'enseignes ne s'affichaient pas : le plugin hors Git `Plugins/CityBrands` n'existait
pas sur le Mac (`citybrands_mounted: false`). `tools/city/unreal/mount_citybrands.sh`, lancé par
`run_city_import.sh`, crée le plugin de contenu vide s'il manque et vérifie qu'il est ignoré par Git.

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
