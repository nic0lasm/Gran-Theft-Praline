# Tableau de bord : où en est le jeu

*Version anglaise : [roadmap-status.md](roadmap-status.md). Roadmap : [roadmap.fr.md](roadmap.fr.md). Systèmes : [systems-inventory.fr.md](systems-inventory.fr.md).*
*Mis à jour à chaque jalon et à chaque bilan. Dernière mise à jour : 2026-11-21 (`next` = `main` = candidat 52, porte du candidat 54 en cours).*

Légende : ✅ prouvé dans le jeu packagé · 🟡 en partie · ⬜ pas fait · ❓ pas mesuré, on ne sait pas · ❄️ gelé

## Cible en cours : la tranche rue de la République, en trois jalons

300 à 400 m de Bellecour aux Jacobins, jugés sur PC. Critères complets :
[roadmap §3](roadmap.fr.md#3-la-cible--la-tranche-en-trois-jalons).

| Jalon | Visé | Statut | Dernière preuve |
| ---- | ---- | ---- | ---- |
| A. Voler une voiture et fuir la police | Fin novembre 2026 | 🟡 vol et témoin prouvés ; fuite de 251 m à 70 km/h en pointe (visé 105 à 135) ; unités de police à 17 km/h pour 25 ; trafic cassé (voitures couchées) ; vidéo pas tournée | candidat 52, 20 nov. |
| B. Les 10 minutes | Mi-décembre 2026 | ⬜ vidéo pas tournée ; réactions des piétons, feux, arrestation, mort et réapparition ❓ | |
| C. Les 5 paires photo / capture | Fin janvier 2027 | 🟡 une seule planche : 1/5 sur deux vues, 0 sur trois ; rien de mesuré depuis | candidat 17b, 12 nov. |
| Package PC sans plantage (chaque porte) | Permanent | ✅ douze portes de suite sans plantage | candidats 40 à 52, du 15 au 20 nov. |

## Les portes depuis la version 4

| Candidat | Date | Parcours automatique | Logique de jeu, médiane | Trafic en mouvement | Ce qu'il apporte |
| ---- | ---- | ---- | ---- | ---- | ---- |
| 40 | 15 nov. | 6/7 | 11,6 ms | 98 | Première porte sur le disque dédié |
| 41 | 16 nov. | 7/7 | 12,5 ms | 104 | Matériau des têtes de la foule corrigé |
| 42 | 16 nov. | 7/7 | 10,8 ms | 94 | Nicolito joueur par défaut |
| 43 | 16 nov. | 7/7 | 11,8 ms | 100 | Foule City Sample par défaut ; poursuite de 150 m exigée |
| 45 | 16 nov. | 7/7 | 11,3 ms | 103 | Roues et différentiel d'origine ; pointe à 70 km/h |
| 46 | 16 nov. | 7/7 | 11,8 ms | 103 | Fuite de 285 m, pointe à 73 km/h |
| 47 | 19 nov. | 7/7 | 12,9 ms | 50 | Échec sur le trafic ; os des roues, bornes hors chaussée |
| 48 | 19 nov. | 7/7 | 12,6 ms | 95 | Physique du trafic gardée comme au 46 ; poursuite de 340 m, pointe à 84 km/h |
| 49 | 19 nov. | 6/7 | 12,2 ms | 97 | Mobilier de rue retiré des voies de circulation |
| 50 | 19 nov. | 7/7 | 13,4 ms | 103 | Chargeur sur le HUD de combat ; le contrôle du témoin échoue |
| 51 | 20 nov. | 7/7 | 12,8 ms | 95 | Nouveau contrôle de la poursuite par la police |
| 52 | 20 nov. | 7/7 | 13,0 ms | **50** debout | Le comptage n'inclut plus les voitures couchées : le trafic est cassé ; témoin réparé |
| 53, 54 | en test | | | | Vraies roues et physique propre à City Sample pour le trafic, apparition de la police, sonde de caméra du braquage |

Jusqu'au candidat 51, « trafic en mouvement » comptait aussi les voitures
couchées sur le flanc qui glissent : avec le comptage honnête, le 52 a
49,7 voitures debout en mouvement, et 71 à 100 % des voitures à moins de
100 m du joueur sont couchées.

Seuils de la porte : logique de jeu, médiane au plus 1,3 fois celle de la
porte précédente ; parcours automatique au moins 6 sur 7 ; à partir du candidat 53, au moins
75,6 voitures debout en mouvement en moyenne de 120 à 300 s et au plus
5 % de voitures couchées à moins de 100 m ; aucun plantage.

## Les systèmes de la priorité 1

| Système | Mesuré sur le PC | Ce qui manque |
| ---- | ---- | ---- |
| Marche, course, caméra | ✅ joueur Nicolito, marche arme en main testée | Essai manette en main |
| Voler une voiture | ✅ en circulation et garée, alarme | Essai manette en main |
| Conduite | 🟡 0 à 50 km/h en 4,4 s, 71 km/h en pointe (essai de lancement, candidat 46) ; correctif des roues dans le 47 | 0 à 100, pointe de 105 à 135, caisse qui ne touche plus la route |
| Trafic | ❌ une cinquantaine de voitures debout en mouvement, la plupart des voitures proches couchées (candidat 52) | Un trafic debout, puis feux et évitement ❓ |
| Piétons | 🟡 foule City Sample, 24 personnages proches ; mains en l'air mesurées ; un témoin appelle la police (un modèle de voiture) | Animations de peur, témoin avec toutes les voitures ❓ |
| Police, étoiles | 🟡 3 étoiles, 5 unités ; 3 contrôles de police sur 4 | Vitesse de poursuite (17 km/h pour 25), semer la police ❓, arrestation ❓ |
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
