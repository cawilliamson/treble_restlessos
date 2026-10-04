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

#define LOG_TAG "BTAudioProviderSessionCodecsDB_2_1"

#include "BluetoothAudioSupportedCodecsDB_2_1.h"

#include <android-base/logging.h>

namespace android {
namespace bluetooth {
namespace audio {

using ::android::hardware::bluetooth::audio::V2_0::BitsPerSample;
using ::android::hardware::bluetooth::audio::V2_0::ChannelMode;

using SampleRate_2_0 = ::android::hardware::bluetooth::audio::V2_0::SampleRate;
using SampleRate_2_1 = ::android::hardware::bluetooth::audio::V2_1::SampleRate;

using SessionType_2_1 =
    ::android::hardware::bluetooth::audio::V2_1::SessionType;
using SessionType_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::SessionType;

namespace {
bool is_2_0_session_type(
    const ::android::hardware::bluetooth::audio::V2_1::SessionType&
        session_type) {
  if (session_type == SessionType_2_1::A2DP_SOFTWARE_ENCODING_DATAPATH ||
      session_type == SessionType_2_1::A2DP_HARDWARE_OFFLOAD_DATAPATH ||
      session_type == SessionType_2_1::HEARING_AID_SOFTWARE_ENCODING_DATAPATH) {
    return true;
  } else {
    return false;
  }
}
}  // namespace

static const ::android::hardware::bluetooth::audio::V2_1::PcmParameters
    kDefaultSoftwarePcmCapabilities_2_1 = {
        .sampleRate = static_cast<SampleRate_2_1>(
            SampleRate_2_1::RATE_44100 | SampleRate_2_1::RATE_48000 |
            SampleRate_2_1::RATE_88200 | SampleRate_2_1::RATE_96000 |
            SampleRate_2_1::RATE_16000 | SampleRate_2_1::RATE_24000),
        .channelMode =
            static_cast<ChannelMode>(ChannelMode::MONO | ChannelMode::STEREO),
        .bitsPerSample = static_cast<BitsPerSample>(BitsPerSample::BITS_16 |
                                                    BitsPerSample::BITS_24 |
                                                    BitsPerSample::BITS_32)};

std::vector<::android::hardware::bluetooth::audio::V2_1::PcmParameters>
GetSoftwarePcmCapabilities_2_1() {
  return std::vector<
      ::android::hardware::bluetooth::audio::V2_1::PcmParameters>(
      1, kDefaultSoftwarePcmCapabilities_2_1);
}

std::vector<CodecCapabilities> GetOffloadCodecCapabilities(
    const ::android::hardware::bluetooth::audio::V2_1::SessionType&
        session_type) {
  if (is_2_0_session_type(session_type)) {
    return GetOffloadCodecCapabilities(
        static_cast<SessionType_2_0>(session_type));
  }
  return std::vector<CodecCapabilities>(0);
}

bool IsSoftwarePcmConfigurationValid_2_1(
    const ::android::hardware::bluetooth::audio::V2_1::PcmParameters&
        pcm_config) {
  if ((pcm_config.sampleRate != SampleRate_2_1::RATE_44100 &&
       pcm_config.sampleRate != SampleRate_2_1::RATE_48000 &&
       pcm_config.sampleRate != SampleRate_2_1::RATE_88200 &&
       pcm_config.sampleRate != SampleRate_2_1::RATE_96000 &&
       pcm_config.sampleRate != SampleRate_2_1::RATE_16000 &&
       pcm_config.sampleRate != SampleRate_2_1::RATE_24000) ||
      (pcm_config.bitsPerSample != BitsPerSample::BITS_16 &&
       pcm_config.bitsPerSample != BitsPerSample::BITS_24 &&
       pcm_config.bitsPerSample != BitsPerSample::BITS_32) ||
      (pcm_config.channelMode != ChannelMode::MONO &&
       pcm_config.channelMode != ChannelMode::STEREO)) {
    LOG(WARNING) << __func__
                 << ": Invalid PCM Configuration=" << toString(pcm_config);
    return false;
  } else if (pcm_config.sampleRate &
                 kDefaultSoftwarePcmCapabilities_2_1.sampleRate &&
             pcm_config.bitsPerSample &
                 kDefaultSoftwarePcmCapabilities_2_1.bitsPerSample &&
             pcm_config.channelMode &
                 kDefaultSoftwarePcmCapabilities_2_1.channelMode &&
             pcm_config.dataIntervalUs != 0) {
    return true;
  }
  LOG(WARNING) << __func__
               << ": Unsupported PCM Configuration=" << toString(pcm_config);
  return false;
}

bool IsOffloadCodecConfigurationValid(
    const ::android::hardware::bluetooth::audio::V2_1::SessionType&
        session_type,
    const ::android::hardware::bluetooth::audio::V2_0::CodecConfiguration&
        codec_config) {
  if (is_2_0_session_type(session_type)) {
    return IsOffloadCodecConfigurationValid(
        static_cast<SessionType_2_0>(session_type), codec_config);
  }

  return false;
}

bool IsOffloadLeAudioConfigurationValid(
    const ::android::hardware::bluetooth::audio::V2_1::SessionType&
        session_type,
    const ::android::hardware::bluetooth::audio::V2_1::Lc3CodecConfiguration&) {

  if (session_type != SessionType_2_1::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH &&
      session_type != SessionType_2_1::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH) {
    return false;
  }

  //TODO: perform checks on le_audio_codec_config once we know supported parameters

  return true;
}

}  // namespace audio
}  // namespace bluetooth
}  // namespace android
