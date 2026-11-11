# Intérieur de référence n°1 : le fast-food de la place Bellecour

Objectif fixé par Nico le 2026-11-11 : entrer dans le fast-food à l'angle de la place
Bellecour et de la rue de la République et y trouver le jeu au niveau attendu :
photoréalisme avec un nombre massif d'objets, plusieurs employés au travail que l'on peut
braquer, friteuses, burgers en préparation, clients, fond de caisse réaliste. Cet intérieur
sert d'étalon à tous les bâtiments suivants. Architecture : [ADR-0012](../adr/adr-0012-interiors.fr.md).

Règle des marques : le restaurant est générique dans Git et dans les packages (vrai nom en
lettrage au plus, règle du fil QA). Logos, couleurs et noms de menu de la vraie chaîne vivent
dans une couche d'enseigne locale ignorée par Git (`-Branding=local|generic`). Aucune personne
réelle : équipe et clients sont des personnages génériques.

## 1. Le bâtiment, d'après les données publiques

| Source | Ce qu'elle donne |
|---|---|
| Nœud OSM 35694312 (45,75820 N, 4,83423 E) | restauration rapide, burgers, salle, terrasse sur la zone piétonne, espèces et cartes acceptées, ouvert de 9 h à 1 h (2 h vendredi et samedi), accès fauteuil |
| Bâtiment BD TOPO `T24GFDBJTRSJ` | emprise 329 m², 7 niveaux, 28,3 m de haut, usage « commercial et services », sol à 166,9 m |
| Cache géo (ville v2) | l'enseigne déjà générée pour ce nœud (`T_Sign_node_35694312`) |

L'emprise est un polygone irrégulier d'environ 22 × 21 m, grande vitrine sur la place
Bellecour (sud) et un côté sur la rue de la République (est). Le découpage des salles
ci-dessous est une hypothèse de travail tirée de la rue, écrite dans
`tools/interiors/fastfood_bellecour.json` et facile à corriger : une modification, une
régénération.

## 2. Plan (hypothèse)

Rectangle de travail 19 × 16 m aligné sur la façade Bellecour, étages de 3,6 m.

| Zone | Niveau | Taille | Contenu |
|---|---|---|---|
| Salle du rez-de-chaussée | 0 | 12 × 7,5 m | 10 tables de 2, 4 de 4, 3 mange-debout, côté vitrine |
| Bornes | 0 | 7 × 3,5 m | 6 bornes de commande près de la porte République |
| File | 0 | 7 × 4 m | couloirs de file, écrans de retrait |
| Comptoir | 0 | 15 × 1,5 m | 4 caisses, 2 colonnes à boissons, coin café, 2 points de retrait |
| Cuisine | 0 | 15 × 4,5 m | 4 friteuses, bac à frites, 2 grills, 2 toasteurs, 2 lignes d'assemblage, 2 armoires de maintien, 4 chariots à pains, machine à milkshakes, plonge |
| Réserve, chambre froide | 0 | 15 × 2,5 m | 6 étagères sèches, 4 étagères froides |
| Escalier | 0-1 | 4 × 8,5 m | 2 volées |
| Salle de l'étage | 1 | 19 × 9 m | 12 tables de 2, 8 de 4, 5 banquettes |
| Vestiaire, bureau | 1 | 9 × 7 m | casiers, bureau, coffre, écrans |
| Toilettes | 1 | 10 × 7 m | 4 WC, 4 lavabos |

## 3. Inventaire des objets par classe

Les classes de maillage et leurs règles de coût sont dans `tools/interiors/catalog.json`
(94 classes) ; le générateur les place et les compte. Aux densités par défaut :

| Famille | Exemples | Objets perçus (M1 / PC) |
|---|---|---|
| Équipement de cuisine | friteuses, paniers, grills, toasteurs, armoires de maintien, colonnes à boissons, café, milkshakes | ~90 |
| Nourriture | pains (chariots de 96), steaks, viennoiseries, frites, sacs de frites, cartons surgelés | ~1 100 |
| Emballages (lots) | gobelets, couvercles, papiers, boîtes, sacs, serviettes, sachets de sauce, pailles | ~6 000 montrés par ~200 maillages de lot |
| Mobilier | tables, chaises, tabourets, banquettes, bornes, casiers, étagères | ~260 |
| Restes sur les tables | plateaux, papiers, gobelets, frites, tickets (60 % des tables occupées) | ~400 à 700 |
| Équipement du bâtiment | dalles de plafond, luminaires, sprinklers, bouches, prises, siphons, extincteurs | ~1 300 |
| **Total** | | **9 575 / 9 880 objets pour 3 593 / 3 898 instances et ~155 appels de dessin** |

