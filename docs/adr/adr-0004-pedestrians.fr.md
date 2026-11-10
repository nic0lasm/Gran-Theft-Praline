# ADR-0004 : Piétons

*Version anglaise : [adr-0004-pedestrians.md](adr-0004-pedestrians.md)*

## Statut

Proposé le 2026-11-10 (la première mesure sur le M1 Pro tranche).

## Date

2026-11-10

## Décideurs

nic0lasm (responsable du projet)

## Résumé

Des piétons marchent sur les trottoirs de la rue de la République, sur un
graphe de cheminements tiré de la BD TOPO, mus par de petites règles sans
moteur et affichés par un pool fixe de maillages squelettiques peu coûteux.
Mass reste l'option si l'on veut des centaines de personnes.

## Contexte

La rue doit sembler vivante, façon GTA : des gens qui marchent, s'arrêtent
devant une vitrine, attendent à un passage, s'écartent, fuient une voiture
rapide. Le budget est déjà tendu (ADR-0002 : p95 <= 42 ms sur le profil M1) et
six conducteurs animés coûtent environ 10 ms de GPU en version 0.3. Mass
(MassCrowd, ZoneGraph, StateTree) vise des milliers d'agents et demande un
graphe de zones dans la carte, binaire que plusieurs fils modifient ; sa
couche de représentation resterait à régler pour Metal.

## Décision

1. **Cheminements :** `tools/city/pedestrians.py` construit un graphe non orienté
   depuis les mêmes routes BD TOPO que le masque de sol : un cheminement de
   chaque côté d'une rue pour voitures, trois sur une rue piétonne, des liens
   aux carrefours marqués « crossing » ou « join ». C'est une donnée (CSV livré
   avec le jeu), pas du contenu de carte.
2. **Comportement :** `GTPWalkLogic.h`, sans type Unreal, vérifié hors éditeur
   par `tools/pedestrians/walk_sim` (même méthode que les conducteurs IA) :
   marche aléatoire qui préfère les trottoirs, pauses, attente à un passage si
   une voiture approche, fuite le long du graphe loin d'une menace.
3. **Exécution :** `UGTPPedestrianSubsystem` (sous-système de monde : rien à
   changer dans la carte ni le game mode) tient un pool dimensionné par le
   profil graphique (12 en m1), le déplace dans un seul tick, et fait
   apparaître ou disparaître les gens hors de vue seulement.
4. **Limites de coût :** une animation en boucle par personne (pas de Blueprint
   d'animation), au plus `MaxVisible` dessinés (les plus proches), ni ombre ni
   LOD fin au-delà de 25 m, rien au-delà de 90 m, les corps cachés ne sont pas
   posés.
5. **Événements pour les autres systèmes :** `OnPedestrianHit` et
   `OnPedestrianScared` sur le sous-système (lieu, cause), pour la police et le
   niveau de recherche.

## Alternatives envisagées

- Mass crowd + StateTree tout de suite : plus extensible, mais un graphe de
  zones dans la carte, des plugins plus lourds et un coût Metal non mesuré ; le
  graphe et les événements d'ici se reportent sur une foule Mass si la 0.9 en
  demande des centaines.
- Personnages avec CharacterMovement et motion matching (comme le joueur) :
  environ dix fois plus cher par personne.
- Maillages animés par sommets et instanciés : le moins cher pour la foule
  lointaine, mais sans réaction aux chocs et avec une préparation par maillage ;
  à revoir pour le lointain.

## Conséquences

- Les gens se traversent (pas d'évitement mutuel, seulement un pas de côté
  devant le joueur) ; l'étalement latéral le rend rare.
- Un seul maillage et un seul jeu d'animations au début : la variété demande
  d'autres maillages (config Meshes) puis des matériaux teintés.
- Le coût n'est pas mesuré avant un créneau Mac : le critère ci-dessous fixe la
  densité par défaut.

## Critères de validation

- `tools/pedestrians/check_walk_logic.py` PASS sur les vrais cheminements de la rue.
- Build m1 empaquetée, 3 minutes à pied dans la rue avec 12 personnes : p95 <=
  42 ms et au plus +2 ms de GPU face à la même passe avec `-Pedestrians=0`.
- Une voiture à 50 km/h sur des gens sur la chaussée les fait courir ; sur un
  passage, elle les fait attendre.

## Liens

ADR-0002 (budget d'image), ADR-0003 (déplacements), docs/gameplay/pedestrians.fr.md.
