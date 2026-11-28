# Gran Theft Praline — version web (three.js)

*English summary below.*

Une version complète et jouable dans le navigateur, faite avec
[three.js](https://threejs.org). Elle reprend la tranche de la rue de la
République du jeu Unreal (bâtiments et voies IGN BD TOPO, fichier
`assets/geo/processed/rue-republique/rue-republique.glb`) et l'étend jusqu'aux
quais du Rhône et de la Saône, avec la place Bellecour au sud. Tout le reste
(façades, voitures, personnages, sons) est généré par le code : aucun asset
externe, aucune marque réelle.

## Lancer le jeu

```bash
git lfs pull --include="assets/geo/processed/rue-republique/rue-republique.glb"
cd web
npm install
npm run dev        # http://localhost:5173
npm run build      # version statique dans web/dist/
```

## Ce qu'on peut faire

- **À pied** : marcher, sprinter (endurance), sauter, viser, frapper, tirer.
- **Voitures** : voler une voiture garée, sortir un conducteur (F), conduite
  arcade (frein à main, dérapages), dégâts, fumée, incendie, explosion.
  Citadine, berline, sportive, utilitaire, taxi, scooter, police, fourgon blindé.
- **Ville vivante** : circulation qui roule à droite et choisit ses
  carrefours, piétons qui fuient, lèvent les mains quand on les braque, et
  appellent la police quand ils voient un crime.
- **Police** : 1 à 5 étoiles, témoins, voitures de patrouille qui calculent
  leur itinéraire (A*), policiers à pied qui arrêtent puis tirent,
  hélicoptère à 4 étoiles, arrestation et hôpital.
- **Traboules** : des portes « Traboule » traversent les immeubles (E) et
  font perdre votre trace à la police.
- **Histoire** : 5 missions pour Tatie Rose contre la chaîne Sucrerouge
  (vol, braquage, livraison chronométrée, poursuite, casse du fourgon).
- **Boutiques** : armurerie, boulangerie (soins), planque (sauvegarde),
  Peinture Express (efface la recherche), garage (revente de voitures).
- **Extras** : 20 pralines cachées, cycle jour/nuit à la latitude de Lyon,
  minicarte avec GPS, carte complète (M), manette, sauvegarde locale.

## Commandes

| Touche | Action |
| ---- | ---- |
| ZQSD / WASD (touches physiques) | Se déplacer, conduire |
| Souris | Regarder |
| Maj | Sprinter |
| Espace | Sauter, frein à main |
| F / Entrée | Voler, monter, descendre |
| E | Traboules, boutiques, missions |
| Clic droit / gauche | Viser / tirer ou frapper |
| Molette, 1 à 4 | Changer d'arme |
| R, H | Recharger, klaxon |
| M | Carte |
| P / Échap | Pause |

## Organisation du code

| Dossier | Contenu |
| ---- | ---- |
| `src/world/` | Chargement du .glb IGN, grille de collision 1 m, extension procédurale, façades et sol peints au canvas, ciel et soleil |
| `src/entities/` | Personnages, véhicules (modèles et conduite), joueur et caméra |
| `src/systems/` | Piétons, circulation, police, armes, missions, objets et boutiques, effets |
| `src/ui/` | HUD, minicarte, carte, boutiques |

---

## English summary

A complete browser version of the game built with three.js. It loads the
IGN BD TOPO slice of rue de la République used by the Unreal project and
extends it procedurally to the Rhône and Saône quays and Place Bellecour.
Features: on-foot combat, car theft and arcade driving, AI traffic and
pedestrians, a 1–5 star wanted system with pathfinding police and a
helicopter, traboules to lose the police, five story missions, shops,
20 hidden pralines, day/night cycle, minimap with GPS, gamepad support and
local saves. Run `npm install && npm run dev` in `web/` after pulling the
LFS file above.
