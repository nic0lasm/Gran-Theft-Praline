# Personnage jouable : intégration du Game Animation Sample

*Version anglaise : [player-character.md](player-character.md).*
*Version 0.1 « Jouer en un clic », tâches 3 et 4 du [tableau de bord](../../production/roadmap-status.fr.md).*
*Dernière mise à jour : 2026-11-09. Statut : plan prêt, sample relevé, intégration dans l'éditeur à venir.*

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
  Nos propres `IA_*` et `IMC_*` vivent dans notre `Content/Input` ; pour
  éviter la collision de chemins, le sample passe dans son plugin
  (`/GameAnimationSample/Input`).
- **Caméra** : Gameplay Camera System (`CameraAsset_SandboxCharacter`,
  `CameraDirector_SandboxCharacter`, rigs Close, Medium et Far). Le mode
  première personne s'ajoute comme un nouveau rig dans ce directeur.
- **Contenu à garder dans le plugin** : `Blueprints`, `Input`, `Audio`
  (bruitages), `Characters/UEFN_Mannequin`, `Characters/UE5_Mannequins`,
  `Widgets`. **À écarter** : `Characters/Paragon`, `Characters/Echo`,
  `MetaHumans`, `IsolatedExamples`, `Levels`, `Movies` (environ 5 Go de moins),
  ainsi que les Blueprints qui en dépendent (`RetargetedCharacters/BP_Echo`,
  `BP_Twinblast`). La liste exacte se vérifie avec le visualiseur de
  références avant suppression.

## 3. Licence et Git : le contenu Epic ne va pas dans le repo public

Le repo est public sous licence MIT. Les assets du GASP (`.uasset`) sont
du contenu Epic soumis à l'Epic Content License : on peut les utiliser et
les livrer **intégrés au jeu empaqueté**, mais pas les redistribuer en
source au grand public. Le code, les fichiers `.ini`, le `.uproject` et nos
propres créations peuvent, eux, être publics.

Règle décidée par Nico le 2026-11-09 : le contenu Epic et Fab reste hors du repo public, en local.

| Contenu | Où | Dans Git ? |
| ---- | ---- | ---- |
| Assets du GASP (personnages, animations, bases de données de poses, Blueprints d'origine) | Plugin de contenu local `Plugins/GameAnimationSample/` | **Non** (`.gitignore`), réinstallé par la procédure §6 |
| Nos Blueprints enfants, réglages, caméra | `Content/Characters/Player/` | Oui (LFS) |
| Nos actions et contextes d'entrée | `Content/Input/` | Oui (LFS) |
| Code C++ (caméra, contrôleur) | `Source/GranTheftPraline/` | Oui |

Ranger le GASP dans un plugin de contenu sépare proprement ce qui est à
Epic de ce qui est à nous, et permet de passer à une version plus récente
du sample sans toucher à notre contenu. La même règle s'appliquera plus
tard à Megascans et aux autres assets Fab : utilisés, empaquetés, jamais
publiés en source.

## 4. Caméra façon GTA

| Mode | Réglage de départ | Commentaire |
| ---- | ---- | ---- |
| **Troisième personne** (défaut) | Bras de caméra ~300 cm à pied, décalage épaule droite ~40 cm, hauteur ~60 cm au-dessus du bassin, champ de vision 75°, collision de caméra avec les murs, retour automatique derrière le personnage après ~2 s sans toucher la caméra (désactivable) | Rues étroites de la Presqu'île : la collision de caméra est indispensable |
| **Première personne** | Caméra attachée à la tête, champ de vision 90° (réglable 70 à 110°), tête masquée pour le joueur, corps et ombre visibles, mêmes animations que la troisième personne | Utiliser le rendu première personne du moteur (champ de vision et échelle séparés pour le corps) s'il est disponible en 5.8, à vérifier dans l'éditeur |
| **Bascule** | Touche `V` / bouton Vue de la manette, transition rapide (~0,2 s) | Le mode choisi est conservé dans les options |

Mise en œuvre : partir de la caméra du GASP (Gameplay Cameras) et ajouter
un mode première personne ; si l'ajout est trop fragile, une petite classe
C++ maison (`UGtpCameraModeComponent`) gère les deux modes sur un bras de
caméra classique. Le choix se fait dans l'éditeur, sur mesure.

## 5. Entrées (Enhanced Input)

Contextes : `IMC_OnFoot_KBM` (clavier-souris), `IMC_OnFoot_Gamepad`,
`IMC_Global` (caméra). Échap et le bouton Menu de la manette sont réservés au menu pause, géré par le code du jeu. Disposition clavier **AZERTY par défaut**
(Mac de Nico), QWERTY en option, remappage complet prévu en 0.12.

| Action | Type | Clavier AZERTY | Clavier QWERTY | Manette |
| ---- | ---- | ---- | ---- | ---- |
| `IA_Move` | Axe 2D | Z Q S D | W A S D | Stick gauche |
| `IA_Look` | Axe 2D | Souris | Souris | Stick droit |
| `IA_Sprint` | Maintien | Maj gauche | Maj gauche | A / Croix (maintien) |
| `IA_Walk` | Bascule | Alt gauche | Alt gauche | Clic stick gauche |
| `IA_Jump` | Appui | Espace | Espace | X / Carré |
| `IA_Crouch` | Bascule | C | C | B / Rond |
| `IA_ToggleCameraMode` | Appui | V | V | Bouton Vue / Share |
| `IA_CameraRecenter` | Appui | clic molette | clic molette | Clic stick droit |

Modificateurs : zone morte radiale 0,2 sur les sticks, sensibilité souris
et manette réglables, inversion de l'axe vertical en option. Les noms
d'actions sont gardés pour les versions suivantes (`IA_Aim`, `IA_Fire`,
`IA_EnterVehicle` arrivent en 0.3 et 0.4).

