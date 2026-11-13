# Démo de poursuite policière, 13/11/2026

Cinq images de la poursuite scriptée (`-PoliceChaseDemo -TestVehicle -Traffic=6 -TrafficLights=0`), Windows, 1280x720.
Logos de voiture, plaques, enseignes et logos sont floutés.

Prise : code de poursuite de `feat/police-hunt-2` fc71466 (même code que `feat/police-hunt-3` f48c06d, rejoué sur `next` b28b614), éditeur `-game`, ancienne carte.

| Image | Seconde de la vidéo | Ce qu'on voit |
|---|---|---|
| `01-car-taken-2-stars.jpg` | 1 | Le joueur conduit la voiture prise sur le quai Jean Moulin, 2 étoiles, 29 km/h |
| `02-3-stars-police-car-across.jpg` | 24 | 3 étoiles, une voiture en travers devant, le joueur ralentit à 11 km/h |
| `03-flight-40kmh.jpg` | 52 | Le joueur a semé la police (seen=0) et repart à 40 km/h |
| `04-search-stars-greyed.jpg` | 70 | Phase de recherche : étoiles grisées, le joueur à 48 km/h |
| `05-level-lost.jpg` | 96 | `wanted: level lost` à 94 s : plus aucune étoile |

Chronologie du journal : 2 étoiles à 2 s, police en vue dès 7 s, 3 étoiles à 22 s, bloqué puis dégagé à 37 s, police semée à 52 s, phase de recherche à 72 s, niveau perdu à 94 s ; les unités de police ont roulé entre 44 et 72 km/h.

Limites connues : vitesse maximale 46 km/h (les 70 à 85 km/h prévus ne sont pas atteints), le joueur heurte encore des obstacles trois ou quatre fois.
Le package de f48c06d (Development) tourne 120 s sans plantage avec piétons, trafic et police ; sa démo de poursuite garde 3 étoiles jusqu'à la fin de la capture (pas de niveau perdu).
