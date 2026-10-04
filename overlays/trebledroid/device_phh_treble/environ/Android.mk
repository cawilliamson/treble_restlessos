include $(CLEAR_VARS)

LOCAL_MODULE := treble-environ-rc
LOCAL_MODULE_TAGS := optional

LOCAL_POST_INSTALL_CMD := \
	mkdir -p $(PRODUCT_OUT)/system/etc/init/ && \
	sed -e 's?%BOOTCLASSPATH%?$(PRODUCT_BOOTCLASSPATH)?g' device/phh/treble/environ/init.treble-environ.rc > $(PRODUCT_OUT)/system/etc/init/init.treble-environ.rc && \
	sed -i -e 's?%SYSTEMSERVERCLASSPATH%?$(PRODUCT_SYSTEM_SERVER_CLASSPATH)?g' $(PRODUCT_OUT)/system/etc/init/init.treble-environ.rc

include $(BUILD_PHONY_PACKAGE)
