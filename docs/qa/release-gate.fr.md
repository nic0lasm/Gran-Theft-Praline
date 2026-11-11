# Liste de contrôle de la porte de version

*Version anglaise : [release-gate.md](release-gate.md).*

À dérouler pour chaque version (0.4, 0.5, …, 0.9-rc), une fois passée la
porte de fusion du [plan de phase de test §5.2](../../production/test-phase-plan.fr.md)
pour chaque fusion. Cocher chaque ligne dans la note de version ; une ligne
non faite devient un défaut connu.

## Build et numéros
- [ ] Commit de `next` choisi, CI verte sur ce commit exact (Provenance check et tests).
- [ ] Tag décidé par Nico ; `python3 tools/qa/stamp_version.py <tag> --check` passe (ProjectVersion = tag).
- [ ] Compilation de contrôle sur le PC (MSVC, éditeur et jeu Win64) du même commit.
- [ ] Package macOS et package Windows construits depuis ce commit ; `build-info.txt` le montre.

## Contrôles automatiques
- [ ] Test de fumée sur chaque package : lancement, 60 s de jeu scripté, sortie propre, `tools/qa/check_game_log.py` PASS, aucun rapport de plantage.
- [ ] Banc M1, palier C plafonné à 40 i/s, 4 scènes de 120 s : ≥ 99 % des images à 25,0 ms ± 1, aucune image > 100 ms, GPU p95 ≤ 24 ms.
- [ ] Banc Windows (m1 et Ultra) : aucune image > 100 ms.
- [ ] 600 s par scène ; session libre de 30 min sans plantage sur chaque machine (60 min pour la 0.9).
- [ ] Mémoire mesurée (≤ 9 Go sur le M1 sans swap pour la 0.9).
- [ ] Temps de lancement mesuré (≤ 60 s sur le M1 pour la 0.9).

## Contenu de la sortie
- [ ] `python3 tools/qa/make_archives.py` lancé sur chaque package ; archives ≤ 2 Gio ; `SHA256SUMS` écrit.
- [ ] `python3 tools/qa/check_release_bundle.py <dossier>` PASS (pas de logos, pas de modèles sources, pas de chemins privés).
- [ ] `python3 tools/qa/check_notices.py` PASS ; l'écran de crédits correspond à `THIRD_PARTY_NOTICES`.
- [ ] Notes de version et guide du joueur en FR et EN (`docs/releases/<version>.md` et `.fr.md`), défauts connus listés.
- [ ] Inventaire des systèmes et état de la feuille de route à jour.
- [ ] Vidéo de preuve de la version gardée (Git LFS sous `production/evidence/<version>/`).
- [ ] Sauvegarde hors Git à jour (bloc collé par Nico).

## Pour une build de test (0.9-rc)
- [ ] Les deux archives ouvertes avec les seules instructions, sur un compte macOS neuf et un compte Windows neuf (les chemins Gatekeeper et SmartScreen marchent comme écrits).
- [ ] Scénarios A à G du protocole joués par un fil ou par Nico avant tout autre testeur.
- [ ] Règle des marques appliquée (l'archive par défaut est sans marques).

## Publier la version (par Nico, depuis son Mac)
Tag, release GitHub avec les archives ou un lien privé, fermeture des
issues : un seul bloc zsh préparé par le fil QA et collé par Nico. Rien
n'est écrit sur GitHub depuis le cloud.
