LOCAL_PATH := $(call my-dir)

.PHONY: vndk-test-sepolicy

vndk-test-sepolicy-path := $(LOCAL_PATH)
vndk-test-sepolicy:
	bash "$(vndk-test-sepolicy-path)/run.sh" "$(vndk-test-sepolicy-path)" "$(PRODUCT_OUT)"
