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
  carrefour comptent pour un) ; les autres attendent à une ligne d'arrêt
  (depuis le 26/11, les voitures d'une même voie s'y suivent, voir 8.10).
  Sur une route à plusieurs files, chaque file continue dans celle de même
  position. Les voies restent à moins de 60 m d'un bâtiment généré, sauf les routes du circuit de fuite, gardées entières ([flight-circuit.fr.md](flight-circuit.fr.md)). Le même
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
- Têtes de feux à chaque ligne d'arrêt, sans ombre ni collision : le feu tricolore du projet (`SM_TrafficLight_Vehicle_A`, objet de la vague 8, `SignalHeadMesh`) tourné vers les voitures qui arrivent, avec un disque de couleur sur sa face avant, rouge en haut, orange, vert en bas. Elles remplacent la tête provisoire vue dans la vidéo de la porte 59 (une sphère colorée sur un poteau fin), qui revient si le maillage ne se charge pas ou avec `-SignalHeads=2`. Pas encore vérifié sur une image : l'avant du maillage est pris en +X (la convention avec laquelle `tools/city/details.py` le pose) et les feux à 25, 55 et 85 cm sous son sommet. `-SignalHeadYaw=D` le tourne, `SignalLensBelowTop` et `SignalLensStep` déplacent les feux, et la ligne « traffic lights: signal heads are ... » du journal donne la taille du maillage. `-TrafficLights=0` coupe les feux, `-SignalHeads=0` masque les têtes. Depuis le candidat 67 (vidéo de la porte 65 : cinq têtes alignées en travers de la place, une voiture qui en traverse une, trois ou quatre feux à un coin) : une tête par approche, côté trottoir, la voie la plus à droite d'un groupe de feux la garde ; aucun pied à moins de 2,2 m de l'axe d'une voie (`SignalHeadLaneClearance`), décalé vers la droite de 50 cm en 50 cm jusqu'à 3 m, sinon pas de tête ; et là où la couche Details a déjà un feu pour voitures à moins de 6 m, tourné vers les mêmes voitures à 60 degrés près (`SignalBorrowRadius` ; `tools/city/details.py` en pose un, du même maillage, à chaque nœud traffic_signals d'OpenStreetMap), les disques de la tête vont sur la face avant de ce feu et la tête ne dessine pas de maillage à elle. Lignes : « traffic lights: N signal heads, M inner lanes share the kerb-side head of their approach, P moved further right off another lane, Q left out ... » et, quand une tête est dessinée pour la première fois, « traffic lights: the head of lane L drawn on the street light ... ». Pas encore vérifié sur une image.
- Simulation, 300 s sur les voies de la rue de la République : 3, 6 et 12 voitures, aucun contact, aucun feu grillé.
- Mesuré dans le jeu (après la porte 59, le trafic legacy étant ce que la porte juge) : toutes les 10 s, « traffic cars
  t=...: driven=D of W wanted (ghosts=G) moving=M (over 1 m/s, upright: P %) stopped=S lying=L not driving (no route or no decisions)=N mean speed=...
  | within 100 m=N (moving, stopped) within 150 m=N (moving, upright, ghosts; far puppets drawn there) within 200 m=N
  nearest=... | stopped for: a car ahead, the queue along the lanes, a person, something else ahead, a red light, a
  junction in use, held (reaction or stop asked), wedged, nothing ahead; stopped over 20 s, at a dead end | recycled
  so far: stuck, far, overturned, by the chase script or the police, reused from the pool », le pendant de
  « MassTraffic cars: ». Chaque voiture comptée couchée a sa propre ligne, « traffic: ... overturned, counted
  lying: up U at X », avec : simulée ou cinématique, vitesse, distance au joueur, réaction, secondes depuis son
  dernier placement et lequel. Porte 62 : une voiture couchée dans la passe default et une dans la passe -GTARun,
  sans rien dans le journal pour dire laquelle. Une voiture pilotée renversée au-delà de 45 degrés est retirée
  aussitôt hors de la vue du joueur (« overturned » dans la ligne) ; en vue, elle se remet sur ses roues après
  2,5 s à l'arrêt. Quand le joueur n'a plus de pion, une ligne « traffic: no player pawn from t=... » le dit une
  fois : aucune voiture n'est alors créée ni retirée selon sa distance au joueur. Porte 62, passe -GTARun :
  aucune voiture à moins de 150 m à partir de 236 s, « nearest=none ». Depuis le candidat 67, un fantôme trouvé
  avec sa physique ou son véhicule Chaos allumé les voit coupés de nouveau, avec une ligne « traffic: X a ghost
  with its physics on (où, S s après son dernier placement, vitesse, verticale): off again, N so far ». Porte 65,
  passe -GTARun : une voiture placée en fantôme à 83 m du joueur, simulée 2 s plus tard et projetée, couchée, à
  87 km/h par les téléportations du fantôme. Le chemin qui laisse la physique allumée n'est pas encore connu :
  cette ligne le dira. Ce qui arrête une voiture est l'arrêt le plus proche de sa dernière décision
  (`UGTPAIDriverComponent::GetLimit`) : ce que touche la sonde (une voiture, un pion, autre chose), la file le long des
  voies, le feu rouge, la ligne d'arrêt d'un carrefour qu'une autre voiture traverse, une retenue (réaction, arrêt
  demandé par le joueur), coincée (pousse une seconde sans avancer, ou recule pour se dégager).
