# ROM Configuration for GrapheneOS builds
# This file is copied to device/phh/treble/grapheneos.mk during the build process

# Force 64-bit only zygote. GrapheneOS Vanadium (Trichrome WebView/Chrome)
# only ships 64-bit native libraries. Without this, 32-bit apps on multilib
# devices fail to load WebView because there are no armeabi-v7a libs in the
# Trichrome APKs.
ZYGOTE_FORCE_64 := true
