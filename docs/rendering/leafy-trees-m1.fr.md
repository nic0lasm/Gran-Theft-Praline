# Platanes feuillus sur M1 : variantes, coût estimé, banc en une commande

Date : 2026-11-10. Fil packs gratuits, chantier performance (aucun nouvel asset, on rend abordable ce qui est déjà sur le Mac).

## Pourquoi les arbres sont nus aujourd'hui

Les 112 arbres de la rue (W14-63 platane adulte, W14-64 jeune arbre protégé) sont deux maillages instanciés (HISM) posés par `place_props.py`. Créneau du 10/11 16:31 UTC, éditeur `-game`, profil M1, map allégée : arbres nus p95 42,77 ms (p50 31,8, 10 à-coups > 100 ms). Le budget (p95 ≤ 42 ms, ADR-0002) est déjà plein : les feuilles doivent être payées par une économie ailleurs ou coûter presque rien.

## Ce qu'il y a sur le Mac (packs Fab gratuits, hors Git)

Taille des `.uasset` de `Geometry/SimpleWind` (indicateur grossier du nombre de triangles, le vrai décompte sort de l'audit du banc) :

| Maillage | Taille | Rôle possible |
| ---- | ---- | ---- |
| SM_EuropeanBeech_Field_01 | 227 Mo | hêtre isolé : trop lourd pour 112 |
| SM_EuropeanBeech_Forest_01..07 | 18 à 165 Mo | trop lourds |
| SM_EuropeanBeech_Forest_08 | 10 Mo | candidat adulte |
| SM_BlackAlder_Forest_06..09 | 1,8 à 2,9 Mo | candidats adultes les plus légers |
| SM_BlackAlder_Field_05 | 8,9 Mo | candidat adulte isolé |
| SM_BlackAlder_Sapling_01..06 | 0,4 à 1,8 Mo | W14-64 (jeune arbre) |
| SM_EuropeanBeech_Sapling_01 | 5,3 Mo | W14-64 |

Le banc choisit seul (`GTP_TREE_MESH=auto`) le maillage SimpleWind adulte le plus léger en triangles LOD 0 parmi ceux d'au moins 8 m, et le plus léger des jeunes arbres. Les packs ont des matériaux d'impostor (`Materials/Impostor`) : le dernier LOD est un billboard. Si l'audit montre Nanite activé, la variante légère force le maillage de repli (fallback), qui est de toute façon celui que dessine un GPU sans Nanite.

## Les trois variantes (une seule map, un seul package)

La map n'est jamais modifiée. `swap_street_trees.py` (mode `pick`, lecture seule) choisit les maillages feuillus et donne l'argument `-TreeSwap=<nu>~<feuillu>`. Au lancement, `UGTPStreetTreesSubsystem` crée un double feuillu des arbres nus (mêmes instances, sans collision : le tronc nu garde la collision) et retire du maillage nu les arbres montrés feuillus. Sans argument, la rue reste comme aujourd'hui. Les dossiers des maillages feuillus sont ajoutés à la cuisson le temps du package seulement (`DefaultGame.ini` remis à l'identique ensuite).

| Argument | Ce qui change |
| ---- | ---- |
| `-Trees=bare` | défaut, arbres nus |
| `-Trees=leafy` | maillage du pack tel quel (vent, ombres, tous les LOD) |
| `-Trees=light` | vent coupé au-delà de 30 m (`-TreeWindCm`), pages VSM non invalidées par le vent (`ShadowCacheInvalidationBehavior=Static`), LOD 0 sauté (MinLOD 1), arbres coupés au-delà de 200 m (`-TreeCullCm`), repli non Nanite |
| `-Trees=impostor` | tous les arbres sur le dernier LOD (billboard), sans vent : le plancher de coût |
| `-TreeCount=10` | seulement 10 arbres feuillus répartis le long de la rue, les autres restent nus |
| `-TreeShadows=0` | arbres feuillus sans ombre |

Le vent (World Position Offset) est le premier suspect : il déplace les sommets chaque image, donc invalide les pages d'ombre virtuelles (VSM) de chaque arbre à chaque image. Le couper au loin et marquer l'ombre comme statique supprime l'essentiel de ce coût.

