# Le circuit de la fuite filmée

[English](flight-circuit.md)

Le parcours automatique (`-GTARun`, `-PoliceChaseDemo`) vole une voiture quai Jean Moulin et s'enfuit. Jusqu'à la porte 78 la fuite était libre sur les voies de la carte : à chaque carrefour, la sortie qui offrait la meilleure suite. N'importe quel mur, poteau ou trou de tout le centre pouvait l'arrêter, et le script remettait la voiture sur une voie 4 à 8 fois en quatre minutes filmées. Désormais la fuite suit un circuit, choisi hors du jeu et gardé dans Git. La police poursuit librement : elle choisit son chemin, et sur une voie du circuit une unité suit la ligne du circuit, comme la fuite (à la porte 78 les unités faisaient 14 à 21 km/h de moyenne sur les points des voies ; `-PoliceCircuitLines=0` pour comparer).

## Le fichier

`GranTheftPraline/Content/Test/Circuits/<Carte>.circuit.csv`, une ligne par voie : où la voie commence et finit (cm Unreal), son nom, et la ligne de conduite suivie sur elle. Le numéro de la voie dans le fichier des voies n'est qu'un indice : le jeu retrouve la voie de chaque ligne par ses deux extrémités, à 3 m près, et le circuit survit donc à un fichier des voies refait avec d'autres numéros. Les lignes placées sous `approach` sont parcourues une fois, du vol à la boucle ; celles placées sous `lap` sont parcourues tour après tour.

Le jeu vérifie le fichier au démarrage de la carte (`GTPFlightCircuit.h`) : chaque voie mène à la suivante, la dernière de la boucle à la première, la dernière de l'approche à une voie de la boucle. Un fichier qui ne correspond pas aux voies lues n'est pas utilisé, la fuite est libre comme avant, et le journal dit pourquoi (`traffic: flight circuit NOT USED: ...`).

## La boucle de L_City_RueDeLaRepublique

La boucle passe par la place Bellecour, la vitrine de la ville : 1793 m, 37 voies. Le quai Jules Courmont vers le sud (346 m), la rue Childebert, la place de la République et son bassin, la rue Marcel-Gabriel Rivière, la place de l'Hôpital, la rue Bellecordière, la rue de la Barre, l'angle nord-est de la place Bellecour, la rue du Président Édouard Herriot vers le nord en passant la place des Jacobins, la rue Jean de Tournes, la rue Stella, la rue Grolée, la rue du Président Carnot, puis à droite de nouveau sur le quai.

Le vol a lieu quai Jean Moulin, côté qui monte vers le nord, 400 m au nord de la boucle. L'approche (488 m) fait demi-tour par le carrefour du pont Lafayette et descend le quai jusqu'à la rue Childebert.

Jusqu'à la porte 80 le fichier des voies ne pouvait pas porter cette boucle, pour deux raisons, toutes deux dans `tools/city/traffic.py` :

- un tronçon de 2,7 m de la rue Herriot, place des Jacobins, était écarté comme trop court, ce qui coupait la rue. Un tronçon de route entier est maintenant gardé à partir de 0,6 m ;
- les voies n'étaient créées qu'à moins de 60 m d'un immeuble généré, ce qui retirait 610 m à cette boucle (201 m du quai au nord de la rue Childebert, 118 m de la rue Grolée, 96 m de la rue Herriot au nord des Jacobins, 82 m de la rue Childebert, 81 m de la rue Stella, 32 m de la rue Jean de Tournes). Les routes du circuit sont maintenant listées dans `tools/city/through_roads/<focus>.txt` et gardées entières dans l'étendue de la carte. Le fichier des voies passe de 229 à 255 voies.

La boucle choisie d'abord faisait 817 m autour de la place des Cordeliers : le plus long circuit fermé du fichier des voies tel qu'il était. Elle ne montrait pas Bellecour.

Ce que cette boucle n'est pas :

