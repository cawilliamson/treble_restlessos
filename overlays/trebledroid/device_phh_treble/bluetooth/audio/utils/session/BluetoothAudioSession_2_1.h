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

#include <android/hardware/bluetooth/audio/2.1/types.h>
#include "BluetoothAudioSession.h"

#include <mutex>
#include <unordered_map>

namespace android {
namespace bluetooth {
namespace audio {

class BluetoothAudioSession_2_1 {
 private:
  std::shared_ptr<BluetoothAudioSession> audio_session;

  ::android::hardware::bluetooth::audio::V2_1::SessionType session_type_2_1_;
  ::android::hardware::bluetooth::audio::V2_1::SessionType raw_session_type_;

  // audio data configuration for both software and offloading
  ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration
      audio_config_2_1_;

  bool UpdateAudioConfig(
      const ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration&
          audio_config);

  static ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration
      invalidSoftwareAudioConfiguration;
  static ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration
      invalidOffloadAudioConfiguration;

 public:
  BluetoothAudioSession_2_1(
      const ::android::hardware::bluetooth::audio::V2_1::SessionType&
          session_type);

  std::shared_ptr<BluetoothAudioSession> GetAudioSession();

  // The report function is used to report that the Bluetooth stack has started
  // this session without any failure, and will invoke session_changed_cb_ to
  // notify those registered bluetooth_audio outputs
  void OnSessionStarted(
      const sp<IBluetoothAudioPort> stack_iface,
      const DataMQ::Descriptor* dataMQ,
      const ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration&
          audio_config);

  // The control function is for the bluetooth_audio module to get the current
  // AudioConfiguration
  const ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration
  GetAudioConfig();

  static constexpr ::android::hardware::bluetooth::audio::V2_1::
      AudioConfiguration& kInvalidSoftwareAudioConfiguration =
          invalidSoftwareAudioConfiguration;
  static constexpr ::android::hardware::bluetooth::audio::V2_1::
      AudioConfiguration& kInvalidOffloadAudioConfiguration =
          invalidOffloadAudioConfiguration;
};

class BluetoothAudioSessionInstance_2_1 {
 public:
  // The API is to fetch the specified session of A2DP / Hearing Aid
  static std::shared_ptr<BluetoothAudioSession_2_1> GetSessionInstance(
      const ::android::hardware::bluetooth::audio::V2_1::SessionType&
          session_type);

 private:
  static std::unique_ptr<BluetoothAudioSessionInstance_2_1> instance_ptr;
  std::mutex mutex_;
  std::unordered_map<::android::hardware::bluetooth::audio::V2_1::SessionType,
                     std::shared_ptr<BluetoothAudioSession_2_1>>
      sessions_map_;
};

}  // namespace audio
}  // namespace bluetooth
}  // namespace android
