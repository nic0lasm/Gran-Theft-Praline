# Roadmap : Gran Theft Praline

*Version anglaise : [roadmap.md](roadmap.md)*
*Version 6, 2026-11-24 : bilan après 18 portes en deux jours. L'objectif et
les jalons ne changent pas. Ce qui change : le terrain jugé (un circuit de
fuite fixe), le juge (une grille de 10 points sur la vidéo), la porte (une
courte, une complète) et l'organisation (deux chantiers de contenu au lieu
de sept). Remplace la version 5 (19 novembre). Le
[plan vers la phase de test 0.9](test-phase-plan.fr.md) reste en pause. Les
versions précédentes restent dans l'historique Git.*
*Suivi au jour le jour : [roadmap-status.fr.md](roadmap-status.fr.md).
Liste de contrôle des systèmes : [systems-inventory.fr.md](systems-inventory.fr.md).
Concept : [game-concept.fr.md](../design/gdd/game-concept.fr.md).*

---

## 1. L'objectif

**Priorité 1 : un GTA-like réaliste du centre de Lyon.** Les mécaniques de
GTA (marcher, voler une voiture, conduire dans le trafic, se battre, être
recherché par la police et la semer) et l'intelligence de ses personnages non
joueurs (des piétons qui réagissent, un trafic crédible, une police qui
traque), dans une rue de Lyon qui ressemble à la vraie.

**Ordre des priorités, fixé le 13 novembre :** les personnages non joueurs
d'abord, puis les sensations (conduite, tir, vol de voiture), puis l'image.
Le premier moment qui doit faire dire « c'est GTA » : **voler une voiture et
fuir la police.** Depuis le 23 novembre, l'image avance en parallèle du jeu,
sur ce que la caméra voit pendant ce moment-là.

L'histoire vient **après**. La piste est posée : des vols de produits
lyonnais et des fuites par les traboules. Elle ne sera écrite que lorsque la
priorité 1 tient son critère.

## 2. Où on en est, honnêtement (24 novembre)

`next` et `main` sont sur le candidat 76. Tout ce qui suit est tiré des
journaux de porte du jeu packagé sur le PC de référence et des images des
vidéos des candidats 76 et 77 ; ce qui n'a pas été mesuré est écrit comme
tel.

### Prouvé dans le jeu packagé, vidéo regardée

- **Le vol.** Le joueur prend une voiture en circulation en 6 secondes
  (candidat 76).
- **La police se reconnaît** depuis le candidat 75 : voiture blanche à bande
  bleue, sommation « POLICE ! NE BOUGEZ PLUS ! » à l'écran, arrestation à
  63 secondes au candidat 76.
- **La fuite atteint 89 km/h en pointe** (candidat 76). À l'essai de
  lancement, la voiture du joueur passe de 0 à 100 km/h en 6,6 secondes.
- **Les étoiles, la mini-carte et le compteur** sont à l'écran.
- **Du trafic autour** : au moins 8 voitures en mouvement à moins de 150 m
  pendant la fuite (12,4 au candidat 77).
- **Stable** : aucun plantage, aucune ligne d'erreur sur les 12 passes de la
  porte 76 ; la logique de jeu tient en 11 ms environ.
- **La place habillée ressemble à une rue** (candidat 77, 90 secondes) :
  façades à balcons, vitrines, passants qui fuient.

### Pas là

- **La fuite est lente en moyenne** : 35 km/h au candidat 76, 25 au 77.
  Elle est conduite par un pilote automatique qui se coince : sur les quatre
  minutes filmées, la voiture a été reposée sur une voie 4 fois au 76 et
  8 fois au 77.
- **La fuite quitte la rue habillée en quelques secondes.** À 89 km/h, les
  300 à 400 m habillés sont traversés en 16 secondes. Le reste se joue entre
  des volumes de fond et sur un quai nu.
