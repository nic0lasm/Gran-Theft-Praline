<p align="center">
  <a href="README.md">English</a> · <b>Français</b>
</p>

<h1 align="center">Gran Theft Praline</h1>

<p align="center">
  <b>Un jeu en monde ouvert original, dans un Lyon fidèle, rue par rue.</b><br>
  Voler une voiture rue de la République, puis tenter de semer la police.
</p>

<p align="center">
  <img src="docs/media/readme/hero-cafe-terrace.jpg" alt="En jeu : une terrasse de café et une voiture qui roule rue de la République, version 0.2" width="100%">
  <br><sub><i>En jeu, version 0.2, rue de la République. Filmé par le jeu lui-même sur un MacBook Pro M1 Pro.</i></sub>
</p>

<p align="center">
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml"><img alt="CI" src="https://github.com/nic0lasm/Gran-Theft-Praline/actions/workflows/ci.yml/badge.svg?branch=main"></a>
  <img alt="État : pré-alpha" src="https://img.shields.io/badge/%C3%A9tat-pr%C3%A9--alpha-orange">
  <img alt="Unreal Engine 5.8" src="https://img.shields.io/badge/Unreal%20Engine-5.8-0e1128?logo=unrealengine">
  <img alt="C++" src="https://img.shields.io/badge/C%2B%2B-syst%C3%A8mes%20de%20jeu-00599C?logo=cplusplus">
  <a href="https://github.com/nic0lasm/Gran-Theft-Praline/releases/tag/v0.3.0"><img alt="Dernière version 0.3.0 pour macOS" src="https://img.shields.io/badge/derni%C3%A8re%20version-0.3.0%20(macOS)-success?logo=apple"></a>
  <a href="LICENSE"><img alt="Licence : MIT" src="https://img.shields.io/badge/licence-MIT-blue.svg"></a>
</p>

<p align="center">
  <a href="#en-bref">En bref</a> ·
  <a href="#où-on-en-est">Où on en est</a> ·
  <a href="#lyon-pour-de-vrai">Lyon, pour de vrai</a> ·
  <a href="#en-images">En images</a> ·
  <a href="#comment-il-est-fait">Comment il est fait</a> ·
  <a href="#feuille-de-route">Feuille de route</a> ·
  <a href="#jouer-ou-compiler">Jouer ou compiler</a> ·
  <a href="#crédits-et-licence">Crédits</a>
</p>

---

## En bref

Gran Theft Praline est un projet personnel, libre et non commercial : un
GTA-like sur **la vraie carte de Lyon**, reconstruite à partir de données
géographiques publiques, fait avec Unreal Engine 5.8 et les contenus
gratuits d'Epic. L'objectif, ce sont les sensations des meilleurs mondes
ouverts actuels : une voiture qu'on vole, une circulation et des passants
qui réagissent, une police qui vous traque, et une image qui tient la
comparaison avec une photo de la vraie rue.

Le travail porte sur **une tranche de 300 à 400 m de la rue de la
République, de Bellecour aux Jacobins**, jugée sur un PC Windows
(RTX 3070 Ti). Pas encore d'histoire, c'est voulu : le terrain de jeu
d'abord.

## Où on en est

*Vérifié le 24 novembre 2026 pour le vol, la fuite, la police et la
circulation, d'après la porte et la vidéo du dernier candidat fusionné sur
`main` (76) ; le 21 novembre pour le reste.*

C'est un travail en cours, et cette partie dit ce qui est **prouvé** dans le
jeu packagé, pas ce que le code est censé faire.

