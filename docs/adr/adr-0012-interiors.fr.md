# ADR-0012 : intérieurs sans chargement, le fast-food comme référence

- Statut : proposé (2026-11-11) ; accepté une fois l'étape 3 de l'ordre de construction mesurée sur M1 et PC
- Propriétaires : fil intérieur de référence ; optimisation (budgets, mesures) ; ville v2 (cellules, coques) ; réalisme (maillages, matériaux)
- Liens : ADR-0002 (budget, M1 plafonné à 40 i/s), ADR-0005 (piétons), ADR-0006 (ragdoll), ADR-0010 (cellules), docs/gameplay/reference-interior.fr.md
- Note : le plan 0.9 réservait le numéro 0012 pour « intérieurs et SaveGame ». Cet ADR prend la partie intérieurs ; la SaveGame versionnée devient l'**ADR-0013**, pour le fil Boutiques, économie et sauvegarde.

## Contexte

Objectif de Nico (2026-11-11) : du photoréalisme à un niveau de détail massif, avec énormément
d'objets. La première preuve est un bâtiment où l'on entre : le fast-food de la place
Bellecour, à l'angle de la rue de la République, avec plusieurs employés au travail que l'on
peut braquer, des friteuses, des burgers en préparation, des clients et un fond de caisse
réaliste.

Contraintes :

1. **M1 Pro 16 Go sans Nanite**, plafonné à 40 i/s (25,0 ms), aucune image au-delà de 100 ms ;
   la rue est déjà au bord du budget. **PC** (RTX 3070 Ti, DX12/SM6) avec Nanite.
2. **Aucun écran de chargement** entre rue et intérieur : les vitrines sur la place Bellecour
   et la rue de la République montrent la salle depuis dehors et la rue depuis dedans.
3. **Tout est régénérable depuis des données**, sans écran, comme la ville (ADR-0010).
4. **L'enseigne est locale** : vrais noms en lettrage au plus, aucun logo ni habillage de
   marque dans Git ni dans les packages (règle provisoire du fil QA). Aucune personne réelle.

## Options

**A. Intérieur posé dans le niveau de la cellule.** Simple, mais la cellule porte des milliers
d'objets même quand le joueur est à 200 m, et régénérer l'intérieur réécrit la cellule.

**B. Un niveau de streaming par intérieur, chargé selon la distance à sa porte** (le mécanisme
de l'ADR-0010, un cran plus bas). La cellule garde la coque et une vue de vitrine
(`M_City_ShopInterior`, interior mapping déjà utilisé par ville v2) ; le niveau
`L_Interior_<id>` est généré depuis le CSV d'aménagement et chargé en asynchrone quand le
joueur entre dans le rayon de préchargement, déchargé avec hystérésis.

**C. Data layers de World Partition.** Écarté pour les raisons de l'ADR-0010.

## Décision (proposée)

**B**, avec ces règles :

1. **Streaming.** Préchargement à 40 m de la porte (`-InteriorPreload=`), déchargement à 70 m.
   Instances sérialisées dans le niveau (jamais construites d'un bloc sur le thread de jeu),
   PSO préchauffés : entrer en courant ne donne **aucune image au-delà de 100 ms**. Hors du
   rayon, la salle est la carte d'interior mapping de la vitrine : zéro objet.
2. **Instances par classe et par étage.** Un composant de maillage instancié par classe et par
   niveau (colonne `ism_group` du CSV) : des milliers d'objets pour quelques centaines
   d'appels de dessin.
3. **Lots (kits).** Les piles et cartons (gobelets, couvercles, papiers, sacs, cornets de
   frites, serviettes) sont un seul maillage qui montre N objets. Un lot ne devient des objets
   séparés que lorsque le joueur le touche.
4. **Promotion.** Les objets sont des instances statiques jusqu'à ce que quelque chose agisse
   dessus (boucle de travail d'un employé, impact, main du joueur) : l'instance est alors
   masquée et remplacée par un acteur, qui redevient instance au repos. Même principe que les
   fantômes du trafic lointain. Plafonds : 64 corps physiques simultanés sur M1, 256 sur PC.
5. **Coût selon la taille** (tools/interiors/catalog.json) : les micro-objets (moins de 5 cm)
   disparaissent à 4 m et ne projettent jamais d'ombre dynamique ; les petits à 12 m, sans
   ombre sur M1 ; chaque maillage a ses LOD, qui servent de repli sur M1 puisque Nanite y est
   inactif. Pas de distance field pour les micro et petits objets.
6. **Budgets dans l'intérieur** (proposés au fil optimisation, qui les possède) :

   | Joueur à l'intérieur | M1 (sans Nanite, 40 i/s) | PC (Nanite, SM6) |
   |---|---|---|
   | GPU de l'intérieur | ≤ 5 ms sur 25 | ≤ 4 ms en Ultra |
   | Instances dessinées | ≤ 12 000 | ≤ 40 000 |
   | Maillages uniques | ≤ 260 | ≤ 600 |
   | Appels de dessin (principal / ombres) | ≤ 1 400 / 450 | ≤ 3 500 / 1 500 |
   | Triangles après LOD | ≤ 3 M | Nanite (source ≤ 25 M) |
   | Personnages (équipe + clients) | ≤ 20, cadence d'animation selon la distance | ≤ 35 |
   | Lumières avec ombre | ≤ 6 | ≤ 16 |
   | Mémoire du niveau intérieur | ≤ 600 Mo, textures 2K max | ≤ 1,5 Go |

   Le générateur d'aménagement vérifie en CI ce qui se compte. Le premier aménagement donne,
   aux densités par défaut : M1 9 505 objets perçus, 3 523 instances, 92 maillages, 184 appels
   de dessin ; PC 9 880 objets pour 196 appels (bornes basses : un matériau par section ; un composant par classe, étage et niveau de densité).
7. **La ville vue de l'intérieur.** Lumen et la rue derrière la vitre restent ; les cellules
   derrière le bâtiment peuvent passer à leur proxy lointain pendant que le joueur est dedans
   (le fil optimisation tranche sur mesure).
8. **Interrupteurs.** Chaque étape se coupe en ligne de commande (`-Interiors=0`,
   `-InteriorDensity=0..3`, `-InteriorStaff=`, `-InteriorCustomers=`, `-InteriorCooking=`,
   `-HoldUp=`, `-InteriorPhysics=`, `-Branding=`) pour l'A/B et le repli.

## Conséquences

- Tous les intérieurs suivants (boutiques de la 0.8, planque) suivent la même chaîne : une
  spec JSON, le catalogue commun, le générateur, un niveau de streaming.
- Les règles de jeu (caisse, braquage, boucles de travail) sont des en-têtes C++ simples
  testés avec g++ en CI, que le jeu enveloppe dans des composants : même schéma que le niveau
  de recherche et le trafic lointain.
- Enseigne, logos et tout habillage de marque réel restent dans une couche locale ignorée par Git.

## Acceptation

Accepté quand l'étape 3 de docs/gameplay/reference-interior.fr.md est mesurée par le fil
optimisation : M1 en package, plafonné à 40, entrée et sortie en courant, ≥ 99 % des images à
25,0 ms ±1, 0 image au-delà de 100 ms, GPU de l'intérieur ≤ 5 ms en densité 2 ; PC compilé et
mesuré en densité 3.
