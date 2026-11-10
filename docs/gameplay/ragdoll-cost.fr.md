# Ragdoll et cassables : coût et comment dépasser 3 ragdolls

*Version anglaise : [ragdoll-cost.md](ragdoll-cost.md)* · ADR-0006

**Tous les chiffres ci-dessous sont des estimations tirées du fonctionnement du
moteur, pas des mesures.** Le premier passage sur le Mac
(`tools/ragdoll/measure-ragdoll-mac.sh`) les remplace.

## Ce qui coûte, par ragdoll simulé

| Coût | Fil | Ordre de grandeur (estimation) | Levier |
| --- | --- | --- | --- |
| Corps rigides et contraintes du physics asset (le mannequin a environ 20 à 30 corps, beaucoup sont des doigts) | physique / jeu | 0,1 à 0,3 ms | simuler moins de corps (mains cinématiques), moins d'itérations du solveur, amortissement pour dormir |
| Réécriture de la pose simulée dans le maillage | jeu | 0,02 à 0,05 ms | pose figée après le repos |
| Capsule qui suit, machine d'états | jeu | négligeable | rien à faire |
| Skinning et ombre du maillage | GPU | comme un maillage animé au même LOD | LOD, pas d'ombre au loin, masquer après le gel |
| Contacts avec les voitures et la collision de la rue | physique | croît avec le nombre de contacts : le vrai risque d'un carambolage | plafond, profil de collision |

Au repos, coût nul : pas de tick, simulation coupée après le gel. Le risque est un
carambolage avec beaucoup de corps en même temps, d'où le plafond.

## Ce qui coûte, par objet cassable

Retirer une instance d'un maillage instancié hiérarchique reconstruit l'arbre de ce
composant : quelques centaines d'instances devraient rester sous la milliseconde, mais
c'est l'accroc le plus probable (à vérifier avec le plus gros groupe de props). Le
corps lancé est un corps rigide (environ 0,02 ms), vie de 25 s, plafond de 6 en m1. Le
contrôle à 20 Hz contre les instances proches d'une voiture est une requête sphère par
groupe : attendu sous 0,1 ms.

## Dépasser 3 ragdolls (interrupteur `-RagdollLite=1`)

| Levier | En mode lite | Gain attendu (estimation) |
| --- | --- | --- |
| Physique simplifiée | mains et doigts cinématiques, 3 itérations de solveur au lieu de la valeur par défaut, amortissement 1,5 | 30 à 40 % du coût physique d'un ragdoll |
| Mise en sommeil rapide | famille de sommeil sensible, repos à 50 cm/s pendant 0,4 s, décision après 5 s au plus, pauses 0,5 / 1,5 s | un corps cesse de coûter environ 40 % plus tôt |
| Gel puis disparition | pose gardée après le repos, corps non joueur masqué 4 s plus tard | plus de coût de skinning pour les vieux corps |
| Plafond par distance | pas de ragdoll au-delà de 35 m du point de vue (le corps reste animé) ; le plafond gèle toujours le plus loin | retire les cas invisibles |
| Plafond plus haut | 6 en m1, 12 ailleurs au lieu de 3, 6, 10 | à confirmer : seulement si le coût lite par corps vaut au plus la moitié du complet |

Règle de décision : lite devient le défaut si trois ragdolls lite coûtent au plus
0,5 ms de GPU et 1 ms de CPU de plus que la référence (`-Ragdoll=0`) et que six ne
coûtent pas plus que trois complets. Sinon le plafond reste à 3.