| | Ce que fait le joueur | État sur `main` |
| :--: | ---- | ---- |
| ✅ | Marcher, courir et regarder autour de soi en **Nicolito**, un personnage fictif fait de pièces de City Sample | Prouvé à chaque test automatique |
| ✅ | S'approcher d'une voiture garée et la voler | Prouvé à chaque test automatique |
| ✅ | Braquer un passant au pistolet : il lève les mains | Prouvé par la mesure ; la vue de près reste trop sombre |
| ✅ | Un passant témoin du vol appelle la police : une étoile de recherche | Prouvé avec un modèle de voiture ; vérification en cours avec un second |
| 🟡 | Voler une voiture en circulation et fuir avec la police derrière | Vol en 6 s, pointe à 89 km/h ; la fuite est conduite par le test automatique, qui se coince encore : 35 km/h de moyenne |
| 🟡 | La police prend le joueur en chasse et l'arrête | Voiture de police reconnaissable, sommation à l'écran, arrestation à 63 s ; les unités roulent à 17 km/h de moyenne au lieu de 25 |
| 🟡 | Rouler dans une circulation vivante | Les voitures roulent debout, au moins 8 en mouvement à moins de 150 m pendant la fuite ; feux et évitement pas encore mesurés |
| ⬜ | Semer la police, mourir et revenir | Pas encore mesuré |
| ⬜ | Une image fidèle à la vraie rue | 1 sur 5 à la première comparaison photo / capture (12 novembre) ; hors de la rue habillée, le lointain est encore vide |

Le jalon A, *voler une voiture et fuir la police*, est attendu vers le
4 décembre 2026. Il est jugé sur une grille de 10 points lue sur une vidéo
de trois minutes sans montage, enregistrée sur le PC ; le candidat 76 en
tient 3 ([tableau de bord](production/roadmap-status.fr.md)).

## Lyon, pour de vrai

La ville n'est pas dessinée à la main : elle est **reconstruite à partir de
données publiques**. Emprises et hauteurs des immeubles viennent du LiDAR et
de la BD TOPO de l'IGN, arbres, bordures et mobilier urbain de la Métropole
de Lyon, noms des boutiques d'OpenStreetMap. Chaque platane, chaque banc,
chaque devanture est à sa place dans la vraie rue.

<table>
  <tr>
    <th width="50%">La vraie rue de la République</th>
    <th width="50%">La même rue dans le jeu (version 0.3)</th>
  </tr>
  <tr>
    <td><img src="docs/media/readme/real-rue-republique.jpg" alt="Photo de la rue de la République piétonne à Lyon, façades en pierre et platanes" width="100%"></td>
    <td><img src="docs/media/readme/0.3-rue-republique-wide.jpg" alt="En jeu : course dans la rue de la République entre façades en pierre, vitrines et platanes" width="100%"></td>
  </tr>
  <tr>
    <td><sub>Photo : Corentin Eustacchi, <a href="https://commons.wikimedia.org/wiki/File:Rue_de_la_R%C3%A9publique_Nord_pi%C3%A9tonne_(Lyon,_2025).jpg">Wikimedia Commons</a>, CC BY-SA 4.0, recadrée.</sub></td>
    <td><sub>Hauteurs réelles issues du LiDAR de l'IGN (erreur médiane des toits 0,39 m), 175 immeubles, 13 000 modules de façade, 112 platanes.</sub></td>
  </tr>
</table>

Ce qui manque encore au jeu face à la photo, honnêtement : les feuilles des
platanes, les vraies enseignes, une lumière crédible à l'ombre, et une ville
qui continue au-delà de la tranche. La ville passe aux outils procéduraux
de City Sample d'Epic ; les images ci-dessous sont antérieures à ce
travail.

## En images

Ce sont des images extraites des vidéos que le jeu a enregistrées de
lui-même pendant ses tests automatiques, sur un MacBook Pro M1 Pro
(versions 0.2 et 0.3, avant le passage au PC). Rien n'est retouché. Elles
montrent la rue, pas les personnages ni la circulation actuels : de
nouvelles captures arriveront une fois le jalon A atteint.