Niveaux de densité (`-InteriorDensity=`) : 0 équipement et mobilier seuls (2 170 objets),
1 à moitié approvisionné (5 893), 2 plein avec restes sur les tables (défaut M1), 3 tout, y
compris frites éparses (défaut PC). Monter encore est un changement de données : catalogue et
budgets disent aussitôt si ça tient.

## 4. Employés, postes et boucles de travail

L'équipe est un type à part (`AGTPStaffCharacter`, persistant dans le niveau intérieur), avec
le squelette et les apparences des piétons. Équipe du soir par défaut : 8 (M1) à 10 (PC).

| Poste | Équipe | Boucle (chaque étape déplace un vrai objet) |
|---|---|---|
| Frites | 1 | sac de frites surgelées → panier → descente dans l'huile (minuterie, bulles) → remontée, égouttage → bac → sel → pelle dans les cornets → cornet vers la goulotte |
| Grill | 1 | steaks du tiroir froid → grill à double plaque → cuisson → bac de l'armoire de maintien |
| Assemblage | 2 | toaster le pain → lire l'écran cuisine → pain, pistolet à sauce, garnitures, steak, fromage → papier ou boîte → glisser vers le comptoir |
| Comptoir et boissons | 2 | prendre la commande en caisse → espèces ou carte → gobelet → colonne → couvercle → plateau ou sac |
| Retrait | 1 | appeler le numéro, remettre le sac ou le plateau |
| Gérant | 1 | passe sur la ligne, prélève les caisses vers le coffre, réapprovisionne la monnaie, bureau |

Une boucle est une liste de places de poste et d'actions sur des objets (prendre, poser,
transformer). Les objets d'une boucle sont des instances que l'employé promeut en acteur
pendant qu'il les manipule (ADR-0012 §4) ; un burger est une pile d'instances qui change
d'état (pain → pain + steak → emballé). Les commandes viennent des clients : la cuisine
travaille au rythme de la salle.

## 5. Parcours des clients

