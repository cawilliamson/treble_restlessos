#
# Copyright (C) 2020 Raphielscape LLC. and Haruka LLC.
#
# Licensed under the Apache License, Version 2.0 (the License);
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an AS IS BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#

LOCAL_PATH := $(call my-dir)

### pixel_charger_res_images ###
ifneq ($(strip $(LOCAL_CHARGER_NO_UI)),true)
define _add-product-charger-image
include $$(CLEAR_VARS)
LOCAL_MODULE := pixel_charger_res_images_charger_$(notdir $(1))
LOCAL_MODULE_STEM := $(notdir $(1))
_img_modules += $$(LOCAL_MODULE)
LOCAL_SRC_FILES := $1
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $$(TARGET_OUT_PRODUCT)/etc/res/images/charger
LOCAL_PRODUCT_MODULE := true
include $$(BUILD_PREBUILT)
endef

_img_modules :=
_images :=
$(foreach _img, $(call find-subdir-subdir-files, "images/charger", "*.png"), \
  $(eval $(call _add-product-charger-image,$(_img))))

### pixel_charger_animation_file ###
define _add-product-charger-animation-file
include $$(CLEAR_VARS)
LOCAL_MODULE := pixel_charger_res_values_charger_$(notdir $(1))
LOCAL_MODULE_STEM := $(notdir $(1))
_anim_modules += $$(LOCAL_MODULE)
LOCAL_SRC_FILES := $1
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $$(TARGET_OUT_PRODUCT)/etc/res/values/charger
LOCAL_PRODUCT_MODULE := true
include $$(BUILD_PREBUILT)
endef

_anim_modules :=
$(foreach _txt, $(call find-subdir-subdir-files, "values/charger", "*.txt"), \
  $(eval $(call _add-product-charger-animation-file,$(_txt))))

include $(CLEAR_VARS)
LOCAL_MODULE := product_charger_res_images
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := $(_img_modules) $(_anim_modules)
include $(BUILD_PHONY_PACKAGE)

_add-product-charger-image :=
_add-product-charger-animation-file :=
_img_modules :=
_anim_modules :=
endif # LOCAL_CHARGER_NO_UI