<table>
  <tr>
    <td width="50%"><img src="docs/media/readme/0.3-driving-rue-republique.jpg" alt="En jeu : au volant dans la rue de la République, vue à la troisième personne" width="100%"><br><sub>Au volant dans la rue de la République (0.3)</sub></td>
    <td width="50%"><img src="docs/media/readme/0.2-carjack-approach.jpg" alt="En jeu : le personnage s'approche d'une voiture garée pour la voler" width="100%"><br><sub>S'approcher d'une voiture garée pour la prendre (0.2)</sub></td>
  </tr>
  <tr>
    <td><img src="docs/media/readme/0.3-square-benches.jpg" alt="En jeu : une petite place avec des bancs en fonte, des platanes et un lampadaire lyonnais" width="100%"><br><sub>Bancs en fonte et lampadaire lyonnais, là où sont les vrais (0.3)</sub></td>
    <td><img src="docs/media/readme/0.3-plane-trees-lamppost.jpg" alt="En jeu : à pied entre les platanes et les bancs, une voiture garée à droite" width="100%"><br><sub>À pied entre les platanes (0.3)</sub></td>
  </tr>
  <tr>
    <td><img src="docs/media/readme/0.3-on-foot-crossroads.jpg" alt="En jeu : un carrefour avec un immeuble d'angle, des bancs et une voiture au loin" width="100%"><br><sub>Un carrefour, la circulation au loin (0.3)</sub></td>
    <td><img src="docs/media/readme/real-quai-jean-moulin.jpg" alt="Photo du quai Jean-Moulin le long du Rhône, platanes, circulation et promenade" width="100%"><br><sub>Le vrai quai Jean-Moulin au bord du Rhône, au bout du trajet de la 0.3. Photo : Corentin Eustacchi, <a href="https://commons.wikimedia.org/wiki/File:Quai_Jean-Moulin_(Lyon,_2025).jpg">Wikimedia Commons</a>, CC BY-SA 4.0, recadrée.</sub></td>
  </tr>
</table>

## Comment il est fait

Au-delà du jeu lui-même, l'intérêt du projet est dans la façon dont il est
fabriqué et vérifié.

**Réutiliser avant de coder.** Le gameplay repose sur les briques gratuites
d'Epic plutôt que sur des systèmes maison : le Game Animation Sample pour
les déplacements, City Sample pour la foule, les voitures et la ville
procédurale, le framework Mass pour des centaines de piétons et de
voitures, Lyra pour le combat, Chaos Vehicles, StateTree, Perception et EQS
pour la police. Le C++ maison (environ 59 000 lignes en deux modules) les
relie à la ville de Lyon et aux règles du jeu.

**La ville à partir de données ouvertes.** Des outils Python transforment
les données de l'IGN, de la Métropole de Lyon et d'OpenStreetMap en niveau
Unreal : immeubles à leur hauteur LiDAR, façades, trottoirs, arbres et
mobilier urbain à leurs vraies coordonnées, voies de circulation pour le
trafic Mass. Voir [tools/city](tools/city/) et
[tools/geo-pipeline](tools/geo-pipeline/).

**Chaque changement est joué avant d'entrer.** Un changement n'arrive sur
la branche `next`, puis sur `main`, qu'après une *porte* sur le PC de
référence : le jeu est compilé et packagé, puis il se joue lui-même
pendant plusieurs minutes dans une dizaine de configurations. La porte
échoue sur tout plantage, sur une régression du temps d'image (médiane de
la logique de jeu au plus 1,3 fois celle de la porte précédente,
actuellement environ 13 ms), sur un comptage de circulation cassé, ou
quand le parcours automatique `-GTARun` rate plus d'une de ses sept
étapes : voler une voiture, être vu par un témoin, attirer la police, être
poursuivi, braquer quelqu'un, etc. Plus de cinquante candidats sont passés
par cette porte depuis le 9 novembre 2026 ; les résultats sont dans le
[tableau de bord](production/roadmap-status.fr.md).

**Des mesures honnêtes.** Un test qui passe n'est pas pris pour une preuve
tant qu'on n'a pas vérifié ce qu'il mesure. Deux exemples tirés des
journaux : le comptage de la circulation incluait des voitures couchées
sur le flanc, et un test de témoin passait avec un modèle de voiture que
le correctif ne concernait pas. Les deux ont été repérés et les tests
réécrits.

