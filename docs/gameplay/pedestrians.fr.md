# Piétons

*Version anglaise : [pedestrians.md](pedestrians.md)*

Décision et alternatives : [ADR-0005](../adr/adr-0005-pedestrians.fr.md).

## Ce que voit le joueur

Des gens marchent sur les trottoirs et la rue piétonne, s'arrêtent de temps en
temps, attendent au bord du trottoir quand une voiture arrive à un passage,
s'écartent quand le joueur leur fonce dessus, courent devant une voiture lancée
sur eux, et sont renversés (ragdoll, 8 s) si elle les touche.

## Données

```
python3 tools/city/pedestrians.py --map L_City_RueDeLaRepublique
```

écrit `gen/<focus>/pedestrians/walkways.csv` (+ `.json`, vue de contrôle `.png`)
et le copie dans `GranTheftPraline/Content/City/Pedestrians/<Carte>.walkways.csv`
(non versionné, livré avec le jeu). Sans ce fichier, pas de piétons. Types
d'arêtes : 0 trottoir, 1 rue piétonne, 2 passage, 3 jonction.

## Exécution

`UGTPPedestrianSubsystem` (GTPPedestrians.h) se crée seul dans les mondes de
jeu. Réglages dans `DefaultGame.ini`, section `GTPPedestrianSubsystem` :

| Réglage | Défaut | Sens |
|---|---|---|
| DensityM1 / Fidelity / Ultra | 12 / 24 / 40 | personnes par profil graphique ; `-Pedestrians=N` force, 0 = aucun |
| MaxVisible | 10 | seuls les plus proches sont dessinés |
| DetailDistance, FarLOD | 20 m, 2 | ombre et LOD complet en deçà, LOD réduit au-delà |
| DrawDistance | 90 m | rien n'est dessiné au-delà |
| WalkAnimation, IdleAnimation, RunAnimation | vide | boucles jouées ; sans marche, pas de piétons |
| Meshes | vide | maillages squelettiques ; vide = celui du personnage joueur |

Trouver les assets candidats : `tools/pedestrians/find_walk_assets.py` (Python
de l'éditeur). Gratuit d'abord : le mannequin du Game Animation Sample et ses
boucles de locomotion ne coûtent rien ; MetaHuman ou maillages Fab pourront
être ajoutés dans `Meshes`.

Les autres systèmes écoutent `OnPedestrianHit` et `OnPedestrianScared` (lieu,
acteur cause).

Interface stable pour les autres systèmes (le système de recherche s'y abonne,
ce module ne dépend d'aucun d'eux) :

```cpp
DECLARE_MULTICAST_DELEGATE_TwoParams(FGTPPedestrianEvent, const FVector& Location, AActor* Cause);
UGTPPedestrianSubsystem::OnPedestrianHit;    // une personne est renversée
UGTPPedestrianSubsystem::OnPedestrianScared; // une personne se met à fuir
```
Cause est nulle pour l'instant (la voiture n'est pas encore identifiée).

## Crochets et variété

- `ScareAround(Lieu, Rayon, Cause)` fait fuir tous ceux du voisinage (tirs,
  explosions, police) et diffuse `OnPedestrianScared` pour chacun.
- `CrossingBlockedQuery` (`TFunction<bool(const FVector& Where, const FVector& WalkDirection)>`) permet aux feux de
  dire qu'un passage est rouge pour les piétons ; sans lui, ils ne surveillent
  que les voitures.
