#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
TOP_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="${TOP_DIR}/src"
PATCHES_DIR="${TOP_DIR}/patches"

usage() {
  cat <<EOF
usage: $(basename "$0") [options]

reset every repo referenced by a patch tier to its upstream base commit,
discarding any applied patches and untracked files, and restore the
vendored snapshots under upstream/ to pristine. leaves src/ ready for a
fresh patch apply pass, and fails loudly if any repo could not be reset.

options:
  -h, --help   show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
  -h | --help)
    usage
    exit 0
    ;;
  *)
    echo "unknown argument: $1" >&2
    usage >&2
    exit 1
    ;;
  esac
done

# every repo dir that appears in any patch tier
projects=$(
  for tier in trebledroid trebledroid-staging rom personal release-builds debug-builds; do
    d="${PATCHES_DIR}/${tier}"
    [[ -d "$d" ]] && ls "$d"
  done | sort -u
)

map_project_to_src() {
  local p
  p="$(tr _ / <<<"$1" | sed -e 's;platform/;;g')"
  [[ "$p" == build ]] && p=build/make
  [[ "$p" == testing ]] && p=platform_testing
  [[ "$p" == treble/app ]] && p=treble_app
  [[ "$p" == system/fs/fs/mgr ]] && p=system/fs/fs_mgr
  [[ "$p" == vendor/hardware/overlay ]] && p=vendor/hardware_overlay
  [[ "$p" == vendor/partner/gms ]] && p=vendor/partner_gms
  echo "$p"
}

# stale lock files and half-finished rebase/am state from interrupted runs
# make later resets fail silently, so clear them before anything else
find "$SRC_DIR" -name index.lock -delete 2>/dev/null || true
find "$SRC_DIR" -type d \( -name rebase-apply -o -name rebase-merge \) -exec rm -rf {} + 2>/dev/null || true

# repos are shallow clones, so the first parentless commit is the upstream
# base. verify every repo actually lands there with a clean tree -- a reset
# that fails silently here poisons the whole patch apply pass.
failures=0
for project in $projects; do
  path="${SRC_DIR}/$(map_project_to_src "$project")"
  if [[ ! -d "$path/.git" ]]; then
    continue # vendored snapshot or not synced; snapshots restored below
  fi
  if (
    cd "$path" || exit 1
    git am --abort >/dev/null 2>&1 || true
    base="$(git rev-list --max-parents=0 HEAD | tail -1)"
    git reset --hard "$base" >/dev/null || exit 1
    git clean -fdx >/dev/null || exit 1
    head="$(git rev-parse HEAD)"
    dirty="$(git status --porcelain)"
    [[ "$head" == "$base" && -z "$dirty" ]] || exit 1
  ); then
    echo "${project} reset"
  else
    echo "FAILED to reset: $project ($path)" >&2
    failures=$((failures + 1))
  fi
done

# vendored snapshots carry no .git so the loop never sees them; restore
# them from upstream/ instead
"$TOP_DIR/scripts/copy-local-upstream-sources.sh" "$SRC_DIR" >/dev/null

echo ""
if [[ "$failures" -ne 0 ]]; then
  echo "reset FAILED for $failures repo(s); fix the errors above before applying patches" >&2
  exit 1
fi
echo "reset complete: all patch-tier repos verified at upstream base, snapshots restored."
