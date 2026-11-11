# QA et sortie

*Version anglaise : [README.md](README.md).*

| Document | Usage |
| ---- | ---- |
| [release-gate.fr.md](release-gate.fr.md) | Liste à dérouler à chaque version |
| [test-build-spec.fr.md](test-build-spec.fr.md) | Journaux, rapports de plantage et export F9 de la build de test |
| [test-build-instructions.fr.md](test-build-instructions.fr.md) | Pour les testeurs : vérifier, ouvrir (Gatekeeper, SmartScreen), trouver les journaux |
| [test-protocol.fr.md](test-protocol.fr.md) | Session d'une heure et scénarios A à G |
| [test-checklist.fr.md](test-checklist.fr.md) | Liste de contrôle d'une page pour les testeurs |
| [feedback-form.fr.md](feedback-form.fr.md), [bug-template.fr.md](bug-template.fr.md) | Ce que renvoient les testeurs |
| [brands-rule.fr.md](brands-rule.fr.md) | Noms et logos de commerces dans les builds partagées |
| [../../THIRD_PARTY_NOTICES.fr.md](../../THIRD_PARTY_NOTICES.fr.md) | Crédits et licences |

Outils de `tools/qa/` (tests : `python3 -I -m unittest discover -s tools/qa/tests`) :
`stamp_version.py` (version depuis le tag), `make_archives.py` (zips de 2 Gio
au plus et `SHA256SUMS`), `check_release_bundle.py`, `check_game_log.py`,
`check_notices.py`, `day_report.py` (compte rendu chiffré depuis les preuves git).
