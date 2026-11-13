# Véhicules conduisibles

*Version anglaise : [vehicles.md](vehicles.md).*
*Version 0.3 « Au volant » de la [roadmap](../../production/roadmap.fr.md).*
*Dernière mise à jour : 2026-11-09. Statut : design écrit, code C++ compilé sans erreur ni avertissement sur le M1 Pro (cibles éditeur et jeu, 2026-11-09) ; pas encore essayé en jeu (il faut d'abord `BP_TestCar`, section 9.1).*

## 1. Objectif

En 0.3, on vole une voiture garée rue de la République et on la conduit
jusqu'au quai du Rhône. La conduite doit avoir la sensation d'un GTA récent :
physique crédible mais pardonnante, entrée et sortie en un geste, caméra de
poursuite qui se replace toute seule.

Les voitures sont **celles du parc circulant en France** (décision de Nico,
2026-11-09) : modèles réels, toutes marques confondues, dans des proportions
réalistes, plus quelques véhicules propres à Lyon (bus TCL, Vélo'v). Usage
non commercial : marques et logos reproduits fidèlement, textures de logos
hors Git (`Plugins/CityBrands/`, voir la [bible artistique](../art/art-bible.fr.md), section 3.5).

Rien n'est réinventé : la physique vient de **Chaos Vehicles** (plugin Epic
livré avec le moteur, socle décidé), les animations d'entrée viendront plus
tard du Game Animation Sample ou d'une capture.

## 2. Ce que fait la 0.3, et ce qu'elle ne fait pas

| Dans la 0.3 | Plus tard |
| ---- | ---- |
| Entrer, sortir, sauter d'une voiture en marche | Passagers, places arrière (0.6, police) |
| Ouvrir une voiture verrouillée en cassant la vitre | Alarme, crochetage, clés |
| Éjecter un conducteur PNJ | PNJ qui résistent, fuient, appellent la police (0.6) |
| Conduite physique : accélération, freinage, marche arrière, frein à main, dérapage | Boîte manuelle, réglages par joueur |
| Caméra de poursuite, vue intérieure (V), regard arrière | Caméra cinématique, caméra capot |
| Dégâts simples : bosses par zone, vitres cassées, épave | Feu, explosion (0.4), pneus crevés, portes arrachées |
| 1 à 3 modèles conduisibles ; quelques voitures conduites par des PNJ à éjecter (§8.2) | Trafic complet avec feux (0.9), 6 véhicules finaux (0.16) |

## 3. Entrer et sortir façon GTA

Une seule touche : **F** au clavier, **Y** (bouton du haut) à la manette.

### Entrer

1. À pied, la touche choisit la voiture la plus proche dont une portière est
   à moins de 3,5 m, en favorisant celle que la caméra regarde.
2. Le personnage se place à la portière conducteur (à gauche, on roule à
   droite en France). En 0.3 il y est replacé directement ; ensuite il y
   marchera, avec motion warping pour caler l'animation.
3. Selon l'état de la voiture :

| État | Ce qui se passe | Durée sans animation |
| ---- | ---- | ---- |
| Ouverte et vide | ouvre la portière, s'assoit | 0,9 s |
| Verrouillée et vide | casse la vitre conducteur (coude), ouvre de l'intérieur | 1,6 s |
| Quelqu'un au volant | ouvre, sort le conducteur et le jette au sol, s'assoit | 2,2 s |

4. Le joueur prend le contrôle de la voiture ; le personnage est attaché au
   siège (socket `seat_driver`). Il reste caché tant qu'on n'a pas de pose
   assise pour notre personnage (option `bShowDriverInside`).

Une animation par cas (montage) se branche sans code : si le montage existe,
sa durée remplace la durée par défaut.

### Sortir

- **À l'arrêt ou lentement** (moins de 40 km/h) : la voiture freine jusqu'à
  moins de 4 km/h (3 s au plus), puis le personnage sort par la portière
  conducteur et la voiture reste au frein à main.
- **Vite** (plus de 40 km/h) : le personnage **saute** sur le côté avec une
  partie de la vitesse de la voiture, qui continue sa route sans conducteur.
  Chute et ragdoll viendront avec la 0.4 (dégâts au personnage).
- **Portière bloquée** (mur, autre voiture) : sortie par l'autre portière,
  puis par le toit ; si tout est bloqué, on reste dedans.

## 4. Conduite (Chaos Vehicles)

Le but est la sensation GTA : une voiture lourde mais vive, qui pardonne au
clavier, et dont on garde le contrôle en dérapage.

- **Boîte automatique**, marche arrière en restant sur le frein à l'arrêt.
- **Frein à main** sur les roues arrière seulement, avec un peu moins
  d'adhérence à l'arrière : un coup de frein à main en virage fait pivoter la
  voiture, comme attendu.
- **Direction qui se durcit avec la vitesse** (100 % à l'arrêt, 40 % à
  130 km/h) pour rester conduisible au clavier.
- **Réglage de base** (classe C) : citadine française type Clio V ou 208,
  1 150 kg, ~100 ch, traction, pneus 195/55 R16. Chaque modèle surcharge ce
  qui le distingue dans son Blueprint.

Trois profils de conduite suffisent pour la 0.3 et le trafic :

| Profil | Exemples | Masse | Couple | Transmission | Ressenti |
| ---- | ---- | ---- | ---- | ---- | ---- |
| Citadine | Clio, 208, C3, Sandero, Yaris, Polo | 1 150 kg | 130 N·m (Chaos sans pertes de transmission) | traction | vive, légère, survire au frein à main |
| Berline / SUV | 308, 3008, Golf, Classe A, Série 3 | 1 400 kg | 300 N·m | traction (propulsion pour BMW) | plus stable, plus rapide |
| Utilitaire | Kangoo, Berlingo, Trafic, Master | 1 900 kg | 330 N·m | traction | lent, roulis, freine long |

Cibles à mesurer en jeu (vidéo + journal) : 0 à 50 km/h en 4 à 5 s pour la
citadine ; vitesse de pointe 150 à 170 km/h ; arrêt depuis 50 km/h en
moins de 15 m.

## 5. Commandes

Les actions de conduite sont créées **en code** (`UGTPVehicleInput`) : aucun
asset d'entrée à fabriquer dans l'éditeur. AZERTY par défaut, QWERTY avec
`-Qwerty` ou `bQwertyKeyboard=True`, comme à pied.

| Action | AZERTY | QWERTY | Manette |
| ---- | ---- | ---- | ---- |
| Entrer / sortir | F | F | Y (haut) |
| Accélérer | Z, ↑ | W, ↑ | gâchette droite |
| Freiner / reculer | S, ↓ | S, ↓ | gâchette gauche |
| Tourner | Q / D, ← / → | A / D, ← / → | stick gauche |
| Frein à main | Espace | Espace | RB |
| Regarder | souris | souris | stick droit |
| Regarder derrière | C | C | clic stick droit |
| Vue intérieure / poursuite | V | V | Vue |
| Pause | Échap | Échap | Menu |

Au volant, les contextes « à pied » sont retirés et remplacés par ceux de
conduite ; l'action entrer / sortir reste toujours active, au-dessus des
autres (priorité 2). À vérifier au premier essai : que le Game Animation
Sample n'utilise ni F ni Y à pied.

## 6. Caméra

Caméra GTA : poursuite en 3e personne par défaut, intérieure en option.
Aucun effet de caméra corporelle (décision du 2026-11-09).

| Réglage | Valeur | Rôle |
| ---- | ---- | ---- |
| Distance / hauteur | 6,2 m / 1,7 m | toute la voiture et la route devant |
| Retard de position / rotation | 8 / 10 | la caméra suit avec souplesse |
| Replacement | après 1,5 s sans toucher la vue, au-dessus de 10 km/h | revient derrière la voiture, inclinée à -10° |
| Champ de vision | 75° à l'arrêt → 88° à 130 km/h | sensation de vitesse |
| Regard arrière | tant que la touche est tenue | vue vers l'arrière, puis retour |
| Vue intérieure | V, FOV 85°, à hauteur des yeux du conducteur | la même touche qu'à pied ; le choix est gardé en sortant |

La caméra évite les murs (bras à ressort avec test de collision).

## 7. Dégâts simples

- **Santé** de 1 000 points. Les chocs comptent à partir de 11 km/h de
  variation de vitesse : un choc frontal à 50 km/h retire environ 130 points.
- **Quatre zones** (avant, arrière, gauche, droite), chacune avec un **morph
  target** de bosse (`Dent_Front`…) appliqué progressivement : pleinement
  enfoncé à 400 points dans la zone. Déformation crédible pour un coût quasi nul.
- **Vitres** : composants séparés étiquetés `Glass` + zone, masqués à
  120 points dans la zone (éclats et son dans le Blueprint via
  `OnGlassBroken`). La vitre conducteur casse aussi quand on force la voiture.
- **Épave** à 0 point : le moteur ne répond plus, la voiture ne peut plus
  être volée. Fumée sous 35 % de santé, puis feu et explosion avec les armes (0.4).
- Les dégâts d'armes passent déjà par `TakeDamage` (zone déduite du point
  d'impact).

## 8. Architecture du code

Module `GranTheftPraline`, préfixe `GTP`, branche `feat/vehicles`.

| Classe | Rôle |
| ---- | ---- |
| `AGTPVehicle` (`AWheeledVehiclePawn`) | réglage de base, entrées de conduite, entrée / sortie (siège, portières, éjection, saut), caméras, dégâts |
| `UGTPWheelFront`, `UGTPWheelRear` (`UChaosVehicleWheel`) | roues de citadine : avant directrices et motrices, arrière avec frein à main |
| `UGTPVehicleInput` | actions et contextes Enhanced Input construits en code |
| `UGTPVehicleInteractionComponent` (sur le PlayerController) | touche entrer / sortir : choix de la voiture, demande de sortie |
| `AGTPPlayerController` (modifié) | échange des contextes à pied / au volant à chaque possession ; V bascule aussi la vue de la voiture |
| `UGTPCameraModeComponent` (modifié) | laisse la voiture gérer sa caméra et lui transmet le choix 1re / 3e personne |
| `AGTPGameMode` (modifié) | `-TestVehicle` gare une voiture devant le joueur dans n'importe quel niveau |
| `UGTPEngineSoundComponent`, `UGTPEngineSoundWave` | son moteur synthétisé à partir du régime Chaos et de l'accélérateur (`GTPEngineSynth.h`), sans asset |
| `AGTPHUD` (modifié) | panneau de conduite : vitesse en km/h, rapport, état de la voiture |
| `AGTPTrafficNetwork` | voies de circulation de la map, fait apparaître et recycle les voitures conduites par un PNJ |
| `UGTPAIDriverComponent` | conduit une voiture le long des voies (`GTPDriveLogic.h`), s'arrête devant un obstacle |

Déroulé d'une entrée : touche F → `UGTPVehicleInteractionComponent` →
`AGTPVehicle::BeginEnter` (personnage figé à la portière, vitre ou
conducteur selon le cas, minuterie) → `FinishEnter` (attache au siège,
`Possess`) → le PlayerController passe en contextes de conduite et la
caméra de la voiture prend la main.

Réglages : section `[/Script/GranTheftPraline.GTPVehicle]` de
`Config/DefaultGame.ini` (durées, caméra, dégâts), ou dans chaque Blueprint.
Le plugin `ChaosVehiclesPlugin` est activé dans le `.uproject`.

### 8.1 Son moteur et panneau de conduite

Le moteur s'entend grâce à un son procédural : un train d'impulsions de
combustion (trois cylindres pour la Clio V TCe : `tr/min / 60 × 3 / 2` par
seconde), plus sèches et plus fortes à l'accélération, passées dans un
filtre « silencieux », avec un bruit d'admission. Il suit le régime du
moteur Chaos, donc chaque changement de rapport s'entend, et il joue tant
que quelqu'un est au volant. `tools/vehicles/engine_sound/` le calcule hors
Unreal et vérifie hauteur, niveau et absence de clics ; des boucles
enregistrées pourront le remplacer à la passe son (0.18).

Au volant, le bas droit de l'écran montre la vitesse (km/h), le rapport
(R, N, 1-5) et une barre d'état de la voiture (blanc, orange, rouge) ; les
aides d'action remontent au-dessus.

### 8.2 Voitures conduites par des PNJ

Juste assez de circulation pour voler une voiture avec quelqu'un dedans (le
trafic complet avec feux et priorités, c'est la 0.9) :

- `tools/city/traffic.py` construit les voies à partir des axes de route
  BD TOPO : routes ouvertes aux voitures seulement, sens tirés de
  `sens_de_circulation`, conduite à droite, liaisons aux carrefours sans
  demi-tour. Avec `--map <Map>`, il les copie dans
  `Content/City/Traffic/<Map>.lanes.csv`, embarqué avec le jeu.
- Le mode de jeu crée un `AGTPTrafficNetwork` quand ce fichier existe. Il
  pose `NumCars` voitures (3, budget mesuré sur M1 : p95 40,4 ms avec 3 voitures, 45,4 ms avec 6 ; `-Traffic=N`, 0 = aucune) avec un conducteur
  sur les voies, à plus de 40 m et hors de vue, et retire hors de vue les
  voitures bloquées en cul-de-sac ou trop loin.
- `UGTPAIDriverComponent` décide avec `GTPDriveLogic.h` : direction en
  poursuite pure, vitesse limitée par la voie, réduite avant les virages et
  pour s'arrêter 4 m derrière ce qui est devant (voitures, gens). Une seule
  voiture à la fois traverse un carrefour (les nœuds proches d'un grand
  carrefour comptent pour un) ; les autres attendent à une ligne d'arrêt.
  Sur une route à plusieurs files, chaque file continue dans celle de même
  position. Les voies restent à moins de 60 m d'un bâtiment généré. Le même
  en-tête tourne dans `tools/vehicles/traffic_sim/` sur les vraies voies,
  avec des critères de réussite.
- Éjection : personne ne monte dans une voiture à plus de 12 km/h ; une
  voiture s'arrête devant un piéton. La voiture freine pendant qu'on sort
  son conducteur, qui s'enfuit ensuite à 25 m.

### 8.3 Chute dans le Rhône

`UGTPRiverRecovery` (sous-système du monde) écoute les boîtes de déclenchement marquées `GTP_RiverRecovery` que la génération de la ville pose sur l'eau, et relève les points `GTP_RiverExit` placés sur les deux berges (`tools/city/riverside.py`). Un lit du fleuve avec collision, 2,5 m plus bas, empêche de tomber hors du monde.

- À pied : le joueur flotte `SwimSeconds` (4 s), l'écran passe au noir et le joueur se retrouve au point de sortie le plus proche.
- En voiture : la voiture est noyée (épave, plus de commandes) ; après `SinkSeconds` (2,5 s), le conducteur est sorti par `AGTPVehicle::EjectDriverTo` et ramené au point de sortie le plus proche avec le même fondu. La voiture disparaît `RemoveCarSeconds` (12 s) après sa chute, avec son conducteur non joueur.

Pas de plugin Water ni de volume physique : de simples événements de chevauchement, presque gratuits sur le M1.
### 8.4 Feux tricolores (version 0.4)

`GTPTrafficLights.h` (sans types moteur, utilisé aussi par `tools/vehicles/traffic_sim`) pose des feux à chaque carrefour du fichier de voies atteint par deux directions qui se croisent. Les voies entrantes à moins de 45° de la première approche (dans un sens ou l'autre) forment le groupe 0, les autres le groupe 1 ; les groupes alternent : vert 12 s, orange 3 s, rouge intégral 2 s, chaque carrefour décalé dans le cycle.

- Les conducteurs PNJ s'arrêtent à la ligne au rouge, et à l'orange s'ils le peuvent en freinant confortablement ; une voiture arrêtée au rouge ne réserve aucun carrefour, donc le trafic transversal continue. Les voitures voient aussi celle qui les précède le long des voies, ce que la sonde droite rate dans les virages.
- Piétons : `AGTPTrafficNetwork::IsWalkAllowed(Location, WalkDirection)` dit si l'on peut traverser à cet endroit maintenant (toujours vrai loin d'un carrefour à feux).
- Têtes de feux provisoires (une sphère colorée sur un poteau à chaque ligne d'arrêt, sans ombre ni collision) en attendant le modèle de mobilier urbain. `-TrafficLights=0` coupe les feux, `-SignalHeads=0` masque les têtes.
- Simulation, 300 s sur les voies de la rue de la République : 3, 6 et 12 voitures, aucun contact, aucun feu grillé.

### 8.3 Passe GTA du 13/11 : vol, sensations de conduite, trafic plus vivant

Écrit en un lot, compilé et essayé ensuite (`GTPCarjackLogic.h`, vérifié hors Unreal par
`tools/vehicles/carjack_test`).

- **Conducteur éjecté** : fuit (55 %), revient sur le voleur (30 % : coups de poing à pied,
  coups sur la portière une fois le voleur à bord, abandonne après trois coups, 8 s, ou quand
  la voiture part) ou se recroqueville puis fuit (15 %). `UGTPCarjackVictimComponent` ;
  `-CarjackMood=flee|resist|cower` force une humeur.
- **Voler une voiture qui roule** : F près d'une voiture du trafic à moins de 45 km/h (6,5 m) :
  son conducteur s'arrête (`AGTPVehicle::RequestAIStop`), le joueur l'éjecte dès qu'elle est
  assez lente. Les voitures de police ne s'arrêtent pas.
- **Démarrage** : une voiture garée démarre en 0,7 s, une voiture à vitre brisée est câblée en
  1,4 s (ni gaz ni bruit moteur avant) ; une voiture prise à son conducteur tourne déjà.
- **Sensations, voiture du joueur seulement** : 240 N.m au lieu de 130 (les PNJ gardent la
  valeur réelle) ; le frein à main au-dessus de 20 km/h réduit l'adhérence arrière et fait
  pivoter l'arrière avec le volant ; les glissades sans frein à main sont amorties pour que la
  voiture se redresse ; sous 35 % de santé le moteur perd jusqu'à 45 % de puissance ; une
  voiture cabossée d'un côté tire de ce côté ; une voiture sur le toit ou le flanc, arrêtée, est
  remise sur ses roues après 2,5 s.
- **Caméra de poursuite** : recule de 1,5 m de plus à pleine vitesse, regarde en partie dans
  le sens de la glisse, secoue aux chocs.
- **Trafic** : 22 voitures conduites (16 avant) et 30 lointaines ; Clio V, 208 et Kangoo quand
  leurs Blueprints sont dans le build (profils utilitaire et 208 par modèle) ; chaque
  conducteur a un tempérament : calme (55 %), pressé (30 % : 15 % plus rapide, klaxonne et
  contourne plus tôt, accélère sous la menace d'une arme ou quand on le percute), craintif
  (15 % : plus lent, sort aussitôt sous la menace).

### 8.5 Deuxième passe du 13/11 : voitures à voler, accidents, armes, sauts en marche

Écrit d'un bloc, pas encore compilé (`GTPIncidentLogic.h`, vérifié hors moteur par
`tools/vehicles/incident_test`).

- **Voitures garées le long des rues** : une dizaine de vraies voitures (Clio V, 208, Kangoo)
  garées au bord du trottoir entre 50 et 110 m du joueur, hors de sa vue, retirées au-delà de
  150 m quand il ne les voit pas. 60 % sont fermées (une vitre à casser), 40 % de celles-ci ont
  une alarme : le klaxon sonne par à-coups 15 s quand la vitre est cassée ou que la voiture
  garée est heurtée. Physique seulement à moins de 35 m. `-StreetParked=N` (0 = aucune).
- **Accidents entre voitures du trafic** (à moins de 80 m du joueur, choc d'au moins
  4,5 m/s, deux à la fois au plus) : un léger accrochage finit par un coup de klaxon (ou, une
  fois sur trois, une dispute) ; sinon, une fois sur cinq, celui qui a percuté accélère et
  s'enfuit pendant que l'autre sort et l'invective ; un conducteur craintif peut s'enfuir à
  pied ; le plus souvent les deux sortent, s'avancent l'un vers l'autre, se disputent 6 à
  10 s avec une bousculade de temps en temps, retournent à leur voiture et repartent.
  Au-delà de 22 m/s, tout le monde s'enfuit. Une arme, un tir ou une voiture volée met fin à
  la scène : ils s'enfuient. `UGTPTrafficArgueComponent` ; `-TrafficIncidents=0` coupe.
- **Les conducteurs du trafic sortent vraiment** : braqué, craintif ou après un accident, le
  conducteur s'arrête et quitte la voiture (avant, `PanicExit` ne faisait rien sur une
  voiture du trafic).
- **Réactions aux armes actives par défaut** : sans le système de menace
  (`-ThreatResponse=1`), le trafic lit lui-même l'arme du joueur : visée (cône de 10 degrés,
  20 m) et tirs (35 m).
