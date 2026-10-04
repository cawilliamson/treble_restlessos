/*
 * Copyright 2022 The Android Open Source Project
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

#define LOG_TAG "BTAudioHalDeviceProxyAIDL"

#include "device_port_proxy.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <audio_utils/primitives.h>
#include <inttypes.h>
#include <log/log.h>
#include <stdlib.h>

#include "BluetoothAudioSessionControl.h"
#include "stream_apis.h"
#include "utils.h"

namespace android {
namespace bluetooth {
namespace audio {
namespace aidl {

using ::aidl::android::hardware::audio::common::SinkMetadata;
using ::aidl::android::hardware::audio::common::SourceMetadata;
using ::aidl::android::hardware::bluetooth::audio::AudioConfiguration;
using ::aidl::android::hardware::bluetooth::audio::BluetoothAudioSessionControl;
using ::aidl::android::hardware::bluetooth::audio::ChannelMode;
using ::aidl::android::hardware::bluetooth::audio::PcmConfiguration;
using ::aidl::android::hardware::bluetooth::audio::PortStatusCallbacks;
using ::aidl::android::hardware::bluetooth::audio::PresentationPosition;
using ::aidl::android::media::audio::common::AudioContentType;
using ::aidl::android::media::audio::common::AudioSource;
using ::aidl::android::media::audio::common::AudioUsage;

using ::android::base::StringPrintf;
using ControlResultCallback = std::function<void(
    uint16_t cookie, bool start_resp, const BluetoothAudioStatus& status)>;
using SessionChangedCallback = std::function<void(uint16_t cookie)>;

namespace {

audio_channel_mask_t OutputChannelModeToAudioFormat(ChannelMode channel_mode) {
  switch (channel_mode) {
    case ChannelMode::MONO:
      return AUDIO_CHANNEL_OUT_MONO;
    case ChannelMode::STEREO:
      return AUDIO_CHANNEL_OUT_STEREO;
    default:
      return kBluetoothDefaultOutputChannelModeMask;
  }
}

audio_channel_mask_t InputChannelModeToAudioFormat(ChannelMode channel_mode) {
  switch (channel_mode) {
    case ChannelMode::MONO:
      return AUDIO_CHANNEL_IN_MONO;
    case ChannelMode::STEREO:
      return AUDIO_CHANNEL_IN_STEREO;
    default:
      return kBluetoothDefaultInputChannelModeMask;
  }
}

audio_format_t BitsPerSampleToAudioFormat(uint8_t bits_per_sample,
                                          const SessionType& session_type) {
  switch (bits_per_sample) {
    case 16:
      return AUDIO_FORMAT_PCM_16_BIT;
    case 24:
      /* Now we use knowledge that Classic sessions used packed, and LE Audio
       * LC3 encoder uses unpacked as input. This should be passed as parameter
       * from BT stack through AIDL, but it would require new interface version,
       * so sticking with this workaround for now. */
      if (session_type ==
              SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH ||
          session_type == SessionType::A2DP_SOFTWARE_ENCODING_DATAPATH) {
        return AUDIO_FORMAT_PCM_24_BIT_PACKED;
      } else {
        return AUDIO_FORMAT_PCM_8_24_BIT;
      }
    case 32:
      return AUDIO_FORMAT_PCM_32_BIT;
    default:
      return kBluetoothDefaultAudioFormatBitsPerSample;
  }
}

std::vector<std::string> CovertAudioTagFromV7(char* tags_v7) {
  std::vector<std::string> tags;
  char tags_copy[AUDIO_ATTRIBUTES_TAGS_MAX_SIZE];
  strlcpy(tags_copy, tags_v7, AUDIO_ATTRIBUTES_TAGS_MAX_SIZE);
  char* tag = strtok(tags_copy, ";");

  while (tag != NULL) {
    tags.push_back(tag);
    tag = strtok(NULL, ";");
  }

  return tags;
}

// The maximum time to wait in std::condition_variable::wait_for()
constexpr unsigned int kMaxWaitingTimeMs = 4500;

}  // namespace

BluetoothAudioPortAidl::BluetoothAudioPortAidl()
    : cookie_(::aidl::android::hardware::bluetooth::audio::
                  kObserversCookieUndefined),
      state_(BluetoothStreamState::DISABLED),
      session_type_(SessionType::UNKNOWN) {}

