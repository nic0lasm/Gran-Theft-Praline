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

## Cheveux des personnages assemblés (`-CrowdHair=0` pour aucun, `-CrowdGrooms=0` pour aucun groom)

Jusqu'au candidat 63 tous les passants étaient chauves (vidéo de la porte 59). Ce n'est pas un problème de cook : les têtes
de la foule de City Sample n'ont pas de cheveux à elles, c'est son code de personnage qui les ajoute (un groom lié au
visage, et une texture précalculée peinte sur le crâne), et notre assemblage (`UGTPCrowdSubsystem::DressFromCitySample`)
n'en donnait aucun.

Depuis le candidat 64 la foule porte les cheveux de City Sample, montés comme City Sample les monte (`FindHair`,
`GiveHair`, `AGTPPedestrian::SetGroom`, `SetHair`, `SetHairNear`) :

- **Ce que le projet contient est demandé au registre d'assets** sous `CrowdCharacterRoot` : les grooms
  (`Hair_<taille>_<coiffure>`), leurs cartes de cheveux (`…_CardsMesh_Group0_LOD0` à `LOD4`), les liaisons qu'Epic a faites
  de chaque groom pour chaque visage (`<sexe>/GroomBindings/GB_<maillage du visage>_<groom>`), les maillages de cheveux
  montrés de loin (`…_Helmet_LOD5`, `…_Mesh_LOD5`), les textures précalculées du crâne (`…_baked`) et les sourcils
  précalculés (`Eyebrows_*_baked`). La coiffure nommée dans `CrowdHairKeptForPlayer` (`PulledBack`) est laissée au joueur.
- **Près du joueur, le groom** : un composant groom sur le visage, avec la liaison de ce groom pour ce visage et le groom
  que la liaison nomme (deux têtes du pack peuvent avoir un groom du même nom ; une liaison ne tient que sur le sien). En
  cartes de cheveux (`bCrowdGroomCards`, le `bUseCards` du composant), et seulement les grooms qui ont des cartes : les
  brins coûtent plusieurs fois plus. La classe et les propriétés du composant sont atteintes par leur nom, comme pour les
  grooms du joueur : le plugin HairStrands n'est pas une dépendance des modules. Il ne s'affiche et ne tourne qu'à moins
  de `CrowdGroomDistance` (20 m) du joueur : le tick d'un groom est son coût sur le fil du jeu (mesuré sur ceux du
  joueur), cette distance est donc le levier si le temps d'image monte.
- **Plus loin, le maillage de cheveux lointain** de la coiffure sur la tête : un maillage statique est fixé à l'os `head`,
  remis en place par l'inverse de la pose de référence de cet os ; un maillage squelettique suit la pose du corps comme un
  vêtement.
