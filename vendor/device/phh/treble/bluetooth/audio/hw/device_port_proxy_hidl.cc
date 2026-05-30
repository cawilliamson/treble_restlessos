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

#define LOG_TAG "BTAudioHalDeviceProxyHIDL"

#include "device_port_proxy_hidl.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <audio_utils/primitives.h>
#include <inttypes.h>
#include <log/log.h>
#include <stdlib.h>

#include "BluetoothAudioSessionControl_2_1.h"
#include "stream_apis.h"
#include "utils.h"

namespace android {
namespace bluetooth {
namespace audio {
namespace hidl {

using ::android::base::StringPrintf;
using ::android::bluetooth::audio::BluetoothAudioSessionControl_2_1;
using ::android::hardware::bluetooth::audio::V2_0::BitsPerSample;
using ::android::hardware::bluetooth::audio::V2_0::ChannelMode;
using ::android::hardware::bluetooth::audio::V2_0::PcmParameters;
using SampleRate = ::android::hardware::bluetooth::audio::V2_0::SampleRate;
using SampleRate_2_1 = ::android::hardware::bluetooth::audio::V2_1::SampleRate;
using BluetoothAudioStatusHidl =
    ::android::hardware::bluetooth::audio::V2_0::Status;
using ControlResultCallback = std::function<void(
    uint16_t cookie, bool start_resp, const BluetoothAudioStatusHidl& status)>;
using SessionChangedCallback = std::function<void(uint16_t cookie)>;

namespace {

unsigned int SampleRateToAudioFormat(SampleRate_2_1 sample_rate) {
  switch (sample_rate) {
    case SampleRate_2_1::RATE_8000:
      return 8000;
    case SampleRate_2_1::RATE_16000:
      return 16000;
    case SampleRate_2_1::RATE_24000:
      return 24000;
    case SampleRate_2_1::RATE_32000:
      return 32000;
    case SampleRate_2_1::RATE_44100:
      return 44100;
    case SampleRate_2_1::RATE_48000:
      return 48000;
    case SampleRate_2_1::RATE_88200:
      return 88200;
    case SampleRate_2_1::RATE_96000:
      return 96000;
    case SampleRate_2_1::RATE_176400:
      return 176400;
    case SampleRate_2_1::RATE_192000:
      return 192000;
    default:
      return kBluetoothDefaultSampleRate;
  }
}
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

audio_format_t BitsPerSampleToAudioFormat(BitsPerSample bits_per_sample) {
  switch (bits_per_sample) {
    case BitsPerSample::BITS_16:
      return AUDIO_FORMAT_PCM_16_BIT;
    case BitsPerSample::BITS_24:
      return AUDIO_FORMAT_PCM_24_BIT_PACKED;
    case BitsPerSample::BITS_32:
      return AUDIO_FORMAT_PCM_32_BIT;
    default:
      return kBluetoothDefaultAudioFormatBitsPerSample;
  }
}

// The maximum time to wait in std::condition_variable::wait_for()
constexpr unsigned int kMaxWaitingTimeMs = 4500;

}  // namespace

BluetoothAudioPortHidl::BluetoothAudioPortHidl()
    : session_type_hidl_(SessionType_2_1::UNKNOWN),
      cookie_(android::bluetooth::audio::kObserversCookieUndefined),
      state_(BluetoothStreamState::DISABLED) {}

BluetoothAudioPortHidlOut::~BluetoothAudioPortHidlOut() {
  if (BluetoothAudioPortHidl::in_use()) BluetoothAudioPortHidl::TearDown();
}

BluetoothAudioPortHidlIn::~BluetoothAudioPortHidlIn() {
  if (BluetoothAudioPortHidl::in_use()) BluetoothAudioPortHidl::TearDown();
}

bool BluetoothAudioPortHidl::SetUp(audio_devices_t devices) {
  if (!init_session_type(devices)) return false;

  state_ = BluetoothStreamState::STANDBY;

  auto control_result_cb = [port = this](
                               uint16_t cookie, bool start_resp,
                               const BluetoothAudioStatusHidl& status) {
    if (!port->in_use()) {
      LOG(ERROR) << "control_result_cb: BluetoothAudioPort is not in use";
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
      LOG(ERROR) << "session_changed_cb: BluetoothAudioPort is not in use";
      return;
    }
    if (port->cookie_ != cookie) {
      LOG(ERROR) << "session_changed_cb: proxy of device port (cookie="
                 << StringPrintf("%#hx", cookie) << ") is corrupted";
      return;
    }
    port->SessionChangedHandler();
  };
  ::android::bluetooth::audio::PortStatusCallbacks cbacks = {
      .control_result_cb_ = control_result_cb,
      .session_changed_cb_ = session_changed_cb};
  cookie_ = BluetoothAudioSessionControl_2_1::RegisterControlResultCback(
      session_type_hidl_, cbacks);
  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_hidl_)
            << ", cookie=" << StringPrintf("%#hx", cookie_);

