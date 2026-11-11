# Roadmap de développement : Gran Theft Praline

*Version anglaise : [roadmap.md](roadmap.md)*
*Version 3, 2026-11-10 : revue après la 0.2. Versions réordonnées selon les chantiers ouverts, budget de performance par système, oublis ajoutés.*
*Version 3.1, 2026-11-11 : la 0.9 devient la build de test du quartier, portes de qualité à chaque fusion, oublis du chemin vers le test. Détail : [test-phase-plan.fr.md](test-phase-plan.fr.md).*
*Liste de contrôle des systèmes : [systems-inventory.fr.md](systems-inventory.fr.md). Suivi : [roadmap-status.fr.md](roadmap-status.fr.md). Assets : [asset-catalog.fr.md](asset-catalog.fr.md).*
*Entrées : [concept](../design/gdd/game-concept.fr.md), [stratégie de rendu](../docs/rendering/rendering-strategy.fr.md),
[ADR-0001](../docs/adr/adr-0001-rendering-pipeline.fr.md), [ADR-0002](../docs/adr/adr-0002-performance-budget.fr.md),
[bible artistique](../docs/art/art-bible.fr.md).*

---

## 1. Comment lire cette roadmap

- Le jeu avance par **versions jouables** : 0.1, 0.2, 0.3… jusqu'à la
  **1.0 = tranche verticale** (nord de la Presqu'île, Bellecour → Terreaux,
  toutes les fonctions P0 du concept en qualité finale).
- **Chaque version se lance en un clic** sur le Mac et apporte quelque chose
  qu'on voit ou qu'on fait en jouant.
- Une version sort quand son critère « fait quand » est vérifié par une
  mesure ou une vidéo, que son **budget de performance** est tenu (§3), que
  la CI est verte et que le commit est avancé sur `next` puis `main`.
  Une version ne retarde pas la suivante pour un système qui n'est pas prêt :
  il passe à la version d'après.
- **Ce qui rythme le projet n'est plus le calendrier mais le Mac.** Le code,
  les données, les assets et la documentation se préparent en parallèle
  dans le cloud ; tout ce qui demande Unreal (import, cuisson, packaging,
  mesure) passe sur un seul MacBook Pro, l'un après l'autre. Les versions
  sont donc ordonnées par **priorité d'accès au Mac**, et les cibles sont
  des ordres de grandeur, revus à chaque version.

### Deux niveaux de qualité : le photoréalisme d'abord

| Profil | Rôle | Critère |
| ---- | ---- | ---- |
| **Ultra (photoréaliste)** | La cible visuelle, sans compromis | Validé par captures comparées aux photos réelles, même à quelques images par seconde sur le M1 |
| **M1 (jouable)** | Ce qu'on joue sur le MacBook Pro M1 Pro 16 Go | **Plafonné à 40 i/s (25,0 ms réguliers), aucune image > 100 ms** (décision du 2026-11-11, mesurée au palier C : 50 piétons, 25 voitures conduites + 24 lointaines, police) ; remplace la cible 30 i/s de l'[ADR-0002](../docs/adr/adr-0002-performance-budget.fr.md), à amender |

Le profil M1 est dérivé du profil Ultra (LOD, densités, réglages), jamais
l'inverse. Toute baisse visible de l'image passe par une carte à Nico.

### Les quatre grandes étapes

| Étape | Versions | Ce qu'on obtient |
| ---- | ---- | ---- |
| **A. La rue vivante** | 0.1 → 0.7 | Une rue de Lyon où l'on marche, conduit dans le trafic, croise des piétons, est poursuivi par la police, de jour comme de nuit, et où l'on se bat |
| **B. Tout le quartier en systèmes** | 0.8 → 0.12 | Bellecour → Terreaux complet, boutiques, argent, sauvegarde, carte et GPS, arsenal, activités : chaque ligne P0 cochée |
| **C. Qualité finale** | 0.13 → 0.16 | Façades et monuments, intérieurs, population et véhicules, son, au niveau Ultra |
| **D. Tranche verticale** | 0.17 → 1.0 | Finition, une heure de jeu sans bloquant, revue de parité |
| Ensuite | 1.x | Nouveaux quartiers, fonctions P1, histoire écrite avec Nico |

### Chantiers continus (toutes les versions)

Ces chantiers ne sont pas des versions : ils livrent un lot dans chaque
version.

