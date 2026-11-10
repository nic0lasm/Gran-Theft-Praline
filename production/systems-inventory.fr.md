# Inventaire des systèmes d'un monde ouvert GTA / RDR

*Version anglaise : [systems-inventory.md](systems-inventory.md). Roadmap : [roadmap.fr.md](roadmap.fr.md).*
*Revue du 2026-11-10, après la livraison de la 0.2. Mise à jour à chaque fin de version.*

Ce document liste **tous** les systèmes qu'un joueur attend d'un monde ouvert
de type GTA ou Red Dead Redemption, et dit où en est chacun dans Gran Theft
Praline. Il sert de liste de contrôle : un système qui n'a ni version ni
chantier est un **oubli**.

Légende : ✅ fait · 🟡 en cours (un chantier y travaille) · 📅 prévu (une
version le porte, personne n'y travaille encore) · ❗ oublié (absent de la
roadmap v2 ou sans chantier, ajouté par cette revue) · ⛔ hors périmètre.

Priorité : P0 = nécessaire à la 1.0 (tranche verticale), P1 = alpha, P2 = plus
tard ; reprise du [concept](../design/gdd/game-concept.fr.md).

## 1. À pied

| Système | Prio | État | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Marcher, courir, sprinter, sauter, s'accroupir (motion matching) | P0 | ✅ | 0.1 | Game Animation Sample |
| Caméra 3e personne façon GTA, bascule 1re personne (V) | P0 | ✅ | 0.1, 0.2 | FOV 86°, recentrage, pas de caméra dans les murs |
| Changement d'épaule, caméra de visée | P0 | 📅 | 0.7 | avec le combat |
| Franchir, escalader les obstacles bas | P0 | ❗ | 0.4 | le GASP le sait faire ; jamais activé ni testé sur nos murets et bornes |
| Endurance | P0 | ❗ | 0.7 | absent des tâches de la roadmap v2 |
| Réactions physiques, ragdoll, trébucher, bousculer | P0 | ❗ | 0.4 | aucune tâche ; nécessaire aux piétons, au trafic et au combat |
| Interactions contextuelles (E : portes, s'asseoir, ramasser) | P0 | 🟡 | 0.2, 0.8 | socle `UGTPInteractionComponent` fait ; portes et contenants restent |
| Nage, chute dans le Rhône ou la Saône | P1 (chute : P0) | ❗ | 0.3 | le quai du Rhône est dans la carte depuis aujourd'hui : il faut au minimum gérer la chute dans l'eau (réapparition sur le quai) |
| Apparence du joueur (vêtements, coiffure) | P1 | 📅 | 1.x | boutique de vêtements en 0.8 |

## 2. Véhicules

| Système | Prio | État | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Conduite physique Chaos (citadine, berline, utilitaire) | P0 | ✅ | 0.3 | Clio V, 208 II, Kangoo II avec LOD |
| Monter, descendre, casser la vitre, éjecter le conducteur | P0 | ✅ | 0.3 | sans animations dédiées (durées fixes) |
| Dégâts par zones, vitres, épave | P0 | ✅ | 0.3 | feu, explosion, pneus crevés : 0.7 |
| Caméra de poursuite, vue intérieure, regard arrière | P0 | ✅ | 0.3 | caméra cinématique et capot : 0.10 |
| Son moteur synthétisé, panneau de conduite | P0 | 🟡 | 0.3 | branche `feat/vehicle-hud-sound` |
| Phares, feux stop, clignotants, klaxon | P0 | ❗ | 0.6 | indispensables la nuit ; rien de prévu |
| Animations entrer / sortir | P0 | ❗ | 0.5 | aucune source d'animation identifiée (voir §9) |
| Deux-roues : scooter, moto, Vélo'v | P0 / P1 | 📅 | 0.12 | Vélo'v décidé par Nico |
| Bus TCL (décor puis conduisible), métro et funiculaire en décor | P2 → P1 | 📅 | 0.9, 1.x | parc TCL décidé par Nico |
| Radio de bord | P1 | 📅 | 1.x | musique libre de droits ou créée |
| Garages, personnalisation | P1 | 📅 | 1.x | |
| Bateaux sur les fleuves | P2 | ⛔ | | |

## 3. Monde vivant

| Système | Prio | État | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Trafic sur les vraies voies (112 voies, conducteurs PNJ) | P0 | 🟡 | 0.3 | branche `feat/traffic-drivers` ; coût GPU des PNJ en cours de réduction |
| Feux tricolores, priorités, passages piétons | P0 | 📅 | 0.4 | |
| Réactions du trafic (klaxon, évitement, fuite, accidents) | P0 | 📅 | 0.4 | |
| Piétons Mass + StateTree sur les trottoirs | P0 | 🟡 | 0.4 | chantier Piétons ouvert le 2026-11-10 |
| Réactions des piétons (fuite, peur, témoins, regarder) | P0 | 🟡 | 0.4 | avec le chantier Piétons |
| Choc voiture-piéton, renverser un piéton | P0 | ❗ | 0.4 | dépend du ragdoll |
| Variété de la population (visages, tenues, âges) | P0 | ❗ | 0.4, 0.15 | MetaHuman coûte cher sur M1 : stratégie LOD et nombre de modèles à décider |
| Paroles des PNJ (réflexions, insultes, cris) | P0 | ❗ | 0.10 | voix à enregistrer ou synthétiser, FR |
| Saluer, provoquer un PNJ (style RDR) | P1 | ❗ | 1.x | |
| Événements d'ambiance (disputes, vols, livraisons) | P1 | 📅 | 1.x | |
| Faune (pigeons, chiens) | P2 | 📅 | 1.x | les pigeons valent tôt pour le réalisme : à voir en habillage |
| Jour, nuit, course du soleil à Lyon | P0 | 🟡 | 0.6 | chantier Jour/nuit + météo ouvert le 2026-11-10 |
| Pluie, sol mouillé, brouillard | P0 | 🟡 | 0.6 | même chantier |
| Éclairage urbain la nuit (lampadaires, vitrines, fenêtres) | P0 | ❗ | 0.6 | coût des lumières sur M1 à budgéter |

## 4. Combat

| Système | Prio | État | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Corps à corps (coups, esquive, neutralisation) | P0 | 📅 | 0.7 | aucun chantier |
| Armes à feu (pistolet d'abord), visée, recul, rechargement | P0 | 📅 | 0.7, 0.11 | aucun chantier |
| Aide à la visée manette | P0 | 📅 | 0.7 | |
| Couverture, tir à l'aveugle | P0 | 📅 | 0.7 | |
| Roue des armes, inventaire limité | P0 | 📅 | 0.11 | |
| Santé, armure, mort, réapparition à l'hôpital | P0 | 📅 | 0.7 | le PNJ éjecté n'a pas encore de santé |
| Réactions aux impacts localisées | P0 | 📅 | 0.7 | dépend du ragdoll (0.4) |
| Effets visuels : impacts, étincelles, sang, fumée, feu, explosions | P0 | ❗ | 0.7 | Niagara ; aucun chantier, aucun budget GPU |
| Objets cassables (potelets, panneaux, lampadaires, vitrines) | P0 | ❗ | 0.4 | signature de GTA en voiture ; les 284 modèles importés sont tous fixes |
| Armes de jet | P1 | 📅 | 1.x | |

## 5. Police et recherche

| Système | Prio | État | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Délits et témoins qui signalent | P0 | 🟡 | 0.5 | chantier Police ouvert le 2026-11-10 |
| Étoiles 1 à 3, zone de recherche, ligne de vue | P0 | 🟡 | 0.5, 0.11 | |
| Patrouilles, poursuites en voiture | P0 | 🟡 | 0.5 | réutilise le trafic de la 0.3 |
| Arrestation, pénalité, reprise rapide | P0 | 🟡 | 0.5 | |
| Police qui tire (niveau 2-3), barrages | P0 | 📅 | 0.7, 0.11 | dépend du combat |
| Sirènes, gyrophares | P0 | ❗ | 0.5 | son et lumière, rien de prévu |
| Hélicoptère, niveaux 4-5, unités spéciales | P1 | 📅 | 1.x | |
| Police française fictive (véhicules, uniformes, insignes fictifs) | P0 | 📅 | 0.5, 0.15 | Mégane ou 308 de police fictive à modéliser |

## 6. Ville et lieux

| Système | Prio | État | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Rue de la République fidèle (LiDAR, 215 enseignes, kit façades) | P0 | ✅ | 0.2 | |
| Quai du Rhône, pont Lafayette | P0 | 🟡 | 0.3 | build `0.3-city` à 65 % d'images < 33 ms |
| Habillage réaliste (petits détails face aux photos) | P0 | 🟡 | continu | chantier Réalisme de la rue ouvert le 2026-11-10 |
| Platanes avec feuillage | P0 | 🟡 | 0.4 | érable Megaplants à mesurer en premier |
| Tout le quartier Bellecour → Terreaux | P0 | 📅 | 0.9 | |
| Streaming du monde (World Partition, HLOD, data layers) | P0 | 📅 | 0.8, 0.9 | |
| Monuments (Hôtel de Ville, Opéra, Bellecour) | P0 | 📅 | 0.13 | |
| Eau des fleuves (surface, quais bas) | P0 | ❗ | 0.3 | plugin Water natif sans simulation ; aucun chantier |
| Boutiques sans chargement, braquage | P0 | 📅 | 0.8 | |
| Halls, cages d'escalier, cours, toits, parkings | P0 (sélection) | 📅 | 0.8, 0.14 | |
| Planques | P0 | 📅 | 0.8 | |

## 7. Interface, menus et services

| Système | Prio | État | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Écran titre, menu pause, profils graphiques | P0 | ✅ | 0.1, 0.2 | |
| HUD : point central, aides, sous-titres, panneau de conduite | P0 | ✅ | 0.2, 0.3 | |
| Mini-carte | P0 | ❗ | 0.5 | prévue en 0.12 seulement ; indispensable dès les poursuites |
| Carte plein écran, vrais noms de rues, GPS | P0 | 📅 | 0.10 | `lanes.csv` a des noms vides : `roads.geojson` à régénérer (chantier ville v2) |
| Argent, économie | P0 | 📅 | 0.8 | |
| Sauvegarde automatique et en planque | P0 | 📅 | 0.8 | |
| Réglages sauvegardés (profil, souris, volumes, touches) | P0 | ❗ | 0.5 | aujourd'hui tout se perd à la fermeture |
| Remappage des touches, sensibilité, inversion | P0 | 📅 | 0.10 | |
| Accessibilité (sous-titres, aide à la visée, couleurs) | P0 | 📅 | 0.10 | |
| Textes du jeu en français et en anglais | P0 | ❗ | 0.4 | tables de chaînes dès maintenant, sinon reprise coûteuse |
| Téléphone (contacts, messages, appareil photo) | P1 | 📅 | 1.x | |
| Mode photo | P2 | 📅 | 1.x | |

## 8. Missions et activités

| Système | Prio | État | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Cadre de missions (objectifs, points de passage, échec, reprise) | P0 | ❗ | 0.12 | pas de scénario, mais il faut l'outil pour les activités, les tests et l'histoire future |
| Activités libres du concept (6) | P0 | 📅 | 0.12 | |
| Statistiques, compétences | P1 | 📅 | 1.x | |
| Histoire, personnages, dialogues | P1 | ⛔ | 1.x | écrite plus tard avec Nico |
| Jeu en ligne | | ⛔ | | hors projet |

## 9. Production et technique (invisible mais bloquant)

| Système | Prio | État | Version | Note |
| ---- | ---- | ---- | ---- | ---- |
| Packaging en un clic, release GitHub | P0 | ✅ | 0.1 | |
| Budget de performance par système | P0 | ❗ | 0.3 | voir [roadmap §3](roadmap.fr.md) ; aujourd'hui seul le total est suivi |
| Optimisation M1 (culling, ombres, Lumen, PSO) | P0 | 🟡 | continu | chantier Optimisation ouvert le 2026-11-10 |
| Test de performance automatique à chaque build | P0 | 🟡 | 0.3 | journaux de soak en Shipping existent ; à rendre bloquant |
| Source des animations (combat, véhicules, PNJ, couverture) | P0 | ❗ | 0.4 | GASP ne couvre que la locomotion ; à recenser (Epic gratuit, Fab, capture vidéo) |
| Audio : mixage, MetaSounds, pas par surface, sirènes, impacts | P0 | ❗ | continu | relégué en 0.18 dans la v2 ; le son fait la moitié des sensations |
| Distribution des builds au-delà de 2 Gio | P0 | ❗ | 0.3 | GitHub limite chaque fichier de release à 2 Gio ; la 0.2 pèse 1,36 Go |
| Lancement sur un autre Mac (Gatekeeper, signature ad hoc) | P1 | ❗ | 0.3 | à documenter dans le README |
| Sauvegarde des sources hors Git (masters, GTAL/assets) | P0 | 🟡 | | dans « Reste à Nico » |
| Manette (retour haptique, glyphes) | P0 | 🟡 | 0.10 | manette branchée, glyphes absents |

## 10. Bilan

- **Fait** : la base à pied, la conduite, la rue fidèle, le packaging. C'est la
  bonne fondation : GTA et RDR reposent d'abord sur la sensation à pied et en
  voiture.
- **En cours aujourd'hui** : trafic, piétons, police, jour/nuit, réalisme,
  optimisation. La roadmap v2 les plaçait en 0.6, 0.8, 0.9 et 0.13 ; la
  nouvelle roadmap les remonte en 0.4 à 0.6.
- **Oubliés les plus graves** (sans eux, le jeu ne ressemble pas à un GTA même
  avec tout le reste) :
  1. **Réactions physiques et ragdoll**, chocs voiture-piéton.
  2. **Source des animations** au-delà de la locomotion.
  3. **Audio** comme chantier continu, pas en fin de projet.
  4. **Effets visuels et objets cassables.**
  5. **Budget de performance par système** : chaque chantier ajoute du coût
     sur un GPU déjà plein.
  6. **Éclairage de nuit** : phares, lampadaires, vitrines.
  7. **Mini-carte, réglages sauvegardés, textes FR/EN** (petits, mais chers à
     rattraper).
  8. **Eau des fleuves et chute dans l'eau**, maintenant que le quai est dans la
     carte.
  9. **Distribution** au-delà de 2 Gio par fichier.
  10. **Cadre de missions**, même sans histoire.
