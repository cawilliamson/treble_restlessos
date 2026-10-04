/*
 *  Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *  Not a Contribution, Apache license notifications and license are retained
 *  for attribution purposes only.
 *
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _BDROID_BUILDCFG_H
#define _BDROID_BUILDCFG_H

//#include <cutils/properties.h>
#include <string.h>

static inline const char* BtmDefLocalName()
{
#if 0
    static char product_device[PROPERTY_VALUE_MAX];
    //Huawei-defined property
    property_get("ro.config.marketing_name", product_device, "");

    if (strcmp(product_device, "") != 0)
        return product_device;

    // Fallback to ro.product.model
    return "";
#endif
    return "hello";
}

static inline int BtmBypassExtraAclSetup() {
#if 0
    int8_t prop = property_get_bool("persist.sys.bt_acl_bypass", false);
    return prop == true;
#endif
    return 1;
}

#define BTM_DEF_LOCAL_NAME BtmDefLocalName()
#define BTM_BYPASS_EXTRA_ACL_SETUP BtmBypassExtraAclSetup()

#endif
