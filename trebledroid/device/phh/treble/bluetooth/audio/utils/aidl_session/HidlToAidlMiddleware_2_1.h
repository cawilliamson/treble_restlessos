/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include <android/hardware/bluetooth/audio/2.1/types.h>

#include "../session/BluetoothAudioSession.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

using SessionType_2_1 =
    ::android::hardware::bluetooth::audio::V2_1::SessionType;
using AudioConfig_2_1 =
    ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration;

class HidlToAidlMiddleware_2_1 {
 public:
  static const AudioConfig_2_1 GetAudioConfig(
      const SessionType_2_1& session_type);
};

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