- un passage par la rue de la République elle-même : cette rue est piétonne et n'a pas de voie. La boucle la coupe place de la République et la longe ;
- un tour de la place Bellecour : le sol de la carte s'arrête 65 m après l'entrée de la place, seul son angle nord-est existe ;
- 70 % de grands axes selon le fichier des voies, qui dit 41 % : il compte une rue à sens unique par sa seule voie de 3 m. Entre les bordures mesurées sur le terrain (`carriageway.geojson`), 77 % de la boucle fait 7 m de large ou plus. `check --carriageway` compte ainsi.

## La ligne de conduite

Les voies sont des axes de rue décalés sur le côté. Leurs points s'écartent d'un mètre, et une voie commence un mètre ou deux à côté de la fin de la précédente. Les conducteurs lisent chacun de ces écarts comme un virage : un écart de 15 degrés vaut 42 km/h. Sur les points des voies, trois tours conduits avec les décisions du jeu font 40 km/h de moyenne et ne dépassent jamais 74, quelle que soit la vitesse demandée.

Chaque ligne du fichier porte donc la ligne de conduite de sa voie : une seule courbe fermée sur la boucle, un point tous les 3 m, les lignes droites redressées, les virages laissés à leur place et arrondis. Depuis la porte 86, cette ligne est ensuite détendue entre les murs de la ville (`tools/city/line_room.py`, `flight_circuit.py write --town <données traitées de la zone>`). La prise longue de la porte 86 a montré pourquoi : les virages « un peu arrondis » avaient 2 à 4 m de rayon dans cinq virages à droite, ce qu'aucune voiture ne tourne (la voiture volée arrivait à 74 km/h, braquait à fond à 15 km/h pendant quatre secondes, sortait large et était remise sur une voie, six fois en quatre-vingt-dix secondes) ; la ligne passait à 1,6 m d'un immeuble place de l'Hôpital ; et elle traversait le manège de la place de la République, un volume dont la position estimée tombe sur la chaussée de la rue Jean de Tournes. La ligne détendue est celle qui se courbe le moins en gardant chacun de ses points à moins de 4 m de la première ligne dans un virage (0,3 m en ligne droite), à 1,2 m à l'intérieur des bordures mesurées (`carriageway.geojson`) et à 2,8 m de tout mur (immeubles, monuments, eau) : un virage se prend par l'extérieur, se coupe à la corde et se quitte par l'extérieur, aussi large que la place le permet. Ce qui se tient sur la ligne est contourné du côté où rien d'autre ne se tient : à la porte 90 le manège, par le nord, sur la place, à 12 m de la voie. Depuis la porte 91, c'est le manège qui est 12 m plus au nord, sur la place (sa position était une estimation ; `lane_clear.LANDMARK_MOVES` déplace son acteur dans la carte de base, et `line_room.py` le lit là quoi que disent les données traitées), et la ligne redescend tout droit la rue Jean de Tournes. Le virage le plus serré de la boucle a maintenant 9,3 m de rayon (rue Herriot vers la rue Jean de Tournes), le demi-tour de l'approche 8,8 m, et aucun mur n'est à moins de 2,8 m. Avec le conducteur du jeu (`flight_circuit_test.cpp`), un tour prend 109 s au lieu de 122 et la première minute se roule à 58 km/h au lieu de 52. Pas encore roulé dans le jeu.

