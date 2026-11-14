# Personnage jouable : intégration du Game Animation Sample

*Version anglaise : [player-character.md](player-character.md).*
*Version 0.1 « Jouer en un clic », tâches 3 et 4 du [tableau de bord](../../production/roadmap-status.fr.md).*
*Dernière mise à jour : 2026-11-09. Statut : intégré et testé sans interface dans `L_RueRepublique` (troisième et première personne) ; vérification manette à la main et test d'enjambement à faire.*

## 1. Objectif

En 0.1, on contrôle un personnage dans la rue de la République en greybox :
marcher, courir, sauter, s'accroupir, avec une caméra **façon GTA** :
troisième personne classique par défaut et **bascule en première personne**
(décision de Nico, 2026-11-09). Bodycam reste une référence de qualité
graphique uniquement : pas d'effet caméra corporelle.

On ne réinvente rien : la locomotion vient du **Game Animation Sample**
d'Epic (GASP, gratuit sur Fab, mis à jour pour la 5.8), branchée sur nos
propres entrées et notre caméra.

## 2. Ce que fournit le GASP

- Personnage jouable en **motion matching** (Pose Search) avec plus de
  500 animations : marche, course, sprint, sauts, accroupi, arrêts et
  pivots, franchissement (enjamber, grimper, monter sur un obstacle).
- `CharacterMovementComponent` (voie stable ; Mover reste à tester plus
  tard, voir le [recensement](../sourcing/asset-sourcing.fr.md)).
- Mannequins UE5 et UEFN, entrées Enhanced Input, caméra troisième personne.

Plugins à activer **avant** toute migration (liste à confronter au
`.uproject` du GASP 5.8, qui fait foi) : Pose Search, Motion Trajectory,
Motion Warping, Animation Warping, Chooser, Animation Locomotion Library,
Deformer Graph, Gameplay Cameras, Mover, Live Link Control Rig.
Il faut aussi reprendre du GASP : les gameplay tags
(`Config/DefaultGameplayTags.ini`), les canaux de collision et les CVars
de projet qu'il déclare.

### Relevé du GASP 5.8 sur le Mac (2026-11-09)

Projet téléchargé dans `Samples/GameAnimationSample` (7,0 Go, 3 890 `.uasset`,
5 niveaux), 100 % Blueprint.

- **Plugins du `.uproject`** : AnimationWarping, RigLogic, LiveLink,
  LiveLinkControlRig, PoseSearch, AnimationLocomotionLibrary, MotionWarping,
  HairStrands, Chooser, Mover, NetworkPrediction, ChaosMover,
  AnimationLayering, MoverExamples, MovieSceneAnimMixer, DrawDebugLibrary,
  SmartObjects, Locomotor, CurveExpression, GameplayInteractions. On les
  active tous, même si notre personnage utilise le
  `CharacterMovementComponent` : les Blueprints du sample y font référence.
  GameplayCameras n'y figure pas mais la caméra s'en sert (activé par
  dépendance, à vérifier).
- **Canaux de collision à recréer avant la copie**, aux mêmes numéros :
  `GameTraceChannel1` Traversable (trace), `GameTraceChannel2` Mouse
  (trace), `GameTraceChannel3` Obstacle (objet), et les profils
  `TraversalObjectPreset`, `CharacterCapsule`, `ObstaclePreset`. Sans eux,
  le franchissement ne détecte rien. Aucun canal personnalisé n'est encore
  pris dans notre projet.
- **Gameplay tags** : copier `DefaultGameplayTags.ini` tel quel (tags
  `MotionMatching.*`, `Foley.*`, `SmartObject.*`, redirections).
- **CVars pilotées par les données** (`DataDrivenConsoleVariableSettings`) :
  les reprendre toutes. On choisit le personnage `SandboxCharacter_CMC`
  (voie stable) ; `DDCVar.NewGameplayCameraSystem.Enable=True` garde la
  nouvelle caméra.
- **Physique** : substepping activé (pas max 16,7 ms, 16 sous-pas), à
  reprendre.
- **Entrées** : `Content/Input/IMC_Sandbox` et 18 actions `IA_*` du sample.
  Le sample garde son chemin `/Game/Input` ; nos propres actions et
  contextes vivent dans `/Game/Input/GTP`.