| Chantier | Ce qu'il livre à chaque version | Propriétaire |
| ---- | ---- | ---- |
| **Optimisation M1** | Le budget de la version tenu, mesuré sur 600 s dans la build | fil Optimisation (config moteur, budget) |
| **Réalisme de la rue** | Un lot de détails face aux photos réelles (petits détails d'abord) | fil Réalisme de la rue (habillage) |
| **Génération de la ville** | Données géo, voies, extension de la carte | fil Ville v2 |
| **Assets** | Gratuits d'abord (Fab, Megascans, CC0), puis Tripo ; import, LOD, collision | fils Packs gratuits, Tripo, Import |
| **Audio** | Le son des systèmes de la version (pas, moteurs, sirènes, impacts), mixage | *à ouvrir* (oubli de la v2) |
| **Animations** | Les animations des systèmes de la version (entrer en voiture, réactions, combat) | *à ouvrir* (oubli de la v2) |

---

## 2. Où on en est (2026-11-10)

**Mise à jour du 2026-11-11.** La 0.3.0 est sortie le 2026-11-10 avec, en
avant-première, les systèmes des versions 0.4 à 0.6 (piétons, feux, ragdoll,
police, mini-carte, réglages sauvegardés, jour/nuit, sons). Le chantier
performance qui a suivi tient le palier C plafonné à 40 i/s sur le M1
(`next` = 3dcf33b). Le code des versions 0.4 à 0.6 est donc déjà là : elles
sortiront en train dès leurs manques comblés. Rien n'existe encore pour
0.7, 0.8 et 0.9. Voir le [plan vers la phase de test](test-phase-plan.fr.md).

*Tableau du 2026-11-10 :*

| Version | État |
| ---- | ---- |
| **0.1 Jouer en un clic** | ✅ livrée le 2026-11-09 (release v0.1.0, 702 Mo) |
| **0.2 La rue habillée** | ✅ livrée le 2026-11-10 (release v0.2.0, 1,36 Go) : 600 s en profil M1, p50 29,9 ms, p95 40,3 ms, 84,9 % des images ≤ 33 ms. Défauts connus : platanes sans feuilles, logos absents |
| **0.3 Au volant** | 🟡 conduite, trafic sur 112 voies, vol avec éjection et carte étendue fonctionnent ; **bloquée par le budget** : 6 PNJ coûtent environ 10 ms GPU, et la carte étendue ne tient que 65 % des images sous 33 ms |
| Chantiers ouverts aujourd'hui | Piétons, Police, Jour/nuit + météo, Optimisation, Réalisme de la rue, Ville v2, Import, Packs gratuits, Tripo |

La v2 prévoyait trois semaines par version ; les deux premières sont sorties
en deux jours grâce aux fils en parallèle. Le vrai frein est apparu : le
**GPU du M1 est déjà plein** avec une rue vide (p50 29,9 ms), et chaque
système vivant (trafic, piétons, nuit, effets) doit payer sa place.

---

## 3. Budget de performance par système

Jusqu'ici, seul le temps total était mesuré, si bien qu'un système pouvait
manger la marge de tous les autres. À partir de la 0.3, chaque système a une
enveloppe GPU dans le profil M1, mesurée à chaque version par le fil
Optimisation (`stat gpu`, `ProfileGPU`, capture au même point de la rue).

Enveloppe proposée pour la cible 30 i/s (33,3 ms), à affiner par la mesure :

| Poste | Enveloppe GPU (profil M1) | Contenu |
| ---- | ---- | ---- |
| Ville (bâtiments, sols, mobilier, végétation) | 14 ms | façades, props, platanes, culling et HLOD |
| Éclairage, ombres, illumination globale | 7 ms | Lumen logiciel, ombres, ciel |
| Trafic | 2 ms | 12 voitures visibles au plus près du joueur, au-delà en imposteurs ou masquées |
| Piétons | 2,5 ms | 40 piétons visibles, LOD agressifs, au-delà en Mass sans squelette |
| Joueur, police, personnages proches | 1,5 ms | |
| Nuit, météo, effets (pluie, phares, lampadaires, Niagara) | 2 ms | les lumières de nuit sont à budgéter à part |
| Post-traitement, TSR, interface | 2,3 ms | |
| Marge (saccades, pics) | 2 ms | |
| **Total** | **33,3 ms** | |

Règles :

- Un système qui dépasse son enveloppe ne va pas sur `next` ; il réduit sa
  densité ou son coût, ou le fil Optimisation libère de la place ailleurs.
- Le processeur (game thread, IA, physique) a aussi son enveloppe : 25 ms
  par image au plus, mesurée avec `stat unit`.
- Tant que la cible provisoire (p95 ≤ 42 ms) est en vigueur, les enveloppes
  sont multipliées par 1,25.

---

## 4. Vue d'ensemble des versions

| Version | Nom | On peut… | Ordre de grandeur |
| ---- | ---- | ---- | ---- |
| **0.1** | Jouer en un clic | lancer le jeu et marcher dans la rue de la République | ✅ 2026-11-09 |
| **0.2** | La rue habillée | se promener dans une rue fidèle, à 25 i/s en profil M1 | ✅ 2026-11-10 |
| **0.3** | Au volant | voler une voiture, rouler dans le trafic jusqu'au quai du Rhône et au pont Lafayette | novembre 2026 |
| **0.4** | La rue vivante | croiser des piétons qui réagissent, s'arrêter aux feux, renverser du mobilier | décembre 2026 |
| **0.5** | La police arrive | être recherché (1-2 étoiles), poursuivi en voiture, arrêté ou semer la police | décembre 2026 |
| **0.6** | Jour et nuit | rouler de jour, au crépuscule, la nuit et sous la pluie | janvier 2027 |
| **0.7** | Armé | se battre à mains nues et au pistolet, se couvrir, mourir et réapparaître ; police qui tire | février 2027 |
| **0.8** | Vivre en ville | entrer dans une boutique sans chargement, acheter, braquer, gagner de l'argent, sauvegarder en planque | mars 2027 |
| **0.9** | Tout le quartier : build de test | parcourir Bellecour → Terreaux sans chargement, se repérer sur la carte, faire 3 activités, sauvegarder ; puis **phase de test** | avril 2027 |
| **0.10** | La carte et le confort | ouvrir la carte de Lyon, suivre le GPS, régler touches, accessibilité, manette | mai 2027 |
| **0.11** | L'arsenal | choisir parmi 6 armes, affronter la police niveau 3 et ses barrages | juin 2027 |
| **0.12** | La ville à soi | faire des activités et des missions libres, rouler en scooter et en Vélo'v : **jalon P0 complet** | été 2027 |
| **0.13** | Façades et monuments | voir le quartier en qualité finale, monuments compris | automne 2027 |
| **0.14** | Les intérieurs | entrer dans 30 boutiques, 10 bâtiments, 3 planques finaux | automne 2027 |
| **0.15** | Les gens et les voitures | croiser une population et une police finales, 6 véhicules finaux | hiver 2027 |
| **0.16** | Le son de Lyon | entendre la ville finale : ambiances par rue, prises de son à Lyon | hiver 2027 |
| **0.17** | Finition | jouer une heure sans bloquant, budget tenu partout | début 2028 |
| **1.0** | Tranche verticale | jouer la tranche verticale complète, revue de parité | début 2028 |

Les ordres de grandeur supposent le rythme observé en novembre 2026 (fils en
parallèle, Mac disponible plusieurs heures par jour). Ils sont **à confiance
faible** et revus à chaque fin de version.

### Pourquoi cet ordre

*Décidé par Nico le 2026-11-10 (carte) : la rue vivante d'abord.*

1. **La rue vivante avant les armes.** Trafic, piétons, police et jour/nuit
   ont déjà un chantier ouvert ; les intégrer d'abord utilise ce travail tout
   de suite et mesure tôt les postes les plus chers pour le GPU (foule,
   trafic, lumières de nuit). C'est aussi ce qui distingue un GTA d'un jeu
   d'action en couloir.
2. **La police avant le combat.** Voler une voiture ou renverser un piéton
   suffit à déclencher 1-2 étoiles ; la police réutilise directement le
   trafic et les piétons. Elle tirera quand le combat existera (0.7).
3. **Jour et nuit avant le combat.** Le chantier est ouvert et touche au rendu,
   donc au budget : mieux vaut connaître le coût des lumières de nuit avant
   d'ajouter les effets d'armes.
4. **Le combat en 0.7** laisse le temps de régler la source des animations
   et les réactions physiques (0.4), sans lesquelles un combat ne ressemble
   pas à un GTA.
5. **Le quartier complet en 0.9**, après les systèmes, parce que le streaming
   se règle mieux quand on sait ce qu'il doit porter (foule, trafic, police).

---

## 5. Détail des versions

Chaque fiche : objectif, « fait quand », tâches, chantiers. Les chantiers
continus (§1) livrent leur lot dans chaque version sans être répétés.

### Étape A : la rue vivante

#### 0.1 Jouer en un clic : ✅ livrée

Application macOS en un clic, écran titre, menu pause, personnage GASP,
caméra GTA 3e/1re personne, rue de la République en greybox, preuves dans
[evidence/0.1](evidence/0.1/).

#### 0.2 La rue habillée : ✅ livrée

Rue de la République à partir du LiDAR HD et de Grand Lyon, kit de 30 modules
de façades, 202 enseignes, 1 203 objets de mobilier, sol corrigé, deux
profils de qualité, 600 s mesurées en profil M1 (p95 40,3 ms). Sensation à
pied (caméra, aide « F Monter », HUD, ambiance) ajoutée.

#### 0.3 Au volant : 🟡 en cours

**Objectif.** On vole une voiture et on roule dans le trafic de la rue de la
République jusqu'au quai du Rhône et au pont Lafayette.

**Fait quand :**
- Clio V, 208 II et Kangoo II conduisibles ; entrer, sortir, casser la vitre,
  éjecter un conducteur PNJ.
- Trafic de PNJ sur les voies issues de la BD TOPO ; son moteur ; panneau de
  conduite.
- Carte étendue au quai du Rhône et au pont Lafayette, avec collision.
- **Chute dans le Rhône gérée** : le joueur ou la voiture qui tombe à l'eau
  réapparaît sur le quai (la nage reste en 1.x). Surface d'eau : notre maillage avec le matériau `M_City_Water` (petites
  vagues, opaque) ; le plugin Water natif est écarté (paysage et passes en
  plus, hors budget M1).
