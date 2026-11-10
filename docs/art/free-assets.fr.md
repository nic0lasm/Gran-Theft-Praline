# Assets gratuits pour Unreal : recensement

*English version: [free-assets.md](free-assets.md)*
*Relevé le 2026-11-10, pages ouvertes ce jour-là. Prix et licences bougent : relire la fiche au moment du téléchargement et l'inscrire dans [`assets/SOURCES.md`](../../assets/SOURCES.md).*
*Complète le [recensement des sources](../sourcing/asset-sourcing.fr.md) (outils, frameworks, géodonnées) et respecte la [bible artistique](art-bible.fr.md).*

But : arrêter de générer (Tripo, 55 crédits par modèle) ce qui existe déjà
gratuitement, en priorité pour ce que voit le joueur rue de la République
(sol, façades, mobilier, enseignes), puis l'eau, le feuillage, les véhicules.

## 1. En bref

1. **Megascans est entièrement gratuit sur Fab, sans date de fin** (annonce du
   2026-11-05, toutes les fiches passent à 0 $ d'ici le 2026-11-12). Licence
   Fab Standard : tout moteur, pas d'attribution, interdiction de redistribuer
   les fichiers seuls, donc **hors Git** comme le reste de nos assets. C'est
   la source n° 1 pour le sol, les surfaces, les décalques et le mobilier
   générique. Il faut le compte Epic de Nico.
2. **Feuillage : arbres Megascans gratuits, mais pas de platane gratuit
   trouvé.** Gratuits et vérifiés : European Beech (17 arbres), European
   Black Alder (22 arbres), Megaplants Norway Maple (érable plane, Forest et
   Saplings). Le seul vrai pack de platanes trouvé est payant (Greenleaf
   Vision, 23 préréglages Platanus, 29,99 $, UE 5.8, Nanite Foliage
   expérimental).
3. **Eau : rien à télécharger.** Le plugin Water est livré avec le moteur
   (rivières par spline, Single Layer Water, flottaison). Sur Mac, le
   *bake* de simulation plante (bug Metal signalé, sans correctif) : on
   l'utilise sans simulation de fluide.
4. **Sans compte, les fils peuvent tout récupérer seuls sur Poly Haven
   (CC0) et ambientCG (CC0)** : 521 modèles, 869 textures et 997 HDRI sur
   Poly Haven, 2 041 matériaux sur ambientCG (comptes lus sur leurs API le
   2026-11-10).
5. **Packs Epic à ne pas prendre maintenant** : City Sample (gratuit,
   à jour 5.8, mais américain, très lourd, licence limitée à Unreal),
   City Sample Vehicles (13 voitures américaines), Electric Dreams (nature,
   exige DirectX 12), Paragon et Infinity Blade (fantasy), PCG Biome
   (expérimental, aucun asset urbain).
6. **Règle proposée** : avant toute nouvelle génération Tripo, chercher
   d'abord dans Megascans puis Poly Haven et ambientCG. Tripo reste pour ce
   qui est propre à Lyon (potelets, abribus TCL, Vélo'v, kiosques, bancs de
   la Ville) et qu'aucune bibliothèque n'a.

## 2. Tableau des packs

Coût M1 = estimation pour le profil M1 (cible p95 ≤ 42 ms, ADR-0002), à
mesurer avant toute intégration sur next.

### 2.1 Fab et Epic (compte Epic requis)

| Pack | Lien | Licence | Taille | UE 5.8 / Mac | Couvre | Coût M1 estimé | Verdict |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Quixel Megascans (bibliothèque entière) | [vendeur Quixel sur Fab](https://www.fab.com/sellers/Quixel%20Megascans), [annonce](https://www.cgchannel.com/2026/11/epic-has-made-megascans-free-again-for-good-this-time/) | Fab Standard : tout moteur, sans attribution, pas de redistribution des fichiers seuls ; « usage avec l'IA : oui » | Par asset (quelques dizaines à quelques centaines de Mo) | Oui, via le plugin Fab de l'éditeur | Pavés, asphalte, pierre, enduits, décalques (fissures, marquages, plaques), mobilier et déchets génériques, plantes | Faible si Nanite et textures plafonnées à 2K en M1 ; décalques à surveiller (surcoût de passes) | **À prendre maintenant** |
| European Beech (Megascans Trees) | [fab.com/s/5db64af5b801](https://fab.com/s/5db64af5b801) | Gratuit, Megascans | Non affichée | « Unreal Engine », version non précisée | 17 arbres (8 forêt, 1 champ, 4 jeunes, 4 semis), vent et saisons, LOD | Moyen : feuillage masqué, ombres VSM | **À prendre** (parcs, quais) |
| European Black Alder (Megascans Trees) | [fab.com/s/8f6f8b0282d1](https://fab.com/s/8f6f8b0282d1) | Gratuit, Megascans | Non affichée | Idem | 22 arbres, vent et saisons, LOD | Moyen | **À prendre** (berges) |
| Megaplants : Norway Maple Forest et Saplings | [Forest](https://www.fab.com/listings/e789ad04-cf79-4973-adb0-85c794d77144), [Saplings](https://www.fab.com/listings/e79a64ce-2291-4505-ba8e-d2fdf20791e8) | Gratuit, Megaplants | Non affichée | Exige le plugin PVE et « Nanite Foliage (Experimental) » | Érable plane, préréglages éditables : arbre d'alignement feuillu en attendant les platanes | Inconnu, à mesurer : Nanite Foliage expérimental | **À prendre et tester** |
| PVE Broadleaf Park Tree Presets (Greenleaf Vision) | [fiche Fab](https://www.fab.com/listings/e141228c-0db1-4560-9dbe-48067e8ba2d5) | **Payant 29,99 $** | 7 Go | UE 5.8 requis, PVE + Nanite Foliage expérimentaux | 23 platanes + 19 tilleuls, variantes d'hiver sans feuilles, vent, saisons | Inconnu, à mesurer | **Achat à décider** après le test Megaplants |
| Plugin Water (moteur) | [doc Epic](https://dev.epicgames.com/documentation/unreal-engine/water-system-in-unreal-engine) | Moteur | 0 (déjà installé) | Plugin listé expérimental ; *bake* de simulation qui plante sur Mac ([fil](https://forums.unrealengine.com/t/ue-v-5-6-1-water-simulation-bake-crash-on-macos-15-6-1-metal-apple-m3-ultra-28-core-cpu-60-core-gpu-32-core/2656076)) ; rivière qui disparaît en LiveSim en 5.8.3 ([fil](https://forums.unrealengine.com/t/wateradvanced-shallowwaterrivercomponent-no-valid-sinks-warning-and-river-disappears-in-5-8-3/2840450)) | Rhône et Saône (spline de rivière, vagues, flottaison, nage plus tard) | Moyen (Single Layer Water + réflexions Lumen) ; éviter la simulation de fluide | **À activer**, sans simulation |
| Procedural Vegetation Editor (moteur) | [doc Epic](https://dev.epicgames.com/documentation/unreal-engine/procedural-vegetation-editor-in-unreal-engine) | Moteur | 0 | Expérimental ; les fichiers PVE 5.7 ne s'ouvrent pas en 5.8 | Faire nos propres platanes (nœud Grower) ; sortie Nanite Foliage + vent | Inconnu | **À tester** avec les Megaplants |
| City Sample (5.8) | [fab.com/s/5e8f5eda64d8](https://fab.com/s/5e8f5eda64d8) | Gratuit, contenu limité à Unreal | Très lourd (plusieurs dizaines de Go) | Mis à jour pour 5.8 (graphes PCG de bâtiments) | Étude PCG de ville ; style américain | Élevé | **Plus tard**, étude seulement |
| City Sample Vehicles | [fab.com/s/62cf08ebaf2e](https://fab.com/s/62cf08ebaf2e) | Gratuit, limité à Unreal | Non affichée | Unreal | 13 véhicules américains conduisibles | Moyen | **Non** : notre parc est français |
| Electric Dreams | [doc Epic](https://dev.epicgames.com/documentation/en-us/unreal-engine/electric-dreams-environment-in-unreal-engine) | Gratuit, Unreal | Lourd | Exige DirectX 12, 32 Go de RAM : pas pour notre Mac | Nature, falaises, ruisseau | Élevé | **Non** |
| PCG Biome Core et Sample | [doc Epic](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-overview-guide-in-unreal-engine) | Moteur | 0 | Expérimental | Outil de placement, aucun asset urbain | Faible | **Non** pour l'instant |
| Paragon, Infinity Blade | [contenu gratuit Epic](https://dev.epicgames.com/documentation/en-us/unreal-engine/free-epic-games-content-for-unreal-engine) | Gratuit, Unreal | Lourd | Variable | Fantasy et science-fiction | — | **Non** : hors style |
| Gratuit pour une durée limitée | [fab.com/limited-time-free](https://www.fab.com/limited-time-free) | Fab | — | — | Jusqu'au 17/11 : Underground Tunnel, SciFi Beast, kit platformer | — | **Rien d'utile** ce mois-ci ; à revoir toutes les deux semaines |

Payants vus en passant, pour mémoire : Paris Dauphine Street (249,99 $, UE 5.4,
avenue parisienne complète), Street Prop Pack Nanite (39,99 $, 42 objets de
rue génériques), Torino Street Props (3,99 $, pas de format Unreal).

### 2.2 Sources libres (aucun compte, les fils téléchargent seuls)

| Source | Lien | Licence | Contenu vérifié | Couvre | Coût M1 estimé | Verdict |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| Poly Haven | [polyhaven.com](https://polyhaven.com), [licence](https://polyhaven.com/license) | CC0, sans attribution, redistribution permise | 521 modèles, 869 textures, 997 HDRI. Exemples utiles : `street_lamp_01/02`, `metal_trash_can`, `water_manhole_cover`, `concrete_road_barrier(_02)`, `modular_street_seating`, `outdoor_table_chair_set_01`, `bar_chair_round_01`, `planter_box_01..03`, `potted_plant_01/02/04`, `trashbag`, `cardboard_box_01`, `plastic_crate_02/03` ; textures `cobblestone_*` (32), `asphalt_*` (22), enduits (73), `granite_tile_*` | Terrasses de café, déchets, plantes en pot, HDRI de look-dev | Faible (objets légers, passent par notre `prepare_props.py`) | **À prendre** |
| ambientCG | [ambientcg.com](https://ambientcg.com), [licence](https://docs.ambientcg.com/license/) | CC0, sans attribution | 2 041 matériaux, dont 151 asphaltes, 39 enduits, 31 façades, catégorie Paving Stones | Variantes de sol et d'enduits | Nul (textures) | **À prendre** au besoin |
| Sketchfab | [sketchfab.com](https://sketchfab.com) | Au cas par cas (CC0, CC BY avec crédit, CC BY-NC…) | Qualité très variable | Objets introuvables ailleurs | Variable | **Au cas par cas**, compte gratuit requis ; CC0 et CC BY seulement |
| Quaternius | [quaternius.com](https://quaternius.com) | Non vérifiée | Packs « stylized » / low poly | — | — | **Non** : hors style photoréaliste |

## 3. Ce que Nico doit cliquer (compte Epic), pas à pas

Une seule séance d'environ 15 minutes sur le Mac. Tout est gratuit, aucune
carte bancaire.

1. Ouvrir [fab.com](https://www.fab.com) et se connecter avec le compte Epic.
2. Ouvrir chaque fiche ci-dessous et cliquer **« Ajouter à ma bibliothèque »**
   (rien n'est téléchargé à ce stade) :
   1. [Megaplants : Norway Maple Forest](https://www.fab.com/listings/e789ad04-cf79-4973-adb0-85c794d77144)
   2. [Megaplants : Norway Maple Saplings](https://www.fab.com/listings/e79a64ce-2291-4505-ba8e-d2fdf20791e8)
   3. [European Beech](https://fab.com/s/5db64af5b801)
   4. [European Black Alder](https://fab.com/s/8f6f8b0282d1)
3. Megascans pour la rue : sur la [page du vendeur Quixel](https://www.fab.com/sellers/Quixel%20Megascans),
   chercher puis ajouter à la bibliothèque ce qui sort pour ces mots
   (prendre 3 à 5 résultats par mot, ceux qui ressemblent à la Presqu'île) :
   `granite cobblestone`, `granite curb`, `asphalt`, `road marking`,
   `manhole`, `drain`, `crack decal`, `limestone`, `plaster`, `cigarette`,
   `leaves decal`, `traffic cone`, `road barrier`, `trash bag`, `cardboard`.
   Les fiches restantes deviennent gratuites d'ici le 12/11 : si un prix
   s'affiche encore, réessayer le 13/11.
4. Dire « c'est fait » dans le fil : un fil importe ensuite depuis la
   bibliothèque Fab dans le projet (fenêtre Fab de l'éditeur, « Ajouter au
   projet »), éditeur et session Epic ouverts sur le Mac, hors Git, avec une
   ligne par asset dans `SOURCES.md`.

Rien à cliquer pour l'eau (plugin moteur) ni pour Poly Haven / ambientCG.

## 4. Ce que les fils font seuls

- Récupérer par script les modèles et textures Poly Haven et ambientCG
  listés plus haut, les faire passer par `tools/assets` (normalisation,
  plafonds de triangles, LOD M1), hors Git.
- Activer le plugin Water et poser une première Saône/Rhône par spline sur
  la carte de la tranche, sans simulation de fluide ; mesurer le coût M1.
- Une fois les Megaplants dans la bibliothèque : remettre des feuilles aux
  112 arbres de la rue avec l'érable plane (feuillage d'automne, saison de
  la bible), mesure M1 avant/après. Si le résultat tient le budget, proposer
  à Nico l'achat des platanes Greenleaf (29,99 $) par carte.
- Croiser le catalogue Tripo restant avec Megascans et Poly Haven et retirer
  du catalogue ce qui existe déjà gratuitement.

## 5. Règles de licence rappelées

- Fab Standard (Megascans) : usage dans un projet public non commercial
  permis, pas d'attribution, **jamais de fichiers d'asset dans le dépôt
  public**. Contenu « UE-only » (City Sample) : seulement dans Unreal.
- CC0 (Poly Haven, ambientCG) : aucune contrainte ; on les garde quand même
  hors Git pour la taille.
- CC BY (Sketchfab) : crédit obligatoire dans `SOURCES.md` et dans les
  crédits du jeu.
- Toute marque ou visage réel reste soumis à la bible (§ 3.5) et aux règles
  de travail.

## 6. Sources

- Megascans gratuit : [CG Channel, 2026-11-06](https://www.cgchannel.com/2026/11/epic-has-made-megascans-free-again-for-good-this-time/), [Creative Bloq](https://creativebloq.com/news/the-worlds-biggest-library-of-scanned-assets-for-3d-artists-is-now-free-and-it-seems-that-this-time-its-for-good)
- Licence Fab : [fab.com/eula](https://www.fab.com/eula)
- Fiches Fab ouvertes le 2026-11-10 : voir les liens du tableau 2.1
- Plugin Water : [doc](https://dev.epicgames.com/documentation/unreal-engine/water-system-in-unreal-engine), fils de forum cités dans le tableau
- PVE : [doc](https://dev.epicgames.com/documentation/unreal-engine/procedural-vegetation-editor-in-unreal-engine), [annonce Greenleaf](https://forums.unrealengine.com/t/greenleaf-vision-pve-procedural-vegetation-broadleaf-park-tree-presets/2740042)
- Poly Haven : [licence](https://polyhaven.com/license), API `api.polyhaven.com/assets`
- ambientCG : [licence](https://docs.ambientcg.com/license/), API `ambientcg.com/api/v2/full_json`