- **Sortie d'une voiture en marche** : dès 20 km/h (40 avant), le joueur saute, roule ou est
  projeté, et se blesse, selon la vitesse (ADR-0006).
- **Suivi d'une voiture qui roule (IDM)** : derrière une voiture en mouvement, le trafic
  applique désormais l'Intelligent Driver Model (Treiber et al. 2000, la loi des
  gestionnaires de trafic façon CARLA, retenue dans le recensement de réutilisation) avec un
  écart de 1,4 s, au lieu de viser l'arrêt derrière chaque voiture ; une file redémarre
  d'un bloc. Obstacles immobiles, feux et lignes d'arrêt gardent l'ancienne règle d'arrêt.
  `tools/vehicles/idm_test`.
- **Tir depuis une voiture** : fait côté combat (feat/combat-gta-shooting) ; le trafic réagit
  aussi à la visée et aux tirs par la vitre (l'arme du conducteur assis dans la voiture du joueur
  est lue).

### 8.6 Voitures City Sample d'Epic (13/11)

Pas compilé. City Sample Vehicles (Fab, gratuit, hors Git) fournit 14 voitures Chaos sur le
squelette SK_Vehicle, dont les os de roues sont les nôtres. `tools/vehicles/unreal/make_epic_vehicles.py`
en reconstruit sept sur `AGTPVehicle` (vol, dégâts, réactions) avec les réglages Chaos d'Epic :
les vehicle02, 03, 05, 07, 12 et la camionnette 01 rejoignent le trafic et les voitures garées
(camions, bus et remorque sont trop gros pour la Presqu'île) ; la vehicle13, berline de
1 850 kg avec rampe lumineuse, devient la voiture de police (`PoliceVehicleClass`). Les classes
absentes sont ignorées : rien ne change tant que le script n'a pas tourné sur le PC. Le Vehicle
Variety Pack est inutilisable tel quel : ses Blueprints dérivent du WheeledVehicle de PhysX,
disparu en 5.8.

Dans le projet City Sample complet (le pack seul n'est plus sur le PC), les Blueprints d'une voiture
sont des acteurs d'affichage de MassTraffic (`CitySampleMassVehicleBase`, un acteur, pas un véhicule
Chaos). Le script prend alors un Blueprint Chaos de cette voiture s'il en existe un ; sinon il lit
le maillage squelettique de la caisse dans les composants du Blueprint et pose sur notre classe les
réglages Chaos relevés dans le pack (AnimBlueprint de roues Chaos seulement : celui d'un acteur
d'affichage est écarté). L'échec d'une voiture n'arrête plus les autres, et le rapport est
toujours écrit.

Ce que les Clio, 208 et Kangoo reprennent de la citadine de City Sample (vehicle02, 1 200 kg) :
ressorts souples sur forte précharge avec grand débattement (100 / 300, +10 / -17 cm au lieu de
250 / 50, +8 / -10), pour que la caisse prenne du roulis et plonge au freinage ; braquage de
45 degrés (38 avant), pour des demi-tours plus serrés ; frein à main de 3 500 N.m. On garde les
nôtres pour les freins (plus forts), la boîte (première plus courte) et l'adhérence. Le couple
arcade du joueur dépend maintenant de la voiture (240 N.m pour une Clio de 130 N.m, davantage
pour une voiture plus puissante).

## 9. Préparer une voiture conduisible

### 9.1 Première voiture d'essai (sans Tripo)

Pour la première compilation, la voiture du modèle de projet *Vehicle*
d'Epic (contenu moteur, gardé en local hors Git comme le GASP) suffit :

1. Copier `SportsCar` du modèle *Vehicle* dans `Content/Vehicles/Test/`.
2. Créer `BP_TestCar`, enfant de `AGTPVehicle` : maillage `SKM_SportsCar`,
   son Animation Blueprint (nœud *Wheel Controller*).
3. Lancer avec `-TestVehicle` : la voiture est garée devant le joueur.

Les os de roues de ce modèle (`Phys_Wheel_FL`…) sont ceux que notre code
attend : nos propres voitures suivent la même convention.

### 9.2 Voitures du jeu (Tripo → Blender → Unreal)

| Étape | Convention |
| ---- | ---- |
| Master Tripo | voiture entière, échelle réelle, sans logo (posé ensuite en décal) ; images de référence du modèle réel |
| Découpe Blender | caisse, 4 roues séparées, vitres en objets séparés (pare-brise, lunette, vitres gauches, vitres droites) |
| Squelette | `root` au sol au centre, os de roues `Phys_Wheel_FL`, `Phys_Wheel_FR`, `Phys_Wheel_BL`, `Phys_Wheel_BR` au centre de chaque roue, axe X vers l'avant |
| Morph targets | `Dent_Front`, `Dent_Rear`, `Dent_Left`, `Dent_Right` (sculptés une fois, réutilisables par silhouette) |
| Sockets | `seat_driver` à la hanche du conducteur |
| Budget | bible artistique : ≤ 1,5 M triangles en Ultra, 120 k en M1, 4 LOD, textures 4K / 2K |
| Unreal | `Content/Vehicles/<Modèle>/` : `SKM_`, `PA_` (asset physique), `ABP_` (Wheel Controller), `BP_` enfant de `AGTPVehicle` avec les vitres taguées |

Le script `tools/vehicles/prepare_vehicle.py` fait tout cela sans Unreal,
dans Blender ou avec le module Python `bpy` (5.2) :

```
python -I tools/vehicles/prepare_vehicle.py -- --master assets/tripo/v/V-01.glb \
    --id V-01 --out assets/staging/vehicles --preview
```

Réglages par modèle dans `tools/vehicles/vehicles.json` (longueur réelle,
rayon et largeur des pneus, siège). Sortie dans `assets/staging/vehicles/<id>/`
(hors Git) : `SKM_<Nom>.fbx` (caisse + roues, squelette, 4 bosses, 1,36 M
triangles), `SM_<Nom>_Glass_<Zone>.fbx`, textures 4K, `report.json` et deux
aperçus (roues en rouge, vitres en bleu).

Premier passage (2026-11-09) sur Clio V, 208 II et Kangoo II : avant
détecté seul, 4 roues trouvées et découpées proprement sur les trois,
environ 2 min par voiture. Limites connues :

- **Vitres** : Tripo peint les vitres avec les reflets de la carrosserie
  (même couleur, rugosité et métal), la texture ne les distingue donc pas.
  Par défaut, le script prend toute la partie haute (au-dessus de la
  ceinture de caisse, sans le toit). Pour une voiture prioritaire,
  `vehicles.json` peut donner le contour réel des fenêtres
  (`"mode": "outline"` : polygones vus de dessus pour le pare-brise et la
  lunette, vus de côté pour les vitres latérales) ; la Clio V l'utilise
  (2026-11-09), rétroviseurs et montants restent carrosserie. Vraie
  transparence = il faudra un habitacle (plus tard).
- **Proportions** : à longueur réelle, les masters sont ~13 % trop hauts.
  Avec `height_m`, le script ne comprime que ce qui est au-dessus des roues
  (2,15 rayons de roue), les roues restent rondes : Clio V 1,46 m (réel
  1,44 m).
- **Bosses** : les quatre morph targets sont exportés à poids 0 (la voiture
  démarre intacte).
- **LOD du profil M1** : générés par la réduction de maillage squelettique
  d'Unreal à l'import (LOD 1 ≈ 120 k triangles) ; la décimation de Blender
  casse l'ombrage de ces surfaces à ce taux.
- Logos : peints dans la texture par Tripo, gardés ; leur version en décal
  hors Git reste à faire.

## 10. Modèles à générer (proposition transmise au fil Tripo)

Le parc français en circulation, pas seulement les marques françaises. Les
proportions donnent la répartition visée dans le trafic et les rues garées
(estimation d'après le parc roulant 2025, à affiner).

### Priorité 1 : conduisibles en 0.3 (3 modèles)

| Id | Modèle | Pourquoi |
| ---- | ---- | ---- |
| V-01 | Renault Clio V (2019+) | voiture la plus courante en France |
| V-02 | Peugeot 208 II (2019+) | deuxième du parc récent |
| V-03 | Renault Kangoo II (artisan, blanc) | l'utilitaire de toutes les rues de Lyon |

### Priorité 2 : trafic et voitures garées (0.7)

| Id | Modèle | Part visée |
| ---- | ---- | ---- |
| V-04 | Dacia Sandero III | 6 % |
| V-05 | Citroën C3 III | 5 % |
| V-06 | Peugeot 308 II | 5 % |
| V-07 | Renault Captur II | 4 % |
| V-08 | Peugeot 3008 II | 4 % |
| V-09 | Volkswagen Golf VII | 4 % |
| V-10 | Toyota Yaris IV (hybride) | 3 % |
| V-11 | Volkswagen Polo VI | 3 % |
| V-12 | Renault Twingo III | 3 % |
| V-13 | Fiat 500 | 3 % |
| V-14 | Renault Mégane IV | 3 % |
| V-15 | Dacia Duster II | 3 % |
| V-16 | Mercedes Classe A (W177) | 2 % |
| V-17 | BMW Série 3 (G20) | 2 % |
| V-18 | Tesla Model Y | 2 % |
| V-19 | Citroën Berlingo III (utilitaire) | 3 % |
| V-20 | Renault Trafic III | 2 % |
| V-21 | Peugeot Boxer / Citroën Jumper | 2 % |
| V-22 | Taxi : Toyota Corolla break hybride (lumineux « TAXI ») | taxis lyonnais |

Clio et 208 font ensemble environ 20 % ; le reste (≈ 20 %) est couvert par
des variantes de couleur et d'année des modèles ci-dessus.

### Priorité 3 : véhicules de la ville et deux-roues (0.6 à 0.16)

| Id | Modèle | Version |
| ---- | ---- | ---- |
| V-23 | Bus TCL articulé (type Heuliez GX 437, livrée TCL) | 0.7 |
| V-24 | Trolleybus TCL (type Hess lighTram, lignes C) | 0.7 |
| V-25 | Peugeot 5008 Police nationale (sérigraphie) | 0.6 |
| V-26 | Renault Mégane / Kangoo Police municipale de Lyon | 0.6 |
| V-27 | Fourgon pompiers SDMIS (VSAV) | 0.16 |
| V-28 | Renault Kangoo La Poste (jaune) | 0.7 |
| V-29 | Benne à ordures de la Métropole | 0.7 |
| V-30 | Vélo'v (vélo en libre-service lyonnais) | 0.13 |
| V-31 | Trottinette en libre-service | 0.13 |
| V-32 | Scooter Piaggio Liberty / Vespa | 0.13 |
| V-33 | Yamaha TMAX | 0.13 |
| V-34 | Vélo cargo de livraison | 0.13 |

Coût estimé au tarif Studio actuel (55 crédits par master, sans les
variantes) : 34 modèles ≈ 1 870 crédits. Pour V-01 à V-03, demander à
Tripo la voiture entière avec images de référence ; la découpe des roues et
des vitres se fait dans Blender (section 9.2).

## 11. Tests et preuve attendue

- Playtest automatique (comme `tools/level/playtest_street.py`) : entrer,
  rouler 30 s en ligne droite puis en virage, sortir ; journal des vitesses
  et des états.
- Vidéo de 3 minutes dans la build Shipping (enregistrée par le jeu), CSV de
  temps d'image : 95 % des images ≤ 33,3 ms en conduisant (profil M1).
- Vérifications à la main par le fil : clavier AZERTY, manette, sortie
  bloquée contre un mur, saut à 60 km/h, vitre cassée sur voiture verrouillée.

## 12. Points ouverts

- **Premier essai en jeu** : le code compile (2026-11-09) ; les réglages
  Chaos (moteur, boîte, roues) restent à éprouver en conduisant.
- Animations entrer / sortir / assis : à trouver (GASP n'en a pas) ; en
  attendant, personnage caché au volant.
- Chaos Vehicles reste marqué expérimental en 5.8.2 : à surveiller, repli
  possible sur le *Mover* plus tard.
- Coût du trafic sur le M1 : 15 véhicules en mouvement prévus par la bible ;
  à mesurer en 0.7.