- **Le lointain est vide** : sol nu, blocs sans façade, bande bleue sous
  l'horizon (candidat 76, 60 secondes). Un lot « lointain » est écrit ; il
  n'a pas encore été vu en jeu.
- **La caméra** passe sur le toit de la voiture, dans les murs (image noire
  à 110 secondes au 77), ou derrière un lampadaire qui couvre l'écran.
- **Des personnages au crâne rasé**, des corps sous les voitures.
- **Les unités de police roulent à 17 à 21 km/h de moyenne**, pour 25
  attendus.
- **Personne n'a joué.** Aucun essai manette en main n'est consigné depuis
  la 0.3 (10 novembre).
- **Pas relu pour ce bilan** : feux, combat à mains nues, mort et
  réapparition, semer la police.
- **Image** : aucune seconde planche des 5 paires depuis le 12 novembre.

### Pourquoi on en est là

Du 22 novembre 15 h 43 au 24 novembre 11 h 30, le PC a passé 18 portes,
soit 24 heures de calcul. Ce n'est plus le PC qui manque : c'est ce que les
portes jugent, et la surface sur laquelle on travaille.

1. **Une porte dure 81 minutes en moyenne.** Celle du 76 : 23 minutes de
   compilation, 10 de package, 41 pour 12 passes, puis la vidéo. Le
   parcours automatique y est joué cinq fois (trois passes, deux vidéos).
   Deux passes, 12 minutes, mesurent un second système de trafic qui n'est
   pas dans le jeu par défaut.
2. **Les critères chiffrés et « ça ressemble à GTA » sont deux choses.**
   Sur les 13 dernières portes, six ont échoué sur un chiffre : cinq de
   suite (65 à 69) pour des lignes d'erreur, dont trois pour le même shader
   de peinture non cuit, et une (74) pour 7,6 voitures au lieu de 8. Sur
   les sept qui tenaient tous les critères, deux (72 et 77) ont été
   refusées en regardant la vidéo : un taxi new-yorkais
   et une mémoire vidéo saturée ; 14 modules de façade retirés par une
   étape de préparation. Le seul vrai juge est le regard sur la vidéo, et
   il arrive au bout de 81 minutes.
3. **Un terrain sans bord.** La fuite est libre sur les 235 voies de la
   carte, alors que 300 à 400 m sont habillés. Chaque mur, poteau ou fosse
   de tout le centre peut coincer le pilote automatique ou donner une image
   vide. Trois chantiers ont écrit chacun leur parade (voies bloquées
   apprises, police qui contourne le décor, mobilier retiré des voies,
   fosses couvertes, façades retournées) ; c'est l'une d'elles qui a troué
   la carte au 77.
4. **La carte est un fichier binaire hors de Git**, modifié avant chaque
   porte par des étapes de plusieurs chantiers. L'état d'une porte
   dépendait de la précédente.
5. **Trop de mains pour un seul goulot.** Sept chantiers de contenu
   (police, trafic, piétons, combat, habillage, image, assets) nourrissent
   une file et un PC. Depuis le 22 novembre, 37 des 93 commits de `main` ne
   modifient que la spécification de porte, et le texte produit par une
   porte est passé de 0,5 Mo (70) à 0,9 Mo (77).
6. **Rien ne compile le C++ avant le PC.** Trois candidats (73, 78, 79) ont
   été abandonnés ou refaits pour une erreur de compilation.

## 3. Ce qui change

### 3.1 Un circuit de fuite, et rien d'autre jusqu'au jalon A

La fuite du jalon A se joue sur **une boucle fixe de 1,5 à 2,5 km**, choisie
une fois, hors du jeu, sur le fichier des voies :

- elle part du lieu du vol et passe par la rue et la place habillées ;
- au moins 70 % de grands axes (chaussée de 7 m et plus), aucune impasse ;
- ses voies sont vérifiées libres de tout décor fixe avant la porte, sur
  les données, pas en jeu ;
- elle est versée dans Git, comme liste de voies.

