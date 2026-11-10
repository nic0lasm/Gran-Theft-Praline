<p align="center">
  <a href="README.md">English</a> · <b>Français</b>
</p>

<h1 align="center">Gran Theft Praline</h1>

<p align="center">
  <b>Un jeu en monde ouvert original, dans un Lyon fidèle, rue par rue.</b><br>
  On s'y promène, on y conduit, et bientôt on s'y fait poursuivre.
</p>

<p align="center">
  <img src="docs/media/readme/hero-cafe-terrace.jpg" alt="En jeu : une terrasse de café et une voiture qui roule rue de la République, version 0.2" width="100%">
  <br><sub><i>En jeu, version 0.2, rue de la République. Filmé par le jeu lui-même sur un MacBook Pro M1 Pro.</i></sub>
</p>

<p align="center">
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/releases/tag/v0.2.0"><img alt="Télécharger la 0.2" src="https://img.shields.io/badge/t%C3%A9l%C3%A9charger-0.2%20pour%20macOS-success?logo=apple"></a>
  <img alt="0.3 en cours" src="https://img.shields.io/badge/prochaine-0.3%20Au%20volant-orange">
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml/badge.svg?branch=main"></a>
  <img alt="Unreal Engine 5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.8-0e1128?logo=unrealengine">
  <a href="LICENSE"><img alt="Licence : MIT" src="https://img.shields.io/badge/licence-MIT-blue.svg"></a>
</p>

<p align="center">
  <a href="#lyon-pour-de-vrai">Lyon, pour de vrai</a> ·
  <a href="#en-images">En images</a> ·
  <a href="#jouer-en-un-clic">Jouer</a> ·
  <a href="#où-on-en-est">Où on en est</a> ·
  <a href="#feuille-de-route">Feuille de route</a> ·
  <a href="#le-compiler-soi-même">Compiler</a> ·
  <a href="#contribuer">Contribuer</a> ·
  <a href="#crédits-et-licence">Crédits</a>
</p>

---

Gran Theft Praline est un projet amateur, libre et non commercial, avec un
grand objectif : la maniabilité, les interactions et la qualité d'image des
meilleurs jeux en monde ouvert actuels, **sur la vraie carte de Lyon**. Pas
encore d'histoire, c'est voulu : le terrain de jeu d'abord. Tout le monde est
le bienvenu pour suivre l'aventure, essayer les versions, partager sa
connaissance de la ville ou donner un coup de main.

## Lyon, pour de vrai

La ville n'est pas dessinée à la main : elle est **reconstruite à partir de
données publiques**. Emprises et hauteurs des immeubles viennent du LiDAR et
de la BD TOPO de l'IGN, arbres, bordures et mobilier urbain de la Métropole
de Lyon, noms des boutiques d'OpenStreetMap. Chaque platane, chaque banc,
chaque devanture est à sa place dans la vraie rue.

<table>
  <tr>
    <th width="50%">La vraie rue de la République</th>
    <th width="50%">La même rue dans le jeu (0.3 en cours)</th>
  </tr>
  <tr>
    <td><img src="docs/media/readme/real-rue-republique.jpg" alt="Photo de la rue de la République piétonne à Lyon, façades en pierre et platanes" width="100%"></td>
    <td><img src="docs/media/readme/0.3-rue-republique-wide.jpg" alt="En jeu : course dans la rue de la République entre façades en pierre, vitrines et platanes" width="100%"></td>
  </tr>
  <tr>
    <td><sub>Photo : Corentin Eustacchi, <a href="https://commons.wikimedia.org/wiki/File:Rue_de_la_R%C3%A9publique_Nord_pi%C3%A9tonne_(Lyon,_2025).jpg">Wikimedia Commons</a>, CC BY-SA 4.0, recadrée.</sub></td>
    <td><sub>Hauteurs réelles issues du LiDAR IGN (écart médian des toits 0,39 m), 175 immeubles, 13 000 modules de façade, 112 platanes.</sub></td>
  </tr>
</table>

