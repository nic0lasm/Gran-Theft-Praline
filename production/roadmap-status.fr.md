# Tableau de bord : où en est le jeu

*Version anglaise : [roadmap-status.md](roadmap-status.md). Roadmap : [roadmap.fr.md](roadmap.fr.md). Systèmes : [systems-inventory.fr.md](systems-inventory.fr.md).*
*Mis à jour par les fils à chaque tâche terminée. Dernière mise à jour : 2026-11-10.*

Légende : ✅ fait · 🟡 en cours · ⬜ à faire · ⛔ bloqué

## Version en cours : 0.3 « Au volant »

Sort dès que le 95e centile est ≤ 42 ms en profil M1 avec le trafic, sans
attendre les autres chantiers.

| # | Tâche | Chantier | Statut | Preuve |
| ---- | ---- | ---- | ---- | ---- |
| 1 | Clio V, 208 II, Kangoo II conduisibles avec LOD | Véhicules | ✅ | `assets/staging/vehicles/evidence-lods/` sur le Mac |
| 2 | Entrer, sortir, casser la vitre, éjecter un conducteur PNJ | 0.3 | ✅ | vidéos `GTAL/Builds/0.3-dev/evidence` |
| 3 | Trafic sur 112 voies, conducteurs PNJ | 0.3 | 🟡 | branche `feat/traffic-drivers` |
| 4 | Son moteur et panneau de conduite | 0.3 | 🟡 | branche `feat/vehicle-hud-sound` |
| 5 | Carte étendue : quai du Rhône, pont Lafayette, transversales de 6 m | Ville v2 | ✅ | build `0.3-city`, test 60 s |
| 6 | Chute dans le Rhône : réapparition sur le quai, surface d'eau | Ville v2 | ⬜ | |
| 7 | Coût GPU des PNJ réduit (ombres, LOD, conducteurs sans animation) | 0.3 | 🟡 | créneau Mac du 2026-11-10 |
| 8 | Budget de la carte étendue (culling, ombres, Lumen) | Optimisation | 🟡 | au créneau Mac |
| 9 | Enveloppes GPU par système mesurées une première fois | Optimisation | ⬜ | |
| 10 | Mesure 600 s : p95 ≤ 42 ms avec trafic | 0.3 | ⬜ | |
| 11 | Archive de release : découpage si > 2 Gio | 0.3 | ⬜ | |

## Chantiers ouverts

| Chantier | Vise | État |
| ---- | ---- | ---- |
| Piétons (Mass + StateTree) | 0.4 | préparation cloud |
| Police et étoiles | 0.5 | préparation cloud |
| Jour/nuit + météo | 0.6 | préparation cloud |
| Optimisation M1 | continu | créneau Mac en file |
| Réalisme de la rue | continu | audit face aux photos |
| Ville v2 | continu | quai et pont faits |
| Import, packs gratuits, Tripo | continu | 84 modèles importés ; érable Megaplants à mesurer |

## Toutes les versions

| Version | Nom | Statut |
| ---- | ---- | ---- |
| 0.1 | Jouer en un clic | ✅ 2026-11-09 |
| 0.2 | La rue habillée | ✅ 2026-11-10 |
| 0.3 | Au volant | 🟡 |
| 0.4 | La rue vivante | ⬜ (piétons en préparation) |
| 0.5 | La police arrive | ⬜ (police en préparation) |
| 0.6 | Jour et nuit | ⬜ (jour/nuit en préparation) |
| 0.7 | Armé | ⬜ |
| 0.8 | Vivre en ville | ⬜ |
| 0.9 | Tout le quartier | ⬜ |
| 0.10 | La carte et le confort | ⬜ |
| 0.11 | L'arsenal | ⬜ |
| 0.12 | La ville à soi | ⬜ |
| 0.13 | Façades et monuments | ⬜ |
| 0.14 | Les intérieurs | ⬜ |
| 0.15 | Les gens et les voitures | ⬜ |
| 0.16 | Le son de Lyon | ⬜ |
| 0.17 | Finition | ⬜ |
| 1.0 | Tranche verticale | ⬜ |

## Indicateurs

| Indicateur | Valeur | Cible | Source |
| ---- | ---- | ---- | ---- |
| Temps d'image, build 0.2, 600 s, profil M1 | p50 29,9 ms, p95 40,3 ms, 84,9 % ≤ 33 ms | p95 ≤ 42 ms (provisoire), 95 % ≤ 33,3 ms (visé) | preuves de la 0.2 sur le Mac |
| Carte étendue 0.3, 60 s | 65 % d'images < 33 ms, GPU p50 32 ms | idem | build `0.3-city` |
| Coût de 6 PNJ conducteurs | environ +10 ms GPU | 2 ms pour tout le trafic (§3 de la roadmap) | créneau du 2026-11-10 |
| Taille de l'archive de release | 1,36 Go (0.2) | < 2 Gio par fichier | release v0.2.0 |
| Modèles importés dans Unreal | 84 (284 meshes) | | fil Import |

## Décisions en attente de Nico

- Aucune. Ordre des versions 0.4 à 0.7 décidé par Nico le 2026-11-10 : la rue vivante d'abord.