## Coût estimé pour 112 arbres (avant mesure)

Estimation à confirmer par le banc, à partir des mesures du chantier (détails muraux, vitrines, nuages) et des tailles ci-dessus :

| Variante | p95 en plus (estimé) | Par arbre |
| ---- | ---- | ---- |
| leafy (pack brut) | +6 à +12 ms | 0,05 à 0,1 ms |
| light | +2 à +4 ms | 0,02 à 0,035 ms |
| impostor | +0,3 à +1 ms | < 0,01 ms |

Proposition de budget : 1,5 ms p95 pour les 112 arbres, soit 0,013 ms par arbre. Seule la variante impostor y entre sûrement ; la variante légère y entre si l'économie sur les nuages (ombre des nuages 8,6 ms) est faite. Le compromis visé : light à moins de 40 m, impostor au-delà (réglage `-TreeCullCm` et LOD de transition à ajuster après la première mesure).

## Banc en une commande (Mac, après le « Mac à toi » du coordinateur)

```
caffeinate -dimsu tools/perf/tree-bench-mac.sh perf-trees 90
```

Étapes : audit des packs (triangles par LOD, Nanite, textures > 2K) ; choix des maillages (map ouverte en lecture seule, md5 vérifié au début et à la fin) ; package Shipping avec les dossiers feuillus cuits ; puis `tools/perf/bench.sh` pour bare, light10, light, impostor, leafy. Règle du pilote : 10 arbres d'abord ; si light10 coûte plus de 2 ms (`MAX_DELTA_10_MS`) de plus que bare, les cas à 112 arbres sont sautés. Durée : environ 1 h au plus (audit et choix 12 min, package 15 à 30, bancs 18). Garde-temps : une étape au-delà du double de sa durée prévue est arrêtée, Unreal est fermé et le script sort (code 3) ; signe de vie toutes les 10 min ; essai à blanc `DRY_RUN=1`. Résultat : `Builds/perf-trees/trees-<date>/TREES.md` (p95 par cas, écart à bare, coût par arbre, maillages choisis) et `mesh_audit.csv`.

Pré-requis : dans le clone qui package, les dossiers `Content/EuropeanBeech`, `Content/BlackAlder` et `Content/MSPresets` (liens vers GTAL dans GTAL-build) et le code de cette branche compilé.

## Roadside Construction (même analyse)

`Content/Scene_RoadsideConstruction`, 4,5 Go, désormais hors Git : 70 maillages (2,7 Go) et 215 textures (2,05 Go, 28 au-delà de 20 Mo, donc 4K ou 8K). Un cône de chantier pèse 75 Mo, un bloc béton 134 Mo : ce sont des scans Nanite à plusieurs millions de triangles. Sur M1, si Nanite ne tourne pas, le moteur dessine leurs maillages de repli, dont la densité n'est pas maîtrisée.

Avant d'en placer un seul dans la rue :

1. l'audit du banc donne triangles et Nanite par maillage (`mesh_audit.csv`, même commande) ;
2. pour chaque objet retenu (cônes, barrières, blocs béton, plots, sacs de ciment, palissades), régler le repli Nanite à moins de 5 000 triangles ou créer 3 LOD (≈ 5 000 / 1 500 / 400 triangles), comme les budgets de `tools/assets/manifest.py` ;
3. plafonner les textures à 2048 (le profil M1 ne charge pas plus) ;
4. les placer en HISM par type, ombres coupées au-delà de 30 m, coupés au-delà de 80 m.

Script prêt : `tools/assets/unreal/prepare_roadside.py` (mode `list` par défaut, rapport seul ; `GTP_ROADSIDE_MODE=apply` coupe Nanite, reconstruit les 3 LOD depuis le scan et plafonne les textures à 2048, uniquement sur les assets du pack, sans toucher de map ; un nouveau téléchargement Fab rend le pack d'origine).

Rien de cela n'est fait tant que le chantier gèle les nouveaux assets : c'est la recette pour le jour où un fil en a besoin.
