# Tableau de bord : où en est le jeu

*Version anglaise : [roadmap-status.md](roadmap-status.md). Roadmap : [roadmap.fr.md](roadmap.fr.md). Systèmes : [systems-inventory.fr.md](systems-inventory.fr.md).*
*Mis à jour à chaque jalon et à chaque bilan. Dernière mise à jour : 2026-11-19 (`next` = `main` = candidat 46, porte du candidat 47 en cours).*

Légende : ✅ prouvé dans le jeu packagé · 🟡 en partie · ⬜ pas fait · ❓ pas mesuré, on ne sait pas · ❄️ gelé

## Cible en cours : la tranche rue de la République, en trois jalons

300 à 400 m de Bellecour aux Jacobins, jugés sur PC. Critères complets :
[roadmap §3](roadmap.fr.md#3-la-cible--la-tranche-en-trois-jalons).

| Jalon | Visé | Statut | Dernière preuve |
| ---- | ---- | ---- | ---- |
| A. Voler une voiture et fuir la police | Fin novembre 2026 | 🟡 vol prouvé ; fuite de 285 m à 73 km/h en pointe (visé 105 à 135) ; vitesse de la police ❓ ; aucun essai manette en main | candidat 46, 16 nov. |
| B. Les 10 minutes | Mi-décembre 2026 | ⬜ vidéo pas tournée ; réactions des piétons, feux, arrestation, mort et réapparition ❓ | |
| C. Les 5 paires photo / capture | Fin janvier 2027 | 🟡 une seule planche : 1/5 sur deux vues, 0 sur trois ; rien de mesuré depuis | candidat 17b, 12 nov. |
| Package PC sans plantage (chaque porte) | Permanent | ✅ six portes de suite sans plantage | candidats 40 à 46, 15 et 16 nov. |

## Les portes depuis la version 4

| Candidat | Date | Parcours automatique | Logique de jeu, médiane | Trafic en mouvement | Ce qu'il apporte |
| ---- | ---- | ---- | ---- | ---- | ---- |
| 40 | 15 nov. | 6/7 | 11,6 ms | 98 | Première porte sur le disque dédié |
| 41 | 16 nov. | 7/7 | 12,5 ms | 104 | Matériau des têtes de la foule corrigé |
| 42 | 16 nov. | 7/7 | 10,8 ms | 94 | Nicolito joueur par défaut |
| 43 | 16 nov. | 7/7 | 11,8 ms | 100 | Foule City Sample par défaut ; poursuite de 150 m exigée |
| 45 | 16 nov. | 7/7 | 11,3 ms | 103 | Roues et différentiel d'origine ; pointe à 70 km/h |
| 46 | 16 nov. | 7/7 | 11,8 ms | 103 | Fuite de 285 m, pointe à 73 km/h |
| 47 | en cours | | | | Roues à leur place, bornes hors chaussée |

Seuils de la porte : logique de jeu, médiane au plus 1,3 fois celle de la
porte précédente ; parcours automatique au moins 6 sur 7 ; trafic en
mouvement au moins 75,6 en moyenne de 120 à 300 s ; aucun plantage.

## Les systèmes de la priorité 1

| Système | Mesuré sur le PC | Ce qui manque |
| ---- | ---- | ---- |
| Marche, course, caméra | ✅ joueur Nicolito, marche arme en main testée | Essai manette en main |
| Voler une voiture | ✅ en circulation et garée, alarme | Essai manette en main |
| Conduite | 🟡 0 à 50 km/h en 4,4 s, 71 km/h en pointe (essai de lancement, candidat 46) ; correctif des roues dans le 47 | 0 à 100, pointe de 105 à 135, caisse qui ne touche plus la route |
| Trafic | 🟡 une centaine de voitures en mouvement | Feux et évitement ❓ |
| Piétons | 🟡 foule City Sample, 24 personnages proches | Mains en l'air lisibles, animations de peur, témoins ❓ |
| Police, étoiles | 🟡 3 étoiles, 5 unités | Vitesse de poursuite ❓, semer la police ❓, arrestation ❓ |
| Combat de base | 🟡 braquage et tir au pistolet testés | Mains nues ❓ |
| Mort et réapparition | ❓ | Test packagé |

## Image

| Écart de la première planche (12 nov.) | Depuis |
| ---- | ---- |
| Ombres noires | Profil ultra sur PC (13 nov.) ; pas de nouvelle planche ❓ |
| Carte vide au-delà de 45 m | Décor lointain prêt, jamais ouvert dans l'éditeur ⬜ |
| Sol en carrelage gris | ❓ |
| Vitrines et nuit noires | Vitrines et éclairage de nuit validés sur PC (13 nov.) |
| Joueur en mannequin | ✅ Nicolito (candidat 42) |
| Ni piétons ni voitures sur les vues | Foule et voitures City Sample en jeu ; pas de nouvelle planche ❓ |

Aucune capture n'est sortie du PC depuis le 13 novembre.

## Gelés

❄️ Missions · audio au-delà de l'existant · menus · intérieurs (fast-food en
maintenance) · sauvegarde de partie · mini-carte · remappage · crédits ·
build de test 0.9.

## Versions

| Version | Statut |
| ---- | ---- |
| 0.1 Jouer en un clic | ✅ 2026-11-09 |
| 0.2 La rue habillée | ✅ 2026-11-10 |
| 0.3 Au volant | ✅ 2026-11-10 (dernière version publique) |
| Prochaine version | ⬜ quand le jalon B est tenu |
