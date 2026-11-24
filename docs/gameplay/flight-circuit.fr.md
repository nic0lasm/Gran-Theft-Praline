# Le circuit de la fuite filmée

[English](flight-circuit.md)

Le parcours automatique (`-GTARun`, `-PoliceChaseDemo`) vole une voiture quai Jean Moulin et s'enfuit. Jusqu'à la porte 78 la fuite était libre sur les voies de la carte : à chaque carrefour, la sortie qui offrait la meilleure suite. N'importe quel mur, poteau ou trou de tout le centre pouvait l'arrêter, et le script remettait la voiture sur une voie 4 à 8 fois en quatre minutes filmées. Désormais la fuite suit un circuit, choisi hors du jeu et gardé dans Git. La police poursuit librement.

## Le fichier

`GranTheftPraline/Content/Test/Circuits/<Carte>.circuit.csv`, une ligne par voie : où la voie commence et finit (cm Unreal), son nom, et la ligne de conduite suivie sur elle. Le numéro de la voie dans le fichier des voies n'est qu'un indice : le jeu retrouve la voie de chaque ligne par ses deux extrémités, à 3 m près, et le circuit survit donc à un fichier des voies refait avec d'autres numéros. Les lignes placées sous `approach` sont parcourues une fois, du vol à la boucle ; celles placées sous `lap` sont parcourues tour après tour.

Le jeu vérifie le fichier au démarrage de la carte (`GTPFlightCircuit.h`) : chaque voie mène à la suivante, la dernière de la boucle à la première, la dernière de l'approche à une voie de la boucle. Un fichier qui ne correspond pas aux voies lues n'est pas utilisé, la fuite est libre comme avant, et le journal dit pourquoi (`traffic: flight circuit NOT USED: ...`).

## La boucle de L_City_RueDeLaRepublique

La boucle passe par la place Bellecour, la vitrine de la ville : 1793 m, 37 voies. Le quai Jules Courmont vers le sud (346 m), la rue Childebert, la place de la République et son bassin, la rue Marcel-Gabriel Rivière, la place de l'Hôpital, la rue Bellecordière, la rue de la Barre, l'angle nord-est de la place Bellecour, la rue du Président Édouard Herriot vers le nord en passant la place des Jacobins, la rue Jean de Tournes, la rue Stella, la rue Grolée, la rue du Président Carnot, puis à droite de nouveau sur le quai.

Le vol a lieu quai Jean Moulin, côté qui monte vers le nord, 400 m au nord de la boucle. L'approche (488 m) fait demi-tour par le carrefour du pont Lafayette et descend le quai jusqu'à la rue Childebert.

Jusqu'à la porte 80 le fichier des voies ne pouvait pas porter cette boucle, pour deux raisons, toutes deux dans `tools/city/traffic.py` :

- un tronçon de 2,7 m de la rue Herriot, place des Jacobins, était écarté comme trop court, ce qui coupait la rue. Un tronçon de route entier est maintenant gardé à partir de 0,6 m ;
- les voies n'étaient créées qu'à moins de 60 m d'un immeuble généré, ce qui écartait 650 m de cette boucle (la rue Herriot au nord des Jacobins, la rue Jean de Tournes, le quai entre la rue Ferrandière et la rue Childebert, l'est de la rue Childebert). Les routes du circuit sont maintenant listées dans `tools/city/through_roads/<focus>.txt` et gardées entières dans l'étendue de la carte. Le fichier des voies passe de 229 à 255 voies.

La boucle choisie d'abord faisait 817 m autour de la place des Cordeliers : le plus long circuit fermé du fichier des voies tel qu'il était. Elle ne montrait pas Bellecour.

Ce que cette boucle n'est pas :

- un passage par la rue de la République elle-même : cette rue est piétonne et n'a pas de voie. La boucle la coupe place de la République et la longe ;
- un tour de la place Bellecour : le sol de la carte s'arrête 65 m après l'entrée de la place, seul son angle nord-est existe ;
- 70 % de grands axes selon le fichier des voies, qui dit 41 % : il compte une rue à sens unique par sa seule voie de 3 m. Entre les bordures mesurées sur le terrain (`carriageway.geojson`), 77 % de la boucle fait 7 m de large ou plus. `check --carriageway` compte ainsi.

## La ligne de conduite

