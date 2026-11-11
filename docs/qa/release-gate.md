# Version gate checklist

*French version: [release-gate.fr.md](release-gate.fr.md).*

Run for every version (0.4, 0.5, …, 0.9-rc), after the merge gate of
[the test-phase plan §5.2](../../production/test-phase-plan.md) has passed
for each merge. Tick every line in the release note; a line not done is
written as a known issue.

## Build and numbers
- [ ] `next` commit chosen, CI green on that exact commit (Provenance check and tests).
- [ ] Tag decided by Nico; `python3 tools/qa/stamp_version.py <tag> --check` passes (ProjectVersion = tag).
- [ ] Compile check on the PC (MSVC, editor and game Win64) of the same commit.
- [ ] macOS package and Windows package built from that commit; `build-info.txt` shows it.

## Automatic checks
- [ ] Smoke test on each package: launch, 60 s scripted play, clean exit, `tools/qa/check_game_log.py` PASS, no crash report.
- [ ] M1 bench, tier C capped at 40 fps, 4 scenes of 120 s: ≥ 99 % of frames at 25.0 ms ± 1, no frame > 100 ms, GPU p95 ≤ 24 ms.
- [ ] Windows bench (m1 and Ultra): no frame > 100 ms.
- [ ] 600 s per scene; 30-minute free session without crash on each machine (60 min for 0.9).
- [ ] Memory measured (≤ 9 GB on the M1 without swap for 0.9).
- [ ] Launch-to-play time measured (≤ 60 s on the M1 for 0.9).

## Release content
- [ ] `python3 tools/qa/make_archives.py` run on each package; archives ≤ 2 GiB; `SHA256SUMS` written.
- [ ] `python3 tools/qa/check_release_bundle.py <folder>` PASS (no logos, no source models, no private paths).
- [ ] `python3 tools/qa/check_notices.py` PASS; credits screen matches `THIRD_PARTY_NOTICES`.
- [ ] Release notes and player guide in FR and EN (`docs/releases/<version>.md` and `.fr.md`), known issues listed.
- [ ] Systems inventory and roadmap status updated.
- [ ] Proof video of the version kept (Git LFS under `production/evidence/<version>/`).
- [ ] Off-Git backup current (block pasted by Nico).

## For a test build (0.9-rc)
- [ ] Both archives opened with the instructions only, on a clean macOS account and a clean Windows account (Gatekeeper and SmartScreen paths work as written).
- [ ] Test protocol scenarios A to G run by a thread or by Nico before anyone else.
- [ ] Brands rule applied (default archive is brand-free).

## Shipping the version (done by Nico, from his Mac)
Tag, GitHub release with the archives or a private link, issue closing: a
single zsh block prepared by the QA thread and pasted by Nico. Nothing is
written to GitHub from the cloud.
