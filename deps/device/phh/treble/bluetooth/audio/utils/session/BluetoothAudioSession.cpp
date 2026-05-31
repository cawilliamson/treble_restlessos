/*
 * Copyright 2018 The Android Open Source Project
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

#define LOG_TAG "BTAudioProviderSession"

#include "BluetoothAudioSession.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "../aidl_session/HidlToAidlMiddleware_2_0.h"

namespace android {
namespace bluetooth {
namespace audio {

using ::aidl::android::hardware::bluetooth::audio::HidlToAidlMiddleware_2_0;
using ::android::hardware::audio::common::V5_0::AudioContentType;
using ::android::hardware::audio::common::V5_0::AudioUsage;
using ::android::hardware::audio::common::V5_0::PlaybackTrackMetadata;
using ::android::hardware::audio::common::V5_0::SourceMetadata;
using ::android::hardware::bluetooth::audio::V2_0::CodecType;
using ::android::hardware::bluetooth::audio::V2_0::TimeSpec;

const CodecConfiguration BluetoothAudioSession::kInvalidCodecConfiguration = {
    .codecType = CodecType::UNKNOWN,
    .encodedAudioBitrate = 0x00000000,
    .peerMtu = 0xffff,
    .isScmstEnabled = false,
    .config = {}};
AudioConfiguration BluetoothAudioSession::invalidSoftwareAudioConfiguration =
    {};
AudioConfiguration BluetoothAudioSession::invalidOffloadAudioConfiguration = {};

static constexpr int kFmqSendTimeoutMs = 1000;  // 1000 ms timeout for sending
static constexpr int kFmqReceiveTimeoutMs =
    1000;                                       // 1000 ms timeout for receiving
static constexpr int kWritePollMs = 1;          // polled non-blocking interval
static constexpr int kReadPollMs = 1;           // polled non-blocking interval

static inline timespec timespec_convert_from_hal(const TimeSpec& TS) {
  return {.tv_sec = static_cast<long>(TS.tvSec),
          .tv_nsec = static_cast<long>(TS.tvNSec)};
}

BluetoothAudioSession::BluetoothAudioSession(const SessionType& session_type)
    : session_type_(session_type), stack_iface_(nullptr), mDataMQ(nullptr) {
  invalidSoftwareAudioConfiguration.pcmConfig(kInvalidPcmParameters);
  invalidOffloadAudioConfiguration.codecConfig(kInvalidCodecConfiguration);
}

// The report function is used to report that the Bluetooth stack has started
// this session without any failure, and will invoke session_changed_cb_ to
// notify those registered bluetooth_audio outputs
void BluetoothAudioSession::OnSessionStarted(
    const sp<IBluetoothAudioPort> stack_iface, const DataMQ::Descriptor* dataMQ,
    const AudioConfiguration& audio_config) {
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  if (stack_iface == nullptr) {
    LOG(ERROR) << __func__ << " - SessionType=" << toString(session_type_)
               << ", IBluetoothAudioPort Invalid";
  } else if (!UpdateAudioConfig(audio_config)) {
    LOG(ERROR) << __func__ << " - SessionType=" << toString(session_type_)
               << ", AudioConfiguration=" << toString(audio_config)
               << " Invalid";
  } else if (!UpdateDataPath(dataMQ)) {
    LOG(ERROR) << __func__ << " - SessionType=" << toString(session_type_)
               << " DataMQ Invalid";
    audio_config_ =
        (session_type_ == SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH
             ? kInvalidOffloadAudioConfiguration
             : kInvalidSoftwareAudioConfiguration);
  } else {
    stack_iface_ = stack_iface;
    LOG(INFO) << __func__ << " - SessionType=" << toString(session_type_)
              << ", AudioConfiguration=" << toString(audio_config);
    ReportSessionStatus();
  }
}

// The report function is used to report that the Bluetooth stack has ended the
// session, and will invoke session_changed_cb_ to notify registered
// bluetooth_audio outputs
void BluetoothAudioSession::OnSessionEnded() {
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  bool toggled = IsSessionReady();
  LOG(INFO) << __func__ << " - SessionType=" << toString(session_type_);
  audio_config_ = (session_type_ == SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH
                       ? kInvalidOffloadAudioConfiguration
                       : kInvalidSoftwareAudioConfiguration);
  stack_iface_ = nullptr;
  UpdateDataPath(nullptr);
  if (toggled) {
    ReportSessionStatus();
  }
}

// invoking the registered session_changed_cb_
void BluetoothAudioSession::ReportSessionStatus() {
  // This is locked already by OnSessionStarted / OnSessionEnded
  if (observers_.empty()) {
    LOG(INFO) << __func__ << " - SessionType=" << toString(session_type_)
              << " has NO port state observer";
    return;
  }
  for (auto& observer : observers_) {
    uint16_t cookie = observer.first;
    std::shared_ptr<struct PortStatusCallbacks> cb = observer.second;
    LOG(INFO) << __func__ << " - SessionType=" << toString(session_type_)
              << " notify to bluetooth_audio=0x"
              << android::base::StringPrintf("%04x", cookie);
    cb->session_changed_cb_(cookie);
  }
}

// The report function is used to report that the Bluetooth stack has notified
// the result of startStream or suspendStream, and will invoke
// control_result_cb_ to notify registered bluetooth_audio outputs
void BluetoothAudioSession::ReportControlStatus(
    bool start_resp, const BluetoothAudioStatus& status) {
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  if (observers_.empty()) {
    LOG(WARNING) << __func__ << " - SessionType=" << toString(session_type_)
                 << " has NO port state observer";
    return;
  }
  for (auto& observer : observers_) {
    uint16_t cookie = observer.first;
    std::shared_ptr<struct PortStatusCallbacks> cb = observer.second;
    LOG(INFO) << __func__ << " - status=" << toString(status)
              << " for SessionType=" << toString(session_type_)
              << ", bluetooth_audio=0x"
              << android::base::StringPrintf("%04x", cookie)
              << (start_resp ? " started" : " suspended");
    cb->control_result_cb_(cookie, start_resp, status);
  }
}

// The function helps to check if this session is ready or not
// @return: true if the Bluetooth stack has started the specified session
bool BluetoothAudioSession::IsSessionReady() {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::IsSessionReady(session_type_);
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  bool dataMQ_valid =
      (session_type_ == SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH ||
       (mDataMQ != nullptr && mDataMQ->isValid()));
  return stack_iface_ != nullptr && dataMQ_valid;
}

bool BluetoothAudioSession::UpdateDataPath(const DataMQ::Descriptor* dataMQ) {
  if (dataMQ == nullptr) {
    // usecase of reset by nullptr
    mDataMQ = nullptr;
    return true;
  }
  std::unique_ptr<DataMQ> tempDataMQ;
  tempDataMQ.reset(new DataMQ(*dataMQ));
  if (!tempDataMQ || !tempDataMQ->isValid()) {
    mDataMQ = nullptr;
    return false;
  }
  mDataMQ = std::move(tempDataMQ);
  return true;
}

bool BluetoothAudioSession::UpdateAudioConfig(
    const AudioConfiguration& audio_config) {
  bool is_software_session =
      (session_type_ == SessionType::A2DP_SOFTWARE_ENCODING_DATAPATH ||
       session_type_ == SessionType::HEARING_AID_SOFTWARE_ENCODING_DATAPATH);
  bool is_offload_session =
      (session_type_ == SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH);
  auto audio_config_discriminator = audio_config.getDiscriminator();
  bool is_software_audio_config =
      (is_software_session &&
       audio_config_discriminator ==
           AudioConfiguration::hidl_discriminator::pcmConfig);
  bool is_offload_audio_config =
      (is_offload_session &&
       audio_config_discriminator ==
           AudioConfiguration::hidl_discriminator::codecConfig);
  if (!is_software_audio_config && !is_offload_audio_config) {
    return false;
  }
  audio_config_ = audio_config;
  return true;
}

// The control function helps the bluetooth_audio module to register
// PortStatusCallbacks
// @return: cookie - the assigned number to this bluetooth_audio output
uint16_t BluetoothAudioSession::RegisterStatusCback(
    const PortStatusCallbacks& cbacks) {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::RegisterControlResultCback(session_type_,
                                                                cbacks);
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  uint16_t cookie = ObserversCookieGetInitValue(session_type_);
  uint16_t cookie_upper_bound = ObserversCookieGetUpperBound(session_type_);

  while (cookie < cookie_upper_bound) {
    if (observers_.find(cookie) == observers_.end()) {
      break;
    }
    ++cookie;
  }
  if (cookie >= cookie_upper_bound) {
    LOG(ERROR) << __func__ << " - SessionType=" << toString(session_type_)
               << " has " << observers_.size()
               << " observers already (No Resource)";
    return kObserversCookieUndefined;
  }
  std::shared_ptr<struct PortStatusCallbacks> cb =
      std::make_shared<struct PortStatusCallbacks>();
  *cb = cbacks;
  observers_[cookie] = cb;
  return cookie;
}

// The control function helps the bluetooth_audio module to unregister
// PortStatusCallbacks
// @param: cookie - indicates which bluetooth_audio output is
void BluetoothAudioSession::UnregisterStatusCback(uint16_t cookie) {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::UnregisterControlResultCback(session_type_,
                                                                  cookie);
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  if (observers_.erase(cookie) != 1) {
    LOG(WARNING) << __func__ << " - SessionType=" << toString(session_type_)
                 << " no such provider=0x"
                 << android::base::StringPrintf("%04x", cookie);
  }
}

// The control function is for the bluetooth_audio module to get the current
// AudioConfiguration
const AudioConfiguration& BluetoothAudioSession::GetAudioConfig() {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return (audio_config_ =
                HidlToAidlMiddleware_2_0::GetAudioConfig(session_type_));
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  if (IsSessionReady()) {
    return audio_config_;
  } else if (session_type_ == SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH) {
    return kInvalidOffloadAudioConfiguration;
  } else {
    return kInvalidSoftwareAudioConfiguration;
  }
}

// Those control functions are for the bluetooth_audio module to start, suspend,
// stop stream, to check position, and to update metadata.
bool BluetoothAudioSession::StartStream() {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::StartStream(session_type_);
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  if (!IsSessionReady()) {
    LOG(DEBUG) << __func__ << " - SessionType=" << toString(session_type_)
               << " has NO session";
    return false;
  }
  auto hal_retval = stack_iface_->startStream();
  if (!hal_retval.isOk()) {
    LOG(WARNING) << __func__ << " - IBluetoothAudioPort SessionType="
                 << toString(session_type_) << " failed";
    return false;
  }
  return true;
}

bool BluetoothAudioSession::SuspendStream() {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::SuspendStream(session_type_);
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  if (!IsSessionReady()) {
    LOG(DEBUG) << __func__ << " - SessionType=" << toString(session_type_)
               << " has NO session";
    return false;
  }
  auto hal_retval = stack_iface_->suspendStream();
  if (!hal_retval.isOk()) {
    LOG(WARNING) << __func__ << " - IBluetoothAudioPort SessionType="
                 << toString(session_type_) << " failed";
    return false;
  }
  return true;
}

void BluetoothAudioSession::StopStream() {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::StopStream(session_type_);
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  if (!IsSessionReady()) {
    return;
  }
  auto hal_retval = stack_iface_->stopStream();
  if (!hal_retval.isOk()) {
    LOG(WARNING) << __func__ << " - IBluetoothAudioPort SessionType="
                 << toString(session_type_) << " failed";
  }
}

bool BluetoothAudioSession::GetPresentationPosition(
    uint64_t* remote_delay_report_ns, uint64_t* total_bytes_readed,
    timespec* data_position) {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::GetPresentationPosition(
        session_type_, remote_delay_report_ns, total_bytes_readed,
        data_position);
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  if (!IsSessionReady()) {
    LOG(DEBUG) << __func__ << " - SessionType=" << toString(session_type_)
               << " has NO session";
    return false;
  }
  bool retval = false;
  auto hal_retval = stack_iface_->getPresentationPosition(
      [&retval, &remote_delay_report_ns, &total_bytes_readed, &data_position](
          BluetoothAudioStatus status,
          const uint64_t& remoteDeviceAudioDelayNanos,
          uint64_t transmittedOctets,
          const TimeSpec& transmittedOctetsTimeStamp) {
        if (status == BluetoothAudioStatus::SUCCESS) {
          if (remote_delay_report_ns)
            *remote_delay_report_ns = remoteDeviceAudioDelayNanos;
          if (total_bytes_readed) *total_bytes_readed = transmittedOctets;
          if (data_position)
            *data_position =
                timespec_convert_from_hal(transmittedOctetsTimeStamp);
          retval = true;
        }
      });
  if (!hal_retval.isOk()) {
    LOG(WARNING) << __func__ << " - IBluetoothAudioPort SessionType="
                 << toString(session_type_) << " failed";
    return false;
  }
  return retval;
}

void BluetoothAudioSession::UpdateTracksMetadata(
    const struct source_metadata* source_metadata) {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::UpdateTracksMetadata(session_type_,
                                                          source_metadata);
  std::lock_guard<std::recursive_mutex> guard(mutex_);
  if (!IsSessionReady()) {
    LOG(DEBUG) << __func__ << " - SessionType=" << toString(session_type_)
               << " has NO session";
    return;
  }

  ssize_t track_count = source_metadata->track_count;
  LOG(INFO) << __func__ << " - SessionType=" << toString(session_type_) << ", "
            << track_count << " track(s)";
  if (session_type_ == SessionType::A2DP_SOFTWARE_ENCODING_DATAPATH ||
      session_type_ == SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH) {
    return;
  }

  struct playback_track_metadata* track = source_metadata->tracks;
  SourceMetadata sourceMetadata;
  PlaybackTrackMetadata* halMetadata;

  sourceMetadata.tracks.resize(track_count);
  halMetadata = sourceMetadata.tracks.data();
  while (track_count && track) {
    halMetadata->usage = static_cast<AudioUsage>(track->usage);
    halMetadata->contentType =
        static_cast<AudioContentType>(track->content_type);
    halMetadata->gain = track->gain;
    LOG(VERBOSE) << __func__ << " - SessionType=" << toString(session_type_)
                 << ", usage=" << toString(halMetadata->usage)
                 << ", content=" << toString(halMetadata->contentType)
                 << ", gain=" << halMetadata->gain;
    --track_count;
    ++track;
    ++halMetadata;
  }
  auto hal_retval = stack_iface_->updateMetadata(sourceMetadata);
  if (!hal_retval.isOk()) {
    LOG(WARNING) << __func__ << " - IBluetoothAudioPort SessionType="
                 << toString(session_type_) << " failed";
  }
}

// The control function writes stream to FMQ
size_t BluetoothAudioSession::OutWritePcmData(const void* buffer,
                                              size_t bytes) {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::OutWritePcmData(session_type_, buffer,
                                                     bytes);
  if (buffer == nullptr || !bytes) return 0;
  size_t totalWritten = 0;
  int ms_timeout = kFmqSendTimeoutMs;
  do {
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    if (!IsSessionReady()) break;
    size_t availableToWrite = mDataMQ->availableToWrite();
    if (availableToWrite) {
      if (availableToWrite > (bytes - totalWritten)) {
        availableToWrite = bytes - totalWritten;
      }

      if (!mDataMQ->write(static_cast<const uint8_t*>(buffer) + totalWritten,
                          availableToWrite)) {
        ALOGE("FMQ datapath writting %zu/%zu failed", totalWritten, bytes);
        return totalWritten;
      }
      totalWritten += availableToWrite;
    } else if (ms_timeout >= kWritePollMs) {
      lock.unlock();
      usleep(kWritePollMs * 1000);
      ms_timeout -= kWritePollMs;
    } else {
      ALOGD("data %zu/%zu overflow %d ms", totalWritten, bytes,
            (kFmqSendTimeoutMs - ms_timeout));
      return totalWritten;
    }
  } while (totalWritten < bytes);
  return totalWritten;
}

// The control function reads stream from FMQ
size_t BluetoothAudioSession::InReadPcmData(void* buffer, size_t bytes) {
  if (HidlToAidlMiddleware_2_0::IsAidlAvailable())
    return HidlToAidlMiddleware_2_0::InReadPcmData(session_type_, buffer,
                                                   bytes);
  if (buffer == nullptr || !bytes) return 0;
  size_t totalRead = 0;
  int ms_timeout = kFmqReceiveTimeoutMs;
  do {
    std::unique_lock<std::recursive_mutex> lock(mutex_);
    if (!IsSessionReady()) break;
    size_t availableToRead = mDataMQ->availableToRead();
    if (availableToRead) {
      if (availableToRead > (bytes - totalRead)) {
        availableToRead = bytes - totalRead;
      }
      if (!mDataMQ->read(static_cast<uint8_t*>(buffer) + totalRead,
                         availableToRead)) {
        ALOGE("FMQ datapath reading %zu/%zu failed", totalRead, bytes);
        return totalRead;
      }
      totalRead += availableToRead;
    } else if (ms_timeout >= kReadPollMs) {
      lock.unlock();
      usleep(kReadPollMs * 1000);
      ms_timeout -= kReadPollMs;
      continue;
    } else {
      ALOGD("in data %zu/%zu overflow %d ms", totalRead, bytes,
            (kFmqReceiveTimeoutMs - ms_timeout));
      return totalRead;
    }
  } while (totalRead < bytes);
  return totalRead;
}

std::unique_ptr<BluetoothAudioSessionInstance>
    BluetoothAudioSessionInstance::instance_ptr =
        std::unique_ptr<BluetoothAudioSessionInstance>(
            new BluetoothAudioSessionInstance());

// API to fetch the session of A2DP / Hearing Aid
std::shared_ptr<BluetoothAudioSession>
BluetoothAudioSessionInstance::GetSessionInstance(
    const SessionType& session_type) {
  std::lock_guard<std::mutex> guard(instance_ptr->mutex_);
  if (!instance_ptr->sessions_map_.empty()) {
    auto entry = instance_ptr->sessions_map_.find(session_type);
    if (entry != instance_ptr->sessions_map_.end()) {
      return entry->second;
    }
  }
  std::shared_ptr<BluetoothAudioSession> session_ptr =
      std::make_shared<BluetoothAudioSession>(session_type);
  instance_ptr->sessions_map_[session_type] = session_ptr;
  return session_ptr;
}

}  // namespace audio
}  // namespace bluetooth
}  // namespace android
