# Recensement des assets et outils existants

*Version anglaise : [asset-sourcing.md](asset-sourcing.md)*
*Recensement du 2026-11-09. Prix et licences changent souvent : relire la
licence sur la fiche au moment du téléchargement et l'inscrire dans
[`assets/SOURCES.md`](../../assets/SOURCES.md).*
*Utilisé par : [roadmap](../../production/roadmap.fr.md), [stratégie de rendu](../rendering/rendering-strategy.fr.md).*

But : lister ce qui existe déjà pour ne pas le refaire, et dire pour chaque
élément si on l'utilise.

**Verdicts**

- **Recommandé** : on construit dessus.
- **À tester** : prometteur ; un court essai tranche, avec mesures sur le M1 Pro.
- **Écarté** : non utilisé, avec la raison.

**Règles de licence valables pour tout** : jeu original, aucun contenu de
jeux existants, ni marques ni personnes réelles (règles 18 à 21 des règles
de travail). Un asset qui montre une vraie marque, un vrai modèle de voiture
ou un vrai visage est retravaillé ou écarté.

---

## 1. Ce qu'il faut retenir

1. **Toute la bibliothèque Quixel Megascans est gratuite sur Fab depuis le
   2026-11-05** (fiches mises à jour jusqu'au 2026-11-12), avec de nouvelles
   conditions qui autorisent aussi l'entraînement d'IA. C'est notre plus
   gros poste de contenu (sols, surfaces, objets, végétation) qui disparaît.
   Réclamer les assets sur Fab maintenant et noter la licence.
2. **Les exemples gratuits d'Epic couvrent l'essentiel du socle gameplay** :
   Game Animation Sample (motion matching, mis à jour pour la 5.8 le
   2026-08-12), Lyra (Gameplay Ability System et armes), Chaos Vehicles,
   StateTree, Smart Objects, Mass, PCG. Aucun framework « monde ouvert
   criminel » tout-en-un ne vaut l'achat.
3. **Aucun kit payant ne reproduit Lyon.** Des kits haussmanniens parisiens
   existent (KitBash3D Paris, PolySphere City Core Paris), proches des
   façades XIXe de la Presqu'île, mais le kit de façades lyonnais tiré de nos
   photos reste le plan. Un kit Paris peut accélérer les volumes de travail
   et les rues secondaires.
4. **La France n'a pas d'équivalent de PLATEAU**, mais BD TOPO + LiDAR HD de
   l'IGN + open data du Grand Lyon + OpenStreetMap couvrent emprises,
   hauteurs, terrain, routes et emplacements des commerces. Notre
   `extract_bbox.py` transforme déjà la BD TOPO en greybox.
5. **Le principal outil de photogrammétrie (RealityScan) est Windows
   seulement.** Sur Mac : Apple Object Capture (gratuit) ou Metashape
   Standard (payant).
6. **City Sample est une référence d'étude, pas une base** : ville
   américaine, systèmes écrits pour une démo (MassTraffic n'est pas un
   plugin officiel du moteur et n'est plus développé), lourd, longtemps
   Windows d'abord.

---

## 2. Frameworks et plugins gameplay

### 2.1 Epic (gratuit, dans le moteur ou exemples officiels)

| Élément | Ce qu'il apporte | Licence, prix | UE 5.8 / macOS | Verdict | Pourquoi |
| ---- | ---- | ---- | ---- | ---- | ---- |
| [Game Animation Sample](https://www.unrealengine.com/tech-blog/download-the-latest-game-animation-sample-project-now-updated-for-ue-5-8) | Locomotion en motion matching, franchissement (enjamber, escalader), 500+ animations, mis à jour pour la 5.8 | Gratuit, EULA UE | 5.8 (2026-08-12) ; tourne sur Mac | **Recommandé** | Notre base à pied : la meilleure locomotion, gratuite |
| CharacterMovementComponent | Déplacement stable du personnage | Moteur | Oui | **Recommandé** | Voie stable pour le MVP |
| [Mover](https://dev.epicgames.com/documentation/en-us/unreal-engine/mover-in-unreal-engine) | Déplacement nouvelle génération (modes, modificateurs) | Plugin moteur | Expérimental en 5.8 (bêta manquée) | **À tester** après M1 | Doit remplacer le CharacterMovementComponent, mais les API bougent encore |
| Gameplay Ability System | Capacités, attributs (santé, armure), effets | Moteur | Oui | **Recommandé** | Standard pour armes, dégâts, états |
| [Lyra Starter Game](https://dev.epicgames.com/documentation/en-us/unreal-engine/lyra-sample-game-in-unreal-engine) | Shooter sur GAS, armes, inventaire, game features modulaires | Gratuit, mis à jour à chaque version | 5.8 (un avertissement GAS signalé en 5.8.1) | **À tester** comme lecture | Reprendre les modèles (armes, inventaire, entrées), pas le projet : structure d'arène multijoueur |
| Enhanced Input, Common UI, UMG | Remappage, menus, HUD | Moteur | Oui | **Recommandé** | Exigences d'accessibilité (remappage) |
| StateTree, AI Perception, Smart Objects | Comportements PNJ et police, vue et ouïe, objets utilisables | Moteur | Oui | **Recommandé** | Base de la police, des commerçants, des piétons |
| [Chaos Vehicles](https://dev.epicgames.com/documentation/en-us/unreal-engine/vehicles-in-unreal-engine) | Physique des véhicules à roues | Plugin moteur | Marqué expérimental en 5.8.2 ; véhicules IA cassés avec Physics Prediction (correctif annoncé) | **Recommandé** | Seule option maintenue ; Physics Prediction désactivé (solo) |
| Chaos Destruction, Geometry Collections | Vitres et objets cassables | Moteur | Oui | **À tester** | Coût sur le M1 Pro à mesurer |
| Mass Entity, MassCrowd | Des milliers d'agents légers | Moteur ; refondu en 5.8 | Oui, peu documenté | **À tester** (phase 2A) | Nécessaire pour la densité ; isolé derrière nos interfaces |
| ZoneGraph | Graphe de voies pour foule et trafic | Plugin moteur | En maintenance | **À tester** | Utilisable mais plus développé ; on pourra générer les voies depuis nos données géo |
| [City Sample](https://dev.epicgames.com/documentation/unreal-engine/city-sample-project-unreal-engine-demonstration) et MassTraffic | Démo de ville complète : foule Mass, trafic, stationnement | Gratuit, contenu « UE seulement » | Lourd (environ 18 Go pour le seul niveau trafic), difficile à porter, foule City Sample longtemps Windows seulement | **Écarté** comme base, gardé pour étude | Allure américaine ; MassTraffic « n'est pas un plugin officiel » et n'est plus développé |
| PCG | Placement par règles ; la 5.8 ajoute l'édition manuelle du résultat et un exemple bâtiments et rues | Moteur | Oui | **Recommandé** | Habillage, placement des modules de façade, végétation |
| World Partition, HLOD, Level Instances, data layers | Streaming du quartier, intérieurs sans chargement | Moteur | Oui | **Recommandé** | Base du monde ouvert et des boutiques |
| Mesh Terrain | Nouveau type de terrain lié au PCG | Moteur | Expérimental | **Écarté** pour l'instant | Presqu'île plate ; à revoir pour Fourvière et la Croix-Rousse |
| Plugin Water | Rivières, flottaison | Moteur | Oui | **À tester** | Surfaces de la Saône et du Rhône, nage (P1) |
| MetaSounds, Audio Modulation, réverbération à convolution | Audio procédural, mixage | Moteur | Oui | **Recommandé** | Véhicules, armes, réverbération des rues |
| SunPosition, Sky Atmosphere, Volumetric Clouds | Vrai soleil de Lyon, ciel, météo | Moteur | Oui | **Recommandé** | Déjà dans l'ADR-0001 |

### 2.2 Tiers (Fab et autres)

| Élément | Ce qu'il apporte | Licence, prix | UE 5.8 / macOS | Verdict | Pourquoi |
| ---- | ---- | ---- | ---- | ---- | ---- |
| [City Traffic Pro](https://www.fab.com/listings/b4aedd9f-2ca9-499d-b283-0d9864520a0d) (lyoshko) | IA de trafic sur splines : voies, changements de voie, carrefours, giratoires | Fab, payant (prix sur la fiche) | UE5, version et binaires Mac à vérifier | **À tester** en phase 2A | Peut faire gagner des semaines sur le trafic ; à comparer à nos voies issues des données géo |
| [Foothold](https://forums.unrealengine.com/t/gregorigin-foothold-mover-gasp-traversal-moves/2841979) | Escalade, rebords, échelles sur Mover | Fab, payant | Mover 5.8 ; API changeante | **À tester** après Mover | Seulement si on passe à Mover |
| [Pedestrian AI System](https://elsewaregames.itch.io/pedestrian-ai-system) | Foule sur splines, feux | Payant | Démos en 5.3 | **Écarté** | Ancien, Mass donne plus de densité |
| Drivable Cars: Basic Vehicle System | Framework voiture en Blueprint | Fab, dégâts vendus à part | Blueprint seulement | **Écarté** | Moins bon que Chaos + notre code ; marketing « façon GTA » |
| Pack Mega Open World | Monde ouvert tout-en-un en Blueprint, système de recherche | Fab, payant | À vérifier | **Écarté** | Enfermement, Blueprint seulement, qualité inconnue ; il nous faut notre propre police |
| Police AI (ancienne marketplace) | Police selon le niveau de recherche | Ancienne marketplace | Époque UE4 | **Écarté** | Non maintenu |
| Plugins d'inventaire et de roue d'armes | Interface et données d'inventaire | Divers | Divers | **Écarté** | Besoin réduit (port limité, roue d'armes) : modèles Lyra + GAS suffisent |

---

## 3. Contenu : environnements, objets, véhicules, personnages

| Élément | Ce qu'il apporte | Licence, prix | UE 5.8 / macOS | Verdict | Pourquoi |
| ---- | ---- | ---- | ---- | ---- | ---- |
| [Quixel Megascans sur Fab](https://quixel.com/) | Surfaces, objets et plantes scannés (Megaplants) | **Gratuit depuis le 2026-11-05** (fiches Fab mises à jour jusqu'au 2026-11-12) ; nouvelles conditions autorisant l'entraînement d'IA ; les anciens téléchargements gardent leur licence | Oui | **Recommandé** | Sols, pierre, asphalte, pavés, végétation, objets génériques en qualité scan |
| Assets Fab sous Fab Standard Licence | Le reste de Fab | Gratuit à payant ; utilisable dans tout moteur | Selon l'asset | **À tester** au cas par cas | Lire la licence de chaque fiche |
| [KitBash3D Paris](https://digitalproduction.com/2026/09/11/kitbash3d-paris-city-break-no-airport/) | Îlots haussmanniens, monuments, mobilier ; Nanite, collisions, Packed Level Instances | Payant (licence KitBash3D) | Chemin Unreal ; maillages Nanite dont les LOD sont à vérifier sur le M1 Pro | **À tester** (achat = décision) | Style existant le plus proche de la rue de la République ; utile en volumes de travail et rues secondaires, jamais pour les monuments lyonnais |
| [PolySphere City Core Paris](https://forums.unrealengine.com/t/polysphere-studio-city-core-paris/2675791) | Pack de quartier parisien | Fab, payant | À vérifier | **À tester** | Alternative à KitBash3D |
| [Modular Paris Haussmann](https://cgtrader.com/3d-models/exterior/house/modular-paris-haussmann) (CGTrader) | Kit de façade haussmannienne | CGTrader libre de droits, payant | FBX, pas prêt pour UE | **À tester** | Référence pour le découpage des modules |
| Timeless French villas | Maisons françaises rurales | Marketplace | UE5 | **Écarté** | Mauvaise typologie |
| [Police Generic Vehicles 3D Pack](https://www.fab.com/listings/a9e12e7c-fb9f-4799-b172-9b47c0939ec3) | Berline, pick-up, fourgon, moto de police sans marque | Fab, payant | À vérifier | **À tester** | Dessins génériques conformes à « aucun vrai modèle » ; livrée fictive à la française à faire |
| Autres packs de voitures Fab (Fleetway, 1980s Cars, Sports and Police Cars) | Voitures conduisibles | Fab | Stylisé, époque PhysX ou années 1980 | **Écarté** | Mauvais style ou physique dépassée |
| Panneaux de signalisation français (Fab, collections européennes) | Panneaux, marquages | Fab | À vérifier | **À tester** | Vérifier la licence du pack |
| [MetaHuman](https://www.metahuman.com/license) | Personnages photoréalistes, créateur dans l'éditeur (l'application web a fermé), LOD de foule | Gratuit sous 1 M$ de revenus, conditions UE | Créateur intégré à l'éditeur 5.8 sur macOS | **Recommandé** | Population, police, commerçants ; visages fictifs seulement |
| Véhicules et bâtiments de City Sample | Voitures et tours américaines | Contenu UE seulement | Lourd | **Écarté** | Allure américaine ; dessins proches de vrais modèles |
| [Poly Haven](https://polyhaven.com), [ambientCG](https://ambientcg.com) | HDRI, textures, quelques modèles | CC0 | Tout | **Recommandé** | HDRI de look-dev, matériaux d'appoint |
| Modèles Sketchfab téléchargeables | Variés | CC BY ou payant, au cas par cas | Tout | **À tester** au cas par cas | Qualité et licence variables |

---

## 4. Données géo et outils de reconstruction de Lyon

| Élément | Ce qu'il apporte | Licence, prix | UE 5.8 / macOS | Verdict | Pourquoi |
| ---- | ---- | ---- | ---- | ---- | ---- |
| BD TOPO IGN (WFS Géoplateforme) | Emprises, hauteurs, altitude au sol, routes, hydrographie | Licence Ouverte 2.0, gratuit | Notre script Python, tout OS | **Recommandé** (en service) | Colonne vertébrale du greybox ; `tools/geo-pipeline/extract_bbox.py` |
| [LiDAR HD IGN](https://www.data.gouv.fr/datasets/mns-lidar-hd/) | Nuages de points classés (sol, bâti, végétation, ponts), MNT et MNS à 0,5 m | Licence Ouverte, gratuit | PDAL / Python, tout OS | **Recommandé** | Terrain, quais, berges, hauteurs de toits, position des arbres ; vérifier la disponibilité des dalles de Lyon sur la carte IGN |
| RGE ALTI, BD ORTHO IGN | Altimétrie, orthophotos | Licence Ouverte | Tout | **Recommandé** | Référence de texture au sol, fond de mini-carte |
| [Maquettes 3D texturées du Grand Lyon (CityGML)](https://www.data.gouv.fr/datasets/611621d83eed0bb631f343d0) | Bâtiments LOD2 avec formes de toits et textures (éditions 2012, 2015, 2018) | Licence Ouverte, gratuit | CityGML ; conversion nécessaire | **À tester** | Toits plus justes que l'extrusion des emprises ; textures anciennes inutilisables en jeu |
| [Photomaillage 2023 du Grand Lyon](https://www.data.gouv.fr/datasets/6752312729bbdda4ba8f637f) | Maillage 3D Tiles issu des photos aériennes de l'été 2023, 602 km² | Open data, licence à confirmer | 3D Tiles (Cesium) | **Recommandé** en référence seulement | Lumière figée, peu de détail au niveau de la rue (ADR-0001) ; idéal pour vérifier les proportions |
| [py3dtilers / cityGMLto3DTiles du LIRIS](https://datasets.liris.cnrs.fr/node/59) | Outils ouverts d'un labo lyonnais pour convertir le CityGML du Grand Lyon | Open source | Python | **À tester** | Conversion spécifique à Lyon déjà résolue |
| OpenStreetMap (API Overpass) | Noms de rues, types de commerces, mobilier urbain, arbres, feux, lignes de tram | ODbL, crédit et partage à l'identique de la base | Tout | **Recommandé** | Placement des commerces, noms pour la carte et le GPS, position des feux |
| [Blosm](https://github.com/vvoovv/blosm) (Blender) | Import OSM, terrain, Google 3D Tiles dans Blender | Base gratuite GPL ; édition premium environ 18 $ (ancien prix) | Blender sur macOS | **Recommandé** (base gratuite) | Volumes de travail OSM rapides dans Blender |
| [Cesium for Unreal](https://cesium.com/learn/cesium-unreal/) | Streaming 3D Tiles (photomaillage, Google 3D Tiles) dans l'éditeur | Plugin gratuit ; offres Cesium ion | Travaux CI macOS 5.8 visibles ; problèmes Mac signalés en 5.6 | **À tester**, éditeur seulement | Référence visuelle dans Unreal ; pas une dépendance du jeu |
| Google Photorealistic 3D Tiles | Maillage photoréaliste de la ville | Conditions Google Maps Platform, compte de facturation | Via Cesium ou Blosm | **Écarté** pour le jeu, référence seulement | Les conditions interdisent de le livrer ; déjà la règle 21 |
| [Panoramax](https://panoramax.fr), Mapillary | Photos au niveau de la rue | Licences ouvertes (à vérifier par instance) | Web | **Recommandé** en référence | Référence de façades et devantures sans déplacement |
| Houdini (méthode City Sample) | Génération procédurale de ville | Payant (licence Indie) | macOS | **Écarté** | Coût et courbe d'apprentissage ; PCG et Blender suffisent |

---

## 5. Photogrammétrie et Gaussian splatting

| Élément | Ce qu'il apporte | Licence, prix | macOS | Verdict | Pourquoi |
| ---- | ---- | ---- | ---- | ---- | ---- |
| [RealityScan 2.x](https://www.realityscan.com/) (ex-RealityCapture) | Photogrammétrie bureau | Gratuit sous 1 M$ de revenus | **Windows seulement** | **Écarté** sur notre Mac | Demanderait un PC |
| Application mobile RealityScan | Scans de petits objets au téléphone | Gratuit | iOS | **À tester** | Scans rapides de mobilier (potelets, poubelles) |
| Apple Object Capture (API macOS, applis qui l'utilisent) | Photogrammétrie sur Apple Silicon | API gratuite ; applis variables | Oui | **À tester** | Natif sur notre Mac ; qualité à comparer |
| Agisoft Metashape Standard | Photogrammétrie bureau | Environ 179 $ licence perpétuelle | Version macOS ; performance Apple Silicon à vérifier | **À tester** (achat = décision) | Si Object Capture ne suffit pas |
| Plugins de Gaussian splatting (plugin UE de Luma, XScene, NanoGS, SplatRenderer…) | Rendu de splats dans Unreal | Gratuit à payant | Luma annonce Mac (expérimental) ; plusieurs exigent DX12 ou seulement 5.6-5.7 | **Écarté** pour le jeu | Lumière figée, pas de collision (ADR-0001) ; peut-être pour la ligne d'horizon lointaine plus tard |

---

## 6. Audio

| Élément | Ce qu'il apporte | Licence, prix | Verdict | Pourquoi |
| ---- | ---- | ---- | ---- | ---- |
| MetaSounds | Sons procéduraux de moteurs, armes, ambiances | Moteur | **Recommandé** | Intégré |
| [Bundles Sonniss GDC](https://sonniss.com/gameaudiogdc) (2015-2026) | Des centaines de Go d'effets sonores professionnels | Gratuit, usage commercial libre de droits ; le bundle 2024 interdit l'entraînement d'IA | **Recommandé** | Véhicules, armes, impacts, ambiances |
| Freesound | Sons communautaires | CC0, CC BY, CC BY-NC selon le son | **À tester** | Filtrer CC0 et CC BY seulement |
| Prises de son à Lyon | Vraies ambiances : quais, trams, marchés, cloches | À nous | **Recommandé** | Identité sonore de la ville ; nécessite un enregistreur |
| Musique de radio | Morceaux sous licence ou commandés | À définir | Plus tard (P1) | Pas en P0 |

---

## 7. Outils génératifs et de modélisation

| Outil | Place dans la chaîne | Point licence | Verdict |
| ---- | ---- | ---- | ---- |
| Flux (Black Forest Labs) | Enseignes, affiches, menus, graffitis, décalques, planches d'ambiance | Sorties de l'API annoncées utilisables commercialement ; lire les conditions API actuelles de BFL ; les poids ouverts « dev » sont non commerciaux | **Recommandé** en amont seulement (ADR-0001) |
| GPT Image | Comme Flux, enseignes riches en texte | Droits sur les sorties cédés à l'utilisateur selon les conditions du fournisseur | **Recommandé** en amont seulement |
| [Tripo](https://www.tripo3d.ai/help/privacy-policy/how-to-use-tripo-models-commercially) | Volumes de travail d'objets et de véhicules, encombrement de fond après nettoyage | **Plan gratuit : Tripo garde les droits** (CGU 5.2.1) ; usage commercial = plan payant (environ 20 $/mois) au moment de la génération | **Recommandé** sur plan payant seulement |
| Blender | Kit de façades, monuments, véhicules, nettoyage de tout maillage généré | Gratuit, GPL (les productions sont à nous) | **Recommandé** |
| Éditeur d'images | Nettoyage des enseignes générées | Au choix | **Recommandé** |

Chaque asset généré a une ligne dans `assets/SOURCES.md` avec l'outil, le
plan, le prompt et la date.

---

## 8. La pile recommandée en un tableau

| Besoin | Outil |
| ---- | ---- |
| À pied | Game Animation Sample + CharacterMovementComponent (Mover plus tard) |
| Capacités, armes, dégâts | Gameplay Ability System, modèles Lyra |
| Véhicules | Chaos Vehicles, notre réglage |
| PNJ, police | StateTree, AI Perception, Smart Objects, notre système de recherche |
| Foule, trafic | Mass (à tester), voies issues de nos données géo ; City Traffic Pro en candidat |
| Monde | World Partition, HLOD, data layers, PCG |
| Plan de la ville | BD TOPO + LiDAR HD + OSM via notre pipeline ; CityGML du Grand Lyon pour les toits |
| Surfaces, objets, plantes | Megascans (gratuit) + Poly Haven / ambientCG |
| Façades | Notre kit lyonnais tiré de photos ; éventuellement un kit Paris pour les volumes |
| Personnages | MetaHuman |
| Audio | MetaSounds + Sonniss + prises de son à Lyon |
| Enseignes et volumes de travail | Flux, GPT Image, Tripo (payant) puis Blender |

## 9. Achats à décider

Aucun n'est nécessaire avant le MVP. Candidats, chacun soumis à décision de
Nico :

| Élément | Coût approximatif | Quand |
| ---- | ---- | ---- |
| Plan Tripo payant | environ 20 $/mois | Dès qu'une sortie Tripo doit aller dans le jeu |
| Metashape Standard | environ 179 $ une fois | Si Apple Object Capture ne suffit pas |
| Kit haussmannien (KitBash3D ou PolySphere) | prix sur la fiche | Phase 2B, s'il fait gagner du temps sur les façades |
| City Traffic Pro | prix sur la fiche | Phase 2A, après un essai |
| Enregistreur audio pour les prises de son | 100 à 250 € | Phase 2 |

## 10. Sources

Les sources sont listées dans la [version anglaise](asset-sourcing.md#10-sources),
section 10, et en lien dans chaque tableau ci-dessus.