Ce qui manque encore au jeu face à la photo, honnêtement : les feuilles des
platanes, les vrais logos des enseignes, des passants dans la rue et un vrai
personnage (le joueur est encore le mannequin blanc d'Epic). Tout cela est
dans la feuille de route.

## En images

Toutes les images ci-dessous sont extraites des vidéos que le jeu enregistre
lui-même pendant ses tests automatiques, sur un MacBook Pro M1 Pro avec le
profil de qualité **M1** du quotidien. Aucune retouche.

<table>
  <tr>
    <td width="50%"><img src="docs/media/readme/0.3-driving-rue-republique.jpg" alt="En jeu : conduite d'une voiture rue de la République, vue à la troisième personne" width="100%"><br><sub>Au volant dans la rue de la République (0.3)</sub></td>
    <td width="50%"><img src="docs/media/readme/0.2-carjack-approach.jpg" alt="En jeu : le personnage s'approche d'une voiture garée pour la voler" width="100%"><br><sub>Une voiture garée : touche F pour la prendre (0.2)</sub></td>
  </tr>
  <tr>
    <td><img src="docs/media/readme/0.3-square-benches.jpg" alt="En jeu : placette avec bancs en fonte, platanes et lampadaire lyonnais" width="100%"><br><sub>Bancs en fonte et lampadaire lyonnais, là où sont les vrais (0.3)</sub></td>
    <td><img src="docs/media/readme/0.3-plane-trees-lamppost.jpg" alt="En jeu : à pied entre platanes et bancs, une voiture garée à droite" width="100%"><br><sub>À pied entre les platanes (0.3)</sub></td>
  </tr>
  <tr>
    <td><img src="docs/media/readme/0.3-on-foot-crossroads.jpg" alt="En jeu : carrefour avec immeuble d'angle, bancs et une voiture au loin" width="100%"><br><sub>Un carrefour, avec la circulation au loin (0.3)</sub></td>
    <td><img src="docs/media/readme/real-quai-jean-moulin.jpg" alt="Photo du quai Jean-Moulin au bord du Rhône, platanes, circulation et promenade" width="100%"><br><sub><b>Prochaine étape :</b> le vrai quai Jean-Moulin au bord du Rhône, qui arrive avec le pont Lafayette dans la 0.3. Photo : Corentin Eustacchi, <a href="https://commons.wikimedia.org/wiki/File:Quai_Jean-Moulin_(Lyon,_2025).jpg">Wikimedia Commons</a>, CC BY-SA 4.0, recadrée.</sub></td>
  </tr>
</table>

🎥 Vidéos et mesures : [test de la version 0.1](production/evidence/0.1/),
[essais de rendu sous quatre éclairages](docs/rendering/test-results/2026-11-09/)
(midi, heure dorée, pluie, nuit).

## Jouer en un clic

**[⬇️ Télécharger la version 0.2 « La rue habillée » pour macOS](https://github.com/nic0lasm/Gran-Theft-Praline/releases/tag/v0.2.0)**
(Mac Apple Silicon, sortie le 10 novembre 2026).

La rue de la République reconstruite d'après le LiDAR de l'IGN, avec ses
façades en pierre, ses devantures à leurs vrais noms, son mobilier urbain et
ses platanes, et une voiture dans laquelle monter pour conduire. Mesuré sur
10 minutes sur un M1 Pro : image médiane 29,9 ms (environ 33 i/s), 95 % des
images sous 40,3 ms.

L'application n'est pas encore signée par Apple : les notes de version
expliquent comment l'ouvrir la première fois. On peut aussi
[la compiler soi-même](#le-compiler-soi-même) avec un seul script.

<details>
<summary><b>Commandes</b> (clavier et manette)</summary>

| Action | Clavier (AZERTY) | Clavier (QWERTY) | Manette |
| ---- | ---- | ---- | ---- |
| Se déplacer | Z Q S D | W A S D | Stick gauche |
| Regarder | Souris | Souris | Stick droit |
| Sprinter (maintenir) | Maj gauche | Maj gauche | A / Croix |
| Marcher (bascule) | Alt gauche | Alt gauche | Clic stick gauche |
| Sauter, enjamber | Espace | Espace | X / Carré |
| S'accroupir | C | C | B / Rond |
| Première / troisième personne | V | V | View / Share |
| Menu pause | Échap | Échap | Menu / Options |
| Monter / descendre d'une voiture | F | F | Y / Triangle |
| Accélérer / freiner, reculer | Z / S | W / S | Gâchette droite / gauche |
| Tourner | Q / D | A / D | Stick gauche |
| Frein à main | Espace | Espace | RB / R1 |

Tous les détails : [personnage](docs/gameplay/player-character.fr.md),
[véhicules](docs/gameplay/vehicles.fr.md).

</details>

## Où on en est

| | Version | Ce qu'on peut faire | État |
| :--: | ---- | ---- | ---- |
| ✅ | **0.1** Jouer en un clic | lancer le jeu et marcher rue de la République | sortie le 9 nov. 2026 |
| ✅ | **0.2** La rue habillée | flâner dans une rue fidèle et habillée, sur un portable M1 | sortie le 10 nov. 2026 |
| 🟡 | **0.3** Au volant | voler une voiture et traverser la circulation jusqu'au quai du Rhône et au pont Lafayette | en cours |

La version 0.3 est en train d'être finalisée : trois voitures pilotables
(Clio V, 208 II, Kangoo II) dans lesquelles on monte, dont on casse la vitre
ou dont on sort le conducteur, une circulation de PNJ sur des voies tirées
des données routières de l'IGN, un son de moteur procédural, la carte
étendue au quai du Rhône et au pont Lafayette, et un retour sur le quai si on
tombe dans le fleuve. Elle sort quand le portable M1 garde 95 % des images
sous 42 ms avec la circulation. Défauts connus : la voiture de démonstration
monte parfois sur le trottoir, et le quai et le pont n'ont pas encore été
filmés.

Déjà en construction en parallèle pour les versions suivantes : des piétons
qui réagissent, des ragdolls, la police et ses étoiles de recherche, une
mini-carte, le jour, la nuit et la pluie, et le son de la rue.

## Feuille de route

Le jeu avance par **versions jouables** : chacune se lance d'un double-clic
et ajoute quelque chose qu'on voit ou qu'on fait.

| Version | On peut… | Visée |
| ---- | ---- | ---- |
| 0.4 La rue vivante | croiser des piétons qui réagissent, s'arrêter aux feux, renverser le mobilier | déc. 2026 |
| 0.5 La police arrive | être recherché (1-2 étoiles), poursuivi en voiture, arrêté ou semer la police | déc. 2026 |
| 0.6 Jour et nuit | rouler de jour, au crépuscule, de nuit et sous la pluie | janv. 2027 |
| 0.7 Armé | se battre à mains nues et au pistolet, se mettre à couvert, mourir et réapparaître | févr. 2027 |
| 0.8 La vie en ville | entrer dans une boutique sans chargement, acheter, la braquer, gagner de l'argent, sauvegarder | mars 2027 |
| 0.9 Tout le quartier | parcourir Bellecour → Terreaux sans chargement | avr. 2027 |
| 0.10 à 0.12 | carte de la ville et GPS, six armes, missions, scooter et Vélo'v | été 2027 |
| 0.13 à 0.17 | façades et monuments définitifs, intérieurs, habitants et voitures, le son de Lyon, finitions | début 2028 |
| **1.0** Tranche verticale | jouer tout le nord de la Presqu'île, de Bellecour aux Terreaux | début 2028 |

Les dates sont des ordres de grandeur à faible confiance, revus à chaque
version. Détails : [feuille de route complète](production/roadmap.fr.md),
[inventaire des systèmes](production/systems-inventory.fr.md),
[concept du jeu](design/gdd/game-concept.fr.md).

### Deux profils de qualité

| Profil | Ce que c'est | Cible |
| ---- | ---- | ---- |
| **Ultra** | La vraie cible visuelle : assets en pleine résolution, Nanite, Lumen haute qualité | Une machine puissante (GPU dédié récent ou Apple Silicon haut de gamme) |
| **M1** | Le profil jouable du quotidien sur un portable Apple M1 Pro | environ 30 i/s en 1080p |

Le profil M1 ne baisse que les réglages graphiques ; les modèles et les
matériaux ne sont jamais simplifiés. Voir
[ADR-0001](docs/adr/adr-0001-rendering-pipeline.fr.md) et
[ADR-0002](docs/adr/adr-0002-performance-budget.fr.md).

## Le compiler soi-même

### Prérequis

- Un Mac Apple Silicon
- [Unreal Engine 5.8](https://www.unrealengine.com/) (Epic Games Launcher)
- Xcode avec la chaîne d'outils Metal
- [Git LFS](https://git-lfs.com/)
- Python 3 (outils de ville et d'assets)

Une partie du contenu **n'est pas dans Git**, pour des raisons de licence ou
de taille, et s'installe ou se régénère en local :

| Contenu | Comment l'obtenir |
| ---- | ---- |
| Game Animation Sample d'Epic (personnage, animations, caméra) | Gratuit sur Fab, puis [`tools/character/install_gasp_content.sh`](tools/character/README.md) |
| Le niveau rue de la République | Généré depuis les données publiques par [`tools/city`](docs/city/plan.fr.md) et [`tools/level`](tools/level/README.md) |
| Données de ville (tables OpenStreetMap) | Régénérées par [`tools/city`](docs/city/plan.fr.md) (ODbL, sans clé) |
| Modèles 3D haute qualité | Gardés hors Git ; préparés pour Unreal par [`tools/assets`](docs/assets/import-pipeline.fr.md) |
| Logos et textures des vraies marques | Gardés hors Git dans `Plugins/CityBrands/` |

### Étapes

```zsh
git lfs install
git clone https://github.com/nic0lasm/Gran-Theft-Praline.git
open Gran-Theft-Praline/GranTheftPraline/GranTheftPraline.uproject
```

Puis, une fois le contenu ci-dessus installé :

```zsh
tools/build/package-mac.sh        # → Builds/<version>/GranTheftPraline.app
tools/build/test-build-mac.sh     # test de 10 minutes, vidéo et temps d'image
```

Guide pas à pas : [compilation macOS en un clic](docs/build/one-click-build.fr.md).

### Technologie

| Domaine | Choix |
| ---- | ---- |
| Moteur | Unreal Engine 5.8 : Lumen, Virtual Shadow Maps, Substrate, TSR |
| Code | C++ pour les systèmes de jeu, Blueprint pour le prototypage |
| Plateforme | macOS Apple Silicon (Metal) d'abord, Windows plus tard |
| Bases de gameplay | Les briques gratuites d'Epic : Game Animation Sample, Gameplay Ability System, Chaos Vehicles, StateTree |
| Ville | IGN (BD TOPO, LiDAR HD, RGE ALTI), data.grandlyon.com et OpenStreetMap, traités par [tools/city](tools/city/) et [tools/geo-pipeline](tools/geo-pipeline/) |
| Graphisme | Quixel Megascans, Poly Haven et ambientCG (CC0), Blender ; voir la [bible artistique](docs/art/art-bible.fr.md) |

## Contribuer

Les fondations bougent encore, mais votre aide est la bienvenue, et pas
besoin de savoir coder :

- **Vous connaissez Lyon ?** Signalez une boutique manquante, une hauteur
  d'immeuble fausse, un détail qui fait qu'une rue sonne juste.
- **Vous avez essayé une version ?** Dites-nous comment elle tourne sur
  votre Mac (modèle, fréquence d'images, problèmes).
- **Unreal, 3D, son, design ?** Choisissez une version dans la
  [feuille de route](production/roadmap.fr.md) et proposez ce que vous
  aimeriez prendre en charge.

Ouvrez une [issue](https://github.com/nic0lasm/Gran-Theft-Praline/issues)
pour lancer la discussion, surtout avant un gros chantier.

<details>
<summary><b>Comment fonctionne le dépôt</b></summary>

- **Branches** : on travaille sur `feat/<sujet>` ou `fix/<sujet>`. `next`
  est la branche de préversion, `main` la branche des versions publiées.
  L'historique reste linéaire (rebase, avance rapide uniquement).
- **CI** : chaque push lance le *Provenance check* (identités des commits,
  fichiers suivis, noms de branches) et les tests des outils. Un changement
  passe sur `next`, puis `main`, seulement quand la CI est verte sur ce
  commit précis.
- **Preuves** : un changement visuel ou de performance s'accompagne d'une
  capture ou d'une mesure.
- **Langues** : code, messages de commit et identifiants en anglais ;
  documentation en anglais et en français (`fichier.md` + `fichier.fr.md`).
- **Assets** : chaque asset externe ou généré a une ligne dans
  [assets/SOURCES.md](assets/SOURCES.md) avec son origine et sa licence. Les
  gros binaires passent par Git LFS ([règles](docs/project/git-lfs.fr.md)).

</details>

## Crédits et licence

- Données géographiques : contient des données de l'**IGN** (BD TOPO, LiDAR
  HD, RGE ALTI) et de la **Métropole de Lyon** (Licence Ouverte / Etalab), et
  **© les contributeurs d'OpenStreetMap** ([ODbL](https://opendatacommons.org/licenses/odbl/)).
- Photos de la vraie ville dans ce README : **Corentin Eustacchi**, Wikimedia
  Commons, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/deed.fr),
  recadrées et redimensionnées ; les deux fichiers image gardent cette
  licence.
- **Epic Games** : Unreal Engine et le Game Animation Sample sont utilisés
  sous le contrat de licence d'Unreal Engine et ne relèvent pas de la
  licence de ce dépôt.
- Les enseignes et les marques appartiennent à leurs propriétaires ; elles
  sont reproduites pour un projet non commercial et ne sont pas distribuées
  dans ce dépôt. Aucune personne réelle n'est représentée dans le jeu, et
  aucun asset n'est repris d'autres jeux.
- Source et licence de chaque asset : [assets/SOURCES.md](assets/SOURCES.md).

Le code et la documentation de ce dépôt sont publiés sous
[licence MIT](LICENSE). Les données et assets tiers gardent leur propre
licence.

<p align="center"><i>Fait à Lyon, avec amour pour ses rues. 🦁</i></p>
