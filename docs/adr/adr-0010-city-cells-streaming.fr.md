# ADR-0010 : charger le quartier par cellules

- Statut : proposé (2026-11-10), à accepter après la mesure M1 du fil optimisation
- Responsables : génération de la ville (Ville v2), optimisation, streaming du monde
- Liens : ADR-0002 (budget de performance), roadmap 0.9 « Tout le quartier »

## Contexte

La 0.9 demande Bellecour → Terreaux, les deux quais et les ponts, sans écran de chargement,
dans le budget M1 (ADR-0002 : plancher 25 i/s, cible 30 i/s, ≤ 9 Go). Une seule rue (la rue
de la République prolongée jusqu'au Rhône) prend déjà tout le budget GPU sur M1 : 21 299
instances de façade, environ 3 400 objets de mobilier et détails, 65 % des images sous 33 ms.
Le quartier entier, découpé en 30 cellules de 250 m, compte 2 156 bâtiments et 174 185
modules de façade : environ huit fois la rue. Il ne
peut pas être un seul niveau chargé d'un bloc.

Deux contraintes du projet :

1. **La map reste régénérable en une commande.** Rien n'est posé à la main : chaque acteur
   sort de données rejouables (tools/city/regen_city.sh). Une cellule doit pouvoir être
   régénérée seule, sans toucher aux autres.
2. **Sans interface et sur un seul M1 de 16 Go.** Chaque étape tourne par script sur le Mac de
   Nico, un seul Unreal à la fois, sans session d'éditeur interactive.

Côté données, c'est prêt : `tools/city/cells.py` découpe le quartier en cellules de 250 m sur
une grille ancrée à l'origine de la zone (des noms comme `cell-p0000-m0250` ne changent jamais
quand le quartier grandit) et génère chacune hors moteur (bâtiments, modules de façade,
enseignes, masque du sol sur les chaussées recalées sur les bordures) dans `gen/<cellule>/`,
avec la même organisation qu'une rue.

## Options

**A. World Partition** (un seul monde, grille d'exécution, HLOD, data layers).
Streaming et HLOD natifs. Mais le monde est un seul asset avec un fichier par acteur :
régénérer une cellule oblige à retrouver et supprimer ses acteurs parmi des dizaines de
milliers, puis à reconstruire le HLOD de toute la grille (WorldPartitionHLODsBuilder, long et
gourmand en mémoire sur 16 Go). La pose en Python dans une map World Partition fonctionne mais
exige que les cellules soient chargées dans l'éditeur. Le HLOD des façades instanciées et des
vitrines translucides est difficile à maîtriser.

**B. Un niveau en streaming par cellule** (niveau persistant + sous-niveaux `L_City_<cellule>`,
chargés et déchargés selon la distance).
Chaque cellule est son propre niveau, écrit par les mêmes scripts qu'aujourd'hui
(`import_city.py`, `place_facades.py`, `place_props.py`, `apply_city_materials.py`, avec la
cellule comme focus) : régénérer une cellule réécrit un seul fichier. Le niveau persistant
garde le terrain, l'éclairage, le ciel, le fleuve et la vue lointaine : un substitut bon marché
par cellule (les coques générées, déjà produites en `shell.glb`, aux couleurs des bâtiments),
toujours chargé. Un petit sous-système C++ charge les cellules dans un rayon autour du joueur
(`ULevelStreamingDynamic`) et cache leur substitut quand la cellule détaillée est visible. Pas
de construction de HLOD par le moteur.

**C. Level Instances / Packed Level Actors** posés dans un monde World Partition.
Combine A et B, mais hérite des contraintes de construction et d'éditeur de World Partition ;
aucun gain à notre échelle.

## Décision (proposée)

**B : un niveau en streaming par cellule, avec un substitut lointain généré par cellule**,
pour la 0.9. On garde la régénération en une commande par cellule, tout tourne sans interface
et le coût est prévisible : au plus les 3 × 3 cellules autour du joueur en détail (environ
750 m de côté, plus que la distance de vue de la rue avec les tables de culling de
`place_facades.py` / `place_props.py`), le reste en substituts.

À mesurer par le fil optimisation avant acceptation :

- GPU et mémoire de 9 cellules détaillées + tous les substituts sur M1 (cible de l'ADR-0002) ;
- les à-coups de chargement quand une cellule arrive en conduisant (50 km/h ≈ 14 m/s : une
  cellule de 250 m toutes les ~18 s) ; le chargement asynchrone doit rester sous une image
  d'à-coup ;
- si 250 m est la bonne taille de cellule (plus petites : à-coups plus courts, plus de niveaux).

On revient sur World Partition (A) si les à-coups ne tiennent pas ou si le quartier dépasse
ce que des niveaux par cellule permettent de gérer.

## Conséquences

- `regen_city.sh` prend un argument de cellule ; par défaut, il construit toujours la map de
  la rue actuelle.
- Nouveau C++ : un sous-système de streaming des cellules (chargement par distance, échange
  des substituts), confié au fil streaming du monde ; la liste des cellules vient de
  `processed/<zone>/cells.json`.
- Voies de trafic, piétons et police ont besoin de données entre cellules : les voies restent
  à l'échelle de la zone (un seul `lanes.csv`), les acteurs sont gardés ou recyclés quand leur
  cellule se décharge.
- La map de rue `L_City_RueDeLaRepublique` reste la map de test jusqu'à la 0.9.