BluetoothAudioPortAidlOut::~BluetoothAudioPortAidlOut() {
  if (in_use()) TearDown();
}

BluetoothAudioPortAidlIn::~BluetoothAudioPortAidlIn() {
  if (in_use()) TearDown();
}

bool BluetoothAudioPortAidl::SetUp(audio_devices_t devices) {
  if (!init_session_type(devices)) return false;

  state_ = BluetoothStreamState::STANDBY;

  auto control_result_cb = [port = this](uint16_t cookie, bool start_resp,
                                         const BluetoothAudioStatus& status) {
    if (!port->in_use()) {
      LOG(ERROR) << "control_result_cb: BluetoothAudioPortAidl is not in use";
      return;
    }
    if (port->cookie_ != cookie) {
      LOG(ERROR) << "control_result_cb: proxy of device port (cookie="
                 << StringPrintf("%#hx", cookie) << ") is corrupted";
      return;
    }
    port->ControlResultHandler(status);
  };
  auto session_changed_cb = [port = this](uint16_t cookie) {
    if (!port->in_use()) {
      LOG(ERROR) << "session_changed_cb: BluetoothAudioPortAidl is not in use";
      return;
    }
    if (port->cookie_ != cookie) {
      LOG(ERROR) << "session_changed_cb: proxy of device port (cookie="
                 << StringPrintf("%#hx", cookie) << ") is corrupted";
      return;
    }
    port->SessionChangedHandler();
  };
  // TODO: Add audio_config_changed_cb
  PortStatusCallbacks cbacks = {
      .control_result_cb_ = control_result_cb,
      .session_changed_cb_ = session_changed_cb,
  };
  cookie_ = BluetoothAudioSessionControl::RegisterControlResultCback(
      session_type_, cbacks);
  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_)
            << ", cookie=" << StringPrintf("%#hx", cookie_);

  return (
      cookie_ !=
      ::aidl::android::hardware::bluetooth::audio::kObserversCookieUndefined);
}

bool BluetoothAudioPortAidl::init_session_type(audio_devices_t device) {
  switch (device) {
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP:
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES:
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER:
      LOG(VERBOSE)
          << __func__
          << ": device=AUDIO_DEVICE_OUT_BLUETOOTH_A2DP (HEADPHONES/SPEAKER) ("
          << StringPrintf("%#x", device) << ")";
      session_type_ = SessionType::A2DP_SOFTWARE_ENCODING_DATAPATH;
      break;
    case AUDIO_DEVICE_OUT_HEARING_AID:
      LOG(VERBOSE) << __func__
                   << ": device=AUDIO_DEVICE_OUT_HEARING_AID (MEDIA/VOICE) ("
                   << StringPrintf("%#x", device) << ")";
      session_type_ = SessionType::HEARING_AID_SOFTWARE_ENCODING_DATAPATH;
      break;
    case AUDIO_DEVICE_OUT_BLE_HEADSET:
      LOG(VERBOSE) << __func__
                   << ": device=AUDIO_DEVICE_OUT_BLE_HEADSET (MEDIA/VOICE) ("
                   << StringPrintf("%#x", device) << ")";
      session_type_ = SessionType::LE_AUDIO_SOFTWARE_ENCODING_DATAPATH;
      break;
    case AUDIO_DEVICE_OUT_BLE_SPEAKER:
      LOG(VERBOSE) << __func__
                   << ": device=AUDIO_DEVICE_OUT_BLE_SPEAKER (MEDIA) ("
                   << StringPrintf("%#x", device) << ")";
      session_type_ = SessionType::LE_AUDIO_SOFTWARE_ENCODING_DATAPATH;
      break;
    case AUDIO_DEVICE_IN_BLE_HEADSET:
      LOG(VERBOSE) << __func__
                   << ": device=AUDIO_DEVICE_IN_BLE_HEADSET (VOICE) ("
                   << StringPrintf("%#x", device) << ")";
      session_type_ = SessionType::LE_AUDIO_SOFTWARE_DECODING_DATAPATH;
      break;
    case AUDIO_DEVICE_OUT_BLE_BROADCAST:
      LOG(VERBOSE) << __func__
                   << ": device=AUDIO_DEVICE_OUT_BLE_BROADCAST (MEDIA) ("
                   << StringPrintf("%#x", device) << ")";
      session_type_ =
          SessionType::LE_AUDIO_BROADCAST_SOFTWARE_ENCODING_DATAPATH;
      break;
    default:
      LOG(ERROR) << __func__
                 << ": unknown device=" << StringPrintf("%#x", device);
      return false;
  }

  if (!BluetoothAudioSessionControl::IsSessionReady(session_type_)) {
    LOG(ERROR) << __func__ << ": device=" << StringPrintf("%#x", device)
               << ", session_type=" << toString(session_type_)
               << " is not ready";
    return false;
  }
  return true;
}

