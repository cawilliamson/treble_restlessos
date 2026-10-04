#!/bin/bash

SUBJECT="/C=GB/ST=Fife/L=Dunfermline/O=cawilliamson/OU=cawilliamson/CN=cawilliamson/emailAddress=contact@chrisaw.io"

CERTS=(
    # essential keys
    "bluetooth"
    "cyngn-app"
    "gmscompat_lib"
    "media"
    "networkstack"
    "nfc"
    "platform"
    "releasekey"
    "sdk_sandbox"
    "shared"
    "testcert"
    "testkey"
    "verity"

    # apex container keys (system partition - GSI)
    "com.android.adbd"
    "com.android.adservices"
    "com.android.appsearch"
    "com.android.art"
    "com.android.bt"
    "com.android.cellbroadcast"
    "com.android.compos"
    "com.android.configinfrastructure"
    "com.android.conscrypt"
    "com.android.crashrecovery"
    "com.android.devicelock"
    "com.android.extservices"
    "com.android.healthfitness"
    "com.android.i18n"
    "com.android.ipsec"
    "com.android.media"
    "com.android.media.swcodec"
    "com.android.mediaprovider"
    "com.android.neuralnetworks"
    "com.android.nfcservices"
    "com.android.npumanager"
    "com.android.ondevicepersonalization"
    "com.android.os.statsd"
    "com.android.permission"
    "com.android.profiling"
    "com.android.resolv"
    "com.android.rkpd"
    "com.android.runtime"
    "com.android.scheduling"
    "com.android.sdkext"
    "com.android.telephonycore"
    "com.android.tethering"
    "com.android.tzdata"
    "com.android.uprobestats"
    "com.android.uwb"
    "com.android.virt"
    "com.android.webapp"
    "com.android.wifi"

    # module-specific APK certificates (for APKs embedded inside APEX modules)
    "com.android.adservices.api"
    "com.android.appsearch.apk"
    "com.android.bluetooth"
    "com.android.connectivity.resources"
    "com.android.federatedcompute"
    "com.android.health.connect.backuprestore"
    "com.android.healthconnect.controller"
    "com.android.hotspot2.osulogin"
    "com.android.nearby.halfsheet"
    "com.android.networkstack.tethering"
    "com.android.safetycenter.resources"
    "com.android.telecom.resources"
    "com.android.telecomui"
    "com.android.uwb.resources"
    "com.android.wifi.dialog"
    "com.android.wifi.resources"
)

# Copy make_key tool locally and patch it to use 4096-bit keys
echo "Copying make_key tool locally..."
cp /repo/src/development/tools/make_key ./make_key
echo "Patching make_key to use 4096-bit keys..."
sed -i 's|2048|4096|g' ./make_key

# Generate certs
for cert in "${CERTS[@]}"; do
    echo "Generating $cert certificate.."
    echo "" | ./make_key "$cert" "$SUBJECT"
    openssl pkcs8 -in "${cert}.pk8" -inform DER -nocrypt -out "${cert}.pem"
done

# Remove patched binary
rm -f make_key

# Output status
echo "Done!"
