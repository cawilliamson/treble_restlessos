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

#pragma once

#include <aidl/android/hardware/audio/common/SinkMetadata.h>
#include <aidl/android/hardware/audio/common/SourceMetadata.h>
#include <aidl/android/hardware/bluetooth/audio/IBluetoothAudioProvider.h>
#include <aidl/android/hardware/bluetooth/audio/IBluetoothAudioProviderFactory.h>
#include <aidl/android/hardware/bluetooth/audio/LatencyMode.h>
#include <aidl/android/hardware/bluetooth/audio/SessionType.h>
#include <fmq/AidlMessageQueue.h>

#include <mutex>
#include <unordered_map>
#include <vector>

// To avoid inclusion of hardware/audio.h
struct sink_metadata;
struct source_metadata;

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

using ::aidl::android::hardware::common::fmq::MQDescriptor;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::android::AidlMessageQueue;

using ::aidl::android::hardware::audio::common::SinkMetadata;
using ::aidl::android::hardware::audio::common::SourceMetadata;

using MQDataType = int8_t;
using MQDataMode = SynchronizedReadWrite;
using DataMQ = AidlMessageQueue<MQDataType, MQDataMode>;
using DataMQDesc =
    ::aidl::android::hardware::common::fmq::MQDescriptor<MQDataType,
                                                         MQDataMode>;

static constexpr uint16_t kObserversCookieSize = 0x0010;  // 0x0000 ~ 0x000f
static constexpr uint16_t kObserversCookieUndefined =
    (static_cast<uint16_t>(SessionType::UNKNOWN) << 8 & 0xff00);
inline SessionType ObserversCookieGetSessionType(uint16_t cookie) {
  return static_cast<SessionType>(cookie >> 8 & 0x00ff);
}
inline uint16_t ObserversCookieGetInitValue(SessionType session_type) {
  return (static_cast<uint16_t>(session_type) << 8 & 0xff00);
}
inline uint16_t ObserversCookieGetUpperBound(SessionType session_type) {
  return (static_cast<uint16_t>(session_type) << 8 & 0xff00) +
         kObserversCookieSize;
}

/***
 * This presents the callbacks of started / suspended and session changed,
 * and the bluetooth_audio module uses to receive the status notification
 ***/
struct PortStatusCallbacks {
  /***
   * control_result_cb_ - when the Bluetooth stack reports results of
   * streamStarted or streamSuspended, the BluetoothAudioProvider will invoke
   * this callback to report to the bluetooth_audio module.
   * @param: cookie - indicates which bluetooth_audio output should handle
   * @param: start_resp - this report is for startStream or not
   * @param: status - the result of startStream
   ***/
  std::function<void(uint16_t cookie, bool start_resp,
                     BluetoothAudioStatus status)>
      control_result_cb_;
  /***
   * session_changed_cb_ - when the Bluetooth stack start / end session, the
   * BluetoothAudioProvider will invoke this callback to notify to the
   * bluetooth_audio module.
   * @param: cookie - indicates which bluetooth_audio output should handle
   ***/
  std::function<void(uint16_t cookie)> session_changed_cb_;
  /***
   * audio_configuration_changed_cb_ - when the Bluetooth stack change the audio
   * configuration, the BluetoothAudioProvider will invoke this callback to
   * notify to the bluetooth_audio module.
   * @param: cookie - indicates which bluetooth_audio output should handle
   ***/
  std::function<void(uint16_t cookie)> audio_configuration_changed_cb_;
  /***
   * low_latency_mode_allowed_cb_ - when the Bluetooth stack low latency mode
   * allowed or disallowed, the BluetoothAudioProvider will invoke
   * this callback to report to the bluetooth_audio module.
   * @param: cookie - indicates which bluetooth_audio output should handle
   * @param: allowed - indicates if low latency mode is allowed
   ***/
  std::function<void(uint16_t cookie, bool allowed)>
      low_latency_mode_allowed_cb_;
};

class BluetoothAudioSession {
 public:
  BluetoothAudioSession(const SessionType& session_type);

  /***
   * The function helps to check if this session is ready or not
   * @return: true if the Bluetooth stack has started the specified session
   ***/
  bool IsSessionReady();

  /***
   * The report function is used to report that the Bluetooth stack has started
   * this session without any failure, and will invoke session_changed_cb_ to
   * notify those registered bluetooth_audio outputs
   ***/
  void OnSessionStarted(const std::shared_ptr<IBluetoothAudioPort> stack_iface,
                        const DataMQDesc* mq_desc,
                        const AudioConfiguration& audio_config,
                        const std::vector<LatencyMode>& latency_modes);