  return (cookie_ != android::bluetooth::audio::kObserversCookieUndefined);
}

bool BluetoothAudioPortHidl::init_session_type(audio_devices_t device) {
  switch (device) {
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP:
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES:
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER:
      LOG(VERBOSE)
          << __func__
          << ": device=AUDIO_DEVICE_OUT_BLUETOOTH_A2DP (HEADPHONES/SPEAKER) ("
          << StringPrintf("%#x", device) << ")";
      session_type_hidl_ = SessionType_2_1::A2DP_SOFTWARE_ENCODING_DATAPATH;
      break;
    case AUDIO_DEVICE_OUT_HEARING_AID:
      LOG(VERBOSE) << __func__
                   << ": device=AUDIO_DEVICE_OUT_HEARING_AID (MEDIA/VOICE) ("
                   << StringPrintf("%#x", device) << ")";
      session_type_hidl_ =
          SessionType_2_1::HEARING_AID_SOFTWARE_ENCODING_DATAPATH;
      break;
    case AUDIO_DEVICE_OUT_BLE_HEADSET:
      LOG(VERBOSE) << __func__
                   << ": device=AUDIO_DEVICE_OUT_BLE_HEADSET (MEDIA/VOICE) ("
                   << StringPrintf("%#x", device) << ")";
      session_type_hidl_ = SessionType_2_1::LE_AUDIO_SOFTWARE_ENCODING_DATAPATH;
      break;
    case AUDIO_DEVICE_OUT_BLE_SPEAKER:
      LOG(VERBOSE) << __func__
                   << ": device=AUDIO_DEVICE_OUT_BLE_SPEAKER (MEDIA) ("
                   << StringPrintf("%#x", device) << ")";
      session_type_hidl_ = SessionType_2_1::LE_AUDIO_SOFTWARE_ENCODING_DATAPATH;
      break;
    case AUDIO_DEVICE_IN_BLE_HEADSET:
      LOG(VERBOSE) << __func__
                   << ": device=AUDIO_DEVICE_IN_BLE_HEADSET (VOICE) ("
                   << StringPrintf("%#x", device) << ")";
      session_type_hidl_ = SessionType_2_1::LE_AUDIO_SOFTWARE_DECODED_DATAPATH;
      break;
    default:
      LOG(ERROR) << __func__
                 << ": unknown device=" << StringPrintf("%#x", device);
      return false;
  }

  if (!BluetoothAudioSessionControl_2_1::IsSessionReady(session_type_hidl_)) {
    LOG(ERROR) << __func__ << ": device=" << StringPrintf("%#x", device)
               << ", session_type=" << toString(session_type_hidl_)
               << " is not ready";
    return false;
  }
  return true;
}

void BluetoothAudioPortHidl::TearDown() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": session_type=" << toString(session_type_hidl_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << " unknown monitor";
    return;
  }

  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_hidl_)
            << ", cookie=" << StringPrintf("%#hx", cookie_);
  BluetoothAudioSessionControl_2_1::UnregisterControlResultCback(
      session_type_hidl_, cookie_);
  cookie_ = android::bluetooth::audio::kObserversCookieUndefined;
}

void BluetoothAudioPortHidl::ControlResultHandler(
    const BluetoothAudioStatusHidl& status) {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortis not in use";
    return;
  }
  std::unique_lock<std::mutex> port_lock(cv_mutex_);
  BluetoothStreamState previous_state = state_;
  LOG(INFO) << "control_result_cb: session_type="
            << toString(session_type_hidl_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", previous_state=" << previous_state
            << ", status=" << toString(status);

  switch (previous_state) {
    case BluetoothStreamState::STARTED:
      /* Only Suspend signal can be send in STARTED state*/
      if (status == BluetoothAudioStatus::SUCCESS) {
        state_ = BluetoothStreamState::STANDBY;
      } else {
        // Set to standby since the stack may be busy switching between outputs
        LOG(WARNING) << "control_result_cb: status=" << toString(status)
                     << " failure for session_type="
                     << toString(session_type_hidl_)
                     << ", cookie=" << StringPrintf("%#hx", cookie_)
                     << ", previous_state=" << previous_state;
      }
      break;
    case BluetoothStreamState::STARTING:
      if (status == BluetoothAudioStatusHidl::SUCCESS) {
        state_ = BluetoothStreamState::STARTED;
      } else {
        // Set to standby since the stack may be busy switching between outputs
        LOG(WARNING) << "control_result_cb: status=" << toString(status)
                     << " failure for session_type="
                     << toString(session_type_hidl_)
                     << ", cookie=" << StringPrintf("%#hx", cookie_)
                     << ", previous_state=" << previous_state;
        state_ = BluetoothStreamState::STANDBY;
      }
      break;
    case BluetoothStreamState::SUSPENDING:
      if (status == BluetoothAudioStatusHidl::SUCCESS) {
        state_ = BluetoothStreamState::STANDBY;
      } else {
        // It will be failed if the headset is disconnecting, and set to disable
        // to wait for re-init again
        LOG(WARNING) << "control_result_cb: status=" << toString(status)
                     << " failure for session_type="
                     << toString(session_type_hidl_)
                     << ", cookie=" << StringPrintf("%#hx", cookie_)
                     << ", previous_state=" << previous_state;
        state_ = BluetoothStreamState::DISABLED;
      }
      break;
    default:
      LOG(ERROR) << "control_result_cb: unexpected status=" << toString(status)
                 << " for session_type=" << toString(session_type_hidl_)
                 << ", cookie=" << StringPrintf("%#hx", cookie_)
                 << ", previous_state=" << previous_state;
      return;
  }
  port_lock.unlock();
  internal_cv_.notify_all();
}

void BluetoothAudioPortHidl::SessionChangedHandler() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPort is not in use";
    return;
  }
  std::unique_lock<std::mutex> port_lock(cv_mutex_);
  BluetoothStreamState previous_state = state_;
  LOG(INFO) << "session_changed_cb: session_type="
            << toString(session_type_hidl_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", previous_state=" << previous_state;
  state_ = BluetoothStreamState::DISABLED;
  port_lock.unlock();
  internal_cv_.notify_all();
}