void BluetoothAudioPortAidl::TearDown() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": session_type=" << toString(session_type_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << " unknown monitor";
    return;
  }

  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_)
            << ", cookie=" << StringPrintf("%#hx", cookie_);
  BluetoothAudioSessionControl::UnregisterControlResultCback(session_type_,
                                                             cookie_);
  cookie_ =
      ::aidl::android::hardware::bluetooth::audio::kObserversCookieUndefined;
}

void BluetoothAudioPortAidl::ControlResultHandler(
    const BluetoothAudioStatus& status) {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidlis not in use";
    return;
  }
  std::unique_lock<std::mutex> port_lock(cv_mutex_);
  BluetoothStreamState previous_state = state_;
  LOG(INFO) << "control_result_cb: session_type=" << toString(session_type_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", previous_state=" << previous_state
            << ", status=" << toString(status);

  switch (previous_state) {
    case BluetoothStreamState::STARTED:
      /* Only Suspend signal can be send in STARTED state*/
      if (status == BluetoothAudioStatus::RECONFIGURATION ||
          status == BluetoothAudioStatus::SUCCESS) {
        state_ = BluetoothStreamState::STANDBY;
      } else {
        // Set to standby since the stack may be busy switching between outputs
        LOG(WARNING) << "control_result_cb: status=" << toString(status)
                     << " failure for session_type=" << toString(session_type_)
                     << ", cookie=" << StringPrintf("%#hx", cookie_)
                     << ", previous_state=" << previous_state;
      }
      break;
    case BluetoothStreamState::STARTING:
      if (status == BluetoothAudioStatus::SUCCESS) {
        state_ = BluetoothStreamState::STARTED;
      } else {
        // Set to standby since the stack may be busy switching between outputs
        LOG(WARNING) << "control_result_cb: status=" << toString(status)
                     << " failure for session_type=" << toString(session_type_)
                     << ", cookie=" << StringPrintf("%#hx", cookie_)
                     << ", previous_state=" << previous_state;
        state_ = BluetoothStreamState::STANDBY;
      }
      break;
    case BluetoothStreamState::SUSPENDING:
      if (status == BluetoothAudioStatus::SUCCESS) {
        state_ = BluetoothStreamState::STANDBY;
      } else {
        // It will be failed if the headset is disconnecting, and set to disable
        // to wait for re-init again
        LOG(WARNING) << "control_result_cb: status=" << toString(status)
                     << " failure for session_type=" << toString(session_type_)
                     << ", cookie=" << StringPrintf("%#hx", cookie_)
                     << ", previous_state=" << previous_state;
        state_ = BluetoothStreamState::DISABLED;
      }
      break;
    default:
      LOG(ERROR) << "control_result_cb: unexpected status=" << toString(status)
                 << " for session_type=" << toString(session_type_)
                 << ", cookie=" << StringPrintf("%#hx", cookie_)
                 << ", previous_state=" << previous_state;
      return;
  }
  port_lock.unlock();
  internal_cv_.notify_all();
}

void BluetoothAudioPortAidl::SessionChangedHandler() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidl is not in use";
    return;
  }
  std::unique_lock<std::mutex> port_lock(cv_mutex_);
  BluetoothStreamState previous_state = state_;
  LOG(INFO) << "session_changed_cb: session_type=" << toString(session_type_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", previous_state=" << previous_state;
  state_ = BluetoothStreamState::DISABLED;
  port_lock.unlock();
  internal_cv_.notify_all();
}

bool BluetoothAudioPortAidl::in_use() const {
  return (
      cookie_ !=
      ::aidl::android::hardware::bluetooth::audio::kObserversCookieUndefined);
}