- Vidéo d'un trajet de 3 minutes ; **p95 ≤ 42 ms** en profil M1 avec le
  trafic (seuil fixé pour sortir la 0.3 sans attendre le reste).
- Enveloppes GPU du §3 mesurées une première fois.
- Distribution : si l'archive dépasse 2 Gio (limite d'un fichier de release
  GitHub), découpage documenté dans le README.

**Chantiers :** 0.3 (voies, conduite PNJ, HUD), Ville v2 (quai, pont, eau),
Optimisation (budget), Véhicules.

#### 0.4 La rue vivante

**Objectif.** La rue vit : des piétons marchent et réagissent, les voitures
s'arrêtent aux feux, le mobilier casse quand on le percute.

**Fait quand :**
- Piétons Mass + StateTree sur les trottoirs, densité réglable ; 40 visibles
  près du joueur dans l'enveloppe de 2,5 ms.
- Réactions : regarder, s'écarter, fuir un accident ; un piéton renversé
  passe en **ragdoll** et se relève ou reste au sol.
- **Réactions physiques du joueur** : trébucher, tomber d'une voiture en
  marche, être percuté.
- Feux tricolores et passages piétons sur la rue de la République ; les PNJ
  conducteurs les respectent, klaxonnent, évitent.
- **Objets cassables** : potelets, panneaux, poubelles, barrières se
  renversent ou se cassent au choc (Chaos, version cassée des modèles
  existants).
