#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TOP_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="${TOP_DIR}/src"
TMP_DIR="${TOP_DIR}/tmp"
CONFIGS_DIR="${TOP_DIR}/configs"


usage() {
  cat <<EOF
usage: $(basename "$0") [options]

fetch latest android sources into ./src for patch development.

options:
  --channel <stable|beta|alpha>  grapheneos release channel (default: stable)
  -h, --help                     show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --channel)
      shift 2
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

if ! command -v repo &> /dev/null; then
  echo "ERROR: 'repo' not found on path" >&2
  exit 1
fi

RELEASES_HTML=$(curl --fail --silent "https://grapheneos.org/releases") || {
  echo "ERROR: failed to fetch releases page from grapheneos.org" >&2
  exit 1
}

TAG=$(echo "$RELEASES_HTML" \
  | grep -oP "id=[a-z]+-stable><td>[^<]+</td><td><a href=#\K[0-9]{10}" \
  | sort -nr | head -1)

if [[ -z "$TAG" ]]; then
  echo "ERROR: failed to extract latest stable tag" >&2
  exit 1
fi

echo "tag: ${TAG}"
rm -rf "${SRC_DIR}"

mkdir --parents "$SRC_DIR" "$TMP_DIR"
cd "$SRC_DIR"
INIT_FLAGS=(
  --depth=1
  --git-lfs
  --manifest-branch "refs/tags/${TAG}"
  --manifest-url https://github.com/GrapheneOS/platform_manifest.git
)

echo "y" | repo init "${INIT_FLAGS[@]}"

mkdir --parents .repo/local_manifests
cp --verbose -- "${CONFIGS_DIR}"/manifests/*.xml .repo/local_manifests/

JOBS=4

echo "repo sync --force-sync --jobs=${JOBS} --no-clone-bundle --no-tags ..."
while ! repo sync --force-sync --jobs="${JOBS}" --no-clone-bundle --no-tags; do
  echo "repo sync failed, retrying in 30s..."
  sleep 30
done

AOSP_TAG=$(grep --max-count=1 "aosp_revision:" .repo/manifests/config.yml | sed "s/.*: *//")
ANDROID_VERSION=$(echo "$AOSP_TAG" | sed "s/android-//;s/_r.*//")
echo "$ANDROID_VERSION" > "${TMP_DIR}/.android_version"

ANDROID_VERSION_TAG=$(grep --max-count=1 "target:" build/release/release_config_map.textproto \
  | sed 's/.*"\([^"]*\)".*/\1/')
echo "$ANDROID_VERSION_TAG" > "${TMP_DIR}/.android_version_tag"

echo ""
echo "sync complete."
echo "  grapheneos tag:        ${TAG}"
echo "  aosp revision:         ${AOSP_TAG}"
echo "  android version:       ${ANDROID_VERSION}"
echo "  android version tag:   ${ANDROID_VERSION_TAG}"