Le pilote automatique de la vidéo suit cette boucle : il ne tire plus de
voie au hasard et n'est plus jamais reposé. La police, elle, poursuit
librement.

Tout le travail de décor et d'image se concentre sur **le couloir** : ce
que la caméra voit depuis la boucle. Hors du couloir, un défaut est noté,
pas corrigé.

### 3.2 Une grille de 10 points, lue sur la vidéo

La porte filme la fuite sur le circuit. On en tire une image toutes les
10 secondes sur les 3 premières minutes. Chaque point est tenu ou non.

| # | Ce qu'on doit voir | Jugé sur |
| ---- | ---- | ---- |
| 1 | **Le vol** : le joueur prend une voiture en circulation en moins de 10 s | journal, image |
| 2 | **Ça roule** : 50 km/h de moyenne au moins sur la première minute de fuite, 90 km/h en pointe | journal |
| 3 | **Sans aide** : aucune remise sur voie, aucun arrêt de plus de 3 s contre le décor | journal |
| 4 | **La police est là** : une voiture de police reconnaissable à l'image pendant la fuite, ses unités à 25 km/h de moyenne au moins | images, journal |
| 5 | **Du monde** : 8 voitures en mouvement au moins à moins de 150 m | journal |
| 6 | **Une fin lisible** : arrestation ou police semée, annoncée à l'écran, avant 120 s | journal, image |
| 7 | **La caméra tient** : jamais dans un mur ni dans une voiture, aucune image noire, aucun objet de premier plan sur plus d'un quart de l'écran | images |
| 8 | **Pas de vide** : aucune image ne montre un sol nu, un horizon sans rien ou des blocs sans façade | images |
| 9 | **Des personnages entiers** : ni crâne rasé par défaut, ni pose figée, ni corps sous la voiture | images |
| 10 | **Fluide et propre** : 95 % des images sous 33 ms pendant la passe filmée, aucun texte du moteur à l'écran | journal, images |

Le candidat 76 en tient 3 sur 10 (points 1, 5 et 6). Le jalon A, c'est
10 sur 10, puis l'essai manette en main.

Un candidat annonce en une phrase **ce que la vidéo montrera de
différent**, et quels points il vise. Un candidat qui ne change rien à la
grille attend le suivant.

### 3.3 Deux portes au lieu d'une

| | Porte courte | Porte complète |
| ---- | ---- | ---- |
| Quand | chaque candidat | une fois par jour, sur la dernière tête passée en porte courte ; avant toute version publique |
| Contenu | compilation, package, passe par défaut, parcours automatique filmé sur le circuit (la passe mesurée **est** la vidéo) | les passes d'aujourd'hui, sans celles du second trafic |
| Durée | environ 40 min (estimation, à mesurer) | environ 70 min |
| Fait avancer | `next` | `main` |
| Bloque sur | plantage, ligne d'erreur, temps d'image, parcours à moins de 6 sur 7, grille en recul | la même chose, plus les mesures de décomposition |

- **Un seuil chiffré manqué de moins de 10 % ne fait pas échouer la porte à
  lui seul** : c'est la grille qui tranche (porte 74 : 7,6 pour 8).
- **Une branche est compilée sur le PC avant d'entrer dans un candidat**,
  en tâche de fond, pas à la porte.
- **Un résumé de porte tient en une page** : la grille, six chiffres, les
  images. Le rapport complet reste sur disque pour les recherches.

### 3.4 La carte se rejoue

La carte d'un candidat, c'est **une carte de base figée** (repérée par son
empreinte) **plus des étapes rejouées dans l'ordre**, toutes dans Git.
Jamais le résultat de la porte précédente. Une étape qui retire des
acteurs porte son contrôle (nombre de modules de façade inchangé). La base
n'est refigée qu'après une porte passée et une vidéo regardée.

### 3.5 Deux chantiers de contenu au lieu de sept