- Où sont les voitures pilotées (candidat 61) : créées hors de la vue entre 40 et 150 m du joueur, retirées au-delà
  de 150 m hors de la vue (`MaxCarDistance`, `-TrafficRing=M` ; ligne au démarrage « traffic: driven cars made out of
  sight ... »). À 250 m, la passe default de la porte 59 finissait avec 19 de ses 22 voitures pilotées au-delà de la
  distance fantôme (90 m) : réparties sur un anneau de 40 à 250 m, environ une voiture sur neuf tombe à moins de 90 m.
  Le script de poursuite de la démo (`-PoliceChaseDemo`, `-GTARun`) ne retire de la route, deux fois par seconde, qu'une
  voiture du trafic sur le chemin du joueur : devant, à moins de 35 m, et à moins de 2,5 m de la ligne que suit sa
  voiture. Jusqu'au candidat 60 il retirait toute voiture à moins de 35 m dans un cône de 60 degrés, voie d'en face
  comprise.

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
- **Sensations, voiture du joueur seulement** : 300 N.m pour 1 150 kg et autant par kilo pour une
  voiture plus lourde, ou le couple de la voiture s'il est plus fort (240 jusqu'au candidat 61 ; les PNJ gardent la
  valeur réelle) ; le frein à main au-dessus de 20 km/h réduit l'adhérence arrière et fait
  pivoter l'arrière avec le volant ; les glissades sans frein à main sont amorties pour que la
  voiture se redresse ; sous 35 % de santé le moteur perd jusqu'à 45 % de puissance ; une
  voiture cabossée d'un côté tire de ce côté ; une voiture sur le toit ou le flanc, arrêtée, est
  remise sur ses roues après 2,5 s (une voiture du trafic dès 45 degrés).
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
  Chacune est posée à 2,5 m à droite d'une voie (`ParkOffset`) et, depuis le candidat 62, jamais à
  moins de 2,1 m de l'axe d'une voie (`ParkLaneClearance`) : à la porte 59, une Car03 garée à côté de la
  voie 96 se trouvait sur la voie de la poursuite, et le script a dû déplacer la voiture du joueur. La ligne
  « traffic: parked ... » finit par le nombre de places écartées parce qu'elles sont sur une autre voie.
  Une place dont le sol se trouve à plus de 60 cm au-dessus de la chaussée de la voie voisine
  (`ParkKerbRise`) est aussi écartée, comptée « on street furniture » : le tracé vers le sol prend la
  première chose qu'il touche, et dans la vidéo de la porte 61 un utilitaire garé est resté 30 s sur le
  toit d'un abribus, à hauteur du premier étage.
  Depuis le candidat 66, une place est aussi écartée quand elle se trouve à moins de 12 m de voie du
  début ou de la fin de sa voie (`ParkEndMargin`), quand sa voie se rapproche de plus de 40 cm du
  trottoir dans ces 12 m (`ParkBendAllowance`), ou quand la carrosserie de la voiture (les bornes de son
  maillage) passe à moins de 1,3 m de l'axe d'une autre voie (`ParkBodyClearance`) ; la ligne les
  compte « near a lane end or bend » et « with the body over a lane ». Le trafic vise quelques mètres
  devant lui et coupe les virages, et la voie suivante commence souvent deux mètres à côté : à la porte
  62, 8 des 9 voitures garées derrière lesquelles le trafic faisait la queue étaient à moins de 10 m du
  début ou de la fin d'une voie, dont une Clio au capot posé sur une autre voie. Les voitures garées
  autour du joueur ne prennent jamais une telle place ; une demande qui doit aboutir (une mission, un
  parcours scripté) en prend une quand aucune autre n'est libre, et sa ligne finit par « no clear spot ».
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
- **Une voiture laissée dans la rue n'y reste plus pour toujours** : le réseau oubliait une
  voiture du trafic dès qu'elle n'avait plus de conducteur, et seules les voitures d'un
  accident étaient retirées. Une voiture dont le conducteur s'est enfui est maintenant gardée
  sur une liste (`GTPLeftCars.h`, testé dans `tools/vehicles/wanted_sim/left_cars_test.cpp`) :
  vide, à plus de 80 m du joueur et hors de sa vue, elle est retirée (`traffic: <car>, left in
  the street by its driver N s ago, taken away N m from the player`). Une voiture que le joueur
  prend, une voiture reconduite et une épave sortent de la liste. Ni compilé ni joué à
  l'écriture.
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
squelette SK_Vehicle (leurs os de roues ne portent pas nos noms : voir 8.7). `tools/vehicles/unreal/make_epic_vehicles.py`
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

Sans risque pour le cook (cook du candidat 30) : les AnimBlueprints de véhicules de City Sample
font un cast vers `BP_VehicleBase_Deformable`, qui demande des paquets de gameplay City Sample que
nous n'avons pas, et ses maillages squelettiques portent des AnimBlueprints de post-traitement qui
ne se chargent pas proprement. Chaque voiture reçoit une copie du maillage d'Epic dans
`/Game/Vehicles/Epic/GTP/Mesh`, sans cet AnimBlueprint de post-traitement, et l'AnimBlueprint de
roues copié du template Vehicle. Le rapport liste, par voiture, ce qu'elle tirerait dans le cook
et qui manque ou le casse (`cook_risk`) : la liste doit être vide avant un package ;
`place_masstraffic.py` fait de même pour les spawners du trafic.

Un seul corps physique (candidat 31) : dans le trafic, les voitures de City Sample étaient en
contact à chaque image (551 000 lignes de chocs ignorés en 2 min, solveur Chaos à 32 ms, image p95
à 416 ms). Soupçonné, pas encore prouvé : leurs physics assets apportent leurs propres corps en plus
du châssis (le nombre de corps du rapport est revenu vide). `AGTPVehicle` ne garde plus que le corps du châssis, celui que Chaos conduit, et coupe les
autres corps et toutes les contraintes, au démarrage puis chaque seconde (`-CarAllBodies` les
garde). Le journal le dit une fois par classe (« vehicle physics: ... keeps its chassis body », avec le nombre : sans cette ligne, un seul corps,
et la cause est ailleurs). Les
chocs ignorés par une voiture sont journalisés au plus une fois toutes les 2 s, avec leur nombre,
et deux corps d'une même voiture qui se touchent ne comptent plus comme un choc. Le bras de caméra
de poursuite ne tourne que sur la voiture du joueur. Mesuré au candidat 31b (PC, 14/11) : les
voitures City Sample portent 29 à 32 corps et 37 à 40 contraintes chacune
(PA_Collision_vehCar_vehicle*) ; avec le seul châssis, 91 chocs ignorés en 2 min au lieu de 551 116,
image de jeu p50 12,9 ms et p95 21,9 ms au lieu de 22 et 416. Ensuite, pour la marge : le maillage
squelettique d'une voiture n'évalue sa pose que s'il est rendu, avec les optimisations de fréquence
d'Unreal, et un son de moteur coupé par le budget de voix ne tick plus.

