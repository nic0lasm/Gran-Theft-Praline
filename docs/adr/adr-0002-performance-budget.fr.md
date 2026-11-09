# ADR-0002 : Cible de performance et budget d'image

*Version anglaise : [adr-0002-performance-budget.md](adr-0002-performance-budget.md)*

## Statut

Proposé. La cible de fréquence d'image et la question du matériel sont des
décisions de Nico ; cet ADR consigne la recommandation en attendant.

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
« fidélité » à 30 i/s en sortie 1440p sur le Mac de développement, et un mode
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

## Décision (recommandée, en attente de Nico)

### Cible

| Mode | Images/s | Sortie | Interne (TSR) | Matériel | Priorité |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Fidélité | 30 verrouillé (33,3 ms) | 2560×1440 | environ 1706×960 (67 %) | Mac de développement | P0 |
| Performance | 60 (16,7 ms) | 1920×1080 | environ 1280×720 (67 %), Lumen Lite | Mac de dev et PC Windows | P1 |

Saccades : aucune image au-dessus de 100 ms pendant les déplacements,
compilation des shaders comprise (précache des PSO actif).

### Budget GPU, mode fidélité (33,3 ms)

| Passe | Budget (ms) |
| ---- | ---- |
| Géométrie Nanite et base pass | 6,0 |
| Virtual Shadow Maps | 4,0 |
| Lumen GI et réflexions | 8,0 |
| Éclairage direct (MegaLights compris) | 3,0 |
| Translucidité, brouillard, nuages, particules, pluie | 3,5 |
| Post-traitement et TSR | 3,0 |
| Personnages et véhicules (squelettes, cheveux, tissu) | 2,5 |
| Marge | 3,3 |

### Mémoire et contenu (valeurs de départ, affinées par la mesure)

- Pool de streaming des textures : 3 Go ; mémoire totale vue par le GPU : au
  plus 60 % de la mémoire unifiée du Mac.
- À l'écran en même temps : 40 personnages animés, 25 véhicules en mouvement,
  200 lumières locales (nuit).
- Nanite : pas de limite de triangles par asset ; pool de streaming Nanite
  dimensionné d'après la scène témoin.

## Alternatives écartées

- **60 i/s comme seule cible** : plus fluide en conduite et au tir, mais
  divise le budget par deux ; sur Apple Silicon, cela imposerait Lumen Lite
  ou une résolution interne basse, au prix d'une bonne part du
  photoréalisme. Gardé comme mode performance plus tard.
- **Pas de cible avant la tranche verticale** : l'art serait produit sans
  budget ; le concept l'interdit.

## Conséquences

- Chaque revue de contenu ou de fonction compare les chiffres capturés à ces
  budgets.
- Si le Mac de développement ne tient pas 33,3 ms dans la scène témoin en
  réglages Epic : baisser la résolution interne, passer à Lumen Lite, ou
  prendre un PC Windows comme référence qualité (décision de Nico).

## Critères de validation

La scène témoin tient un temps GPU au 95e centile inférieur ou égal à
33,3 ms dans ses quatre états d'éclairage sur le Mac de développement.

## Liens

- [ADR-0001 : pipeline de rendu](adr-0001-rendering-pipeline.fr.md)
- [Stratégie de rendu](../rendering/rendering-strategy.fr.md)
