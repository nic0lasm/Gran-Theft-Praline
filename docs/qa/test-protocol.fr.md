# Protocole de test de la build 0.9 du quartier

*Version anglaise : [test-protocol.md](test-protocol.md).*

Public : Nico et quelques proches, sur macOS (Apple silicon) et Windows. La
build est en cours de travail : le but est de trouver ce qui bloque le jeu,
pas de juger l'art final.

## Avant la session

1. Ouvrir la build avec [test-build-instructions.fr.md](test-build-instructions.fr.md).
2. Noter votre machine : système, puce ou processeur, carte graphique, mémoire.
3. Lancer une fois et laisser finir les shaders du premier lancement.

## Session (environ 1 heure)

| Temps | Quoi | À noter |
| ---- | ---- | ---- |
| 0-5 min | Marche libre, regarder autour, essayer les commandes et remapper une touche | Ce qui est confus, ce qui manque |
| 5-15 min | Scénario A puis B | Voir plus bas |
| 15-30 min | Scénarios C et D | |
| 30-50 min | Jeu libre : aller où l'on veut dans le quartier, à pied et en voiture, de jour, de nuit, sous la pluie | Saccades, chutes, endroits bloquants, rendus bizarres |
| 50-60 min | Quitter par le menu ; appuyer une fois sur F9 avant de quitter | |

## Scénarios

Chaque scénario a une ligne de **réussite**. Noter réussi, échoué ou bloqué,
et l'heure de l'horloge du jeu.

| N° | Scénario | Réussi quand |
| ---- | ---- | ---- |
| A | **Trouver son chemin.** Ouvrir la carte plein écran, lire un vrai nom de rue, poser une destination, la suivre au GPS à pied | On arrive sans s'écarter du tracé de plus de quelques pas |
| B | **Braquer la boutique.** Lancer l'activité, entrer sans écran de chargement, menacer le vendeur, prendre l'argent, sortir | Argent ajouté, niveau de recherche qui réagit, jeu fluide |
| C | **Voler une voiture sur commande.** Lancer l'activité, voler la voiture demandée, la livrer au garage fictif | Récompense versée ; un échec (voiture détruite) permet de recommencer |
| D | **Semer la police.** Lancer l'activité, provoquer 2 étoiles, atteindre la zone sûre sans être vu | Étoiles à 0 ; si arrêté, réapparition et nouvel essai possible |
| E | **Sauvegarder et charger.** Sauvegarder à la planque, quitter, relancer, choisir Continuer | Position, argent et heure retrouvés |
| F | **Jour, nuit, pluie.** Laisser tourner l'horloge (ou le menu) pendant une nuit et une pluie | Lumières allumées, pas de plantage, pas de longue saccade |
| G | **Charge.** Rouler vite à travers tout le quartier, puis le traverser à pied en sens inverse | Aucun écran de chargement, aucun arrêt d'image supérieur à 1 s |

## Ce qui est bloquant

Plantage, gel de plus de 5 s, scénario impossible à finir, sauvegarde qui ne
se recharge pas, chute sous le monde, blocage définitif. À signaler dans la
journée. Tout le reste va dans le formulaire.

## Après la session

1. Remplir le [formulaire de retour](feedback-form.fr.md) (5 minutes).
2. Déposer un bogue par problème avec le [modèle de bogue](bug-template.fr.md),
   en joignant le dossier compressé comme expliqué dans les instructions
   (section 4).
3. Envoyer le formulaire et les bogues à Nico.
