# Bible artistique : Gran Theft Praline

*English version: [art-bible.md](art-bible.md)*
*Version 1, 2026-11-09. Statut : en vigueur ; les chiffres marqués « départ » sont revus après les mesures de la 0.2.*
*Entrées : [concept](../../design/gdd/game-concept.fr.md), [ADR-0001](../adr/adr-0001-rendering-pipeline.fr.md),
[ADR-0002](../adr/adr-0002-performance-budget.fr.md), [stratégie de rendu](../rendering/rendering-strategy.fr.md),
[roadmap](../../production/roadmap.fr.md), [catalogue d'assets](../../production/asset-catalog.fr.md),
[mesures de la scène témoin](../rendering/test-results/2026-11-09/README.md).*

---

## 1. À quoi sert ce document

Une seule règle du jeu pour tout ce qui entre à l'image : photos de
référence, matériaux, modèles Tripo, façades, éclairage, post-traitement.
Chaque asset, chaque fil et chaque revue visuelle s'y réfère. En cas de
conflit avec un autre document sur un sujet artistique, la bible fait foi ;
les ADR restent la référence pour les choix techniques et les budgets de
temps d'image.

**L'intention en une phrase :** on doit pouvoir confondre une capture du jeu
avec une photo prise rue de la République un jour ordinaire.

---

## 2. Piliers visuels

| # | Pilier | Ce que ça veut dire | Ce qu'on refuse |
| ---- | ---- | ---- | ---- |
| 1 | **Lyon, pas une ville générique** | Proportions, pierres, enduits, ferronneries, pavés, enseignes et mobilier de la Presqu'île, relevés sur le terrain | Façades « européennes » passe-partout, mobilier de catalogue sans rapport avec Lyon |
| 2 | **La lumière d'abord** | Soleil et ciel physiques à 45,76° N, exposition bornée, ombres de contact, rebond Lumen | Lumière d'ambiance plate, scènes trop claires la nuit, exposition qui « pompe » |
| 3 | **Des matériaux calibrés et usés** | Albédo sans lumière cuite, rugosité qui varie, salissures, traces d'eau, usure aux arêtes | Matières propres et uniformes, rugosité constante, textures IA brutes |
| 4 | **La densité de la vraie rue** | Câbles, panneaux, poubelles, deux-roues, terrasses, affiches, feuilles, chewing-gums | Rues vides et rangées, objets alignés au cordeau |
| 5 | **Une caméra de jeu naturelle** | Objectif sobre, TSR stable, flou de mouvement modéré | Effets « bodycam » : distorsion, grain fort, aberration chromatique, secousses, éblouissements |

Référence de **qualité** : *Bodycam* (UE5) pour les matériaux scannés, la
lumière Lumen et la densité près de la caméra. Référence de **caméra et de
gameplay** : le modèle GTA, 3e personne classique et 1re personne en option
(décision de Nico, 2026-11-09, ADR-0001). On ne reprend **aucun** effet de
caméra corporelle.

---

## 3. Direction photo : Lyon

### 3.1 Ce qu'on reproduit

Tranche verticale : nord de la Presqu'île, de Bellecour aux Terreaux, entre
Saône et Rhône. Tout ce qui se voit dans la rue est **reproduit fidèlement
d'après photos terrain**, y compris les enseignes et marques réelles
(section 3.5).

| Élément | Caractère à retrouver (à valider sur photos terrain) |
| ---- | ---- |
| Façades XIXe (rue de la République, rue Édouard-Herriot, quais) | Immeubles de rapport de 5 à 7 niveaux, pierre de taille calcaire claire (crème à ocre pâle), socles et marches en calcaire dur gris, balcons filants en fer forgé aux 2e et 5e, corniches, bandeaux, toits en zinc et ardoise avec lucarnes |
| Façades enduites (rues secondaires, pentes vers les Terreaux) | Enduits ocre, jaune, rose saumon, ocre rouge ; volets persiennés ; génoises ; peintures écaillées par endroits |
| Rez-de-chaussée | Vitrines continues, devantures en bois ou alu, rideaux métalliques, stores bannes, enseignes en drapeau et bandeaux, terrasses |
| Sols | Dalles claires sur l'axe piéton, pavés en granit, bordures en pierre, asphalte gris des voies, plaques d'égout, bandes podotactiles ; sol rouge de Bellecour |
| Végétation | Platanes des quais et de Bellecour, bacs plantés, tilleuls ponctuels |
| Monuments | Hôtel de Ville, Opéra, statue équestre et place Bellecour, ponts sur Saône et Rhône : modélisés à part (Blender + photos), jamais générés par Tripo en entier |
| Eau | Saône et Rhône : eau verte à grise, reflets mats, berges aménagées |

Les photos terrain (RAW, carte de couleur dans le champ, temps couvert pour
les textures « délumées ») vont dans `assets/` avec une ligne dans
`assets/SOURCES.md`. Une capture du jeu qui contredit une photo terrain a
tort.

### 3.2 Lumière et heures de référence

Quatre états servent à toutes les revues et mesures (mêmes états que la
scène témoin). Valeurs d'exposition **de départ** en EV100, bornes min et
max de l'exposition auto ; elles corrigent la surexposition de l'heure
dorée et la nuit trop claire relevées le 2026-11-09.

| État | Date et heure (Lyon) | Soleil | Ciel et atmosphère | Exposition EV100 (min – max) | Intention |
| ---- | ---- | ---- | ---- | ---- | ---- |
| **Midi** | juin, 13 h 30 | ≈ 68° de hauteur, plein sud | Ciel clair, légère brume de chaleur | 13,5 – 15 | Ombres nettes, pierre presque blanche, contraste fort mais lisible |
| **Heure dorée** | octobre, 18 h | ≈ 6° ouest | Ciel dégradé, brume sur la Saône | 10 – 12,5 | Façades est éclairées en orange, rues en ombre bleutée, pas de ciel blanc |
| **Pluie** | novembre, 11 h | caché | Couvert bas, pluie, brouillard léger | 10,5 – 12,5 | Sol mouillé et flaques, reflets des vitrines, couleurs désaturées |
| **Nuit** | janvier, 22 h | sous l'horizon | Ciel sombre légèrement orangé par la ville | 2 – 6 | Noirs profonds, flaques de lumière des lampadaires et vitrines, monuments éclairés |

Règles :
- Unités physiques : soleil en lux, lumières locales en candela ou lumens,
  profils IES pour les lampadaires.
- Nuit : éclairage public LED blanc chaud (≈ 3000 K) en majorité, quelques
  restes sodium orangés à vérifier sur le terrain ; vitrines plus froides ;
  mise en lumière des monuments dans l'esprit du Plan Lumière de Lyon.
- Jamais de lumière de « remplissage » invisible pour déboucher les ombres :
  si c'est trop sombre, on corrige l'exposition ou on ajoute une vraie source.

### 3.3 Saisons

La tranche verticale se joue en **automne** par défaut (feuilles des platanes
jaunies, lumière rasante, pluie fréquente), ce qui maximise le mouillé et la
lumière dorée. Les autres saisons arrivent avec le cycle jour-nuit et la météo
(0.13) :

| Saison | Différences visibles |
| ---- | ---- |
| Hiver | Platanes nus, brouillard du matin sur les fleuves, ciel blanc, illuminations, givre léger (pas de neige en 1.0) |
| Printemps | Feuillage vert clair, averses, terrasses qui rouvrent |
| Été | Feuillage dense, ombres dures, poussière claire, terrasses pleines, stores sortis |
| Automne | Feuilles au sol, flaques, lumière basse, ciels chargés |

### 3.4 Couleur

- Palette naturelle tirée des photos : pierre crème, zinc gris-bleu, fer
  forgé noir à vert foncé, enduits ocre, asphalte gris neutre.
- Pas d'étalonnage stylisé (pas de LUT teintée, pas de virage teal-orange).
  Seule la balance des blancs varie avec la lumière.
- La couleur d'un asset se juge sous l'éclairage neutre de la scène de revue
  (section 10), pas sous la lumière dorée.

### 3.5 Enseignes, marques et textes (décision de Nico, 2026-11-09)

Le jeu est à usage **non commercial** : Nico demande que **toutes les
enseignes soient reproduites fidèlement**, marques réelles comprises (par
exemple l'enseigne de restauration rapide à l'angle de la rue de la
République et de la place Bellecour, si elle s'y trouve sur les photos
terrain). Cette règle remplace l'interdiction des marques des documents
précédents pour les enseignes et la signalétique de la ville.

| Sujet | Règle |
| ---- | ---- |
| Enseignes de boutiques, restaurants, banques, pharmacies | Reproduites à l'identique d'après photos terrain : logo, couleurs, typographie, position, taille, éclairage de nuit |
| Signalétique urbaine (plaques de rue, transports, mobilier municipal) | Reproduite à l'identique : vrais noms de rues, vraies couleurs |
| Affiches, menus, vitrines | Contenu réel ou plausible d'après photos ; pas de texte inventé incohérent |
| Source des logos | Photo terrain redressée, ou vectorisation fidèle refaite à la main ; jamais de logo inventé par une IA quand le vrai existe |
| Rangement | Les textures et matériaux d'enseignes réelles vont dans un plugin de contenu local **hors Git**, `Plugins/CityBrands/`, comme le contenu Epic et Fab : le dépôt public est sous licence MIT et ne peut pas relicencier des marques qui appartiennent à leurs propriétaires |
| Registre | Chaque enseigne a sa ligne dans `assets/SOURCES.md` (photo d'origine, date, adresse) avec la mention « marque réelle, usage non commercial » |
| Véhicules | Modèles réels du parc circulant en France, toutes marques (décision de Nico, 2026-11-09 ; voir [véhicules](../gameplay/vehicles.fr.md), section 10) ; logos et plaques en décals, rangés comme les enseignes |
| Inchangé | Visages et personnes réelles : interdits. Aucun asset, personnage ou logo repris d'un jeu existant |

Point de vigilance, noté une fois : l'usage non commercial réduit le risque
mais ne vaut pas autorisation des titulaires de marques. Si le jeu est un
jour diffusé publiquement (build téléchargeable, vidéo monétisée), la liste
des enseignes de `Plugins/CityBrands/` permet de les remplacer d'un bloc.

---

## 4. Matériaux PBR

### 4.1 Principes

- **Substrate** (activé dans le projet) pour tous les matériaux.
- **Un matériau maître par famille**, réglé par instances. Aucun graphe de
  matériau unique en production.
- **Albédo délumé et calibré** : ni ombre, ni reflet, ni occlusion cuits.
  Contrôle à la pipette sur la carte de couleur des photos.
- **La rugosité porte le réalisme** : jamais une valeur constante ; chaque
  matériau a une variation (salissure, usure, poussière, traces d'eau).
- **Mouillé global** : paramètre `Wetness` et `Puddles` dans la collection
  `MPC_Weather`, lus par tous les maîtres (albédo plus sombre, rugosité plus
  basse, flaques dans les creux de la hauteur).

### 4.2 Plages de valeurs (sRGB 8 bits, carte couleur de référence)

| Matière | Albédo | Métallique | Rugosité typique |
| ---- | ---- | ---- | ---- |
| Calcaire clair (façade) | 150 – 210 | 0 | 0,65 – 0,9 |
| Enduit coloré | 110 – 200 | 0 | 0,7 – 0,95 |
| Asphalte sec | 40 – 70 | 0 | 0,75 – 0,95 |
| Granit des pavés | 70 – 130 | 0 | 0,55 – 0,85 |
| Fer forgé peint | 20 – 60 | 0 | 0,4 – 0,7 |
| Zinc de toiture | 120 – 160 | 0,8 – 1 | 0,4 – 0,6 |
| Inox, alu brut | 180 – 230 | 1 | 0,2 – 0,5 |
| Verre de vitrine | Substrate verre, transmission | 0 | 0,0 – 0,1 (+ salissures) |
| Végétation | 40 – 140 | 0 | 0,5 – 0,8 |
| Peinture de carrosserie | Couche de base + vernis (Substrate) | selon peinture | vernis 0,02 – 0,1 |

Bornes absolues : aucun albédo non métallique sous 30 ni au-dessus de 240.

### 4.3 Matériaux maîtres

| Maître | Usage |
| ---- | ---- |
| `M_Master_Surface` | Opaque standard : props, mobilier, intérieurs |
| `M_Master_Facade` | Pierre et enduit : textures tuilables + trim sheet + masque d'usure par sommets + mouillé |
| `M_Master_Ground` | Sols : mélange en hauteur de 2 à 3 couches, flaques, parallaxe (profil M1) |
| `M_Master_Glass` | Vitrines, vitres, abribus, avec salissure et reflets |
| `M_Master_CarPaint` | Carrosseries : base, flocons, vernis Substrate |
| `M_Master_Foliage` | Feuillage double face, transmission |
| `M_Master_Decal` | Décalques Substrate (DBuffer) |
| `M_Master_Emissive` | Enseignes lumineuses, écrans, vitrines la nuit, en unités physiques (nits) |

### 4.4 Résolutions et densité de texels

Les textures sources sont toujours produites pour le profil **Ultra**. Le
profil M1 les réduit par le streaming et les groupes de LOD de textures,
jamais en réduisant le fichier source.

| Catégorie | Densité cible (Ultra) | Taille source max | Profil M1 (taille max chargée, départ) |
| ---- | ---- | ---- | ---- |
| À portée de main (rez-de-chaussée, mobilier, props, armes) | 10,24 px/cm | 4K (8K pour armes en 1re personne) | 2K |
| Façades au-dessus du rez-de-chaussée | 5,12 px/cm | 4K tuilable + trim sheet 4K | 2K |
| Sols (pavés, dalles, asphalte) | 10,24 px/cm | 4K tuilable (8K Megascans gardé en master) | 2K |
| Toitures, objets lointains | 2,56 px/cm | 2K | 1K |
| Décalques (salissures, fissures, marquages) | selon atlas | atlas 4K | 2K |
| Personnages (MetaHuman) | standard MetaHuman | standard | préréglage MetaHuman bas |
| Véhicules | 10,24 px/cm | 4K | 2K |

Pool de streaming : **1,5 Go** sur le profil M1 (ADR-0002) ; Ultra à fixer
sur une machine Ultra (départ 4 Go). Cartes standard par matériau :
`_BC` (couleur), `_N` (normales DirectX, comme Unreal), `_ORM` (occlusion,
rugosité, métallique), `_H` (hauteur) si parallaxe ou tessellation, `_M`
(masques), `_E` (émissif).

### 4.5 Trim sheets et kits de façades

- Un **kit de façade** = modules par travée (rez-de-chaussée, étage courant,
  étage noble à balcon, attique, toiture, angle) sur une grille de **50 cm**,
  hauteurs d'étage relevées sur le terrain.
- Chaque famille de pierre a **une trim sheet 4K** (corniches, bandeaux,
  appuis, encadrements, moulures) et **une texture tuilable 4K** pour les
  surfaces planes, tirées de nos photos.
- Variété par instances (teinte, saleté, mouillé) et par décalques, pas par
  nouvelles textures.
- Ferronneries (balcons, garde-corps) : maillages réels (Nanite), pas de
  masque d'opacité sur le profil Ultra ; masque autorisé sur les LOD lointains
  du profil M1.

### 4.6 Décalques

- Toujours au-dessus d'un matériau propre : coulures sous les appuis,
  salissures de pied de mur, chewing-gums, taches d'huile, fissures,
  marquages au sol, affiches, graffitis relevés sur le terrain.
- Atlas 4K par famille, matériaux en `M_Master_Decal` (Substrate DBuffer).
- Budget de départ : 150 décalques visibles à l'écran en Ultra, 60 en M1,
  distance d'apparition réduite sur M1.

---

## 5. Budgets par catégorie et par profil

Rappel (ADR-0002) : tout le contenu est fait pour **Ultra** (Nanite, Lumen
haute qualité, textures pleine résolution) ; le profil **M1** (30 i/s en
1080p sur le M1 Pro, sans Nanite ni RT matériel) passe par les LOD, le
streaming et les réglages, jamais par des sources simplifiées. Valeurs de
**départ**, revues après les mesures de la 0.2.

### 5.1 Géométrie

| Catégorie | Ultra : maillage Nanite source (triangles) | M1 : LOD0 max (triangles) | LOD M1 | Collision |
| ---- | ---- | ---- | ---- | ---- |
| Petit prop (borne, poubelle, panneau, < 1 m) | ≤ 300 k | 3 k | 3 LOD | boîte ou capsule simple |
| Prop moyen (banc, lampadaire, table, 1 – 3 m) | ≤ 1 M | 10 k | 4 LOD | convexes simples |
| Grand prop (abribus, kiosque, conteneur) | ≤ 2 M | 30 k | 4 LOD | convexes simples |
| Module de façade (une travée, un niveau) | ≤ 3 M | 50 k (ADR-0002) | 4 LOD + HLOD | boîtes, jamais la géométrie complète |
| Monument | par pièce ≤ 5 M | 150 k par pièce | 5 LOD + HLOD | simplifiée à la main |
| Arbre (platane) | Nanite feuillage | 60 k | 4 LOD + imposteur | tronc seulement |
| Véhicule (extérieur) | ≤ 1,5 M | 120 k | 4 LOD | Chaos Vehicles (physique) |
| Personnage | MetaHuman | préréglage MetaHuman | LOD MetaHuman | capsule |
| Arme 1re personne | ≤ 500 k | 40 k | 3 LOD | aucune |
| Meuble d'intérieur | ≤ 500 k | 8 k | 3 LOD | convexes simples |

### 5.2 Mémoire de textures par catégorie (une instance unique)

| Catégorie | Ultra (Mo, compressé, mips compris) | M1 (Mo chargés) |
| ---- | ---- | ---- |
| Petit prop | ≤ 22 (un jeu 2K) | ≤ 6 |
| Prop moyen ou grand | ≤ 45 (un jeu 4K) | ≤ 12 |
| Famille de façade (tuilable + trim + masques) | ≤ 120 | ≤ 30 |
| Véhicule | ≤ 90 | ≤ 25 |
| Arme 1re personne | ≤ 120 (8K) | ≤ 25 |

Les maîtres Tripo bruts (≈ 1 M sommets, 64 à 74 Mo par GLB) sont au-dessus
de ces chiffres : ils sont nettoyés et ré-échantillonnés dans Blender avant
import (section 8).

### 5.3 Densité à l'écran (rappel ADR-0002, profil M1)

20 personnages animés, 15 véhicules en mouvement, 8 lumières locales avec
ombres et jusqu'à 100 sans ombre la nuit. Le profil Ultra n'a pas de plafond
fixé tant qu'aucune machine Ultra n'a été mesurée.

---

## 6. LOD, Nanite et HLOD

1. **Tout maillage statique opaque** est importé avec Nanite activé (profil
   Ultra) **et** une chaîne de LOD vérifiée à la main (profil M1). Le maillage
   de repli Nanite est réglé pour ne pas dépasser le LOD0 M1 du tableau 5.1.
2. Réduction par LOD d'environ 50 % ; tailles d'écran de départ 1,0 / 0,5 /
   0,25 / 0,1. Aucun « pop » visible dans les 3 plans fixes de la scène
   témoin.
3. Sources du LOD M1 : réduction Unreal pour les maillages simples ;
   variante légère **Smart Mesh** Tripo ou retopologie Blender pour les props
   Tripo ; LOD MetaHuman pour les personnages.
4. **Translucides et squelettiques** : toujours des LOD (pas de Nanite).
5. **HLOD World Partition** au-delà d'environ 150 m ; imposteurs pour les
   arbres lointains.
6. Le relief des pavés et des murs passe par la tessellation Nanite en
   Ultra et par la parallaxe (`_H`) en M1.

---

## 7. Nommage et dossiers Content/

### 7.1 Préfixes

| Préfixe | Type | Préfixe | Type |
| ---- | ---- | ---- | ---- |
| `SM_` | Maillage statique | `SK_` | Maillage squelettique |
| `M_` | Matériau maître | `MI_` | Instance de matériau |
| `MF_` | Fonction de matériau | `MPC_` | Collection de paramètres |
| `T_` | Texture | `BP_` | Blueprint |
| `L_` | Niveau (ex. `L_RueRepublique`) | `LI_` | Level Instance |
| `PCG_` | Graphe PCG | `NS_` | Système Niagara |
| `DA_` | Data Asset | `DT_` | Table de données |
| `IA_`, `IMC_` | Entrées (déjà en place) | `ABP_`, `AM_` | Anim Blueprint, montage |
| `PP_` | Volume ou réglages de post-traitement | `IES_` | Profil photométrique |

Forme : `Préfixe_Famille_Nom_Variante`, en anglais, PascalCase, sans espace
ni accent. Exemples : `SM_Bench_WoodCastIron_A`, `MI_Facade_LimestoneCream_Dirty`,
`T_Facade_LimestoneCream_BC`, `SM_Facade_Rep_Bay3_Floor2`.
Suffixes de textures : `_BC`, `_N`, `_ORM`, `_H`, `_M`, `_E`.

Le code catalogue Tripo (`W1-01`…) n'entre pas dans le nom : il est noté
dans `assets/SOURCES.md` et dans les métadonnées de l'asset (balise
`CatalogId`).

### 7.2 Dossiers

Un dossier par asset (maillage, instances et textures ensemble), rangé par
famille et non par vague Tripo :

```
Content/
  Maps/                         L_RueRepublique, futurs niveaux
  Characters/Player/            (en place) enfants du GASP, caméra
  Characters/NPC/
  Input/                        (en place) IA_*, IMC_*
  Materials/Masters/            M_Master_*
  Materials/Functions/          MF_*
  Materials/Parameters/         MPC_Weather, MPC_TimeOfDay
  Environment/Facades/<Kit>/    modules, trim sheets, instances
  Environment/Ground/<Matière>/
  Environment/Props/<Famille>/<Asset>/      ex. Props/StreetFurniture/Bench_WoodCastIron/
  Environment/Vegetation/<Espèce>/
  Environment/Landmarks/<Monument>/
  Environment/Decals/<Famille>/
  Interiors/<TypeDeBoutique>/
  Vehicles/<Véhicule>/
  Weapons/<Arme>/
  Lighting/IES/   Lighting/Sky/
  PostProcess/
  FX/   UI/   Audio/
Plugins/ (hors Git, locaux)
  GameAnimationSample/          contenu Epic (déjà en place)
  FabContent/                   Megascans et autres assets Fab
  CityBrands/                   enseignes et marques réelles (section 3.5)
```

Ce rangement remplace `Content/Props/<vague>/` du catalogue.

---

## 8. Charte Tripo

Elle s'applique à toutes les vagues du [catalogue](../../production/asset-catalog.fr.md).

### 8.1 Ce que Tripo fait et ne fait pas

| Oui | Non |
| ---- | ---- |
| Mobilier urbain, props, objets portés, mobilier et produits d'intérieur, ornements de façade, ébauches de carrosseries | Façades et bâtiments, sols, personnages, monuments entiers, véhicules finaux sans reprise dans Blender, logos et enseignes réels (photos terrain, section 3.5) |

Un modèle Tripo peut aller **près de la caméra** s'il passe la revue
(section 10) après nettoyage dans Blender ; sinon il reste en arrière-plan.

### 8.2 Modèle de prompt

Prompts **en anglais**, une seule entité par génération, dans cet ordre :

```
<objet précis>, <matières et couleurs>, <état : usure, saleté, rouille>,
<contexte : street furniture of Lyon, France, 2020s>,
photorealistic, real-world proportions, single object, isolated,
standing upright on flat ground, front view facing camera,
no base plate, no ground, no background, no people,
even neutral lighting, no baked shadows, PBR materials
```

Exemple W1-01 :
`public bench with wooden slats and anthracite cast-iron legs and armrests, weathered wood, slightly dusty, street furniture of Lyon, France, 2020s, photorealistic, real-world proportions, single object, isolated, standing upright on flat ground, front view facing camera, no base plate, no ground, no background, no people, even neutral lighting, no baked shadows, PBR materials`

Règles :
- Reprendre les notes et dimensions de la ligne du catalogue ; pour les
  objets lyonnais typiques, donner une image de référence (photo terrain
  d'abord, sinon Flux ou GPT Image).
- Texte et logos : demander `no text` ; les inscriptions réelles sont posées
  ensuite par décalque d'après photo.
- Réglages Studio : qualité maximale, textures PBR à la plus haute
  résolution, génération du maître sans limite de polygones ; la variante
  légère passe par Smart Mesh.
- Plusieurs variantes par objet, on garde la meilleure ; les autres ne sont
  pas exportées.

### 8.3 Échelle, pivot et orientation (nettoyage Blender)

| Règle | Valeur |
| ---- | ---- |
| Unités | Mètres dans Blender (échelle d'unité 1,0) ; Unreal en centimètres après import |
| Échelle | Dimensions du catalogue (colonne Taille) à ± 2 % ; Tripo ne garantit pas l'échelle, on la fixe toujours dans Blender |
| Pivot | Au sol, au centre de l'empreinte. Objets muraux (applique, climatiseur, distributeur, plaque) : au centre de la face arrière, sur le plan du mur |
| Orientation dans Unreal | Face avant vers **+X**, haut vers +Z, pour que le placement à la main et par PCG tourne correctement (à vérifier sur le premier import de la vague 1) |
| Transformations | Appliquées (rotation 0, échelle 1) avant export |
| Géométrie | Pas de faces internes ni de pièces flottantes ; ouvertures réelles (grilles, ajours) en géométrie pour Ultra |
| UV | Canal 0 pour les textures ; textures rebakées à la densité de la section 4.4 |
| Matériaux | Remplacés par une instance du maître correspondant ; les textures Tripo servent de base, recalibrées (albédo délumé, rugosité variée) |
| Sorties | Maître GLB : `assets/tripo/<vague>/<id>.glb` (hors Git). Unreal : `Content/Environment/Props/<Famille>/<Asset>/` |

---

## 9. Post-traitement

Objectif : un objectif photo sobre, pas une caméra corporelle.

| Réglage | Valeur de départ | Note |
| ---- | ---- | ---- |
| Anticrénelage | TSR toujours actif | Résolution interne fixée par ADR-0002 (67 % en M1) |
| Exposition | Auto, histogramme, bornes de la section 3.2 par état | Vitesse d'adaptation lente (≈ 1 s vers le clair, 3 s vers le sombre) |
| Exposition locale | Activée, modérée | Évite les ombres bouchées sans aplatir |
| Tonemapper | Filmique d'Unreal par défaut | Pas de LUT stylisée |
| Balance des blancs | Selon l'état (plus chaud à l'heure dorée) | Jamais de teinte uniforme (la nuit sépia du 2026-11-09 est un défaut) |
| Bloom | Faible (intensité ≈ 0,3) | Seulement sur les vraies sources lumineuses |
| Flou de mouvement | Modéré (quantité ≈ 0,3, max 2 %) | Plus marqué en conduite rapide |
| Vignettage | Léger (≈ 0,2) | |
| Profondeur de champ | Désactivée en jeu | Cinématiques et mode photo seulement |
| Grain | Désactivé | Essai à très faible dose (≤ 0,1) seulement si les captures montrent des bandes de couleur |
| Aberration chromatique, distorsion, reflets d'objectif, saleté d'objectif | Désactivés | Effets « bodycam » exclus (décision de Nico) |
| Brouillard | Exponential Height Fog + volumétrique | Plus dense le matin près des fleuves |

Les valeurs vivent dans `Content/PostProcess/` (un `PP_` par état et par
profil) ; elles sont mesurées dans la scène témoin à chaque changement.

---

## 10. Revue visuelle et validation

Aucun asset n'entre sur `next` sans revue avec capture :

1. **Scène de revue** : asset posé sous un éclairage neutre (ciel couvert,
   EV fixe) à côté d'une charte de gris et d'une carte de couleur virtuelle.
2. **Contrôles** : échelle (mannequin de 1,80 m à côté), pivot,
   orientation, albédo dans les plages, rugosité variée, aucune ombre cuite,
   LOD sans pop, collision, coût dans les budgets de la section 5.
3. **En situation** : capture dans la rue de la République sous les
   4 états, à côté d'une photo terrain du même type d'objet ou du même lieu.
4. **Grille** (stratégie de rendu, section 7) : exposition, matériaux,
   ombres, reflets, mouillé, densité notés de 1 à 5 contre les photos ;
   moyenne ≥ 4, aucun critère sous 3.
5. **Registre** : ligne dans `assets/SOURCES.md`, statut dans le catalogue.

---

## 11. Évolution

- Révision après les mesures de la 0.2 (choix des leviers du profil M1) et
  après le premier lot de la vague 1 intégré.
- Toute règle qui touche l'intention visuelle (piliers, enseignes, caméra)
  se change par décision de Nico ; les chiffres de départ se mettent à jour
  sur mesure, avec la date.
