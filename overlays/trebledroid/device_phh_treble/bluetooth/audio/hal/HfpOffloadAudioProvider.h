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

#include "BluetoothAudioProvider.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

class HfpOffloadAudioProvider : public BluetoothAudioProvider {
 public:
  HfpOffloadAudioProvider();

  bool isValid(const SessionType& sessionType) override;

  ndk::ScopedAStatus startSession(
      const std::shared_ptr<IBluetoothAudioPort>& host_if,
      const AudioConfiguration& audio_config,
      const std::vector<LatencyMode>& latency_modes, DataMQDesc* _aidl_return);

 private:
  ndk::ScopedAStatus onSessionReady(DataMQDesc* _aidl_return) override;
};

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