  /***
   * The report function is used to report that the Bluetooth stack has ended
   * the session, and will invoke session_changed_cb_ to notify registered
   * bluetooth_audio outputs
   ***/
  void OnSessionEnded();

  /***
   * The report function is used to report that the Bluetooth stack has notified
   * the result of startStream or suspendStream, and will invoke
   * control_result_cb_ to notify registered bluetooth_audio outputs
   ***/
  void ReportControlStatus(bool start_resp, BluetoothAudioStatus status);

  /***
   * The control function helps the bluetooth_audio module to register
   * PortStatusCallbacks
   * @return: cookie - the assigned number to this bluetooth_audio output
   ***/
  uint16_t RegisterStatusCback(const PortStatusCallbacks& cbacks);

  /***
   * The control function helps the bluetooth_audio module to unregister
   * PortStatusCallbacks
   * @param: cookie - indicates which bluetooth_audio output is
   ***/
  void UnregisterStatusCback(uint16_t cookie);

  /***
   * The control function is for the bluetooth_audio module to get the current
   * AudioConfiguration
   ***/
  const AudioConfiguration GetAudioConfig();

  /***
   * The report function is used to report that the Bluetooth stack has notified
   * the audio configuration changed, and will invoke
   * audio_configuration_changed_cb_ to notify registered bluetooth_audio
   * outputs
   ***/
  void ReportAudioConfigChanged(const AudioConfiguration& audio_config);

  /***
   * The report function is used to report that the Bluetooth stack has notified
   * the low latency mode allowed changed, and will invoke
   * low_latency_mode_allowed_changed_cb to notify registered bluetooth_audio
   * outputs
   ***/
  void ReportLowLatencyModeAllowedChanged(bool allowed);
  /***
   * Those control functions are for the bluetooth_audio module to start,
   * suspend, stop stream, to check position, and to update metadata.
   ***/
  bool StartStream(bool low_latency);
  bool SuspendStream();
  void StopStream();
  bool GetPresentationPosition(PresentationPosition& presentation_position);
  void UpdateSourceMetadata(const struct source_metadata& source_metadata);
  void UpdateSinkMetadata(const struct sink_metadata& sink_metadata);
  // New versions for AIDL-only clients.
  bool UpdateSourceMetadata(const SourceMetadata& hal_source_metadata);
  bool UpdateSinkMetadata(const SinkMetadata& hal_sink_metadata);

  std::vector<LatencyMode> GetSupportedLatencyModes();
  void SetLatencyMode(const LatencyMode& latency_mode);

  // The control function writes stream to FMQ
  size_t OutWritePcmData(const void* buffer, size_t bytes);
  // The control function read stream from FMQ
  size_t InReadPcmData(void* buffer, size_t bytes);

  // Return if IBluetoothAudioProviderFactory implementation existed
  static bool IsAidlAvailable();

 private:
  // using recursive_mutex to allow hwbinder to re-enter again.
  std::recursive_mutex mutex_;
  SessionType session_type_;

  // audio control path to use for both software and offloading
  std::shared_ptr<IBluetoothAudioPort> stack_iface_;
  // audio data path (FMQ) for software encoding
  std::unique_ptr<DataMQ> data_mq_;
  // audio data configuration for both software and offloading
  std::unique_ptr<AudioConfiguration> audio_config_;
  std::vector<LatencyMode> latency_modes_;
  bool low_latency_allowed_ = true;

  // saving those registered bluetooth_audio's callbacks
  std::unordered_map<uint16_t, std::shared_ptr<struct PortStatusCallbacks>>
      observers_;

  bool UpdateDataPath(const DataMQDesc* mq_desc);
  bool UpdateAudioConfig(const AudioConfiguration& audio_config);
  // invoking the registered session_changed_cb_
  void ReportSessionStatus();

  static inline std::atomic<bool> is_aidl_checked = false;
  static inline std::atomic<bool> is_aidl_available = false;
  // BEGIN sysbta
  static inline const std::string kDefaultAudioProviderFactoryInterface =
      std::string() + IBluetoothAudioProviderFactory::descriptor + "/sysbta";
  // END sysbta
};

class BluetoothAudioSessionInstance {
 public:
  // The API is to fetch the specified session of A2DP / Hearing Aid
  static std::shared_ptr<BluetoothAudioSession> GetSessionInstance(
      const SessionType& session_type);

 private:
  static std::mutex mutex_;
  static std::unordered_map<SessionType, std::shared_ptr<BluetoothAudioSession>>
      sessions_map_;
};

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
