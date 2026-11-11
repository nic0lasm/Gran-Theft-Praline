# Plan vers la phase de test : la 0.9

*Version anglaise : [test-phase-plan.md](test-phase-plan.md). Roadmap : [roadmap.fr.md](roadmap.fr.md). Systèmes : [systems-inventory.fr.md](systems-inventory.fr.md).*
*Revue du 2026-11-11, pendant le chantier performance (`next` = 3dcf33b). Complète la roadmap v3 : elle fixe ce que la 0.9 doit tenir pour qu'on puisse la tester, ce qui manque, et dans quel ordre on le fait.*

---

## 1. L'objectif, rappelé

| Point | Ce qui est décidé | Source |
| ---- | ---- | ---- |
| Le jeu | Jeu criminel en monde ouvert, original, parité de fonctions et de qualité avec GTA VI, GTA V et Red Dead Redemption 2, sur un Lyon fidèle ; jamais leur contenu | [concept](../design/gdd/game-concept.fr.md) |
| L'image | Photoréaliste ; le profil M1 dérive du profil Ultra, toute baisse visible passe par Nico | roadmap §1, ADR-0001 |
| Le terrain | Nord de la Presqu'île, Bellecour → Terreaux, quais de Saône et du Rhône (~1,5 km²) | concept |
| La ville vivante | 100+ piétons et 40+ véhicules perçus, rue habillée, platanes feuillus, par les techniques des mondes ouverts, jamais en appauvrissant l'image | chantier performance |
| La machine de référence | MacBook Pro M1 Pro 16 Go, profil m1, **plafonné à 40 i/s (25,0 ms réguliers)**, aucune image > 100 ms (décision du 2026-11-11, remplace la cible 30 i/s de l'ADR-0002) | chantier performance |
| La seconde machine | PC Windows (Ryzen 9 3900, RTX 3070 Ti) : build Windows (~81 i/s en Epic 1080p mesuré), compilation de contrôle, calcul | chantier performance |
| Le cadre | Non commercial, dépôt public MIT, gratuit d'abord, pas de personnes réelles, histoire écrite plus tard avec Nico | concept, règles du projet |
| L'ordre | 0.4 rue vivante, 0.5 police, 0.6 jour/nuit, 0.7 combat, 0.8 boutique, argent, sauvegarde, 0.9 quartier complet, puis 0.10 → 1.0 | roadmap v3 |

**Décidé par Nico le 2026-11-11 (carte) :**
la 0.9 est la **build de test du quartier**. Tout le quartier se parcourt
sans chargement avec les systèmes des versions 0.4 à 0.8, plus le minimum
pour jouer une vraie session (se repérer, avoir un but, régler ses
commandes). On la teste, puis les versions 0.10 et suivantes reprennent ce
qui reste du P0 et l'art final.

---

## 2. Critères de sortie : la 0.9 est prête pour la phase de test quand…

### A. Le monde
- [ ] Bellecour → Terreaux, quais et ponts du périmètre, générés en une commande (ville v2), parcourus à pied et en voiture **sans écran de chargement ni saccade de streaming** (cellules de l'ADR-0010, HLOD).
- [ ] Mémoire ≤ 9 Go sur le M1 sans swap, pool de textures 1,5 Go tenu, sur un trajet complet.
- [ ] Rue habillée complète (détails, vitrines, platanes feuillus), voitures garées, piétons lointains (imposteurs) en place.
- [ ] Une rue vitrine en qualité photoréaliste (rue de la République, profil Ultra) comparée aux photos (décidé le 2026-11-11).

### B. Les systèmes (0.4 → 0.8)
- [ ] Ville vivante : 100+ piétons et 40+ véhicules perçus, feux, réactions, ragdoll, choc voiture-piéton, mobilier cassable.
- [ ] Police 1-2 étoiles, témoins, sirènes et gyrophares, poursuite, arrestation, mini-carte.
- [ ] Jour, nuit, pluie ; phares, feux stop, clignotants ; lampadaires et vitrines la nuit.
- [ ] Combat : mains nues et pistolet, couverture, santé, mort et réapparition ; la police tire au niveau 2.
- [ ] Une boutique sans chargement (achat, braquage), argent, une planque, **sauvegarde automatique et en planque**, chargement d'une partie.

### C. De quoi jouer une session (ramené de 0.10 et 0.12)
- [ ] Carte plein écran avec **vrais noms de rues** et point de destination suivi par un GPS simple.
- [ ] **Cadre de missions minimal** (objectif, point de passage, échec, reprise) et **3 activités** construites avec : braquer la boutique, voler une voiture sur commande, semer la police.
- [ ] Commandes : clavier-souris et manette complètes, **remappage des touches**, sensibilité, glyphes manette ; sous-titres et textes en FR et EN.
- [ ] Menus : écran titre (nouvelle partie, continuer), pause, réglages, **crédits**, version affichée.

### D. La qualité
- [ ] **M1** : 40 i/s tenus (≥ 99 % des images à 25,0 ms ± 1), aucune image > 100 ms, sur le trajet complet à pied et en voiture, de jour, de nuit et sous la pluie, avec le streaming.
- [ ] **Windows** : 60 i/s en Epic 1080p sur la RTX 3070 Ti, aucune image > 100 ms ; plus de matériau par défaut (280 matériaux sans le flag Nanite corrigés).
- [ ] Une heure de jeu libre sans plantage ni bloquant, sur chaque machine.
- [ ] Temps de lancement jusqu'au jeu ≤ 60 s sur le M1 (à mesurer, aucune valeur aujourd'hui).

### E. La sortie de test
- [ ] Builds macOS et Windows en un clic, archives ≤ 2 Gio par fichier, sommes de contrôle, instructions d'ouverture (Gatekeeper, SmartScreen).
- [ ] **Build de test qui écrit ses journaux** (le Shipping n'en écrit pas) et garde les rapports de plantage en local, avec une touche qui exporte temps d'image et position.
- [ ] Protocole de test, scénarios, formulaire de retour et modèle de bogue, pour Nico et quelques proches (décidé le 2026-11-11).
- [ ] Notes de version et guide du joueur en FR et EN, défauts connus.
- [ ] Licences et crédits complets (IGN, Grand Lyon, OpenStreetMap ODbL, Fab et Megascans, Poly Haven, sons, polices, Unreal Engine) ; règle des vraies marques pour une build partagée en privé (à écrire avant la 0.9).
- [ ] Sauvegarde hors Git à jour (contenu Unreal hors dépôt, masters).

---

## 3. Écarts par version

Constat principal : **le code des versions 0.4 à 0.6 est déjà sur `next`**
(avant-premières de la 0.3, puis chantier performance), mais aucune de ces
versions n'est sortie, et l'inventaire des systèmes ne le reflète plus. À
l'inverse, **rien n'existe encore pour 0.7, 0.8 et 0.9**, et la 0.9 porte le
risque le plus lourd (génération, import, streaming, mémoire).

### 0.4 La rue vivante
| Fait (sur `next`) | Manque | Oubli du plan |
| ---- | ---- | ---- |
| Piétons, réactions, peur et fuite ; feux tricolores ; ragdoll et réactions du joueur ; mobilier cassable ; tables de chaînes FR/EN ; poses d'entrée en voiture ; palier C tenu (50 piétons, 25 conduites + 24 lointaines) | Piéton lointain (rig raté), voitures garées (matériaux), platanes feuillus (d827191 à mesurer), détails de rue 12, franchissement des obstacles bas, variété de la population (nombre de modèles décidé), objectif 100+ piétons | Porte de version chiffrée (palier C à 40 i/s) ; build Windows de la version ; `ProjectVersion` encore à 0.2.0 |

### 0.5 La police arrive
| Fait | Manque | Oubli du plan |
| ---- | ---- | ---- |
| Étoiles, témoins, poursuites, sirènes, mini-carte et carte plein écran, réglages sauvegardés, saccade de réapparition corrigée (09429f1) | Voiture de police fictive modélisée, arrestation et pénalité vérifiées en vidéo, poursuite de 3 min mesurée, barrages (branche non fusionnée, prévus en 0.11) | Coût de l'audio jamais mesuré au banc (passe audio) |

### 0.6 Jour et nuit
| Fait | Manque | Oubli du plan |
| ---- | ---- | ---- |
| Course du soleil, nuages en light function, nuages de nuit allégés, lumières de nuit budgétées, 6 météos, phares (`-LitCars`) | Captures Ultra face aux photos, piétons qui s'adaptent à la pluie (parapluies, allure), coût des 24 voitures lointaines la nuit et sous la pluie (vitres) | — |

### 0.7 Armé
| Fait | Manque | Oubli du plan |
| ---- | ---- | ---- |
| Ragdoll, réactions aux chocs légers | Tout le combat ; ADR du framework gameplay (la roadmap cite « ADR-0008 », numéro déjà pris : ce sera l'**ADR-0011**) ; **source des animations** (coups, visée, couverture) ; effets Niagara et leur budget | Budget GPU des effets d'armes ; chantier Combat jamais ouvert |

### 0.8 Vivre en ville
| Fait | Manque | Oubli du plan |
| ---- | ---- | ---- |
| Socle d'interactions (`UGTPInteractionComponent`) | Intérieur sans chargement, vendeur, achat, braquage, argent, planque, **SaveGame** (aucun code) ; ADR des intérieurs (la roadmap cite « ADR-0005 », pris par les piétons : ce sera l'**ADR-0012**) | Versionnage des sauvegardes ; le braquage dépend du combat (0.7) |

### 0.9 Tout le quartier
| Fait | Manque | Oubli du plan |
| ---- | ---- | ---- |
| ADR-0010 (cellules), rue de la République, quai du Rhône et pont Lafayette, cache géo du quartier, streaming de quartier en mesure | Génération de tout le quartier, import fiable (4 échecs), HLOD, mémoire ≤ 9 Go, bus TCL en décor | Carte, GPS, missions, remappage (prévus 0.10-0.12, indispensables au test) ; QA, journaux, crédits, protocole de test |

---

## 4. Ce que le plan avait oublié

| Sujet | État aujourd'hui | À faire | Version | Propriétaire proposé |
| ---- | ---- | ---- | ---- | ---- |
| **Porte de performance à chaque fusion** | Bancs manuels, seuils oraux | Porte écrite (§5.2), banc en une commande qui échoue seul | dès maintenant | Optimisation |
| **Build Windows** | Package fait à la main, 280 matériaux sans flag Nanite | Package à chaque version, banc m1 + Ultra, cache PSO D3D12 | chaque version | PC Windows |
| **Synchronisation du contenu hors Git** (31 Go, deux machines) | Copie unique du Mac le 11/11 | Une seule source par asset, manifeste de hachages, archive des seules différences | 0.4 | PC Windows + Import |
| **Sauvegarde de partie** | Rien (seuls les réglages sont sauvés) | SaveGame versionné, autosave, planque, test de chargement | 0.8 | Boutiques et sauvegarde (*à ouvrir*) |
| **Commandes** | Clavier-souris et manette de base | Remappage, sensibilité, glyphes, vibrations | 0.9 (avancé de 0.10) | Interface (fil Mini-carte élargi) |
| **Menus** | Écran titre, pause, réglages | Continuer, charger, crédits, version | 0.8-0.9 | Interface |
| **Audio** | Code et mix sur `next`, jamais mesuré | Passe audio au banc, sons du combat et de la boutique | 0.5, 0.7 | Audio |
| **Boucle de jeu** | Aucune | Cadre de missions minimal et 3 activités | 0.9 (avancé de 0.12) | Missions et activités (*à ouvrir*) |
| **Carte et GPS** | Carte plein écran sans noms de rues | `roads.geojson` régénéré, destination, GPS | 0.9 (avancé de 0.10) | Ville v2 + Interface |
| **QA et protocole de test** | Tests hors Unreal, playtests scriptés de la 0.1 | Test de fumée automatique de chaque build, session d'une heure, scénarios, modèle de bogue | 0.6 → 0.9 | QA et sortie (*à ouvrir*) |
| **Journaux et plantages** | Le Shipping n'écrit ni journal ni CSV | Configuration de test avec journaux, rapports de plantage gardés en local, touche d'export | 0.7 | QA et sortie |
| **Numéro de version** | `ProjectVersion=0.2.0` | Posé par le script de package depuis le tag | 0.4 | QA et sortie |
| **Licences et crédits** | Crédits dans le README seulement | Écran de crédits, fichier des licences tierces, règle des marques | 0.8 | QA et sortie |
| **Docs** | Roadmap, ADR, notes 0.3 | Guide du joueur FR/EN, protocole, inventaire à jour | 0.9 | README + QA |
| **ADR-0002** | Dit 30 i/s visés | Amendé : M1 plafonné à 40 i/s, règle « aucune image > 100 ms » | maintenant | Optimisation |
| **Sauvegarde hors Git** | Bloc prêt, pas encore collé | Copie sur le SSD avant le test | avant 0.9 | Sauvegarde (Nico colle le bloc) |

---

## 5. Plan adapté

### 5.1 Ordre et dépendances

```
Étape 0  fin du chantier perf ─┐
                               ├─► 0.4 ─► 0.5 ─► 0.6   (train de versions : surtout de l'intégration)
Préparation dans le cloud ─────┤
  ADR-0011 combat, animations  ├─► 0.7 combat ─► 0.8 boutique, argent, sauvegarde (le braquage dépend du combat)
  ADR-0012 intérieurs, SaveGame┘
Ville v2 + import + PC ─────────────────────────────────► 0.9 quartier (chemin critique, commence tout de suite)
Interface, missions, QA ────────────────────────────────► 0.9 build de test ─► phase de test ─► 0.10…
```

1. **Étape 0, maintenant → verdict du chantier.** Finir les critères de
   sortie 3 à 7 du chantier (budget par système mesuré en A/B, banc de
   non-régression en une commande, imposteurs de foule, streaming,
   mémoire, règles de contenu). Aucune fonction nouvelle sur `next`.
2. **Train 0.4 → 0.6.** Le code est là : chaque version sort dès que ses
   manques (§3) sont comblés et que sa porte (§5.2) passe. Rythme visé :
   quelques jours par version, pas des semaines.
3. **En parallèle, dans le cloud, dès le verdict** : ADR-0011 et code du
   combat testé hors Unreal, recensement des animations (gratuit d'abord) ;
   ADR-0012, SaveGame et économie ; cadre de missions ; remappage et menus.
   Rien de cela n'occupe le Mac avant son intégration.
4. **Chemin critique, dès aujourd'hui** : génération de tout le quartier
   (ville v2, cache géo déjà là), réparation de l'import (diagnostic hors
   Unreal d'abord), HLOD et LOD calculés sur le PC. C'est le plus long et
   le plus risqué ; il ne doit pas attendre la 0.8.
5. **0.7 puis 0.8**, puis **0.9** qui réunit le quartier, les systèmes et la
   boucle de jeu, et se termine par une **0.9-rc** passée au protocole de
   test par les fils avant d'arriver chez Nico.

### 5.2 Portes de qualité

**À chaque fusion sur `next`** (aucune exception) :
1. CI verte sur le commit exact (Provenance check, tests hors Unreal).
2. Compilation de contrôle sur le PC (MSVC, éditeur et jeu Win64) du même commit.
3. M1, profil m1 plafonné à 40 i/s, monde du palier C, 4 scènes de 120 s
   (fixe, parcours déterministe, nuit, pluie) : ≥ 99 % des images à 25,0 ms
   ± 1 et **aucune image > 100 ms** ; GPU p95 ≤ 24 ms (marge sous le plafond).
4. Si la branche ajoute un système : A/B marche/arrêt contre sa ligne du
   budget par système.

Le plafond rend le temps d'image insensible à la variance de ~2 ms du
palier C ; c'est le GPU p95 qui sert de jauge de marge.

**À chaque version**, en plus : 600 s par scène, une session libre de 30 min
sans plantage (60 min en 0.9), package et banc Windows (m1 et Ultra, aucune
image > 100 ms), mesure mémoire, vidéo de preuve, notes FR/EN, numéro de
version posé, inventaire des systèmes mis à jour.

### 5.3 Mac, PC et cloud en parallèle

| Machine | Rôle | Ne fait plus |
| ---- | ---- | ---- |
| **MacBook Pro M1 Pro** | Banc de référence (porte de chaque fusion), package macOS, captures de la version | Import d'assets, calculs longs (passent au PC) |
| **PC Windows** | Compilation de contrôle de chaque branche ; package et banc Windows ; **import headless** et calculs (LOD, HLOD, imposteurs, textures PBR, Blender) ; profilage GPU fin ; source du contenu importé, envoyé au Mac par archive de différences | — |
| **Cloud (fils)** | Code, tests hors Unreal, données géo, docs ; scripts essayés à blanc avant tout créneau | — |

Règle : le Mac ne reçoit que ce qui a déjà compilé sur le PC et passé la
CI ; le PC n'est jamais à vide (import, calculs ou package en file).

### 5.4 Qui porte quoi

| Fil | Porte jusqu'à la 0.9 |
| ---- | ---- |
| Optimisation (pilote) | Porte de fusion, banc de non-régression, budget par système, ADR-0002 amendé, mémoire |
| PC Windows | Compilation de contrôle, package et banc Windows, synchronisation du contenu, calculs |
| Trafic | Feux, conduite, bus TCL en décor, coût des voitures lointaines la nuit |
| Piétons | 100+ perçus, piéton lointain, variété, pluie |
| Police | 0.5 complète, tir au niveau 2 avec le combat |
| Ragdoll | Réactions physiques, franchissement, **animations hors locomotion** |
| Jour/nuit | Captures Ultra, feux des voitures, coût nuit et pluie |
| Audio | Passe au banc, sons du combat et de la boutique |
| Ville v2 | Génération du quartier, cellules et HLOD, `roads.geojson` |
| Réalisme | Détails, vitrines, rue vitrine photoréaliste |
| Arbres, Import | Platanes feuillus ; import fiable sur le PC |
| Mini-carte → **Interface** | Carte et GPS, remappage, glyphes, menus, crédits |
| Sauvegarde | Copie hors Git avant la 0.9 |
| README | Guide du joueur, notes de version |
| *À ouvrir* : **Combat et armes** | ADR-0011, corps à corps, pistolet, couverture, santé, effets |
| *À ouvrir* : **Boutiques, économie, sauvegarde de partie** | ADR-0012, intérieur, braquage, argent, planque, SaveGame |
| *À ouvrir* : **Missions et activités** | Cadre minimal, 3 activités |
| *À ouvrir* : **QA et sortie** | Build de test avec journaux, test de fumée, protocole, crédits et licences, numéro de version, archives |

### 5.5 Ce qui passe après la phase de test

Arsenal à 6 armes et roue des armes, police niveau 3 et barrages, 5 types
de boutiques et 3 planques, deux-roues, 6 activités, accessibilité complète,
paroles des PNJ, art final du quartier (0.13 → 0.16), finition (0.17).

---

## 6. Questions posées à Nico (toutes tranchées)

1. ~~Ce que veut dire « phase de test » après la 0.9~~ : **décidé**, build de test du quartier (§1).
2. ~~Qui teste~~ : **décidé**, Nico et quelques proches, sur Mac et Windows, builds partagées en privé. D'où : builds des deux systèmes, journaux et rapports de plantage, formulaire de retour, écran de crédits (§2 E).
3. ~~Le niveau visuel~~ : **décidé**, qualité de travail partout, plus la rue de la République en photoréaliste (§2 A).

Les trois réponses ont été données par Nico le 2026-11-11, toutes sur l'option recommandée.

---

## 7. Risques propres à la 0.9

| Risque | Parade |
| ---- | ---- |
| Quartier complet trop lourd pour 16 Go ou pour le GPU du M1 | Cellules petites, HLOD dès la génération, mesure mémoire à chaque étape, porte à 40 i/s sur le trajet complet |
| Import qui échoue encore | Diagnostic hors Unreal, import sur le PC, une seule source par asset |
| Animations de combat introuvables | Recensement avant tout code du combat ; poses faites main (méthode des poses de voiture) en repli |
| Deux machines qui divergent | Manifeste de hachages vérifié avant chaque package |
| Vraies enseignes dans une build partagée en privé | Règle écrite avant la 0.9 ; variante sans marques prête |
