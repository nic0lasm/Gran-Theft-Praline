# Stratégie de rendu : le photoréalisme dans Gran Theft Praline

*Version anglaise : [rendering-strategy.md](rendering-strategy.md)*
*Décisions : [ADR-0001](../adr/adr-0001-rendering-pipeline.fr.md), [ADR-0002](../adr/adr-0002-performance-budget.fr.md)*
*Dernière vérification : 2026-11-09, Unreal Engine 5.8.3*

## 1. La recommandation en bref

1. Utiliser la pile standard d'Unreal Engine 5.8 : **Lumen (ray tracing
   logiciel sur Mac), Nanite, Virtual Shadow Maps, Substrate, TSR**, soleil et
   ciel physiques. Pas de moteur de rendu maison.
2. Tirer le photoréalisme de la **qualité du contenu** : matériaux
   « dé-éclairés » et calibrés à partir de nos propres photos de Lyon, kit
   modulaire de façades en Nanite, habillage dense de la rue, usure et
   humidité. C'est là que se joue l'essentiel de l'écart entre « démo UE5 »
   et « vraie rue ».
3. **Ne pas mettre en jeu de scans bruts de la ville** (photomaillage, Google
   3D Tiles, Gaussian splats) : ils servent de référence et de volumes de
   blocage.
4. Les outils génératifs travaillent **en amont** (concept, enseignes,
   décalques, ébauches), jamais comme géométrie finale de premier plan.
5. Le prouver sur une **scène témoin** d'une rue du quartier de la tranche
   verticale avant toute production à grande échelle, avec chiffres et
   images capturés.

## 2. Ce qui rend vraiment une image réaliste

Par ordre d'impact pour une ville vue de la rue :

1. **Cohérence de la lumière** : intensités physiques du soleil et du ciel,
   exposition juste, lumière rebondie (Lumen), ombres de contact douces
   (VSM). Une mauvaise exposition ruine de bons assets.
2. **Calibration des matériaux** : albédo sans lumière incrustée, rugosité
   variée et crédible, humidité. L'essentiel du « look 3D » vient d'une
   rugosité uniforme et d'un albédo trop propre.
3. **Densité et imperfection géométriques** : arêtes chanfreinées,
   corniches, profondeur des fenêtres, pavés en déplacement, bordures pas
   parfaitement droites (Nanite rend cela abordable).
4. **Habillage et désordre qui raconte** : enseignes, câbles, poubelles,
   trottinettes, taches, affiches, terrasses de café, feuilles.
5. **Caméra** : stabilité du TSR, post-traitement sobre, optique naturelle.
6. **Vie** : passants, circulation, vent, particules, reflets sur l'asphalte
   mouillé.

## 3. Fonctions de rendu : Mac contre PC

| Fonction | Rôle | Apple Silicon (UE 5.8, Epic) | Windows + GPU type RTX | Notre usage |
| ---- | ---- | ---- | ---- | ---- |
| Lumen, RT logiciel | GI et réflexions dynamiques | Pris en charge dès M1 | Pris en charge | **Socle** |
| Lumen, RT matériel | Réflexions plus nettes, objets fins | Expérimental, M2+ | Production | Palier qualité PC ; sur Mac seulement si stable au test |
| Lumen Lite (bêta) | GI environ 2× moins chère | À vérifier | Pris en charge | Mode performance, réglages bas |
| Nanite | Détail statique illimité | Bêta, M2+ (pas M1) | Production | **Tous les maillages statiques opaques** |
| Virtual Shadow Maps | Ombres nettes et stables | Pris en charge avec Nanite | Production | **Soleil et lumières principales** |
| MegaLights | Des centaines de lumières ombrées | Expérimental, M2+ | Production | Rues de nuit, s'il passe sur Mac |
| Substrate | Matériaux physiques en couches | Pris en charge (actif dans le projet) | Production | **Tous les matériaux** |
| TSR | Suréchantillonnage temporel et AA | Pris en charge | Production | **Toujours actif** ; MetalFX évalué plus tard |
| Path tracer | Rendus de référence | Indisponible | Disponible | PC seulement, images de référence |
| Nuages volumétriques, brouillard | Météo, atmosphère | Pris en charge | Pris en charge | **Oui** |

**Notre Mac actuel (vérifié le 2026-11-09) : MacBook Pro M1 Pro, GPU
16 cœurs, 16 Go.** Il fait tourner Lumen logiciel mais **pas Nanite** et n'a
pas de RT matériel : bien pour prototyper le gameplay et le greybox, pas pour
juger le rendu final. Il faut une machine de référence du rendu (décision de
Nico, voir ADR-0002).