bool BluetoothAudioPortAidl::GetPreferredDataIntervalUs(
    size_t* interval_us) const {
  if (!in_use()) {
    return false;
  }

  const AudioConfiguration& hal_audio_cfg =
      BluetoothAudioSessionControl::GetAudioConfig(session_type_);
  if (hal_audio_cfg.getTag() != AudioConfiguration::pcmConfig) {
    return false;
  }

  const PcmConfiguration& pcm_cfg =
      hal_audio_cfg.get<AudioConfiguration::pcmConfig>();
  *interval_us = pcm_cfg.dataIntervalUs;
  return true;
}

bool BluetoothAudioPortAidlOut::LoadAudioConfig(
    audio_config_t* audio_cfg) const {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidlOut is not in use";
    audio_cfg->sample_rate = kBluetoothDefaultSampleRate;
    audio_cfg->channel_mask = kBluetoothDefaultOutputChannelModeMask;
    audio_cfg->format = kBluetoothDefaultAudioFormatBitsPerSample;
    return false;
  }

  const AudioConfiguration& hal_audio_cfg =
      BluetoothAudioSessionControl::GetAudioConfig(session_type_);
  if (hal_audio_cfg.getTag() != AudioConfiguration::pcmConfig) {
    audio_cfg->sample_rate = kBluetoothDefaultSampleRate;
    audio_cfg->channel_mask = kBluetoothDefaultOutputChannelModeMask;
    audio_cfg->format = kBluetoothDefaultAudioFormatBitsPerSample;
    return false;
  }
  const PcmConfiguration& pcm_cfg =
      hal_audio_cfg.get<AudioConfiguration::pcmConfig>();
  LOG(VERBOSE) << __func__ << ": session_type=" << toString(session_type_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << ", state=" << state_ << ", PcmConfig=[" << pcm_cfg.toString()
               << "]";
  if (pcm_cfg.channelMode == ChannelMode::UNKNOWN) {
    return false;
  }
  audio_cfg->sample_rate = pcm_cfg.sampleRateHz;
  audio_cfg->channel_mask =
      (is_stereo_to_mono_
           ? AUDIO_CHANNEL_OUT_STEREO
           : OutputChannelModeToAudioFormat(pcm_cfg.channelMode));
  audio_cfg->format =
      BitsPerSampleToAudioFormat(pcm_cfg.bitsPerSample, session_type_);
  return true;
}

bool BluetoothAudioPortAidlIn::LoadAudioConfig(
    audio_config_t* audio_cfg) const {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidlIn is not in use";
    audio_cfg->sample_rate = kBluetoothDefaultSampleRate;
    audio_cfg->channel_mask = kBluetoothDefaultInputChannelModeMask;
    audio_cfg->format = kBluetoothDefaultAudioFormatBitsPerSample;
    return false;
  }

  const AudioConfiguration& hal_audio_cfg =
      BluetoothAudioSessionControl::GetAudioConfig(session_type_);
  if (hal_audio_cfg.getTag() != AudioConfiguration::pcmConfig) {
    audio_cfg->sample_rate = kBluetoothDefaultSampleRate;
    audio_cfg->channel_mask = kBluetoothDefaultInputChannelModeMask;
    audio_cfg->format = kBluetoothDefaultAudioFormatBitsPerSample;
    return false;
  }
  const PcmConfiguration& pcm_cfg =
      hal_audio_cfg.get<AudioConfiguration::pcmConfig>();
  LOG(VERBOSE) << __func__ << ": session_type=" << toString(session_type_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << ", state=" << state_ << ", PcmConfig=[" << pcm_cfg.toString()
               << "]";
  if (pcm_cfg.channelMode == ChannelMode::UNKNOWN) {
    return false;
  }

  audio_cfg->sample_rate = pcm_cfg.sampleRateHz;
  audio_cfg->channel_mask = InputChannelModeToAudioFormat(pcm_cfg.channelMode);
  audio_cfg->format =
      BitsPerSampleToAudioFormat(pcm_cfg.bitsPerSample, session_type_);
  return true;
}

