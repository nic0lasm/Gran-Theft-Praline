# ADR-0002 : Cible de performance et budget d'image

*Version anglaise : [adr-0002-performance-budget.md](adr-0002-performance-budget.md)*

## Statut

Accepté le 2026-11-09 par Nico : cible fidélité à 30 i/s, mode performance
à 60 i/s plus tard, sur le M1 Pro (sans achat de matériel). Les chiffres
ci-dessous sont des valeurs de départ, remesurées dans la scène témoin.

## Date

2026-11-09

## Dernière vérification

2026-11-09

## Décideurs

nic0lasm (responsable du projet)

## Résumé

Fixe la fréquence d'image et la résolution visées sur le matériel de
référence, et découpe l'image GPU en budgets par fonction, pour que chaque
choix de l'ADR-0001 se mesure contre un chiffre fixe. Recommandé : mode
« fidélité » à 30 i/s en sortie 1080p sur le M1 Pro, et un mode
« performance » à 60 i/s ajouté plus tard.

## Compatibilité moteur

| Champ | Valeur |
| ---- | ---- |
| **Moteur** | Unreal Engine 5.8 |
| **Domaine** | Rendu / Cœur |
| **Couche** | Foundation |
| **Risque de connaissance** | ÉLEVÉ (aucun chiffre UE 5.8 publié sur Apple Silicon ; ils viendront de nos mesures) |
| **Vérifications requises** | Tous les chiffres ci-dessous, mesurés dans la scène témoin |

## Dépendances

Dépend de l'ADR-0001. Permet de fixer les budgets de production (triangles,
mémoire de textures, densité de foule et de circulation). Bloque la
production de contenu en qualité finale ; le concept exige cet ADR avant la
production.

## Machine de développement

La machine de développement et de référence est le MacBook Pro actuel (lu
avec `system_profiler` le 2026-11-09) : **M1 Pro, GPU 16 cœurs, 16 Go de
mémoire unifiée**. Nico a décidé le 2026-11-09 de rester dessus, sans achat
de matériel. Selon Epic, les puces M1 n'exécutent pas Nanite et n'ont pas de
RT matériel, et 16 Go est juste pour l'éditeur avec une ville dense. Les
budgets ci-dessous sont fixés pour cette machine.

## Décision

### Cible (M1 Pro, 16 Go)

| Mode | Images/s | Sortie | Interne (TSR) | Éclairage | Priorité |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Fidélité | 30 verrouillé (33,3 ms) | 1920×1080 | environ 1280×720 (67 %) | Lumen logiciel, qualité moyenne | P0 |
| Performance | 60 (16,7 ms) | 1920×1080 | environ 1080×608 (56 %) | Lumen Lite | P1 |

Saccades : aucune image au-dessus de 100 ms pendant les déplacements,
compilation des shaders comprise (précache des PSO actif).

### Budget GPU, mode fidélité (33,3 ms)

| Passe | Budget (ms) |
| ---- | ---- |
| Géométrie (maillages à LOD) et base pass | 7,0 |
| Ombres (VSM, ou cascades + champs de distance si moins cher) | 5,0 |
| Lumen GI et réflexions | 8,0 |
| Éclairage direct | 3,0 |
| Translucidité, brouillard, nuages, particules, pluie | 3,5 |
| Post-traitement et TSR | 2,5 |
| Personnages et véhicules (squelettes, cheveux, tissu) | 2,5 |
| Marge | 1,8 |

### Mémoire et contenu (valeurs de départ, affinées par la mesure)

- Pool de streaming des textures : 1,5 Go ; mémoire totale vue par le GPU :
  au plus 9 Go sur les 16.
- À l'écran en même temps : 20 personnages animés, 15 véhicules en
  mouvement, 8 lumières locales ombrées et jusqu'à 100 sans ombre (nuit).
- Maillages statiques : LOD obligatoires ; LOD0 d'un module de façade à
  50 000 triangles au plus ; HLOD World Partition au-delà d'environ 150 m.

## Alternatives écartées

- **Acheter une machine de référence du rendu** (PC Windows RTX ou Mac
  M4/M5 Max) : Nanite, RT matériel, MegaLights, bien plus proche de la
  référence, mais 2 500 à 4 500 €. Écarté par Nico le 2026-11-09. Les assets
  restent prêts pour Nanite afin de pouvoir y revenir sans refaire le travail.
- **60 i/s comme seule cible** : plus fluide en conduite et au tir, mais
  divise le budget par deux ; sur Apple Silicon, cela imposerait Lumen Lite
  ou une résolution interne basse, au prix d'une bonne part du
  photoréalisme. Gardé comme mode performance plus tard.
- **Pas de cible avant la tranche verticale** : l'art serait produit sans
  budget ; le concept l'interdit.

## Conséquences

- Chaque revue de contenu ou de fonction compare les chiffres capturés à ces
  budgets.
- Si le M1 Pro ne tient pas 33,3 ms dans la scène témoin : baisser la
  résolution interne, passer à Lumen Lite ou réduire la densité (décision de
  Nico si cela touche la cible visuelle).
- L'image restera en dessous de la référence en détail géométrique et en
  qualité des réflexions ; c'est accepté avec la décision sur le matériel.

## Critères de validation

La scène témoin tient un temps GPU au 95e centile inférieur ou égal à
33,3 ms dans ses quatre états d'éclairage sur le M1 Pro.

## Liens

- [ADR-0001 : pipeline de rendu](adr-0001-rendering-pipeline.fr.md)
- [Stratégie de rendu](../rendering/rendering-strategy.fr.md)
