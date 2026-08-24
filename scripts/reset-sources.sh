#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TOP_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="${TOP_DIR}/src"


usage() {
  cat <<EOF
usage: $(basename "$0") [options]

reset every repo under ./src to its upstream base commit, discarding any
applied patches and untracked files. leaves src/ in a pristine state ready
for a fresh patch apply pass.

options:
  -h, --help   show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
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

if ! command -v repo &> /dev/null; then
  echo "ERROR: 'repo' not found on path" >&2
  exit 1
fi

if [[ ! -d "$SRC_DIR/.repo" ]]; then
  echo "ERROR: no .repo found under ${SRC_DIR}; nothing to reset" >&2
  exit 1
fi

cd "$SRC_DIR"

# abort any in-flight git am sessions, reset to the root (upstream) commit,
# then strip untracked and ignored files. repos are shallow clones, so the
# first parentless commit is the upstream base.
repo forall -c '
  git am --abort 2>/dev/null || true
  base="$(git rev-list --max-parents=0 HEAD | tail -1)"
  git reset --hard "$base" >/dev/null
  git clean -fdx >/dev/null
  echo "${REPO_PATH} reset"
'

echo ""
echo "reset complete: $(repo forall -c 'echo x' | wc -l) repos at upstream base."