# Sensation de jeu à pied : caméra, aides, HUD, ambiance

*Version anglaise : [on-foot-feel.md](on-foot-feel.md).*
*Version 0.2. Mise à jour : 2026-11-09. État : code prêt, compilation et playtest sur le Mac à faire.*

## 1. Objectif

À pied, le jeu doit se lire comme les références du genre (action à la
troisième personne en monde ouvert, moderne ou western) : une caméra qui suit
avec du poids sans jamais lutter contre le joueur, une aide contextuelle
simple quand quelque chose peut être utilisé, un écran presque vide et une
rue qui sonne vivante. La locomotion elle-même (marche, course, sprint,
départs, arrêts, pivots, sauts, accroupi, enjambements) reste au motion
matching du Game Animation Sample ([player-character.fr.md](player-character.fr.md)),
qui donne déjà des transitions et des arrêts crédibles ; ce travail l'habille.

Tout ce qui suit est notre code C++ et notre config (MIT), aucun asset à créer
dans l'éditeur ; chaque valeur est dans `Config/DefaultGame.ini` et se règle
sans recompiler.

## 2. Caméra troisième personne (`UGTPFollowCameraComponent`)

Portée par le contrôleur du joueur ; elle pilote une caméra-acteur qui devient
la cible de vue. Les assets caméra du sample ne sont pas touchés :
`bEnabled=False` leur rend la vue.

| Comportement | Valeur (défaut) | Pourquoi |
| ---- | ---- | ---- |
| Pivot | 62 cm au-dessus du centre de la capsule (épaules) | Cadre tout le corps et la rue devant |
| Distance | 290 cm en marchant, +45 cm en plein sprint, -45 cm accroupi | La vitesse se lit à l'écran ; accroupi, la caméra se rapproche |
| Épaule | 38 cm à droite, 8 cm plus haut | Personnage un peu à gauche, la rue reste visible |
| Regard vers le haut | Bras réduit jusqu'à 55 % à +60° | La caméra ne s'enfonce pas dans le sol |
| Retard de position | 11/s à l'horizontale, 6/s à la verticale, 3,5/s en l'air, au plus 55 / 70 cm | Du poids aux départs, arrêts, sauts et marches, sans flottement |
| Retard de rotation | aucun | Souris et stick restent directs |
| Collision | Sphère de 14 cm balayée depuis les épaules (canal Camera) ; rentre aussitôt, ressort à 4/s | Jamais derrière un mur dans les rues étroites de la Presqu'île, sans à-coups |
| Trop près | Sous 45 cm le corps est masqué pour le joueur seulement (ombre gardée) | Collé à un mur, la vue ne montre pas l'intérieur de la tête |
| Champ de vision | 78° en marchant, 86° en plein sprint (fondu de 480 à 650 cm/s), 3/s | Sensation de vitesse |
| Recentrage auto | Après 2,5 s sans toucher la vue, en mouvement au-dessus de 120 cm/s et à moins de 100° : glisse derrière la direction de course, au plus 35°/s | Courir de côté fait une courbe douce ; marcher vers la caméra garde le visage visible |
| Recentrage manuel | Clic molette : derrière le personnage à -10° en 0,25 s | Se réorienter vite |

La première personne (V) ne change pas (caméra à la tête, 90°) ; la bascule
fond les deux caméras-acteurs en 0,2 s. En sortant d'une voiture, la vue
passe de la caméra de poursuite à la caméra à pied en 0,5 s.

## 3. Interaction contextuelle (E)

- `UGTPInteractableComponent` : à ajouter sur n'importe quel acteur (porte,
  comptoir, distributeur), à placer où va la main, avec `Prompt` (le verbe,
  par exemple « Entrer ») et l'événement `OnInteracted`. `Message` affiche un
  sous-titre à l'usage, en attendant que la vraie action existe (intérieurs
  des boutiques).
- `UGTPInteractionComponent` sur le contrôleur : dix fois par seconde, retient
  l'objet dans son rayon (180 cm), devant la vue (cosinus > 0,35) et sans
  obstacle entre les deux, le plus proche et le plus centré d'abord.
- Touches : E (croix directionnelle droite à la manette), dans le contexte
  d'interaction toujours actif construit en code (`UGTPVehicleInput`). Les
  véhicules gardent F (Y).
- Aides : « E  <verbe> » pour l'objet visé et « F  Monter » près d'une voiture.

## 4. HUD (`AGTPHUD`)

Dessiné sur le canevas, sans widget, caché quand un menu est ouvert :
- un point central de 2,5 px cerclé de sombre, opaque à 45 % en troisième
  personne et 80 % en première ;
- les aides en bas à droite, une ligne chacune : la touche dans un cadre
  clair, puis le verbe, sur un panneau sombre translucide, en fondu glissé
  (0,12 s) ;
- une ligne de sous-titre en bas au centre pour les messages courts.
Tailles données pour 1080 lignes, mises à l'échelle de la fenêtre.

## 5. Ambiance de rue

`tools/audio/make_street_ambience.py` synthétise une boucle stéréo de 120 s
sans raccord (48 kHz) : grondement lointain de la circulation qui respire
lentement, voitures, scooters et un bus de temps en temps qui passent avec
effet Doppler et déplacement stéréo, léger murmure de passants et quelques
moineaux. Aucun enregistrement : pas de question de licence, aucune dépense.
`tools/audio/import_ambience.py` l'importe en
`/Game/Sound/Ambience/SW_Amb_Street_Day` (en boucle, régénéré, non versionné) ;
le contrôleur la joue en 2D au volume 0,5 avec un fondu de 3 s. Les pas,
sauts et réceptions viennent des bruitages du sample.

Suite : sources positionnées (cafés, tram sur les quais, fontaines),
variante de nuit, couches CC0 enregistrées si le fond synthétique sonne
pauvre.

## 6. Contrôles (playtest)

`tools/character/playtest_character.sh` vérifie aussi :

| Contrôle | Réussite |
| ---- | ---- |
| Cadrage à l'arrêt | Champ 74-82°, caméra à 2 m au moins |
| Cadrage en sprint | Champ 82-90°, caméra à 2,4 m au moins |
| Arrêt après un sprint | Arrêt en 0,2-3 m et 1,2 s |
| Caméra jamais derrière un mur | Ligne épaules → caméra libre sur au moins 99 % des images |
| Recentrage auto | 6 s de course en diagonale tournent la vue de 15 à 200° |
| Vue troisième personne | Tête visible, caméra à plus de 1,2 m |

`GTP_CAPTURE=<secondes>` filme le passage (images par `UGTPFrameCapture`) ;
les temps d'image viennent d'un passage séparé, non filmé, en profil m1.

## 7. Résultats

À compléter après le passage sur le Mac : rapport de playtest, vidéo, temps
d'image avant et après (profil m1).
