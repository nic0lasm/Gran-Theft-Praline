# ADR-0003 : Déplacement (à pied, en voiture, conducteurs PNJ)

*Version anglaise : [adr-0003-movement.md](adr-0003-movement.md)*

## Statut

Accepté le 2026-11-10 pour la version 0.3 (applique le socle choisi par le
chef de projet le 2026-11-09 : briques Epic gratuites, police et boutiques
maison).

## Date

2026-11-10

## Décideurs

nic0lasm (chef de projet)

## Résumé

Comment bougent le joueur et les voitures : le personnage garde
`CharacterMovement` avec le Game Animation Sample (motion matching) ; les
voitures utilisent Chaos Vehicles avec réglages en C++ ; les conducteurs PNJ
suivent des voies tirées de la BD TOPO avec un en-tête de décision sans
types moteur, simulé hors Unreal.

## Contexte

La 0.3 doit permettre de voler une voiture garée ou d'en sortir le
conducteur, puis de rouler de la rue de la République au Rhône. Unreal 5.8
propose plusieurs systèmes : `CharacterMovement` (mûr), `Mover`
(expérimental), Chaos Vehicles (marqué expérimental en 5.8.2 mais seule
physique de véhicule maintenue), et pour le trafic MassTraffic / ZoneGraph
(City Sample, lourd, écarté comme base dans le recensement). Machine cible :
M1 Pro, 30 i/s (ADR-0002).

## Décision

1. **À pied :** `CharacterMovement` + motion matching GASP, comme livré en
   0.1. `Mover` sera réexaminé quand il ne sera plus expérimental.
2. **Voitures :** Chaos Vehicles (`AWheeledVehiclePawn`), réglage de base
   dans `AGTPVehicle` (citadine, traction, boîte automatique), chaque modèle
   en Blueprint enfant (docs/gameplay/vehicles.fr.md).
3. **Conducteurs PNJ :** voies tirées de la BD TOPO
   (`tools/city/traffic.py`), direction en poursuite pure et règles de
   vitesse dans `GTPDriveLogic.h`, sans types moteur, donc vérifié par
   `tools/vehicles/traffic_sim` sur les vraies voies. Les voitures
   apparaissent et disparaissent hors de vue. Pas de navmesh.
4. **Son moteur :** synthétisé à partir du régime Chaos
   (`GTPEngineSynth.h`), vérifié hors Unreal, en attendant des boucles
   enregistrées.

## Alternatives examinées

- `Mover` pour le personnage : expérimental, GASP repose sur
  `CharacterMovement`.
- MassTraffic / ZoneGraph pour le trafic : conçus pour des milliers de
  voitures et le réseau du City Sample ; trop lourds à adapter pour 6
  voitures en 0.3. Gardés en option pour la 0.9 (centaines de voitures) si
  nos voies ne suivent pas.
- Arbres de comportement et navmesh pour les conducteurs : plus durs à
  tester sans l'éditeur, et un navmesh sur toute la ville coûte de la
  mémoire sur le M1.

## Conséquences

- L'IA de conduite et le son moteur se développent et se testent dans le
  cloud ; seuls les réglages et les mesures demandent le Mac.
- Priorités aux carrefours, feux et changements de file manquent jusqu'à la
  0.9 ; un léger contact à un carrefour reste possible.
- Chaos Vehicles peut changer d'une version du moteur à l'autre : revérifier
  les réglages après chaque mise à jour.

## Critères de validation

- `traffic_sim` réussi sur les voies de la map courante (écart à la voie
  p95 < 1,5 m, p99 < 3 m, vitesse moyenne > 12 km/h).
- Dans le jeu empaqueté : éjecter le conducteur d'une voiture qui roule,
  conduire 3 minutes, 95 % des images ≤ 33,3 ms en profil M1 en conduisant
  avec 6 voitures PNJ.

## Liens

ADR-0002 (budget d'image), docs/gameplay/vehicles.fr.md, production/roadmap.fr.md (0.3).
