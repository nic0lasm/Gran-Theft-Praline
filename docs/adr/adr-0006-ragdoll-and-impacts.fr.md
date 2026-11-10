# ADR-0006 : Ragdoll et réactions physiques

*Version anglaise : [adr-0006-ragdoll-and-impacts.md](adr-0006-ragdoll-and-impacts.md)*

## Statut

Proposé le 2026-11-10 (la première mesure sur le M1 Pro fixe les plafonds).

## Date

2026-11-10

## Décideurs

nic0lasm (responsable du projet)

## Résumé

Un seul jeu de règles décide de l'effet d'un choc, d'une chute ou d'un saut de
voiture sur un corps (`GTPImpactLogic.h`, sans types Unreal, vérifié hors moteur).
Un composant rend un maillage squelettique à la physique puis le récupère
(`UGTPRagdollComponent`), pour le joueur et les piétons. Un sous-système de monde
plafonne le nombre de ragdolls simulés selon le profil graphique.

## Contexte

Piétons, police et combat ont tous besoin d'un corps qui réagit. La 0.4 demande
qu'un piéton renversé vole, tombe puis se relève ou reste au sol, et que le
joueur trébuche, tombe d'une voiture en marche ou soit percuté. Le fil piétons a
déjà un `AGTPPedestrian::StartRagdoll` minimal à impulsion fixe ; le véhicule a
des points d'accroche de montages pour entrer et sortir. Manquent la couche de
décision commune, le côté joueur et une limite de coût, dans un budget d'image
déjà serré (ADR-0002).

## Décision

1. **Règles** dans `GTPImpactLogic.h` : gravité selon la vitesse de rapprochement
   (aucune, bousculade, renversement, projection, violent à 4, 12, 28 et 55 km/h),
   courbe de dégâts (40 km/h fait mal, 70 km/h tue), vitesse de projection tirée
   de la quantité de mouvement (le corps part à environ la moitié de la vitesse de
   la voiture, avec de la hauteur), atterrissages (trébuchement, roulade, ragdoll
   selon la vitesse verticale, accroupi la divise par deux), saut d'une voiture en
   marche, cycle de vie du ragdoll (chute, repos, pause, relevé sur le dos ou sur
   le ventre, ou reste au sol), et quels ragdolls geler au-delà du plafond.
2. **Mécanique** dans `UGTPRagdollComponent` : simuler le maillage, faire suivre le
   bassin par la capsule, remettre le personnage au sol sous le bassin au relevé.
   Il ne tick que pendant un ragdoll. Les montages de relevé sont des propriétés
   optionnelles (voir docs/animation/sources.fr.md) ; sans eux, le corps est rendu
   avec un saut de pose.
3. **Plafond** dans `UGTPRagdollSubsystem` : m1 3, fidelity 6, ultra 10 ragdolls
   simultanés (`-MaxRagdolls=N` pour forcer), le plus loin puis le plus ancien est
   gelé (pose gardée, simulation coupée), contrôle deux fois par seconde.
4. **Joueur** dans `UGTPPlayerReactionComponent` : cherche les voitures au contact
   de la capsule à 20 Hz, mesure la vitesse d'atterrissage, réagit à un saut de
   voiture, tient une santé de 0 à 100 et annonce les dégâts par `OnHurt`.
5. **Accroches, additives seulement**, dans des fichiers d'autres fils :
   - piétons : remplacer l'impulsion fixe par `GTPImpact::ClassifyCarHit` et
     utiliser `UGTPRagdollComponent` (ou garder `StartRagdoll` en appelant
     seulement les règles) ; diffuser `OnPedestrianHit` avec la gravité dès un
     renversement ;
   - véhicule : `DoExit(bBailOut = true)` appelle `ReactToBailOut(Vitesse)` sur le
     composant de réaction du joueur ; entrer et sortir utilisent les
     `EnterMontage`/`ExitMontage` existants dès que les animations existent.

## Alternatives envisagées

- Physical Animation (ragdoll partiel, poids de mélange au choc) : meilleur rendu
  pour les tirs, laissé au combat (0.7) ; cet ADR construit d'abord le ragdoll
  complet dont il a besoin.
- Ragdoll et relevés motion matching du Game Animation Sample d'Epic, annoncés
  dans la version 5.8 (à vérifier, voir docs/animation/sources.fr.md) : à préférer
  pour le fondu du relevé une fois contrôlés ; les règles et le plafond restent
  les nôtres.
- Ragdoll par piéton sans plafond : coût physique non borné lors d'un carambolage.

## Conséquences

- Sans animation de relevé, le personnage saute à la pose debout : acceptable pour
  les piétons, à remplacer pour le joueur.
- La logique capsule/maillage n'est validée que par les règles hors moteur tant
  qu'il n'y a pas de créneau Mac ; le code Unreal a été écrit sans compilation.
- Santé et dégâts restent locaux au composant joueur jusqu'à un système de santé.

## Critères de validation

- `python3 tools/ragdoll/check_impact_logic.py` PASS (fait dans le cloud).
- Mac, profil m1 : une voiture à 20, 40 et 70 km/h dans le joueur et dans un
  piéton : gravité et issue comme dans le tableau de docs/gameplay/ragdoll.fr.md,
  filmé ; une chute de 3 m et de 8 m ; un saut de voiture à 20, 40 et 70 km/h.
- Trois ragdolls simultanés coûtent au plus +1 ms de CPU fil de jeu et +0,5 ms de
  GPU en p95 par rapport au même parcours sans eux (à confirmer avec le fil
  optimisation).

## Liens

ADR-0002 (budget), ADR-0003 (mouvement), ADR-0005 (piétons),
docs/animation/sources.fr.md, production/systems-inventory.fr.md (§ ragdoll).