Ce que les Clio, 208 et Kangoo reprennent de la citadine de City Sample (vehicle02, 1 200 kg) :
ressorts souples sur forte précharge avec grand débattement (100 / 300, +10 / -17 cm au lieu de
250 / 50, +8 / -10), pour que la caisse prenne du roulis et plonge au freinage ; braquage de
45 degrés (38 avant), pour des demi-tours plus serrés ; frein à main de 3 500 N.m. On garde les
nôtres pour les freins (plus forts), la boîte (première plus courte) et l'adhérence. Le couple
arcade du joueur dépend maintenant de la voiture : 240 N.m pour la Clio de 1 150 kg, autant par
kilo pour une voiture plus lourde, son propre couple pour une voiture plus forte que cela (selon la
masse depuis le 16/11, voir 8.7).

### 8.7 Adhérence des voitures City Sample (16/11)

Fumée de Police sur le candidat 43 : la Car05 volée roule à 3-8 km/h pendant 4 s en 1re entre 4 300 et
4 850 tr/min, et à 49 km/h au bout de 15 s en 4e à 3 568 tr/min, ce qui fait 68 km/h aux roues
motrices ; la voiture de police fait 2,1 km/h de moyenne. Les roues motrices tournent bien plus vite
que la voiture n'avance.

Lu dans le code Chaos d'Epic (`FSimpleWheelSim::Simulate`, `UChaosWheeledVehicleSimulation`) : une
roue pousse au plus de sa charge multipliée par le frottement du sol (son matériau physique) et par
son `FrictionForceMultiplier` ; la charge vaut la moitié de la force du ressort plus la moitié de la
part statique du poids (`WheelLoadRatio` 0,5), et cette part statique vient de la position des
ressorts autour de l'origine du composant, pas autour du centre de gravité (`ComputeSprungMasses`).
Les roues motrices sont fixées par le différentiel (`DifferentialSetup`), quoi que dise
`bAffectedByEngine`, pour une roue qui a un essieu. Nos voitures City Sample sont construites à
partir de leurs pièces (`make_epic_vehicles.py`, les Blueprints de voitures de City Sample demandent
son module de jeu) : traction avant d'`AGTPVehicle`, chiffres d'Epic pour la masse, le couple, les
rapports et les roues, rien pour le différentiel, le centre de gravité ni la courbe de couple. Le
joueur reçoit 240 N·m multipliés par le couple de la voiture sur 130 : 785 N·m pour la Car05 (425),
1 292 pour la Police13.

Pistes, aucune prouvée : couple au-delà de l'adhérence ; peu de charge sur les roues avant (origine
ou centre de gravité loin en arrière, poids qui part vers l'arrière au démarrage) ; frottement faible
de notre revêtement ; caisse posée sur le sol ou qui frotte. La sonde de conduite les départage en un
passage (`AGTPVehicle::ProbeDrive`, active sauf `-NoDriveProbe`) :
- « drive config » une fois par classe : masse, différentiel et roues motrices, chiffres des roues,
  position de l'origine et du centre de gravité entre les essieux, hauteur de la caisse au-dessus du
  sol ;
- « drive probe » chaque seconde pour la voiture du joueur (toutes les 3 s, 40 lignes, pour une
  voiture de police) : pédales, rapport, régime, couple, part du poids portée par les ressorts,
  contacts de la caisse avec le sol, et par roue son contact, la force du ressort, le couple moteur,
  sa vitesse propre, le glissement, le matériau et le frottement du sol ;
- « 0-50 km/h in », « 0-100 km/h in » à l'arrêt pied au plancher, et la vitesse de pointe.

`tools/vehicles/unreal/dump_vehicle_chaos.py` (lecture seule) relève les réglages Chaos de nos
voitures et des Blueprints de voitures et de roues de City Sample copiés dans le projet, avec leur
classe parente quand ils ne se chargent pas.

Sonde du candidat 44 (bac7e06, PC, 16/11) : les ressorts portent 0 à 50 % du poids (la Police13 le
plus souvent moins de 10 %), les roues avant motrices patinent en permanence (glissement de 280 à
1 160 pour un seuil de 20), et Chaos ne monte pas de rapport tant qu'une roue patine : les voitures de
police restent en 1re, 72,7 km/h au plus. Le relevé montre que les chiffres ne sont pas ceux de City
Sample : les sept voitures roulent sur `UGTPWheelFront` / `UGTPWheelRear`, la roue de la citadine
(31 cm, ressort 100, +10 / -17 cm, frottement 3,0), et non sur les Blueprints de roues que
`make_epic_vehicles.py` a écrits pour chaque voiture (35,3 cm et ressort 260 pour la Car05) : les
classes de roues posées sur les réglages de roues tels qu'ils avaient été lus n'ont pas atteint les
Blueprints enregistrés. Et les sept sont en traction avant, alors que les vehicle03, 05, 12 et 13 de
City Sample gardent la propulsion par défaut de Chaos (seuls les vehicle02, 07 et le van01 imposent la
traction avant).

