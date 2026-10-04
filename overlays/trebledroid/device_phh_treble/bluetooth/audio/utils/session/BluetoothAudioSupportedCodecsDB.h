/*
 * Copyright 2018 The Android Open Source Project
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

#pragma once

#include <android/hardware/bluetooth/audio/2.0/types.h>

namespace android {
namespace bluetooth {
namespace audio {

using ::android::hardware::bluetooth::audio::V2_0::CodecCapabilities;
using ::android::hardware::bluetooth::audio::V2_0::CodecConfiguration;
using ::android::hardware::bluetooth::audio::V2_0::PcmParameters;
using ::android::hardware::bluetooth::audio::V2_0::SessionType;

std::vector<PcmParameters> GetSoftwarePcmCapabilities();
std::vector<CodecCapabilities> GetOffloadCodecCapabilities(
    const SessionType& session_type);

bool IsSoftwarePcmConfigurationValid(const PcmParameters& pcm_config);
bool IsOffloadCodecConfigurationValid(const SessionType& session_type,
                                      const CodecConfiguration& codec_config);

}  // namespace audio
}  // namespace bluetooth
}  // namespace android
