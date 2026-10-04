#
# Copyright (C) 2024 The LineageOS Project
#
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(WITH_GMS),true)

# Inherit from the proprietary version
$(call inherit-product, vendor/pixel/gsans/common/common-vendor.mk)

PRODUCT_PACKAGE_OVERLAYS += \
    vendor/pixel/gsans/overlay

endif