La ligne reste à moins de 8 m de sa voie, et commence et finit à moins de 16 m des extrémités de la voie (`GTPFlightCircuit::LineEndsMetres`, 12 m jusqu'à la porte 86). Une ligne du fichier dont la ligne de conduite contourne quelque chose porte `round` dans la dernière colonne et peut être à 16 m de sa voie ; le jeu n'y lit que `end`. `flight_circuit.py check` mesure la place de la ligne quand il trouve la ville à côté de `--carriageway` (ou avec `--town`) : le mur le plus proche, le virage le plus serré ; une ligne qui traverse un mur fait échouer le contrôle.

Chaque ligne porte aussi la vitesse que la fuite y demande (colonne `kmh`), à partir de 4 s après le vol : 100 km/h sur les quais, 75 rue de la Barre, rue Herriot et rue Carnot, 60 sur Bellecour et dans les rues Childebert, Jean de Tournes, Stella et Grolée, 50 rue Rivière et rue Bellecordière, 45 place de la République, 40 place de l'Hôpital. Une ligne sans vitesse suit la largeur que donne le fichier des voies : 100 km/h sur une chaussée de 12 m et plus, 75 sur un autre grand axe, 60 dans une rue moyenne.

Ce que dit la simulation (`flight_circuit_test.cpp` : les décisions du jeu, un modèle de voiture à deux roues, pas la physique du jeu) : sur la ligne un tour prend environ deux minutes, la première minute fait 52 à 53 km/h de moyenne, la pointe est à 100 km/h. Comme le film commence, à l'arrêt le long du trottoir puis par le demi-tour : 50 à 52 km/h sur la première minute, la place de la République vers 38 s, Bellecour vers 70 s, la place des Jacobins vers 85 s. Seule une porte sur le PC dit ce que fait la voiture du jeu.

## Les pédales dans les virages

Porte 83 : la voiture volée a quitté sa voie dans trois virages (quai vers rue Childebert, rue Childebert vers place de la République, rue Herriot vers rue Jean de Tournes) et a été remise sur une voie 6 fois dans une passe, 3 dans l'autre. Ses lignes, une par seconde, disent pourquoi. Le suiveur de voie abandonne les points d'une voie dès que le point le plus proche de la voiture est sur la suivante, et un virage placé tout au début d'un chemin n'est pas compté comme un virage : la vitesse voulue passe de 24 à 57 km/h au milieu du virage, et la voiture reçoit tout l'accélérateur roues braquées à fond (`lane=45 kmh=36 throttle=1.00 steer=1.00`, puis `kmh=52`, puis 27 m hors de sa voie).

Depuis, les pédales du script suivent sa direction (`GTPFlightPedals.h`) : au-delà d'un tiers du braquage demandé, pas d'accélérateur et un peu de frein tant que la voiture va plus vite que ce que ce braquage tient à 6 m/s² (15 km/h braquée à fond, 28 à un tiers). Sous un tiers du braquage rien ne change : sur le quai la voiture demande 0,14 du braquage à 98 km/h, et dans les lignes de la porte 83 elle n'en demande jamais un tiers au-dessus de 45 km/h. La direction reste telle que demandée. Les lignes `DemoDrive` montrent les pédales que la voiture reçoit.

La cause elle-même a disparu aussi, pour la fuite et pour les unités de police, qui conduisent avec le même suiveur de voie : une voie quittée garde ses 12 derniers mètres en tête du chemin du conducteur (`GTPDrive::KeptBehind`), et le virage reste mesuré avec le chemin qui le précède. `lane_change_corner_test.cpp` conduit les trois virages de trois façons : voie abandonnée entière, la vitesse voulue saute ; fin de voie gardée, le conducteur décide image par image comme celui dont le chemin n'est jamais coupé. Les voitures du trafic ont leur propre copie de ce code et abandonnent encore la voie entière.

Pas prouvé sur la voiture du jeu au moment d'écrire. Les deux tests conduisent les virages sur les lignes mêmes du circuit avec les décisions du jeu, mais la voiture de leur modèle tient les virages dans tous les cas : ils montrent les décisions, pas les 13 à 27 m dont la voiture du jeu a quitté sa voie.

## Où finit le film

Dans `-GTARun` le joueur se rend après l'étape de poursuite : la voiture s'arrête et la police vient à elle. À l'horloge seule, cela tombait 100 à 200 m avant la place Bellecour. Le fichier du circuit marque les voies où la fuite doit finir (colonne `mark`, valeur `end`) : les 65 derniers mètres de la rue de la Barre et les 30 m de la place Bellecour, le long du côté nord de la place. Une fois ses 40 s écoulées, l'étape de poursuite continue jusqu'à ce que la voiture soit sur l'une d'elles, 20 s de plus au maximum ; une voiture qui les a franchies avant la fin des 40 s s'arrête là où elle est. L'étape le dit : `ended after N s on the place the circuit ends on`, `past` ou `short of`.

## Quand la voiture est arrêtée

Le script remet encore la voiture sur une voie quand elle est bloquée ou durablement hors de sa voie, mais seulement sur une voie de la boucle, et chaque remise compte dans `DemoChase ... relocations=N`. Un film avec une remise sur voie n'est pas valable. Il ne la pose jamais dans une autre voiture ni, depuis la porte 83, sur quoi que ce soit d'autre qui arrête une voiture (un arceau à vélos, un muret, un arbre) : l'endroit est écarté, et la ligne de la remise dit combien l'ont été (`N place(s) passed over with something standing there`). Depuis la porte 86, un endroit où se tient une personne ou une voiture est écarté aussi, quoi qu'elles répondent au canal de collision d'une voiture : rue Grolée, la voiture était debout sur le nez deux secondes après avoir été posée, un passant dessous. Sur la boucle, la voiture est posée sur la ligne qu'elle y conduit, dans son sens, et non sur les points de la voie (la trace finit par `on its driving line`) : posée sur les points de la voie, à plusieurs mètres de sa ligne, elle repartait braquée à fond. Et le sol n'est jamais un obstacle : la sonde qui dit ce qui retient la voiture touchait la chaussée à la porte 86, deux tuiles du terrain ont été lues comme des obstacles « à 0 à 2 m » et leurs voies fermées ; elle regarde maintenant à l'horizontale à partir de 55 cm au-dessus de l'origine de la voiture et écarte une face tournée vers le haut (`the ground: not an obstacle`).

La prise longue de la porte 88 a montré ce qu'aucun de ces tests ne pouvait voir : une voiture du trafic lointain. Elle est dessinée et n'a pas de collision, donc aucun test de recouvrement ne la trouve sur place ; le joueur arrivant d'un coup à côté d'elle, le trafic lointain l'a passée en voiture conduite, collision comprise, dans la voiture volée tenue sur sa nouvelle voie, et la voiture volée était debout sur l'arrière 0,3 s après le retour de sa physique (la voiture grise est dans les images ; aucune ligne de ce journal ne la nomme). Trois règles depuis : le trafic lointain ne passe jamais une voiture en voiture conduite à moins de 7 m du joueur ; les voitures lointaines à moins de 15 m de l'endroit sont renvoyées quand la voiture y est posée et à chaque pas de la tenue (`N far traffic car(s) sent away from the place`) ; et la physique ne revient pas tant qu'une voiture est dans la voiture tenue : une voiture du trafic est retirée de la route, toute autre est attendue, 3 s au plus (`... is in the car held on its new lane`). Le journal nomme aussi ce que la voiture touche pendant ses deux premières secondes sur place (`it touches ... of ...`) et qui est dedans (`who is in the car`).

## À grande vitesse sur le quai

La prise longue de la porte 89 a perdu la voiture au même endroit dans ses deux fuites, comme la porte 83 : quai Jules Courmont, 99 km/h, la voiture à 1,4 m à gauche de sa ligne, à 4,8 m deux secondes plus tard, à travers douze passants de la promenade puis dans un arbre. Trois choses ont été trouvées, et chacune a sa règle depuis la porte 93.

Le fichier des voies place la voie de gauche du quai à 1,5 m de la bordure de la promenade, et l'approche la conduisait. Là où la fuite est demandée à 90 km/h ou plus, sa ligne garde maintenant 4,25 m des bordures, une voie de 3 m entre la voiture et la bordure, là où la chaussée est assez large et en son milieu sinon (`line_room.off_kerbs` ; les bordures sont celles rencontrées en travers de la ligne, le débouché d'une rue n'en est pas une). Sur le quai l'approche passe maintenant où passe le tour, dans la deuxième voie. La ligne garde sa voie du fichier : elle est à 3 m à côté.

À grande vitesse la voiture ne donne à ses roues qu'une part du braquage demandé, moins de la moitié à 100 km/h (`GTPSteeringCurve.h`, les clés avec lesquelles le véhicule est construit, faites pour un clavier). Le suiveur de voie calcule un angle de roues ; le script demande maintenant davantage à la voiture au-dessus de 50 km/h, et tout ce que la voiture retranche à partir de 80 km/h, pour que les roues tournent comme calculé (`GTPFlightPedals::SteeringSent`, `-DemoSteerAlone` l'annule). D'après la seule mesure disponible (3 degrés par seconde pour 0,12 demandé à 99 km/h), la voiture tourne encore moins que ses roues ne le disent : le prochain journal le dira.

