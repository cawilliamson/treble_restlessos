/*
 * Copyright 2020 The Android Open Source Project
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

#include "BluetoothAudioSupportedCodecsDB.h"

#include <android/hardware/bluetooth/audio/2.1/types.h>

namespace android {
namespace bluetooth {
namespace audio {

std::vector<::android::hardware::bluetooth::audio::V2_1::PcmParameters>
GetSoftwarePcmCapabilities_2_1();
std::vector<::android::hardware::bluetooth::audio::V2_0::CodecCapabilities>
GetOffloadCodecCapabilities(
    const ::android::hardware::bluetooth::audio::V2_1::SessionType&
        session_type);

bool IsSoftwarePcmConfigurationValid_2_1(
    const ::android::hardware::bluetooth::audio::V2_1::PcmParameters&
        pcm_config);

bool IsOffloadCodecConfigurationValid(
    const ::android::hardware::bluetooth::audio::V2_1::SessionType&
        session_type,
    const ::android::hardware::bluetooth::audio::V2_0::CodecConfiguration&
        codec_config);

bool IsOffloadLeAudioConfigurationValid(
    const ::android::hardware::bluetooth::audio::V2_1::SessionType&
        session_type,
    const ::android::hardware::bluetooth::audio::V2_1::Lc3CodecConfiguration&
        le_audio_codec_config);
}  // namespace audio
}  // namespace bluetooth
}  // namespace android
