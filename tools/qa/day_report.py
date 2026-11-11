#!/usr/bin/env python3
"""Assemble the 24-hour figures report (Markdown, French) from git evidence.

Reads, without checking anything out:
  * commits that entered the integration branch in the window,
  * PC proofs (compile verdicts, packages) from a proofs branch,
  * Mac evidence (result.txt, *.summary.txt, state files) from an evidence branch,
  and writes a report with one table per source plus a list of what is missing.
The report states only what the proofs say; an empty source is reported as such.

Usage: day_report.py --since 2026-11-11T22:00 [--until ...] [--next-ref origin/next]
                     [--pc-ref origin/feat/pc-proofs-20261111]
                     [--mac-ref origin/feat/mac-evidence-20261112] [--out FILE]
"""
import argparse
import re
import subprocess
import sys

VERDICT_RE = re.compile(r"VERDICT:\s*(\w+)\s+(\S+)\s+@(\w+)(?:\s+\((\d+)s\))?")
PERCENTILE_RE = re.compile(r"p50\D+([\d.,]+)\D+p95\D+([\d.,]+)", re.I)


def git(repo, *args):
    out = subprocess.run(["git", "-C", repo, *args], capture_output=True, text=True)
    return out.stdout if out.returncode == 0 else ""


def parse_proof(name, text):
    """Classify one PC proof file. Returns dict(kind, status, detail)."""
    verdict = VERDICT_RE.search(text)
    if verdict:
        return {"kind": "compile", "status": verdict.group(1), "detail": f"{verdict.group(2)} @{verdict.group(3)}"}
    if name.startswith("package"):
        failed = "failed" in text.lower()
        return {"kind": "package", "status": "FAIL" if failed else "OK", "detail": name}
    first = next((l.strip("# ").strip() for l in text.splitlines() if l.strip()), name)
    return {"kind": "other", "status": "-", "detail": first}


def parse_percentiles(text):
    match = PERCENTILE_RE.search(text)
    if not match:
        return None
    return tuple(float(v.replace(",", ".")) for v in match.groups())


def section_commits(repo, ref, since, until):
    args = ["log", ref, f"--since={since}", "--no-merges", "--format=%h %s"]
    if until:
        args.append(f"--until={until}")
    lines = [l for l in git(repo, *args).splitlines() if l.strip()]
    out = [f"## Intégré sur `{ref}`", "", f"{len(lines)} commits dans la fenêtre.", ""]
    out += [f"- `{l.split(' ', 1)[0]}` {l.split(' ', 1)[1] if ' ' in l else ''}" for l in lines[:40]]
    if len(lines) > 40:
        out.append(f"- … et {len(lines) - 40} autres")
    return out + [""]


def section_pc(repo, ref):
    files = [f for f in git(repo, "ls-tree", "-r", "--name-only", ref).splitlines() if f.startswith("tools/pc-proofs/") and f.endswith(".md")]
    rows, counts = [], {"OK": 0, "FAIL": 0}
    for f in sorted(files):
        name = f.rsplit("/", 1)[1][:-3]
        proof = parse_proof(name, git(repo, "show", f"{ref}:{f}"))
        if proof["kind"] in ("compile", "package"):
            ok = proof["status"].upper() in ("OK", "PASS")
            counts["OK" if ok else "FAIL"] += 1
            rows.append(f"| {proof['kind']} | {proof['detail']} | {'OK' if ok else 'ÉCHEC (' + proof['status'] + ')'} |")
    out = ["## PC Windows (preuves)", ""]
    if not files:
        return out + ["Aucune preuve trouvée sur la branche.", ""]
    out += [f"{counts['OK']} contrôles réussis, {counts['FAIL']} échecs.", "", "| Type | Objet | Résultat |", "| ---- | ---- | ---- |"] + rows
    return out + [""]


def section_mac(repo, ref):
    files = git(repo, "ls-tree", "-r", "--name-only", ref).splitlines()
    wanted = [f for f in files if f.endswith(("result.txt", ".summary.txt", "Builds_ETAT.md"))
              and not f.startswith(("GranTheftPraline/", "production/evidence/0."))]
    out = ["## Mac (preuves)", ""]
    if not wanted:
        return out + ["Aucune mesure trouvée sur la branche.", ""]
    rows = ["| Source | p50 ms | p95 ms | Verdict |", "| ---- | ---- | ---- | ---- |"]
    notes = []
    for f in sorted(wanted):
        text = git(repo, "show", f"{ref}:{f}")
        pct = parse_percentiles(text)
        verdict = re.search(r"verdict=(\w+)", text)
        if pct or verdict:
            rows.append(f"| `{f}` | {pct[0] if pct else '-'} | {pct[1] if pct else '-'} | {verdict.group(1) if verdict else '-'} |")
        else:
            notes.append(f"- `{f}` : {text.strip().splitlines()[-1] if text.strip() else '(vide)'}")
    if len(rows) > 2:
        out += rows + [""]
    else:
        out += ["Pas encore de chiffres de temps d'image ; états seulement :", ""]
    return out + notes + [""]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument("--repo", default=".")
    p.add_argument("--since", required=True)
    p.add_argument("--until", default="")
    p.add_argument("--next-ref", default="origin/next")
    p.add_argument("--pc-ref", default="origin/feat/pc-proofs-20261111")
    p.add_argument("--mac-ref", default="origin/feat/mac-evidence-20261112")
    p.add_argument("--out")
    a = p.parse_args(argv)
    lines = [f"# Compte rendu chiffré (depuis {a.since})", "",
             "Généré par `tools/qa/day_report.py` à partir des preuves git ; rien n'y est supposé.", ""]
    lines += section_commits(a.repo, a.next_ref, a.since, a.until)
    lines += section_pc(a.repo, a.pc_ref)
    lines += section_mac(a.repo, a.mac_ref)
    text = "\n".join(lines) + "\n"
    if a.out:
        with open(a.out, "w", encoding="utf-8") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
