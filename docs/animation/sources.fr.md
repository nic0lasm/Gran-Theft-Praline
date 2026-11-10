# Sources d'animations au-delà de la locomotion

*Version anglaise : [sources.md](sources.md)* · Inventaire du 2026-11-10, gratuit d'abord, rien d'acheté.

Le Game Animation Sample (GASP) couvre marcher, courir, sauter, s'accroupir et le
franchissement. Les versions 0.4 à 0.7 demandent aussi : entrer et sortir d'une
voiture, être assis au volant, réactions aux coups, relevés après ragdoll, puis
combat et couverture. État de chaque source (**vérifié** = lu sur sa page le
2026-11-10 ; **à vérifier** = vu dans un résultat de recherche ou connu de mémoire,
licence et prix à lire sur la page avant usage).

| Source | Contenu | Licence et coût | État |
| --- | --- | --- | --- |
| **Game Animation Sample** (Epic, Fab) | locomotion ; un billet 5.8 dit qu'un personnage peut passer en ragdoll puis jouer des relevés en motion matching | gratuit, contenu Epic : utilisable dans un projet Unreal, même commercial, **jamais dans le dépôt public** (gardé en local, comme aujourd'hui) | l'affirmation ragdoll et relevé 5.8 vient d'un extrait de recherche ; la page d'Epic n'a pas pu être lue (403). **À vérifier** en ouvrant le sample 5.8 sur le Mac |
| **Quaternius Universal Animation Library 1 et 2** | environ 120 et 130 clips sur un squelette humanoïde universel : assis, mort, ramper, pousser, nager, locomotion | **CC0**, gratuit (une offre Pro payante existe, inutile) | **vérifié**. Pas de clips de voiture. Demande un IK Retargeter vers le mannequin UEFN ; CC0 permettrait même de les mettre dans Git, mais on les garde hors dépôt avec le reste |
| **Car Interaction Animset Vol 01** (Fab, fiche 55c80a94-955e-468b-904f-9df6155be5dd) | entrée et sortie de voiture, conduite, combat en voiture, squelette UE5 Manny natif, sans blueprint d'animation | prix et licence **à vérifier** sur Fab | meilleure piste pour entrer et sortir si gratuit ou bon marché ; **demande un clic de Nico** (prix) |
| **Pack civils monde ouvert** (Fab, fiche d97667b8-bf0f-43fa-b923-4e7802e4881b) | interactions intérieur et extérieur de voiture, bus, animations quotidiennes de PNJ, FBX | prix et licence **à vérifier** | candidat pour piétons et interaction voiture |
| **Mixamo** (Adobe) | des milliers de clips de capture : chutes, réactions aux coups, relevés, assis, quelques poses de conduite | gratuit avec un compte Adobe ; des messages de la communauté disent que l'usage commercial dans un jeu est permis mais **pas la redistribution des fichiers bruts** ; la page de licence serait retirée | **à vérifier**. Si utilisé : FBX hors Git, retarget en local, source notée dans assets/SOURCES.md |
| **Lyra Starter Game** (Epic) | locomotion, quelques réactions aux coups, même famille de mannequin que GASP | gratuit, contenu Epic, local seulement | de mémoire, **à vérifier** |
| **CMU Graphics Lab Motion Capture Database** | captures de chutes, relevés, assis | libre pour tout usage, fichiers BVH, conversion et nettoyage nécessaires | de mémoire, **à vérifier** ; gros nettoyage, dernier recours |

## Recommandation

1. **Relevés après ragdoll :** ouvrir d'abord le sample GASP 5.8 sur le Mac (10
   minutes, juste l'éditeur). Si les relevés y sont, les brancher sur
   `GetUpFaceUp` et `GetUpFaceDown` de `UGTPRagdollComponent` ; rien à acheter.
2. **Réactions aux coups et chutes :** clips Quaternius plus le ragdoll pour les
   violents ; Mixamo seulement s'il manque un clip.
3. **Entrer et sortir de voiture :** regarder d'abord le prix des deux fiches Fab ;
   repli : trois poses clés à la main dans Blender (tendre le bras, s'asseoir,
   fermer), environ une demi-journée, avec les accroches de montage déjà dans
   `AGTPVehicle` (`EnterMontage`, `ExitMontage`, `CarjackMontage`,
   `BreakWindowMontage`).
4. **Combat et couverture (0.7) :** décider après les points 1 à 3, avec la même
   méthode.

Chaque clip utilisé va dans `assets/SOURCES.md` avec sa source et sa licence ;
tout ce qui n'est ni CC0 ni de nous reste hors du dépôt public.

## Retarget

Le mannequin de GASP est le UEFN. Les clips d'un autre squelette passent par un IK
Rig et un IK Retargeter (créés une fois par squelette source, dans Content, hors Git
s'ils dépendent de maillages tiers), puis sont cuits en nouvelles séquences
d'animation.
