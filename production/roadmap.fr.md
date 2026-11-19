# Roadmap : Gran Theft Praline

*Version anglaise : [roadmap.md](roadmap.md)*
*Version 5, 2026-11-19 : bilan à une semaine du recadrage. La tranche est
scindée en trois jalons, le jeu passe avant l'image. Remplace la version 4
(12 novembre). Le [plan vers la phase de test 0.9](test-phase-plan.fr.md)
reste en pause. Les versions précédentes restent dans l'historique Git.*
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
fuir la police.**

L'histoire vient **après**. La piste est posée : des vols de produits
lyonnais et des fuites par les traboules. Elle ne sera écrite que lorsque la
priorité 1 tient son critère.

## 2. Où on en est, honnêtement (19 novembre)

`next` et `main` sont sur le candidat 46. Tout ce qui suit est tiré des
comptes rendus de porte du jeu packagé sur le PC de référence ; ce qui n'a
pas été mesuré est écrit comme tel.

### Prouvé dans le jeu packagé

- **Stable.** Six portes passées de suite les 15 et 16 novembre (candidats
  40 à 46), sans aucun plantage sur les 7 à 9 passes de chaque porte.
  Une image dure environ 15 ms sur le PC (mesure du 14 novembre), dont
  11,8 ms de logique de jeu au candidat 46.
- **Un parcours automatique de 7 étapes réussit 7 sur 7** depuis le
  candidat 41 : vol d'une voiture en circulation, poursuite, vol d'une
  voiture garée avec alarme, marche arme en main, braquage d'un passant de
  loin et de près, tir.
- **Trafic** : une centaine de voitures en mouvement sur 120 simulées,
  tenues sur 300 s.
- **Les modèles d'Epic ont remplacé les nôtres** : le joueur n'est plus un
  mannequin (Nicolito, candidat 42), la foule est faite de personnages
  City Sample (candidat 43), le trafic et la police roulent en voitures
  City Sample (13 novembre).
- **Poursuite** : 3 étoiles, 5 unités de police, 285 m de fuite et 73 km/h
  en pointe au candidat 46. La cible est de 105 à 135 km/h, et le pilote
  automatique du test est remis sur une voie quand il se coince.

### Pas prouvé, ou pas là

- **Personne n'a joué.** Aucun essai manette en main n'est consigné depuis
  la 0.3 (10 novembre). La sensation de conduite, de tir et de vol n'a été
  jugée par personne.
- **La conduite était cassée du 13 au 16 novembre** : les quatre roues des
  voitures City Sample étaient posées au même point, d'où le patinage et la
  caisse qui frotte la route. Le correctif est dans le candidat 47, dont la
  porte tourne au moment où ce bilan est écrit.
- **La police suit-elle vraiment ?** Sa vitesse moyenne en poursuite était
  de 1 km/h au candidat 43. Elle n'a pas été remesurée depuis : on ne sait
  pas.
- **Les piétons ne réagissent pas de façon lisible.** Les mains en l'air
  sont illisibles sur les images du PC, et les animations de peur (fuite,
  accroupi) n'étaient pas chargées au candidat 40 ; non revérifié depuis.
- **Aucun test packagé ne couvre** : les feux et l'évitement du trafic, les
  témoins qui appellent la police, le combat à mains nues, semer la police,
  l'arrestation, la mort et la réapparition. Le code existe depuis le
  13 novembre ; son comportement en jeu packagé : on ne sait pas.
- **L'image n'a pas été mesurée depuis le 12 novembre.** La seule planche
  des 5 paires date du candidat 17b (1 sur 5 à deux vues, 0 aux trois
  autres). Depuis : vitrines et éclairage de nuit (13 novembre), joueur,
  foule et voitures d'Epic. Aucune seconde planche, aucun travail d'image
  sur `main` depuis le 13 novembre, et aucune capture n'est sortie du PC
  depuis cette date. Le niveau réel de l'image aujourd'hui : on ne sait
  pas. La génération de ville par les briques d'Epic (CitySamplePCG) n'a
  jamais été essayée, le décor lointain jamais ouvert dans l'éditeur.