- **Caméra** : Gameplay Camera System (`CameraAsset_SandboxCharacter`,
  `CameraDirector_SandboxCharacter`, rigs Close, Medium et Far), gardé tel
  quel pour la troisième personne.
- **Contenu réellement nécessaire** (fermeture du registre d'assets,
  références dures et souples, depuis `SandboxCharacter_CMC`, son
  Blueprint d'animation, `GM_Sandbox`, `PC_Sandbox`, `/Game/Input` et les
  assets de caméra) : 2 039 paquets, 2,0 Go. `Characters/UEFN_Mannequin`
  (1,97 Go), `Levels/LevelPrototyping` (38 Mo, référence dure de
  `AC_TraversalLogic` vers `LevelBlock_Traversable`), `Audio` (12 Mo),
  `Blueprints` (31 Mo), `Input`, `Misc`. La fermeture brute fait 4,7 Go
  car `GM_Sandbox` référence en souple les personnages alternatifs du
  sample (Echo, Twinblast, mannequin UE4, MetaHuman Kellan) ; ces
  références souples ne sont pas suivies, notre mode de jeu ne s'en sert
  pas. **Non copiés** : `Characters/Paragon`, `Characters/Echo`,
  `Characters/UE4_Mannequin`, `Characters/UE5_Mannequins`, `MetaHumans`,
  `IsolatedExamples`, `Movies`, les niveaux du sample, `RetargetedCharacters`.

## 3. Licence et Git : le contenu Epic ne va pas dans le repo public

Le repo est public sous licence MIT. Les assets du GASP (`.uasset`) sont
du contenu Epic soumis à l'Epic Content License : on peut les utiliser et
les livrer **intégrés au jeu empaqueté**, mais pas les redistribuer en
source au grand public. Le code, les fichiers `.ini`, le `.uproject` et nos
propres créations peuvent, eux, être publics.

Règle décidée par Nico le 2026-11-09 : le contenu Epic et Fab reste hors du repo public, en local.

