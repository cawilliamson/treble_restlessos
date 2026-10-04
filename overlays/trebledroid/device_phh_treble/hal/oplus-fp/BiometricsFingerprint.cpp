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
#define LOG_TAG "android.hardware.biometrics.fingerprint@2.1-service.oplus.compat"
#define LOG_VERBOSE "android.hardware.biometrics.fingerprint@2.1-service.oplus.compat"

#include <hardware/hardware.h>
#include <hardware/fingerprint.h>
#include "BiometricsFingerprint.h"

#include <inttypes.h>
#include <unistd.h>
#include <utils/Log.h>
#include <thread>

namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {
namespace V2_1 {
namespace implementation {

BiometricsFingerprint::BiometricsFingerprint() {
    for(int i=0; i<10; i++) {
        mOplusBiometricsFingerprint = vendor::oplus::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprint::tryGetService();
        if(mOplusBiometricsFingerprint != nullptr) break;
        sleep(10);
    }
    if(mOplusBiometricsFingerprint == nullptr) exit(0);
}

static bool receivedCancel;
static bool receivedEnumerate;
static uint64_t myDeviceId;
static std::vector<uint32_t> knownFingers;
class OplusClientCallback : public vendor::oplus::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprintClientCallback {
public:
    sp<android::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprintClientCallback> mClientCallback;

    OplusClientCallback(sp<android::hardware::biometrics::fingerprint::V2_1::IBiometricsFingerprintClientCallback> clientCallback) : mClientCallback(clientCallback) {}
    Return<void> onEnrollResult(uint64_t deviceId, uint32_t fingerId,
        uint32_t groupId, uint32_t remaining) {
        ALOGE("onEnrollResult %" PRIu64 " %u %u %u", deviceId, fingerId, groupId, remaining);
        if(mClientCallback != nullptr)
            mClientCallback->onEnrollResult(deviceId, fingerId, groupId, remaining);
        return Void();
    }

    Return<void> onAcquired(uint64_t deviceId, vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo acquiredInfo,
        int32_t vendorCode) {
        ALOGE("onAcquired %" PRIu64 " %d", deviceId, vendorCode);
        if(mClientCallback != nullptr)
            mClientCallback->onAcquired(deviceId, OplusToAOSPFingerprintAcquiredInfo(acquiredInfo), vendorCode);
        return Void();
    }

    Return<void> onAuthenticated(uint64_t deviceId, uint32_t fingerId, uint32_t groupId,
        const hidl_vec<uint8_t>& token) {
        ALOGE("onAuthenticated %" PRIu64 " %u %u", deviceId, fingerId, groupId);
        if(mClientCallback != nullptr)
            mClientCallback->onAuthenticated(deviceId, fingerId, groupId, token);
        return Void();
    }

    Return<void> onError(uint64_t deviceId, vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError error, int32_t vendorCode) {
        ALOGE("onError %" PRIu64 " %d", deviceId, vendorCode);
        if(error == vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_CANCELED) {
            receivedCancel = true;
        }
        if(mClientCallback != nullptr)
            mClientCallback->onError(deviceId, OplusToAOSPFingerprintError(error), vendorCode);
        return Void();
    }

    Return<void> onRemoved(uint64_t deviceId, uint32_t fingerId, uint32_t groupId,
        uint32_t remaining) {
        ALOGE("onRemoved %" PRIu64 " %" PRIu32, deviceId, fingerId);
        if(mClientCallback != nullptr)
            mClientCallback->onRemoved(deviceId, fingerId, groupId, remaining);
        return Void();
    }

    Return<void> onEnumerate(uint64_t deviceId, uint32_t fingerId, uint32_t groupId,
        uint32_t remaining) {
        receivedEnumerate = true;
        ALOGE("onEnumerate %" PRIu64 " %u %u %u", deviceId, fingerId, groupId, remaining);
        if(mClientCallback != nullptr)
            mClientCallback->onEnumerate(deviceId, fingerId, groupId, remaining);
        return Void();
    }

    Return<void> onTouchUp(uint64_t deviceId) { return Void(); }
    Return<void> onTouchDown(uint64_t deviceId) { return Void(); }
    Return<void> onSyncTemplates(uint64_t deviceId, const hidl_vec<uint32_t>& fingerId, uint32_t remaining) {
        ALOGE("onSyncTemplates %" PRIu64 " %zu %" PRIu32, deviceId, fingerId.size(), remaining);
        myDeviceId = deviceId;

        for(auto fid : fingerId) {
            ALOGE("\t- %u", fid);
        }
        knownFingers = fingerId;

        return Void();
    }
    Return<void> onFingerprintCmd(int32_t deviceId, const hidl_vec<uint32_t>& groupId, uint32_t remaining) { return Void(); }
    Return<void> onImageInfoAcquired(uint32_t type, uint32_t quality, uint32_t match_score) { return Void(); }
    Return<void> onMonitorEventTriggered(uint32_t type, const hidl_string& data) { return Void(); }
    Return<void> onEngineeringInfoUpdated(uint32_t length, const hidl_vec<uint32_t>& keys, const hidl_vec<hidl_string>& values) { return Void(); }
    Return<void> onUIReady(int64_t deviceId) { return Void(); }

private:

    Return<android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo> OplusToAOSPFingerprintAcquiredInfo(vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo info) {
        switch(info) {
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_GOOD: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_GOOD;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_PARTIAL: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_PARTIAL;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_INSUFFICIENT: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_INSUFFICIENT;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_IMAGER_DIRTY: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_IMAGER_DIRTY;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_TOO_SLOW: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_TOO_SLOW;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_TOO_FAST: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_TOO_FAST;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_VENDOR: return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_VENDOR;
            default:
                return android::hardware::biometrics::fingerprint::V2_1::FingerprintAcquiredInfo::ACQUIRED_GOOD;
        }
    }

    Return<android::hardware::biometrics::fingerprint::V2_1::FingerprintError> OplusToAOSPFingerprintError(vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError error) {
        switch(error) {
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_ERROR: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_ERROR;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_HW_UNAVAILABLE: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_HW_UNAVAILABLE;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_UNABLE_TO_PROCESS: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_UNABLE_TO_PROCESS;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_TIMEOUT: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_TIMEOUT;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_SPACE: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_SPACE;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_CANCELED: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_CANCELED;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_UNABLE_TO_REMOVE: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_UNABLE_TO_REMOVE;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_LOCKOUT: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_LOCKOUT;
            case vendor::oplus::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_VENDOR: return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_VENDOR;
            default:
                return android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_NO_ERROR;
        }
    }
};

Return<uint64_t> BiometricsFingerprint::setNotify(
        const sp<IBiometricsFingerprintClientCallback>& clientCallback) {
    ALOGE("setNotify");
    mOplusClientCallback = new OplusClientCallback(clientCallback);
    return mOplusBiometricsFingerprint->setNotify(mOplusClientCallback);
}

Return<RequestStatus> BiometricsFingerprint::OplusToAOSPRequestStatus(vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus req) {
    switch(req) {
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_UNKNOWN: return RequestStatus::SYS_UNKNOWN;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_OK: return RequestStatus::SYS_OK;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_ENOENT: return RequestStatus::SYS_ENOENT;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EINTR: return RequestStatus::SYS_EINTR;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EIO: return RequestStatus::SYS_EIO;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EAGAIN: return RequestStatus::SYS_EAGAIN;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_ENOMEM: return RequestStatus::SYS_ENOMEM;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EACCES: return RequestStatus::SYS_EACCES;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EFAULT: return RequestStatus::SYS_EFAULT;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EBUSY: return RequestStatus::SYS_EBUSY;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_EINVAL: return RequestStatus::SYS_EINVAL;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_ENOSPC: return RequestStatus::SYS_ENOSPC;
        case vendor::oplus::hardware::biometrics::fingerprint::V2_1::RequestStatus::SYS_ETIMEDOUT: return RequestStatus::SYS_ETIMEDOUT;
        default:
            return RequestStatus::SYS_UNKNOWN;
    }
}

Return<uint64_t> BiometricsFingerprint::preEnroll()  {
    ALOGE("preEnroll");
    return mOplusBiometricsFingerprint->preEnroll();
}

Return<RequestStatus> BiometricsFingerprint::enroll(const hidl_array<uint8_t, 69>& hat,
    uint32_t gid, uint32_t timeoutSec)  {
    ALOGE("enroll");
    return OplusToAOSPRequestStatus(mOplusBiometricsFingerprint->enroll(hat, gid, timeoutSec));
}

Return<RequestStatus> BiometricsFingerprint::postEnroll()  {
    ALOGE("postEnroll");
    return OplusToAOSPRequestStatus(mOplusBiometricsFingerprint->postEnroll());
}

Return<uint64_t> BiometricsFingerprint::getAuthenticatorId()  {
    ALOGE("getAuthId");
    return mOplusBiometricsFingerprint->getAuthenticatorId();
}

Return<RequestStatus> BiometricsFingerprint::cancel()  {
    receivedCancel = false;
    RequestStatus ret = OplusToAOSPRequestStatus(mOplusBiometricsFingerprint->cancel());
    ALOGE("CANCELING");
    if(!receivedCancel) {
        ALOGE("Sending cancel error");
        mOplusClientCallback->mClientCallback->onError(
                myDeviceId,
                android::hardware::biometrics::fingerprint::V2_1::FingerprintError::ERROR_CANCELED,
                0);
    }
    return ret;
}

Return<RequestStatus> BiometricsFingerprint::enumerate()  {
    receivedEnumerate = false;
    RequestStatus ret = OplusToAOSPRequestStatus(mOplusBiometricsFingerprint->enumerate());
    ALOGE("ENUMERATING");
    if(ret == RequestStatus::SYS_OK && !receivedEnumerate) {
        size_t nFingers = knownFingers.size();
        ALOGE("received fingers, sending our own %zu", nFingers);
        if(nFingers > 0) {
            for(auto finger: knownFingers) {
                mOplusClientCallback->mClientCallback->onEnumerate(
                        myDeviceId,
                        finger,
                        0,
                        --nFingers);

            }
        } else {
            mOplusClientCallback->mClientCallback->onEnumerate(
                    myDeviceId,
                    0,
                    0,
                    0);

        }
    }
    return ret;
}

Return<RequestStatus> BiometricsFingerprint::remove(uint32_t gid, uint32_t fid)  {
    ALOGE("remove");
    return OplusToAOSPRequestStatus(mOplusBiometricsFingerprint->remove(gid, fid));
}

Return<RequestStatus> BiometricsFingerprint::setActiveGroup(uint32_t gid,
    const hidl_string& storePath)  {
    ALOGE("setActiveGroup");
    return OplusToAOSPRequestStatus(mOplusBiometricsFingerprint->setActiveGroup(gid, storePath));
}

Return<RequestStatus> BiometricsFingerprint::authenticate(uint64_t operationId, uint32_t gid)  {
    ALOGE("auth");
    return OplusToAOSPRequestStatus(mOplusBiometricsFingerprint->authenticate(operationId, gid));
}

} // namespace implementation
}  // namespace V2_1
}  // namespace fingerprint
}  // namespace biometrics
}  // namespace hardware
}  // namespace android
