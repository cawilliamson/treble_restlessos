#!/usr/bin/env bash
set -euo pipefail

# fetch-esim-blobs.sh — extract EuiccGoogle LPA from a Pixel factory image
# via adevtool (same mechanism GrapheneOS uses for Pixel builds).

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
TOP_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="${TOP_DIR}/src"

if [ ! -f "${SRC_DIR}/build/release/release_config_map.textproto" ]; then
    echo "ERROR: Source not synced. Run sync-sources.sh first." >&2
    exit 1
fi

# Extract the Pixel device codename from GrapheneOS release config
# (same logic as sync-sources.sh)
CODENAME=$(grep --max-count=1 "target:" \
    "${SRC_DIR}/build/release/release_config_map.textproto" \
    | sed 's/.*"\([^"]*\)".*/\1/')

if [ -z "$CODENAME" ]; then
    echo "ERROR: Could not determine Pixel device codename from release config." >&2
    exit 1
fi

echo "Target device: ${CODENAME}"

# Install adevtool dependencies (once)
if [ ! -d "${SRC_DIR}/vendor/adevtool/node_modules" ]; then
    echo "Installing adevtool dependencies..."
    yarn --cwd "${SRC_DIR}/vendor/adevtool/" install
fi

# Run adevtool to extract proprietary blobs from the Pixel factory image
echo "Running adevtool generate-all -d ${CODENAME}..."
"${SRC_DIR}/vendor/adevtool/node_modules/.bin/adevtool" generate-all -d "$CODENAME"

# The APK now lives at:
SKEL_DIR="${SRC_DIR}/vendor/adevtool/vendor-skels/google_devices/${CODENAME}"
APK_SRC="${SKEL_DIR}/proprietary/product/priv-app/EuiccGoogle/EuiccGoogle.apk"
XML_SRC="${SRC_DIR}/frameworks/native/data/etc/android.hardware.telephony.euicc.xml"

if [ ! -f "$APK_SRC" ]; then
    echo "ERROR: EuiccGoogle.apk not found at ${APK_SRC}" >&2
    echo "       adevtool may not have extracted it. Check the device codename." >&2
    exit 1
fi

# Copy into vendor/hardware_overlay where the GSI build picks it up
DEST="${SRC_DIR}/vendor/hardware_overlay/EuiccGoogle"
mkdir -p "$DEST"

cp "$APK_SRC" "${DEST}/EuiccGoogle.apk"

# Use the AOSP feature XML (it's identical to stock — just declares the feature)
if [ -f "$XML_SRC" ]; then
    cp "$XML_SRC" "${DEST}/android.hardware.telephony.euicc.xml"
else
    # Fallback: create minimal feature XML
    cat > "${DEST}/android.hardware.telephony.euicc.xml" << 'XML'
<?xml version="1.0" encoding="utf-8"?>
<permissions>
    <feature name="android.hardware.telephony.euicc" />
</permissions>
XML
fi

echo "EuiccGoogle APK + feature XML copied to ${DEST}"
echo "Done — ready for patch application and build."