Correctif, pas encore mesuré : `make_epic_vehicles.py` donne aux Blueprints de roues de chaque voiture
tous les chiffres des Blueprints de roues de City Sample pour cette voiture (`WHEEL_COPY` ; eux se
chargent, contrairement à ses voitures), réécrit chaque réglage de roue dans le tableau, pose le
différentiel de City Sample, et son rapport dit d'où vient chaque roue et quelles classes de roues et
quel différentiel le Blueprint enregistré contient. Les voitures sont à reconstruire sur le PC. La
ligne « drive config » nomme la classe de chaque roue ; la hauteur de la caisse se lit sur la
géométrie statique seulement (le tracé de visibilité pouvait s'arrêter sur le conducteur assis).
`-DriveLaunchTest`, avec le jeu lancé dans la voiture (`-PlayerPawn=<classe de la voiture>`), attend
3 s puis roule tout droit pied au plancher pendant 25 s : 0-50, 0-100, vitesse de pointe et distance
(« drive launch test »). Depuis le candidat 49, la voiture est d'abord posée sur la plus longue ligne
droite dégagée de la carte (`PlaceForLaunchTest` : voies à moins de 1,5 m d'une droite, prolongées par
la voie suivante qui garde le cap ; une boîte de la largeur d'une voiture balayée de 0,45 à 1,55 m
au-dessus de la chaussée trouve ce qui s'y dresse), tenue sur la ligne, et mesurée jusqu'à une
demi-seconde et 15 m avant sa fin, puis freine ; la ligne ajoute le 0-130, la vitesse tous les 100 m,
l'endroit de la ligne droite où l'essai s'est arrêté, le couple, la masse et le centre de gravité. À
lancer avec `-Pedestrians=0 -Police=0 -StreetParked=0` : la boîte ne voit ni les pions ni les
véhicules. La première prise du PC (candidat 49, Car05) a trouvé 181 m dégagés, la plus longue ligne
de voies de la carte, et s'est arrêtée aussitôt à 0 km/h : la voiture n'était plus sur cette ligne
droite au départ, et freiner jusqu'à l'arrêt avant sa fin en laissait trop peu pour 100 km/h.
L'endroit où est la voiture est maintenant écrit au départ (« drive launch test: car at ..., last
placed by ... ») ; une voiture sortie de la ligne droite y est reposée, deux fois au plus. La prise
packagée de la porte du 49 l'a retrouvée au point de départ du joueur trois fois sur trois : le
solveur annule la téléportation d'une voiture simulée, comme l'avait vu le déplacement de la poursuite
de GTPDemoRoute le 16/11. La voiture est maintenant déplacée physique coupée et tenue là une
demi-seconde avant que la physique revienne (ligne « drive launch test: physics back on at ... »),
comme le fait ce déplacement. Pour le 0-130 km/h, la Car05 à 480 N.m demande environ 400 m
(estimation tirée des 94 km/h après 112 m de cette prise, pas une mesure) ; la plus longue ligne de
voies à moins de 1,5 m d'une droite fait 181 m sur la carte.

Porte 59 (b42ea35, packagée, Car05 à 480 N.m et 2 300 kg) : 0-50 km/h en 2,4 s, 0-100 en 7,9 s, 103 km/h
après 148 m en 8,3 s, là où la prise s'arrête, 15 m et une demi-seconde avant la fin de la ligne droite
de 181 m, en gagnant encore environ 2 m/s2. La pointe est celle de la ligne droite, pas de la voiture,
et reste sous les 105 à 135 km/h du jalon A. Le temps du 0-100 suit l'inverse du couple (4,8 s à
785 N.m au candidat 47 dans l'éditeur, 7,9 s à 480 : 3 770 et 3 790 N.m.s). Le candidat 62 donne à la voiture du
joueur 300 N.m pour 1 150 kg au lieu de 240 (600 N.m pour la Car05) : environ 6,3 s jusqu'à 100 km/h
selon cette règle, et environ 110 à 115 km/h au bout de la même ligne droite selon un modèle simple
ajusté sur la prise de la porte 59 (pas mesuré).

Candidat 45 (adaf9c1, PC, 16/11) : chaque voiture a ses roues et le différentiel de City Sample
(Car03, 05, 12 et Police13 en propulsion), `-GTARun` 7/7 avec une poursuite de 154 m. Car05 pied au
plancher : 0-50 km/h en 4,4 s, rapports 1 à 4, 70 km/h au bout de 10,4 s (prise arrêtée à 11,4 s). Reste :
en roulant les ressorts portent 1 à 21 % du poids, la boîte de la caisse est 2 à 7 cm sous la chaussée
et la roue avant gauche perd le contact par instants : la caisse repose encore sur la chaussée. Pour
savoir quel corps le fait, `KeepChassisBodyOnly` écrit une fois par classe chaque corps du physics
asset (« vehicle collision » : os, formes, de quelle hauteur à quelle hauteur au-dessus du plan de
contact des roues, face à la remontée de la suspension), et « drive config » donne la hauteur de la
collision de la caisse au-dessus de ce plan.

Candidat 46 (acc4a66, PC, 16/11) : aucun corps de collision ne descend sous le plan de contact des
roues (le plus bas, le châssis, est 22 à 36 cm au-dessus), mais « drive config » place les quatre roues
en X 0 Z 0, empattement 0 cm. Les meshes de voitures de City Sample n'ont pas d'os `Phys_Wheel_*` :
Chaos place une roue à la position de son os dans la pose de référence (`LocateBoneOffset`), et un os
absent du mesh se lit comme l'origine ; les quatre ressorts de chaque voiture City Sample étaient donc
en un seul point sous elle depuis le 13/11. Car05 pied au plancher : 0-50 km/h en 4,4 s, 71 km/h au
plus, 143 m avant un obstacle, des secondes sans aucune roue au sol. Correctif, pas encore mesuré :
`AGTPVehicle::FindWheelBones`, dans un monde de jeu avant l'enregistrement des composants, prend les
quatre os de roues dans le mesh quand l'os d'un réglage de roue manque : os nommés comme une roue
(wheel, whl, tire, tyre ; ni steer ni spare) aux quatre coins de leur pose de référence, un os
« phys » d'abord, puis le plus profond ; une fois par classe, une ligne « vehicle wheels » les nomme, ou
liste les os du mesh quand aucun ne convient.

Candidat 47 (82c56ff, PC, 16/11) : les sept classes trouvent leurs os (`wheel_front_l`,
`wheel_front_r`, `wheel_rear_no_spin_l`, `wheel_rear_no_spin_r`), empattement 281 cm pour la Car05,
297 pour la Police13. Car05 pied au plancher : 0-50 km/h en 1,4 s, 0-100 en 4,8 s, 111 km/h avant un
obstacle au bout de 112 m ; la poursuite roule 320 m, 83 km/h au plus. Restent deux défauts. La
Police13 lève le nez : son centre de gravité est à 0,29 de l'empattement depuis l'essieu arrière et une
roue avant décolle dans 147 relevés sur 160 (4,6 km/h de moyenne). Et la voiture du joueur va plus
vite que n'importe quelle berline réelle. Correctifs, pas encore mesurés : le centre de gravité d'une
voiture City Sample passe au-dessus de l'origine du corps du châssis, là où Chaos répartit le poids
entre les ressorts (`ComputeSprungMasses`), à sa propre hauteur (`CentreMassOverOrigin`, ligne
« vehicle balance ») ; le couple arcade du joueur est celui de la Clio par kilo (240 N.m pour
1 150 kg), ou le couple propre de la voiture s'il est plus fort : 480 N.m pour la Car05 au lieu de 785,
700 pour la Police13 au lieu de 1 292, 500 pour les Car03 et Car12 ; notre Clio garde 240, la 208 et
le Kangoo passent aussi à 240 (258 et 369 avant : leur masse Chaos est les 1 150 kg de la classe).

Trafic au candidat 47 (PC, 19/11, même carte, passe mass de 310 s) : 47,8 voitures en mouvement au lieu
de 102,0 au candidat 46, 86 arrêtées (64 derrière une autre voiture). MassTraffic simule les voitures
proches du joueur avec une physique simple qu'il extrait d'une de nos voitures créée pour cela
(`PhysicsVehicleTemplateActor`, `place_masstraffic.py`) : avec les roues à l'origine jusqu'au 46, avec
les vraies roues et le centre de gravité de City Sample très en arrière depuis le 47. Probable, pas
prouvé : ces voitures lèvent le nez et calent comme la Police13. Le centre de gravité est maintenant
déplacé avant la création du véhicule (`FindWheelBones`, à partir du centre des formes du corps du
châssis pondéré par leur volume, « vehicle balance ... set before the vehicle is created »), pour que
le modèle de MassTraffic l'ait aussi ; posé au premier tick (candidat 48), il n'atteignait que les
acteurs.

Candidat 48 (19/11) : 53,3 voitures en mouvement, 75 arrêtées (50 derrière une autre voiture) ; sans
voitures garées (`-StreetParked=0`, candidat 47) 44,3 : ni le centre de gravité ni les voitures garées
n'expliquent la chute. Pis-aller : la voiture que MassTraffic crée pour en extraire sa physique
(`UMassTrafficSubsystem::GetOrExtractVehiclePhysicsTemplate` : paramètres de création par défaut, donc
à l'origine, sans propriétaire ni instigateur, ce qu'aucune de nos créations ne fait) est reconnue
(`IsPhysicsTemplateSpawn`) et laissée exactement comme au candidat 46 : pas d'os de roue cherchés, pas
de centre de gravité déplacé, une ligne par classe « vehicle physics template: ... left as at
candidate 46 ». Les voitures conduites par le joueur et la police gardent leurs vraies roues. Reste à
expliquer après la porte : comment les vraies roues font s'arrêter les voitures de la physique simple
de MassTraffic.

Porte du candidat 48 (2ff03c2, packagé) : Car05 0-50 km/h en 2,4 s, pointe 91 km/h, arrêtée par un
obstacle après 112 m, pas de 0-100 (47 en éditeur : 1,4 s, 4,8 s, 111 km/h). Le chiffre du 47 a été pris
avec le couple d'arcade de 785 N.m, celui du 48 avec 480 N.m (couple selon la masse) : 1,4 s x 785 / 480
= 2,3 s, l'écart vient du couple, pas du jeu packagé (non mesuré à part). La physique simple de
MassTraffic (`UMassTrafficVehiclePhysicsProcessor`) trace ses suspensions contre le plan de la voie de
la voiture, applique les forces des pneus et des suspensions aux positions de roues extraites du modèle
et retient la voiture près de sa voie (`ClampLateralDeviation`) ; `-MassTemplateWheels` redonne au
modèle les vraies roues, et les lignes « MassTraffic physics template » / « MassTraffic physics car »
(GTPMassTraffic, à chaque comptage) donnent ce qui a été extrait et comment roulent les 6 voitures les
plus proches : vitesse, commandes, rapport, roues sur la route, hauteur et place par rapport à la voie.

Ces lignes, lues dans la passe mass du candidat 49 (PC, 19/11), montrent que les 97 voitures « en
mouvement » du pis-aller cachent un trafic à moyenne distance cassé : le modèle laissé comme au
candidat 46 donne à la physique simple de MassTraffic ses quatre roues en un seul point, l'origine,
sous un centre de gravité 63 à 93 cm plus haut. La voiture tient sur un seul ressort comme un crayon
sur sa pointe et tombe : 5 des 6 voitures sondées entre 51 et 67 m du joueur étaient couchées (roulis
de 102 à 174°), aucune roue sur la route, pleins gaz et braquage à fond, encore au-dessus de 1 m/s et
donc comptées en mouvement ; à 295 s, 44 des 69 voitures sous cette physique avaient une roue hors de
la route. La ligne « physics cars » compte maintenant aussi les voitures couchées (toit à plus de 45°
de la verticale). Les vraies roues avec le centre de gravité de City Sample (`-MassTemplateWheels`)
sont mesurées dans une passe mass de 300 s à la porte du candidat 51 ; le pis-aller doit leur laisser
la place dès qu'elles tiennent le trafic en mouvement. Après la porte 54 (branche
`fix/masstraffic-no-simple-physics`, voir traffic-mass.fr.md), le modèle prend par défaut les vraies
roues et le centre de gravité de City Sample, même si plus aucune voiture ne reçoit cette physique
sans `-MassTrafficPhysicsRadius=<m>` (ligne « vehicle physics template: ... given the real
wheels and City Sample's centre of mass »), `-MassTemplate46` redonne le pis-aller.

Depuis le candidat 52, la ligne « MassTraffic cars » ne compte en `moving=` que les voitures debout
(toit à moins de 45° de la verticale) et donne l'ancien compte à côté (`moving_with_lying=`,
`lying_over=`) ; une ligne « MassTraffic lying over » donne les voitures couchées à moins de 100 m du
joueur, en nombre et en part. `-MassTrafficPicture[=150,290]` prend, à ces instants, une image d'une
voiture du trafic à 50-70 m du joueur (la plus couchée, sinon la plus proche de 60 m), vue de là où se
tient le joueur, sinon de 60 m derrière ou devant elle (`Saved/Screenshots/<plateforme>/mass-traffic-<n>.png`),
et écrit la voiture, ses distances, son roulis et son tangage (« MassTraffic picture »).

