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

#ifndef ANDROID_HARDWARE_AUDIO_UTIL_H
#define ANDROID_HARDWARE_AUDIO_UTIL_H

// clang-format off
#include PATH(android/hardware/audio/CORE_TYPES_FILE_VERSION/types.h)
// clang-format on

#include <algorithm>
#include <vector>

#include <system/audio.h>

namespace android {
namespace hardware {
namespace audio {
namespace CORE_TYPES_CPP_VERSION {
namespace implementation {

using namespace ::android::hardware::audio::common::COMMON_TYPES_CPP_VERSION;
using namespace ::android::hardware::audio::CORE_TYPES_CPP_VERSION;

namespace util {

/** @return true if gain is between 0 and 1 included. */
constexpr bool isGainNormalized(float gain) {
    return gain >= 0.0 && gain <= 1.0;
}

template <typename T>
inline bool element_in(T e, const std::vector<T>& v) {
    return std::find(v.begin(), v.end(), e) != v.end();
}

static inline Result analyzeStatus(status_t status) {
    switch (status) {
        case 0:
            return Result::OK;
        case -EINVAL:
            return Result::INVALID_ARGUMENTS;
        case -ENODATA:
            return Result::INVALID_STATE;
        case -ENODEV:
            return Result::NOT_INITIALIZED;
        case -ENOSYS:
            return Result::NOT_SUPPORTED;
        default:
            return Result::INVALID_STATE;
    }
}

static inline Result analyzeStatus(const char* className, const char* funcName, status_t status,
                                   const std::vector<int>& ignoreErrors = {}) {
    if (status != 0 && !element_in(-status, ignoreErrors)) {
        ALOGW("Error from HAL %s in function %s: %s", className, funcName, strerror(-status));
    }
    return analyzeStatus(status);
}

}  // namespace util
}  // namespace implementation
}  // namespace CORE_TYPES_CPP_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_UTIL_H
