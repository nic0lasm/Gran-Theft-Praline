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
| Maquette 3D texturée de Lyon 2018 (Maquettes 3D texturées, CityGML LOD2 : bâtiments, ponts, édifices remarquables) pour le décor lointain | Métropole de Lyon (data.grandlyon.com) | Licence Ouverte v2 | Contient des données de la Métropole de Lyon (maquette 3D 2018) |
| Année, niveaux, matériaux et usage des bâtiments (BDNB open, millésime 2026-02-b) pour la table des bâtiments | CSTB, Base de Données Nationale des Bâtiments (bdnb.io) | Licence Ouverte Etalab 2.0 | Contient des données de la BDNB (CSTB) |
| Orthophotographie (BD ORTHO) sous le relief du décor lointain | IGN, via data.geopf.fr | Licence Ouverte Etalab 2.0 | Contient des données de l'IGN |
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

## Attributions obligatoires (CC BY 4.0)

Intérieur de référence du fast-food. Sous licence CC BY 4.0 (https://creativecommons.org/licenses/by/4.0/), sans modification hormis le niveau de détail et la taille des textures :

- "Air Duct Kit" by Gerulf Doesinger (https://www.fab.com/listings/bdd899c9-04ea-428f-847b-d1012854803c), CC BY 4.0
- "Cash register" by Wnoz (https://www.fab.com/listings/6ac6b752-88c5-42e9-88ce-2c0ee9e839e9), CC BY 4.0
- "Compostable Burger Box RAWscan" by Spogna (https://www.fab.com/listings/9d5f4c5a-1877-498b-a4d6-8491f41d94f5), CC BY 4.0
- "Double Metal Shelving" by Jimbogies (https://www.fab.com/listings/0ae68265-4aa1-4538-9213-051e260448da), CC BY 4.0
- "Fridge" by Gerardo Justel (https://www.fab.com/listings/52e9035e-2a20-45ce-8479-6b269fd91aaf), CC BY 4.0
- "Gourmet Cheeseburger and Crispy Fries" by Zarrar Abbas (https://www.fab.com/listings/b8b57837-b085-41b0-a7cd-e1c9b61514ce), CC BY 4.0
- "Heavy Duty Fridge" by Gerardo Justel (https://www.fab.com/listings/d8d2bbd8-56e1-4644-b9ea-aa4dce9737c0), CC BY 4.0
- "Lockers" by Normal Map Enjoyer (https://www.fab.com/listings/a4bb96b2-ff10-4d27-860c-71a1c60f377b), CC BY 4.0
- "Menu Board for Restaurant" by ZendeScan (https://www.fab.com/listings/7b098fbb-22e6-48bb-8945-06a8a7ae459c), CC BY 4.0
- "Metal Food Tray" by Kami Rapacz (https://www.fab.com/listings/fbebe61b-eb0a-4d69-8f5c-6f3394928f69), CC BY 4.0
- "Office Binder" by AshenCat (https://www.fab.com/listings/e6d48397-4207-4d7e-94e5-7acbfd9e1877), CC BY 4.0
- "Office Pack Vol.1" by MobWModels (https://www.fab.com/listings/ce136033-3265-46d3-ac4d-fdbd5c9d0462), CC BY 4.0
- "Paper Bag Scan MEDPOLY" by sixonetwo (https://www.fab.com/listings/5c16c3a9-3f06-454e-9009-1cd788824f2b), CC BY 4.0
- "Realistic Fire Extinguisher" by RED2000 (https://www.fab.com/listings/eb018466-9bb6-46d9-9e43-04f5c9d10b2c), CC BY 4.0
- "Set of 3 Self-Service Kiosks" by ahmed kanana (https://www.fab.com/listings/6f0ebc82-dbda-4c39-a244-c36dedb7f71c), CC BY 4.0

La liste complète est dans `assets/SOURCES.md`.

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
