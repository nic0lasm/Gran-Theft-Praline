<p align="center">
  <a href="README.md">English</a> · <b>Français</b>
</p>

<p align="center">
  <img src="docs/media/banner.svg" alt="Gran Theft Praline : un monde ouvert à Lyon" width="100%">
</p>

<p align="center">
  <a href="LICENSE"><img alt="Licence : MIT" src="https://img.shields.io/badge/licence-MIT-blue.svg"></a>
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml/badge.svg?branch=main"></a>
  <img alt="Version 0.1" src="https://img.shields.io/badge/version-0.1-success">
  <img alt="Unreal Engine 5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.8-0e1128?logo=unrealengine">
  <img alt="Plateforme : macOS" src="https://img.shields.io/badge/plateforme-macOS%20(Apple%20Silicon)-lightgrey?logo=apple">
</p>

<p align="center">
  <a href="#cest-quoi-">C'est quoi ?</a> ·
  <a href="#le-jeu-en-images">En images</a> ·
  <a href="#jouer-en-un-clic">Jouer</a> ·
  <a href="#feuille-de-route">Feuille de route</a> ·
  <a href="#le-compiler-soi-même">Compiler</a> ·
  <a href="#contribuer">Contribuer</a> ·
  <a href="#crédits-et-licence">Crédits</a>
</p>

# Gran Theft Praline

**Un jeu en monde ouvert original, dans un Lyon fidèle et photoréaliste.**

Bienvenue ! Ce projet amateur, libre et non commercial vise quelque chose de
grand : la maniabilité, les interactions et la qualité d'image des meilleurs
jeux en monde ouvert actuels, sur la vraie carte de Lyon. Tout le monde est
le bienvenu pour suivre l'aventure, essayer les versions, partager sa
connaissance de la ville ou donner un coup de main.

## C'est quoi ?

- **Lyon, pour de vrai.** La ville est reconstruite à partir de données
  géographiques publiques (IGN, Métropole de Lyon, OpenStreetMap) : vraies
  rues, vraies hauteurs d'immeubles, vrais arbres et mobilier urbain, et les
  **vraies enseignes** là où elles se trouvent.
- **D'abord un vrai terrain de jeu.** Se déplacer à pied et en voiture, les
  armes, les boutiques et immeubles où l'on entre, la police, la foule et la
  circulation : au moins les interactions qu'on attend d'un monde ouvert
  récent.
- **Pas encore de scénario, et c'est voulu.** L'intrigue, les personnages et
  le ton seront écrits une fois la base jouable en place.
- **Le photoréalisme d'abord.** La référence visuelle, ce sont les jeux les
  plus réalistes du moment ; la caméra et les commandes restent classiques :
  troisième personne, vue à la première personne en option.
- **On commence petit.** La première zone est le **nord de la Presqu'île**,
  de la place Bellecour à la place des Terreaux, entre Saône et Rhône
  (environ 1,5 km²), en commençant par la rue de la République.

Aucune personne réelle, aucun asset repris d'autres jeux. Les marques
visibles dans le jeu appartiennent à leurs propriétaires : elles ne sont pas
couvertes par la licence MIT et ne sont pas stockées dans ce dépôt.

## Le jeu en images

La version 0.1, filmée par le jeu lui-même sur un MacBook Pro M1 Pro. La rue
est encore une maquette grise construite à partir de 334 vrais bâtiments de
l'IGN : le vrai rendu arrive avec la version 0.2.

<p align="center">
  <img src="production/evidence/0.1/title.jpg" alt="Écran titre de la version 0.1" width="49%">
  <img src="docs/media/0.1-gameplay.jpg" alt="Le personnage marche dans la rue de la République, version 0.1" width="49%">
</p>

- 🎥 [Vidéo de deux minutes de la version 0.1](production/evidence/0.1/capture.mov)
  (37 Mo)
- 📊 [Résultats des tests](production/evidence/0.1/) : 10 minutes en plein
  écran sans plantage, première image 1,7 s après le lancement, temps
  d'image p50 41 ms / p95 51 ms (30 i/s visés à partir de la 0.2)
- 🖼️ [Essais de rendu](docs/rendering/test-results/2026-11-09/) sous quatre
  éclairages (midi, heure dorée, pluie, nuit)

## Jouer en un clic

La version **0.1 « Jouer en un clic »** est sortie le **9 novembre 2026**,
18 jours avant la date prévue : double-clic sur `GranTheftPraline.app`, bouton
*Jouer*, et vous voilà rue de la République.

