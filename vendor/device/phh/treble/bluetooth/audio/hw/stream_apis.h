/*
 * Copyright 2019 The Android Open Source Project
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

#include <hardware/audio.h>
#include <system/audio.h>

#include <list>

#include "device_port_proxy.h"
#include "device_port_proxy_hidl.h"

constexpr unsigned int kBluetoothDefaultSampleRate = 44100;
constexpr audio_format_t kBluetoothDefaultAudioFormatBitsPerSample =
    AUDIO_FORMAT_PCM_16_BIT;

constexpr unsigned int kBluetoothDefaultInputBufferMs = 20;
constexpr unsigned int kBluetoothDefaultInputStateTimeoutMs = 20;

constexpr unsigned int kBluetoothDefaultOutputBufferMs = 10;
constexpr unsigned int kBluetoothSpatializerOutputBufferMs = 10;

constexpr audio_channel_mask_t kBluetoothDefaultOutputChannelModeMask =
    AUDIO_CHANNEL_OUT_STEREO;
constexpr audio_channel_mask_t kBluetoothDefaultInputChannelModeMask =
    AUDIO_CHANNEL_IN_MONO;

enum class BluetoothStreamState : uint8_t {
  DISABLED = 0,  // This stream is closing or set param "suspend=true"
  STANDBY,
  STARTING,
  STARTED,
  SUSPENDING,
  UNKNOWN,
};

std::ostream& operator<<(std::ostream& os, const BluetoothStreamState& state);

struct BluetoothStreamOut {
  // Must be the first member so it can be cast from audio_stream
  // or audio_stream_out pointer
  audio_stream_out stream_out_{};
  std::unique_ptr<::android::bluetooth::audio::BluetoothAudioPort>
      bluetooth_output_;
  bool is_aidl;
  int64_t last_write_time_us_;
  // Audio PCM Configs
  uint32_t sample_rate_;
  audio_channel_mask_t channel_mask_;
  audio_format_t format_;
  size_t preferred_data_interval_us;
  // frame is the number of samples per channel
  // frames count per tick
  size_t frames_count_;
  // total frames written, reset on standby
  uint64_t frames_rendered_;
  // total frames written after opened, never reset
  uint64_t frames_presented_;
  mutable std::mutex mutex_;
};

struct BluetoothAudioDevice {
  // Important: device must be first as an audio_hw_device* may be cast to
  // BluetoothAudioDevice* when the type is implicitly known.
  audio_hw_device audio_device_{};
  // protect against device->output and stream_out from being inconsistent
  std::mutex mutex_;
  std::list<BluetoothStreamOut*> opened_stream_outs_ =
      std::list<BluetoothStreamOut*>(0);
  uint32_t next_unique_id = 1;
};

struct BluetoothStreamIn {
  // Must be the first member so it can be cast from audio_stream
  // or audio_stream_in pointer
  audio_stream_in stream_in_;
  std::unique_ptr<::android::bluetooth::audio::BluetoothAudioPort>
      bluetooth_input_;
  bool is_aidl;
  int64_t last_read_time_us_;
  // Audio PCM Configs
  uint32_t sample_rate_;
  audio_channel_mask_t channel_mask_;
  audio_format_t format_;
  size_t preferred_data_interval_us;
  // frame is the number of samples per channel
  // frames count per tick
  size_t frames_count_;
  // total frames read after opened, never reset
  uint64_t frames_presented_;
  mutable std::mutex mutex_;
};

int adev_open_output_stream(struct audio_hw_device* dev,
                            audio_io_handle_t handle, audio_devices_t devices,
                            audio_output_flags_t flags,
                            struct audio_config* config,
                            struct audio_stream_out** stream_out,
                            const char* address __unused);

void adev_close_output_stream(struct audio_hw_device* dev,
                              struct audio_stream_out* stream);

size_t adev_get_input_buffer_size(const struct audio_hw_device* dev,
                                  const struct audio_config* config);

int adev_open_input_stream(struct audio_hw_device* dev,
                           audio_io_handle_t handle, audio_devices_t devices,
                           struct audio_config* config,
                           struct audio_stream_in** stream_in,
                           audio_input_flags_t flags __unused,
                           const char* address __unused,
                           audio_source_t source __unused);

void adev_close_input_stream(struct audio_hw_device* dev,
                             struct audio_stream_in* in);
