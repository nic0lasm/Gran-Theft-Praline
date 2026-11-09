# ADR-0001 : Pipeline de rendu photoréaliste

*Version anglaise : [adr-0001-rendering-pipeline.md](adr-0001-rendering-pipeline.md)*

## Statut

Proposé. Passe à Accepté quand la scène témoin
([plan de test](../rendering/rendering-strategy.fr.md#7-scène-témoin--plan-de-test))
réussit sur la machine de référence du rendu (voir ADR-0002).

## Date

2026-11-09

## Dernière vérification

2026-11-09 (Unreal Engine 5.8.3, page des prérequis macOS et notes de version 5.8)

## Décideurs

nic0lasm (responsable du projet)

## Résumé

Comment Gran Theft Praline atteint le photoréalisme dans Unreal Engine 5.8 sur
un Mac Apple Silicon : quelles fonctions de rendu, comment les assets sont
obtenus et construits, et où se placent les outils génératifs. Réponse : la
pile UE5 standard (Lumen, Nanite, Virtual Shadow Maps, Substrate, TSR) avec le
ray tracing logiciel comme socle sur Mac, alimentée par un kit modulaire de
façades lyonnaises issu de nos photos plutôt que par des scans bruts de la
ville.

## Compatibilité moteur

| Champ | Valeur |
| ---- | ---- |
| **Moteur** | Unreal Engine 5.8 |
| **Domaine** | Rendu |
| **Couche** | Foundation |
| **Risque de connaissance** | ÉLEVÉ (5.8 est récent ; plusieurs fonctions sont en bêta ou expérimentales sur Mac) |
| **Références consultées** | Notes de version UE 5.8 ; page Epic « macOS development requirements » ; blog technique Epic sur la parité macOS |
| **API récentes utilisées** | MegaLights (production en 5.8), Lumen Lite (bêta en 5.8) |
| **Vérifications requises** | Nanite, VSM, Lumen RT logiciel et matériel, MegaLights sur le Mac de dev, mesurés dans la scène témoin |

## Dépendances

| Champ | Valeur |
| ---- | ---- |
| **Dépend de** | Aucun |
| **Permet** | ADR-0002 (cible et budget de performance), bible artistique, étude de reproduction de la ville |
| **Bloque** | La production d'art en qualité finale pour la tranche verticale |
| **Ordre** | Le greybox et le prototypage du gameplay peuvent commencer avant l'acceptation |

## Contexte

### Problème

Le concept vise la parité visuelle avec les meilleurs mondes ouverts actuels
sur un Lyon fidèle. En temps réel, le photoréalisme vient moins d'une
fonction que d'une lumière physique cohérente, d'une géométrie dense et de
matériaux crédibles, variés et usés. Il faut un pipeline qu'un développeur
seul avec des outils génératifs peut alimenter, et qui tourne sur Apple
Silicon.

### État actuel

`GranTheftPraline/Config/DefaultEngine.ini` active déjà Lumen (GI et
réflexions), les Virtual Shadow Maps, les champs de distance, le support du
ray tracing et Substrate, avec les shaders SM6 sur Metal. Aucun contenu.

### Contraintes

- Développement et première cible : macOS sur Apple Silicon (Metal, SM6).
- Niveaux sur Mac (Epic, UE 5.8) : Lumen RT logiciel dès M1 ; Nanite dès M2
  (bêta) ; Lumen RT matériel et MegaLights dès M2 (expérimental) ; pas de path
  tracer.
- Mac de développement actuel (vérifié le 2026-11-09) : MacBook Pro M1 Pro,
  GPU 16 cœurs, 16 Go de mémoire unifiée. **Il ne prend pas en charge
  Nanite** (M2 minimum) ni le RT matériel : il ne peut pas afficher ce
  pipeline en qualité finale ; la machine de référence du rendu est une
  décision de Nico (voir ADR-0002).
- Ni marques ni personnes réelles ; chaque asset inscrit dans `assets/SOURCES.md`.
- Développeur seul : privilégier les kits réutilisables et le placement
  procédural plutôt que des bâtiments uniques faits main.

### Exigences

- Soleil, ciel et météo physiques pour la latitude de Lyon (45,76° N).
- Géométrie statique dense (façades, corniches, pavés, mobilier urbain).
- Surfaces mouillées, flaques, pluie, brouillard, éclairage de nuit.
- Personnages niveau MetaHuman, véhicules avec peinture en couches.
- Tenir le budget d'image fixé par l'ADR-0002.

## Décision

1. **GI et réflexions : Lumen**, RT logiciel comme socle partout. Le RT
   matériel s'active par plateforme après mesure (PC Windows avec GPU type
   RTX d'abord, Mac seulement si la scène témoin le montre stable). Lumen Lite
   sert de repli pour les réglages bas.
2. **Géométrie : Nanite** pour tous les maillages statiques opaques
   (bâtiments, sol, accessoires), y compris le déplacement Nanite pour pavés
   et trottoirs. Les machines de développement du rendu doivent être des Mac M2 ou plus récents, ou des PC Windows avec GPU type RTX.
3. **Ombres : Virtual Shadow Maps** pour le soleil et les lumières principales.
4. **Nombreuses lumières de nuit : MegaLights**, derrière un réglage de
   qualité, gardé seulement s'il passe sur Mac ; sinon lumières ombrées à
   rayon court et appoints sans ombre.
5. **Matériaux : Substrate** (déjà actif), un matériau maître par famille
   (pierre et enduit, métal, verre, peinture carrosserie avec vernis, couche
   mouillée), pilotés par des instances et un paramètre global d'humidité.
6. **Anticrénelage et suréchantillonnage : TSR**, résolution interne fixée
   par l'ADR-0002. MetalFX sera évalué plus tard comme alternative sur Mac.
7. **Ciel et météo : Sky Atmosphere, Volumetric Clouds, Exponential Height
   Fog**, position réelle du soleil (plugin SunPosition) pour Lyon. Les HDRI
   ne servent qu'au look-dev et à la validation des assets, jamais de ciel en
   jeu.
8. **Post-traitement : caméra physiquement plausible** : plages d'exposition
   en EV100, exposition locale, tonemapper filmique, effets d'optique
   légers ; ni étalonnage lourd, ni aberration chromatique, ni bloom marqué.
   Référence couleur : nos propres photos du quartier.
9. **Stratégie d'assets** : kit modulaire de façades lyonnaises (Blender,
   trim sheets et matériaux répétables tirés de nos photos de la
   Presqu'île), Megascans et autres assets Fab pour les surfaces et
   accessoires génériques, MetaHuman pour les personnages, véhicules fictifs
   dans Blender. Les maillages ouverts de la ville et les Gaussian splats
   servent de référence uniquement. Détails dans
   [rendering-strategy.fr.md](../rendering/rendering-strategy.fr.md).
10. **Outils génératifs** (Flux, GPT Image, Tripo) en amont : concept art,
    enseignes fictives, décalques, bases de textures, ébauches
    d'accessoires. Jamais livrés comme géométrie de premier plan ni comme
    cartes PBR non retravaillées.

Le schéma d'architecture et les consignes d'implémentation sont dans la
[version anglaise](adr-0001-rendering-pipeline.md#architecture-diagram).
Points obligatoires : tout maillage statique opaque en Nanite ; matériaux
dérivés des matériaux maîtres ; textures dé-éclairées et calibrées ; unités
de lumière physiques et exposition bornée ; préréglages de qualité mesurés
dès la première scène.

## Alternatives écartées

1. **Photogrammétrie brute ou maillages 3D ouverts de la ville en jeu**
   (photomaillage Grand Lyon 2023, Google 3D Tiles) : lumière et ombres
   incrustées, résolution faible au niveau de la rue, pas de collision ni
   d'intérieurs, licence limitée pour Google. Gardés comme référence et
   blocage.
2. **Gaussian splatting pour la ville** : pas de prise en charge native dans
   UE (plugins tiers), lumière figée, pas de collision, mémoire lourde.
   Incompatible avec l'heure du jour et la météo dynamiques ; à revoir
   éventuellement pour l'horizon lointain.
3. **Éclairage précalculé (lightmaps)** : pas d'heure du jour dynamique,
   temps de calcul et mémoire énormes pour une ville.
4. **RT matériel partout, PC d'abord** : la meilleure image possible, mais
   expérimental sur Mac ; la machine de développement ne pourrait pas montrer
   la qualité finale. Gardé comme palier qualité Windows plus tard.

## Conséquences

- Positives : pile UE5 standard et documentée ; le même contenu passe du Mac
  au PC en changeant le mode de ray tracing et les réglages ; un kit
  modulaire rend 1,5 km² faisable seul.
- Négatives : Lumen logiciel est moins précis que le RT matériel ; la
  performance sur Mac est le risque principal ; photographier les façades
  demande du temps sur le terrain.
- Risques : les fonctions Mac en bêta peuvent régresser entre correctifs (on
  rejoue la scène témoin après chaque mise à jour du moteur) ; les licences
  Megascans ont changé plusieurs fois (vérifier et inscrire chaque asset).

## Critères de validation

La scène témoin tient le budget de l'ADR-0002 dans ses quatre états
d'éclairage et la grille visuelle obtient au moins 4/5 face à nos photos de
référence.

## Liens

- [Stratégie de rendu](../rendering/rendering-strategy.fr.md)
- [ADR-0002 : cible et budget de performance](adr-0002-performance-budget.fr.md)
- [Concept du jeu](../../design/gdd/game-concept.fr.md)
