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

#define LOG_TAG "BTAudioHalUtils"

#include "utils.h"

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <log/log.h>
#include <stdlib.h>
#include <sstream>
#include <vector>

namespace android {
namespace bluetooth {
namespace audio {
namespace utils {

std::unordered_map<std::string, std::string> ParseAudioParams(
    const std::string& params) {
  std::vector<std::string> segments = android::base::Split(params, ";");
  std::unordered_map<std::string, std::string> params_map;
  for (const auto& segment : segments) {
    if (segment.length() == 0) {
      continue;
    }
    std::vector<std::string> kv = android::base::Split(segment, "=");
    if (kv[0].empty()) {
      LOG(WARNING) << __func__ << ": Invalid audio parameter " << segment;
      continue;
    }
    params_map[kv[0]] = (kv.size() > 1 ? kv[1] : "");
  }
  return params_map;
}

std::string GetAudioParamString(
    std::unordered_map<std::string, std::string>& params_map) {
  std::ostringstream sout;
  for (const auto& ptr : params_map) {
    sout << "key: '" << ptr.first << "' value: '" << ptr.second << "'\n";
  }
  return sout.str();
}

size_t FrameCount(uint64_t microseconds, uint32_t sample_rate) {
  return (microseconds * sample_rate) / 1000000;
}

}  // namespace utils
}  // namespace audio
}  // namespace bluetooth
}  // namespace android
