/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <aidl/android/hardware/bluetooth/audio/A2dpStatus.h>
#include <aidl/android/hardware/bluetooth/audio/ChannelMode.h>
#include <aidl/android/hardware/bluetooth/audio/CodecInfo.h>
#include <aidl/android/hardware/bluetooth/audio/CodecParameters.h>

namespace aidl::android::hardware::bluetooth::audio {

class A2dpOffloadCodec {
 protected:
  A2dpOffloadCodec(const CodecInfo& info) : info(info) {}
  virtual ~A2dpOffloadCodec() {}

 public:
  const CodecInfo& info;

  const CodecId& GetCodecId() const { return info.id; }

  virtual A2dpStatus ParseConfiguration(
      const std::vector<uint8_t>& configuration,
      CodecParameters* codec_parameters) const = 0;

  virtual bool BuildConfiguration(
      const std::vector<uint8_t>& remote_capabilities,
      const std::optional<CodecParameters>& hint,
      std::vector<uint8_t>* configuration) const = 0;
};

}  // namespace aidl::android::hardware::bluetooth::audio