Et le tirage des bosses. Une voiture abîmée d'un côté tire de ce côté : l'aide à la conduite de la voiture du joueur ajoutait ce tirage au manche du joueur et donnait la somme à la voiture à chaque image. Avec un script au volant le manche est à 0, donc le braquage du script était remplacé par le seul tirage dès la première bosse. L'aide ne touche plus au braquage tant qu'un script tient le volant (`AGTPVehicle::NoteScriptedSteering` ; le journal dit une fois `its dents pull the steering by ...; a script holds the wheel`). Lu dans le code : aucune ligne du journal de la porte 89 ne montre une bosse.

La ligne `DemoDrive`, une par seconde, porte maintenant ce qui manquait pour départager : `off` (mètres depuis la ligne conduite), `xy` et `yaw`, `pull` (ce que les bosses ajouteraient), et `sent` à côté de `steer` (donné à la voiture, et demandé par le suiveur de voie).

## Les gens sur la chaussée

La sonde du script fait 9 m. À 98 km/h, c'est un tiers de seconde : à la porte 86 le script a freiné à fond pour une personne à 0 m (`kmh=98 brake=1.00 limit=obstacle obstacle=0 m`), la voiture l'a heurtée à 86 km/h, est partie en travers et s'est arrêtée contre un arbre à 4,4 m de sa ligne. Depuis, la fuite ne freine pour une personne que si elle peut encore s'arrêter avant elle (7 m/s², 1 m de marge : sous 43 km/h pour une personne au bout de la sonde) ou si elle roule au pas ; sinon elle garde sa ligne et sa vitesse (`GTPFlightPedals::StopsForPerson`). La ligne `DemoDrive` le dit : `not braked for`. Les passants qui s'écartent d'une voiture, ce n'est pas encore fait.

