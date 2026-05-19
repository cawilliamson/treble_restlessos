# ROM configuration for RestlessOS builds
# this file is copied to device/phh/treble/restlessos.mk during the build process

# ensure product fonts directory exists for third-party font modules
$(shell mkdir -p $(PRODUCT_OUT)/system/product/fonts)

# google pixel gsans fonts (issue #63)
$(call inherit-product-if-exists, vendor/pixel/gsans/common/common-vendor.mk)
