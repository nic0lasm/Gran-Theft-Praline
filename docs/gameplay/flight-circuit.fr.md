# Le circuit de la fuite filmée

[English](flight-circuit.md)

Le parcours automatique (`-GTARun`, `-PoliceChaseDemo`) vole une voiture quai Jean Moulin et s'enfuit. Jusqu'à la porte 78 la fuite était libre sur les voies de la carte : à chaque carrefour, la sortie qui offrait la meilleure suite. N'importe quel mur, poteau ou trou de tout le centre pouvait l'arrêter, et le script remettait la voiture sur une voie 4 à 8 fois en quatre minutes filmées. Désormais la fuite suit un circuit, choisi hors du jeu et gardé dans Git. La police poursuit librement.

## Le fichier

`GranTheftPraline/Content/Test/Circuits/<Carte>.circuit.csv`, une ligne par voie : où la voie commence et finit (cm Unreal), son nom, et la ligne de conduite suivie sur elle. Le numéro de la voie dans le fichier des voies n'est qu'un indice : le jeu retrouve la voie de chaque ligne par ses deux extrémités, à 3 m près, et le circuit survit donc à un fichier des voies refait avec d'autres numéros. Les lignes placées sous `approach` sont parcourues une fois, du vol à la boucle ; celles placées sous `lap` sont parcourues tour après tour.

Le jeu vérifie le fichier au démarrage de la carte (`GTPFlightCircuit.h`) : chaque voie mène à la suivante, la dernière de la boucle à la première, la dernière de l'approche à une voie de la boucle. Un fichier qui ne correspond pas aux voies lues n'est pas utilisé, la fuite est libre comme avant, et le journal dit pourquoi (`traffic: flight circuit NOT USED: ...`).

## La boucle de L_City_RueDeLaRepublique

Les voies de cette carte ne sont créées qu'à moins de 60 m d'un immeuble généré. Le plus grand ensemble de voies d'où l'on peut revenir au lieu du vol compte 33 voies autour de la place des Cordeliers ; ses circuits fermés font de 214 à 817 m. La boucle retenue est le plus long : 817 m, 21 voies, 75 % de grands axes (chaussée de 7 m et plus), aucune rue étroite. Quai Jean Moulin vers le nord, demi-tour à la rue Gentil, les quais vers le sud, rue Saint-Bonaventure, rue Grolée vers le nord, place des Cordeliers vers l'est. Deux tours font 1,6 km, trois tours 2,4 km.

L'approche est la voie du milieu du quai Jean Moulin depuis le lieu du vol, 189 m tout droit, jusqu'à la boucle au demi-tour de la rue Gentil. Sans elle, le chemin le plus court vers la boucle était un demi-tour au premier carrefour.

Ce que cette boucle n'est pas : une boucle unique de 1,5 à 2,5 km, ni un passage par la rue de la République. Cette rue est piétonne et n'a pas de voie, et les voies autour de la place Bellecour sont coupées des quais dans le fichier des voies d'aujourd'hui. Une boucle plus longue demande un autre fichier des voies.

## La ligne de conduite

Les voies sont des axes de rue décalés sur le côté. Leurs points s'écartent d'un mètre, et une voie commence un mètre ou deux à côté de la fin de la précédente. Les conducteurs lisent chacun de ces écarts comme un virage : un écart de 15 degrés vaut 42 km/h. Sur les points des voies, trois tours conduits avec les décisions du jeu font 40 km/h de moyenne et ne dépassent jamais 74, quelle que soit la vitesse demandée.

Chaque ligne du fichier porte donc la ligne de conduite de sa voie : une seule courbe fermée sur la boucle, un point tous les 3 m, les lignes droites redressées, les virages laissés à leur place et arrondis (0,9 m coupé à l'intérieur d'un virage à droite, dont l'intérieur est le trottoir ; 2,5 m à l'intérieur d'un virage à gauche). La ligne reste à moins de 4,5 m de sa voie. Sur le circuit la fuite demande 100 km/h sur un quai ou un boulevard (chaussée de 12 m et plus), 75 sur un autre grand axe, 60 dans une rue moyenne, à partir de 4 s après le vol.

Ce que dit la simulation (`flight_circuit_test.cpp` : les décisions du jeu, un modèle de voiture à deux roues, pas la physique du jeu) : sur la ligne un tour prend 51 à 55 s, la première minute fait 52 à 56 km/h de moyenne, la pointe est à 100 km/h. Comme le film commence, à l'arrêt le long du trottoir : 49 à 53 km/h sur la première minute. Seule une porte sur le PC dit ce que fait la voiture du jeu.

## Quand la voiture est arrêtée

Le script remet encore la voiture sur une voie quand elle est bloquée ou durablement hors de sa voie, mais seulement sur une voie de la boucle, et chaque remise compte dans `DemoChase ... relocations=N`. Un film avec une remise sur voie n'est pas valable.

## Choisir et vérifier un circuit

`tools/city/flight_circuit.py`, Python sans dépendance :

- `find <lanes.csv>` : tous les circuits fermés sans voie prise deux fois, le plus long d'abord, avec leur part de grands axes ;
- `write <lanes.csv> --lanes 214,7,... --approach 188,215,... --out <Carte>.circuit.csv` : le fichier du circuit avec ses lignes de conduite ;
- `check <circuit.csv> <lanes.csv> --report lane_clear_report.json --png boucle.png` : le circuit face au fichier des voies que lit le jeu, et face au rapport de l'étape de préparation `lanes` : rien de ce que cette étape a seulement signalé (modules de façade, enseignes) ne doit se trouver sur une voie du circuit. Code de sortie 1 quand le circuit ne tient pas. À lancer avant chaque porte, sur le fichier des voies de cette porte.

## Interrupteur

`-FlightCircuit=0` : la fuite est libre sur les voies, comme jusqu'à la porte 78.

## Changé aussi pour tous les conducteurs

La limite de vitesse à la fin du chemin connu comptait la distance à vol d'oiseau jusqu'à cette fin. Là où un chemin revient à côté de lui-même (une boucle, un quai monté d'un côté et descendu de l'autre), la fin se trouve à quelques mètres d'une voiture qui a encore 200 m à parcourir. La distance est maintenant comptée le long du chemin quand elle est plus longue (`GTPDriveLogic.h`).
