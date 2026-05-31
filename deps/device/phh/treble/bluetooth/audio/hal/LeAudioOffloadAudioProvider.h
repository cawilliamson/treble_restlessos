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

#include <map>

#include "BluetoothAudioProvider.h"
#include "aidl/android/hardware/bluetooth/audio/LeAudioAseConfiguration.h"
#include "aidl/android/hardware/bluetooth/audio/MetadataLtv.h"
#include "aidl/android/hardware/bluetooth/audio/SessionType.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

using LeAudioAseConfigurationSetting =
    IBluetoothAudioProvider::LeAudioAseConfigurationSetting;
using AseDirectionRequirement = IBluetoothAudioProvider::
    LeAudioConfigurationRequirement::AseDirectionRequirement;
using AseDirectionConfiguration = IBluetoothAudioProvider::
    LeAudioAseConfigurationSetting::AseDirectionConfiguration;
using AseQosDirectionRequirement = IBluetoothAudioProvider::
    LeAudioAseQosConfigurationRequirement::AseQosDirectionRequirement;
using LeAudioAseQosConfiguration =
    IBluetoothAudioProvider::LeAudioAseQosConfiguration;
using LeAudioBroadcastConfigurationSetting =
    IBluetoothAudioProvider::LeAudioBroadcastConfigurationSetting;

class LeAudioOffloadAudioProvider : public BluetoothAudioProvider {
 public:
  LeAudioOffloadAudioProvider();

  bool isValid(const SessionType& sessionType) override;

  ndk::ScopedAStatus startSession(
      const std::shared_ptr<IBluetoothAudioPort>& host_if,
      const AudioConfiguration& audio_config,
      const std::vector<LatencyMode>& latency_modes, DataMQDesc* _aidl_return);
  ndk::ScopedAStatus setCodecPriority(const CodecId& in_codecId,
                                      int32_t in_priority) override;
  ndk::ScopedAStatus getLeAudioAseConfiguration(
      const std::optional<std::vector<
          std::optional<IBluetoothAudioProvider::LeAudioDeviceCapabilities>>>&
          in_remoteSinkAudioCapabilities,
      const std::optional<std::vector<
          std::optional<IBluetoothAudioProvider::LeAudioDeviceCapabilities>>>&
          in_remoteSourceAudioCapabilities,
      const std::vector<
          IBluetoothAudioProvider::LeAudioConfigurationRequirement>&
          in_requirements,
      std::vector<IBluetoothAudioProvider::LeAudioAseConfigurationSetting>*
          _aidl_return) override;
  ndk::ScopedAStatus getLeAudioAseQosConfiguration(
      const IBluetoothAudioProvider::LeAudioAseQosConfigurationRequirement&
          in_qosRequirement,
      IBluetoothAudioProvider::LeAudioAseQosConfigurationPair* _aidl_return)
      override;
  ndk::ScopedAStatus onSourceAseMetadataChanged(
      IBluetoothAudioProvider::AseState in_state, int32_t in_cigId,
      int32_t in_cisId,
      const std::optional<std::vector<std::optional<MetadataLtv>>>& in_metadata)
      override;
  ndk::ScopedAStatus onSinkAseMetadataChanged(
      IBluetoothAudioProvider::AseState in_state, int32_t in_cigId,
      int32_t in_cisId,
      const std::optional<std::vector<std::optional<MetadataLtv>>>& in_metadata)
      override;
  ndk::ScopedAStatus getLeAudioBroadcastConfiguration(
      const std::optional<std::vector<
          std::optional<IBluetoothAudioProvider::LeAudioDeviceCapabilities>>>&
          in_remoteSinkAudioCapabilities,
      const IBluetoothAudioProvider::LeAudioBroadcastConfigurationRequirement&
          in_requirement,
      LeAudioBroadcastConfigurationSetting* _aidl_return) override;

 private:
  ndk::ScopedAStatus onSessionReady(DataMQDesc* _aidl_return) override;
  std::map<CodecId, uint32_t> codec_priority_map_;
  std::vector<LeAudioBroadcastConfigurationSetting> broadcast_settings;

