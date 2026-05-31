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

#include <aidl/android/hardware/bluetooth/audio/BnBluetoothAudioProvider.h>
#include <aidl/android/hardware/bluetooth/audio/LatencyMode.h>
#include <aidl/android/hardware/bluetooth/audio/SessionType.h>
#include <fmq/AidlMessageQueue.h>

using ::aidl::android::hardware::common::fmq::MQDescriptor;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::android::AidlMessageQueue;

using MqDataType = int8_t;
using MqDataMode = SynchronizedReadWrite;
using DataMQ = AidlMessageQueue<MqDataType, MqDataMode>;
using DataMQDesc = MQDescriptor<MqDataType, MqDataMode>;

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

/// Enable flag for the reference implementation for A2dp Codec
/// Extensibility.
///
/// A2dp Codec extensibility cannot be enabled until the following
/// requirements are fulfilled.
///
///  1. The Bluetooth controller must support the HCI Requirements
///     v1.04 or later, and must support the vendor HCI command
///     A2DP Offload Start (v2), A2DP Offload Stop (v2) as indicated
///     by the field a2dp_offload_v2 of the vendor capabilities.
///
///  2. The implementation of the provider must be completed with
///     DSP configuration for streaming.
enum : bool {
  kEnableA2dpCodecExtensibility = false,
};

class BluetoothAudioProvider : public BnBluetoothAudioProvider {
 public:
  BluetoothAudioProvider();
  ndk::ScopedAStatus startSession(
      const std::shared_ptr<IBluetoothAudioPort>& host_if,
      const AudioConfiguration& audio_config,
      const std::vector<LatencyMode>& latency_modes, DataMQDesc* _aidl_return);
  ndk::ScopedAStatus endSession();
  ndk::ScopedAStatus streamStarted(BluetoothAudioStatus status);
  ndk::ScopedAStatus streamSuspended(BluetoothAudioStatus status);
  ndk::ScopedAStatus updateAudioConfiguration(
      const AudioConfiguration& audio_config);
  ndk::ScopedAStatus setLowLatencyModeAllowed(bool allowed);
  ndk::ScopedAStatus setCodecPriority(
      const ::aidl::android::hardware::bluetooth::audio::CodecId& in_codecId,
      int32_t in_priority) override;
  ndk::ScopedAStatus getLeAudioAseConfiguration(
      const std::optional<std::vector<std::optional<
          ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
              LeAudioDeviceCapabilities>>>& in_remoteSinkAudioCapabilities,
      const std::optional<std::vector<std::optional<
          ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
              LeAudioDeviceCapabilities>>>& in_remoteSourceAudioCapabilities,
      const std::vector<
          ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
              LeAudioConfigurationRequirement>& in_requirements,
      std::vector<::aidl::android::hardware::bluetooth::audio::
                      IBluetoothAudioProvider::LeAudioAseConfigurationSetting>*
          _aidl_return) override;
  ndk::ScopedAStatus getLeAudioAseQosConfiguration(
      const ::aidl::android::hardware::bluetooth::audio::
          IBluetoothAudioProvider::LeAudioAseQosConfigurationRequirement&
              in_qosRequirement,
      ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
          LeAudioAseQosConfigurationPair* _aidl_return) override;
  ndk::ScopedAStatus getLeAudioAseDatapathConfiguration(
      const std::optional<::aidl::android::hardware::bluetooth::audio::
                              IBluetoothAudioProvider::StreamConfig>&
          in_sinkConfig,
      const std::optional<::aidl::android::hardware::bluetooth::audio::
                              IBluetoothAudioProvider::StreamConfig>&
          in_sourceConfig,
      ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
          LeAudioDataPathConfigurationPair* _aidl_return) override;
  ndk::ScopedAStatus onSinkAseMetadataChanged(
      ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
          AseState in_state,
      int32_t cigId, int32_t cisId,
      const std::optional<std::vector<std::optional<
          ::aidl::android::hardware::bluetooth::audio::MetadataLtv>>>&
          in_metadata) override;
  ndk::ScopedAStatus onSourceAseMetadataChanged(
      ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
          AseState in_state,
      int32_t cigId, int32_t cisId,
      const std::optional<std::vector<std::optional<
          ::aidl::android::hardware::bluetooth::audio::MetadataLtv>>>&
          in_metadata) override;
  ndk::ScopedAStatus getLeAudioBroadcastConfiguration(
      const std::optional<std::vector<std::optional<
          ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
              LeAudioDeviceCapabilities>>>& in_remoteSinkAudioCapabilities,
      const ::aidl::android::hardware::bluetooth::audio::
          IBluetoothAudioProvider::LeAudioBroadcastConfigurationRequirement&
              in_requirement,
      ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
          LeAudioBroadcastConfigurationSetting* _aidl_return) override;
  ndk::ScopedAStatus getLeAudioBroadcastDatapathConfiguration(
      const ::aidl::android::hardware::bluetooth::audio::AudioContext&
          in_context,
      const std::vector<::aidl::android::hardware::bluetooth::audio::
                            LeAudioBroadcastConfiguration::BroadcastStreamMap>&
          in_streamMap,
      ::aidl::android::hardware::bluetooth::audio::IBluetoothAudioProvider::
          LeAudioDataPathConfiguration* _aidl_return) override;

  ndk::ScopedAStatus parseA2dpConfiguration(
      const CodecId& codec_id, const std::vector<uint8_t>& configuration,
      CodecParameters* codec_parameters, A2dpStatus* _aidl_return);
  ndk::ScopedAStatus getA2dpConfiguration(
      const std::vector<A2dpRemoteCapabilities>& remote_a2dp_capabilities,
      const A2dpConfigurationHint& hint,
      std::optional<audio::A2dpConfiguration>* _aidl_return);

  virtual bool isValid(const SessionType& sessionType) = 0;

 protected:
  virtual ndk::ScopedAStatus onSessionReady(DataMQDesc* _aidl_return) = 0;

  ::ndk::ScopedAIBinder_DeathRecipient death_recipient_;

  std::shared_ptr<IBluetoothAudioPort> stack_iface_;
  std::unique_ptr<AudioConfiguration> audio_config_ = nullptr;
  SessionType session_type_;
  std::vector<LatencyMode> latency_modes_;
};
}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
