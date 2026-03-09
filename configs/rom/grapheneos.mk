# ROM Configuration for GrapheneOS builds
# This file is copied to device/phh/treble/grapheneos.mk during the build process

# Force 64-bit only ABI list. Vanadium ships arm64-only native libs and
# targets SDK 36+; without this, vendor-advertised 32-bit ABIs cause
# PackageManager to reject it (forceMatch in derivePackageAbi).
PRODUCT_SYSTEM_PROPERTIES += \
    ro.product.cpu.abilist=arm64-v8a \
    ro.product.cpu.abilist32= \
    ro.product.cpu.abilist64=arm64-v8a
