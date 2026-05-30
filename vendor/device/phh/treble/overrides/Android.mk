LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := phh-gapps-go-overrides
PACKAGES.$(LOCAL_MODULE).OVERRIDES := \
	Wellbeing \
	GMailGo \
	YouTubeGo \
	DuoGo \
	Traceur \
	GmsSampleIntegration \
	GmsSampleIntegrationGo \
	GmsSampleIntegrationGo2GB \
	GmsSampleIntegrationGo512M \
	GmsEEAType1Integration \
	GmsEEAType1IntegrationGo \
	GmsEEAType2Integration \
	GmsEEAType2IntegrationGo \
	GmsEEAType3aIntegration \
	GmsEEAType3aIntegrationGo \
	GmsEEAType3bIntegration \
	GmsEEAType3bIntegrationGo \
	GmsEEAType4aIntegration \
	GmsEEAType4aIntegrationGo \
	GmsEEAType4bIntegration \
	GmsEEAType4bIntegrationGo \
	GmsEEAType4cIntegration \
	GmsEEAType4cIntegrationGo

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := nothing.txt
LOCAL_UNINSTALLABLE_MODULE := true
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := phh-gapps-overrides
PACKAGES.$(LOCAL_MODULE).OVERRIDES := \
	Drive \
	YTMusic \
	Videos \
	Duo \
	Traceur \
	Wellbeing \
	YouTube \
	Photos \
	Maps \
	Gmail2 \
	FilesGoogle \
	GmsSampleIntegration \
	GmsSampleIntegrationGo \
	GmsSampleIntegrationGo2GB \
	GmsSampleIntegrationGo512M \
	GmsEEAType1Integration \
	GmsEEAType1IntegrationGo \
	GmsEEAType2Integration \
	GmsEEAType2IntegrationGo \
	GmsEEAType3aIntegration \
	GmsEEAType3aIntegrationGo \
	GmsEEAType3bIntegration \
	GmsEEAType3bIntegrationGo \
	GmsEEAType4aIntegration \
	GmsEEAType4aIntegrationGo \
	GmsEEAType4bIntegration \
	GmsEEAType4bIntegrationGo \
	GmsEEAType4cIntegration \
	GmsEEAType4cIntegrationGo

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_ETC)
LOCAL_SRC_FILES := nothing.txt
LOCAL_UNINSTALLABLE_MODULE := true
include $(BUILD_PREBUILT)