bool BluetoothAudioPortHidl::in_use() const {
  return (cookie_ != android::bluetooth::audio::kObserversCookieUndefined);
}

bool BluetoothAudioPortHidl::GetPreferredDataIntervalUs(
    size_t* interval_us) const {
  if (!in_use()) {
    return false;
  }

  const ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration&
      hal_audio_cfg =
          BluetoothAudioSessionControl_2_1::GetAudioConfig(session_type_hidl_);
  if (hal_audio_cfg.getDiscriminator() !=
      ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration::
          hidl_discriminator::pcmConfig) {
    return false;
  }

  const ::android::hardware::bluetooth::audio::V2_1::PcmParameters& pcm_cfg =
      hal_audio_cfg.pcmConfig();
  *interval_us = pcm_cfg.dataIntervalUs;
  return true;
}

bool BluetoothAudioPortHidl::CondwaitState(BluetoothStreamState state) {
  bool retval;
  std::unique_lock<std::mutex> port_lock(cv_mutex_);
  switch (state) {
    case BluetoothStreamState::STARTING:
      LOG(VERBOSE) << __func__
                   << ": session_type=" << toString(session_type_hidl_)
                   << ", cookie=" << StringPrintf("%#hx", cookie_)
                   << " waiting for STARTED";
      retval = internal_cv_.wait_for(
          port_lock, std::chrono::milliseconds(kMaxWaitingTimeMs),
          [this] { return this->state_ != BluetoothStreamState::STARTING; });
      retval = retval && state_ == BluetoothStreamState::STARTED;
      break;
    case BluetoothStreamState::SUSPENDING:
      LOG(VERBOSE) << __func__
                   << ": session_type=" << toString(session_type_hidl_)
                   << ", cookie=" << StringPrintf("%#hx", cookie_)
                   << " waiting for SUSPENDED";
      retval = internal_cv_.wait_for(
          port_lock, std::chrono::milliseconds(kMaxWaitingTimeMs),
          [this] { return this->state_ != BluetoothStreamState::SUSPENDING; });
      retval = retval && state_ == BluetoothStreamState::STANDBY;
      break;
    default:
      LOG(WARNING) << __func__
                   << ": session_type=" << toString(session_type_hidl_)
                   << ", cookie=" << StringPrintf("%#hx", cookie_)
                   << " waiting for KNOWN";
      return false;
  }

  return retval;  // false if any failure like timeout
}

