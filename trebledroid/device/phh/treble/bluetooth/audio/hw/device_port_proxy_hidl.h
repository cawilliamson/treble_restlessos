/*
 * Copyright 2022 The Android Open Source Project
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
#include <hardware/audio.h>

#include <condition_variable>
#include <mutex>
#include <unordered_map>

#include "device_port_proxy.h"

enum class BluetoothStreamState : uint8_t;

namespace android {
namespace bluetooth {
namespace audio {
namespace hidl {

using SessionType_2_1 =
    ::android::hardware::bluetooth::audio::V2_1::SessionType;

class BluetoothAudioPortHidl : public BluetoothAudioPort {
 public:
  BluetoothAudioPortHidl();
  virtual ~BluetoothAudioPortHidl() = default;

  bool SetUp(audio_devices_t devices) override;

  void TearDown() override;

  void ForcePcmStereoToMono(bool force) override { is_stereo_to_mono_ = force; }

  bool Start() override;

  bool Suspend() override;

  void Stop() override;

  bool GetPresentationPosition(uint64_t* delay_ns, uint64_t* bytes,
                               timespec* timestamp) const override;

  void UpdateTracksMetadata(const source_metadata* source_metadata) const;

  BluetoothStreamState GetState() const override;

  void SetState(BluetoothStreamState state) override;

  bool IsA2dp() const override {
    return session_type_hidl_ ==
               SessionType_2_1::A2DP_SOFTWARE_ENCODING_DATAPATH ||
           session_type_hidl_ ==
               SessionType_2_1::A2DP_HARDWARE_OFFLOAD_DATAPATH;
  }

  bool IsLeAudio() const override {
    return session_type_hidl_ ==
               SessionType_2_1::LE_AUDIO_SOFTWARE_ENCODING_DATAPATH ||
           session_type_hidl_ ==
               SessionType_2_1::LE_AUDIO_SOFTWARE_DECODED_DATAPATH ||
           session_type_hidl_ ==
               SessionType_2_1::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH ||
           session_type_hidl_ ==
               SessionType_2_1::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH;
  }

  bool GetPreferredDataIntervalUs(size_t* interval_us) const override;

 protected:
  SessionType_2_1 session_type_hidl_;
  uint16_t cookie_;
  BluetoothStreamState state_;
  // WR to support Mono: True if fetching Stereo and mixing into Mono
  bool is_stereo_to_mono_ = false;

  bool in_use() const;

 private:
  mutable std::mutex cv_mutex_;
  std::condition_variable internal_cv_;

  bool init_session_type(audio_devices_t device);

  bool CondwaitState(BluetoothStreamState state);

  void ControlResultHandler(
      const ::android::hardware::bluetooth::audio::V2_0::Status& status);

  void SessionChangedHandler();
};

class BluetoothAudioPortHidlOut : public BluetoothAudioPortHidl {
 public:
  ~BluetoothAudioPortHidlOut();

  size_t WriteData(const void* buffer, size_t bytes) const override;
  bool LoadAudioConfig(audio_config_t* audio_cfg) const override;
};

class BluetoothAudioPortHidlIn : public BluetoothAudioPortHidl {
 public:
  ~BluetoothAudioPortHidlIn();

  size_t ReadData(void* buffer, size_t bytes) const override;
  bool LoadAudioConfig(audio_config_t* audio_cfg) const override;
};

}  // namespace hidl
}  // namespace audio
}  // namespace bluetooth
}  // namespace android