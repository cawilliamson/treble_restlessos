/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "ParametersUtil.h"
#include "Util.h"

#include <system/audio.h>

#include <util/CoreUtils.h>

namespace android {
namespace hardware {
namespace audio {
namespace CORE_TYPES_CPP_VERSION {
namespace implementation {

/** Converts a status_t in Result according to the rules of AudioParameter::get*
 * Note: Static method and not private method to avoid leaking status_t dependency
 */
static Result getHalStatusToResult(status_t status) {
    switch (status) {
        case OK:
            return Result::OK;
        case BAD_VALUE:  // Nothing was returned, probably because the HAL does
                         // not handle it
            return Result::NOT_SUPPORTED;
        case INVALID_OPERATION:  // Conversion from string to the requested type
                                 // failed
            return Result::INVALID_ARGUMENTS;
        default:  // Should not happen
            ALOGW("Unexpected status returned by getParam: %u", status);
            return Result::INVALID_ARGUMENTS;
    }
}

Result ParametersUtil::getParam(const char* name, bool* value) {
    String8 halValue;
    Result retval = getParam(name, &halValue);
    *value = false;
    if (retval == Result::OK) {
        if (halValue.empty()) {
            return Result::NOT_SUPPORTED;
        }
        *value = !(halValue == AudioParameter::valueOff);
    }
    return retval;
}

Result ParametersUtil::getParam(const char* name, int* value) {
    const String8 halName(name);
    AudioParameter keys;
    keys.addKey(halName);
    std::unique_ptr<AudioParameter> params = getParams(keys);
    return getHalStatusToResult(params->getInt(halName, *value));
}

Result ParametersUtil::getParam(const char* name, String8* value, AudioParameter context) {
    const String8 halName(name);
    context.addKey(halName);
    std::unique_ptr<AudioParameter> params = getParams(context);
    return getHalStatusToResult(params->get(halName, *value));
}

void ParametersUtil::getParametersImpl(
    const hidl_vec<ParameterValue>& context, const hidl_vec<hidl_string>& keys,
    std::function<void(Result retval, const hidl_vec<ParameterValue>& parameters)> cb) {
    AudioParameter halKeys;
    for (auto& pair : context) {
        halKeys.add(String8(pair.key.c_str()), String8(pair.value.c_str()));
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        halKeys.addKey(String8(keys[i].c_str()));
    }
    std::unique_ptr<AudioParameter> halValues = getParams(halKeys);
    Result retval =
        (keys.size() == 0 || halValues->size() != 0) ? Result::OK : Result::NOT_SUPPORTED;
    hidl_vec<ParameterValue> result;
    result.resize(halValues->size());
    String8 halKey, halValue;
    for (size_t i = 0; i < halValues->size(); ++i) {
        status_t status = halValues->getAt(i, halKey, halValue);
        if (status != OK) {
            result.resize(0);
            retval = getHalStatusToResult(status);
            break;
        }
        result[i].key = halKey.c_str();
        result[i].value = halValue.c_str();
    }
    cb(retval, result);
}

std::unique_ptr<AudioParameter> ParametersUtil::getParams(const AudioParameter& keys) {
    String8 paramsAndValues;
    char* halValues = halGetParameters(keys.keysToString().c_str());
    if (halValues != NULL) {
        paramsAndValues.setTo(halValues);
        free(halValues);
    } else {
        paramsAndValues.clear();
    }
    return std::unique_ptr<AudioParameter>(new AudioParameter(paramsAndValues));
}

Result ParametersUtil::setParam(const char* name, const char* value) {
    AudioParameter param;
    param.add(String8(name), String8(value));
    return setParams(param);
}

Result ParametersUtil::setParam(const char* name, bool value) {
    AudioParameter param;
    param.add(String8(name), String8(value ? AudioParameter::valueOn : AudioParameter::valueOff));
    return setParams(param);
}

Result ParametersUtil::setParam(const char* name, int value) {
    AudioParameter param;
    param.addInt(String8(name), value);
    return setParams(param);
}

Result ParametersUtil::setParam(const char* name, float value) {
    AudioParameter param;
    param.addFloat(String8(name), value);
    return setParams(param);
}

Result ParametersUtil::setParametersImpl(const hidl_vec<ParameterValue>& context,
                                         const hidl_vec<ParameterValue>& parameters) {
    AudioParameter params;
    for (auto& pair : context) {
        params.add(String8(pair.key.c_str()), String8(pair.value.c_str()));
    }
    for (size_t i = 0; i < parameters.size(); ++i) {
        params.add(String8(parameters[i].key.c_str()), String8(parameters[i].value.c_str()));
    }
    return setParams(params);
}

Result ParametersUtil::setParam(const char* name, const DeviceAddress& address) {
    audio_devices_t halDeviceType;
    char halDeviceAddress[AUDIO_DEVICE_MAX_ADDRESS_LEN];
    if (CoreUtils::deviceAddressToHal(address, &halDeviceType, halDeviceAddress) != NO_ERROR) {
        return Result::INVALID_ARGUMENTS;
    }
    AudioParameter params{String8(halDeviceAddress)};
    params.addInt(String8(name), halDeviceType);
    return setParams(params);
}

Result ParametersUtil::setParams(const AudioParameter& param) {
    int halStatus = halSetParameters(param.toString().c_str());
    return util::analyzeStatus(halStatus);
}

}  // namespace implementation
}  // namespace CORE_TYPES_CPP_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android
