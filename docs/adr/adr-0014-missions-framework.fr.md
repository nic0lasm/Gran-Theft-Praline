# ADR-0014 : cadre de missions et d'activités, données en tables

*Version anglaise : [adr-0014-missions-framework.md](adr-0014-missions-framework.md)*

- Statut : proposé (11/11/2026) ; accepté quand les trois activités de la 0.9 tournent sur M1 et PC à travers la porte de fusion
- Responsables : fil Missions ; consomme Police (étoiles), Boutiques/économie/sauvegarde (argent, SaveGame, intérieurs), Combat (menace), Trafic et véhicules
- Liens : ADR-0004 (recherche), ADR-0010 (cellules), ADR-0012 (intérieurs), ADR-0013 (SaveGame), docs/gameplay/missions.fr.md
- Numérotation : 0011 combat, 0012 intérieurs, 0013 SaveGame sont pris par d'autres fils ; celui-ci prend 0014.

## Contexte

La session de test 0.9 a besoin d'un but : un cadre de missions minimal et trois activités
(braquer la boutique, voler une voiture sur commande, semer la police). Cadre complet et six
activités en 0.12. Aucun scénario ni personnage : l'histoire s'écrit plus tard avec Nico.

Contraintes : aucun coût moteur global (la porte de fusion n'autorise pas de régression), chaque
système a un interrupteur (`-Missions=0`), textes uniquement par tables de chaînes (FR/EN), logique
testable hors Unreal. Le fil lit Police, économie, combat et trafic sans les modifier.

## Options

**A. Blueprint ou script par mission.** Rapide au départ, mais les missions deviennent du code,
la CI ne peut pas les vérifier, et la sauvegarde ou la reprise ne sont pas uniformes.

**B. Plugin de quêtes ou assets Unreal (graphe UDataAsset).** Beaucoup de travail dans l'éditeur,
fichiers binaires non diffables dans Git, impossible à tester sans le moteur.

**C. Tables (CSV) lues par une petite machine à états sans moteur, avec un adaptateur Unreal.**
Missions et objectifs sont des lignes ; l'exécuteur est du C++ pur qui reçoit un contexte à chaque
tick et les événements au fil de l'eau, et renvoie des commandes. Diffable, vérifiable en CI, et
tous les autres systèmes ne sont atteints que par l'adaptateur.

## Décision (proposée)

**C.**

1. **Tables** dans `Content/Data/Missions/` : `missions.csv` (id, genre, clés de titre et de
   résumé, zone de départ, répétable, délai de reprise, récompense en argent, limite de temps,
   politique de reprise, effets au départ, prérequis), `objectives.csv` (lignes ordonnées par
   mission : type, cible, paramètres, clé d'indice, point de reprise, limite de temps, événements
   d'échec), `zones.csv` (mètres locaux de la ville et rayon, calés sur
   des sommets de rues réelles), `strings.tsv` (textes FR/EN, à intégrer à `GTPStringTable.h` par le fil Interface).
   L'analyseur refuse toute incohérence (zone inconnue, trou d'indice, nombre faux, type inconnu)
   et la CI le lance sur les tables livrées.
2. **Types d'objectif** : GoTo, EnterInterior, WaitEvent, EnterVehicle, DeliverVehicle,
   LoseWanted. Une nouvelle activité = de nouvelles lignes ; un nouveau type = un `case` et un test.
3. **Machine à états** (`GTPMissionRunner.h`) : Idle → Active → Succeeded | Failed ; un échec
   attend Retry (reprise au dernier point de reprise, ou depuis le début, ou aucune, selon la
   mission) ou Abort. L'objectif 0 est toujours un point de reprise. Échecs systématiques :
   `player.arrested`, `player.died`. Une seule mission active à la fois.
4. **Aucun couplage.** L'exécuteur n'appelle jamais un autre système. Il lit un contexte (position,
   véhicule, étoiles, intérieur) et des événements (`holdup.cash_taken`, `player.arrested`,
   `vehicle.destroyed`…) et émet des commandes (afficher l'objectif, point de passage, forcer des
   étoiles, faire apparaître un véhicule, point de reprise, récompense, résultat). L'adaptateur
   Unreal (`UGTPMissionSubsystem`, écrit quand les interfaces sont sur `next`) traduit dans les deux
   sens : il s'abonne aux délégués des autres fils et exécute les commandes par leurs API publiques.
5. **Récompense et sauvegarde.** Un succès incrémente le compteur et émet `GiveReward` avec une
   clé de passage `<mission>#<compteur>` dans le même pas, et marque la sauvegarde sale. L'économie
   refuse une clé déjà payée ; SaveGame (ADR-0013, écriture atomique) stocke `FSaveData`
   (compteurs, délais, mission en cours et point de reprise). Charger une sauvegarde reprend la
   mission en cours à son point de reprise. Rien de plus fin n'est stocké.
6. **Interrupteur** : `-Missions=0` ne crée aucun sous-système. Coût au repos : une recherche de
   table par tick d'une mission active, rien sans mission (cible 0 ms).

## Conséquences

- Positif : les nouvelles activités sont des données ; toute la logique est testée en CI en quelques
  millisecondes ; sauvegarde et reprise uniformes ; aucun autre fil modifié.
- Négatif : les missions à embranchements de la 0.12 demanderont des conditions en table ou un
  second niveau d'exécuteur ; volontairement hors 0.9.
- Dépendances à confirmer avec leurs propriétaires : les noms d'événements et le moyen, via
  `ReportCrime`, de monter les étoiles au départ de la fuite (police), `AddMoney(montant, raison,
  clé)` idempotent (boutiques/économie), les événements `holdup.*` (intérieur de référence),
  recherche de véhicule par étiquette près d'une zone (trafic/voitures garées).
