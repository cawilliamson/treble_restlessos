LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := phh-overrides
PACKAGES.phh-overrides.OVERRIDES := \
	webview \
	HTMLViewer

ifneq ($(wildcard vendor/gapps),)
PACKAGES.phh-overrides.OVERRIDES += \
	RecorderPrebuilt \
	NgaResources \
	Photos \
	WallpaperPickerGoogleRelease \
	Music \
	QuickSearchBox \
	Traceur \
	WellbeingPrebuilt \
	TipsPrebuilt \
	Turbo \
	TurboPrebuilt \
	Browser2 \

endif


LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
# This will install the file in /system/etc/permissions
LOCAL_MODULE_PATH := $(TARGET_OUT_PRODUCT_ETC)
LOCAL_SRC_FILES := phh.txt
LOCAL_UNINSTALLABLE_MODULE := true
include $(BUILD_PREBUILT)
