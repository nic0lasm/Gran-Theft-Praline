# ADR-0004 : Cycle jour/nuit et météo pilotés par code

*Version anglaise : [adr-0004-day-night-weather.md](adr-0004-day-night-weather.md)*

## Statut

Proposé. Passe à Accepté quand les sept ambiances de
`tools/environment/measure-ambiences-mac.sh` tiennent le budget M1 sur le
MacBook Pro M1 Pro (p95 ≤ 42 ms, nuit ≤ 3 ms de plus que le jour, pluie
≤ 1 ms). Le même passage clôt le dernier point ouvert de l'ADR-0001
(exposition de l'heure dorée et de la nuit).

## Date

2026-11-10

## Contexte

La 0.2 est figée à midi : soleil fixe, exposition bloquée à EV100 14
(bf8acf1) parce que l'adaptation automatique éclaircissait le sol sombre.
La bible demande un soleil physique à 45,76° N, quatre états (midi, heure
dorée, pluie, nuit) avec leurs plages d'exposition, une nuit noire avec des
flaques de lumière, la pluie et le sol mouillé, le brouillard sur les
fleuves. La map est un binaire partagé par plusieurs chantiers, le budget
M1 est tendu (ADR-0002) et MegaLights n'existe pas sur le Mac.

## Décision

1. Un sous-système de monde C++ (`UGTPEnvironmentSubsystem`) pilote à
   l'exécution le soleil, la lune, le ciel, les nuages, le brouillard,
   l'exposition et les lumières de nuit, sans modifier aucune map ; ses
   réglages sont dans `DefaultGame.ini`.
2. Position du soleil et de la lune calculée (NOAA, série lunaire), heure
   légale de Lyon ; le soleil tourne par pas de 0,1° (cache des ombres
   virtuelles).
3. Exposition par histogramme à plage étroite (± 0,5 EV) autour d'une cible
   EV100 fonction de la hauteur du soleil et de la météo, sur un composant de
   post-traitement de priorité 1000 ; la fixation EV100 14 de la map reste le
   repli.
4. Lumières de nuit générées depuis les données de la ville et gérées par un
   budget par profil : seules les plus proches sont allumées (m1 : 64 dont 4
   avec ombres) ; brouillard volumétrique désactivé en m1.
5. Météo par états fondus ; mouillé et émissifs de nuit par une Material
   Parameter Collection (`MPC_Environment`) que les matériaux lisent.
6. Gratuit d'abord : ciel, nuages et brouillard natifs d'Unreal, textures
   procédurales, aucun achat.

## Conséquences

- Positif : toutes les maps de rue profitent du cycle sans édition ; chaque
  ambiance se reproduit et se mesure par la ligne de commande ; le coût de la
  nuit est plafonné par le budget de lumières.
- Négatif : les matériaux de rue sont modifiés par un script après chaque
  régénération de la ville ; les lumières suivent les positions
  OpenStreetMap (fichier hors Git) ; la pluie est une couche simple sans
  éclaboussures.
- Les fils ville et réalisme rue multiplient leurs émissifs par
  `EmissiveScale` ; les cvars du profil M1 restent au fil optimisation.

## Options écartées

- **Une map par état** (comme la scène témoin) : pas de transitions,
  quatre binaires à tenir à jour.
- **Ultra Dynamic Sky ou autre ciel payant** : contraire à « gratuit
  d'abord », le natif suffit.
- **Lumières de nuit toutes allumées** : 423 lumières sur la seule rue de la
  République, hors budget M1.
- **Pluie en particules Niagara** : plus coûteuse et non scriptable sans
  éditeur ; reportée à un passage effets.