## Choisir et vérifier un circuit

`tools/city/flight_circuit.py`, Python sans dépendance :

- `find <lanes.csv>` : tous les circuits fermés sans voie prise deux fois, le plus long d'abord, avec leur part de grands axes ;
- `write <lanes.csv> --lanes 381,367,... --approach 397,161,... --speeds "QU JULES COURMONT=100,..." --out <Carte>.circuit.csv --roads-out tools/city/through_roads/<focus>.txt` : le fichier du circuit avec ses lignes de conduite et ses vitesses, et la liste de ses routes pour `traffic.py` ;
- `check <circuit.csv> <lanes.csv> --carriageway carriageway.geojson --report lane_clear_report.json --png boucle.png` : le circuit face au fichier des voies que lit le jeu, et face au rapport de l'étape de préparation `lanes` : rien de ce que cette étape a seulement signalé (modules de façade, enseignes) ne doit se trouver sur une voie du circuit. Code de sortie 1 quand le circuit ne tient pas. À lancer avant chaque porte, sur le fichier des voies de cette porte.

Quand les routes du circuit changent : `write` sur un fichier des voies qui les contient (fait par `traffic.py` avec un fichier de routes traversantes qui les liste), puis de nouveau `traffic.py`, l'étape de préparation `lanes`, et `check`.

## Interrupteur

`-FlightCircuit=0` : la fuite est libre sur les voies, comme jusqu'à la porte 78.

`-DemoPedalsAlone` : les pédales ne suivent plus la direction, comme jusqu'à la porte 83.

`-LaneKeptBehind=0` : une voie quittée est abandonnée entière, comme jusqu'à la porte 83.

`-DemoStopsForPeople` : la fuite freine pour toute personne dans sa sonde, comme jusqu'à la porte 86.

## Changé aussi pour tous les conducteurs

La limite de vitesse à la fin du chemin connu comptait la distance à vol d'oiseau jusqu'à cette fin. Là où un chemin revient à côté de lui-même (une boucle, un quai monté d'un côté et descendu de l'autre), la fin se trouve à quelques mètres d'une voiture qui a encore 200 m à parcourir. La distance est maintenant comptée le long du chemin quand elle est plus longue (`GTPDriveLogic.h`).