- Franchir les obstacles bas (muret, borne) avec le GASP.
- Variété de la population : nombre de modèles et stratégie MetaHuman / LOD
  décidés et mesurés.
- Textes du jeu passés en tables de chaînes, en français et en anglais.

**Chantiers :** Piétons, 0.3 (feux, conduite PNJ), Animations et réactions
physiques (*à ouvrir*), Réalisme de la rue (version cassable des objets).

#### 0.5 La police arrive

**Objectif.** Un délit vu par un témoin déclenche une recherche de niveau 1
ou 2, qu'on peut semer.

**Fait quand :**
- Délits : vol de voiture, piéton renversé, accident grave, coup porté à un
  policier ; signalés par des témoins (pas de détection magique).
- Étoiles 1 et 2, zone de recherche, ligne de vue, fuite et cache.
- Patrouilles à pied et en voiture de police fictive, **sirènes et
  gyrophares**, poursuites, arrestation avec pénalité et reprise rapide.
- **Mini-carte** au HUD (rues, police, zone de recherche).
- **Réglages sauvegardés** (profil graphique, souris, volumes).
- Animations entrer / sortir de voiture remplaçant les durées fixes.
- Vidéo d'une poursuite de 3 minutes ; budget tenu avec police, trafic et
  piétons.

**Chantiers :** Police, Interface et réglages (*à ouvrir*), Audio (*à ouvrir*),
Animations.