| Contenu | Où | Dans Git ? |
| ---- | ---- | ---- |
| Assets du GASP (personnages, animations, bases de données de poses, Blueprints d'origine) | Mêmes chemins `/Game` que dans le sample : `Content/Audio`, `Content/Blueprints`, `Content/Characters/UEFN_Mannequin`, `Content/Input` (sauf `GTP`), `Content/Levels`, `Content/Misc` | **Non** (`.gitignore`), réinstallés par `tools/character/install_gasp_content.sh` |
| Notre Blueprint enfant | `Content/Characters/Player/` | Oui (LFS) ; reconstructible par `tools/character/create_player_assets.py` |
| Nos actions et contextes d'entrée | `Content/Input/GTP/` | Oui (LFS), même script |
| Code C++ (mode de caméra, contrôleur), config, scripts | `Source/GranTheftPraline/`, `Config/`, `tools/character/` | Oui |

Le sample garde ses propres chemins `/Game` (décision du 2026-11-09, à la
place d'un plugin de contenu) : aucune redirection à corriger, le projet
du sample reste intact, et une version plus récente s'installe en
relançant le script.

## 4. Caméra façon GTA

| Mode | Réglage de départ | Commentaire |
| ---- | ---- | ---- |
| **Troisième personne** (défaut) | Bras de caméra ~300 cm à pied, décalage épaule droite ~40 cm, hauteur ~60 cm au-dessus du bassin, champ de vision 75°, collision de caméra avec les murs, retour automatique derrière le personnage après ~2 s sans toucher la caméra (désactivable) | Rues étroites de la Presqu'île : la collision de caméra est indispensable |
| **Première personne** | Caméra attachée à la tête, champ de vision 90° (réglable 70 à 110°), tête masquée pour le joueur, corps et ombre visibles, mêmes animations que la troisième personne | Utiliser le rendu première personne du moteur (champ de vision et échelle séparés pour le corps) s'il est disponible en 5.8, à vérifier dans l'éditeur |
| **Bascule** | Touche `V` / bouton Vue de la manette, transition rapide (~0,2 s) | Le mode choisi est conservé dans les options |

Mise en œuvre (choisie sur mesure, 2026-11-09) : `UGTPCameraModeComponent`,
un composant C++ porté par notre contrôleur de joueur (le mode survit donc
à une réapparition).
- Troisième personne : depuis la 0.2, notre caméra de suivi
  `UGTPFollowCameraComponent` (retard, épaule, collision, champ de vision en
  sprint, recentrage ; voir [on-foot-feel.fr.md](on-foot-feel.fr.md)). Avec
  `bEnabled=False`, retour à la caméra de jeu du GASP portée par le pion
  (champ de vision 80° dans le réglage du sample).
- Première personne : la cible de vue passe (0,2 s, courbe cubique) à une
  caméra placée sur l'os `head` plus un petit décalage (12 cm devant, 6 cm
  au-dessus), tournée avec la rotation de contrôle, champ de vision 90°.
  L'os `head` est masqué sur le maillage : le corps (bras, jambes) reste
  visible. L'état d'entrée `WantsToStrafe` du personnage est activé en
  première personne pour que le corps suive la caméra.
- Pourquoi pas un rig dans le directeur de caméra du GASP : le directeur
  et ses rigs sont des assets Epic (hors Git) à modifier dans l'éditeur ;
  le changement de cible de vue marche avec le Gameplay Camera System
  intact et reste entièrement dans notre code.
- Limite connue : masquer l'os retire aussi la tête de l'ombre. Le rendu
  première personne du moteur (champ de vision et échelle séparés pour le
  corps) reste à évaluer.

## 5. Entrées (Enhanced Input)

Contextes dans `/Game/Input/GTP` : `IMC_OnFoot_KBM_AZERTY` (par défaut),
`IMC_OnFoot_KBM_QWERTY` (`-Qwerty` en ligne de commande ou
`bQwertyKeyboard=True` dans `DefaultGame.ini`) et `IMC_OnFoot_Gamepad`.
`AGTPPlayerController` retire `IMC_Sandbox` du sample après chaque prise
de contrôle du pion et ajoute les nôtres. Échap et le bouton Menu de la
manette sont réservés au menu pause, géré par le code du jeu. Remappage
complet prévu en 0.12.

Lettres : Unreal lit le caractère tapé, donc sur un clavier AZERTY la
touche marquée Z arrive comme `Z` ; ZQSD est la disposition AZERTY de WASD.

| Action | Type | Clavier AZERTY | Clavier QWERTY | Manette |
| ---- | ---- | ---- | ---- | ---- |
| `IA_Move` (sample) | Axe 2D | Z Q S D | W A S D | Stick gauche |
| `IA_Look` / `IA_Look_Gamepad` (sample) | Axe 2D | Souris | Souris | Stick droit |
| `IA_Sprint` (sample) | Maintien | Maj gauche | Maj gauche | A / Croix (maintien) |
| `IA_Walk` (sample) | Bascule | Alt gauche | Alt gauche | Clic stick gauche |
| `IA_Jump` et `IA_Traverse` (sample) | Appui | Espace | Espace | X / Carré |
| `IA_Crouch` (sample) | Bascule | C | C | B / Rond |
| `IA_ToggleCameraMode` (le nôtre) | Appui | V | V | Bouton Vue / Share |
| `IA_CameraRecenter` (le nôtre, construit en code) | Appui | clic molette | clic molette | aucun (clic stick droit déjà pris) |
| `IA_Interact` (le nôtre, construit en code) | Appui | E | E | croix droite |

Modificateurs repris de `IMC_Sandbox` : inversion et permutation d'axes sur
les touches de déplacement, axe Y de la souris inversé par un facteur, zone
morte radiale 0,3 sur le stick gauche, zone morte 0,2, courbe exponentielle
et facteur 90/70 sur le stick droit. Les réglages de sensibilité et
d'inversion arrivent avec le menu d'options. Les noms d'actions sont gardés
pour les versions suivantes (`IA_Aim`, `IA_Fire`, `IA_EnterVehicle`
arrivent en 0.3 et 0.4).

## 6. Étapes d'intégration

1. Récupérer le GASP 5.8 sur le Mac : launcher Epic, onglet Fab,
   « Game Animation Sample », créer le projet dans
   `Samples/GameAnimationSample`, à côté du repo (hors repo).
2. `tools/character/install_gasp_content.sh` : liste les paquets
   nécessaires par une exécution sans interface et en lecture seule dans le
   sample (`gasp_dependency_closure.py`) et les copie dans
   `GranTheftPraline/Content` aux mêmes chemins.
3. Config (dans Git) : plugins du sample et GameplayCameras dans le
   `.uproject` ; canaux et profils de collision, CVars pilotées par les
   données, substepping physique et réglages Pose Search dans
   `DefaultEngine.ini` ; `DefaultGameplayTags.ini` ; `PlayerPawnClass`, la
   section de notre contrôleur et `DirectoriesToAlwaysCook` dans
   `DefaultGame.ini`. Les réglages de rendu du sample ne sont pas repris
   (les nôtres suivent les ADR).
4. Compiler l'éditeur, puis `tools/character/create_player_assets.py`
   (sans interface) : `BP_PlayerCharacter`, `IA_ToggleCameraMode` et les
   trois contextes d'entrée.
5. `tools/character/playtest_character.sh` : joue `L_RueRepublique` en
   mode `-game` et vérifie course, marche, sprint, saut, accroupi, la
   bascule première personne et une réapparition en première personne,
   avec captures et temps d'image (§9).

## 7. Critère « fait quand »

- Marcher, courir, sprinter, sauter, s'accroupir et enjamber au
  clavier-souris et à la manette, dans `L_RueRepublique`.
- Caméra troisième personne qui ne traverse pas les murs ; bascule
  première personne sans saut d'image ni tête visible.
- Preuves : vidéo de 2 minutes et captures des deux modes, rangées avec
  les preuves de la 0.1 ; aucun asset Epic suivi par Git (contrôle
  `git ls-files GranTheftPraline/Content`).

## 8. Risques

| Risque | Parade |
| ---- | ---- |
| Migration du GASP incomplète (caméra qui ne démarre pas, tags manquants), problèmes déjà signalés par la communauté | Plugins activés avant la copie, tags et CVars repris, test immédiat dans une carte vide |
| Coût du motion matching sur le M1 Pro | Mesure dès l'intégration ; budget exigé seulement à partir de la 0.2 |
| Première personne : clipping du corps, mal des transports | Rendu première personne du moteur, champ de vision réglable, pas de balancement de tête par défaut |
| Contenu Epic poussé par erreur | `.gitignore` sur chaque dossier copié du sample et vérification avant chaque push |

## 9. Résultats de l'intégration (2026-11-09, M1 Pro, build éditeur en mode `-game`)

`tools/character/playtest_character.sh`, environ 50 s de jeu scripté dans
`L_RueRepublique` en 1920x1080, tous les contrôles passent :

| Contrôle | Mesure |
| ---- | ---- |
| Apparition | `BP_PlayerCharacter` au PlayerStart (1212, -15665) |
| Course (par défaut) | 500 cm/s |
| Marche (bascule) | 200 cm/s |
| Sprint (maintien) | 700 cm/s |
| Saut | en l'air, 127 cm de hauteur |
| Accroupi (bascule) | accroupi, 225 cm/s |
| Première personne | cible de vue sur la caméra, champ de vision 90°, tête masquée, corps visible en regardant vers le bas |
| Réapparition en première personne | nouveau pion, toujours en première personne, tête masquée |
| Retour en troisième personne | vue du pion (caméra du GASP), tête affichée |

Temps d'image sur le parcours (hors chauffe) : image p50 45 ms, p95 56 ms ;
thread de jeu p50 14 à 19 ms ; GPU p50 45 ms, p95 55 ms. Le parcours est
limité par le GPU, en accord avec la mesure de la scène de référence (GPU
p95 de 43 à 51 ms avant les profils de rendu) ; le coût du motion matching
sur le thread de jeu sera budgété en 0.2. Aucune erreur de nos assets dans
le journal. L'enjambement (`IA_Traverse`) et une vraie manette restent à
vérifier à la main.

Contrôle de cuisson : `-run=cook -targetplatform=Mac -map=/Game/Maps/L_RueRepublique`
réussit en 58 s, 2 899 paquets, 0 erreur, 0 avertissement ; notre
Blueprint de joueur, nos actions et contextes d'entrée sont dans le
registre d'assets cuit.

## 10. Nicolito, le visage du joueur (décision du 13/11/2026)

Le joueur est un homme blanc fictif nommé Nicolito (pas la ressemblance d'une personne réelle). Le responsable du
projet a écarté le MetaHuman Kellan du Game Animation Sample et choisi, parmi trois voies gratuites, un visage de
la foule du City Sample (personnages dérivés de MetaHuman, déjà sur le PC avec le City Sample complet). Kellan
tient le rôle d'ici là ; le remplacement se fait par `PlayerVisualClass` et son dossier de cuisson dans
`DefaultGame.ini`, puisque le corps suit le pion par le retarget à l'exécution et que les clips d'arme jouent sur
le pion.

Étapes, chacune par la file du PC : `tools/character/citysample_crowd_survey.py` (Python simple, lecture seule)
liste les dossiers de la foule et leur poids, les têtes, corps, tenues et coiffures par nom, et les paquets qui
exigent les classes C++ propres au sample ; puis trois visages d'hommes en capture, parmi lesquels le responsable
du projet choisit ; puis la tête, le corps, la tenue et la coiffure retenus assemblés dans le blueprint de
Nicolito, dans notre projet.

**Portraits (14/11/2026).** `-NicolitoFaces` (`UGTPFaceLineupSubsystem`, rien sans l'interrupteur) filme chaque
tête d'homme de corpulence normale du pack trouvée sous `/Game/Crowd/Character/Male` (registre d'assets,
`*_nrw_FaceMesh`, au plus `MaxHeads`) : 10 s après le départ, chaque tête tour à tour sur le corps d'homme de
corpulence normale de la foule (pull ras du cou, jean, mocassins), à 4 m devant le joueur et tournée vers lui,
visage à son niveau de détail le plus fin, cadrée tête et épaules par une caméra à elle (`CameraDistanceCm` 100,
`CameraFOV` 30), puis enregistrée en `Nicolito_<tête>.png` dans le dossier des captures (`Saved/Screenshots/...`) ;
la vue du joueur revient à la fin. Le sens du regard est mesuré dans la pose de référence du corps (de la clavicule
gauche à la droite), pas supposé, et écrit au journal. Ce sont les têtes nues du pack, sans coiffure (l'assemblage
de la foule n'en a pas) et en pose de référence ; la coiffure vient avec le blueprint, une fois le visage choisi.
Lancement dans l'éditeur en mode jeu, les assets du pack n'étant pas cuits : `UnrealEditor GranTheftPraline.uproject
-game -NicolitoFaces -SkipTitle -DayNight=0 -Pedestrians=0 -Police=0` ; le journal dit `face lineup: ...` pour
chaque tête et chaque image.

Premier passage sur le PC (14/11/2026, 6 têtes, m_001 à m_006 : City Sample n'a pas m_007 à m_012) : aucune
erreur, mais les images étaient la vue du jeu, Kellan de dos et la tête large de quelques pixels à 5 m. La caméra
de suivi du jeu reprend la vue à chaque image quand ce n'est pas la sienne (`UGTPFollowCameraComponent`). Le banc
la met maintenant en pause, cache le joueur (le pion et le visuel qui lui est attaché) et le HUD le temps des
portraits, remet tout à la fin, et écrit pour chaque image la cible de vue au moment de la prise (`ours` ou
`NOT OURS`) avec la distance et le FOV de la caméra. Mesuré lors de ce passage : le corps d'homme de corpulence
normale du pack regarde vers +Y dans son maillage, comme le mannequin.

**Assemblage (prêt, attend le choix).** `tools/character/create_nicolito_visual.py` (éditeur, module de jeu compilé ;
`GTP_NICOLITO_HEAD=m_00N`) copie BP_Kellan, jamais modifié, en `/Game/MetaHumans/GTP/Nicolito/BP_Nicolito` et pose
la tête choisie sur son composant `Face`, sans les surcharges de matériaux de Kellan ; le corps, la tenue, le
retarget et la gestion des grooms de Kellan restent, ses grooms sont vidés (liés à son visage ; le mode de jeu cache
un groom sans asset), puis ceux nommés dans `GTP_NICOLITO_GROOMS` (par défaut `Eyebrows`) sont remis avec une nouvelle
liaison au visage choisi (`/Game/MetaHumans/GTP/Nicolito/Grooms`, `GroomBlueprintLibrary` de HairStrands, transférée
depuis le visage pour lequel le groom a été fait : les visages MetaHuman ont tous la même topologie). `GTP_NICOLITO_MODE=full` pose aussi le corps d'homme de corpulence normale et la tenue de la
foule (`BP_Nicolito_Full`), enregistré seulement si ce corps a le squelette du blueprint de retarget du corps. Le
visage garde l'animation faciale de Kellan seulement si la nouvelle tête a le squelette pour lequel elle a été faite ;
sinon le mode de jeu fait suivre au visage la tête et le cou du corps (journal : `Face has no animation of its own`).
Côté C++ : `UGTPAnimGraphTools::DescribeBlueprintComponents` et `SetBlueprintComponentAsset` (gabarits du script de
construction, modifiés comme le fait le panneau de détails). Rapport dans `Saved/GTPCombat/nicolito-visual.txt` ;
essai avec `-PlayerVisual=/Game/MetaHumans/GTP/Nicolito/BP_Nicolito.BP_Nicolito_C`, puis `PlayerVisualClass` une fois
qu'une capture le montre juste. Nicolito est chauve jusqu'au choix de sa coiffure, et le cou dira si la peau du corps
de Kellan va avec le nouveau visage (sinon `full`). La capture se fait avec `-NicolitoPortrait` (même banc, sur le
joueur tel qu'il est en jeu, laissé visible) : `Nicolito_player_face.png` (du menton au crâne, 120 cm, FOV 30) et
`Nicolito_player_body.png` (des pieds au crâne, de face, `PlayerBodyDistanceCm` 380, `PlayerBodyFOV` 50).

Le responsable du projet a choisi la tête **m_004** (14/11/2026, sur les six portraits).

Premier passage du script sur le PC (14/11/2026, m_004) : enregistré. La tête de la foule a son propre
`Face_Archetype_Skeleton` (`/Game/Crowd/Character/Shared/Rig`), un autre asset que celui des MetaHumans : le visage
suit donc le corps, sans animation faciale. Tous les grooms ont été vidés, sourcils compris, d'où leur remise par
`GTP_NICOLITO_GROOMS`. Sa capture a montré : des mains bien plus foncées que le visage (la peau du corps de Kellan sous
la tête m_004), le devant du sweat de Kellan qui pend jusqu'aux genoux comme une cape, le menton coupé sur le plan
du visage et la tête coupée sur le plan en pied (cadrage corrigé plus haut). Passage suivant en mode `full` : le
corps et la tenue de la foule, faits pour ces têtes ; le blueprint de retarget du corps (`ABP_GenericRetarget`) est
un gabarit fait pour aucun squelette, ce que la vérification accepte maintenant. Le rapport liste les paramètres
des matériaux de peau du visage et du corps, pour les accorder si les teintes diffèrent encore.

Passage `full` (14/11/2026) : enregistré, cadrage juste sur les deux plans, pas de couture au cou, mains et visage de
la même teinte, mais cette teinte est brun foncé, un pan sombre pend de la taille entre les jambes, et les sourcils
n'ont pas été reliés (`GroomBlueprintLibrary.create_new_groom_binding_asset_with_path` n'est pas exposé à Python). La
peau synthétisée de la foule prend sa couleur dans des atlas (`FaceColorAtlas`, `BodyColorAtlas`) en
`SourceU`/`SourceV` avec un `AtlasSelector`, laissés aux valeurs par défaut des instances dans notre copie (City
Sample les pose par personnage à l'exécution). `tools/character/nicolito_skin_probe.py` (lecture seule) liste ces
paramètres pour les six têtes et le corps, exporte les deux atlas en PNG et liste les appels de groom dont dispose le
Python de l'éditeur.

Résultats de la sonde (14/11/2026) : chaque atlas fait 8192 px, une grille 4x4 de tuiles de 2048 px, 12 occupées,
aux mêmes teintes tuile pour tuile dans l'atlas du visage et dans celui du corps ; `SourceU`/`SourceV` restent à 0,5
(le milieu de la tuile). Le matériau propre de chaque tête d'homme lit sa propre tuile (`AtlasSelector` m_001 = 0 à
m_006 = 5, m_004 = 3) ; le matériau partagé du corps lit la tuile 0. City Sample donne la même tuile à la tête et au
corps (`CrowdBlueprintLibrary.cpp` : `AtlasSelector` = `SkinTextureDefinition.TextureAtlasIndex` sur les deux). Le
passage `full` a donc mis un visage de la tuile 3 sur un corps de la tuile 0. L'appel de liaison existe sous son nom
de script, `unreal.GroomLibrary`.
`create_nicolito_visual.py` donne maintenant à chaque emplacement du visage et du corps qui lit une autre tuile une
instance de son propre matériau qui lit la tuile de la tête (ou `GTP_NICOLITO_SKIN`), sous
`/Game/MetaHumans/GTP/Nicolito/Materials`, sans toucher aux matériaux de la foule ; il relie les sourcils par
`GroomLibrary` ; en mode `full`, il liste, pour le corps de Kellan et pour chaque vêtement, les os qui manquent au
squelette du corps de foule (un vêtement suit le corps par nom d'os ; un corps sans les os sur lesquels son
reciblage a été réglé reste dans sa pose de référence, la pose A vue), leur tissu et leur blueprint de
post-traitement, et les vêtements de foule qui vont à ce corps (`GTP_NICOLITO_OUTFIT` en change un).
L'ordre des tuiles dans l'atlas (rangée par rangée depuis le coin haut gauche, ou autre) n'est pas prouvé : le
premier passage garde la tuile propre de la tête, la teinte des portraits sur lesquels Nico a choisi.

Second passage `full` (14/11/2026, lumière fixe avec `-DayNight=0`) : visage et mains ont maintenant la même
teinte, mais la tuile 3 est un brun moyen hâlé, pas l'homme blanc demandé ; la liaison des sourcils a été créée
(transférée depuis le visage de Kellan) mais ils ne se voient pas ; le pan et la pose A restent ; le diagnostic
s'est arrêté sur la liste des tissus. Passage suivant : peau claire (`GTP_NICOLITO_SKIN=1`, qui dit aussi l'ordre des
tuiles : clair si rangée par rangée), sourcils transférés depuis le maillage source de la liaison de Kellan (la tête
sur laquelle le groom a été fait) ou projetés directement, sous un nouveau nom, avec les sections du LOD 0 des deux
visages dans le rapport ; le diagnostic écrit d'abord les os et liste aussi les os qu'écrit le reciblage du joueur
(`RTG_UEFNped_to_Metahuman`, racine et chaînes de son IK rig cible) qui manquent au corps de foule.

Le même pan et la même pose A légère étaient sur les images du mode face (corps et tenue de Kellan, le pan sous son
sweat, qui pend comme une chasuble) : ils ne viennent ni du corps de foule ni de ses vêtements, et Kellan, le visuel
actuel du joueur, les a sans doute aussi en jeu (la caméra de suivi voit son dos). Le journal du jeu dit que le
reciblage est posé (`RTG_UEFNped_to_Metahuman`, 5 réglages) et que le corps est animé à chaque image. Pour voir en
chiffres ce que montrent les images, `-NicolitoPortrait` écrit maintenant à chaque prise (`rig: ...`) l'angle du
bras gauche avec la verticale sur le maillage caché du pion et sur le corps (le même si le corps suit la pose du
pion), le LOD du corps, les os du corps restés à l'origine du composant alors que leur place de référence ne l'est
pas (jamais écrits par sa pose), et pour chaque maillage qui suit la pose du corps les os qui lui manquent et ceux
qu'il lit à l'origine (un sommet pondéré sur un tel os est tiré vers les pieds : une pointe qui pend entre les
jambes).

Troisième passage `full` (14/11/2026, tuile 1, `-DayNight=0`) : visage et mains clairs et de la même teinte. Les
lignes `rig:` : le bras gauche est à 17-18 degrés de la verticale sur le maillage du pion comme sur le corps, donc le
corps suit la pose du pion (la « pose A » est l'attente du sample) ; un seul os du corps, le pelvis, est à l'origine
du composant alors que sa place de référence ne l'est pas, et chaque pièce qui suit le corps l'y lit. Le pelvis est
la racine du reciblage : les autres os sont placés par les chaînes du reciblage, le pelvis par ses opérations de
bassin et de racine. Tout sommet pondéré sur le pelvis est tiré vers les pieds : l'ourlet du haut ou du sweat qui
pend comme une tunique ou une cape, la pointe entre les jambes, le pantalon qui paraît large ; Kellan, le visuel
actuel du joueur, l'a aussi en jeu. Les sourcils reliés directement se sont posés en travers de la lèvre
supérieure : plus aucun groom n'est relié par défaut. `tools/character/nicolito_rig_probe.py` (lecture seule) donne
les réglages du reciblage (le retargeter et les deux IK rigs : racine, chaînes, opérations, chacun exporté en texte)
et liste les assets de sourcils, de grooms, de liaisons et de cheveux de foule, pour la correction et pour le choix
des cheveux.

**Grooms (14/11/2026).** Kellan a six composants groom (cheveux, sourcils, cils, duvet, moustache, barbe), qui
tickent tous dans le package pour un budget de 13 ms. La simulation des brins est coupée
(`r.HairStrands.Simulation=0`). Le mode de jeu cache aussi, sans tick, les grooms dont le nom contient un mot de
`PlayerGroomsHidden` (`Fuzz`, `Eyelashes` : invisibles depuis la caméra à la troisième personne) et tient les
autres au LOD `PlayerGroomForcedLOD` (1) par leur `SetForcedLOD`, appelé par réflexion (le plugin HairStrands n'est
pas une dépendance). Le journal donne le LOD forcé de chaque groom avant et après, et une fois les LOD de son asset
(brins, cartes ou maillages), pour qu'une synchronisation de LOD du blueprint qui le remettrait se voie.
`-PlayerGrooms=full` les garde tels que le blueprint les a faits. Il en va de même pour le visuel de Nicolito s'il
a des grooms.

Mesuré sur le PC (package de développement) : les ticks de grooms passent de 6 à 4. `SetForcedLOD` n'est pas
atteignable par réflexion (pas une UFUNCTION en 5.8), le LOD forcé n'est donc pas appliqué ; il ne réduirait que le
coût GPU, le coût du thread de jeu étant les ticks. Le blueprint remet l'indicateur visible des deux grooms cachés ;
le journal donne maintenant l'état réel (indicateur visible, caché en jeu, tick) et un groom sans asset (sans doute la barbe
de Kellan) est lui aussi caché sans tick. Une fois par groom, le journal donne le nombre de LOD de l'asset et son
premier LOD en cartes (l'export omet la valeur par défaut, les brins : le LOD 1 de Hair est en brins allégés, pas en
cartes).

## 11. Assertion du Foot Placement (14/11/2026)

Candidat 35, `-GTARun` dans l'éditeur : 6/6, puis 20 s plus tard, le joueur en visée à l'arrêt avec trois policiers
à 2 m, `Assertion failed: !PelvisTransformCS.ContainsNaN()` dans `AnimNode_FootPlacement.cpp` (plugin Animation
Warping), dans le `SandboxCharacter_CMC_ABP` du Game Animation Sample sur un maillage `SKM_UEFN_Mannequin`. Le
contexte du plantage ne nomme pas l'acteur : le joueur et tous les conducteurs du trafic et de la police
(`DriverClass` vide, donc la classe du joueur) partagent ce maillage et ce blueprint. Lu dans la source du moteur
(5.8) : le nœud divise le déplacement d'un pied par le temps cumulé depuis sa dernière évaluation, sans garde
(ligne 1545) ; une évaluation sans mise à jour depuis la précédente donne un NaN. Deux fenêtres où cela peut arriver
sont fermées, la cause elle-même n'étant pas prouvée :

- le mannequin caché du joueur, posé à chaque image pour le visuel, tourne sans optimisations de fréquence de mise à
  jour (elles peuvent évaluer un maillage caché sur une image sans mise à jour) ; le journal dit si elles étaient
  actives ;
- un conducteur caché sur son siège n'est plus posé du tout (`bNoSkeletonUpdate`, étiquette `GTPSeatDormant`)
  jusqu'à `AGTPVehicle::ReleaseCharacter`, au lieu de garder ses montages en tick.

La preuve est la porte : le `-GTARun` en éditeur et celui du package qui tournent au-delà de `GTARun done` sans
l'assertion.
