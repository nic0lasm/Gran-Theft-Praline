# QA and release

*French version: [README.fr.md](README.fr.md).*

| Document | Use |
| ---- | ---- |
| [release-gate.md](release-gate.md) | Checklist run for every version |
| [test-build-spec.md](test-build-spec.md) | Logs, crash reports and F9 export of the test build |
| [test-build-instructions.md](test-build-instructions.md) | For testers: verify, open (Gatekeeper, SmartScreen), find logs |
| [test-protocol.md](test-protocol.md) | One-hour session and scenarios A to G |
| [feedback-form.md](feedback-form.md), [bug-template.md](bug-template.md) | What testers send back |
| [brands-rule.md](brands-rule.md) | Real shop names and logos in shared builds |
| [../../THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md) | Credits and licences |

Tools in `tools/qa/` (tests: `python3 -I -m unittest discover -s tools/qa/tests`):
`stamp_version.py` (version from the tag), `make_archives.py` (zips of at most
2 GiB and `SHA256SUMS`), `check_release_bundle.py`, `check_game_log.py`,
`check_notices.py`.