#### 0.6 Jour et nuit

**Objectif.** Le temps passe : soleil à la latitude de Lyon, crépuscule, nuit
éclairée, pluie et sol mouillé.

**Fait quand :**
- Cycle jour/nuit réglable (vitesse du temps), course réelle du soleil à Lyon,
  exposition corrigée de jour comme de nuit (ADR-0001).
- **Éclairage de nuit** : lampadaires, vitrines, fenêtres allumées, **phares,
  feux stop et clignotants** des voitures.
- Pluie, flaques, sol mouillé, brouillard ; trafic et piétons qui s'y
  adaptent (parapluies, vitesse).
- Captures Ultra midi, heure dorée, nuit, pluie comparées aux photos ;
  enveloppe « nuit, météo, effets » tenue.

**Chantiers :** Jour/nuit + météo, Véhicules (feux), Réalisme de la rue
(vitrines et fenêtres), Optimisation.

#### 0.7 Armé

**Objectif.** On se bat : mains nues et pistolet, couverture, santé, mort et
réapparition ; la police tire au niveau 2.

**Fait quand :**
- ADR-0011 (framework gameplay : Gameplay Ability System, StateTree ; le numéro 0008 est pris par le jour/nuit).
- Corps à corps (coups, esquive, neutralisation) ; pistolet (viser, tirer,
  recharger, recul), **aide à la visée** à la manette, changement d'épaule.
- Couverture contre murs et voitures, passage d'un abri à l'autre, tir à
  l'aveugle.
- Santé, armure, **endurance**, mort, réapparition à l'hôpital (fictif).
- Réactions localisées aux impacts, ragdoll à la mort.
- **Effets visuels** : impacts par surface, étincelles, verre, sang retenu,
  fumée ; feu et explosion des voitures.
- Piétons qui fuient les armes ; police qui tire au niveau 2.
- Vidéo d'un combat de 2 minutes ; budget tenu.

**Chantiers :** Combat et armes (*à ouvrir*), Animations, Audio, Police.

### Étape B : tout le quartier en systèmes

| Version | Fait quand | Chantiers |
| ---- | ---- | ---- |
| **0.8 Vivre en ville** | ADR-0012 (intérieurs sans chargement ; les cellules de la ville sont dans l'ADR-0010) ; versionnage des sauvegardes ; une boutique sans écran de chargement (Level Instance ou data layer), vendeur PNJ qui sert, se rend ou appelle la police, achat, braquage ; argent persistant ; 1 planque, **sauvegarde automatique et en planque** ; portes et contenants | Boutiques et intérieurs, Économie et sauvegarde |
| **0.9 Tout le quartier** | Bellecour → Terreaux, quais et ponts en une commande (Ville v2) ; World Partition, HLOD ; trajet complet filmé à pied et en voiture, budget tenu, mémoire ≤ 9 Go ; bus TCL en décor dans le trafic. **Avancés pour le test** : carte avec vrais noms de rues et GPS simple, cadre de missions minimal et 3 activités, remappage et glyphes manette, crédits, build de test avec journaux, builds macOS et Windows. Critères complets : [plan vers la phase de test](test-phase-plan.fr.md) | Ville v2, Streaming du monde, Optimisation, Interface, Missions, QA et sortie |
| **0.10 La carte et le confort** | Ce qui reste après la 0.9 : GPS qui suit les sens de circulation ; remappage complet des touches, sensibilité, accessibilité (sous-titres, aide à la visée, couleurs) ; glyphes et vibrations manette ; caméra cinématique en voiture ; premières paroles des PNJ en français | Interface et réglages, Ville v2, Audio |
| **0.11 L'arsenal** | 6 armes fictives, roue des armes, inventaire limité ; niveau 3, barrages, poursuites armées ; 5 types de boutiques (alimentation, vêtements, armurerie, garage, café), 3 planques | Combat et armes, Police, Boutiques |
| **0.12 La ville à soi** | Cadre de missions complet (le minimum arrive en 0.9) ; 6 activités libres du concept construites avec ; scooter, moto, Vélo'v ; **jalon « Systèmes P0 complets »** : chaque ligne P0 de l'[inventaire](systems-inventory.fr.md) cochée en qualité de travail | Missions et activités (*à ouvrir à la 0.8*), Véhicules |

### Étape C : qualité finale

L'art final est produit **en avance** pendant A et B (vagues Tripo, kits,
photos, packs gratuits) et intégré ici.