- **La vidéo des 10 minutes n'est pas tournée.**

### Pourquoi on en est là

- **Sept jours depuis la version 4, quatre de travail réel** : une journée
  pour déménager le projet sur un disque dédié (14 au 15 novembre), deux
  jours d'arrêt complet (16 au 18 novembre).
- **Remplacer le fait maison par les modèles d'Epic était le bon choix,
  mais chaque remplacement a cassé quelque chose** (roues des voitures,
  matériau des têtes, mains en l'air) et a coûté un à trois jours de
  diagnostic par les journaux.
- **Un seul PC, une seule file.** Rien ne compile ailleurs ; chaque essai
  attend son tour.
- **La porte mesurait le robot de test plus que le jeu.** Une étape pouvait
  réussir sans prouver le comportement : la poursuite du candidat 41 était
  validée avec une voiture à l'arrêt.
- **La règle « 5 paires par candidat » n'a pas été tenue** une fois
  l'ordre des priorités fixé, alors que la version 4 gardait l'image comme
  critère à égalité.

## 3. La cible : la tranche en trois jalons

Le terrain ne change pas : **300 à 400 m de rue, de Bellecour aux
Jacobins**, sur PC. La tranche de la version 4 ne tiendra pas en entier
fin novembre. Elle est scindée, dans l'ordre des priorités.

### Jalon A : voler une voiture et fuir la police (fin novembre 2026)

C'est fini quand, sur le package PC :

1. On vole une voiture, en circulation ou garée, et elle se conduit bien :
   0 à 100 km/h mesuré, pointe entre 105 et 135 km/h, caisse qui ne touche
   plus la route.
2. Les étoiles montent, et la police suit vraiment : on mesure la vitesse
   moyenne de ses unités, pas seulement leur nombre.
3. On la sème, ou on se fait arrêter.
4. **Quelqu'un y joue manette en main** et dit que ça ressemble à GTA.
   Preuve : une vidéo de 3 minutes sans montage, sortie du PC.

### Jalon B : les 10 minutes (mi-décembre 2026)

Le critère de jeu de la version 4, en entier : trafic qui s'arrête aux
feux et s'écarte, passants qui vivent et réagissent (fuient, lèvent les
mains, appellent la police), combat, étoiles, arrestation, mort et
réapparition. Chaque comportement a son test dans le jeu packagé. Preuve :
**une vidéo des 10 minutes**, sans montage.

### Jalon C : la rue tient la comparaison (fin janvier 2027)

Les **5 paires photo réelle / capture du jeu** aux mêmes points de vue :
entrée côté Bellecour, façade du cinéma, vitrines à hauteur d'homme,
bassin de la place de la République, nuit devant le cinéma.

Une seconde planche est prise **tout de suite**, pour savoir où est
l'image : c'est une mesure, pas un chantier. Le travail d'image reprend
après le jalon B, par un essai de génération de ville avec les briques
d'Epic.

### À chaque porte

Package PC sans plantage, comme aujourd'hui.

### Gelé jusqu'au jalon B

