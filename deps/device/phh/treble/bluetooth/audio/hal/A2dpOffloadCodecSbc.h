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

#include "A2dpOffloadCodec.h"

namespace aidl::android::hardware::bluetooth::audio {

struct SbcParameters : public CodecParameters {
  enum class AllocationMethod { SNR, LOUDNESS };

  AllocationMethod allocation_method;
  int block_length;
  int subbands;
  int min_bitpool;
  int max_bitpool;
};

class A2dpOffloadCodecSbc : public A2dpOffloadCodec {
  CodecInfo info_;

  A2dpStatus ParseConfiguration(const std::vector<uint8_t>& configuration,
                                CodecParameters* codec_parameters,
                                SbcParameters* sbc_parameters) const;

 public:
  A2dpOffloadCodecSbc();

  A2dpStatus ParseConfiguration(
      const std::vector<uint8_t>& configuration,
      CodecParameters* codec_parameters) const override {
    return ParseConfiguration(configuration, codec_parameters, nullptr);
  }

  A2dpStatus ParseConfiguration(const std::vector<uint8_t>& configuration,
                                SbcParameters* sbc_parameters) const {
    return ParseConfiguration(configuration, sbc_parameters, sbc_parameters);
  }

  bool BuildConfiguration(const std::vector<uint8_t>& remote_capabilities,
                          const std::optional<CodecParameters>& hint,
                          std::vector<uint8_t>* configuration) const override;
};

}  // namespace aidl::android::hardware::bluetooth::audio
