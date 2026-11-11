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
| `OnShotFired` | tireur, origine, direction, arme, acteur touché (peut être nul) | Police (`ShotsFired`), Audio (détonation, queue, écho par rue), Piétons s'il leur faut plus que la peur |
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
| Changer d'épaule | X | clic du stick droit |
| Ranger, dégainer | 1 | croix haut |

À vérifier par le fil Interface contre les actions existantes (`F` monte en voiture, `E` interagit, `Espace`
saute).

## Budget des effets (proposé au fil Optimisation)

Ligne dans l'enveloppe « nuit et effets » (2 ms GPU), pour le pire cas d'une fusillade dans la rue au
palier C, profil M1, plafonné à 40 i/s :

| Élément | Plafond sur M1 | Coût visé |
| --- | --- | --- |
| Effets d'impact (Niagara, simulation CPU, sans lumière, sans collision) | 12 vivants à la fois, 0,6 s de vie, particules par type 8 à 48 (`GTPDamage::MaxParticles`) | ≤ 0,25 ms GPU p95 |
| Décalques d'impact (réserve, sans mélange de normales) | 32 dans le monde, le plus ancien recyclé, fondu 20 s | ≤ 0,05 ms GPU |
| Flamme de bouche | sprite + lumière de 0,05 s **sans ombres**, joueur seulement ; tirs des PNJ : sprite seul | ≤ 0,05 ms GPU |
| Fumée | une petite bouffée par tir (≤ 6 particules, peu de surdessin), aucune sous la pluie | compris ci-dessus |
| Traçantes | coupées sur M1, actives en « fidelity » | 0 sur M1 |
| Vitrines qui cassent | cassables existants (budget du fil Ragdoll) | non compté ici |
| Fil de jeu (traces, santé à 10 Hz, aide à la visée) | 3 tireurs, 1 trace par tir, traces de couverture seulement à l'appui | ≤ 0,4 ms CPU |

Total proposé : **≤ 0,35 ms GPU p95 et ≤ 0,4 ms CPU du fil de jeu** dans la scène de fusillade, en A/B
contre `-Combat=0`. Plafond atteint : l'effet le plus ancien disparaît, rien n'attend. Un profil
« fidelity » peut doubler les plafonds et activer les traçantes.

## Plan

1. Règles hors Unreal (fait, cette branche).
2. Quand feat/hit-reactions et feat/threat-response sont sur next : `UGTPHealthComponent`,
   `UGTPCombatSubsystem` (`ApplyHit`, délégués, `-Combat=0`), `UGTPWeaponComponent` avec le pistolet et les
   effets d'impact, réapparition à l'hôpital. Compilation de contrôle sur le PC, puis créneau Mac (porte +
   scène de fusillade).
3. Poings, esquive, garde, neutralisation (`UGTPCombatMeleeComponent`).
4. Couverture (`UGTPCoverComponent`), tir à l'aveugle.
5. Police qui tire à deux étoiles (avec le fil Police), vidéo d'un combat de 2 min.
