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
choix de l'ADR-0001 se mesure contre un chiffre fixe. Deux niveaux de
qualité (orientation de Nico, 2026-11-09) : le contenu est fait pour un niveau
**Ultra** photoréaliste sur du matériel puissant, et le M1 Pro fait tourner un
préréglage **M1** réduit à 30 i/s en 1080p. Un mode performance à 60 i/s
viendra plus tard.

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

### Niveaux de qualité

L'objectif reste le photoréalisme, quitte à ce que le jeu soit lourd en
poids et en calcul graphique (Nico, 2026-11-09). La qualité s'échelonne donc
par niveau, sans jamais baisser globalement :

| Niveau | Rôle | Matériel | Rendu | Cible |
| ---- | ---- | ---- | ---- | ---- |
| **Ultra** | La cible photoréaliste ; tout le contenu est produit pour elle | Apple M2 Max ou plus récent, Windows avec GPU type RTX | Nanite, Lumen haute qualité (RT matériel si disponible), Virtual Shadow Maps, MegaLights, textures en pleine résolution | 30 i/s en 1440p ou plus ; mesuré dès qu'un tel matériel est disponible |
| **M1** | Préréglage jouable de développement sur le MacBook Pro actuel | M1 Pro, 16 Go | Repli Nanite / LOD, Lumen réduit (Lumen Lite ou résolution interne plus basse, choisi d'après les mesures), lumières de nuit classiques, pool de textures plafonné | 30 i/s en 1080p, budget ci-dessous |

Règles : on ne simplifie jamais les assets ni les matériaux à la source pour
le niveau M1 ; ce niveau vient uniquement des réglages de qualité, des LOD et
du streaming des textures. La taille du client ne contraint pas le niveau
Ultra ; son budget de taille sera fixé plus tard avec Nico.

### Cible du niveau M1 (M1 Pro, 16 Go)

| Mode | Images/s | Sortie | Interne (TSR) | Éclairage | Priorité |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Fidélité | 30 verrouillé (33,3 ms) | 1920×1080 | 960×540 (50 %) | Lumen Lite (GI et réflexions en Medium), ombres en Medium : profil `m1`, choisi le 2026-11-09 | P0 |
| Performance | 60 (16,7 ms) | 1920×1080 | environ 1080×608 (56 %) | Lumen Lite | P1 |

Saccades : aucune image au-dessus de 100 ms pendant les déplacements,
compilation des shaders comprise (précache des PSO actif).

### Budget GPU du niveau M1, mode fidélité (33,3 ms)

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

### Mesures des préréglages M1 (scène témoin, 2026-11-09)

Rue en greybox, sortie 1080p, Mac déverrouillé et au repos, profils alternés
ambiance par ambiance. Temps GPU p50 / p95 en ms (fichiers bruts dans
[test-results/2026-11-09](../rendering/test-results/2026-11-09/)) :

| Profil | Midi | Heure dorée | Pluie | Nuit |
| ---- | ---- | ---- | ---- | ---- |
| `fidelity` (TSR 67 %, Lumen High) | 44,5 / 58,7 | 29,3 / 43,3 | 35,5 / 48,6 | 29,4 / 50,8 |
| `lumenlite` (TSR 67 %, Lumen Lite) | 31,3 / 42,0 | 27,5 / 41,6 | 32,8 / 45,8 | 26,6 / 47,6 |
| `res50` (TSR 50 %, Lumen High) | 28,5 / 39,2 | 24,4 / 38,4 | 29,3 / 42,8 | 22,3 / 44,0 |
| `m1` (TSR 50 %, Lumen Lite, ombres Medium) | 25,7 / 37,1 | 22,7 / 38,1 | 27,0 / 40,8 | 19,5 / 41,9 |

Le responsable du projet a choisi la combinaison `m1` le 2026-11-09. Elle tient
la médiane (19,5 à 27 ms), mais son 95e centile dépasse encore le budget de 4 à
9 ms, avec une saccade à l'heure dorée (131 ms) et une la nuit (198 ms).

**Cible M1 provisoire (choisie par le responsable du projet le 2026-11-09) :** le préréglage `m1` est retenu pour le développement à environ
25 i/s, GPU p95 au plus 42 ms et aucune image au-dessus de 100 ms. Les 30 i/s
(33,3 ms) restent l'objectif du M1, à atteindre par l'optimisation du contenu
(HLOD, objets qui projettent des ombres, complexité des lumières) d'ici la
version 0.2 ; les critères de validation ci-dessous ne changent pas.

Hypothèse sur les saccades : dans les deux cas, le thread de rendu et le GPU
bloquent ensemble pendant une image (thread de rendu à 106 et 198 ms), alors que
le thread de jeu reste sous 5 ms et que le nombre de shaders chargés ne change
pas. Cela désigne plutôt un état de pipeline (PSO) compilé à la première
utilisation sur Metal, ou une réallocation ponctuelle d'une cible de rendu ou du
pool de pages d'ombres, que le streaming des textures. À vérifier avec les
journaux de précache des PSO (`r.PSOPrecache.Validation`) quand la rue sera
habillée.

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