bool BluetoothAudioPortAidl::CondwaitState(BluetoothStreamState state) {
  bool retval;
  std::unique_lock<std::mutex> port_lock(cv_mutex_);
  switch (state) {
    case BluetoothStreamState::STARTING:
      LOG(VERBOSE) << __func__ << ": session_type=" << toString(session_type_)
                   << ", cookie=" << StringPrintf("%#hx", cookie_)
                   << " waiting for STARTED";
      retval = internal_cv_.wait_for(
          port_lock, std::chrono::milliseconds(kMaxWaitingTimeMs),
          [this] { return this->state_ != BluetoothStreamState::STARTING; });
      retval = retval && state_ == BluetoothStreamState::STARTED;
      break;
    case BluetoothStreamState::SUSPENDING:
      LOG(VERBOSE) << __func__ << ": session_type=" << toString(session_type_)
                   << ", cookie=" << StringPrintf("%#hx", cookie_)
                   << " waiting for SUSPENDED";
      retval = internal_cv_.wait_for(
          port_lock, std::chrono::milliseconds(kMaxWaitingTimeMs),
          [this] { return this->state_ != BluetoothStreamState::SUSPENDING; });
      retval = retval && state_ == BluetoothStreamState::STANDBY;
      break;
    default:
      LOG(WARNING) << __func__ << ": session_type=" << toString(session_type_)
                   << ", cookie=" << StringPrintf("%#hx", cookie_)
                   << " waiting for KNOWN";
      return false;
  }

  return retval;  // false if any failure like timeout
}

bool BluetoothAudioPortAidl::Start() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidl is not in use";
    return false;
  }

  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", state=" << state_
            << ", mono=" << (is_stereo_to_mono_ ? "true" : "false")
            << " request";
  bool retval = false;
  if (state_ == BluetoothStreamState::STANDBY) {
    state_ = BluetoothStreamState::STARTING;
    if (BluetoothAudioSessionControl::StartStream(session_type_)) {
      retval = CondwaitState(BluetoothStreamState::STARTING);
    } else {
      LOG(ERROR) << __func__ << ": session_type=" << toString(session_type_)
                 << ", cookie=" << StringPrintf("%#hx", cookie_)
                 << ", state=" << state_ << " Hal fails";
    }
  }

  if (retval) {
    LOG(INFO) << __func__ << ": session_type=" << toString(session_type_)
              << ", cookie=" << StringPrintf("%#hx", cookie_)
              << ", state=" << state_
              << ", mono=" << (is_stereo_to_mono_ ? "true" : "false")
              << " done";
  } else {
    LOG(ERROR) << __func__ << ": session_type=" << toString(session_type_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << ", state=" << state_ << " failure";
  }

  return retval;  // false if any failure like timeout
}

bool BluetoothAudioPortAidl::Suspend() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidl is not in use";
    return false;
  }

  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", state=" << state_ << " request";
  bool retval = false;
  if (state_ == BluetoothStreamState::STARTED) {
    state_ = BluetoothStreamState::SUSPENDING;
    if (BluetoothAudioSessionControl::SuspendStream(session_type_)) {
      retval = CondwaitState(BluetoothStreamState::SUSPENDING);
    } else {
      LOG(ERROR) << __func__ << ": session_type=" << toString(session_type_)
                 << ", cookie=" << StringPrintf("%#hx", cookie_)
                 << ", state=" << state_ << " Hal fails";
    }
  }

  if (retval) {
    LOG(INFO) << __func__ << ": session_type=" << toString(session_type_)
              << ", cookie=" << StringPrintf("%#hx", cookie_)
              << ", state=" << state_ << " done";
  } else {
    LOG(ERROR) << __func__ << ": session_type=" << toString(session_type_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << ", state=" << state_ << " failure";
  }

  return retval;  // false if any failure like timeout
}

void BluetoothAudioPortAidl::Stop() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidl is not in use";
    return;
  }
  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", state=" << state_ << " request";
  state_ = BluetoothStreamState::DISABLED;
  BluetoothAudioSessionControl::StopStream(session_type_);
  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", state=" << state_ << " done";
}

size_t BluetoothAudioPortAidlOut::WriteData(const void* buffer,
                                            size_t bytes) const {
  if (!in_use()) return 0;
  if (!is_stereo_to_mono_) {
    return BluetoothAudioSessionControl::OutWritePcmData(session_type_, buffer,
                                                         bytes);
  }

  // WAR to mix the stereo into Mono (16 bits per sample)
  const size_t write_frames = bytes >> 2;
  if (write_frames == 0) return 0;
  auto src = static_cast<const int16_t*>(buffer);
  std::unique_ptr<int16_t[]> dst{new int16_t[write_frames]};
  downmix_to_mono_i16_from_stereo_i16(dst.get(), src, write_frames);
  // a frame is 16 bits, and the size of a mono frame is equal to half a stereo.
  return BluetoothAudioSessionControl::OutWritePcmData(session_type_, dst.get(),
                                                       write_frames * 2) *
         2;
}

