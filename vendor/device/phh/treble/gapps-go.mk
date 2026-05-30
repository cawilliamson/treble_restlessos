BUILD_GMS := yes
#include not inherit, because we must edit some values
-include vendor/google/products/gms_go.mk
-include vendor/partner_gms/products/gms_go.mk

FILTER_OUT = $(foreach v,$(2),$(if $(findstring $(1),$(v)),,$(v)))

#gms_go.mk has a broken copy of google_go.xml => google.xml
#But there already is a google.xml => google.xml
#Only difference between those is maps go and youtube go applinks, so good enough
PRODUCT_COPY_FILES := $(call FILTER_OUT,google_go.xml:, $(PRODUCT_COPY_FILES))

#We need to use this way because it overrides sync adapters
PRODUCT_PACKAGES := $(filter-out CalendarGoogle, $(PRODUCT_PACKAGES))
PRODUCT_PACKAGES := $(filter-out GoogleContacts, $(PRODUCT_PACKAGES))

#Launcher3Go has no recents support...?
PRODUCT_PACKAGES := $(filter-out Launcher3Go, $(PRODUCT_PACKAGES))

PRODUCT_SHIPPING_API_LEVEL :=

PRODUCT_PACKAGES += \
	phh-gapps-go-overrides \
	GoogleContactsSyncAdapter \
