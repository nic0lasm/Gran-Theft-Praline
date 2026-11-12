# Réactions du trafic

Les conducteurs du trafic réagissent à la rue, en plus de la conduite de base (voies, feux, carrefours, distances, arrêt devant une personne). Règles : `GTPTrafficReactLogic.h`, sans type moteur, vérifiées par `tools/vehicles/react_test` dans la CI. Branchement : `UGTPAIDriverComponent::UpdateReactions` (`GTPTraffic.cpp`).

| État | Quand | Ce que fait le conducteur |
|---|---|---|
| Attente | arrêté derrière quelque chose qui ne bouge pas : le joueur, sa voiture, une épave, une voiture abandonnée, une personne plantée sur la chaussée, une voiture elle-même en attente | premier coup de klaxon au bout de 2,5 s (1 s pour le joueur), puis un toutes les 3 s |
| Contournement | bloqué 5 s, côté gauche libre sur une largeur de voie, de 5 m derrière à 25 m après l'obstacle | se décale de 2,9 m à gauche, à moins de la moitié de la vitesse, revient dans sa voie quand la route devant est libre depuis 2 s (9 s au plus) |
| Panique | un coup de feu à moins de 35 m, ou une arme pointée sur le conducteur en mouvement (20 m, 10°) | klaxon, 1,6 fois la vitesse de la voie pendant 8 s, feux rouges et priorités ignorés |
| Fuite | une arme pointée 1 s sur le conducteur arrêté ou lent | s'arrête, sort (saute au-dessus de 40 km/h) et s'enfuit à l'opposé de l'arme ; la voiture reste, libre à prendre |
| Choc | la voiture du joueur la percute | un long coup de klaxon, reste immobile 2,5 s, repart |

Une file au feu rouge n'est pas « immobile » : le conducteur attend sans rien dire. Le klaxon est synthétisé (`GTPHornSynth.h`, deux tons vers 410 et 512 Hz, une hauteur propre à chaque voiture) et ne sonne qu'à moins de 80 m de la caméra. Les voitures fantômes (lointaines) ne réagissent pas.

## Interrupteurs et réglages

- `-TrafficReact=0` : aucune réaction (A/B pour les mesures).
- `-TrafficReactDemo=S` : toutes les S secondes, la voiture conduite la plus proche, entre 10 et 50 m et dans le champ, est tour à tour visée, percutée et prise sous le feu. Le journal écrit `traffic react demo: <événement> on <voiture>`.
- Section `[/Script/GranTheftPraline.GTPTrafficNetwork]` de `DefaultGame.ini` : `ReactHonkAfter`, `ReactBypassAfter`, `ReactPanicSeconds`, `ReactAimRange`, `ReactAimHalfAngle`, `ReactShotRange`, `ReactHornDistance`.
- Chaque changement d'état est journalisé : `traffic react: <voiture> <avant> -> <après> (ahead <acteur> <m>, <km/h>)`.

## Scènes de démo pour le PC

1. Réactions à tour de rôle : `-TrafficReactDemo=12`, debout dans une rue passante, 120 s. Attendu : `panic` ou `flee` après `aim`, `shaken` après `ram`, `panic` après `shot`, et des klaxons audibles.
2. Voie bloquée : `-TestDrive=hold-gentil -TestDriveMode=rail` immobilise la voiture du joueur 90 s au départ du trajet Gentil vers Bourse. Attendu, si du trafic arrive derrière : `cruise -> waiting`, des klaxons, puis `waiting -> bypass` quand la gauche est libre.
3. Coût : le même street-demo avec et sans `-TrafficReact=0`. Attendu : aucun écart GPU mesurable, moins de 0,1 ms de plus sur le thread de jeu (un balayage latéral seulement quand un contournement se décide).

## Ce qui manque encore face à la cible

Plusieurs modèles de voitures (une seule Clio aujourd'hui : ce sont des assets, pas du code), les changements de voie sur les axes à plusieurs voies, la priorité aux piétons sur les passages sans feux, et un conducteur qui reprend une voiture laissée par le joueur.