## 6. Étapes d'intégration

Préparation (sans l'éditeur du projet) :
1. Récupérer le GASP 5.8 sur le Mac : launcher Epic, onglet Fab,
   « Game Animation Sample », créer le projet dans
   `Samples/GameAnimationSample`, à côté de `GTAL` (hors repo).
2. Relever dans son `.uproject` et ses `.ini` les plugins, tags et CVars
   exacts, et mettre à jour ce document.

Dans l'éditeur (une seule session Unreal à la fois, après la conversion du
projet en C++) :
3. Dans le projet GASP : créer un plugin de contenu `GameAnimationSample`,
   y déplacer le contenu du sample, corriger les redirections.
4. Copier le plugin dans `GranTheftPraline/Plugins/`, activer les plugins
   moteur, fusionner tags et CVars.
5. Créer `BP_PlayerCharacter` (enfant du personnage du GASP) dans
   `Content/Characters/Player/`, nos `IA_*` et `IMC_*` dans
   `Content/Input/`, et le déclarer comme pion par défaut : `PlayerPawnClass`
   dans `DefaultGame.ini`, section `[/Script/GranTheftPraline.GTPGameMode]`.
   Ajouter `Content/Characters` et `Content/Input` (et le plugin GASP) aux
   dossiers toujours cuits (`DirectoriesToAlwaysCook`).
6. Ajouter le mode première personne et la bascule.
7. Tester dans la greybox : parcours de 2 minutes, capture vidéo, relevé du
   temps d'image (indicatif en 0.1), vérification manette.

## 7. Critère « fait quand »

- Marcher, courir, sprinter, sauter, s'accroupir et enjamber au
  clavier-souris et à la manette, dans `L_RueRepublique`.
- Caméra troisième personne qui ne traverse pas les murs ; bascule
  première personne sans saut d'image ni tête visible.
- Preuves : vidéo de 2 minutes et captures des deux modes, rangées avec
  les preuves de la 0.1 ; aucun asset Epic suivi par Git (contrôle
  `git ls-files Plugins/`).

## 8. Risques

| Risque | Parade |
| ---- | ---- |
| Migration du GASP incomplète (caméra qui ne démarre pas, tags manquants), problèmes déjà signalés par la communauté | Plugins activés avant la copie, tags et CVars repris, test immédiat dans une carte vide |
| Coût du motion matching sur le M1 Pro | Mesure dès l'intégration ; budget exigé seulement à partir de la 0.2 |
| Première personne : clipping du corps, mal des transports | Rendu première personne du moteur, champ de vision réglable, pas de balancement de tête par défaut |
| Contenu Epic poussé par erreur | `.gitignore` sur `Plugins/GameAnimationSample/` et vérification avant chaque push |
