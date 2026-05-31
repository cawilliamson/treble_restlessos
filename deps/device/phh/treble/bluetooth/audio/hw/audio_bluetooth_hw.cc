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

#define LOG_TAG "BTAudioHw"

#include <android-base/logging.h>
#include <hardware/hardware.h>
#include <log/log.h>
#include <malloc.h>
#include <string.h>
#include <system/audio.h>

#include "stream_apis.h"
#include "utils.h"

using ::android::bluetooth::audio::utils::GetAudioParamString;
using ::android::bluetooth::audio::utils::ParseAudioParams;

static int adev_set_parameters(struct audio_hw_device* dev,
                               const char* kvpairs) {
  LOG(VERBOSE) << __func__ << ": kevpairs=[" << kvpairs << "]";
  std::unordered_map<std::string, std::string> params =
      ParseAudioParams(kvpairs);
  if (params.empty()) return 0;

  LOG(VERBOSE) << __func__ << ": ParamsMap=[" << GetAudioParamString(params)
               << "]";
  if (params.find("A2dpSuspended") == params.end() &&
      params.find("LeAudioSuspended") == params.end()) {
    return -ENOSYS;
  }

  auto* bluetooth_device = reinterpret_cast<BluetoothAudioDevice*>(dev);
  std::lock_guard<std::mutex> guard(bluetooth_device->mutex_);
  for (auto sout : bluetooth_device->opened_stream_outs_) {
    if (sout->stream_out_.common.set_parameters != nullptr) {
      sout->stream_out_.common.set_parameters(&sout->stream_out_.common,
                                              kvpairs);
    }
  }
  return 0;
}

static char* adev_get_parameters(const struct audio_hw_device* dev,
                                 const char* keys) {
  LOG(VERBOSE) << __func__ << ": keys=[" << keys << "]";
  return strdup("");
}

static int adev_init_check(const struct audio_hw_device* dev) { return 0; }

static int adev_set_voice_volume(struct audio_hw_device* dev, float volume) {
  LOG(VERBOSE) << __func__ << ": volume=" << volume;
  return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device* dev, float volume) {
  LOG(VERBOSE) << __func__ << ": volume=" << volume;
  return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device* dev, float* volume) {
  return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device* dev, bool muted) {
  LOG(VERBOSE) << __func__ << ": mute=" << muted;
  return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device* dev, bool* muted) {
  return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device* dev, audio_mode_t mode) {
  LOG(VERBOSE) << __func__ << ": mode=" << mode;
  return 0;
}

static int adev_set_mic_mute(struct audio_hw_device* dev, bool state) {
  LOG(VERBOSE) << __func__ << ": state=" << state;
  return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device* dev, bool* state) {
  return -ENOSYS;
}

static int adev_create_audio_patch(struct audio_hw_device* device,
                                   unsigned int num_sources,
                                   const struct audio_port_config* sources,
                                   unsigned int num_sinks,
                                   const struct audio_port_config* sinks,
                                   audio_patch_handle_t* handle) {
  if (device == nullptr || sources == nullptr || sinks == nullptr ||
      handle == nullptr || num_sources != 1 || num_sinks == 0 ||
      num_sinks > AUDIO_PATCH_PORTS_MAX) {
    return -EINVAL;
  }
  if (sources[0].type == AUDIO_PORT_TYPE_DEVICE) {
    if (num_sinks != 1 || sinks[0].type != AUDIO_PORT_TYPE_MIX) {
      return -EINVAL;
    }
  } else if (sources[0].type == AUDIO_PORT_TYPE_MIX) {
    for (unsigned int i = 0; i < num_sinks; i++) {
      if (sinks[i].type != AUDIO_PORT_TYPE_DEVICE) {
        return -EINVAL;
      }
    }
  } else {
    return -EINVAL;
  }

  auto* bluetooth_device = reinterpret_cast<BluetoothAudioDevice*>(device);
  std::lock_guard<std::mutex> guard(bluetooth_device->mutex_);
  if (*handle == AUDIO_PATCH_HANDLE_NONE) {
    *handle = ++bluetooth_device->next_unique_id;
  }

  LOG(INFO) << __func__ << ": device=" << std::hex << sinks[0].ext.device.type
            << " handle: " << *handle;
  return 0;
}

static int adev_release_audio_patch(struct audio_hw_device* device,
                                    audio_patch_handle_t patch_handle) {
  if (device == nullptr) {
    return -EINVAL;
  }
  LOG(INFO) << __func__ << ": patch_handle=" << patch_handle;
  return 0;
}

static int adev_get_audio_port_v7(struct audio_hw_device* device,
                                  struct audio_port_v7* port) {
  if (device == nullptr || port == nullptr) {
    return -EINVAL;
  }
  return -ENOSYS;
}

static int adev_get_audio_port(struct audio_hw_device* device,
                               struct audio_port* port) {
  if (device == nullptr || port == nullptr) {
    return -EINVAL;
  }
  return -ENOSYS;
}

static int adev_dump(const audio_hw_device_t* device, int fd) { return 0; }

static int adev_close(hw_device_t* device) {
  auto* bluetooth_device = reinterpret_cast<BluetoothAudioDevice*>(device);
  delete bluetooth_device;
  return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device) {
  LOG(VERBOSE) << __func__ << ": name=[" << name << "]";
  if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) return -EINVAL;

  auto bluetooth_audio_device = new BluetoothAudioDevice{};
  struct audio_hw_device* adev = &bluetooth_audio_device->audio_device_;
  if (!adev) return -ENOMEM;

  adev->common.tag = HARDWARE_DEVICE_TAG;
  adev->common.version = AUDIO_DEVICE_API_VERSION_3_2;
  adev->common.module = (struct hw_module_t*)module;
  adev->common.close = adev_close;

  adev->init_check = adev_init_check;
  adev->set_voice_volume = adev_set_voice_volume;
  adev->set_master_volume = adev_set_master_volume;
  adev->get_master_volume = adev_get_master_volume;
  adev->set_mode = adev_set_mode;
  adev->set_mic_mute = adev_set_mic_mute;
  adev->get_mic_mute = adev_get_mic_mute;
  adev->set_parameters = adev_set_parameters;
  adev->get_parameters = adev_get_parameters;
  adev->get_input_buffer_size = adev_get_input_buffer_size;
  adev->open_output_stream = adev_open_output_stream;
  adev->close_output_stream = adev_close_output_stream;
  adev->open_input_stream = adev_open_input_stream;
  adev->close_input_stream = adev_close_input_stream;
  adev->dump = adev_dump;
  adev->set_master_mute = adev_set_master_mute;
  adev->get_master_mute = adev_get_master_mute;
  adev->create_audio_patch = adev_create_audio_patch;
  adev->release_audio_patch = adev_release_audio_patch;
  adev->get_audio_port_v7 = adev_get_audio_port_v7;
  adev->get_audio_port = adev_get_audio_port;

  *device = &adev->common;
  return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common =
        {
            .tag = HARDWARE_MODULE_TAG,
            .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
            .hal_api_version = HARDWARE_HAL_API_VERSION,
            .id = AUDIO_HARDWARE_MODULE_ID,
            .name = "Bluetooth Audio HW HAL",
            .author = "The Android Open Source Project",
            .methods = &hal_module_methods,
        },
};
