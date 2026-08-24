#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TOP_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="${TOP_DIR}/src"
PATCHES_DIR="${TOP_DIR}/patches"


usage() {
  cat <<EOF
usage: $(basename "$0") [options]

apply every patch tier in the same order as the build pipeline and stop
hard on the first failure. use this after editing a patch to verify the
full apply pass end-to-end.

run scripts/reset-sources.sh first (or pass --reset) so repos are at their
upstream base before applying, otherwise git am will fail on already-applied
patches.

options:
  --debug     apply debug-builds tier instead of release-builds
  --reset     reset src/ to upstream base before applying
  -h, --help  show this help
EOF
}

DEBUG=0
RESET=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug)
      DEBUG=1
      shift
      ;;
    --reset)
      RESET=1
      shift
      ;;
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

if [[ ! -d "$SRC_DIR/.repo" ]]; then
  echo "ERROR: no .repo found under ${SRC_DIR}; run scripts/sync-sources.sh first" >&2
  exit 1
fi

cd "$SRC_DIR"

if [[ "$RESET" -eq 1 ]]; then
  echo "==> resetting src/ to upstream base"
  "$SCRIPT_DIR/reset-sources.sh"
fi

TIERS=(trebledroid trebledroid-staging rom personal)
if [[ "$DEBUG" -eq 1 ]]; then
  TIERS+=(debug-builds)
else
  TIERS+=(release-builds)
fi

echo "==> applying tiers: ${TIERS[*]}"
failed_tier=""
failed_patch=""

for tier in "${TIERS[@]}"; do
  echo "--- tier: ${tier} ---"
  if ! OUTPUT="$("${PATCHES_DIR}/apply.sh" . "${tier}" 2>&1)"; then
    failed_tier="$tier"
    failed_patch="$(echo "$OUTPUT" | grep -oE ">> [^ ]+" | tail -1 | sed 's/>> //')"
    echo "$OUTPUT"
    echo ""
    echo "FAILED: tier '${failed_tier}'"
    [[ -n "$failed_patch" ]] && echo "  last patch: ${failed_patch}"
    echo "  abort with: cd \$SRC && git am --abort   (or rerun with --reset)"
    exit 1
  fi
  echo "$OUTPUT"
done

echo ""
echo "PASS: all tiers applied cleanly."