| Chantier | Ce qu'il possède |
| ---- | ---- |
| **Le jeu** | La séquence entière : vol, conduite du joueur, trafic, police, caméra de poursuite, passants du circuit. Un bug de la fuite a un seul propriétaire. |
| **Le décor** | La carte et ses étapes, l'habillage du couloir, les matériaux, le lointain, les assets gratuits. |
| **Les machines** | La file du PC, les deux portes, la grille, les vidéos. |

### 3.6 Ce qui s'arrête jusqu'au jalon A

- Les passes et le travail sur le second système de trafic (MassTraffic) :
  le code reste, la décision se reprend au jalon B
  ([ADR-0015](../docs/adr/adr-0015-mass-crowd-and-traffic.fr.md)).
- Toute correction de décor ou de voie hors du couloir.
- Les sondes et relevés qui expliquent un défaut que la grille ne voit pas.
- Les armes et le combat, hors caméra.

## 4. Les jalons

Le terrain de la tranche ne change pas : **300 à 400 m de rue, de Bellecour
aux Jacobins**, sur PC. Le circuit de fuite la traverse et la déborde.

### Jalon A : voler une voiture et fuir la police (vers le 4 décembre 2026)

Trois marches, chacune jugée sur la vidéo de porte :

| Marche | Visé | Points de la grille |
| ---- | ---- | ---- |
| **A1. Le circuit tient** : la boucle est choisie, le pilote la suit sans aide, la police suit | 27 novembre | 1 à 6 |
| **A2. Ce que la caméra voit** : caméra, lointain, couloir habillé, personnages | 1er décembre | 7 à 9 |
| **A3. La vidéo** : 3 minutes sans montage, sorties du PC, puis l'essai manette en main | 4 décembre | 10, puis le joueur |

Le jalon est tenu quand **quelqu'un y joue manette en main** et dit que ça
ressemble à GTA. Un essai de cinq minutes plus tôt, sur le candidat du
jour, est bienvenu : il dira ce que le pilote automatique ne dit pas.

### Jalon B : les 10 minutes (redaté au jalon A ; ordre de grandeur, fin décembre 2026)

Trafic qui s'arrête aux feux et s'écarte, passants qui vivent et réagissent
(fuient, lèvent les mains, appellent la police), combat, étoiles,
arrestation, mort et réapparition. La grille s'étend à ces comportements.
Preuve : **une vidéo des 10 minutes**, sans montage.

### Jalon C : la rue tient la comparaison (fin janvier 2027)

Les **5 paires photo réelle / capture du jeu** aux mêmes points de vue :
entrée côté Bellecour, façade du cinéma, vitrines à hauteur d'homme,
bassin de la place de la République, nuit devant le cinéma. Une seconde
planche est prise avec la vidéo du jalon A.

### Gelé jusqu'au jalon B