  // Private matching function definitions
  bool isMatchedValidCodec(CodecId cfg_codec, CodecId req_codec);
  bool isCapabilitiesMatchedContext(
      AudioContext setting_context,
      const IBluetoothAudioProvider::LeAudioDeviceCapabilities& capabilities);
  bool isMatchedSamplingFreq(
      CodecSpecificConfigurationLtv::SamplingFrequency& cfg_freq,
      CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies&
          capability_freq);
  bool isMatchedFrameDuration(
      CodecSpecificConfigurationLtv::FrameDuration& cfg_fduration,
      CodecSpecificCapabilitiesLtv::SupportedFrameDurations&
          capability_fduration);
  bool isMatchedAudioChannel(
      CodecSpecificConfigurationLtv::AudioChannelAllocation& cfg_channel,
      CodecSpecificCapabilitiesLtv::SupportedAudioChannelCounts&
          capability_channel);
  bool isMatchedCodecFramesPerSDU(
      CodecSpecificConfigurationLtv::CodecFrameBlocksPerSDU& cfg_frame_sdu,
      CodecSpecificCapabilitiesLtv::SupportedMaxCodecFramesPerSDU&
          capability_frame_sdu);
  bool isMatchedOctetsPerCodecFrame(
      CodecSpecificConfigurationLtv::OctetsPerCodecFrame& cfg_octets,
      CodecSpecificCapabilitiesLtv::SupportedOctetsPerCodecFrame&
          capability_octets);
  bool isCapabilitiesMatchedCodecConfiguration(
      std::vector<CodecSpecificConfigurationLtv>& codec_cfg,
      std::vector<CodecSpecificCapabilitiesLtv> codec_capabilities);
  bool isMatchedAseConfiguration(LeAudioAseConfiguration setting_cfg,
                                 LeAudioAseConfiguration requirement_cfg);
  bool isMatchedBISConfiguration(
      LeAudioBisConfiguration bis_cfg,
      const IBluetoothAudioProvider::LeAudioDeviceCapabilities& capabilities);
  void filterCapabilitiesAseDirectionConfiguration(
      std::vector<std::optional<AseDirectionConfiguration>>&
          direction_configurations,
      const IBluetoothAudioProvider::LeAudioDeviceCapabilities& capabilities,
      std::vector<std::optional<AseDirectionConfiguration>>&
          valid_direction_configurations);
  void filterRequirementAseDirectionConfiguration(
      std::vector<std::optional<AseDirectionConfiguration>>&
          direction_configurations,
      const std::optional<std::vector<std::optional<AseDirectionRequirement>>>&
          requirements,
      std::vector<std::optional<AseDirectionConfiguration>>&
          valid_direction_configurations);
  std::optional<LeAudioAseConfigurationSetting>
  getCapabilitiesMatchedAseConfigurationSettings(
      IBluetoothAudioProvider::LeAudioAseConfigurationSetting& setting,
      const IBluetoothAudioProvider::LeAudioDeviceCapabilities& capabilities,
      uint8_t direction);
  std::optional<LeAudioAseConfigurationSetting>
  getRequirementMatchedAseConfigurationSettings(
      IBluetoothAudioProvider::LeAudioAseConfigurationSetting& setting,
      const IBluetoothAudioProvider::LeAudioConfigurationRequirement&
          requirement);
  bool isMatchedQosRequirement(LeAudioAseQosConfiguration setting_qos,
                               AseQosDirectionRequirement requirement_qos);
  std::optional<LeAudioBroadcastConfigurationSetting>
  getCapabilitiesMatchedBroadcastConfigurationSettings(
      LeAudioBroadcastConfigurationSetting& setting,
      const IBluetoothAudioProvider::LeAudioDeviceCapabilities& capabilities);
  void getBroadcastSettings();
};

class LeAudioOffloadOutputAudioProvider : public LeAudioOffloadAudioProvider {
 public:
  LeAudioOffloadOutputAudioProvider();
};

class LeAudioOffloadInputAudioProvider : public LeAudioOffloadAudioProvider {
 public:
  LeAudioOffloadInputAudioProvider();
};

class LeAudioOffloadBroadcastAudioProvider
    : public LeAudioOffloadAudioProvider {
 public:
  LeAudioOffloadBroadcastAudioProvider();
};

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
