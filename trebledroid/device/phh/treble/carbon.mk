$(call inherit-product, vendor/carbon/config/gsm.mk)
$(call inherit-product, vendor/carbon/config/common.mk)
PRODUCT_PROPERTY_OVERRIDES += \
    ro.carbon.maintainer="phh"

#By default, CarbonROM copies it into root/init.carbon.rc
#For this to work in GSI, we put it in /system
PRODUCT_COPY_FILES += \
	vendor/carbon/prebuilt/etc/init.carbon.rc:system/etc/init/init.carbon.rc