Les voies sont des axes de rue décalés sur le côté. Leurs points s'écartent d'un mètre, et une voie commence un mètre ou deux à côté de la fin de la précédente. Les conducteurs lisent chacun de ces écarts comme un virage : un écart de 15 degrés vaut 42 km/h. Sur les points des voies, trois tours conduits avec les décisions du jeu font 40 km/h de moyenne et ne dépassent jamais 74, quelle que soit la vitesse demandée.

Chaque ligne du fichier porte donc la ligne de conduite de sa voie : une seule courbe fermée sur la boucle, un point tous les 3 m, les lignes droites redressées, les virages laissés à leur place et arrondis (0,9 m coupé à l'intérieur d'un virage à droite, dont l'intérieur est le trottoir ; 2,5 m à l'intérieur d'un virage à gauche). La ligne reste à moins de 4,5 m de sa voie, et commence et finit à moins de 12 m des extrémités de la voie.

Chaque ligne porte aussi la vitesse que la fuite y demande (colonne `kmh`), à partir de 4 s après le vol : 100 km/h sur les quais, 75 rue de la Barre, rue Herriot et rue Carnot, 60 sur Bellecour et dans les rues Childebert, Jean de Tournes, Stella et Grolée, 50 rue Rivière et rue Bellecordière, 45 place de la République, 40 place de l'Hôpital. Une ligne sans vitesse suit la largeur que donne le fichier des voies : 100 km/h sur une chaussée de 12 m et plus, 75 sur un autre grand axe, 60 dans une rue moyenne.

Ce que dit la simulation (`flight_circuit_test.cpp` : les décisions du jeu, un modèle de voiture à deux roues, pas la physique du jeu) : sur la ligne un tour prend environ deux minutes, la première minute fait 52 à 53 km/h de moyenne, la pointe est à 100 km/h. Comme le film commence, à l'arrêt le long du trottoir puis par le demi-tour : 50 à 52 km/h sur la première minute, la place de la République vers 38 s, Bellecour vers 70 s, la place des Jacobins vers 85 s. Seule une porte sur le PC dit ce que fait la voiture du jeu.

## Quand la voiture est arrêtée

Le script remet encore la voiture sur une voie quand elle est bloquée ou durablement hors de sa voie, mais seulement sur une voie de la boucle, et chaque remise compte dans `DemoChase ... relocations=N`. Un film avec une remise sur voie n'est pas valable.

## Choisir et vérifier un circuit

`tools/city/flight_circuit.py`, Python sans dépendance :

- `find <lanes.csv>` : tous les circuits fermés sans voie prise deux fois, le plus long d'abord, avec leur part de grands axes ;
- `write <lanes.csv> --lanes 381,367,... --approach 397,161,... --speeds "QU JULES COURMONT=100,..." --out <Carte>.circuit.csv --roads-out tools/city/through_roads/<focus>.txt` : le fichier du circuit avec ses lignes de conduite et ses vitesses, et la liste de ses routes pour `traffic.py` ;
- `check <circuit.csv> <lanes.csv> --carriageway carriageway.geojson --report lane_clear_report.json --png boucle.png` : le circuit face au fichier des voies que lit le jeu, et face au rapport de l'étape de préparation `lanes` : rien de ce que cette étape a seulement signalé (modules de façade, enseignes) ne doit se trouver sur une voie du circuit. Code de sortie 1 quand le circuit ne tient pas. À lancer avant chaque porte, sur le fichier des voies de cette porte.

Quand les routes du circuit changent : `write` sur un fichier des voies qui les contient (fait par `traffic.py` avec un fichier de routes traversantes qui les liste), puis de nouveau `traffic.py`, l'étape de préparation `lanes`, et `check`.

## Interrupteur

`-FlightCircuit=0` : la fuite est libre sur les voies, comme jusqu'à la porte 78.

## Changé aussi pour tous les conducteurs

La limite de vitesse à la fin du chemin connu comptait la distance à vol d'oiseau jusqu'à cette fin. Là où un chemin revient à côté de lui-même (une boucle, un quai monté d'un côté et descendu de l'autre), la fin se trouve à quelques mètres d'une voiture qui a encore 200 m à parcourir. La distance est maintenant comptée le long du chemin quand elle est plus longue (`GTPDriveLogic.h`).