bool BluetoothAudioPortHidl::Start() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPort is not in use";
    return false;
  }

  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_hidl_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", state=" << state_
            << ", mono=" << (is_stereo_to_mono_ ? "true" : "false")
            << " request";
  bool retval = false;
  if (state_ == BluetoothStreamState::STANDBY) {
    state_ = BluetoothStreamState::STARTING;
    if (BluetoothAudioSessionControl_2_1::StartStream(session_type_hidl_)) {
      retval = CondwaitState(BluetoothStreamState::STARTING);
    } else {
      LOG(ERROR) << __func__
                 << ": session_type=" << toString(session_type_hidl_)
                 << ", cookie=" << StringPrintf("%#hx", cookie_)
                 << ", state=" << state_ << " Hal fails";
    }
  }

  if (retval) {
    LOG(INFO) << __func__ << ": session_type=" << toString(session_type_hidl_)
              << ", cookie=" << StringPrintf("%#hx", cookie_)
              << ", state=" << state_
              << ", mono=" << (is_stereo_to_mono_ ? "true" : "false")
              << " done";
  } else {
    LOG(ERROR) << __func__ << ": session_type=" << toString(session_type_hidl_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << ", state=" << state_ << " failure";
  }

  return retval;  // false if any failure like timeout
}

bool BluetoothAudioPortHidl::Suspend() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPort is not in use";
    return false;
  }

  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_hidl_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", state=" << state_ << " request";
  bool retval = false;
  if (state_ == BluetoothStreamState::STARTED) {
    state_ = BluetoothStreamState::SUSPENDING;
    if (BluetoothAudioSessionControl_2_1::SuspendStream(session_type_hidl_)) {
      retval = CondwaitState(BluetoothStreamState::SUSPENDING);
    } else {
      LOG(ERROR) << __func__
                 << ": session_type=" << toString(session_type_hidl_)
                 << ", cookie=" << StringPrintf("%#hx", cookie_)
                 << ", state=" << state_ << " Hal fails";
    }
  }

  if (retval) {
    LOG(INFO) << __func__ << ": session_type=" << toString(session_type_hidl_)
              << ", cookie=" << StringPrintf("%#hx", cookie_)
              << ", state=" << state_ << " done";
  } else {
    LOG(ERROR) << __func__ << ": session_type=" << toString(session_type_hidl_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << ", state=" << state_ << " failure";
  }

  return retval;  // false if any failure like timeout
}

