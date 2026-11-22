# Piétons sur Mass (module GTPMass)

## Premier module : la foule (GTPMass, `-Pedestrians=mass`)

`GTPCrowd.h/.cpp` : `UGTPCrowdSubsystem` charge le graphe des trottoirs, crée `CrowdCount` (300, `-PedMass=N`) entités Mass
(`FGTPCrowdWalkerFragment` = le `GTPWalk::FWalker` sans moteur de l'ancien système, `FGTPCrowdLookFragment`,
`FGTPCrowdTag`) et un maillage instancié (`CrowdMesh`, une capsule en attendant le personnage cuit).
`UGTPCrowdMoveProcessor` les fait marcher (même `GTPWalk::Step`) et replace autour du joueur ceux qui sont restés derrière ;
`UGTPCrowdRepresentationProcessor` écrit les transformations des instances. `-Pedestrians=legacy` ou un nombre garde l'ancien système.
Ensuite : les proches remplacés par les `AGTPPedestrian` du pool (panique, mains en l'air, pont ragdoll), les lointains animés
(AnimToTexture), les traversées aux feux, les voies ZoneGraph à la place du graphe des trottoirs.

**Par défaut (feat/mass-default) :** sans argument, la foule Mass est le système de piétons ; `-Pedestrians=legacy` (densité du profil)
ou `-Pedestrians=N` ramène l'ancien système. La foule Mass fait ce que fait l'ancienne : propagation de la panique, badauds devant
un accident, fuite au coup de feu, mains en l'air sous la menace et ragdoll sous le véhicule du joueur ; sa densité est de 300 personnes
dont 24 personnages complets près du joueur (`CrowdCount`, `NearCount` dans `[/Script/GTPMass.GTPCrowdSubsystem]`).

## Passages piétons et voitures MassTraffic (`-CrowdCrossingWait=0` pour couper)

Jusqu'au candidat 57 la foule traversait une personne après l'autre quoi que fassent les voitures (`GTPWalk::Step` avec
`bCrossingUnsafe` toujours faux), et chaque personne sur un passage est un obstacle pour lequel les voitures MassTraffic freinent :
avec un flux continu de piétons, la voiture en tête de file ne repartait jamais (files bloquées jusqu'à 250 s à la porte 57).
Depuis le candidat 58 :

- **Ce qu'une voiture est pour un passage** (`GTPWalk::LookAtCar`, sans moteur) : seule compte une voiture dont la trajectoire coupe
  le passage, pas celle qui le longe ni celle de la rue voisine. Elle est *proche* quand son avant est à moins de 12 m du passage,
  arrêtée ou non, et *dangereuse* quand elle y sera dans moins de 3 s à sa vitesse, ou qu'elle roule dessus. Le sous-système regarde
  quatre fois par seconde les voitures MassTraffic qui roulent autour du joueur (`GTPMassTrafficCars::Find`) et la voiture que conduit
  le joueur.
- **Par vagues, comme à un feu piéton.** Avec une voiture proche, les gens ne quittent le bord que pendant les `CrossingWalkSeconds` (8)
  premières secondes de chaque `CrossingCycleSeconds` (30) ; chaque passage a sa propre horloge. La voiture passe entre deux vagues.
  Avec une voiture dangereuse ils attendent quelle que soit l'horloge. Sans voiture en vue ils traversent aussitôt, comme avant.
- **Personne ne reste bloqué, d'aucun côté.** Qui attend au bord n'est pas un obstacle pour les voitures (pas de
  `FMassTrafficObstacleTag` avant de s'engager) : une voiture n'attend jamais quelqu'un qui l'attend. Personne n'attend plus de
  `CrossingWaitMaxSeconds` (45) : ensuite il traverse et les voitures freinent. Ceux qui attendent se tiennent côte à côte le long
  du bord.
- **La ligne de journal, toutes les 10 s :** `crowd: crossings: N waiting at the kerb (longest wait now N s), N on a crossing; since
  the start: most waiting at once N, longest wait N s, N stepped off after the longest wait allowed (45 s); cars: N seen, N of the N
  crossings around have one near (N dangerous)`. Une bonne passe montre des gens en attente dès qu'un passage a une voiture proche,
  une plus longue attente sous 25 s environ, et personne (ou presque) qui s'engage après l'attente maximale. `cars: 0 seen (no
  MassTraffic cars to ask for)` : la carte n'a pas de voitures MassTraffic, personne n'attend.
- **Vérifié hors moteur** (`python3 tools/pedestrians/check_walk_logic.py`) : une personne toutes les 2 s pendant 300 s à un passage
  où une voiture attend. Une par une, le passage n'est jamais libre. Par vagues il est libre 62 % du temps, jamais occupé plus de
  11 s de suite, la plus longue attente est de 21 s et autant de gens traversent. Douze voitures sont lues (arrêtée avant, arrivant
  vite, déjà passée, dans la rue voisine, le long du passage). **Pas prouvé en jeu** : que les files de la porte 57 se vident.

## Personnages City Sample (par défaut ; `-CrowdCitySample=0` pour les mannequins)

Les 24 personnages proches sont assemblés à partir du sous-ensemble City Sample Crowds sous `/Game/Crowd` (hors Git) :
un corps, trois vêtements et un visage qui suivent le corps par pose maîtresse. Allumé par défaut depuis le candidat 43 :
à la porte du candidat 42, le jeu packagé tenait un game p50 de 11,49 ms avec eux (budget 13) sans erreur de matériau.
`-CrowdCitySample=0` ramène les mannequins d'avant (le maillage du corps du joueur) ; un projet sans le pack y revient de
lui-même et le dit (« crowd: assembled City Sample characters off, mannequins »). Avec les mannequins, le journal nomme
leur corps et leurs matériaux (« crowd: mannequin material slot … »).

### Matériau de tête

`M_Crowd_Head_v2` (`/Game/Crowd/Character/Shared/Materials/MetaHuman`) échoue sur une permutation à la cuisson SM6 : le base
pass Nanite (`TBasePassCSFNoLightMapPolicy`, `FNaniteVertexFactory`) demande 65 SRV et D3D en permet 64. Une permutation en
échec laisse toute la hiérarchie sans shader map, et le jeu packagé écrit « Loading a material resource None with an invalid
ShaderMap » et « Tried to access an uncooked shader map ID » dès qu'une tête se charge (celles de la foule et celle de
Nicolito, qui utilisent la même hiérarchie). L'éditeur ne demande jamais cette permutation, il ne montre donc rien.

City Sample rencontre le même échec et y répond dans son propre `DefaultEngine.ini` : `r.Nanite.AllowSkinnedMeshes=0`, avec
un commentaire qui nomme `M_Crowd_Head_v2` et ses plus de 64 SRV. Notre `[SystemSettings]` a la même ligne depuis le
candidat 41 : la permutation Nanite des maillages à squelette n'est plus compilée, le matériau d'Epic reste intact, et le
jeu packagé n'a plus de `LogMaterial: Error`, ni avec la foule ni avec Nicolito (porte du candidat 41). Conséquence : aucun
maillage à squelette du jeu ne peut être dessiné par Nanite.

### Coût des personnages proches

Chaque personnage proche suit l'économie des anciens piétons et des voitures de l'IA : au-delà de `NearDetailDistance` (20 m),
pas d'ombre et le niveau de détail `NearFarLOD` (2) pour le corps, les vêtements au niveau du corps (`bSyncAttachParentLOD`) ;
la pose est rafraîchie 20 fois par seconde au-delà de 15 m et 12 fois au-delà de 25 m (`bNearReduceAnimRate`), à chaque image
pour un corps qui tombe. Réglages dans `[/Script/GTPMass.GTPCrowdSubsystem]`.

### Animations des personnages assemblés

Les corps de City Sample ont le squelette MetaHuman, pas celui du mannequin : la marche, l'attente, la course et la boucle
mains en l'air sont cuites pour lui sous `/Game/MetaHumans/GTP/Anims` (`tools/character/retarget_player_animations.py`,
préfixe `MH_`). La boucle mains en l'air est `MH_A_HandsUp` (`CrowdHandsUpAnimation`) ; tant qu'elle n'est pas cuite, celle
du mannequin est jouée et le journal dit « ANOTHER SKELETON ». `-GTARun` enregistre ce que voit le joueur quand le braquage
réussit (`Saved/Screenshots/gtarun-aim.png`, `gtarun-aimclose.png`) : les états qu'il compte ne disent rien de la pose affichée.