Les voitures de police couchées pendant la poursuite du candidat 48 y restaient : la règle « put back
on its wheels » ne valait que pour la voiture du joueur. Elle vaut maintenant aussi pour les voitures
conduites par l'IA (`PutBackOnWheels`, ligne « put back on its wheels at ... (AI driver) »). Le centre
de gravité des voitures City Sample est réglé dans leurs Blueprints ou leurs physics assets, que notre
projet ne peut pas charger (leurs classes parentes sont dans le module de jeu de City Sample) :
`tools/vehicles/read_uasset_props.py DOSSIER` lit, sans l'éditeur, l'override du centre de gravité, le
COMNudge, l'échelle d'inertie et l'override de masse que ces paquets contiennent.

Lu sur le PC le 19/11 (octets à la place de la propriété, à confirmer dans l'éditeur de City Sample),
les Blueprints Sandbox de City Sample placent le centre de gravité, en cm dans le repère de la caisse,
à (26, 0, 56) pour vehicle03, vehicle12 et vehicle13 (Police13), (38, 0, 60) pour vehicle05, (0, 0, 54)
pour vehicle02, (30, 0, 50) pour vehicle07 et (33, 0, 40) pour la fourgonnette ; leur COMNudge est nul
(le (10, 0, -15) du parent déformable est remplacé). Nous le mettions au-dessus de l'origine à la
hauteur des formes de la caisse : (0, 0, 83) pour la Police13, 27 cm plus haut et 26 cm plus en
arrière, ce qui lève le nez à l'accélération et couche la voiture en virage. `FindWheelBones` pose
maintenant la valeur de City Sample pour chaque voiture `BP_GTP_*` avant la création du véhicule
(ligne « vehicle balance: ... City Sample's own override »), et `make_epic_vehicles.py` l'écrit dans
les Blueprints qu'il construit (`com` dans `TUNING`). Le modèle de MassTraffic garde le réglage du
candidat 46 sauf avec `-MassTemplateWheels` : ce commutateur essaie maintenant les vraies roues avec le
centre de gravité de City Sample, candidat pour un seul réglage partagé par le trafic, le joueur et la
police.

### 8.8 Voitures City Sample dessinées, trafic pendant la fuite (23/11)

Voitures invisibles, vues à la porte 67 : le joueur vole une Car05 et la conduit une minute sans
voiture autour de lui (38 à 61 km/h au compteur, Nicolito qui glisse debout), et aucune voiture de
police n'apparaît sur les images des portes 51 à 68. Cause, lue dans `epic_vehicles_report.json` et
dans `police_look_report.txt` de Police (PC) : les Blueprints de voiture de City Sample sont des acteurs
d'affichage de MassTraffic dont le script ne lit pas les composants, donc `make_epic_vehicles.py` a pris
le maillage squelettique au nom le plus court, `SKM_veh..._vehicleNN`, pour les sept voitures. Ce
maillage est le squelette : les os, l'asset physique, un emplacement `lambert1` sans matériau et une
boîte de 0 x 0 x 0 cm. La carrosserie est dessinée par `SKM_Exterior_...` (peinture, pneus, pare-brise,
9 emplacements) et `SKM_Destructible_...` (vitres, feux, rampe lumineuse, 22 emplacements), à côté.

- `AGTPVehicle::LookMeshes` : les maillages qui dessinent la voiture. Chacun devient un composant sur
  la carrosserie, sans collision, posé par elle (leader pose, comme les pièces des piétons), avec son
  propre LOD. Le squelette reste le corps (physique, os des roues, conduite inchangée). Comme il n'est
  jamais rendu, sa pose est rafraîchie tant qu'une pièce a été dessinée dans la dernière seconde ou que
  la voiture est proche (trafic), toujours pour une voiture hors du trafic ; les leviers d'ombre et de
  LOD du trafic s'appliquent aussi aux pièces.
- `make_epic_vehicles.py` copie les deux maillages en `/Game/Vehicles/Epic/GTP/Mesh/SKM_GTP_<Nom>_Exterior`
  et `_Destructible` sans leur AnimBlueprint de post-traitement et remplit `LookMeshes`, seulement si la
  boîte du corps fait moins de 1 m. Rapport : `body_box_cm`, `look`, `drawn` par voiture, `not_drawn`
  (vide avant un package).
- Journal : « vehicle look: <classe> drawn | NOT DRAWN, N look part(s) built: <chaque maillage, sa
  boîte, ses emplacements> » une fois par classe à sa première apparition ; « traffic: car class
  <classe> drawn, in the traffic | NOT DRAWN, left out of the traffic and the parked cars » pour chaque
  classe du trafic au démarrage. Une classe qui ne dessine rien est retirée du trafic, des voitures
  garées et de la police (une voiture du trafic la remplace, « police: car class ... NOT DRAWN ») ; si
  aucune n'est dessinée, toutes sont gardées.
- Le passage du script sur le PC (candidat 72) a écrit 34 lignes Error et la tâche a échoué, aucune
  venue de nos assets (`cook_risk` vide pour les sept voitures, aucun AnimBlueprint de post-traitement
  sur les 14 pièces) : 12 au chargement des acteurs d'affichage MassTraffic de City Sample (leurs
  `ABP_Proxy_Post_*`, « Unknown structure », et `ABP_vehCar_vehicle05`), 22 « FindAssetData failed »
  pour des paquets City Sample non copiés (BrokenGlass, `CAS_*`, `NS_FlashingLight`) cherchés en
  trouvant les roues. Le script lit désormais le registre d'assets pour ces recherches et ne charge un
  Blueprint que si le registre dit que c'est un véhicule Chaos ou une roue. Les pièces sont sur les
  squelettes `SK_Proxy_*` de City Sample, le squelette porteur sur `SK_veh*` : la leader pose apparie
  les os par leur nom, non vérifié.
- Pas prouvé : que les pneus de l'extérieur suivent les os des roues (même squelette `SK_Vehicle`,
  donné par le rapport), le coût de dessin de vraies carrosseries sur 6 classes sur 9 (les portes
  jusqu'au 68 ont mesuré des voitures invisibles), et l'aspect des matériaux de City Sample une fois
  cuits.

Trafic pendant la fuite (porte 67 : 7,1 voitures qui roulent à moins de 150 m pour un seuil de 8 ; 68,
même code de trafic, 10,1). Deux causes dans `legacy-traffic-lines-gtarun.txt` :

- À 83 s, GTARun a déplacé le joueur de 700 m : toutes les voitures ont été retirées comme lointaines,
  et la ligne des voitures, lue dans ce même tick, disait « driven=0 of 22 ». La réserve les rendait
  4 par tick, après 2 s de refroidissement. Désormais un saut (plus de `MaxCarDistance` depuis le
  dernier tick avec un joueur) remplit d'un coup depuis la réserve pendant `PoolCooldown` + 1 s
  (« traffic: the player moved N m in one tick ») ; le refroidissement compte depuis l'arrêt de la
  physique de la voiture (un fantôme lointain mis en réserve est déjà refroidi) ; une voiture réutilisée
  repart en roulant à la vitesse de la voie (30 km/h au plus) au lieu d'être arrêtée ; la ligne des
  voitures est lue à la fin du tick.
- À partir de 103 s, une Kangoo s'est arrêtée à 1,3 m derrière une Car07 garée, et 4 voitures y ont
  attendu tour à tour, chacune jusqu'à son retrait comme bloquée (35 s), avec 9 à 16 derrière elles
  (qui roulent à moins de 150 m : 5 à 114 s, 1 à 124 s). La Kangoo avait été mise en réserve en panique
  et en est ressortie trois fois toujours paniquée : une voiture remise sur une voie repart désormais
  calme (`UGTPAIDriverComponent::Start` remet ses réactions à zéro). Une voiture garée par le gardien
  derrière laquelle une voiture du trafic attend depuis 8 s (`StreetParkedBlockSeconds`) est retirée
  hors de vue (« traffic: street parked ... taken away out of sight, in the way »), gardée et notée une
  fois tant qu'elle est en vue ; les voitures posées à la demande (GTARun) ne sont jamais retirées. Pas
  prouvé : pourquoi cette voiture garée était sur le chemin de la Kangoo (contrôles de pose passés :
  aucune voie sous sa carrosserie).

### 8.9 La fuite reste sur les grands axes (24/11)

La fuite filmée de la porte 75 (`-GTARun -GTARunEncore`) tenait 24 à 34 km/h de moyenne. Sur ses
images : de 22 à 28 s, en quittant le quai Jean Moulin, la voiture coupe le virage sur un poteau de
feu tricolore et y reste ; à 29 s le script la pose sur une autre voie (bloquée deux fois au même
endroit), une rue étroite parcourue à 57-72 km/h, où elle se coince entre deux murs de 39 à 46 s avant
d'être reposée ailleurs ; de 72 à 95 s elle est arrêtée contre un bloc blanc au coin d'une place,
puis l'étape « outcome » du run l'arrête (le joueur se rend) jusqu'à l'arrestation. Le script de
poursuite ignorait la largeur des rues : à un carrefour il prenait la sortie la plus droite parmi
celles qui ont 250 m de route ouverte (6 carrefours devant), et il posait une voiture bloquée ou au
bout des voies sur une voie tirée au hasard entre 80 et 150 m.

Le journal de la passe filmée, lu sur les 235 voies que lit le jeu (`L_City_RueDeLaRepublique`),
montre des obstacles fixes de la carte (accessoires en maillage statique instancié hiérarchique,
collision active) posés sur cinq voies : `StaticMeshActor_27`, le bloc blanc, à 0,5 m du milieu de la
rue de la Bourse (voie 13) ; `StaticMeshActor_107` sur le passage du quai Jean Moulin à la rue Gentil
(voies 87 et 211), rencontré 15 fois en 2,5 km par la fuite d'avant la porte 75, qui tournait autour
de ce pâté ; `StaticMeshActor_22` rue Claudia (voie 4) ; `StaticMeshActor_0` rue Antoine Salles
(voie 62). Ils relèvent de l'habillage de la carte (Réalisme) : à sortir des voies.

- `GTPRoadWidth.h`, hors Unreal (`tools/vehicles/wanted_sim/road_width_test.cpp`, en CI) :
  - la chaussée autour d'une voie : les `width_m` des voies de la même route BD TOPO (`road_id`) à
    moins de 20 m de son milieu, deux sens compris. Moins de 5 m : rue étroite ; 7 m et plus : grand
    axe. Sur les 235 voies : 130 grand axe, 46 moyennes, 59 étroites. Les deux colonnes étaient déjà
    dans le fichier des voies : rien à régénérer ;
  - les voies sans fin : de là un chemin continue toujours sans impasse ni voie bloquée (on retire
    les impasses, puis les voies qui n'y mènent qu'elles, et ainsi de suite) : 103 sur 235 ;
  - la valeur du chemin après une sortie (`WayQuality`) : son meilleur chemin sur 500 m et 10
    carrefours, chaque mètre pondéré par la largeur (1 sur un grand axe, 0,7 dans une rue moyenne,
    0,25 dans une rue étroite), divisé par deux sur les 25 dernières voies prises pour que la fuite
    ne tourne pas autour d'un pâté, arrêté à une voie bloquée ;
  - la sortie prise à un carrefour (`FGTPLaneFollower::bPreferStraight`, seul le script de poursuite
    s'en sert) : jamais une voie bloquée s'il en existe une autre ; une voie sans fin d'abord ; puis
    le meilleur chemin ; puis la plus droite. Une première version (voie sans fin, puis la sortie la
    plus large) faisait moins bien que l'ancien choix sur les 235 voies : 20 % sur grands axes, 61 %
    en rues étroites ;
  - la vitesse de la fuite à partir de 12 s : 70 km/h sur un grand axe, 60 dans une rue moyenne ou
    de largeur inconnue, 45 dans une rue étroite (90 partout avant la porte 75) ;
  - où une voiture est posée (`UGTPDemoRoute::RelocateChase`), passe après passe : un grand axe sans
    fin, une rue moyenne sans fin, une voie sans fin, puis comme avant ; jamais une voie bloquée et,
    sauf à la dernière passe, jamais une voie prise récemment.
- Bloquée sur sa voie (à moins de 3 m de son chemin) contre quelque chose qui ne bouge pas (pas de
  corps simulé, pas un pion, à moins de 3 m devant, à moins de 3,5 m de la voie) : la voie est marquée
  bloquée (`AGTPTrafficNetwork::MarkLaneBlocked`, les voies sans fin sont recalculées sans elle) et la
  voiture est posée tout de suite sur une autre voie. Hors de sa voie, la voiture recule tout de
  suite au lieu de pousser 3 s.
- Journaux : « traffic: lanes for a flight: N of M never end (no dead end ahead); carriageway: ... »
  au démarrage ; « traffic: lane N blocked by ... flights keep off it » une fois par voie ; la ligne
  de remise sur voie donne « never ends », la chaussée, sa classe et la passe ; la ligne « held up »
  finit par « , does not move: backing off » le cas échéant.
- Rejoué avec les règles du jeu sur les 235 voies (`road_width_test <lanes.csv>
  [x_cm,y_cm|all] [[learn=]voies bloquées]`), 2,5 km depuis le vol de -GTARun sur le quai Jean
  Moulin : avant la porte 75, 49 % sur grands axes, 51 % en rues moyennes, autour du pâté de la rue
  Gentil ; maintenant 72 % sur grands axes, 19 % moyennes, 8 % étroites (quais Jean Moulin et Jules
  Courmont, rue Saint-Bonaventure, rue Grolée, place des Cordeliers), sans impasse. Les cinq voies
  bloquées rencontrées puis apprises : 2 rencontres (voies 211 et 87, à 190 m du vol) au lieu de 15 ;
  connues dès le départ : 65 %, 23 %, 12 % et aucune. Depuis chacune des 103 voies sans fin : 56 %
  sur grands axes (45 % avant), aucune impasse (4 avant), 19 rencontres avec les voies bloquées (260 avant).
- Non prouvé : en jeu. Tant que `StaticMeshActor_107` reste en place, la fuite de -GTARun devrait le
  rencontrer une fois environ 190 m après le vol et y être posée sur une autre voie, ce que l'étape
  pursuit compte comme une aide. La part de la police dans la fuite (passe mesurée de la porte 75 :
  unités à 23 km/h de moyenne pour 25 attendus) relève du fil Police.

### 8.10 Le trafic autour d'un joueur immobile (26/11)

Portes 103, 104 et 106 : pendant la fuite filmée, 13 à 24 voitures roulaient à moins de 150 m du joueur ;
une fois le joueur arrêté, 0 à 9 sur 28, dont jusqu'à 21 tenues par « the queue along the lanes », jusqu'à
la fin de la passe. Sur la vidéo du 104, la voiture volée reste à cheval sur le milieu d'une rue à deux
voies à partir de 78 s : aucune voiture, dans aucun sens, n'a la place de la contourner (le regard de côté
demande une voie entière libre à gauche).

- **Ce que les règles faisaient de cette voiture** (`tools/vehicles/traffic_sim/standing_sim.cpp`, les
  en-têtes du jeu, des voitures qui glissent le long des voies comme les voitures lointaines, 28 voitures
  dans l'anneau de 40 à 250 m autour de lui, sa voiture qui ferme la voie où il est et celle d'à côté).
  Sans rien sur la chaussée, les règles du 106 ne bouchent pas : 7,9 voitures sur 15 roulent à moins de
  150 m (moyenne de 14 lieux d'un fichier de 255 voies du quartier). Avec sa voiture : 5,3 sur 18,
  5,9 km/h en moyenne, 8 voitures arrêtées depuis plus de 20 s, et plus aucune voiture qui roule au pire
  moment. La dernière voiture de chaque file se tenait devant un carrefour qu'elle avait demandé en
  arrivant, et le gardait tant que la file ne bougeait pas : aucune voiture des autres rues ne passait ce
  carrefour, leurs files atteignaient les carrefours d'avant, et ainsi de suite. Rien ne disait à une
  voiture, au carrefour d'avant, que la rue était fermée, et de nouvelles voitures étaient posées
  derrière la file.
- **Ce qui change** (trois règles, chacune avec son interrupteur) :
  - une voiture arrêtée depuis 2 s derrière une voiture, une file, une personne ou un objet rend le
    carrefour devant elle et le redemande quand elle repart (`FGTPJunctionClaims::HeldUpSeconds` ;
    `-TrafficJunctionGiveBack=0` : elle le garde, comme jusqu'au 106) ;
  - les voitures évitent une voie tenue (`GTPTrafficHeld.h`) : une voiture arrêtée depuis 30 s derrière
    une autre ou une file (un feu rouge tient une file 22 s), ou depuis 8 s derrière une chose immobile
    sans place pour la contourner, le dit ; pendant 20 s après le dernier signalement, aucune voiture ne
    prend cette voie au carrefour d'avant si une autre sortie est ouverte, une voiture qui comptait la
    prendre refait son chemin, et aucune voiture n'y est posée. Une voie qui ne mène qu'à des voies
    tenues compte comme fermée aussi (`-TrafficHeldLanes=0` : coupé) ;
  - les voitures d'une même voie se suivent dans un carrefour au lieu de passer une par une ; celles
    d'une autre voie attendent qu'il soit vide, trois voitures passent encore une fois que l'une d'elles
    attend, puis la voie qui avait le carrefour laisse passer l'autre d'abord (`FGTPJunctionTable` ;
    `-TrafficJunctionFollow=0` : une voiture à la fois).
  Une voiture de police à l'arrêt (un barrage, une unité arrêtée par son homme, celle qui attend dans une
  rue de côté) est maintenant une chose que le trafic peut contourner après ses 5 s ; jusqu'au 106, le
  trafic attendait derrière elle tant qu'elle restait là.
- **Compté hors Unreal, pas joué.** Mêmes 14 lieux, sa voiture sur la chaussée : 7,1 voitures sur 16
  roulent (5,3 avant), 9,0 km/h (5,9), 4,7 arrêtées depuis plus de 20 s (8,2) ; sans rien sur la chaussée
  8,5 (7,9). Chaque règle seule : le carrefour rendu 5,9, les voies tenues 6,3. Sur une grille de rues à
  double sens sans fichier (`standing_sim grid`, lancée par la CI avec ce que les règles doivent tenir) :
  6,6 qui roulent autour de sa voiture avec les règles du 106, 8,5 maintenant, 9,0 dans la rue vide.
  `junction_test.cpp` vérifie les règles une à une. Les voitures juste derrière la sienne attendent
  toujours là : rien ici ne leur donne un passage.
- **Non prouvé :** que sa voiture est bien ce qui tenait les files dans le jeu, et pas aussi une voiture
  de police ou autre chose ; le modèle n'a ni physique, ni police, ni trafic lointain. Les lignes à lire
  à la prochaine porte : « traffic heads t=...: N car(s) stopped over 20 s for something other than the
  queue ahead: voiture on lane L, S s for quoi: la chose rencontrée, D m from the player » (les six
  arrêtées depuis le plus longtemps, toutes les 10 s) ; « traffic: lane L is held: voiture has stood
  there S s behind ... » ; « voiture: lane L ahead is held, takes lane M from lane K » ; et à la fin de
  la ligne « traffic cars », « lanes held=N (T time(s) so far, C car(s) turned away, P place(s) left
  empty there), junctions given back by a queue=G, cars that followed another through a junction=F ».

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