- Variété avec un seul maillage : la taille varie de `HeightSpread` et les
  matériaux du corps reçoivent une couleur de vêtement d'une petite palette
  urbaine via les paramètres vectoriels de `TintParameters` (ceux qu'un matériau
  n'a pas sont ignorés). D'autres maillages vont dans `Meshes`.

- Comportement de rue : un quart des gens marchent par deux (le partenaire reste à
  côté et un peu derrière et fait comme le meneur), et deux personnes à moins de
  90 cm se contournent par le côté (`Separate` dans `GTPWalkLogic.h`, vérifié à 12
  et 60 personnes par `walk_sim`).

- Arrêts : environ un tiers sont des appels téléphoniques (ils restent sur place,
  avec `PhoneAnimation` si elle est réglée), les autres du lèche-vitrines : ils
  vont vers la façade (`facade_side` du cheminement) et lui font face. Une voiture
  qui passe à moins de 3 m sur leur cheminement les fait s'écarter. De l'extérieur,
  `ScareAround` les effraie (le fil audio l'appelle pour tirs, explosions et chocs).

## Contrôles

- `python3 tools/pedestrians/check_walk_logic.py [walkways.csv]` compile
  `walk_sim` et fait marcher 200 personnes 10 minutes (pas de saut, elles
  continuent à marcher, elles fuient une menace). Tests Python :
  `tools/city/tests/test_pedestrians.py`.
- Coût sur le M1 Pro : pas encore mesuré (critères de l'ADR-0005). La ligne
  `LogGTPPedestrians: active=.. drawn=..` sort toutes les 10 s, aussi en Shipping.

## Limites

Pas d'évitement entre personnes, un seul maillage et un seul jeu d'animations,
ni assis ni scènes de boutique, pas de réaction aux tirs (les événements sont
prêts).

## Prêter des piétons (commerces, scènes)

`UGTPPedestrianSubsystem::AcquireFromPool(Location, YawDegrees)` prête un piéton du pool (immobile,
dessiné en plein détail, laissé tranquille par le pool) et `ReturnToPool(Actor)` le rend. On prend
d'abord un piéton libre, sinon le marcheur le plus loin du joueur (jamais un ragdoll). Renvoie
`nullptr` si les piétons sont coupés ou si personne n'est disponible. L'emprunteur joue sa propre
boucle avec `AGTPPedestrian::PlayLoop` et doit rendre l'acteur ; `GetBorrowedCount()` les compte.

## Rue vivante : panique, curiosité, événements de démo

- **La panique se propage.** Celui qui se met à courir (voiture rapide, coup de feu, accident) passe
  la panique une fois aux gens calmes dans `PanicRadius` (7 m, 65 % chacun, deux rangs au plus :
  `PanicMaxGeneration`) : un groupe se disperse comme un troupeau (`GTPWalk::CatchPanic`).
- **Curiosité.** Au-delà du rayon d'effroi d'un bruit fort (2,5 fois) et autour d'une voiture qui
  renverse quelqu'un (25 m), les gens s'arrêtent et regardent l'endroit 4 à 9 s (`GTPWalk::Gawk`, pause
  de type Gawk, face à l'événement), puis repartent. Pas ceux qui courent déjà, ni les ragdolls, ni
  les piétons prêtés.
- **Événements de démo**, pour filmer et mesurer : `-PedDemo=panic|gawk|shot` rejoue cet événement
  toutes les 25 s près du piéton dessiné le plus proche (le premier 8 s après le début).
- Vérifié hors moteur : `tools/pedestrians/check_walk_logic.py`.

## Foule dense et réactions (feat/pedestrian-crowd)

- Profil Ultra : `DensityUltra=120` personnages complets, `MaxVisibleUltra=80` dessinés à la fois, `FarCrowdUltra=300` silhouettes
  en instances au-delà du pool (`-PedFar=N`, `-PedMaxVisible=N`, `-Pedestrians=N` pour forcer). Les lointains deviennent des
  personnages complets à moins de `PromoteDistance`.
- Silhouettes : taille, largeur (`WidthSpread`), couleur de vêtements, personnages supplémentaires (`Meshes`).
- Réactions : panique qui se propage, badauds, mains en l'air sous la menace (réponse aux menaces active par défaut,
  `-ThreatResponse=0` la coupe ; la boucle vient de `tools/pedestrians/make_handsup_animation.py`), fuite, ragdoll au choc
  d'une voiture, attroupement autour d'un accident, le témoin le plus proche sort son téléphone (`CallPoliceChance` ; le crime
  arrive à la police par la logique de témoins du composant de recherche), quelques personnes ripostent (`FightBackChance`).
- Démos : `-PedDemo=panic|gawk|shot|holdup`.
