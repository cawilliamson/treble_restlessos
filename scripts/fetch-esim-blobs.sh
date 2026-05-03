#!/usr/bin/env bash
set -euo pipefail

# fetch-esim-blobs.sh — extract EuiccGoogle LPA from a Pixel factory image
# via adevtool download + manual extraction. avoids adevtool generate-all
# which triggers a heavy internal dep build that fails in CI.

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TOP_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="${TOP_DIR}/src"

if [ ! -f "${SRC_DIR}/build/release/release_config_map.textproto" ]; then
    echo "ERROR: Source not synced. Run sync-sources.sh first." >&2
    exit 1
fi

# If device codename provided as argument, use it directly
if [ "${1:-}" ]; then
    DEVICE="$1"
    echo "Device (from arg): ${DEVICE}"
else
    # Map build target to adevtool device codename (e.g. bp4a → rango)
    CODENAME=$(grep --max-count=1 "target:" \
        "${SRC_DIR}/build/release/release_config_map.textproto" \
        | sed 's/.*"\([^"]*\)".*/\1/')
    echo "Target device: ${CODENAME}"

    if [ -f "${SRC_DIR}/vendor/adevtool/config/device/${CODENAME}.yml" ]; then
        DEVICE="${CODENAME}"
    else
        DEVICE=$(grep "factory:" \
            "${SRC_DIR}/vendor/adevtool/config/build-index/build-index-main.yml" \
            | grep "\-${CODENAME}\." \
            | head -1 \
            | awk '{print $NF}' \
            | sed 's/-.*//')
    fi

    if [ -z "${DEVICE}" ]; then
        echo "ERROR: Could not map build target ${CODENAME} to a device config." >&2
        exit 1
    fi
    echo "Device: ${DEVICE}"
fi

# Install adevtool dependencies (once)
if [ ! -d "${SRC_DIR}/vendor/adevtool/node_modules" ]; then
    echo "Installing adevtool dependencies..."
    yarn --cwd "${SRC_DIR}/vendor/adevtool/" install
fi
# Download factory image using adevtool (this step works without dep build)
echo "Downloading factory image for ${DEVICE}..."
pushd "${SRC_DIR}" > /dev/null
ADEVTOOL_SKIP_DEP_BUILD=1 vendor/adevtool/bin/run download -d "${DEVICE}"
popd > /dev/null

# Find the downloaded factory zip
FACTORY_ZIP=$(ls -1 "${SRC_DIR}/vendor/adevtool/dl/${DEVICE}"*-factory-*.zip 2>/dev/null | head -1)
if [ -z "${FACTORY_ZIP}" ] || [ ! -f "${FACTORY_ZIP}" ]; then
    echo "ERROR: Factory image not found in vendor/adevtool/dl/" >&2
    exit 1
fi
echo "Factory image: ${FACTORY_ZIP}"

# Extract APK manually
TMPDIR=$(mktemp -d)
trap 'rm -rf "${TMPDIR}"' EXIT

echo "Extracting inner image zip..."
unzip -q "${FACTORY_ZIP}" -d "${TMPDIR}"

INNER_ZIP=$(find "${TMPDIR}" -name "image-*.zip" | head -1)
if [ -z "${INNER_ZIP}" ]; then
    echo "ERROR: Inner image zip not found" >&2
    exit 1
fi

echo "Extracting product.img..."
if ! unzip -q -o "${INNER_ZIP}" product.img -d "${TMPDIR}" 2>/dev/null; then
    echo "ERROR: product.img not found in factory image (dynamic partitions may use super.img instead)" >&2
    exit 1
fi

IMG="${TMPDIR}/product.img"
DEST="${SRC_DIR}/vendor/hardware_overlay/EuiccGoogle"
mkdir -p "${DEST}"

# Detect filesystem type and extract APK
echo "Detecting filesystem type..."
ERofsMagic=$(python3 -c "import sys; f=open('$IMG','rb'); f.seek(0x400); print(f.read(4).hex()); f.close()")
Ext4Magic=$(python3 -c "import sys; f=open('$IMG','rb'); f.seek(0x438); print(f.read(2).hex()); f.close()")

if [ "${ERofsMagic}" = "e2e1f5e0" ]; then
    echo "Filesystem: EROFS"
    fsck.erofs --extract="${TMPDIR}/extracted" "${IMG}"
    cp "${TMPDIR}/extracted/priv-app/EuiccGoogle/EuiccGoogle.apk" "${DEST}/"
elif [ "${Ext4Magic}" = "53ef" ]; then
    echo "Filesystem: ext4"
    debugfs -R "cat /priv-app/EuiccGoogle/EuiccGoogle.apk" "${IMG}" > "${DEST}/EuiccGoogle.apk"
else
    echo "ERROR: Unknown filesystem type (erofs magic: ${ERofsMagic}, ext4 magic: ${Ext4Magic})" >&2
    exit 1
fi

# Copy feature XML
XML_SRC="${SRC_DIR}/frameworks/native/data/etc/android.hardware.telephony.euicc.xml"
if [ -f "$XML_SRC" ]; then
    cp "$XML_SRC" "${DEST}/android.hardware.telephony.euicc.xml"
else
    cat > "${DEST}/android.hardware.telephony.euicc.xml" << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<permissions>
    <feature name="android.hardware.telephony.euicc" />
</permissions>
XML
fi

echo "EuiccGoogle APK + feature XML copied to ${DEST}"
echo "Done"