**Un dépôt propre.** L'intégration continue lance les tests des outils et
un contrôle de provenance à chaque push (identités des commits, fichiers
suivis, noms de branches). L'historique est linéaire, en avance rapide
uniquement. Les logos de marques, les contenus d'Epic et les tables brutes
de la ville restent hors de Git pour des raisons de licence.

| Domaine | Choix |
| ---- | ---- |
| Moteur | Unreal Engine 5.8 : Lumen, Nanite, Virtual Shadow Maps, TSR |
| Code | C++ pour les systèmes de jeu, Python pour les outils de ville et de build, Blueprint pour les prototypes |
| Machine de référence | Windows 11, Ryzen 9 3900, RTX 3070 Ti, DirectX 12 |
| Versions précédentes | macOS sur Apple Silicon (Metal), versions 0.1 à 0.3 |
| Décisions d'architecture | [docs/adr](docs/adr/), 13 fiches de décision |

Toute la documentation, en français et en anglais :
**[docs/README.fr.md](docs/README.fr.md)**.

## Feuille de route

Le jeu d'abord, et l'image suit ce que la caméra voit. La tranche se
livre en trois jalons, chacun jugé sur une vidéo sans montage :

| Quand (ordre de grandeur) | Quoi |
| ---- | ---- |
| Vers le 4 décembre 2026 | **A.** Voler une voiture et fuir la police sur un circuit de fuite fixe, sur PC, en vidéo sans montage |
| Fin décembre 2026 | **B.** Dix minutes de jeu qui font GTA : circulation, passants qui réagissent, combat, étoiles de recherche, poursuite |
| Fin janvier 2027 | **C.** Cinq paires photo réelle / capture du jeu qui tiennent |
| Février 2027 | Le nord de la Presqu'île, de Bellecour aux Terreaux |
| Mars 2027 | Portage Mac |
| Ensuite | L'histoire : vols de spécialités lyonnaises et fuites par les traboules |

D'ici là, missions, menus, intérieurs, sauvegardes et audio sont gelés. Les
assets restent **uniquement gratuits** (Megascans, MetaHuman, City Sample,
contenus Fab gratuits, fait maison). Détails :
[feuille de route](production/roadmap.fr.md),
[tableau de bord](production/roadmap-status.fr.md),
[concept du jeu](design/gdd/game-concept.fr.md).

## Jouer ou compiler

