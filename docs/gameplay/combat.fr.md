# Combat et armes (version 0.7 « Armé »)

*Version anglaise : [combat.md](combat.md)* · ADR-0011 · interrupteur : `-Combat=0` (coupé)

État au 2026-11-11 : règles écrites et vérifiées hors Unreal (`tools/combat/combat_logic_test.cpp`,
150 vérifications, tâche CI `Combat rules`). Composants Unreal pas encore écrits : ils attendent que les
réactions aux coups et la réponse à la menace du fil Ragdoll arrivent sur `next`.

## Nombres (par défaut, tous dans `DefaultGame.ini` quand les composants existeront)

| Quoi | Valeur |
| --- | --- |
| Santé, armure, endurance | 100, 0 à 100, 100 |
| Armure | prend 70 % d'un coup tant qu'elle dure ; corps à corps et chutes passent à travers |
| Régénération | après 6 s sans dégât, 4 points/s, jusqu'à 50 (le reste : trousse, nourriture, hôpital) |
| Réapparition | 6 s après la mort, hôpital fictif le plus proche, 3 s sans dégât, armure perdue |
| Pistolet (9 mm fictif) | 12 balles, réserve jusqu'à 96, 330 coups/min semi-automatique, rechargement 1,5 s (automatique à vide) |
| Dégâts du pistolet | 20 au torse jusqu'à 15 m, jusqu'à 10 à 45 m ; tête × 2,5, bras × 0,6, jambe × 0,7 |
| Dispersion (demi-angle) | hanche 3°, visée 0,5°, +0,7° par tir jusqu'à +4°, revient à 6°/s, +2° en courant, tir à l'aveugle × 4 |
| Recul | 1,4° vers le haut par tir (± 0,35° de côté), revient à 10°/s |
| Joueur pris pour cible | prend 45 % des dégâts des PNJ, jamais plus de × 1,5 à la tête |
| Poings | 8, 10 puis 18 (lourd, fait tomber) ; 8 d'endurance par coup ; enchaînement de 3 |
| Esquive | 0,45 s, invulnérable de 0,05 à 0,30 s, 18 d'endurance |
| Garde | arrête 80 % d'un coup de face (± 70°) pour 14 d'endurance ; coup lourd ou plus d'endurance = garde brisée, étourdi 1,2 s |
| Neutralisation | à 1,5 m, de dos (± 60°) sur quelqu'un qui ne se méfie pas, ou sur quiconque est étourdi ; jamais sur quelqu'un qui braque une arme |
| Aide à la visée (manette) | accroche à 9° et 40 m au début de la visée (hostiles d'abord), friction × 0,45 à moins de 3,5° |
| Couverture | prise à moins de 1,2 m d'un mur face au joueur (± 50°) ; basse (voiture, muret) ou haute ; saut vers un abri jusqu'à 12 m dans un cône de 40° |

Équilibre vérifié par le test : un policier tombe en 5 balles au torse ou 2 à la tête à 10 m ; le joueur
encaisse 12 balles au torse sans armure, 23 avec une armure pleine ; un enchaînement complet de poings (36)
ne tue jamais un corps en pleine santé.

## Contrat avec les autres fils

**Sorties (délégués de `UGTPCombatSubsystem`) :**

| Délégué | Arguments | Qui écoute |
| --- | --- | --- |
| `OnShotFired` | tireur, origine, direction, arme, le `FHitResult` de la balle et s'il a touché | Police (`ShotsFired`), Audio (détonation, queue, écho par rue), Piétons s'il leur faut plus que la peur |
| `OnDamageDealt` | auteur, victime, dégâts, zone, corps à corps ou non, santé restante | Police (`Assault`, `HitPolice`), HUD (marqueur de touche), Audio (impact) |
| `OnPawnKilled` | tueur, victime, corps à corps ou non | Police (`Killing`, `KillPolice`), missions, statistiques |
| `OnPlayerDied` / `OnPlayerRespawned` | lieu, hôpital | Police (efface le niveau), économie (facture d'hôpital si voulue), sauvegarde, HUD |
| `OnArmedThreat` | source, cible, en visée ou non | Script de braquage (fast-food), Police (`ArmedThreat`) |

**Appels faits par ce fil :** `UGTPHitReactionComponent::ReactToHit(FGTPHitInfo)` après chaque coup ;
`UGTPThreatSubsystem::SetThreat(Source, bArmed, Origin, Aim)` à chaque image où un corps armé vise,
`ClearThreat()` à la fin de la visée, `NotifyShot(From, Target)` à chaque tir. Cela remplace `-ThreatDemo`.

**Proposé au fil Police :** nouveaux `EGTPCrimeKind` : `Assault` (poings, 15 de chaleur), `ShotsFired` (20),
`ArmedThreat` (25), `Killing` (60), `KillPolice` (90). À deux étoiles, les policiers tirent avec
`UGTPWeaponComponent` (facteur de précision de l'IA 2,5 × la dispersion du joueur, un seul tireur à la fois
par unité en 0.7).

**Proposé au fil Ragdoll :** quand le corps a un `UGTPHealthComponent`, `UGTPPlayerReactionComponent` y envoie
ses dégâts de voiture et de chute au lieu de son propre `Health`.

**HUD (fil Interface) :** petit point de visée qui s'ouvre avec la dispersion (rayon = angle de dispersion à
l'écran) ; marqueur de touche (une croix, 0,15 s, rouge si mort) ; santé et armure en deux arcs fins autour de
la mini-carte, montrés seulement quand ils changent ou sous 50 % ; munitions `12 / 36` en bas à droite
seulement arme en main ; vignette rouge sous 25 % de santé ; invites de couverture et de neutralisation dans
le coin d'aide existant ; aide à la visée et sensibilité dans le menu des commandes.

**Sons (fil Audio) :** détonation du pistolet proche (sèche, courte) et lointaine (claquement et queue,
retardée par la distance), écho selon la largeur de la rue (la carte des zones existe), clic à vide,
chargeur sorti et remis, culasse ; impacts de balle par type (poussière, étincelles, carrosserie, verre,
bois, terre, eau, chair) ; sifflement d'une balle qui frôle ; coups de poing léger et lourd, parade, souffle
d'esquive ; chutes de corps déjà faites (ragdoll). Les détonations baissent l'ambiance et le trafic 0,5 s.

**Commandes (créées ici, remappées par le fil Interface) :**

| Action | Clavier et souris | Manette |
| --- | --- | --- |
| Viser (garde sans arme) | bouton droit | gâchette gauche |
| Tirer (coup de poing sans arme ou sans viser) | bouton gauche | gâchette droite |
| Recharger | R | X / Carré |
| Esquiver | Alt gauche | B / Rond |
| Couverture (prendre, quitter) | Q | bouton droit (RB) |
| Changer d'épaule | X | croix gauche |
| Ranger, dégainer | 1 | croix bas (croix haut = grande carte) |

À vérifier par le fil Interface contre les actions existantes (`F` monte en voiture, `E` interagit, `Espace`
saute).

## Budget des effets (proposé au fil Optimisation)

Ligne dans l'enveloppe « nuit et effets » (2 ms GPU), pour le pire cas d'une fusillade dans la rue au
palier C, profil M1, plafonné à 40 i/s :

| Élément | Plafond sur M1 | Coût visé |
| --- | --- | --- |
| Effets d'impact (Niagara, simulation CPU, sans lumière, sans collision) | 12 vivants à la fois, 0,6 s de vie, particules par type 8 à 48 (`GTPDamage::MaxParticles`) | ≤ 0,25 ms GPU p95 |
| Décalques d'impact (réserve, sans mélange de normales) | 32 dans le monde, le plus ancien recyclé, fondu 20 s | ≤ 0,05 ms GPU |
| Sang (« façon GTA », choisi par Nico le 2026-11-11) | un décalque de flaque sous chaque corps mort (8 au plus, 90 s, fondu 10 s) ; taches sur les vêtements par le paramètre de matériau `GTPBlood` (0 à 1, +0,25 par balle), aucun dessin en plus | ≤ 0,05 ms GPU |
| Flamme de bouche | sprite + lumière de 0,05 s **sans ombres**, joueur seulement ; tirs des PNJ : sprite seul | ≤ 0,05 ms GPU |
| Fumée | une petite bouffée par tir (≤ 6 particules, peu de surdessin), aucune sous la pluie | compris ci-dessus |
| Traçantes | coupées sur M1, actives en « fidelity » | 0 sur M1 |
| Vitrines qui cassent | cassables existants (budget du fil Ragdoll) | non compté ici |
| Fil de jeu (traces, santé à 10 Hz, aide à la visée) | 3 tireurs, 1 trace par tir, traces de couverture seulement à l'appui | ≤ 0,4 ms CPU |

Total proposé (sang compris) : **≤ 0,35 ms GPU p95 et ≤ 0,4 ms CPU du fil de jeu** dans la scène de fusillade, en A/B
contre `-Combat=0`. Plafond atteint : l'effet le plus ancien disparaît, rien n'attend. Un profil
« fidelity » peut doubler les plafonds et activer les traçantes.

Accepté par le fil Optimisation le 2026-11-11 avec trois amendements, intégrés au budget :

- D'où vient la marge : sur next 8b16340, le GPU p95 de la porte vaut 20,05 / 22,31 / 22,27 / 22,54 ms (fixe /
  parcours / nuit / pluie) pour 24 ms, soit 1,5 ms de marge dans la pire scène ; cette ligne en prend un quart.
- Vérifié **de nuit et sous la pluie**, pas seulement à midi (étincelles, fumée et verre translucides y coûtent
  plus). Plafonds durs sur les particules et décalques vivants, en réserve recyclée ; aucune lumière dynamique ne
  projette d'ombre ; les flammes de bouche jamais.
- **Aucune image > 100 ms au premier tir** : systèmes Niagara et leurs PSO préchargés au lancement du niveau
  (apparition cachée de préchauffage), premier tir mesuré à froid.

## Plan

1. Règles hors Unreal (fait, cette branche).
2. Quand feat/hit-reactions et feat/threat-response sont sur next : `UGTPHealthComponent`,
   `UGTPCombatSubsystem` (`ApplyHit`, délégués, `-Combat=0`), `UGTPWeaponComponent` avec le pistolet et les
   effets d'impact, réapparition à l'hôpital. Compilation de contrôle sur le PC, puis créneau Mac (porte +
   scène de fusillade).
3. Poings, esquive, garde, neutralisation (`UGTPCombatMeleeComponent`).
4. Couverture (`UGTPCoverComponent`), tir à l'aveugle.
5. Police qui tire à deux étoiles (avec le fil Police), vidéo d'un combat de 2 min.

## Tir façon GTA (lot du 13/11)

Ce que le lot ajoute, au-dessus de ce qui précède :

- **Deux armes** (règles dans `GTPWeaponLogic.h`, namespace `GTPArsenal`, testées hors Unreal) :

  | | Pistolet 9 mm (fictif) | Carabine (fictive) |
  |---|---|---|
  | Tir | semi-automatique, 330 coups/min | automatique, 600 coups/min (gâchette tenue) |
  | Chargeur / réserve de départ | 12 / 36 | 30 / 90 |
  | Recharge | 1,5 s | 2,2 s |
  | Portée | 60 m | 90 m |
  | Dispersion hanche / visée | 3° / 0,5° | 4,5° / 0,6° |
  | Recul par coup | 1,4°, revient à 10°/s | 0,75°, revient à 4°/s (la rafale monte) |
  | Dégâts torse à 10 m | 20 (policier : 5 coups) | 26 (policier : 4 coups) |
  | Depuis une voiture | oui (une main) | non (deux mains) |

  Le joueur a les deux ; la police et le personnel n'ont que le pistolet. Changer d'arme prend 0,45 s.
- **Visée à l'épaule** : en visant, la caméra à pied (`UGTPFollowCameraComponent`) passe à 175 cm derrière
  l'épaule, décalée de 52 cm, 4 cm plus haut, champ 60°, sans recentrage automatique : le personnage dans le tiers
  bas gauche, de la tête à la taille, bras et arme visibles (135 cm et 14 cm plus haut ne montraient que la tête). Le changement d'épaule glisse d'un côté à
  l'autre. Réglages `Aim*` dans `DefaultGame.ini`.
- **Visée assistée à la manette** : la gâchette gauche accroche le corps le plus proche du réticule (9°, 40 m,
  les policiers d'abord, jamais un mort ni quelqu'un derrière un mur). La souris n'est jamais assistée.
- **Tir depuis la voiture** : au volant, viser (clic droit, ou bumper gauche à la manette) sort le pistolet ;
  clic gauche, ou bumper droit pendant la visée (le frein à main reprend ensuite), tire par les vitres latérales
  ou le pare-brise, jamais droit derrière (`GTPDriveBy` : 150° de chaque côté, de -30° à +35°). La dispersion est
  ×1,6 et grandit avec la vitesse (+2,5° à 108 km/h). La voiture du joueur n'est jamais touchée par ses tirs.
- **Police et panique** : chaque tir du joueur va au système de menace (les piétons fuient, y compris la foule
  Mass, et le trafic freine ou fuit). Les délits passent par une seule table, celle du fil Police
  (`UGTPWantedComponent::BindCombat`, docs/gameplay/wanted.md), branchée sur les délégués du combat : coups de feu,
  menace armée, agression, policier touché, meurtre, meurtre de policier ; les témoins décident si ça compte.
  Le combat garantit deux choses à cette table : `OnArmedThreat` ne part que si quelqu'un est dans le viseur
  (12°, 25 m), pas en visant une rue vide ; au volant, le tireur annoncé est la voiture du joueur
  (`UGTPWeaponComponent::GetShooter`), donc un tir par la vitre compte comme un tir à pied.
  `UGTPCombatSubsystem::IsBenchRunning()` dit aux écouteurs que le banc `-CombatBench=1` tourne.
- **Lyra** (contenu Epic, hors Git, migré sur le PC avec ses chemins d'origine) : flamme de bouche
  `NS_WeaponFire_MuzzleFlash_Rifle` (préchauffée au chargement), impacts `NS_ImpactConcrete` et `NS_ImpactGlass`
  (`ImpactSystems`). Meshes d'armes et animations : chemins à renseigner dans `Looks` et `Clips`
  (`UGTPWeaponComponent`) d'après le rapport du PC. Chaque asset absent est signalé une fois dans le journal ;
  le jeu tourne sans.

Commandes ajoutées : arme suivante = molette, 2, ou bumper gauche à pied ; arme précédente = molette vers le haut.

### Portage Lyra (13/11 au soir)

D'après le relevé du PC (`/mnt/project-files/combat/lyra-armes-valeurs.md`). Contenu Epic, hors Git ; aucun code
Lyra n'est repris : Lyra repose sur GAS et ShooterCore, que l'ADR-0011 écarte. On prend ses assets.

- **Armes en main** : `SK_Pistol` et `SK_Rifle` (chemins Lyra), tirs et flamme depuis leur socket `Muzzle`,
  visibles seulement dégainées. Placement : socket `weapon_r` du joueur s'il existe, sinon `hand_r` avec
  `AttachOffset` (à régler d'après le rapport du script).
- **Animations** : les montages Lyra (Fire, Reload, Equip, DryFire des deux armes) et les poses de visée
  `MM_*_Idle_ADS` sont sur le squelette UE5 Mannequin ; le joueur est en UEFN Mannequin. Le script
  `tools/combat/unreal/lyra_weapons_setup.py` (éditeur, Python) les retargete dans `/Game/Combat/Weapons/Anim/`
  (suffixe `_UEFN`), liste les slots présents dans les animation blueprints du joueur, dit si `weapon_r` existe,
  et écrit les lignes de `DefaultGame.ini` qui correspondent à ce qui existe vraiment
  (`Saved/GTPCombat/lyra-weapons-setup.txt`).
- **Slots** : Lyra joue Fire en `FullBodyAdditivePreAim` et le reste en `UpperBody`. Si l'animation blueprint du
  joueur n'a pas ces slots, un clip peut forcer son slot (`Clips=(…,Slot="DefaultSlot")`) ; une animation additive
  n'est jamais jouée dans un slot non additif. La pose de visée (`_AimLoop`) se tient pendant toute la visée, dans
  le slot donné. Sans slot haut du corps, elle reste coupée : c'est le blocage principal, à lever par une
  modification de l'animation blueprint (slot `UpperBody` + AimOffset), qui est un asset de Content.
- **Impacts** : `NS_ImpactConcrete` et `NS_ImpactGlass` (s'ils ne dépendent pas des data channels de Lyra),
  impacts de balles en décalque : le nôtre (`M_GTP_BulletHole`, fait par `make_blood_material.py`) et celui de
  Lyra sur le verre. 24 au plus, 30 s, accrochés à ce qui est touché, préchauffés au chargement.
- **Sons** : les MetaSounds de tir Lyra (`MSS_Weapons_Pistol_Fire`, `MSS_Weapons_Rifle_Fire`) et le clic à vide
  sont proposés au fil Audio, qui possède la lecture des sons de combat.
- **Valeurs** : les multiplicateurs lisibles de Lyra (visée 0,65 ; accroupi 0,6 à 0,65 ; saut 1,25 à 1,6) sont
  proches des nôtres ; la cadence, le chargeur, la recharge et les dégâts de Lyra ne se lisent pas en Python,
  les nôtres restent ceux du tableau ci-dessus.

### Désarmement et ramassage (nuit du 13/11)

- **Qui lâche son arme** : un corps non joueur qui a encore des munitions lâche son arme quand il meurt, ou
  quand il prend un coup lourd ou une neutralisation l'arme dégainée (`GTPPickup::Disarms`). Un policier désarmé
  garde le composant d'arme mais n'a plus rien (`HasAmmo()` faux) : le fil Police le fait passer au corps à corps
  ou à l'arrestation. Une arme déjà lâchée ne tombe jamais deux fois.
- **Ce qui reste au sol** : `AGTPWeaponPickup`, le mesh de l'arme (ou une petite boîte sombre si le mesh manque)
  qui tourne lentement, avec son chargeur plus la moitié de sa réserve, au moins 6 balles et au plus deux
  chargeurs (`GTPPickup::DropRounds`). Six armes au sol au plus (`MaxPickups`, la plus ancienne part), 60 s
  chacune (`PickupLifeSeconds`).
- **Ramasser** : en marchant dessus, ou en roulant dessus (l'arme du conducteur, `FindHeld`). Une arme que le
  joueur n'a pas est ajoutée avec ces balles en réserve (la carabine prise à un policier) ; sinon les balles vont
  dans sa réserve jusqu'au maximum. Réserve pleine : l'arme reste au sol.
- **Foule et tirs** : chaque tir entendu (`UGTPCombatAudio::PlayShot` → `UGTPAudioWorld::OnLoudSound`) fait fuir
  la foule dans `ShotScareRadius`, et le système de menace fait fuir piétons et trafic. Avec `-Audio=0`, seul le
  système de menace reste : aucun bruit fort n'est diffusé.
- **Sons Lyra** : `lyra_weapons_setup.py` liste aussi les chemins complets des sons de tir et de tir à vide
  trouvés après la migration, pour les réglages audio.

### Haut du corps et retarget (13/11, après la migration)

- **Premier passage sur le PC** : les assets Lyra sont là, le retarget était bloqué : pas de rig IK UEFN au chemin
  du sample et pas de mesh du mannequin Lyra. Le script construit maintenant son propre retargeter
  (`RTG_GTP_UE5_to_UEFN`) à partir des premiers rigs IK trouvés (cible : `IK_SKM_UEFN_Mannequin` des piétons quand
  celui du sample manque), associe les chaînes par nom (la méthode éprouvée en 5.8 par
  `create_pedestrian_retargeter.py`), ne reprend jamais le retargeter d'un autre fil, prend `SKM_Manny` de Lyra
  comme mesh source (ou tout mesh sur le squelette des animations) et appelle le retarget par lot de 5.8 avec des
  mots-clés (`target_path`).
- **Haut du corps seulement** : le blueprint du joueur (`SandboxCharacter_CMC_ABP`) n'a que `DefaultSlot`. Les
  clips joués dans un slot passent par un profil de mélange créé au chargement à partir du squelette du joueur, en
  mode WeightFactor (`UpperBodyRoot`, `spine_01` et ses enfants pèsent 1, tous les autres os 0 ; un profil BlendMask
  déclenche une assertion dans un slot de montage en 5.8) : dégainer, recharger et la pose de visée
  laissent les jambes marcher. Aucune modification du blueprint. Sans l'os, ou si le masque ne peut pas être
  créé, les clips jouent sur tout le corps comme avant (signalé une fois dans le journal).
- **Clips** : actions Lyra en pose complète (`MM_*_Equip`, `_Reload`, `_DryFire`, `_Idle_ADS`) dans
  `DefaultSlot`. Le Fire de Lyra n'existe qu'en additif : il attend un slot additif dans le blueprint du joueur ;
  d'ici là, le recul de la caméra et la flamme portent le tir.
- **Nicolito, le joueur (visuel MetaHuman)** : l'arme va dans la main qu'on voit. Quand le mesh du pion est caché et que le
  corps du visuel y est accroché, l'arme est attachée au `hand_r` du corps, placée à chaque image sur la prise du
  `weapon_r` du pion, décalée de l'écart entre les deux mains (bonne orientation quels que soient les axes des os
  du corps) ; sinon sur le `weapon_r` du pion. Avec `-PlayerVisualBaked`, dégainer remet le corps en retarget à
  l'exécution (il suit le pion, clips d'arme compris) et ranger le ramène aux clips cuits. Le visuel par
  défaut est un MetaHuman (`PlayerVisualClass` dans `DefaultGame.ini`, retarget à l'exécution avec
  `RTG_UEFNped_to_Metahuman` ; Kellan, du Game Animation Sample, tient le rôle jusqu'à ce que le MetaHuman de
  Nicolito existe, simple changement de config ;
  `bPlayerVisualBaked=True` au même endroit ou `-PlayerVisualBaked` passe aux clips cuits) ; `-PlayerVisual=`
  (vide) montre le mannequin pour les tests d'armes.
- **Bancs de test** : `-CombatBench=1` tire toutes les 0,45 s debout, visée à l'épaule tenue (pose et caméra de
  visée) ; `-CombatBench=2` marche en cercle lent (demi-vitesse, 15 degrés par seconde) et tire à la hanche : la
  caméra large montre les jambes qui marchent pendant que la recharge joue sur le haut du corps.
- **Pose de visée tenue (correctif de la nuit du 13/11)** : la capture du candidat 27 ne montrait pas de pose de
  visée. Chaque clip joué en montage arrêtait la pose : le montage Fire de Lyra (même groupe de montages) à chaque
  tir, la recharge et le dégainage dans `DefaultSlot`, et rien ne la relançait. Fire joue maintenant sans arrêter
  les autres montages, et tant que la visée dure, la pose revient dès que son slot est libre (vérifié au plus
  toutes les 0,3 s). Le journal du joueur écrit `weapon: aim pose … playing` au départ, puis toutes les 2 s arme
  sortie `weapon: hand probe (aim|hip)` : slot actif, poids du montage, et position des mains par rapport à la tête
  (`hand_r` : hauteur, avancée, côté ; `hand_l` : hauteur) sur le pion et sur le corps visible, puis
  `weapon: frame probe` : hauteur des deux têtes au-dessus du centre de la capsule et place de la caméra (hauteur,
  recul, côté, tangage, champ de vision) : une main levée bien
  devant la tête, c'est une visée ; une main très basse et à l'aplomb du corps, non. Mesuré sur le candidat 28b : la
  pose joue sur le pion (main 12 cm sous la tête, 43 cm devant) mais pas sur le corps MetaHuman (53 cm sous la tête,
  3 cm en arrière) : le retarget à l'exécution ne la transmet pas, cause en cours de recherche :
  `tools/combat/unreal/player_visual_retarget.py` vide les retargeters utilisés (ops, correspondance des chaînes,
  réglages des bras, buts IK du rig MetaHuman) sans les modifier, et construit notre `RTG_GTP_UEFN_to_MetaHuman` à
  essayer avec `-PlayerVisualRetargeter=`.
- **Qui est mis en joue (correctif de la nuit du 13/11)** : le joueur à pied signale la menace depuis ses yeux vers
  ce que couvre le réticule (la ligne de la vue jusqu'au premier obstacle), comme il tire depuis la vue, et non plus
  depuis la bouche du canon. Celle-ci est dans la main du corps visible : main basse à la hanche, un piéton à 2 m
  sortait du cône de 12 degrés et personne ne réagissait (étape braquage de `-GTARun` en échec à 226 cm, réussie à
  498 cm). Le tir en voiture garde la bouche du canon. L'étape braquage de `-GTARun` place la vue sur le point que
  teste la peur du piéton, comme un joueur met le réticule sur quelqu'un.
