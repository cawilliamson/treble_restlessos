BUILD_GMS := yes
ifneq ($(wildcard vendor/google)$(wildcard vendor/partner_gms),)
-include vendor/google/products/gms.mk
-include vendor/partner_gms/products/gms.mk
PRODUCT_SHIPPING_API_LEVEL :=

PRODUCT_PACKAGES := $(filter-out CalendarGoogle, $(PRODUCT_PACKAGES))
PRODUCT_PACKAGES := $(filter-out GoogleContacts, $(PRODUCT_PACKAGES))

PRODUCT_PACKAGES += \
	phh-gapps-overrides \
	Chrome \
	GoogleContactsSyncAdapter \
	talkback \

PRODUCT_SYSTEM_DEFAULT_PROPERTIES := $(filter-out ro.boot.vendor.overlay.theme=%,$(PRODUCT_SYSTEM_DEFAULT_PROPERTIES))
endif

ifneq ($(wildcard vendor/opengapps),)
PRODUCT_COPY_FILES += \
	device/phh/treble/empty-permission.xml:system/etc/permissions/com.google.android.camera2.xml \
	device/phh/treble/empty-permission.xml:system/etc/permissions/com.google.android.camera.experimental2015.xml \
	device/phh/treble/empty-permission.xml:system/etc/permissions/com.google.android.camera.experimental2016.xml \
	device/phh/treble/empty-permission.xml:system/etc/permissions/com.google.android.camera.experimental2017.xml

PRODUCT_PACKAGE_OVERLAYS += device/phh/treble/overlay-gapps
GAPPS_VARIANT := pico
DONT_DEXPREOPT_PREBUILTS := true
WITH_DEXPREOPT_BOOT_IMG_AND_SYSTEM_SERVER_ONLY := true
GAPPS_FORCE_PACKAGE_OVERRIDES := true
GAPPS_FORCE_BROWSER_OVERRIDES := true
GAPPS_FORCE_WEBVIEW_OVERRIDES := true
PRODUCT_PACKAGES += \
       Chrome \
       CalculatorGoogle \
       PrebuiltDeskClockGoogle \
       CalendarGooglePrebuilt \
       LatinImeGoogle \
       phh-overrides

$(call inherit-product, vendor/opengapps/build/opengapps-packages.mk)
endif

#Pixel Experience gapps
ifneq ($(wildcard vendor/gapps),)
IS_PHONE := true
$(call inherit-product,vendor/gapps/config.mk)
PRODUCT_PACKAGES += \
       phh-overrides \
	   com.simplemobiletools.gallery.pro \

endif
