# Spécification de la build de test (journaux, plantages, touche d'export)

*Version anglaise : [test-build-spec.md](test-build-spec.md).*

Statut : conception pour 0.6 → 0.7, écrite sans machine ; à vérifier contre
le moteur par le pilote et le fil Windows. Rien ici ne change la config
moteur : les propositions de cvars ou d'ini vont au fil Optimisation.

## Pourquoi

Une build Shipping n'écrit ni journal ni CSV de profil, et la configuration
Test ne compilait pas sur le moteur du Launcher (Development en repli). Les
testeurs ont besoin de journaux et de rapports de plantage pour des retours
utiles.

## Exigences

1. **Configuration.** Packager une build de type `Test` qui garde les
   journaux. Si `Test` ne compile toujours pas, packager `Development`, ou
   Shipping avec `bUseLoggingInShipping = true` dans
   `GranTheftPraline.Target.cs`. Décision après la première compilation de
   contrôle sur le PC.
2. **Journal.** `Saved/Logs/GranTheftPraline.log` à chaque lancement
   (anciens journaux tournants, 5 gardés) ; la première ligne donne la
   version, l'heure de build, le commit et le profil graphique.
3. **Rapports de plantage gardés en local.** Le Crash Report Client est
   réglé pour ne jamais envoyer (`bAgreeToCrashUpload=False`, aucun
   point d'envoi) ; dossier `Saved/Crashes`. Le testeur le compresse ; rien
   ne quitte la machine.
4. **Touche d'export (F9).** Écrit `Saved/Exports/export-<horodatage>.txt` :
   version, commit, profil, système, GPU, position et orientation du joueur,
   cellule de carte, heure du jour, météo, et les 60 dernières secondes de
   temps d'image (p50, p95, pire, images > 100 ms). Affiché 3 s : « Exporté
   dans … ».
5. **Version.** Affichée à l'écran titre et dans le menu pause, depuis
   `ProjectVersion` posé par `tools/qa/stamp_version.py` (widget par le fil
   Interface).
6. **Test de fumée.** `tools/qa/check_game_log.py` sur le journal après la
   course scriptée de 60 s de `test-build-mac.sh` (et son équivalent
   Windows) : aucun fatal, aucune erreur hors `tools/qa/known-log-noise.txt`,
   au moins 50 lignes.

## Partage du travail

| Partie | Responsable |
| ---- | ---- |
| Pose de version, archives, contrôle de paquet, contrôle de journal, contrôle des mentions | QA (`tools/qa/`) |
| Exporteur F9, en-tête de journal, widget de version | Interface pour le widget ; petite classe C++ à QA, compilation de contrôle sur le PC |
| Target et interrupteurs ini (`bUseLoggingInShipping`, client de plantage) | Optimisation applique, QA propose |
| Équivalent Windows de la course de fumée | Fil PC Windows |
