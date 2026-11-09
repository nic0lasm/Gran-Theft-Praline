# Tableau de bord : où en est le jeu

*Version anglaise : [roadmap-status.md](roadmap-status.md). Roadmap détaillée : [roadmap.fr.md](roadmap.fr.md).*
*Mis à jour par les fils à chaque tâche terminée. Dernière mise à jour : 2026-11-09.*

Légende : ✅ fait · 🟡 en cours · ⬜ à faire · ⛔ bloqué

## Version en cours : 0.1 « Jouer en un clic »

Cible : 2026-11-27. Avancement : 0 / 8 tâches.

| # | Tâche | Fil | Statut | Preuve |
| ---- | ---- | ---- | ---- | ---- |
| 1 | Projet passé en C++ (GameMode, PlayerController, GameInstance) | Build et lancement en un clic | 🟡 | code écrit, compilation sur le Mac à faire |
| 2 | Règles Git LFS posées, quota vérifié | Build et lancement en un clic | 🟡 | [règles](../docs/project/git-lfs.fr.md) ; quota à relever |
| 3 | Game Animation Sample intégré (personnage, motion matching, caméra) | Personnage jouable | 🟡 plan prêt, éditeur en attente | [plan](../docs/gameplay/player-character.fr.md) |
| 4 | Enhanced Input clavier-souris et manette | Personnage jouable | 🟡 mappings définis | [§5 du plan](../docs/gameplay/player-character.fr.md) |
| 5 | Niveau `L_RueRepublique` jouable (collisions, PlayerStart, limites) | Rue de la République jouable | ⬜ | |
| 6 | Écran titre et menu pause | Build et lancement en un clic | 🟡 | code écrit (UMG en C++), à tester |
| 7 | Script de packaging macOS → `GTAL/Builds/0.1/` | Build et lancement en un clic | 🟡 | [package-mac.sh](../tools/build/package-mac.sh), à exécuter |
| 8 | Build testée : vidéo 2 min, CSV de temps d'image, 10 min sans plantage | Build et lancement en un clic | ⬜ | |

En parallèle (pour la 0.2) : vague Tripo 1, mobilier urbain, 0 / 60 modèles
(voir [catalogue](asset-catalog.fr.md)).

## Toutes les versions

| Version | Nom | Cible | Statut |
| ---- | ---- | ---- | ---- |
| 0.1 | Jouer en un clic | 2026-11-27 | 🟡 |
| 0.2 | La rue habillée | 2026-12-18 | ⬜ (mesures de perf déjà en cours) |
| 0.3 | Au volant | 2027-01-15 | ⬜ |
| 0.4 | Armé | 2027-02-05 | ⬜ |
| 0.5 | Première boutique | 2027-02-19 | ⬜ |
| 0.6 | La police arrive | 2027-03-05 | ⬜ |
| 0.7 | Tout le quartier | 2027-04-09 | ⬜ |
| 0.8 | La foule | 2027-05-14 | ⬜ |
| 0.9 | Le trafic | 2027-06-25 | ⬜ |
| 0.10 | L'arsenal | 2027-07-30 | ⬜ |
| 0.11 | Vivre en ville | 2027-09-03 | ⬜ |
| 0.12 | La carte | 2027-09-24 | ⬜ |
| 0.13 | Le temps qui passe | 2027-10-29 | ⬜ |
| 0.14 | Façades et monuments | 2027-12-17 | ⬜ |
| 0.15 | Le quartier habité | 2028-01-28 | ⬜ |
| 0.16 | Les gens et les voitures | 2028-02-25 | ⬜ |
| 0.17 | Les intérieurs | 2028-03-31 | ⬜ |
| 0.18 | Le son de Lyon | 2028-04-28 | ⬜ |
| 0.19 | Finition | 2028-05-26 | ⬜ |
| 1.0 | Tranche verticale | 2028-06-16 | ⬜ |

## Indicateurs

| Indicateur | Valeur | Cible | Source |
| ---- | ---- | ---- | ---- |
| Temps d'image GPU, 95e centile (scène témoin, éditeur, avant profils) | 43,0 à 50,7 ms | ≤ 33,3 ms en profil M1 | Mesure du 2026-11-09, 4 éclairages |
| Modèles Tripo générés / intégrés | 0 / 0 | ~800 d'ici la 0.17 | [catalogue](asset-catalog.fr.md) |
| Crédits Tripo Studio restants | 25 200 | à consommer avant expiration | Interface Studio |
| Build jouable la plus récente | aucune | une par version | `GTAL/Builds/` |

## Décisions en attente de Nico

- Leviers du profil M1 pour tenir 30 i/s (pendant la 0.2).
- Où stocker les masters Tripo haute qualité, hors Git (avant la fin de la vague 1).