Missions, audio (au-delà de l'existant), menus, intérieurs, sauvegarde de
partie, mini-carte, remappage des touches, crédits. Le fast-food reste en
maintenance. Le build de test du quartier (0.9) est reporté. Les systèmes
gelés ne reçoivent que des corrections de plantage.

## 5. Comment on travaille

| Règle | Ce que ça veut dire |
| ---- | ---- |
| **La vidéo juge** | Un candidat se juge sur la grille de 10 points, lue sur les images de la vidéo de porte. Les chiffres servent la grille, pas l'inverse. |
| **Un terrain borné** | Jusqu'au jalon A, on ne travaille que sur le circuit et son couloir. |
| **Un candidat, une différence visible** | Chaque candidat dit ce que la vidéo montrera de différent. Les lots sont regroupés : six candidats par jour au plus. |
| **PC d'abord** | `next` avance par la porte courte, `main` par la porte complète. Le Mac ne bloque rien. |
| **La carte se rejoue** | Base figée plus étapes dans Git ; jamais d'état hérité de la porte précédente. |
| **Les preuves sortent du PC** | Images et vidéo sont versées avec le résumé de porte, logos floutés. |
| **Capitaliser** | Les modèles et systèmes d'Epic avant le fait maison ; un remplacement à la fois, avec sa mesure. |
| **Assets gratuits seulement** | Zéro euro : Quixel Megascans, MetaHuman, City Sample, Fab gratuit, CC0, fait maison. Chaque asset a sa ligne de licence dans [assets/SOURCES.md](../assets/SOURCES.md). |

## 6. Calendrier

Des ordres de grandeur, revus à chaque jalon, sans promesse au-delà.

| Quand | Quoi | Version 5 |
| ---- | ---- | ---- |
| **27 novembre 2026** | A1 : le circuit tient | |
| **1er décembre 2026** | A2 : ce que la caméra voit | |
| **Vers le 4 décembre 2026** | Jalon A : voler une voiture et fuir la police | Fin novembre |
| **Fin décembre 2026** | Jalon B : les 10 minutes | Mi-décembre |
| **Fin janvier 2027** | Jalon C : les 5 paires d'image | Fin janvier |
| **Février 2027** | Le nord de la Presqu'île, de Bellecour aux Terreaux (~1,5 km²), au niveau du jalon B | Février |
| **Mars 2027** | Portage Mac | Mars |
| **Printemps 2027, avec une forte marge** | Un quartier jouable et beau sur PC | Printemps |
| Ensuite | Le scénario : vols de produits lyonnais, fuites par les traboules | |

Hors de portée de ce plan : tout Lyon, les cinématiques.

## 7. Prochaines étapes

1. Choisir la boucle sur le fichier des voies, la verser dans Git, vérifier
   ses voies contre le décor fixe.
2. Faire suivre la boucle au pilote automatique ; une remise sur voie rend
   la vidéo non valable.
3. Écrire la porte courte et le résumé d'une page ; retirer les passes du
   second trafic.
4. Figer la carte de base et l'ordre des étapes.
5. Unités de police à 25 km/h de moyenne sur la boucle.
6. Caméra de poursuite : ni mur, ni toit, ni premier plan.
7. Lointain et couloir : plus aucune image vide sur la boucle.
8. Personnages : cheveux, poses.
9. Vidéo de 3 minutes, puis essai manette en main.

## 8. Risques

| Risque | Parade |
| ---- | ---- |
| La boucle idéale n'existe pas dans les voies actuelles (impasses, rues étroites) | Elle est choisie sur les données avant tout code ; une boucle de 1,5 km suffit pour 3 minutes avec deux tours |
| Le pilote automatique suit la boucle mais le jeu déçoit manette en main | Essai de cinq minutes dès A1, sans attendre la vidéo finale |
| La porte courte laisse passer une régression de performance | La porte complète quotidienne la voit le jour même ; `main` n'avance que par elle |
| Le lointain coûte trop pour 8 Go de mémoire vidéo | Mesuré par le point 10 de la grille à chaque porte |
| Le PC unique est arrêté | La file tourne seule ; tout arrêt de plus d'une heure est signalé avec son effet sur les dates |
| Le regroupement des chantiers perd un savoir-faire | Chaque chantier fermé laisse sa note de remise ; le code et les outils restent |
| Un défaut hors du couloir gêne plus tard | Il est noté dans le tableau de bord ; il sera traité quand le terrain s'élargit (jalon B) |

## 9. Ce qui est livré

| Version | Contenu | Sortie |
| ---- | ---- | ---- |
| 0.1 Jouer en un clic | Lancer le jeu et marcher rue de la République | 9 nov. 2026 |
| 0.2 La rue habillée | Une rue fidèle et habillée, jouable sur un portable M1 | 10 nov. 2026 |
| 0.3 Au volant | Voler une voiture, traverser la circulation jusqu'au quai du Rhône | 10 nov. 2026 |

La prochaine version publique sortira quand le jalon B est tenu ; son
numéro sera fixé à ce moment.
