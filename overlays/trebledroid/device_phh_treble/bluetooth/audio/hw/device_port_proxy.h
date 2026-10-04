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

#include <aidl/android/hardware/audio/common/SinkMetadata.h>
#include <aidl/android/hardware/audio/common/SourceMetadata.h>
#include <aidl/android/hardware/bluetooth/audio/BluetoothAudioStatus.h>
#include <aidl/android/hardware/bluetooth/audio/SessionType.h>
#include <hardware/audio.h>

#include <condition_variable>
#include <mutex>
#include <unordered_map>

enum class BluetoothStreamState : uint8_t;

namespace android {
namespace bluetooth {
namespace audio {

/***
 * Proxy for Bluetooth Audio HW Module to communicate with Bluetooth Audio
 * Session Control. All methods are not thread safe, so users must acquire a
 * lock. Note: currently, in stream_apis.cc, if GetState() is only used for
 * verbose logging, it is not locked, so the state may not be synchronized.
 ***/
class BluetoothAudioPort {
 public:
  BluetoothAudioPort(){};
  virtual ~BluetoothAudioPort() = default;

  /***
   * Fetch output control / data path of BluetoothAudioPort and setup
   * callbacks into BluetoothAudioProvider. If SetUp() returns false, the audio
   * HAL must delete this BluetoothAudioPort and return EINVAL to caller
   ***/
  virtual bool SetUp(audio_devices_t) { return false; }

  /***
   * Unregister this BluetoothAudioPort from BluetoothAudioSessionControl.
   * Audio HAL must delete this BluetoothAudioPort after calling this.
   ***/
  virtual void TearDown() {}

  /***
   * When the Audio framework / HAL tries to query audio config about format,
   * channel mask and sample rate, it uses this function to fetch from the
   * Bluetooth stack
   ***/
  virtual bool LoadAudioConfig(audio_config_t*) const { return false; };

  /***
   * WAR to support Mono mode / 16 bits per sample
   ***/
  virtual void ForcePcmStereoToMono(bool) {}

  /***
   * When the Audio framework / HAL wants to change the stream state, it invokes
   * these 3 functions to control the Bluetooth stack (Audio Control Path).
   * Note: Both Start() and Suspend() will return true when there are no errors.
   * Called by Audio framework / HAL to start the stream
   ***/
  virtual bool Start() { return false; }

  /***
   * Called by Audio framework / HAL to suspend the stream
   ***/
  virtual bool Suspend() { return false; };

  /***
    virtual bool Suspend() { return false; }
    * Called by Audio framework / HAL to stop the stream
  ***/
  virtual void Stop() {}

  /***
   * Called by the Audio framework / HAL to fetch information about audio frames
   * presented to an external sink, or frames presented fror an internal sink
   ***/
  virtual bool GetPresentationPosition(uint64_t*, uint64_t*, timespec*) const {
    return false;
  }

  /***
   * Return the current BluetoothStreamState
   ***/
  virtual BluetoothStreamState GetState() const {
    return static_cast<BluetoothStreamState>(0);
  }

  /***
   * Set the current BluetoothStreamState
   ***/
  virtual void SetState(BluetoothStreamState state) {}

  virtual bool IsA2dp() const { return false; }

  virtual bool IsLeAudio() const { return false; }

  virtual bool GetPreferredDataIntervalUs(size_t* interval_us) const {
    return false;
  };

  virtual size_t WriteData(const void* buffer, size_t bytes) const {
    return 0;
  };
  virtual size_t ReadData(void* buffer, size_t bytes) const { return 0; };
};

namespace aidl {

using ::aidl::android::hardware::bluetooth::audio::BluetoothAudioStatus;
using ::aidl::android::hardware::bluetooth::audio::SessionType;

class BluetoothAudioPortAidl : public BluetoothAudioPort {
 public:
  BluetoothAudioPortAidl();
  virtual ~BluetoothAudioPortAidl() = default;

  bool SetUp(audio_devices_t devices) override;

  void TearDown() override;

  void ForcePcmStereoToMono(bool force) override { is_stereo_to_mono_ = force; }

  bool Start() override;
  bool Suspend() override;
  void Stop() override;

  bool GetPresentationPosition(uint64_t* delay_ns, uint64_t* byte,
                               timespec* timestamp) const override;

  void UpdateSourceMetadata(const source_metadata_v7* source_metadata) const;

  /***
   * Called by the Audio framework / HAL when the metadata of the stream's
   * sink has been changed.
   ***/
  virtual void UpdateSinkMetadata(const sink_metadata_v7* sink_metadata) const;

  BluetoothStreamState GetState() const override;

  void SetState(BluetoothStreamState state) override;

  bool IsA2dp() const override {
    return session_type_ == SessionType::A2DP_SOFTWARE_ENCODING_DATAPATH ||
           session_type_ ==
               SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH;
  }

  bool IsLeAudio() const override {
    return session_type_ == SessionType::LE_AUDIO_SOFTWARE_ENCODING_DATAPATH ||
           session_type_ == SessionType::LE_AUDIO_SOFTWARE_DECODING_DATAPATH ||
           session_type_ ==
               SessionType::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH ||
           session_type_ ==
               SessionType::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH ||
           session_type_ ==
               SessionType::LE_AUDIO_BROADCAST_SOFTWARE_ENCODING_DATAPATH ||
           session_type_ ==
               SessionType::
                   LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH;
  }

  bool GetPreferredDataIntervalUs(size_t* interval_us) const override;

 protected:
  uint16_t cookie_;
  BluetoothStreamState state_;
  SessionType session_type_;
  // WR to support Mono: True if fetching Stereo and mixing into Mono
  bool is_stereo_to_mono_ = false;
  virtual bool in_use() const;

 private:
  mutable std::mutex cv_mutex_;
  std::condition_variable internal_cv_;

  // Check and initialize session type for |devices| If failed, this
  // BluetoothAudioPortAidl is not initialized and must be deleted.
  bool init_session_type(audio_devices_t device);

  bool CondwaitState(BluetoothStreamState state);

  void ControlResultHandler(const BluetoothAudioStatus& status);
  void SessionChangedHandler();
};

class BluetoothAudioPortAidlOut : public BluetoothAudioPortAidl {
 public:
  ~BluetoothAudioPortAidlOut();

  // The audio data path to the Bluetooth stack (Software encoding)
  size_t WriteData(const void* buffer, size_t bytes) const override;
  bool LoadAudioConfig(audio_config_t* audio_cfg) const override;
};

class BluetoothAudioPortAidlIn : public BluetoothAudioPortAidl {
 public:
  ~BluetoothAudioPortAidlIn();

  // The audio data path from the Bluetooth stack (Software decoded)
  size_t ReadData(void* buffer, size_t bytes) const override;
  bool LoadAudioConfig(audio_config_t* audio_cfg) const override;
};

}  // namespace aidl
}  // namespace audio
}  // namespace bluetooth
}  // namespace android