Les clients sont des piétons du pool (fil piétons) qui entrent depuis la rue : porte → borne
ou file → caisse → attente (l'écran affiche leur numéro) → retrait → table ou sortie → repas
(durée selon le menu) → retour du plateau → sortie. Fréquentation cible par heure (pic de midi,
après-midi calme, fin de soirée). M1 jusqu'à 12 clients dedans, PC 25.

## 6. Braquage de plusieurs employés

Règles dans `GTPHoldUpModel.h` (testées en CI) :

- Peur par personne. La réaction générique (Calm → Alarmed → HandsUp → Complying / Backing /
  Fleeing / Down), la peur et sa contagion appartiennent au fil ragdoll
  (`UGTPThreatResponseComponent`) ; le script de braquage écoute ses états.
- Le joueur tient en joue un employé à la fois. Le caissier tenu en joue, sommé de vider la
  caisse, l'ouvre (2,5 s) et tend les billets (4 s) ; les pièces seulement si le joueur prend
  le temps.
- Les employés que personne ne tient agissent au bout de quelques secondes s'ils sont
  courageux : le gérant appuie sur l'alarme silencieuse sous le comptoir (délit signalé 20 s
  plus tard par le centre de télésurveillance), un équipier file par la porte de derrière
  (délit signalé dès qu'il atteint la rue). Crier fait monter la peur de tous et remet à zéro leur temps hors
  de visée : c'est ce qui garde la salle sous contrôle.
- Les clients réagissent par `OnPedestrianScared` des piétons ; une personne touchée passe aux
  réactions d'impact du fil ragdoll.
- Délit signalé par `UGTPWantedComponent::ReportCrime` ; le fil police fait le reste.
- Le coffre (bureau, à l'étage) est une étape ultérieure : il faut le gérant et une
  temporisation.

## 7. Le fond de caisse

Règles dans `GTPCashDrawerModel.h` (testées en CI). Chaque caisse ouvre avec un fond de
150,00 € : 1 × 20, 4 × 10, 6 × 5, 10 × 2, 20 × 1, 20 × 0,50, 25 × 0,20, 30 × 0,10, 20 × 0,05,
25 × 0,02, 50 × 0,01. Le service est simulé depuis l'ouverture : 45 clients par heure et par
caisse, 18 % paient en espèces, la monnaie sort du tiroir pièce par pièce, le gérant
réapprovisionne la monnaie depuis le coffre et prélève les billets au-delà de 400 € pour
redescendre à 100. Une caisse braquée contient donc entre 100 et environ 550 € selon l'heure ;
exemple après 3 heures : 275,00 € en billets et 26,90 € en pièces. Les billets obtenus par le
joueur sont de vrais billets (nombre par valeur), montrés comme tels dans le sac et au HUD.

## 8. Dedans et dehors

L'intérieur est son propre niveau de streaming, chargé à 40 m de la porte et déchargé à 70 m
(ADR-0012). De plus loin, la vitrine montre la carte d'interior mapping. Les portes sont des
passages ouverts, sans chargement. Lumière : la salle est éclairée par ses propres luminaires
(6 lumières avec ombre sur M1), la rue reste visible à travers la vitre.

## 9. Budgets

Voir ADR-0012 §6. Le rapport du générateur (`interior_layout.py --report`) donne la partie
comptable à chaque push ; GPU, mémoire et à-coups sont mesurés par le fil optimisation à
chaque étape.

## 10. Ordre de construction, en étapes fines et testables

Chaque étape est derrière un interrupteur, coupée par défaut tant qu'elle n'est pas mesurée,
et passe la porte de fusion du plan 0.9 (CI, compilation PC, M1 plafonné à 40 : ≥ 99 % à
25,0 ms ±1, aucune image au-delà de 100 ms, GPU p95 ≤ 24 ms).

| Étape | Quoi | Interrupteur | Où | Preuve |
|---|---|---|---|---|
| 1 | Règles hors machine : fond de caisse et service, braquage multi-employés, générateur d'aménagement et budget d'objets en CI | aucun (outils et en-têtes) | cloud | CI verte (cette branche) |
| 2 | Coque et streaming : `L_Interior_fastfood_bellecour` généré depuis le CSV en densité 0 avec des boîtes grises à la taille de chaque classe, vitrine, porte, lumières ; streaming selon la distance | `-Interiors=0/1`, `-InteriorPreload=` | compilation PC, banc Mac | entrée en courant : 0 image au-delà de 100 ms ; GPU de l'intérieur en densité 0 |
| 3 | Densité : lots et instances jusqu'à la densité 3, assets gratuits substitués classe par classe (`catalog` → table d'assets) | `-InteriorDensity=0..3` | PC (assets, compilation), banc Mac | tableau de budget de l'ADR-0012 mesuré ; **ADR-0012 accepté** |
| 4 | Boucles de travail de l'équipe (postes, promotion d'objets) | `-InteriorStaff=N` | cloud (règles de boucle en C++ testées), PC, Mac | vidéo d'un service complet ; coût CPU de N employés |
| 5 | Cuisine visible : huile, vapeur, états des burgers, remplissage des cornets | `-InteriorCooking=0/1` | PC, Mac | images face aux photos de référence ; budget des effets |
| 6 | Clients venus de la rue | `-InteriorCustomers=N` | avec le fil piétons | pic de midi au plafond M1 |
| 7 | Braquage branché sur les états de menace et les caisses, alarme, ReportCrime | `-HoldUp=0/1` | avec ragdoll, police, puis combat | vidéo d'un braquage de 3 employés et de la fuite |
| 8 | Physique : objets renversés, déversements, promotions sous plafond | `-InteriorPhysics=0/1` | avec ragdoll | aucune image au-delà de 100 ms pendant une bagarre |
| 9 | Couche d'enseigne locale, panneaux de menu, lettrage | `-Branding=local/generic` | Mac et PC, en local seulement | build sans la couche toujours propre |
| 10 | Passe photoréaliste : matériaux scannés, usure, graisse, lumière | aucun | réalisme, PC | comparaison avec des références prises de la rue ; porte tenue |

## 11. Qui fait quoi

| Fil | Part |
|---|---|
| Intérieur de référence (celui-ci) | spec, générateur, niveau intérieur, équipe et boucles, script de caisse et de braquage, interrupteurs |
| Réalisme | maillages et matériaux intérieurs (reprend `gtp_surfaces.py`, `surfaces.json`, `lean_small()`, fusion en atlas) |
| Ville v2 | coque du bâtiment, ouvertures de porte, carte de vitrine, lien avec la cellule |
| Piétons | clients (pool), `OnPedestrianScared` |
| Ragdoll | états de menace, peur, réactions d'impact |
| Police | `ReportCrime` et réponse |
| Optimisation | budgets, mesures, porte de fusion |
| PC Windows | téléchargement d'assets gratuits (Fab, Megascans), compilation de contrôle, mesure PC |
| Boutiques, économie, sauvegarde (à ouvrir) | argent conservé, achats, SaveGame (ADR-0013), autres boutiques sur cette chaîne |
| Combat (à ouvrir) | la menace armée qui remplace `-ThreatDemo` |
| Missions (à ouvrir) | l'activité « braquer la boutique » utilise ce restaurant |