void BluetoothAudioPortHidl::Stop() {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPort is not in use";
    return;
  }
  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_hidl_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", state=" << state_ << " request";
  state_ = BluetoothStreamState::DISABLED;
  BluetoothAudioSessionControl_2_1::StopStream(session_type_hidl_);
  LOG(INFO) << __func__ << ": session_type=" << toString(session_type_hidl_)
            << ", cookie=" << StringPrintf("%#hx", cookie_)
            << ", state=" << state_ << " done";
}

bool BluetoothAudioPortHidl::GetPresentationPosition(
    uint64_t* delay_ns, uint64_t* bytes, timespec* timestamp) const {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPort is not in use";
    return false;
  }
  bool retval = BluetoothAudioSessionControl_2_1::GetPresentationPosition(
      session_type_hidl_, delay_ns, bytes, timestamp);
  LOG(VERBOSE) << __func__
               << ": session_type=" << StringPrintf("%#hhx", session_type_hidl_)
               << ", cookie=" << StringPrintf("%#hx", cookie_)
               << ", state=" << state_ << ", delay=" << *delay_ns
               << "ns, data=" << *bytes
               << " bytes, timestamp=" << timestamp->tv_sec << "."
               << StringPrintf("%09ld", timestamp->tv_nsec) << "s";

  return retval;
}

void BluetoothAudioPortHidl::UpdateTracksMetadata(
    const source_metadata* source_metadata) const {
  if (!in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPort is not in use";
    return;
  }
  LOG(DEBUG) << __func__ << ": session_type=" << toString(session_type_hidl_)
             << ", cookie=" << StringPrintf("%#hx", cookie_)
             << ", state=" << state_ << ", " << source_metadata->track_count
             << " track(s)";
  if (source_metadata->track_count == 0) return;
  BluetoothAudioSessionControl_2_1::UpdateTracksMetadata(session_type_hidl_,
                                                         source_metadata);
}

BluetoothStreamState BluetoothAudioPortHidl::GetState() const { return state_; }

void BluetoothAudioPortHidl::SetState(BluetoothStreamState state) {
  state_ = state;
}

size_t BluetoothAudioPortHidlOut::WriteData(const void* buffer,
                                            size_t bytes) const {
  if (!BluetoothAudioPortHidl::in_use()) return 0;
  if (!BluetoothAudioPortHidl::is_stereo_to_mono_) {
    return BluetoothAudioSessionControl_2_1::OutWritePcmData(session_type_hidl_,
                                                             buffer, bytes);
  }

  // WAR to mix the stereo into Mono (16 bits per sample)
  const size_t write_frames = bytes >> 2;
  if (write_frames == 0) return 0;
  auto src = static_cast<const int16_t*>(buffer);
  std::unique_ptr<int16_t[]> dst{new int16_t[write_frames]};
  downmix_to_mono_i16_from_stereo_i16(dst.get(), src, write_frames);
  // a frame is 16 bits, and the size of a mono frame is equal to half a stereo.
  return BluetoothAudioSessionControl_2_1::OutWritePcmData(
             session_type_hidl_, dst.get(), write_frames * 2) *
         2;
}

size_t BluetoothAudioPortHidlIn::ReadData(void* buffer, size_t bytes) const {
  if (!BluetoothAudioPortHidl::in_use()) return 0;
  return BluetoothAudioSessionControl_2_1::InReadPcmData(session_type_hidl_,
                                                         buffer, bytes);
}