Missions, audio (au-delà de l'existant), menus, intérieurs, sauvegarde de
partie, mini-carte, remappage des touches, crédits. Le fast-food reste en
maintenance. Le build de test du quartier (0.9) est reporté. Les systèmes
gelés ne reçoivent que des corrections de plantage.

## 4. Comment on travaille

| Règle | Ce que ça veut dire |
| ---- | ---- |
| **PC d'abord** | Un changement entre sur `next` quand la CI est verte, que le projet compile sur le PC, qu'un package PC tourne 120 s sans plantage et que les seuils de la porte tiennent (temps d'image, parcours automatique, trafic). |
| **Un test prouve un comportement** | Chaque étape du parcours automatique mesure ce que le joueur verrait (mètres roulés, vitesse, mains levées à l'écran), pas seulement un état interne. Un 7 sur 7 ne vaut pas un essai manette en main. |
| **Les preuves sortent du PC** | Une capture ou une vidéo qui reste sur le PC ne prouve rien pour le projet. Images et vidéos des jalons sont versées avec le compte rendu, logos floutés. |
| **Un remplacement à la fois** | Quand un modèle d'Epic remplace du fait maison, il arrive seul dans son candidat, avec la mesure qui montre que rien n'a régressé. |
| **Assets gratuits seulement** | Zéro euro : Quixel Megascans, MetaHuman, City Sample, Fab gratuit, fait maison. Chaque asset a sa ligne de licence dans [assets/SOURCES.md](../assets/SOURCES.md). |
| **Mac ensuite** | Le MacBook Pro M1 Pro ne bloque pas les fusions. Le portage Mac jouable vient après le quartier. |
| **Trois chantiers de jeu, un de machines** | Conduite et trafic ; piétons ; police ; machines (compilation, packages, portes). L'image est en pause jusqu'au jalon B, sauf la planche de mesure. |

## 5. Calendrier

Des ordres de grandeur, revus à chaque jalon, sans promesse au-delà.

| Quand | Quoi | Version 4 |
| ---- | ---- | ---- |
| **Fin novembre 2026** | Jalon A : voler une voiture et fuir la police | Toute la tranche |
| **Mi-décembre 2026** | Jalon B : les 10 minutes | Fin novembre |
| **Fin janvier 2027** | Jalon C : les 5 paires d'image | Fin novembre |
| **Février 2027** | Le nord de la Presqu'île, de Bellecour aux Terreaux (~1,5 km²), au niveau du jalon B. À confirmer après le premier essai de génération de ville | Mi à fin décembre |
| **Mars 2027** | Portage Mac | Février |
| **Printemps 2027, avec une forte marge** | Un quartier jouable et beau sur PC | Vers février |
| Ensuite | Le scénario : vols de produits lyonnais, fuites par les traboules | |

Hors de portée de ce plan : tout Lyon, les cinématiques.

## 6. Prochaines étapes

1. **Candidat 47** (porte en cours) : roues des voitures City Sample à
   leur place, animation des mains en l'air trouvée par le jeu, bornes
   retirées de la chaussée.
2. **Candidat 48** : centre de masse des voitures (la voiture de police se
   cabre), couple de la voiture du joueur, test de marche armée fiabilisé.
3. Mesurer la vitesse de la police en poursuite, puis corriger.
4. Seconde planche des 5 vues et première vidéo de fuite, sorties du PC.
5. Premier essai manette en main du jalon A.
6. Animations de peur de la foule, mains en l'air lisibles.

## 7. Risques

| Risque | Parade |
| ---- | ---- |
| Le PC unique est éteint ou sa file est bloquée | Les lots s'écrivent sans lui et attendent groupés ; tout arrêt de plus d'une journée est signalé avec son effet sur les dates |
| Un nouvel arrêt complet du travail (deux jours perdus du 16 au 18 novembre) | Dates données avec leur marge ; glissement annoncé le jour même |
| Un test passe sans prouver le comportement | Règle « un test prouve un comportement » ; chaque porte relit ce que l'étape a mesuré |
| La conduite ou la poursuite déçoivent manette en main, découvert tard | L'essai manette en main est le critère du jalon A, pas une étape après |
| L'image est plus loin qu'on ne le croit | Planche de mesure tout de suite ; méthode de ville décidée sur un essai, pas sur un plan |
| Un remplacement par un modèle d'Epic casse un système | Un remplacement par candidat, avec sa mesure |
| Le PC (8 Go de mémoire vidéo) limite l'image | Budgets mesurés à la reprise de l'image |

## 8. Ce qui est livré

| Version | Contenu | Sortie |
| ---- | ---- | ---- |
| 0.1 Jouer en un clic | Lancer le jeu et marcher rue de la République | 9 nov. 2026 |
| 0.2 La rue habillée | Une rue fidèle et habillée, jouable sur un portable M1 | 10 nov. 2026 |
| 0.3 Au volant | Voler une voiture, traverser la circulation jusqu'au quai du Rhône | 10 nov. 2026 |

La prochaine version publique sortira quand le jalon B est tenu ; son
numéro sera fixé à ce moment.
