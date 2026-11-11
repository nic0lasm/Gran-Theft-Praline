# Sources d'animation du combat (version 0.7)

*Version anglaise : [combat-sources.md](combat-sources.md)* · Inventaire du 2026-11-11, gratuit d'abord, rien acheté · ADR-0011

Clips nécessaires au combat de la 0.7, sur le personnage GASP (mannequin UEFN / UE5) : poings (3 coups, le troisième
lourd), garde, parade, esquive (4 directions), neutralisation (attaquant et victime), pistolet (dégainer, ranger,
attente en visée, aim offset, tir additif, rechargement, clic à vide), couverture (attente basse et haute, déplacement
le long, se pencher à gauche et à droite, se relever, tir à l'aveugle, entrer, sortir), réactions et morts
(`AM_HitLight_*`, `AM_Stagger_*` du fil Ragdoll), mains en l'air (faites à la main, `tools/animation/car_poses.json`).

**Vérifié** = lu sur une page le 2026-11-11 (résultat de recherche ou fiche) ; **à vérifier** = connu de mémoire ou vu
sans la licence ni la liste des clips ; ouvrir la page avant tout téléchargement.

| Source | Ce qu'elle couvrirait | Licence et coût | État |
| --- | --- | --- | --- |
| **Lyra Starter Game** (Epic, Fab) | pistolet, fusil et fusil à pompe sur le mannequin UE5 : aim offsets, tir, rechargement, dégainer, montage de corps à corps par arme, réactions, morts | gratuit, contenu Epic : utilisable dans un projet Unreal, **jamais dans le repo public** | gratuit et Epic : **vérifié** (documentation Epic) ; liste des clips d'après une analyse tierce (montage de corps à corps, système d'armes) : **à vérifier** dans le projet. Même famille de squelette que GASP : reciblage le plus simple. **Premier choix pour le pistolet** |
| **Quaternius Universal Animation Library 2** | « enchaînements à mains nues et armés », parkour, environ 130 clips sur un squelette humanoïde universel | **CC0**, gratuit, téléchargement direct (sans compte) | licence et thème **vérifiés** ; clips exacts **à vérifier** dans la visionneuse en ligne (quaternius.com/animviewer.html). **Premier choix pour poings et esquives** |
| **Quaternius Universal Animation Library 1** | environ 120 clips : morts, locomotion ; un article cite des armes | **CC0**, gratuit | licence **vérifiée** (déjà dans docs/animation/sources.fr.md) ; clips d'armes **à vérifier** |
| **Animation Starter Pack** (Epic, mannequin UE4) | jeu de postures au fusil : visée, tir, rechargement, accroupi, réactions, morts | contenu Epic gratuit (de mémoire) | **à vérifier** (encore sur Fab ou seulement dans le coffre du launcher) |
| **Personnages Paragon** (Epic) | héros stylisés au pistolet, jeux complets | contenu Epic gratuit (de mémoire), téléchargements lourds | **à vérifier** ; stylisé, dernier recours pour le pistolet |
| « Free Animation Pack » de Fab (mannequin UE5) | attente, marche, virages | gratuit pour prototypes et usage commercial (fiche) | **vérifié** : aucun clip de combat, inutile ici |
| Échantillon mains nues GameDevHero (108 clips) | coups de poing, de pied, esquives | gratuit seulement après inscription à un Discord | **écarté** : demande un compte |
| **Mixamo** (Adobe) | coups, esquives, pistolet, couverture, morts | gratuit avec un compte Adobe ; fichiers bruts non redistribuables | **à vérifier** ; demande un compte : seulement si Nico en ouvre un |
| Rapa Motion strikes, MandB unarmed, Mocap Online pistol starter | jeux complets de poings ou de pistolet sur le mannequin UE5 | payants (Rapa Motion strikes 39,99 $) ou prix non affiché | non achetés ; seulement s'il reste un trou, avec une carte à Nico |

## Recommandation

1. **Pistolet** : la couche pistolet de Lyra (aim offset, tir, rechargement, dégainer) reciblée sur le personnage GASP.
   Demandée au fil des téléchargements gratuits (aucun achat, aucun compte).
2. **Poings, garde, esquive** : Quaternius UAL 2 (CC0), reciblée par un IK Retargeter fait une fois pour le squelette
   universel (le même sert à UAL 1 pour le fil Ragdoll).
3. **Couverture** (attente basse et haute, se pencher, se relever, tir à l'aveugle) : aucun jeu gratuit trouvé ; **poses
   faites à la main** par la méthode de `tools/animation/` (os clés en JSON, construits dans Blender, importés en
   montages), environ une demi-journée, faite dans le cloud jusqu'à l'import.
4. **Neutralisation** : deux clips faits à la main (attaquant, victime), la victime passe ensuite en ragdoll (fil Ragdoll).
5. Réactions et morts restent au fil Ragdoll (ses clips et le ragdoll).

Chaque clip utilisé va dans `assets/SOURCES.md` avec sa source et sa licence ; le contenu Epic et Mixamo reste hors de Git.
