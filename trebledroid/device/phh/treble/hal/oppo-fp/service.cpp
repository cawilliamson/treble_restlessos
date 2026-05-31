/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "android.hardware.biometrics.fingerprint@2.1-service.realme_sdm710"

#include <android-base/logging.h>
#include <hidl/HidlTransportSupport.h>

#include "BiometricsFingerprint.h"

using android::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprint;
using android::hardware::biometrics::fingerprint::V2_1::implementation::BiometricsFingerprint;
using android::hardware::configureRpcThreadpool;
using android::hardware::joinRpcThreadpool;
using android::OK;
using android::sp;
using android::status_t;

int main() {
    sp<BiometricsFingerprint> biometricsFingerprint;
    status_t status;

    LOG(INFO) << "Fingerprint HAL Adapter service is starting.";

    biometricsFingerprint = new BiometricsFingerprint();
    if (biometricsFingerprint == nullptr) {
        LOG(ERROR) << "Can not create an instance of Fingerprint HAL Adapter BiometricsFingerprint Iface, exiting.";
        goto shutdown;
    }

    configureRpcThreadpool(1, true /*callerWillJoin*/);

    status = biometricsFingerprint->registerAsService();
    if (status != OK) {
        LOG(ERROR) << "Could not register service for Fingerprint HAL Adapter BiometricsFingerprint Iface ("
                   << status << ")";
        goto shutdown;
    }

    LOG(INFO) << "Fingerprint HAL Adapter service is ready.";
    joinRpcThreadpool();
    // Should not pass this line

shutdown:
    // In normal operation, we don't expect the thread pool to shutdown
    LOG(ERROR) << "Fingerprint HAL Adapter service is shutting down.";
    return 1;
}
