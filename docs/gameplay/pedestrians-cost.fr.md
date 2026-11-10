# Coût des piétons et montée en charge de la foule

*Version anglaise : [pedestrians-cost.md](pedestrians-cost.md)*

État : **aucune mesure n'existe encore pour les piétons** (rien n'était compilé
dans Unreal à l'écriture). La seule référence est la 0.3 : 6 conducteurs IA, chacun
un personnage complet avec son Blueprint d'animation, coûtent environ 10 ms de GPU
sur le profil M1. Un piéton d'ici est bien plus léger (une animation en boucle, pas
de Blueprint d'animation, pas de mouvement de personnage, pas de physique), mais
de combien, c'est la première chose à mesurer. Les gains ci-dessous sont des
hypothèses classées par ordre de grandeur attendu, à confirmer ou écarter par le banc.

## D'où vient le coût d'une personne dessinée

| Poste | Ce qui le fait varier | Levier |
|---|---|---|
| GPU, géométrie et ombrage | triangles du LOD dessiné, emplacements de matériaux, résolution (m1 = 50 %) | LOD forcé avec la distance, moins d'emplacements, moins de gens dessinés |
| GPU, ombres | un maillage animé qui bouge fait redessiner des pages de shadow map virtuelles à chaque image | pas d'ombre au-delà de 25 m (fait), pas d'ombre de contact (interrupteur) |
| GPU, éclairage | contribution GI dynamique et champs de distance d'un maillage animé | coupée pour les piétons (interrupteur) |
| CPU, animation | évaluation de la pose et mise à jour du skinning, à chaque image, par personne | cadence d'animation selon la distance (interrupteur), LOD de squelette, corps cachés non posés (fait) |
| CPU, thread jeu | déplacement sur le cheminement, tir au sol, écartement | mises à jour à 5 Hz hors champ (interrupteur), tirs en tourniquet (fait) |

## Leviers, dans l'ordre où les essayer

| N° | Levier | État | Gain attendu (hypothèse) | Interrupteur |
|---|---|---|---|---|
| 1 | Moins de dessinés (`MaxVisible` 10, 6, 20) | fait | linéaire en gens dessinés : le premier et le plus sûr | `-PedMaxVisible=N` |
| 2 | LOD de maillage réduit et pas d'ombre avec la distance | fait | fort par personne lointaine, nul pour les proches | `FarLOD`, `DetailDistance` |
| 3 | Cadence d'animation selon la distance (1/20 s, 1/12 s, 1/8 s) | fait, actif | CPU : divise par deux le coût de pose des lointains | `-PedAnimRate=0/1` |
| 4 | Drapeaux de rendu économes (pas d'ombre de contact, décalques, occlusion, GI, flou par os ; bornes fixes) | fait, actif | GPU : petit par personne, mais s'additionne | `-PedCheap=0/1` |
| 5 | Mises à jour lointaines à 5 Hz | fait, actif | thread jeu : les gens cachés presque gratuits | `-PedSpread=0/1` |
| 6 | Apparition hors champ, rien au-delà de 90 m | fait | évite l'apparition à vue ; garde le pool petit | `DrawDistance` |
| 7 | LOD de squelette : retirer doigts, os de torsion et IK dès le LOD 1 | à faire (éditeur) | CPU : le coût du squelette baisse avec le nombre d'os | réglage d'asset |
| 8 | Foule lointaine en instances à texture d'animation de sommets (un appel pour beaucoup) | à faire, étape d'asset par maillage | ce qui rend 100+ possible ; pas de réaction aux chocs, donc seulement au-delà de 25 m | nouveau |
| 9 | Sprites imposteurs au-delà de 60 m | à faire | foule lointaine la moins chère ; une capture par maillage | nouveau |
| 10 | Représentation de foule Mass | alternative | pensée pour des milliers ; plugins lourds, graphe de zones dans la carte | voir ADR-0005 |

Les leviers 1 à 6 sont dans le code derrière des interrupteurs ; 7 à 9 demandent
du travail d'éditeur et une mesure montrant qu'ils sont nécessaires. De 12 à une
foule crédible de 30 à 60, le chemin probable : leviers 1 à 5 pour savoir combien
de gens dessinés le budget porte (1,5 ms GPU, 0,5 ms CPU), puis le levier 8 pour
tous les autres, de sorte que la rue paraisse peuplée alors que seuls les 10 plus
proches sont de vrais personnages.

## Mesurer

```
caffeinate -dimsu tools/pedestrians/bench-crowd-mac.sh <version> [duree_s]
```

Lance la build m1 empaquetée avec 0, 12, 30 et 60 personnes, puis à 30 chaque
levier coupé tour à tour et `MaxVisible` à 6 et à 20, 180 s chacun par défaut, et
écrit `Builds/<version>/evidence/crowd-<horodatage>/SUMMARY.md`. Coût d'une
personne = temps d'image avec N moins avec 0, divisé par les gens dessinés (lignes
`LogGTPPedestrians: active= drawn=`). Ne demande que la build empaquetée, pas
d'éditeur.

Les nouveaux comportements sont gelés jusqu'à ces chiffres.

## Couches pour 50 puis 100+ personnes (palier C et après)

La foule est construite en couches, chacune moins chère que la précédente, de
sorte que la qualité près du joueur ne baisse jamais :

| Couche | Distance | Simulation | Dessin | État |
|---|---|---|---|---|
| 1 Personnage complet | < 20 m | à chaque image, réactions, évitement, couples | maillage squelettique, animation propre, ombre | fait |
| 2 Personnage réduit | 20-90 m | 5 Hz hors champ ; animation 1/20-1/8 s | maillage squelettique, LOD réduit, sans ombre | fait |
| 3 Foule lointaine | 45-250 m | données seules, 5 Hz, pas d'acteur | instances d'un maillage statique cuit, un appel de dessin | dans le code, coupée tant qu'aucun maillage n'est réglé |
| 4 Imposteurs / animation de sommets | au-delà de 60 m | comme la couche 3 | billboards animés ou textures d'animation de sommets | à faire, après le banc |

La couche 3 compte `FarCrowdCount` personnes (`-PedFar=N`) dessinées en instances
de `FarCrowdMesh` (un maillage statique d'une personne en pose de marche : dans
l'éditeur, Créer un maillage statique depuis la pose du maillage squelettique).
Une personne lointaine qui passe sous `PromoteDistance` (40 m) est confiée au
pool et devient un personnage complet ; les lointains apparaissent hors champ et
sont recyclés au-delà de `FarMaxDistance`. La hauteur du sol est celle du joueur
(les lointains sont petits ; la vraie hauteur viendra plus tard). Sans maillage
réglé, la couche est coupée et ne coûte rien.

## Budget (fil performance, 2026-11-10)

Cible : 20 personnes dessinées au plus à 2 ms de GPU et 1 ms de CPU ; ombres
seulement à moins de 20 m, cadence d'animation réduite au-delà de 25 m,
imposteurs ou foule instanciée au-delà de 40 m. Le cas `n40-vis20` du banc est
cette cible. Le juge est `tools/perf/bench.sh` ; le banc de foule sert à la
comparaison rapide par levier.