| Version | Fait quand | Chantiers |
| ---- | ---- | ---- |
| **0.13 Façades et monuments** | Kit de façades v2 sur tout le quartier ; Hôtel de Ville, Opéra, Bellecour et sa statue, ponts ; captures Ultra jour, nuit, pluie ; budget M1 tenu sur tout le quartier | Ville v2, Réalisme de la rue, Monuments |
| **0.14 Les intérieurs** | 30 boutiques, 10 bâtiments (halls, cages d'escalier, cours, toits, parking), 3 planques en qualité finale | Boutiques et intérieurs, Assets |
| **0.15 Les gens et les voitures** | Population et police finales (visages fictifs), 6 véhicules finaux du parc français, voiture de police fictive, bus TCL | Piétons, Véhicules, Assets |
| **0.16 Le son de Lyon** | Ambiances par rue, réverbération des façades, prises de son à Lyon, mixage final | Audio |

### Étape D : tranche verticale

| Version | Fait quand | Chantiers |
| ---- | ---- | ---- |
| **0.17 Finition** | Saccades, mémoire, bugs ; une heure de jeu libre sans bloquant en profil M1 ; budget tenu partout ; étude du mode 60 i/s | Optimisation, Qualité, playtests de Nico |
| **1.0 Tranche verticale** | Revue de parité ligne par ligne avec l'[inventaire](systems-inventory.fr.md) ; vidéo de présentation ; décision de Nico sur la suite | Revue de parité, Présentation |

### Après la 1.0

Un quartier à la fois (sud de la Presqu'île, Vieux-Lyon, Croix-Rousse, rive
gauche, Fourvière), fonctions P1 (nage, téléphone, radio, garages,
personnalisation, hélicoptère, saluer et provoquer les PNJ, événements
d'ambiance) et l'histoire écrite avec Nico.

---

## 6. Assets

- **Gratuit d'abord** : Fab et Megascans (gratuits à vie), Megaplants, Poly
  Haven et textures CC0, MetaHuman ; vérifier ces sources avant toute
  génération. Pas de packs de mods de GTA V (droits de Rockstar et des
  moddeurs, dépôt public exposé au retrait).
- **Tripo Studio Max** ensuite pour les objets isolés (mobilier, accessoires,
  intérieurs, carrosseries), « sans logo » ; les vraies enseignes viennent des
  photos et restent hors Git (`Plugins/CityBrands/`). Aucune dépense sans
  carte à Nico.
- **Flux** : catalogue → génération → nettoyage (échelle, pivot, LOD,
  collision) → import headless → ligne dans `assets/SOURCES.md` → statut dans
  le [catalogue](asset-catalog.fr.md).
- **Masters** gardés en haute qualité hors Git ; variante légère pour le
  profil M1.
- **Nouveau à partir de la 0.4** : chaque objet de rue susceptible d'être
  percuté a une version cassable ou mobile.

---

## 7. Systèmes, versions et dépendances

| # | Système | Versions | Dépend de | Point de départ |
| ---- | ---- | ---- | ---- | ---- |
| 1 | Code de base, packaging, mesure | 0.1 | rien | `tools/build/package-mac.sh` |
| 2 | À pied et caméra | 0.1, 0.2, 0.7 | 1 | Game Animation Sample |
| 3 | Interactions contextuelles | 0.2, 0.8 | 2 | `UGTPInteractionComponent` |
| 4 | Véhicules | 0.3, 0.6, 0.12, 0.15 | 2, 3 | Chaos Vehicles, `AGTPVehicle` |
| 5 | Trafic | 0.3, 0.4, 0.9 | 4, voies géo | `AGTPTrafficNetwork` |
| 6 | Réactions physiques, ragdoll | 0.4, 0.7 | 2 | Physics Asset, Physical Animation |
| 7 | Animations hors locomotion | 0.4 → 0.11 | 2 | à recenser (Epic gratuit, Fab) |
| 8 | Piétons et IA des PNJ | 0.4, 0.15 | 6 | Mass, StateTree, Smart Objects |
| 9 | Objets cassables, effets visuels | 0.4, 0.7 | 6 | Chaos, Niagara |
| 10 | Police et recherche | 0.5, 0.7, 0.11 | 5, 8 | maison sur StateTree |
| 11 | Interface, réglages, textes FR/EN | 0.4, 0.5, 0.10 | 1 | UMG, Common UI, tables de chaînes |
| 12 | Heure et météo, éclairage de nuit | 0.6 | rendu | Sky Atmosphere, SunPosition |
| 13 | Combat, santé, mort | 0.7, 0.11 | 2, 6, 7 | Gameplay Ability System |
| 14 | Boutiques, intérieurs, économie, sauvegarde | 0.8, 0.11, 0.14 | 3, 8, 15 | Level Instances, SaveGame |
| 15 | Streaming du monde | 0.8, 0.9 | génération de la ville | World Partition, HLOD |
| 16 | Carte, GPS | 0.5 (mini-carte), 0.10 | données géo | tuiles maison |
| 17 | Missions et activités | 0.12 | 10, 13, 14 | maison |
| 18 | Audio | continu, 0.16 | tous | MetaSounds |

---

## 8. Risques

| Risque | Probabilité | Impact | Parade |
| ---- | ---- | ---- | ---- |
| **GPU du M1 Pro déjà plein** (p50 29,9 ms rue vide ; +10 ms pour 6 PNJ ; 65 % d'images < 33 ms sur la carte étendue) | Certaine | Très haut | Enveloppe par système (§3) ; fil Optimisation propriétaire du budget ; culling, imposteurs, LOD agressifs, Mass sans squelette au loin ; aucune version sur `next` hors budget |
| **Un seul Mac pour tout** (éditeur, cuisson, packaging, mesure, et parfois Nico qui joue) | Certaine | Haut | File d'attente du Mac tenue par le coordinateur ; tout préparé dans le cloud avant le créneau ; scripts headless ; `caffeinate` pour les tâches longues ; un seul Unreal à la fois |
| **Mémoire 16 Go** (éditeur + cuisson, carte complète en 0.9) | Haute | Haut | Petites cellules de streaming, HLOD dès 0.8, mesure mémoire à chaque version |
| **Animations introuvables** pour le combat, les véhicules, les PNJ | Haute | Haut | Chantier Animations ouvert dès la 0.4 ; recensement des sources gratuites avant tout développement du combat |
| **Divergence des clones sur le Mac** (cartes et matériaux copiés d'un clone à l'autre) | Haute | Moyen | Vérification avant chaque packaging ; une seule source par asset partagé |
| **Taille du jeu** (zip 1,36 Go en 0.2, limite GitHub de 2 Gio par fichier) | Haute | Moyen | Découpage d'archive documenté ; textures compressées par profil |
| **Frameworks expérimentaux** (Chaos Vehicles 5.8, Mass, Mover) | Haute | Moyen | Derrière nos interfaces ; repli en acteurs |
| **Intégration de chantiers parallèles** (trafic, piétons, police touchent les mêmes PNJ) | Haute | Moyen | Propriétaires fixés par le coordinateur ; interfaces communes (perception, délits) définies avant le code |
| **Qualité des modèles générés** | Haute | Moyen | Gratuit d'abord, nettoyage, revue visuelle |
| **Licences et marques** | Moyenne | Haut | `assets/SOURCES.md` ; vraies marques hors du dépôt public, remplaçables d'un bloc ; pas de personnes réelles |
| **Périmètre** face à un studio de milliers de personnes | Certaine | Haut | P0 seulement jusqu'à 1.0 ; l'[inventaire](systems-inventory.fr.md) sert de garde-fou |
| **Pas de build Unreal en CI** | Certaine | Faible | Packaging et mesures scriptés sur le Mac, preuves versionnées |

---

## 9. Pilotage

- [Tableau de bord](roadmap-status.fr.md) : version en cours et état des
  chantiers, mis à jour à chaque fin de tâche.
- [Inventaire des systèmes](systems-inventory.fr.md) : revu à chaque fin de
  version ; tout système sans version ni chantier y apparaît comme oubli.
- Fin de version : critère et budget vérifiés, build rangée dans
  `GTAL/Builds/`, preuves versionnées, résumé d'une ligne à Nico.
- Décisions structurantes (ordre des versions, achats, budget de
  performance, baisse visible de l'image) : cartes à Nico avec
  recommandation.
