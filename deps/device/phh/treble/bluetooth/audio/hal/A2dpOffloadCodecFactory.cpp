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

#include "A2dpOffloadCodecFactory.h"

#include <algorithm>
#include <cassert>

#include "A2dpOffloadCodecAac.h"
#include "A2dpOffloadCodecSbc.h"

namespace aidl::android::hardware::bluetooth::audio {

/**
 * Local Capabilities Configuration
 */

enum : bool {
  kEnableAac = true,
  kEnableSbc = true,
};

/**
 * Class implementation
 */

A2dpOffloadCodecFactory::A2dpOffloadCodecFactory()
    : name("Offload"), codecs(ranked_codecs_) {
  ranked_codecs_.reserve(kEnableAac + kEnableSbc);

  if (kEnableAac)
    ranked_codecs_.push_back(std::make_shared<A2dpOffloadCodecAac>());
  if (kEnableSbc)
    ranked_codecs_.push_back(std::make_shared<A2dpOffloadCodecSbc>());
}

std::shared_ptr<const A2dpOffloadCodec> A2dpOffloadCodecFactory::GetCodec(
    CodecId id) const {
  auto codec = std::find_if(begin(ranked_codecs_), end(ranked_codecs_),
                            [&](auto c) { return id == c->info.id; });

  return codec != end(ranked_codecs_) ? *codec : nullptr;
}

bool A2dpOffloadCodecFactory::GetConfiguration(
    const std::vector<A2dpRemoteCapabilities>& remote_capabilities,
    const A2dpConfigurationHint& hint, A2dpConfiguration* configuration) const {
  decltype(ranked_codecs_) codecs;

  codecs.reserve(ranked_codecs_.size());

  auto hinted_codec =
      std::find_if(begin(ranked_codecs_), end(ranked_codecs_),
                   [&](auto c) { return hint.codecId == c->info.id; });

  if (hinted_codec != end(ranked_codecs_)) codecs.push_back(*hinted_codec);

  std::copy_if(begin(ranked_codecs_), end(ranked_codecs_),
               std::back_inserter(codecs),
               [&](auto c) { return c != *hinted_codec; });

  for (auto codec : codecs) {
    auto rc =
        std::find_if(begin(remote_capabilities), end(remote_capabilities),
                     [&](auto& rc__) { return codec->info.id == rc__.id; });

    if ((rc == end(remote_capabilities)) ||
        !codec->BuildConfiguration(rc->capabilities, hint.codecParameters,
                                   &configuration->configuration))
      continue;

    configuration->id = codec->info.id;
    A2dpStatus status = codec->ParseConfiguration(configuration->configuration,
                                                  &configuration->parameters);
    assert(status == A2dpStatus::OK);

    configuration->remoteSeid = rc->seid;

    return true;
  }

  return false;
}

}  // namespace aidl::android::hardware::bluetooth::audio