**La dernière version publique est la [0.3.0 pour macOS](https://github.com/nic0lasm/Gran-Theft-Praline/releases/tag/v0.3.0)**
(Apple Silicon, 10 novembre 2026) : voler une voiture et rouler dans la
circulation de la rue de la République jusqu'au Rhône. Elle est antérieure
au travail sur PC décrit plus haut. La prochaine version viendra avec le
jalon B. [Notes de version](docs/releases/0.3.fr.md), avec la marche à
suivre pour ouvrir une app non signée.

<details>
<summary><b>Le compiler soi-même</b></summary>

Prérequis : Unreal Engine 5.8, Visual Studio avec les outils C++ (Windows)
ou Xcode (Mac), [Git LFS](https://git-lfs.com/) et Python 3.

Certains contenus **ne sont pas dans Git**, pour des raisons de licence ou
de taille :

| Contenu | Comment l'obtenir |
| ---- | ---- |
| Game Animation Sample et City Sample d'Epic | Gratuits sur Fab, puis [`tools/character/install_gasp_content.sh`](tools/character/README.md) |
| Le niveau rue de la République | Généré à partir des données publiques par [`tools/city`](docs/city/plan.fr.md) et [`tools/level`](tools/level/README.md) |
| Données de la ville (tables OpenStreetMap) | Régénérées par [`tools/city`](docs/city/plan.fr.md) (ODbL, sans clé) |
| Vrais logos et textures de marques | Gardés hors de Git |

```zsh
git lfs install
git clone https://github.com/nic0lasm/Gran-Theft-Praline.git
```

Package Windows : `powershell -ExecutionPolicy Bypass -File tools\build\package-win.ps1`.
Package et test macOS : `tools/build/package-mac.sh` puis
`tools/build/test-build-mac.sh`. Guide :
[compilation en un clic](docs/build/one-click-build.fr.md).

</details>

<details>
<summary><b>Commandes</b> (clavier et manette)</summary>

| Action | Clavier (AZERTY) | Clavier (QWERTY) | Manette |
| ---- | ---- | ---- | ---- |
| Se déplacer | Z Q S D | W A S D | Stick gauche |
| Regarder | Souris | Souris | Stick droit |
| Sprint (maintenir) | Maj gauche | Maj gauche | A / Croix |
| Sauter, enjamber | Espace | Espace | X / Carré |
| Première / troisième personne | V | V | View / Share |
| Monter / descendre d'une voiture | F | F | Y / Triangle |
| Accélérer / freiner, reculer | Z / S | W / S | Gâchette droite / gauche |
| Tourner | Q / D | A / D | Stick gauche |
| Frein à main | Espace | Espace | RB / R1 |

Détails : [personnage](docs/gameplay/player-character.fr.md),
[véhicules](docs/gameplay/vehicles.fr.md), [combat](docs/gameplay/combat.fr.md).

</details>

<details>
<summary><b>Contribuer</b></summary>

L'aide est la bienvenue, et pas besoin d'écrire du code : signaler une
boutique manquante ou une hauteur d'immeuble fausse, ou dire comment une
version tourne sur votre machine. Ouvrez une
[issue](https://github.com/nic0lasm/Gran-Theft-Praline/issues) pour lancer
la discussion, surtout avant un gros chantier.

- Branches : `feat/<sujet>` ou `fix/<sujet>` ; `next` est la préproduction,
  `main` la production ; l'historique reste linéaire.
- Un changement passe sur `next`, puis `main`, seulement quand la CI est
  verte sur ce commit précis et que la porte du PC est passée.
- Code, commits et identifiants en anglais ; documentation en français et
  en anglais (`fichier.md` + `fichier.fr.md`).
- Chaque asset externe a sa ligne dans [assets/SOURCES.md](assets/SOURCES.md) ;
  les gros binaires passent par Git LFS ([règles](docs/project/git-lfs.fr.md)).

</details>

## Crédits et licence

- Données géographiques : contient des données de l'**IGN** (BD TOPO,
  LiDAR HD, RGE ALTI) et de la **Métropole de Lyon** (Licence Ouverte /
  Etalab), et **© les contributeurs d'OpenStreetMap**
  ([ODbL](https://opendatacommons.org/licenses/odbl/)).
- Photos de la vraie ville dans ce README : **Corentin Eustacchi**,
  Wikimedia Commons, [CC BY-SA 4.0](https://creativecommons.org/licenses/by-sa/4.0/deed.fr),
  recadrées et redimensionnées ; les deux fichiers gardent cette licence.
- **Epic Games** : Unreal Engine, le Game Animation Sample, City Sample et
  Lyra sont utilisés selon le CLUF d'Unreal Engine ; ils ne sont pas dans
  ce dépôt et ne sont pas couverts par sa licence.
- Les enseignes et marques appartiennent à leurs propriétaires et ne sont
  pas distribuées ici. Aucune personne réelle n'est représentée dans le
  jeu, et aucun asset ne vient d'un autre jeu. Détails :
  [THIRD_PARTY_NOTICES.fr.md](THIRD_PARTY_NOTICES.fr.md),
  [assets/SOURCES.md](assets/SOURCES.md).

Le code et la documentation de ce dépôt sont publiés sous
[licence MIT](LICENSE). Les données et assets tiers gardent leurs propres
licences.

<p align="center"><i>Fait à Lyon, avec amour pour ses rues.</i></p>
