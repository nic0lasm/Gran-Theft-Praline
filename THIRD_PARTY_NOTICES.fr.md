# Mentions des tiers

*Version anglaise : [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).*

Gran Theft Praline est un projet de fan non commercial, publié sous
[licence MIT](LICENSE) (code et documentation). Tout ce qui suit garde sa
propre licence. Le même texte s'affiche sur l'écran de crédits du jeu.
L'origine exacte de chaque asset est dans [assets/SOURCES.md](assets/SOURCES.md).

## Données géographiques

| Donnée | Source | Licence | Mention |
| ---- | ---- | ---- | ---- |
| BD TOPO (bâtiments, routes), LiDAR HD, RGE ALTI | IGN, via data.geopf.fr | Licence Ouverte Etalab 2.0 | Contient des données de l'IGN |
| Arbres d'alignement, bordures, mobilier, données administratives | Métropole de Lyon (data.grandlyon.com) | Licence Ouverte / Etalab | Contient des données de la Métropole de Lyon |
| Commerces, bancs, fontaines, identifiants de marques | OpenStreetMap | ODbL 1.0 | © les contributeurs d'OpenStreetMap (https://www.openstreetmap.org/copyright) |
| Noms et couleurs de marques | Wikidata | CC0 | — |

Les tables dérivées (bâtiments, voies, commerces) sont produites à partir de
ces sources par `tools/city` et `tools/geo-pipeline`. Au sens de l'ODbL, la
base dérivée s'obtient en lançant ces outils (aucune clé nécessaire) ; le jeu
empaqueté ne contient que la géométrie générée.

## Moteur, animation et art

| Élément | Source | Licence |
| ---- | ---- | ---- |
| Unreal Engine 5.8 | Epic Games | EULA d'Unreal Engine (hors licence MIT) |
| Game Animation Sample (animation du personnage) | Epic Games | EULA d'Unreal Engine |
| Quixel Megascans (surfaces, végétation, accessoires) | Fab (Epic Games) | Licence standard Fab, assets gratuits uniquement ; les scans ne sont pas redistribuables hors du jeu |
| Poly Haven, ambientCG (textures, HDRI) | polyhaven.com, ambientcg.com | CC0 |
| Photos de la ville réelle (README seulement, pas dans le jeu) | Corentin Eustacchi, Wikimedia Commons | CC BY-SA 4.0 |

Chaque ligne utilisée par le build est détaillée dans `assets/SOURCES.md` ;
une ligne y est ajoutée **avant** qu'un asset entre dans le projet.

## Son et polices

| Élément | Source | Licence |
| ---- | ---- | ---- |
| Effets, ambiances, voix de passants, radio | Générés par `tools/audio` (aucun enregistrement, aucune voix réelle) | MIT (original) |
| Enregistrement d'ambiance de rue (`SW_Amb_Street_Day`), s'il est gardé | Voir `assets/SOURCES.md` | Comme indiqué là |
| Polices | Polices par défaut du moteur (Roboto) | Apache 2.0 |

## Marques, personnes et autres jeux

- Les enseignes montrent le nom réel des commerces en lettrage. Les
  logos (œuvres de marque) ne sont **pas** distribués dans ce dépôt et ne
  figurent dans une build partagée en privé que si le propriétaire du projet
  l'a décidé ; voir [docs/qa/brands-rule.fr.md](docs/qa/brands-rule.fr.md).
  Toutes les marques appartiennent à leurs propriétaires ; le projet n'est ni
  affilié ni approuvé par aucune d'elles.
- Aucune personne réelle n'est représentée. Aucun asset ne vient d'un autre
  jeu, aucun fichier de jeu n'est utilisé.

## Signaler un crédit manquant

Ouvrir une issue sur le dépôt du projet. Un crédit manquant est un défaut,
corrigé avant la build de test suivante.
