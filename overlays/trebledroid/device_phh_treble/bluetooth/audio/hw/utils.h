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

#include <string>
#include <unordered_map>

namespace android {
namespace bluetooth {
namespace audio {
namespace utils {

// Creates a hash map based on the |params| string containing key and value
// pairs.  Pairs are expected in the form "key=value" separated by the ';'
// character.  Both ';' and '=' characters are invalid in keys or values.
// Examples:
//   "key0"                      -> map: [key0]=""
//   "key0=value0;key1=value1;"  -> map: [key0]="value0" [key1]="value1"
//   "key0=;key1=value1;"        -> map: [key0]="" [key1]="value1"
//   "=value0;key1=value1;"      -> map: [key1]="value1"
std::unordered_map<std::string, std::string> ParseAudioParams(
    const std::string& params);

// Dumps the contents of the hash_map to the log for debugging purposes.
// If |map| is not NULL, all entries of |map| will be dumped, otherwise
// nothing will be dumped. Note that this function does not take the ownership
// of the |map|.
std::string GetAudioParamString(
    std::unordered_map<std::string, std::string>& params_map);

size_t FrameCount(uint64_t microseconds, uint32_t sample_rate);
}  // namespace utils
}  // namespace audio
}  // namespace bluetooth
}  // namespace android
