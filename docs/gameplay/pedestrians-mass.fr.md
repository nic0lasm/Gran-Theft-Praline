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

## Personnages City Sample (`-CrowdCitySample=1`, éteint par défaut)

Les 24 personnages proches peuvent être assemblés à partir du sous-ensemble City Sample Crowds sous `/Game/Crowd` (hors Git) :
un corps, trois vêtements et un visage qui suivent le corps par pose maîtresse. L'interrupteur reste éteint tant que la foule
ne tient pas le budget du jeu et ne cuit pas sans erreur de matériau.

### Matériau de tête

`M_Crowd_Head_v2` (`/Game/Crowd/Character/Shared/Materials/MetaHuman`) échoue sur une permutation à la cuisson SM6 : le base
pass Nanite (`TBasePassCSFNoLightMapPolicy`, `FNaniteVertexFactory`) demande 65 SRV et D3D en permet 64. Une permutation en
échec laisse toute la hiérarchie sans shader map, et le jeu packagé écrit « Loading a material resource None with an invalid
ShaderMap » et « Tried to access an uncooked shader map ID » dès qu'une tête se charge (celles de la foule et celle de
Nicolito, qui utilisent la même hiérarchie). L'éditeur ne demande jamais cette permutation, il ne montre donc rien.

Nos têtes sont dessinées comme des maillages à squelette ordinaires, notre copie du matériau n'a donc pas besoin de la
permutation Nanite. `tools/pedestrians/fix_crowd_head_material.py` liste les usages, les textures lues, les instances et les
maillages à squelette de la hiérarchie avec leur réglage Nanite ; avec `GTP_APPLY=1`, il retire « Used with Nanite » et
« Automatically set usage in editor » sur notre copie, la recompile et l'enregistre. Seulement dans ce projet, jamais dans le
dossier de City Sample ; à relancer sur toute nouvelle copie du pack. Contrôle : la cuisson n'écrit plus « Failed to compile
Material » pour la tête, et le journal du jeu packagé n'a plus de `LogMaterial: Error`.
