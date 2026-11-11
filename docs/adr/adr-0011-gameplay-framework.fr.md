# ADR-0011 : Framework gameplay du combat

*Version anglaise : [adr-0011-gameplay-framework.md](adr-0011-gameplay-framework.md)*

## Statut

Proposé le 2026-11-11. Règles vérifiées hors Unreal ; la partie Unreal n'est pas encore
écrite. Accepté quand la première tranche Unreal (santé, pistolet, impact) passe la
porte de fusion.

## Date

2026-11-11

## Décideurs

nic0lasm (responsable du projet)

## Résumé

Pas de Gameplay Ability System. Le combat tient en quelques composants simples posés sur
des en-têtes de règles sans types Unreal (`GTPHealthLogic.h`, `GTPDamageLogic.h`,
`GTPWeaponLogic.h`, `GTPCombatMeleeLogic.h`, `GTPCoverLogic.h`), vérifiés par
`tools/combat/combat_logic_test.cpp` en CI, comme les étoiles, le ragdoll et l'audio. Un
sous-système de monde est l'unique point d'entrée de chaque coup et diffuse ce que les
autres systèmes écoutent. Pas de StateTree en 0.7 ; c'est le candidat pour les groupes
d'ennemis à partir de la 0.11.

## Contexte

La 0.7 (« Armé ») demande le combat à mains nues, un pistolet, la couverture, santé et
armure, mort et réapparition, réactions localisées, impacts par surface, des gens qui
fuient les armes et une police qui tire à deux étoiles. La 0.8 a besoin d'une menace armée
pour le braquage (le fast-food de Bellecour). Plusieurs systèmes existent déjà et
possèdent une part de la réponse :

- Fil Ragdoll : ragdoll et relevés (`UGTPRagdollComponent`, ADR-0006), réactions
  localisées (`UGTPHitReactionComponent`, `GTPHitLogic.h`, branche feat/hit-reactions) et
  une seule peur pour tout le jeu (`UGTPThreatSubsystem`, `UGTPThreatResponseComponent`,
  branche feat/threat-response). Leur contrat : le combat leur **signale** les visées, les
  tirs et les coups déjà résolus ; eux décident de la réaction.
- Police (ADR-0004) : `UGTPWantedComponent::ReportCrime`, `EGTPCrimeKind`.
- Piétons (ADR-0005) : corps en pool, `OnPedestrianHit` / `OnPedestrianScared`.
- Intérieurs (ADR-0012, branche feat/reference-interior) : script de braquage, caisses,
  alarme.
- Budget (ADR-0002) : M1 Pro plafonné à 40 i/s, images à 25,0 ms, GPU p95 ≤ 24 ms avec
  toute la rue ; « nuit et effets » dispose de 2 ms GPU pour tout.

Le jeu est solo, sur Mac M1 Pro et PC Windows, avec une seule arme en 0.7 et un arsenal
seulement en 0.11.

## Décision

1. **Règles sans types Unreal**, un en-tête par sujet, tous les nombres dans des
   structures de réglages que les composants remplissent depuis `DefaultGame.ini` :
   - `GTPHealthLogic.h` : santé, armure qui prend 70 % d'un coup tant qu'elle dure,
     régénération jusqu'à mi-santé après 6 s sans dégât, endurance, minuterie de mort,
     grâce à la réapparition, hôpital le plus proche.
   - `GTPDamageLogic.h` : zone d'après le nom de l'os, facteurs par zone, baisse avec la
     distance, le joueur prend 45 % des dégâts des PNJ et jamais plus de 1,5 × à la tête,
     type d'impact par surface (identifiants partagés avec la carte des pas
     `GTPSurfaceLogic.h`) et plafond de particules par type d'impact.
   - `GTPWeaponLogic.h` : chargeur et réserve, cadence semi-automatique, rechargement qui
     ne remplit qu'à la fin et peut être interrompu, dispersion (hanche, visée, mouvement,
     échauffement, tir à l'aveugle × 4), recul qui revient, tirage aléatoire à graine (un
     combat rejoué est identique), et aide à la visée à la manette (accroche au début de
     la visée, friction).
   - `GTPCombatMeleeLogic.h` : machine à états d'un combattant (armé, frappe,
     récupération, enchaînement de trois coups dont le troisième lourd, esquive
     invulnérable en son milieu, garde qui coûte de l'endurance et cède, étourdissement)
     et règle de neutralisation.
   - `GTPCoverLogic.h` : couverture basse et haute d'après trois traces, bords, façon de
     viser depuis la couverture, abri suivant dans la direction du stick, protection d'un
     corps à couvert face à un tireur donné (pour les décisions des PNJ).
