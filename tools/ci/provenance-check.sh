#!/usr/bin/env bash
# Provenance check: every commit must come from the project identity, and the
# tree must stay free of third-party tooling markers.
set -euo pipefail

EXPECTED_EMAIL="106993577+nic0lasm@users.noreply.github.com"
# Forbidden markers, stored encoded so this file does not trip its own check.
MARKERS="$(printf '%s' 'Y2xhdWRlfGFudGhyb3BpYw==' | base64 -d)"
fail=0

err() {
  echo "::error::$*"
  fail=1
}

# 1. Commit identities and messages
for sha in $(git rev-list HEAD); do
  short="${sha:0:7}"
  ae="$(git log -1 --format=%ae "$sha")"
  ce="$(git log -1 --format=%ce "$sha")"
  test "$ae" = "$EXPECTED_EMAIL" || err "$short: author email '$ae' is not the project identity"
  test "$ce" = "$EXPECTED_EMAIL" || err "$short: committer email '$ce' is not the project identity"
  if git log -1 --format=%B "$sha" | grep -qiE "co-authored-by|$MARKERS"; then
    err "$short: commit message contains a forbidden trailer or marker"
  fi
done

# 2. Tracked file names
if git ls-files | grep -iE "(^|/)\.?($MARKERS)(\.[a-z]+)?(/|$)"; then
  err "forbidden tooling files are tracked (see above)"
fi

# 3. Tracked file contents
if git grep -Iil -E "$MARKERS" HEAD -- . ; then
  err "forbidden markers found in tracked files (see above)"
fi

# 4. Branch naming
branch="${GITHUB_HEAD_REF:-${GITHUB_REF_NAME:-$(git rev-parse --abbrev-ref HEAD)}}"
case "$branch" in
  main|next|feat/*|fix/*) ;;
  *) err "branch '$branch' must be main, next, feat/* or fix/*" ;;
esac

if test "$fail" -ne 0; then
  echo "Provenance check failed."
  exit 1
fi
echo "Provenance check passed: $(git rev-list --count HEAD) commits on '$branch'."
