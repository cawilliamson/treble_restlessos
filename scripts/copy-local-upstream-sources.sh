#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
TOP_DIR="$(dirname "$SCRIPT_DIR")"
UPSTREAM_DIR="${TOP_DIR}/upstream/trebledroid"
SRC_DIR="${1:-${TOP_DIR}/src}"

# vendored trebledroid snapshots: <upstream dir>:<path under src/>
PAIRS=(
  device_phh_treble:device/phh/treble
  treble_app:treble_app
  vendor_hardware_overlay:vendor/hardware_overlay
  vendor_interfaces:vendor/interfaces
)

# these dirs are not in the repo manifest any more, so they carry no .git;
# patches/apply.sh falls back to git apply for them.
for pair in "${PAIRS[@]}"; do
  d="${pair%%:*}"
  t="${SRC_DIR}/${pair#*:}"
  [ -d "$UPSTREAM_DIR/$d" ] || {
    echo "ERROR: missing upstream/trebledroid/$d" >&2
    exit 1
  }
  rm -rf "$t"
  mkdir -p "$t"
  cp -aT "$UPSTREAM_DIR/$d" "$t"
  echo "$d -> ${pair#*:}"
done
