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

namespace aidl::android::hardware::bluetooth::audio {

class A2dpBits {
  const uint8_t* cdata_;
  uint8_t* data_;

 public:
  A2dpBits(const std::vector<uint8_t>& vector)
      : cdata_(vector.data()), data_(nullptr) {}

  A2dpBits(std::vector<uint8_t>& vector)
      : cdata_(vector.data()), data_(vector.data()) {}

  struct Range {
    const int first, len;
    constexpr Range(int first, int last)
        : first(first), len(last - first + 1) {}
    constexpr Range(int index) : first(index), len(1) {}
  };

  constexpr bool get(int bit) const {
    return (cdata_[bit >> 3] >> (7 - (bit & 7))) & 1;
  }

  constexpr unsigned get(const Range& range) const {
    unsigned v(0);
    for (int i = 0; i < range.len; i++)
      v |= get(range.first + i) << ((range.len - 1) - i);
    return v;
  }

  constexpr void set(int bit, int value = 1) {
    uint8_t m = 1 << (7 - (bit & 7));
    if (value)
      data_[bit >> 3] |= m;
    else
      data_[bit >> 3] &= ~m;
  }

  constexpr void set(const Range& range, int value) {
    for (int i = 0; i < range.len; i++)
      set(range.first + i, (value >> ((range.len - 1) - i)) & 1);
  }

  constexpr int find_active_bit(const Range& range) const {
    unsigned v = get(range);
    int i = 0;
    for (; i < range.len && ((v >> i) & 1) == 0; i++)
      ;
    return i < range.len && (v ^ (1 << i)) == 0
               ? range.first + (range.len - 1) - i
               : -1;
  }
};

}  // namespace aidl::android::hardware::bluetooth::audio
