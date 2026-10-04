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

#include <aidl/android/hardware/bluetooth/audio/A2dpConfiguration.h>
#include <aidl/android/hardware/bluetooth/audio/A2dpConfigurationHint.h>
#include <aidl/android/hardware/bluetooth/audio/A2dpRemoteCapabilities.h>

#include <memory>

#include "A2dpOffloadCodec.h"

namespace aidl::android::hardware::bluetooth::audio {

class A2dpOffloadCodecFactory {
  std::vector<std::shared_ptr<const A2dpOffloadCodec>> ranked_codecs_;

 public:
  const std::string name;
  const std::vector<std::shared_ptr<const A2dpOffloadCodec>>& codecs;

  A2dpOffloadCodecFactory();

  std::shared_ptr<const A2dpOffloadCodec> GetCodec(CodecId id) const;

  bool GetConfiguration(const std::vector<A2dpRemoteCapabilities>&,
                        const A2dpConfigurationHint& hint,
                        A2dpConfiguration* configuration) const;
};

}  // namespace aidl::android::hardware::bluetooth::audio