**[⬇️ Télécharger la version 0.1 pour macOS](https://github.com/nic0lasm/Gran-Theft-Praline/releases/tag/v0.1.0)**
(Mac Apple Silicon). L'application n'est pas encore signée par Apple : les
notes de version expliquent comment l'ouvrir la première fois. On peut aussi
[compiler l'application soi-même](#le-compiler-soi-même) avec un seul script.

| Action | Clavier AZERTY | Clavier QWERTY | Manette |
| ---- | ---- | ---- | ---- |
| Se déplacer | Z Q S D | W A S D | Stick gauche |
| Regarder | Souris | Souris | Stick droit |
| Sprinter (maintenir) | Maj gauche | Maj gauche | A / Croix |
| Marcher (bascule) | Alt gauche | Alt gauche | Clic stick gauche |
| Sauter, enjamber | Espace | Espace | X / Carré |
| S'accroupir | C | C | B / Rond |
| Première / troisième personne | V | V | Vue / Share |
| Menu pause | Échap | Échap | Menu / Options |

Tous les détails : [personnage jouable](docs/gameplay/player-character.fr.md).

## Feuille de route

Le jeu avance par **versions jouables** : chacune se lance d'un double-clic
et apporte quelque chose qu'on peut voir ou faire.

| Version | Nom | On peut… | Cible | État |
| ---- | ---- | ---- | ---- | ---- |
| **0.1** | Jouer en un clic | lancer le jeu, marcher, courir, sauter rue de la République | nov. 2026 | ✅ sortie le 9 nov. |
| 0.2 | La rue habillée | flâner dans une rue de la République photoréaliste à 30 i/s | déc. 2026 | 🟡 en cours |
| 0.3 | Au volant | monter dans une voiture garée et rouler jusqu'au Rhône | janv. 2027 | 🟡 conception faite |
| 0.4 à 0.6 | Armé, première boutique, police | tirer, braquer une boutique, semer la police | mars 2027 | ⬜ |
| 0.7 à 0.13 | Tout le quartier | Bellecour → Terreaux avec foule, circulation, carte, économie, jour et nuit, météo | oct. 2027 | ⬜ |
| 0.14 à 0.19 | Qualité finale | façades, monuments, intérieurs, habitants, voitures, le son de Lyon, finitions | mai 2028 | ⬜ |
| **1.0** | Tranche verticale | une heure de jeu dans le quartier, sans blocage | **juin 2028** | ⬜ |

Ce qui se prépare en ce moment :

- **0.2** : les réglages de rendu qui tiennent 30 i/s sur un portable M1, la
  première vague de modèles 3D (mobilier urbain : bancs, abribus, potelets…)
  et une reconstruction 3D plus fidèle de la Presqu'île (relief et toits
  LiDAR).
- **0.3** : les véhicules conduisibles, d'après les voitures qu'on croise
  vraiment en France (Clio, 208, Kangoo, Golf, Yaris…), plus les bus TCL et
  les Vélo'v.

Pour suivre :

- [Feuille de route complète](production/roadmap.fr.md) : chaque version en
  détail, risques, dépendances
- [Tableau de bord](production/roadmap-status.fr.md) : tâche par tâche, avec
  la preuve de chacune
- [Concept du jeu](design/gdd/game-concept.fr.md) et
  [catalogue des modèles 3D](production/asset-catalog.fr.md)

### Deux profils de qualité

| Profil | Ce que c'est | Cible |
| ---- | ---- | ---- |
| **Ultra** | La vraie cible visuelle : assets pleine résolution, Nanite, Lumen haute qualité | Une machine puissante (carte graphique dédiée récente ou Apple Silicon haut de gamme) |
| **M1** | Le profil jouable au quotidien sur un portable Apple M1 Pro | 30 i/s en 1080p, 95 % des images ≤ 33,3 ms |

Le profil M1 ne baisse que les réglages graphiques ; les modèles et matériaux
ne sont jamais simplifiés. Voir
[ADR-0001](docs/adr/adr-0001-rendering-pipeline.fr.md) et
[ADR-0002](docs/adr/adr-0002-performance-budget.fr.md).

## Le compiler soi-même

### Prérequis

- Un Mac Apple Silicon
- [Unreal Engine 5.8](https://www.unrealengine.com/) (Epic Games Launcher)
- Xcode avec la chaîne d'outils Metal
- [Git LFS](https://git-lfs.com/)
- Python 3 (outils ville et assets)

Une partie du contenu **n'est pas dans Git**, pour des raisons de licence ou
de taille, et s'installe ou se régénère en local :

| Contenu | Comment l'obtenir |
| ---- | ---- |
| Game Animation Sample d'Epic (personnage, animations, caméra) | Gratuit sur Fab, puis [`tools/character/install_gasp_content.sh`](tools/character/README.md) |
| Le niveau rue de la République | Généré à partir de la maquette IGN par [`tools/level`](tools/level/README.md) |
| Données de la ville (tables OpenStreetMap) | Régénérées par [`tools/city`](docs/city/plan.fr.md) (ODbL, sans clé) |
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
tools/build/package-mac.sh        # → Builds/0.1/GranTheftPraline.app
tools/build/test-build-mac.sh     # test de 10 minutes, vidéo et temps d'image
```

Guide pas à pas : [build macOS en un clic](docs/build/one-click-build.fr.md).

### Technologies

| Domaine | Choix |
| ---- | ---- |
| Moteur | Unreal Engine 5.8 : Lumen, Virtual Shadow Maps, Substrate, TSR |
| Code | C++ pour les systèmes de jeu, Blueprint pour prototyper |
| Plateforme | macOS sur Apple Silicon (Metal) d'abord, Windows plus tard |
| Socle gameplay | Les briques gratuites d'Epic : Game Animation Sample, Gameplay Ability System, Chaos Vehicles, StateTree, Mass |
| Ville | IGN (BD TOPO, LiDAR HD, RGE ALTI), data.grandlyon.com et OpenStreetMap, traités par [tools/city](tools/city/) et [tools/geo-pipeline](tools/geo-pipeline/) |
| Graphismes | Quixel Megascans, Tripo pour les modèles 3D, Blender ; voir la [bible artistique](docs/art/art-bible.fr.md) |

## Contribuer

Les fondations bougent encore, mais votre aide est la bienvenue, et pas
besoin de savoir coder :

- **Vous connaissez Lyon ?** Signalez une boutique manquante, une hauteur
  d'immeuble fausse, un détail qui rend une rue vraie.
- **Vous avez essayé une version ?** Dites-nous comment elle tourne sur votre
  Mac (modèle, images par seconde, problèmes).
- **Unreal, 3D, son, game design ?** Choisissez une version dans la
  [feuille de route](production/roadmap.fr.md) et proposez ce que vous
  aimeriez prendre en charge.

Ouvrez une [issue](https://github.com/nic0lasm/Gran-Theft-Praline/issues)
pour lancer la discussion, surtout avant un gros chantier.

Fonctionnement du dépôt :

- **Branches** : travailler sur `feat/<sujet>` ou `fix/<sujet>`. `next` est la
  branche de préproduction, `main` la branche de production. Historique
  linéaire (rebase, avance rapide uniquement).
- **CI** : chaque push lance le *Provenance check* (identités des commits,
  fichiers suivis, noms de branches) et les tests des outils. Un changement
  ne passe sur `next`, puis `main`, qu'une fois la CI verte sur ce commit
  précis.
- **Preuves** : un changement visuel ou de performance s'accompagne d'une
  capture ou d'une mesure.
- **Langues** : code, messages de commit et identifiants en anglais ;
  documentation en anglais et en français (`fichier.md` + `fichier.fr.md`).
- **Assets** : chaque asset externe ou généré a sa ligne dans
  [assets/SOURCES.md](assets/SOURCES.md) avec son origine et sa licence. Les
  gros binaires passent par Git LFS ([règles](docs/project/git-lfs.fr.md)).

## Crédits et licence

- Données géographiques : contient des données de l'**IGN** (BD TOPO,
  LiDAR HD, RGE ALTI) et de la **Métropole de Lyon** (Licence Ouverte /
  Etalab), et **© les contributeurs d'OpenStreetMap**
  ([ODbL](https://opendatacommons.org/licenses/odbl/)).
- **Epic Games** : Unreal Engine et le Game Animation Sample sont utilisés
  selon le CLUF d'Unreal Engine et ne sont pas couverts par la licence de ce
  dépôt.
- Les enseignes et marques appartiennent à leurs propriétaires ; elles sont
  reproduites dans un projet non commercial et ne sont pas distribuées dans
  ce dépôt.
- Source et licence de chaque asset : [assets/SOURCES.md](assets/SOURCES.md).

Le code et la documentation de ce dépôt sont publiés sous
[licence MIT](LICENSE). Les données et assets tiers gardent leur propre
licence.

<p align="center"><i>Fait à Lyon, avec amour pour ses rues. 🦁</i></p>
