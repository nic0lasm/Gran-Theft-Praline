# Tableau de bord : où en est le jeu

*Version anglaise : [roadmap-status.md](roadmap-status.md). Roadmap : [roadmap.fr.md](roadmap.fr.md). Systèmes : [systems-inventory.fr.md](systems-inventory.fr.md).*
*Mis à jour à chaque jalon et à chaque bilan. Dernière mise à jour : 2026-11-24 (`next` = `main` = candidat 76 ; roadmap version 6).*

Légende : ✅ prouvé dans le jeu packagé · 🟡 en partie · ⬜ pas fait · ❓ pas mesuré, on ne sait pas · ❄️ gelé

## Cible en cours : le jalon A sur le circuit de fuite

Critères complets : [roadmap §3](roadmap.fr.md#3-ce-qui-change) et
[§4](roadmap.fr.md#4-les-jalons).

| Jalon | Visé | Statut | Dernière preuve |
| ---- | ---- | ---- | ---- |
| A1. Le circuit tient (points 1 à 6 de la grille) | 27 novembre 2026 | 🟡 3 points sur 6 ; le circuit n'est pas encore choisi | candidat 76, 24 nov. |
| A2. Ce que la caméra voit (points 7 à 9) | 1er décembre 2026 | ⬜ 0 sur 3 | candidat 76, 24 nov. |
| A3. La vidéo de 3 minutes, puis l'essai manette en main (point 10) | Vers le 4 décembre 2026 | ⬜ | |
| B. Les 10 minutes | Fin décembre 2026, redaté au jalon A | ⬜ | |
| C. Les 5 paires photo / capture | Fin janvier 2027 | 🟡 une seule planche : 1/5 sur deux vues, 0 sur trois | candidat 17b, 12 nov. |
| Package PC sans plantage (chaque porte) | Permanent | ✅ aucun plantage aux portes 70 à 77 | 24 nov. |

## La grille de 10 points, candidat 76

Lue sur les journaux de la porte et sur les images de sa vidéo.

| # | Point | Candidat 76 | |
| ---- | ---- | ---- | ---- |
| 1 | Le vol en moins de 10 s | 6 s | ✅ |
| 2 | 50 km/h de moyenne, 90 en pointe | 35 de moyenne, 89 en pointe | ⬜ |
| 3 | Sans aide | 4 remises sur voie en 4 minutes filmées | ⬜ |
| 4 | Police à l'image, unités à 25 km/h | voiture de police reconnaissable ; unités à 17 km/h | 🟡 |
| 5 | 8 voitures en mouvement à moins de 150 m | critère de porte tenu | ✅ |
| 6 | Une fin lisible avant 120 s | arrestation à 63 s, sommation à l'écran | ✅ |
| 7 | La caméra tient | caméra sur le toit de la voiture à l'arrestation | ⬜ |
| 8 | Pas de vide | sol nu, blocs sans façade, bande bleue à l'horizon (60 s) | ⬜ |
| 9 | Des personnages entiers | crânes rasés | ⬜ |
| 10 | Fluide et propre | parcours automatique : 64,8 % des images sous 33 ms (99,8 % au candidat 77) | ⬜ |

**3 sur 10.**

## Les portes du 22 au 24 novembre

18 portes, 24 heures de PC, 81 minutes par porte en moyenne.

| Candidat | Verdict | Ce qui a décidé |
| ---- | ---- | ---- |
| 59 à 62 | passées (5 portes) | Depuis le 60, la porte juge le trafic du jeu par défaut |
| 64 | critères tenus | |
| 65 | échouée | Passe incomplète, 288 lignes d'erreur après l'habillage de la fuite |
| 66 | échouée | 288 lignes d'erreur |
| 67, 68, 69 | échouées | Le même shader de peinture de police non cuit (6 lignes d'erreur) ; 7,1 voitures pour 8 au 67 |
| 70, 71 | critères tenus | Peinture de police coupée, sol mat |
| 72 | critères tenus, refusée à la vidéo | Logique de jeu +49 %, mémoire vidéo saturée, taxi new-yorkais |
| 74 | échouée | 7,6 voitures en mouvement pour 8 |
| 75 | passée | Police blanche à bande bleue, reconnaissable |
| 76 | passée, `next` = `main` | Vol, fuite à 89 km/h en pointe, arrestation à 63 s |
| 77 | critères tenus, non promue | Une étape de préparation a retiré 14 modules de façade |

## Les portes du 15 au 21 novembre

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

Seuils de la porte à cette date : logique de jeu, médiane au plus 1,3 fois celle de la
porte précédente ; parcours automatique au moins 6 sur 7 ; à partir du candidat 53, au moins
75,6 voitures debout en mouvement en moyenne de 120 à 300 s et au plus
5 % de voitures couchées à moins de 100 m ; aucun plantage.

## Les systèmes de la priorité 1

| Système | Mesuré sur le PC | Ce qui manque |
| ---- | ---- | ---- |
| Marche, course, caméra | 🟡 joueur Nicolito ; caméra dans les murs et sur le toit des voitures | Caméra de poursuite, essai manette en main |
| Voler une voiture | ✅ en circulation, en 6 s (candidat 76) | Essai manette en main |
| Conduite | 🟡 0 à 100 km/h en 6,6 s, 102 km/h au bout de 134 m (essai de lancement, candidat 76) | Essai manette en main |
| Trafic | 🟡 trafic par défaut debout, 8 à 12 voitures en mouvement à moins de 150 m pendant la fuite | Feux et évitement ❓ |
| Piétons | 🟡 foule City Sample, passants qui fuient et lèvent les mains | Crânes rasés, corps sous les voitures |
| Police, étoiles | 🟡 3 étoiles, voiture reconnaissable, sommation, arrestation | Unités à 17 à 21 km/h pour 25 ; semer la police ❓ |
| Combat de base | 🟡 braquage et tir au pistolet testés | Mains nues ❓ |
| Mort et réapparition | ❓ | Pas relu pour ce bilan |

## Image

L'image avance en parallèle du jeu depuis le 23 novembre, sur ce que la
caméra voit pendant la fuite.

| Écart | État |
| ---- | ---- |
| Façades des rues de la fuite « en boîtes peintes » | Teinte pierre de Lyon et fenêtres en retrait dans `main` (candidat 76) ; la pierre se lit encore comme des planches à 89 km/h |
| Lointain vide (sol nu, bande bleue, blocs lisses) | Lot « lointain » écrit, pas encore vu en jeu ⬜ |
| Quai nu, trottoirs blancs | Albédo du sol plafonné ; ni arbres ni mobilier sur le quai ⬜ |
| Les 5 paires photo / capture | Une seule planche (12 nov.) ; la seconde est prise avec la vidéo du jalon A ⬜ |

## Défauts notés hors du couloir

À remplir à partir du choix du circuit : un défaut vu hors du couloir est
inscrit ici, pas corrigé avant le jalon A.

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