2. **Composants Unreal**, minces, possédés par ce fil :
   - `UGTPHealthComponent` sur le joueur, les policiers et les employés. C'est **la**
     santé d'un corps. Les piétons du pool gardent une simple valeur de santé dans leur
     fiche de pool (pas de composant par corps).
   - `UGTPWeaponComponent` : tient un `GTPWeapon::FState`, trace le tir, utilisé de la même
     façon par le joueur et l'IA de police (l'IA passe son propre facteur de précision).
   - `UGTPCombatMeleeComponent` : un `GTPMelee::FFighter`, un balayage sphérique à
     l'instant de la frappe, montages par nom.
   - `UGTPCoverComponent` (joueur seulement en 0.7) : lance les traces à l'appui de la
     touche de couverture, jamais à chaque image.
   - `UGTPCombatSubsystem` (monde) : **point d'entrée unique** `ApplyHit(FGTPCombatHit)`
     qui résout les dégâts, les applique à la santé, appelle
     `UGTPHitReactionComponent::ReactToHit`, prévient `UGTPThreatSubsystem` (`SetThreat`
     tant qu'un corps armé vise, `NotifyShot` à chaque tir), fait apparaître l'effet
     d'impact depuis une réserve plafonnée et diffuse les délégués `OnShotFired`,
     `OnDamageDealt`, `OnPawnKilled`, `OnPlayerDied`, `OnPlayerRespawned`. Il fait avancer
     la santé de tous les corps inscrits à 10 Hz. Il gère la mort et la réapparition à
     l'hôpital fictif le plus proche (`ATargetPoint` marqué `GTPHospital`).
     L'interrupteur `-Combat=0` coupe le tout.
3. **Animation** : on reste sur le personnage GASP : motion matching pour les jambes,
   couche haut du corps pour le pistolet (aim offset, recul additif), emplacements de
   montages pour coups, esquives, rechargements et neutralisations, jeu de poses en boucle
   pour la couverture. Clips recensés dans `docs/animation/combat-sources.fr.md` ; les
   manquants sont posés à la main par la méthode de `tools/animation/`.
4. **Frontières** (fichiers possédés ailleurs, jamais réécrits ici) :
   - Ragdoll : réactions, ragdoll, peur. Le combat les appelle. Les dégâts de voiture et
     de chute du joueur (`UGTPPlayerReactionComponent`) passent par `UGTPHealthComponent`
     quand il est présent : petit ajout proposé au fil Ragdoll.
   - Police : s'abonne aux délégués et appelle son propre `ReportCrime`. Nouveaux délits
     proposés au fil Police : `Assault`, `ShotsFired`, `Killing`, `KillPolice`,
     `ArmedThreat`. L'IA de tir de la police utilise `UGTPWeaponComponent`.
   - Piétons : lisent la peur via `UGTPThreatResponseComponent` / `OnThreatState` ; les
     corps du pool reçoivent les coups par `ApplyHit` avec leur santé de pool.
   - Caméra et sensation à pied (`GTPFollowCamera*`, `GTPCameraMode*`) : caméra de visée,
     changement d'épaule et caméra de couverture spécifiés ici, proposés au propriétaire.
   - HUD (réticule, santé, armure, munitions, marqueur de touche) : spécifié ici, dessiné
     par le fil Interface. Actions d'entrée : créées ici, remappage par le fil Interface.
   - Sons : spécification au fil Audio, qui les fabrique et les mixe.
   - Config moteur, cvars, budgets : proposés au fil Optimisation.

## Alternatives envisagées

- **Gameplay Ability System** (comme Lyra). Fort pour beaucoup de capacités, les tags,
  effets et cues, et la prédiction réseau. Contre, ici : le projet est solo, n'a qu'une
  arme jusqu'à la 0.11 et vérifie ses règles hors Unreal ; GAS ajoute trois modules, un
  composant de capacités par corps (coût sur les piétons du pool et la police), des
  attributs non testables sans le moteur, et une boucle d'apprentissage et de compilation
  longue sur le Mac. Les en-têtes de règles restent utilisables dans des capacités GAS si
  l'arsenal de la 0.11 le demande ; la question se rouvre alors, pas avant.
- **Dégâts natifs d'Unreal** (`TakeDamage`, `ApplyPointDamage`) : hors du chemin principal.
  Une seule entrée (`ApplyHit`) se mesure, se coupe et se rejoue plus facilement. Les
  acteurs qui ne connaissent que `TakeDamage` (cassables) peuvent le recevoir du
  sous-système.
- **StateTree pour le combat de la police dès maintenant.** La décision de la police en
  0.7 (tirer, se couvrir, approcher) est assez petite pour des règles dans le fil Police ;
  StateTree (greffon non activé aujourd'hui) est candidat pour les groupes d'ennemis en
  0.11.
- **Animation physique pour les réactions** (ragdoll partiel) : plus beau, mais coût CPU
  sur le M1 avec plusieurs corps ; d'abord les clips de sursaut et de vacillement du fil
  Ragdoll, l'animation physique sera une option du profil « fidelity » plus tard.

## Conséquences

- Le code Unreal du combat dépend de feat/hit-reactions et feat/threat-response : elles
  arrivent sur next d'abord, puis le combat s'appuie dessus.
- Deux valeurs de santé coexistent jusqu'à ce que le fil Ragdoll branche
  `UGTPPlayerReactionComponent` sur `UGTPHealthComponent`.
- Les nombres (dégâts, temps pour abattre) sont fixés à un seul endroit et leur
  équilibre est vérifié par le test : un policier tombe en 5 balles au torse ou 2 à la
  tête à 10 m ; le joueur encaisse 12 de ces balles sans armure et 23 avec une armure
  pleine ; un enchaînement complet de poings ne tue pas.
- Les effets ont un plafond dur (voir le budget dans `docs/gameplay/combat.fr.md`) ;
  plafond atteint, l'impact le plus ancien disparaît, le jeu n'attend jamais un effet.

## Critères de validation

- `tools/combat/combat_logic_test.cpp` passe en CI (tâche `Combat rules`), et les
  en-têtes passent les avertissements stricts de conversion (MSVC est plus strict que
  g++).
- Compilation de contrôle sur le PC Windows (éditeur et jeu, Win64) de chaque branche.
- Banc M1, palier C plafonné à 40 i/s, 4 scènes × 120 s plus une scène de fusillade de
  2 min : ≥ 99 % des images à 25,0 ms ± 1, aucune image > 100 ms, GPU p95 ≤ 24 ms ; A/B
  `-Combat=0` dans la ligne de budget du combat.
- Vidéo d'un combat de 2 min (poings, pistolet, couverture, mort, hôpital).

## Liens

ADR-0002 (budget), ADR-0003 (déplacement), ADR-0004 (étoiles), ADR-0005 (piétons),
ADR-0006 (ragdoll et réactions), ADR-0009 (mixage audio), ADR-0012 (intérieurs),
docs/gameplay/combat.fr.md, docs/animation/combat-sources.fr.md.