size_t BluetoothAudioPortAidlIn::ReadData(void* buffer, size_t bytes) const {
  if (!in_use()) return 0;
  return BluetoothAudioSessionControl::InReadPcmData(session_type_, buffer,
                                                     bytes);
}

bool BluetoothAudioPortAidl::GetPresentationPosition(
    uint64_t* delay_ns, uint64_t* bytes, timespec* timestamp) const {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidl is not in use";
    return false;
  }
  PresentationPosition presentation_position;
  bool retval = BluetoothAudioSessionControl::GetPresentationPosition(
      session_type_, presentation_position);
  *delay_ns = presentation_position.remoteDeviceAudioDelayNanos;
  *bytes = presentation_position.transmittedOctets;
  *timestamp = {.tv_sec = static_cast<__kernel_old_time_t>(
                    presentation_position.transmittedOctetsTimestamp.tvSec),
                .tv_nsec = static_cast<long>(
                    presentation_position.transmittedOctetsTimestamp.tvNSec)};
  LOG(VERBOSE) << __func__
               << ": session_type=" << StringPrintf("%#hhx", session_type_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << ", state=" << state_ << ", delay=" << *delay_ns
               << "ns, data=" << *bytes
               << " bytes, timestamp=" << timestamp->tv_sec << "."
               << StringPrintf("%09ld", timestamp->tv_nsec) << "s";

  return retval;
}

void BluetoothAudioPortAidl::UpdateSourceMetadata(
    const source_metadata_v7* source_metadata) const {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidl is not in use";
    return;
  }
  LOG(DEBUG) << __func__ << ": session_type=" << toString(session_type_)
             << ", cookie=" << StringPrintf("%#hx", cookie_)
             << ", state=" << state_ << ", " << source_metadata->track_count
             << " track(s)";
  ssize_t track_count = source_metadata->track_count;
  if (track_count == 0) return;
  SourceMetadata hal_source_metadata;
  hal_source_metadata.tracks.resize(track_count);
  for (int i = 0; i < track_count; i++) {
    hal_source_metadata.tracks[i].usage =
        static_cast<AudioUsage>(source_metadata->tracks[i].base.usage);
    hal_source_metadata.tracks[i].contentType = static_cast<AudioContentType>(
        source_metadata->tracks[i].base.content_type);
    hal_source_metadata.tracks[i].tags =
        std::move(CovertAudioTagFromV7(source_metadata->tracks[i].tags));
  }

  BluetoothAudioSessionControl::UpdateSourceMetadata(session_type_,
                                                     hal_source_metadata);
}

void BluetoothAudioPortAidl::UpdateSinkMetadata(
    const sink_metadata_v7* sink_metadata) const {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortAidl is not in use";
    return;
  }
  LOG(DEBUG) << __func__ << ": session_type=" << toString(session_type_)
             << ", cookie=" << StringPrintf("%#hx", cookie_)
             << ", state=" << state_ << ", " << sink_metadata->track_count
             << " track(s)";
  ssize_t track_count = sink_metadata->track_count;
  if (track_count == 0) return;
  SinkMetadata hal_sink_metadata;
  hal_sink_metadata.tracks.resize(track_count);
  for (int i = 0; i < track_count; i++) {
    hal_sink_metadata.tracks[i].source =
        static_cast<AudioSource>(sink_metadata->tracks[i].base.source);
    hal_sink_metadata.tracks[i].gain = sink_metadata->tracks[i].base.gain;
    hal_sink_metadata.tracks[i].tags =
        std::move(CovertAudioTagFromV7(sink_metadata->tracks[i].tags));
  }

  BluetoothAudioSessionControl::UpdateSinkMetadata(session_type_,
                                                   hal_sink_metadata);
}

BluetoothStreamState BluetoothAudioPortAidl::GetState() const { return state_; }

void BluetoothAudioPortAidl::SetState(BluetoothStreamState state) {
  state_ = state;
}

}  // namespace aidl
}  // namespace audio
}  // namespace bluetooth
}  // namespace android