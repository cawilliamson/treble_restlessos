#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
TOP_DIR="$(dirname "$SCRIPT_DIR")"
UPSTREAM_DIR="${TOP_DIR}/upstream"
SRC_DIR="${1:-${TOP_DIR}/src}"

# vendored upstream snapshots: <dir under upstream/>:<path under src/>
# snapshotted at: qcrilam dc599b6, lptools c8be7de, magisk d8056f8,
#                 gsans 817f7c8, vndk-tests 533390a
PAIRS=(
  andycgyan/QcRilAm:packages/apps/QcRilAm
  phhusson/lptools:vendor/lptools
  phhusson/magisk:vendor/magisk
  phhusson/vndk-tests:vendor/vndk-tests
  pixelos/gsans:vendor/pixel/gsans
  trebledroid/device_phh_treble:device/phh/treble
  trebledroid/treble_app:treble_app
  trebledroid/vendor_hardware_overlay:vendor/hardware_overlay
  trebledroid/vendor_interfaces:vendor/interfaces
)

# these dirs are not in the repo manifest any more, so they carry no .git;
# patches/apply.sh falls back to git apply for them.
for pair in "${PAIRS[@]}"; do
  d="${pair%%:*}"
  t="${SRC_DIR}/${pair#*:}"
  [ -d "$UPSTREAM_DIR/$d" ] || {
    echo "ERROR: missing upstream/$d" >&2
    exit 1
  }
  rm -rf "$t"
  mkdir -p "$(dirname "$t")"
  cp -aT "$UPSTREAM_DIR/$d" "$t"
  echo "$d -> ${pair#*:}"
done