bool BluetoothAudioPortHidlIn::LoadAudioConfig(
    audio_config_t* audio_cfg) const {
  if (!BluetoothAudioPortHidl::in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortIn is not in use";
    audio_cfg->sample_rate = kBluetoothDefaultSampleRate;
    audio_cfg->channel_mask = kBluetoothDefaultInputChannelModeMask;
    audio_cfg->format = kBluetoothDefaultAudioFormatBitsPerSample;
    return false;
  }

  const ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration&
      hal_audio_cfg =
          BluetoothAudioSessionControl_2_1::GetAudioConfig(session_type_hidl_);
  if (hal_audio_cfg.getDiscriminator() !=
      ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration::
          hidl_discriminator::pcmConfig) {
    audio_cfg->sample_rate = kBluetoothDefaultSampleRate;
    audio_cfg->channel_mask = kBluetoothDefaultInputChannelModeMask;
    audio_cfg->format = kBluetoothDefaultAudioFormatBitsPerSample;
    return false;
  }
  const ::android::hardware::bluetooth::audio::V2_1::PcmParameters& pcm_cfg =
      hal_audio_cfg.pcmConfig();
  LOG(VERBOSE) << __func__ << ": session_type=" << toString(session_type_hidl_)
               << ", cookie="
               << StringPrintf("%#hx", BluetoothAudioPortHidl::cookie_)
               << ", state=" << BluetoothAudioPortHidl::state_
               << ", PcmConfig=[" << toString(pcm_cfg) << "]";
  if (pcm_cfg.sampleRate == SampleRate_2_1::RATE_UNKNOWN ||
      pcm_cfg.channelMode == ChannelMode::UNKNOWN ||
      pcm_cfg.bitsPerSample == BitsPerSample::BITS_UNKNOWN) {
    return false;
  }

  audio_cfg->sample_rate = SampleRateToAudioFormat(pcm_cfg.sampleRate);
  audio_cfg->channel_mask = InputChannelModeToAudioFormat(pcm_cfg.channelMode);
  audio_cfg->format = BitsPerSampleToAudioFormat(pcm_cfg.bitsPerSample);
  return true;
}

bool BluetoothAudioPortHidlOut::LoadAudioConfig(
    audio_config_t* audio_cfg) const {
  if (!BluetoothAudioPortHidl::in_use()) {
    LOG(ERROR) << __func__ << ": BluetoothAudioPortOut is not in use";
    audio_cfg->sample_rate = kBluetoothDefaultSampleRate;
    audio_cfg->channel_mask = kBluetoothDefaultOutputChannelModeMask;
    audio_cfg->format = kBluetoothDefaultAudioFormatBitsPerSample;
    return false;
  }

  const ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration&
      hal_audio_cfg =
          BluetoothAudioSessionControl_2_1::GetAudioConfig(session_type_hidl_);
  if (hal_audio_cfg.getDiscriminator() !=
      ::android::hardware::bluetooth::audio::V2_1::AudioConfiguration::
          hidl_discriminator::pcmConfig) {
    audio_cfg->sample_rate = kBluetoothDefaultSampleRate;
    audio_cfg->channel_mask = kBluetoothDefaultOutputChannelModeMask;
    audio_cfg->format = kBluetoothDefaultAudioFormatBitsPerSample;
    return false;
  }
  const ::android::hardware::bluetooth::audio::V2_1::PcmParameters& pcm_cfg =
      hal_audio_cfg.pcmConfig();
  LOG(VERBOSE) << __func__ << ": session_type=" << toString(session_type_hidl_)
               << ", cookie="
               << StringPrintf("%#hx", BluetoothAudioPortHidl::cookie_)
               << ", state=" << BluetoothAudioPortHidl::state_
               << ", PcmConfig=[" << toString(pcm_cfg) << "]";
  if (pcm_cfg.sampleRate == SampleRate_2_1::RATE_UNKNOWN ||
      pcm_cfg.channelMode == ChannelMode::UNKNOWN ||
      pcm_cfg.bitsPerSample == BitsPerSample::BITS_UNKNOWN) {
    return false;
  }
  audio_cfg->sample_rate = SampleRateToAudioFormat(pcm_cfg.sampleRate);
  audio_cfg->channel_mask =
      (BluetoothAudioPortHidl::is_stereo_to_mono_
           ? AUDIO_CHANNEL_OUT_STEREO
           : OutputChannelModeToAudioFormat(pcm_cfg.channelMode));
  audio_cfg->format = BitsPerSampleToAudioFormat(pcm_cfg.bitsPerSample);
  return true;
}

}  // namespace hidl
}  // namespace audio
}  // namespace bluetooth
}  // namespace android