Conséquence : un Mac récent (M2 ou plus, idéalement Pro ou Max avec 32 Go ou
plus) peut produire et faire tourner tout le rendu avec Lumen logiciel. Un PC
Windows avec un GPU type RTX ajoute le ray tracing matériel, le path tracer
pour la référence, et c'est la future plateforme principale des joueurs.
L'achat éventuel est une décision de Nico, éclairée par les mesures de la
scène témoin.

## 4. Sources d'assets

| Contenu | Source principale | Secondaire | Jamais en jeu |
| ---- | ---- | ---- | ---- |
| Façades (immeubles XIXe en pierre et enduit de la Presqu'île) | Kit modulaire dans Blender : travées, corniches, balcons, volets, devantures ; trim sheets et matériaux répétables **tirés de nos propres photos** du quartier | Kits de façades Fab, retravaillés aux proportions lyonnaises | Photomaillage Grand Lyon, Google 3D Tiles (référence et blocage seulement) |
| Sol : pavés, asphalte, trottoirs, bordures | Surfaces Megascans / Fab + nos scans de trottoirs lyonnais (RealityScan) | Couches Substrate procédurales pour usure et humidité | |
| Mobilier urbain, accessoires | Fab et Megascans, rendus fictifs (sans marques) | Ébauches Tripo finies dans Blender | |
| Monuments (Hôtel de Ville, Opéra, fontaines, statues) | Modélisés à la main dans Blender d'après photos et plans ; photogrammétrie des détails | | |
| Végétation (platanes des quais, Bellecour) | Plantes Fab / Megascans, feuillage Nanite | | |
| Personnages | MetaHuman (créateur dans l'éditeur, disponible sur macOS en 5.8), visages fictifs uniquement | | Scans de personnes réelles |
| Véhicules | Modèles fictifs : ébauche Tripo ou concept, modélisation dure dans Blender, Chaos Vehicles, peinture carrosserie Substrate | Bases de véhicules Fab, débadgées et redessinées | Vraies marques, logos, plaques |
| Ciel et éclairage | Sky Atmosphere, Volumetric Clouds, brouillard, position réelle du soleil | HDRI (Poly Haven, CC0) pour le look-dev uniquement | HDRI comme ciel du jeu |
| Enseignes, affiches, menus | Flux / GPT Image, puis nettoyage en retouche | | Vraies marques |

**Prises de vue à Lyon** (sorties terrain par Nico ou toute personne sur
place) : jours couverts pour des textures sans lumière incrustée, une charte
de couleurs dans le cadre, photos RAW, recouvrement de 60 à 80 % pour la
photogrammétrie de détails. Le résultat va dans `assets/` avec une ligne dans
`assets/SOURCES.md`.

**Licences à vérifier pour chaque asset** : Fab Standard License (utilisable
dans tout moteur), anciens téléchargements Megascans sous Epic Content
License (Unreal seulement), Poly Haven CC0, données ouvertes Grand Lyon sous
Licence Ouverte 2.0 avec mention obligatoire, IGN sous Licence Ouverte. Les
prix et conditions de Megascans ont changé plusieurs fois depuis 2024 : lire
la licence sur Fab au moment du téléchargement.

## 5. La place des outils génératifs

| Outil | Bon pour | Pas pour |
| ---- | ---- | ---- |
| Flux 3 Max, GPT Image | Planches d'ambiance et images clés des plans visés ; enseignes, logos, affiches, menus, graffitis fictifs ; bases de décalques et de salissures ; pistes de textures | Cartes PBR finales sorties telles quelles (ni dé-éclairage, ni répétition, ni rugosité fiables) ; façades |
| Tripo | Ébauches rapides d'accessoires et de silhouettes de véhicules ; encombrement d'arrière-plan après nettoyage | Assets de premier plan, bâtiments, tout ce qui est proche de la caméra sans retopologie, UV et vrais matériaux dans Blender |
| Blender | Kit de façades, monuments, véhicules, nettoyage de tout maillage généré | |

Chaque asset généré est inscrit dans `assets/SOURCES.md` (outil, prompt, date).

## 6. Lumière, ciel et météo

- Position du soleil selon la date, l'heure et les coordonnées de Lyon
  (plugin SunPosition).
- Sky Atmosphere + Volumetric Clouds + Exponential Height Fog, pilotés par un
  seul contrôleur météo : couverture nuageuse, pluie, humidité, flaques,
  brouillard, vent.
- L'humidité est un paramètre global mélangé dans les matériaux maîtres
  (albédo plus sombre, rugosité plus basse, flaques dans les creux).
- Nuit : réverbères avec profils photométriques mesurés (IES), lumière des
  vitrines, MegaLights si validé.
- Exposition : auto-exposition bornée en EV100 ; jour vers EV 13 à 15, rue de
  nuit vers EV 3 à 6.

## 7. Scène témoin : plan de test

### Lieu

Un tronçon d'environ 150 m de la **rue de la République**, au nord de la
Presqu'île : rue large, façades XIXe continues, devantures, dallage et
mobilier urbain, espaces piétons et voitures à proximité. Il est dans le
quartier de la tranche verticale.

### Contenu

| Élément | Quantité |
| ---- | ---- |
| Modules du kit de façades | 8 à 12, pour les deux côtés de la rue |
| Matériaux de sol | 4 (dallage, asphalte, bordure, pavés) avec humidité |
| Accessoires | environ 50 uniques, 300 instances (poubelles, bancs, potelets, panneaux, trottinettes, chaises de terrasse) |
| Réverbères | 20, profils IES |
| Véhicules | 3 fictifs, garés et un en mouvement |
| Personnages | 10 MetaHumans qui marchent |
| Arbres | 6 platanes |

### États d'éclairage

1. Midi dégagé en juin.
2. Heure dorée en octobre.
3. Couvert avec pluie et sol mouillé.
4. Nuit avec éclairage public et vitrines.

### Protocole de mesure

- Un trajet caméra Sequencer de 60 secondes au niveau de la rue plus 3 plans
  fixes, identiques à chaque passage.
- Exécution sur la machine de référence du rendu, dans un build packagé Development (pas dans l'éditeur) en
  2560×1440, réglages Epic, TSR à 67 %.
- Capture avec le profileur CSV (`csvprofile start` et `csvprofile stop`) et
  Unreal Insights pour un passage par état ; captures `stat gpu` sur les plans
  fixes.
- Images : rendus Movie Render Queue des 3 plans fixes par état, côte à côte
  avec nos photos des mêmes endroits.
- Même passage sur un PC Windows si disponible, ray tracing matériel actif
  puis inactif.

### Critères de réussite

| Mesure | Seuil |
| ---- | ---- |
| Temps GPU par image, 95e centile, chaque état | ≤ 33,3 ms (ADR-0002) |
| Image la plus longue du trajet | ≤ 100 ms |
| Mémoire vue par le GPU | ≤ 60 % de la mémoire unifiée |
| Grille visuelle (exposition, matériaux, ombres, réflexions, humidité, densité), notée de 1 à 5 face aux photos | Moyenne ≥ 4, aucun critère sous 3 |
| Stabilité | Aucun plantage ni artefact visible (trous Nanite, fuites de lumière Lumen, traînées TSR) sur les 3 plans fixes |

Les résultats vont dans `docs/rendering/test-results/` avec captures et
chiffres, avant toute fusion sur la branche d'intégration.

### Étapes

1. Greybox de la rue d'après les données géo (emprises et hauteurs), réglage
   de l'éclairage et du post-traitement.
2. Photos terrain du tronçon et calibration des matériaux.
3. Kit de façades v1 et matériaux de sol.
4. Habillage, véhicules, personnages, contrôleur météo.
5. Mesurer, comparer aux photos, ajuster, remesurer.

## 8. Coût

| Poste | Coût |
| ---- | ---- |
| Unreal Engine, MetaHuman, RealityScan, Blender | Gratuits (redevance Unreal seulement au-delà d'1 million USD de revenus) |
| Assets Fab et Megascans | De gratuit à payant selon l'asset ; budget à fixer au besoin |
| HDRI et textures Poly Haven | Gratuits (CC0) |
| Charte de couleurs pour les photos | Environ 30 à 100 € |
| Machine de référence du rendu (nécessaire : le M1 Pro n'a pas Nanite) | PC Windows avec GPU type RTX 5080 environ 2 500 à 3 000 € ; ou Mac M4/M5 Max 48 Go+ environ 3 000 à 4 500 € |
| Temps | Le kit de façades et la scène témoin sont le plus gros effort : plusieurs semaines pour une personne |

## 9. Sources

Voir la [version anglaise](rendering-strategy.md#9-sources).