- **Sous les deux, peint sur le visage** : sur chaque matériau du visage qui a le `HairAttributeMap` du matériau de tête,
  une instance dynamique prend la texture précalculée de la coiffure, des sourcils précalculés (`EyebrowsAttributeMap`)
  et `ShowBakedGroomTextures` (seulement une texture du même genre que celle qu'elle remplace, virtuelle ou non).
- **Couleurs** : une mélanine et une rousseur par personne, foncé plus souvent que clair, sur le matériau de tête et sur
  les scalaires des matériaux de cheveux dont le nom contient `Melanin` ou `Redness`. `CrowdBaldShare` (0,1) des hommes
  gardent le crâne nu.
- **Journal** : à la première personne habillée, `crowd hair: men: N styles in the project under …: AfroFade (groom yes,
  cards yes, bindings 3, far mesh yes, painted yes), …`, de même pour les femmes (`NONE, they stay bald` quand le projet
  n'a rien) et `crowd hair: N groom bindings and N baked eyebrows in the project; …` ; une fois par visage et coiffure,
  `crowd hair: face m_001_nrw_FaceMesh, style AfroFade: groom …, binding …, cards asked for (bUseCards)` ou `…: NO GROOM
  on this head` ; une fois par matériau, `crowd hair: face slot N <matériau>: HairAttributeMap was …, set to …; eyebrows
  …` et `crowd hair: groom … slot N material …, its scalars: …` ; `crowd hair: NO material of the face … has a
  HairAttributeMap texture: …` ; toutes les 10 s, `crowd: dressed N times: groom on the head N (not made N), far hair mesh
  N, hair painted on the scalp N, eyebrows painted N, left bald N, no head material that takes hair N; grooms shown now N
  (most at once N); without a top N, without trousers or a skirt N, without shoes N (women N), without a face N`.
- **À copier** (assets hors Git, éditeur fermé, City Sample en lecture seule), comme pour les cheveux de Nicolito :
  d'après l'inventaire du 14 novembre le projet a un seul groom (celui du joueur) et deux textures de crâne, donc sans la copie la foule n'a
  qu'un crâne peint. `GTP_CROWD_HAIR=1` avec `tools/character/crowd_hair_deps.py` liste ce qui manque : les 27 liaisons
  des neuf grooms à cartes pour les visages 001 à 003 de chaque sexe (les grooms et leurs cartes suivent au tour suivant,
  comme dépendances des liaisons), les neuf maillages lointains, cinq textures précalculées et tous les maillages de
  vêtements que le code demande ; `tools/vehicles/copy_citysample_content.py --list Saved/CitySampleCopy/missing.txt` les
  copie (`--dry-run` d'abord, pour le poids) ; relancer les deux jusqu'à ce qu'il ne reste que des vêtements que City
  Sample n'a pas (« nothing matches »).
- **Pas prouvé** : rien en jeu. Pas connu d'ici : si un groom se lie sur un visage tenu à son niveau 4 et qui suit la pose
  du corps, ce que coûtent 24 grooms en cartes, si les maillages lointains sont statiques ou squelettiques et comment ils
  tombent sur chaque tête, si le visage au niveau 4 a un matériau avec `HairAttributeMap`. Les lignes ci-dessus
  répondent à chacune.

**Un groom créé en jeu demande ses états de pipeline avant de s'enregistrer (candidat 67).** Une fois les coiffures
copiées sur le PC (porte 65 : 6 styles pour les hommes, 5 pour les femmes, 31 liaisons trouvées), chaque passe qui
posait un groom sur un personnage proche levait un « handled ensure », environ 36 lignes `Error`, et faisait échouer la
règle « 0 erreur » de la porte : `PSOPrecache.cpp`, `!IsComponentPSOPrecachingEnabled() || bPSOPrecacheCalled`, sous
`AGTPPedestrian::SetGroom`. Le composant groom du moteur ne demande ses états de pipeline qu'au chargement depuis le
disque (son `PostLoad`) ; créé par `NewObject`, il arrivait à `CreateSceneProxy` sans l'avoir fait. `SetGroom` appelle
maintenant `PrecachePSOs` sur le composant une fois le groom, la liaison et les cartes posés et avant
`RegisterComponent`, comme le moteur le fait lui-même pour un maillage squelettique. La ligne `crowd hair: face …` se
termine par « pipeline states asked for before it registered ». **Non prouvé** : rien n'est compilé ; attendu à la
prochaine porte, aucun ensure `PSOPrecache` dans aucune passe.

**Chaussures.** Un haut, un pantalon ou une jupe et des chaussures sont chargés pour tout le monde (`LoadAny`, le premier
maillage de la liste que le projet possède). À la porte 55, l'homme braqué à 3 m porte des chaussures de cuir fauve
(`gtarun-aim.jpg`) ; à 10 m dans la vidéo de la porte 59 les mêmes chaussures se lisent comme des pieds nus. Pour les
femmes rien ne le montre : les matériaux listés à la porte 36 n'en avaient aucun pour ses chaussures. Le journal le dit
maintenant : `crowd: garments of the first man|woman: top …, bottom …, shoes …` (`NONE: … barefoot` quand le projet n'a
aucun des maillages), et le script ci-dessus liste les maillages de chaussures présents et rapporte les manquants.

La foule lointaine (silhouettes beiges au-delà des 24 personnages proches) est un autre sujet : un seul maillage
provisoire, `CrowdMesh`.

## Le sol sous une personne, et aucun maillage lointain près du joueur (candidat 66)

Vidéo de la porte 62, de 96 à 140 s : des gens courent en l'air à la hauteur des boules des poteaux de feux, l'un se
tient sur un parasol, d'autres flottent à côté des arbres ; et dans la foule qui fuit certains sont encore des mannequins
blancs, dont un à 3 m de la caméra.

- **En l'air.** Le sol sous une personne était la première surface rencontrée depuis 4 m au-dessus du sol du joueur : le
  dessus d'un arbre, d'un parasol ou d'un poteau quand elle passait dessous, gardé jusqu'au regard suivant (toutes les
  2 s environ). Maintenant (`UGTPCrowdSubsystem::GroundUnder`, `GTPWalk::PickGround`, sans moteur) les surfaces de la
  ville sont rencontrées un composant après l'autre, depuis la hauteur de la tête (1,5 m) au-dessus de son propre sol,
  et son sol est la plus haute sur laquelle elle peut monter d'un pas (`GroundStepUp`, 35 cm) : ce qui la surplombe
  n'est jamais rencontré, un banc ou une poubelle laisse la place au sol qui est dessous. Quand toutes les surfaces sont
  plus hautes qu'un pas (une rampe montée en courant), la moins haute. Quelqu'un qui vient d'être placé prend la surface
  la plus proche du sol du joueur, cherchée depuis 4 m au-dessus. Vérifié hors moteur (`check_walk_logic.py`) : 12 cas
  sur 12 (arbre, poteau de feu, banc, bordure, rampe, cave, parasol au-dessus d'une table).
- **Mannequins blancs.** Ce ne sont pas des passants restés sans habits : c'est le maillage de la foule lointaine
  (`CrowdMesh`, un seul maillage figé tiré du mannequin). Les personnages complets vont aux gens les plus proches, 24
  jusqu'au candidat 65 ; quand plus de gens fuient autour du joueur, les autres étaient dessinés avec ce maillage, si
  près soient-ils. Maintenant `NearCount` vaut 36 (`-CrowdNear=N` pour mesurer un autre nombre sans build) et aucun
  maillage lointain n'est dessiné à moins de `StandInMinDistance` (25 m) du joueur : quelqu'un qui y reste sans
  personnage complet n'est pas dessiné tant qu'aucun n'est libre. Le maillage lointain lui-même ne change pas au-delà :
  l'habiller est un travail d'assets (les maillages de foule lointaine de City Sample, `Crowd/VAT`), pas fait.
- **Journal**, toutes les 10 s : `crowd: ground: N people standing more than 1.5 m above the player's ground within 60 m
  of him now (highest N m); since the start: most at once N, highest N m, a surface above the ground left aside N times`
  (une bonne passe garde les deux premiers à 0 sur sol plat) et `crowd: bodies: N full characters; people within 40 m of
  the player without one N now (most N), of whom not drawn because nearer than 25 m N now (most N)` (le dernier « most »
  dit si 36 suffisent).
- **Pas prouvé en jeu.** Pas connu : ce que coûtent 12 personnages complets de plus (13,4 ms de p50 de jeu mesurés avec
  24, pour une limite de 17,38), et si les gens vus noirs dans cette vidéo ne sont qu'à contre-jour (ceux à moins de
  20 m, soleil derrière la caméra, sont éclairés comme d'habitude).

## Un corps à terre sort du chemin des voitures (candidat 66)

Porte du candidat 62, passe `-GTARun` : les lignes du trafic montrent cinq voitures passer `cruise -> waiting (ahead
GTPPedestrian_2147479682 …)` de 22:23:05 à 22:24:10, et la ligne de la foule compte `fallen 1` de 22:23:12 à 22:24:02,
puis 0. Ce n'est pas quelqu'un debout sur la voie : une personne de la foule qui marche n'a aucune collision, donc le
seul piéton que la sonde avant du trafic (un balayage sur les pions, les véhicules et les corps physiques) peut
rencontrer est un corps renversé, qui a le profil `Ragdoll`. La personne renversée reste où gît son corps jusqu'à ce que
le corps soit donné à quelqu'un d'autre, et chaque voiture de cette voie attendait derrière tout ce temps.

Depuis le candidat 66 un corps à terre depuis 3 s sort du chemin des voitures (`AGTPPedestrian::SettleRagdoll`, une
minuterie posée par `StartRagdoll`) : sa collision devient physique seulement (aucune requête ne le voit : la sonde, une
balle) et ignore véhicules et pions ; il reste donc étendu au sol et les voitures passent dessus sans le toucher.
Journal : `pedestrian <nom>: down for 3 s, the body leaves the cars' way …`. À la porte : aucune voiture n'attend
derrière le même `GTPPedestrian` plus de quelques secondes. **Pas prouvé en jeu.** Pas fait : la personne qui se relève
et s'enfuit (pas d'animation pour cela), et les voitures du trafic qui s'arrêtent pour les gens debout ou en marche
(elles ne les voient pas).

## Un corps projeté par la voiture du joueur reste borné et glisse de la voiture

Vidéo longue de la porte 74 (`-GTARun`, candidat 74) : à 75 s la voiture volée, vers 30 km/h, heurte un homme qui monte
à 4 ou 5 m, retombe sur le toit et glisse sur le côté gauche ; à 143 s et 148 s, à 38 à 40 km/h dans une foule, des
corps volent aussi haut et 15 m en avant ; à 160 s l'un retombe sur le toit de la camionnette. (À 80 s le corps est au
sol contre le flanc gauche de la voiture ; la tache claire sur le toit est son vitrage qui reflète le ciel.) Deux
causes dans le code :

- **La chute commençait dans la voiture.** La règle était « à moins de 170 cm du centre de la voiture » : une voiture
  mesure 4 m, le pare-chocs était donc déjà entré de 30 cm dans la personne quand son corps recevait sa collision, et la
  physique l'éjectait du volume de la voiture. La boîte de la voiture est maintenant lue une fois par image
  (`UGTPCrowdSubsystem::ReadPlayerCar`) et `GTPImpact::CarReaches` dit quand son extrémité qui avance va toucher la
  personne (son rayon, plus ce que la voiture parcourt en 0,05 s) ou quand un flanc la frôle. Quelqu'un à 20 cm du
  flanc n'est plus renversé.
- **Rien ne bornait ce que la collision de la voiture faisait ensuite au corps.** `AGTPPedestrian` tique maintenant
  tant que son corps tombe et tient chaque partie dans `GTPImpact::FBodyBounds` : jamais plus vite au sol que la
  voiture qui a heurté (fois 1,05), jamais plus de 3 m/s vers le haut, plus rien ne monte une fois le bassin à 1,4 m
  au-dessus du sol où la personne se tenait (sous le toit d'une voiture), rotation de 10 rad/s au plus, et une partie
  qui commence dans la voiture en est repoussée à 3 m/s au plus. Le lancer lui-même est la règle qu'a déjà le joueur
  renversé (`GTPImpact::ClassifyCarHit` : 5,7 m/s vers l'avant et 2 m/s vers le haut à 40 km/h), donné à tout le corps
  d'un coup : sur le bassin seul il faisait faire un saut périlleux.

**Un corps couché sur une voiture.** Bassin à plus de 60 cm du sol, ni en chute ni lancé (`GTPImpact::StepOnCar`) :
après 0,4 s le corps est poussé en travers de la voiture, vers son côté le plus proche, à 2,8 m/s ; encore là-haut
après 1,5 s (une autre voiture, ou la poussée n'a pas suffi), les voitures ne le retiennent plus et il tombe au sol. Le
compte ne commence que 0,8 s après le choc, le temps qu'une personne met à tomber.

Vérifié hors moteur (`python3 tools/ragdoll/check_impact_logic.py`) : un bassin frappé vers le haut à 20 m/s à chaque
image ne dépasse jamais 1,4 m de plus que le trajet d'une image, et culmine à 143 cm après un seul coup ; de 18 à
130 km/h, à 30 et 60 images par seconde, la personne tombe avant que le pare-chocs soit en elle ; un lancer libre n'est
jamais pris pour un corps couché sur une voiture. Journal, toutes les 10 s : `crowd: run over: N people hit by the
player's car, N bodies fell; highest a pelvis went above its ground … cm (bound 140), fastest part found … cm/s, …
frames brought back within bounds, … bodies slid off a car, … dropped through one` ; par choc `crowd: <nom> hit by the
player's car at … km/h …` et `pedestrian <nom>: falls, leaves at …` ; la ligne `down for 3 s …` se termine maintenant
par la hauteur maximale du bassin. **Non prouvé** : rien n'est compilé, rien n'est vu en jeu. Attendu dans la prochaine
vidéo longue : personne au-dessus du toit d'une voiture, aucun corps resté sur une voiture plus de 2 à 3 s, « highest »
à 140 cm ou juste au-dessus.

## Voitures arrêtées sur un trottoir ou une rue piétonne (`-CrowdAvoidCars=0` pour les traverser comme avant)

Vidéo de la porte 59, de 130 à 180 s : la voiture du joueur est arrêtée dans une rue piétonne et les gens la traversent,
l'un d'eux monte sur le capot. Deux causes, deux réponses :

- **Le sol sous une personne** était un tracé de visibilité, qui s'arrête sur une voiture : la personne se tenait dessus.
  C'est maintenant un tracé sur les seuls objets statiques (`ECC_WorldStatic`, comme les tracés de sol des voitures), et
  ce qui dépasse de plus de 40 cm le sol 60 cm derrière la personne n'est pas pris pour du sol (un banc, une jardinière). Depuis
  le candidat 66 le sol se distingue de ce qui est posé dessus ou le surplombe par la section ci-dessus.
- **Une voiture sur le chemin se contourne** (`GTPWalk::AvoidBox`, sans moteur) : quatre fois par seconde le sous-système
  liste les véhicules du jeu arrêtés ou roulant à moins de 5 km/h à moins de `CarAvoidDistance` (70 m) du joueur, par la
  boîte de leur carrosserie vue de dessus, et les voitures MassTraffic lentes qui n'ont pas d'acteur. Une personne dont
  les 3,5 m suivants passent dans une boîte (45 cm de marge) prend une ligne à côté sur son trottoir ; juste devant elle
  s'arrête et se décale ; sans place d'aucun côté elle attend 4 s et fait demi-tour. Une voiture rapide ne se contourne
  pas : on la fuit ou elle renverse.
- **Journal**, toutes les 10 s : `crowd: cars: N standing or slow within 70 m of the player; people going round one N,
  standing in front of one N, within the body of one N; since the start: most within a body at once N, turned back at one
  N, something high not taken for the ground N times`. Une bonne passe garde « within the body of one » à 0 ou 1 (une
  voiture qui s'arrête sur quelqu'un).
- **Vérifié hors moteur** (`python3 tools/pedestrians/check_walk_logic.py`) : une voiture sur un trottoir, 20 personnes
  pendant 180 s. Sans contournement : 5 114 images dans sa carrosserie. Avec, sur 7 m de large : 0 image dedans et autant
  de passages (173 pour 174) ; sur 2 m de large : 0 image dedans, personne ne passe, tout le monde fait demi-tour après
  4 s. **Pas prouvé en jeu.**

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
