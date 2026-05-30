/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <aidl/android/hardware/bluetooth/audio/LeAudioCodecCapabilitiesSetting.h>
#include <android-base/logging.h>

#include <unordered_map>
#include <vector>

#include "aidl/android/hardware/bluetooth/audio/CodecInfo.h"
#include "aidl/android/hardware/bluetooth/audio/SessionType.h"
#include "aidl_android_hardware_bluetooth_audio_setting.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

class BluetoothLeAudioCodecsProvider {
 public:
  static std::optional<setting::LeAudioOffloadSetting>
  ParseFromLeAudioOffloadSettingFile();
  static std::vector<LeAudioCodecCapabilitiesSetting>
  GetLeAudioCodecCapabilities(
      const std::optional<setting::LeAudioOffloadSetting>&
          le_audio_offload_setting);
  static void ClearLeAudioCodecCapabilities();
  static std::unordered_map<SessionType, std::vector<CodecInfo>>
  GetLeAudioCodecInfo(const std::optional<setting::LeAudioOffloadSetting>&
                          le_audio_offload_setting);

 private:
  static inline std::vector<setting::Scenario> supported_scenarios_;
  static inline std::unordered_map<std::string, setting::Configuration>
      configuration_map_;
  static inline std::unordered_map<std::string, setting::CodecConfiguration>
      codec_configuration_map_;
  static inline std::unordered_map<std::string, setting::StrategyConfiguration>
      strategy_configuration_map_;
  static inline std::unordered_map<SessionType, std::vector<CodecInfo>>
      session_codecs_map_;

  static std::vector<setting::Scenario> GetScenarios(
      const std::optional<setting::LeAudioOffloadSetting>&
          le_audio_offload_setting);
  static void UpdateConfigurationsToMap(
      const std::optional<setting::LeAudioOffloadSetting>&
          le_audio_offload_setting);
  static void UpdateCodecConfigurationsToMap(
      const std::optional<setting::LeAudioOffloadSetting>&
          le_audio_offload_setting);
  static void UpdateStrategyConfigurationsToMap(
      const std::optional<setting::LeAudioOffloadSetting>&
          le_audio_offload_setting);
  static void LoadConfigurationToMap(
      const std::optional<setting::LeAudioOffloadSetting>&
          le_audio_offload_setting);

  static std::vector<LeAudioCodecCapabilitiesSetting>
  ComposeLeAudioCodecCapabilities(
      const std::vector<setting::Scenario>& supported_scenarios);

  static UnicastCapability GetUnicastCapability(
      const std::string& coding_direction);
  static BroadcastCapability GetBroadcastCapability(
      const std::string& coding_direction);

  template <class T>
  static inline UnicastCapability ComposeUnicastCapability(
      const CodecType& codec_type, const AudioLocation& audio_location,
      const uint8_t& device_cnt, const uint8_t& channel_count,
      const T& capability);

  template <class T>
  static inline BroadcastCapability ComposeBroadcastCapability(
      const CodecType& codec_type, const AudioLocation& audio_location,
      const uint8_t& channel_count, const std::vector<T>& capability);

  static inline Lc3Capabilities ComposeLc3Capability(
      const setting::CodecConfiguration& codec_configuration);

  static inline AptxAdaptiveLeCapabilities ComposeAptxAdaptiveLeCapability(
      const setting::CodecConfiguration& codec_configuration);

  static inline AudioLocation GetAudioLocation(
      const setting::AudioLocation& audio_location);
  static inline CodecType GetCodecType(const setting::CodecType& codec_type);

  static inline bool IsValidCodecConfiguration(
      const setting::CodecConfiguration& codec_configuration);
  static inline bool IsValidStrategyConfiguration(
      const setting::StrategyConfiguration& strategy_configuration);
};

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
