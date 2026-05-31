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

#include <android/hardware/bluetooth/audio/2.0/types.h>

#include "../session/BluetoothAudioSession.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

using SessionType_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::SessionType;
using PortStatusCallbacks_2_0 =
    ::android::bluetooth::audio::PortStatusCallbacks;
using AudioConfig_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::AudioConfiguration;

class HidlToAidlMiddleware_2_0 {
 public:
  static bool IsAidlAvailable();

  static bool IsSessionReady(const SessionType_2_0& session_type);

  static uint16_t RegisterControlResultCback(
      const SessionType_2_0& session_type,
      const PortStatusCallbacks_2_0& cbacks);

  static void UnregisterControlResultCback(const SessionType_2_0& session_type,
                                           uint16_t cookie);

  static const AudioConfig_2_0 GetAudioConfig(
      const SessionType_2_0& session_type);

  static bool StartStream(const SessionType_2_0& session_type);

  static void StopStream(const SessionType_2_0& session_type);

  static bool SuspendStream(const SessionType_2_0& session_type);

  static bool GetPresentationPosition(const SessionType_2_0& session_type,
                                      uint64_t* remote_delay_report_ns,
                                      uint64_t* total_bytes_readed,
                                      timespec* data_position);

  static void UpdateTracksMetadata(
      const SessionType_2_0& session_type,
      const struct source_metadata* source_metadata);

  static size_t OutWritePcmData(const SessionType_2_0& session_type,
                                const void* buffer, size_t bytes);

  static size_t InReadPcmData(const SessionType_2_0& session_type, void* buffer,
                              size_t bytes);
};

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
