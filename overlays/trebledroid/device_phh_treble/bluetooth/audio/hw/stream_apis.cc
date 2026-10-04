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

#include "device_port_proxy.h"
#define LOG_TAG "BTAudioHalStream"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <cutils/properties.h>
#include <inttypes.h>
#include <log/log.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <memory>

#include "BluetoothAudioSession.h"
#include "stream_apis.h"
#include "utils.h"

using ::android::base::StringPrintf;
using ::android::bluetooth::audio::utils::FrameCount;
using ::android::bluetooth::audio::utils::GetAudioParamString;
using ::android::bluetooth::audio::utils::ParseAudioParams;

namespace {

constexpr unsigned int kMinimumDelayMs = 50;
constexpr unsigned int kMaximumDelayMs = 1000;
constexpr int kExtraAudioSyncMs = 200;

std::ostream& operator<<(std::ostream& os, const audio_config& config) {
  return os << "audio_config[sample_rate=" << config.sample_rate
            << ", channels=" << StringPrintf("%#x", config.channel_mask)
            << ", format=" << config.format << "]";
}

void out_calculate_feeding_delay_ms(const BluetoothStreamOut* out,
                                    uint32_t* latency_ms,
                                    uint64_t* frames = nullptr,
                                    struct timespec* timestamp = nullptr) {
  if (latency_ms == nullptr && frames == nullptr && timestamp == nullptr) {
    return;
  }

  // delay_report is the audio delay from the remote headset receiving data to
  // the headset playing sound in units of nanoseconds
  uint64_t delay_report_ns = 0;
  uint64_t delay_report_ms = 0;
  // absorbed_bytes is the total number of bytes sent by the Bluetooth stack to
  // a remote headset
  uint64_t absorbed_bytes = 0;
  // absorbed_timestamp is the ...
  struct timespec absorbed_timestamp = {};
  bool timestamp_fetched = false;

  std::unique_lock<std::mutex> lock(out->mutex_);
  if (out->bluetooth_output_->GetPresentationPosition(
          &delay_report_ns, &absorbed_bytes, &absorbed_timestamp)) {
    delay_report_ms = delay_report_ns / 1000000;
    // assume kMinimumDelayMs (50ms) < delay_report_ns < kMaximumDelayMs
    // (1000ms), or it is invalid / ignored and use old delay calculated
    // by ourselves.
    if (delay_report_ms > kMinimumDelayMs &&
        delay_report_ms < kMaximumDelayMs) {
      timestamp_fetched = true;
    } else if (delay_report_ms >= kMaximumDelayMs) {
      LOG(INFO) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                << ", delay_report=" << delay_report_ns << "ns abnormal";
    }
  }
  if (!timestamp_fetched) {
    // default to old delay if any failure is found when fetching from ports
    // audio_a2dp_hw:
    //   frames_count = buffer_size / frame_size
    //   latency (sec.) = frames_count / samples_per_second (sample_rate)
    // Sync from audio_a2dp_hw to add extra delay kExtraAudioSyncMs(+200ms)
    delay_report_ms =
        out->frames_count_ * 1000 / out->sample_rate_ + kExtraAudioSyncMs;
    if (timestamp != nullptr) {
      clock_gettime(CLOCK_MONOTONIC, &absorbed_timestamp);
    }
    LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " uses the legacy delay " << delay_report_ms << " ms";
  }
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", delay=" << delay_report_ms << "ms, data=" << absorbed_bytes
               << " bytes, timestamp=" << absorbed_timestamp.tv_sec << "."
               << StringPrintf("%09ld", absorbed_timestamp.tv_nsec) << "s";

  if (latency_ms != nullptr) {
    *latency_ms = delay_report_ms;
  }
  if (frames != nullptr) {
    const uint64_t latency_frames = delay_report_ms * out->sample_rate_ / 1000;
    *frames = absorbed_bytes / audio_stream_out_frame_size(&out->stream_out_);
    if (out->frames_presented_ < *frames) {
      // Are we (the audio HAL) reset?! The stack counter is obsoleted.
      *frames = out->frames_presented_;
    } else if ((out->frames_presented_ - *frames) > latency_frames) {
      // Is the Bluetooth output reset / restarted by AVDTP reconfig?! Its
      // counter was reset but could not be used.
      *frames = out->frames_presented_;
    }
    // suppose frames would be queued in the headset buffer for delay_report
    // period, so those frames in buffers should not be included in the number
    // of presented frames at the timestamp.
    if (*frames > latency_frames) {
      *frames -= latency_frames;
    } else {
      *frames = 0;
    }
  }
  if (timestamp != nullptr) {
    *timestamp = absorbed_timestamp;
  }
}

void in_calculate_starving_delay_ms(const BluetoothStreamIn* in,
                                    int64_t* frames, int64_t* time) {
  // delay_report is the audio delay from the remote headset receiving data to
  // the headset playing sound in units of nanoseconds
  uint64_t delay_report_ns = 0;
  uint64_t delay_report_ms = 0;
  // dispersed_bytes is the total number of bytes received by the Bluetooth
  // stack from a remote headset
  uint64_t dispersed_bytes = 0;
  struct timespec dispersed_timestamp = {};

  std::unique_lock<std::mutex> lock(in->mutex_);
  in->bluetooth_input_->GetPresentationPosition(
      &delay_report_ns, &dispersed_bytes, &dispersed_timestamp);
  delay_report_ms = delay_report_ns / 1000000;

  const uint64_t latency_frames = delay_report_ms * in->sample_rate_ / 1000;
  *frames = dispersed_bytes / audio_stream_in_frame_size(&in->stream_in_);

  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << ", delay=" << delay_report_ms
               << "ms, data=" << dispersed_bytes
               << " bytes, timestamp=" << dispersed_timestamp.tv_sec << "."
               << StringPrintf("%09ld", dispersed_timestamp.tv_nsec) << "s";

  if (in->frames_presented_ < *frames) {
    // Was audio HAL reset?! The stack counter is obsoleted.
    *frames = in->frames_presented_;
  } else if ((in->frames_presented_ - *frames) > latency_frames) {
    // Is the Bluetooth input reset ?! Its counter was reset but could not be
    // used.
    *frames = in->frames_presented_;
  }
  // suppose frames would be queued in the headset buffer for delay_report
  // period, so those frames in buffers should not be included in the number
  // of presented frames at the timestamp.
  if (*frames > latency_frames) {
    *frames -= latency_frames;
  } else {
    *frames = 0;
  }

  *time = (dispersed_timestamp.tv_sec * 1000000000LL +
           dispersed_timestamp.tv_nsec) /
          1000;
}

}  // namespace

std::ostream& operator<<(std::ostream& os, const BluetoothStreamState& state) {
  switch (state) {
    case BluetoothStreamState::DISABLED:
      return os << "DISABLED";
    case BluetoothStreamState::STANDBY:
      return os << "STANDBY";
    case BluetoothStreamState::STARTING:
      return os << "STARTING";
    case BluetoothStreamState::STARTED:
      return os << "STARTED";
    case BluetoothStreamState::SUSPENDING:
      return os << "SUSPENDING";
    case BluetoothStreamState::UNKNOWN:
      return os << "UNKNOWN";
    default:
      return os << StringPrintf("%#hhx", state);
  }
}

static uint32_t out_get_sample_rate(const struct audio_stream* stream) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  audio_config_t audio_cfg;
  if (out->bluetooth_output_->LoadAudioConfig(&audio_cfg)) {
    LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " audio_cfg=" << audio_cfg;
    return audio_cfg.sample_rate;
  } else {
    LOG(WARNING) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << ", sample_rate=" << out->sample_rate_ << " failed";
    return out->sample_rate_;
  }
}

static int out_set_sample_rate(struct audio_stream* stream, uint32_t rate) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", sample_rate=" << out->sample_rate_;
  return (rate == out->sample_rate_ ? 0 : -1);
}

static size_t out_get_buffer_size(const struct audio_stream* stream) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  size_t buffer_size =
      out->frames_count_ * audio_stream_out_frame_size(&out->stream_out_);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", buffer_size=" << buffer_size;
  return buffer_size;
}

static audio_channel_mask_t out_get_channels(
    const struct audio_stream* stream) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  audio_config_t audio_cfg;
  if (out->bluetooth_output_->LoadAudioConfig(&audio_cfg)) {
    LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " audio_cfg=" << audio_cfg;
    return audio_cfg.channel_mask;
  } else {
    LOG(WARNING) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << ", channels=" << StringPrintf("%#x", out->channel_mask_)
                 << " failure";
    return out->channel_mask_;
  }
}

static audio_format_t out_get_format(const struct audio_stream* stream) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  audio_config_t audio_cfg;
  if (out->bluetooth_output_->LoadAudioConfig(&audio_cfg)) {
    LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " audio_cfg=" << audio_cfg;
    return audio_cfg.format;
  } else {
    LOG(WARNING) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << ", format=" << out->format_ << " failure";
    return out->format_;
  }
}

static int out_set_format(struct audio_stream* stream, audio_format_t format) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", format=" << out->format_;
  return (format == out->format_ ? 0 : -1);
}

static int out_standby(struct audio_stream* stream) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  std::unique_lock<std::mutex> lock(out->mutex_);
  int retval = 0;

  // out->last_write_time_us_ = 0; unnecessary as a stale write time has same
  // effect
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << " being standby (suspend)";
  if (out->bluetooth_output_->GetState() == BluetoothStreamState::STARTED) {
    out->frames_rendered_ = 0;
    retval = (out->bluetooth_output_->Suspend() ? 0 : -EIO);
  } else if (out->bluetooth_output_->GetState() ==
                 BluetoothStreamState::STARTING ||
             out->bluetooth_output_->GetState() ==
                 BluetoothStreamState::SUSPENDING) {
    LOG(WARNING) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " NOT ready to be standby";
    retval = -EBUSY;
  } else {
    LOG(DEBUG) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << " standby already";
  }
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << " standby (suspend) retval=" << retval;

  return retval;
}

static int out_dump(const struct audio_stream* stream, int fd) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState();
  return 0;
}

static int out_set_parameters(struct audio_stream* stream,
                              const char* kvpairs) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  std::unique_lock<std::mutex> lock(out->mutex_);
  int retval = 0;

  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", kvpairs=[" << kvpairs << "]";

  std::unordered_map<std::string, std::string> params =
      ParseAudioParams(kvpairs);
  if (params.empty()) return retval;

  LOG(VERBOSE) << __func__ << ": ParamsMap=[" << GetAudioParamString(params)
               << "]";

  audio_config_t audio_cfg;
  if (params.find(AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES) != params.end() ||
      params.find(AUDIO_PARAMETER_STREAM_SUP_CHANNELS) != params.end() ||
      params.find(AUDIO_PARAMETER_STREAM_SUP_FORMATS) != params.end()) {
    if (out->bluetooth_output_->LoadAudioConfig(&audio_cfg)) {
      out->sample_rate_ = audio_cfg.sample_rate;
      out->channel_mask_ = audio_cfg.channel_mask;
      out->format_ = audio_cfg.format;
      LOG(VERBOSE) << "state=" << out->bluetooth_output_->GetState()
                   << ", sample_rate=" << out->sample_rate_
                   << ", channels=" << StringPrintf("%#x", out->channel_mask_)
                   << ", format=" << out->format_;
    } else {
      LOG(WARNING) << __func__
                   << ": state=" << out->bluetooth_output_->GetState()
                   << " failed to get audio config";
    }
  }

  if (params.find(AUDIO_PARAMETER_STREAM_ROUTING) != params.end()) {
    auto routing_param = params.find("routing");
    LOG(INFO) << __func__ << ": state=" << out->bluetooth_output_->GetState()
              << ", stream param '" << routing_param->first.c_str() << "="
              << routing_param->second.c_str() << "'";
  }

  if (params.find("A2dpSuspended") != params.end() &&
      out->bluetooth_output_->IsA2dp()) {
    if (params["A2dpSuspended"] == "true") {
      LOG(INFO) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                << " stream param stopped";
      out->frames_rendered_ = 0;
      if (out->bluetooth_output_->GetState() == BluetoothStreamState::STARTED) {
        out->bluetooth_output_->Suspend();
        out->bluetooth_output_->SetState(BluetoothStreamState::DISABLED);
      } else if (out->bluetooth_output_->GetState() !=
                 BluetoothStreamState::DISABLED) {
        out->bluetooth_output_->Stop();
      }
    } else {
      LOG(INFO) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                << " stream param standby";
      if (out->bluetooth_output_->GetState() ==
          BluetoothStreamState::DISABLED) {
        out->bluetooth_output_->SetState(BluetoothStreamState::STANDBY);
      }
    }
  }

  if (params.find("LeAudioSuspended") != params.end() &&
      out->bluetooth_output_->IsLeAudio()) {
    LOG(INFO) << __func__ << ": LeAudioSuspended found LEAudio="
              << out->bluetooth_output_->IsLeAudio();
    if (params["LeAudioSuspended"] == "true") {
      LOG(INFO) << __func__ << ": LeAudioSuspended true, state="
                << out->bluetooth_output_->GetState()
                << " stream param standby";
      if (out->bluetooth_output_->GetState() == BluetoothStreamState::STARTED) {
        LOG(INFO) << __func__ << ": Stream is started, suspending LE Audio";
      } else if (out->bluetooth_output_->GetState() !=
                 BluetoothStreamState::DISABLED) {
        LOG(INFO) << __func__ << ": Stream is disabled, suspending LE Audio";
      }
    } else {
      LOG(INFO) << __func__ << ": LeAudioSuspended false, state="
                << out->bluetooth_output_->GetState()
                << " stream param standby";
      if (out->bluetooth_output_->GetState() ==
          BluetoothStreamState::DISABLED) {
        LOG(INFO) << __func__ << ": Stream is disabled, unsuspending LE Audio";
      }
    }
  }

  if (params.find(AUDIO_PARAMETER_KEY_CLOSING) != params.end()) {
    if (params[AUDIO_PARAMETER_KEY_CLOSING] == "true") {
      LOG(INFO) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                << " stream param closing, disallow any writes?";
      if (out->bluetooth_output_->GetState() !=
          BluetoothStreamState::DISABLED) {
        out->frames_rendered_ = 0;
        out->frames_presented_ = 0;
        out->bluetooth_output_->Stop();
      }
    }
  }

  if (params.find(AUDIO_PARAMETER_KEY_EXITING) != params.end()) {
    if (params[AUDIO_PARAMETER_KEY_EXITING] == "1") {
      LOG(INFO) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                << " stream param exiting";
      if (out->bluetooth_output_->GetState() !=
          BluetoothStreamState::DISABLED) {
        out->frames_rendered_ = 0;
        out->frames_presented_ = 0;
        out->bluetooth_output_->Stop();
      }
    }
  }

  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", kvpairs=[" << kvpairs << "], retval=" << retval;
  return retval;
}

static char* out_get_parameters(const struct audio_stream* stream,
                                const char* keys) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  std::unique_lock<std::mutex> lock(out->mutex_);

  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", keys=[" << keys << "]";

  std::unordered_map<std::string, std::string> params = ParseAudioParams(keys);
  if (params.empty()) return strdup("");

  audio_config_t audio_cfg;
  if (out->bluetooth_output_->LoadAudioConfig(&audio_cfg)) {
    LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " audio_cfg=" << audio_cfg;
  } else {
    LOG(ERROR) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << " failed to get audio config";
  }

  std::unordered_map<std::string, std::string> return_params;
  if (params.find(AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES) != params.end()) {
    std::string param;
    if (audio_cfg.sample_rate == 16000) {
      param = "16000";
    }
    if (audio_cfg.sample_rate == 24000) {
      param = "24000";
    }
    if (audio_cfg.sample_rate == 44100) {
      param = "44100";
    }
    if (audio_cfg.sample_rate == 48000) {
      param = "48000";
    }
    if (audio_cfg.sample_rate == 88200) {
      param = "88200";
    }
    if (audio_cfg.sample_rate == 96000) {
      param = "96000";
    }
    if (audio_cfg.sample_rate == 176400) {
      param = "176400";
    }
    if (audio_cfg.sample_rate == 192000) {
      param = "192000";
    }
    return_params[AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES] = param;
  }

  if (params.find(AUDIO_PARAMETER_STREAM_SUP_CHANNELS) != params.end()) {
    std::string param;
    if (audio_cfg.channel_mask == AUDIO_CHANNEL_OUT_MONO) {
      param = "AUDIO_CHANNEL_OUT_MONO";
    }
    if (audio_cfg.channel_mask == AUDIO_CHANNEL_OUT_STEREO) {
      param = "AUDIO_CHANNEL_OUT_STEREO";
    }
    return_params[AUDIO_PARAMETER_STREAM_SUP_CHANNELS] = param;
  }

  if (params.find(AUDIO_PARAMETER_STREAM_SUP_FORMATS) != params.end()) {
    std::string param;
    if (audio_cfg.format == AUDIO_FORMAT_PCM_16_BIT) {
      param = "AUDIO_FORMAT_PCM_16_BIT";
    }
    if (audio_cfg.format == AUDIO_FORMAT_PCM_24_BIT_PACKED) {
      param = "AUDIO_FORMAT_PCM_24_BIT_PACKED";
    }
    if (audio_cfg.format == AUDIO_FORMAT_PCM_8_24_BIT) {
      param = "AUDIO_FORMAT_PCM_8_24_BIT";
    }
    if (audio_cfg.format == AUDIO_FORMAT_PCM_32_BIT) {
      param = "AUDIO_FORMAT_PCM_32_BIT";
    }
    return_params[AUDIO_PARAMETER_STREAM_SUP_FORMATS] = param;
  }

  std::string result;
  for (const auto& ptr : return_params) {
    result += ptr.first + "=" + ptr.second + ";";
  }

  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", result=[" << result << "]";
  return strdup(result.c_str());
}

static uint32_t out_get_latency_ms(const struct audio_stream_out* stream) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  uint32_t latency_ms = 0;
  out_calculate_feeding_delay_ms(out, &latency_ms);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", latency=" << latency_ms << "ms";
  return latency_ms;
}

static int out_set_volume(struct audio_stream_out* stream, float left,
                          float right) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", Left=" << left << ", Right=" << right;
  return -1;
}

static ssize_t out_write(struct audio_stream_out* stream, const void* buffer,
                         size_t bytes) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  std::unique_lock<std::mutex> lock(out->mutex_);
  size_t totalWritten = 0;

  if (out->bluetooth_output_->GetState() != BluetoothStreamState::STARTED) {
    LOG(INFO) << __func__ << ": state=" << out->bluetooth_output_->GetState()
              << " first time bytes=" << bytes;
    lock.unlock();
    if (stream->resume(stream)) {
      LOG(ERROR) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " failed to resume";
      if (out->bluetooth_output_->GetState() ==
          BluetoothStreamState::DISABLED) {
        // drop data for cases of A2dpSuspended=true / closing=true
        totalWritten = bytes;
      }
      usleep(out->preferred_data_interval_us);
      return totalWritten;
    }
    lock.lock();
  }
  lock.unlock();
  totalWritten = out->bluetooth_output_->WriteData(buffer, bytes);
  lock.lock();

  struct timespec ts = {.tv_sec = 0, .tv_nsec = 0};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  if (totalWritten) {
    const size_t frames = bytes / audio_stream_out_frame_size(stream);
    out->frames_rendered_ += frames;
    out->frames_presented_ += frames;
    out->last_write_time_us_ = (ts.tv_sec * 1000000000LL + ts.tv_nsec) / 1000;
  } else {
    const int64_t now = (ts.tv_sec * 1000000000LL + ts.tv_nsec) / 1000;
    const int64_t elapsed_time_since_last_write =
        now - out->last_write_time_us_;
    // frames_count = written_data / frame_size
    // play_time (ms) = frames_count / (sample_rate (Sec.) / 1000000)
    // sleep_time (ms) = play_time - elapsed_time
    int64_t sleep_time = bytes * 1000000LL /
                             audio_stream_out_frame_size(stream) /
                             out_get_sample_rate(&stream->common) -
                         elapsed_time_since_last_write;
    if (sleep_time > 0) {
      LOG(VERBOSE) << __func__ << ": sleep " << (sleep_time / 1000)
                   << " ms when writting FMQ datapath";
      lock.unlock();
      usleep(sleep_time);
      lock.lock();
    } else {
      // we don't sleep when we exit standby (this is typical for a real alsa
      // buffer).
      sleep_time = 0;
    }
    out->last_write_time_us_ = now + sleep_time;
  }
  return totalWritten;
}

static int out_get_render_position(const struct audio_stream_out* stream,
                                   uint32_t* dsp_frames) {
  if (dsp_frames == nullptr) return -EINVAL;

  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  // frames = (latency (ms) / 1000) * samples_per_second (sample_rate)
  const uint64_t latency_frames =
      (uint64_t)out_get_latency_ms(stream) * out->sample_rate_ / 1000;
  if (out->frames_rendered_ >= latency_frames) {
    *dsp_frames = (uint32_t)(out->frames_rendered_ - latency_frames);
  } else {
    *dsp_frames = 0;
  }

  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", dsp_frames=" << *dsp_frames;
  return 0;
}

static int out_add_audio_effect(const struct audio_stream* stream,
                                effect_handle_t effect) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", effect=" << effect;
  return 0;
}

static int out_remove_audio_effect(const struct audio_stream* stream,
                                   effect_handle_t effect) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", effect=" << effect;
  return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out* stream,
                                        int64_t* timestamp) {
  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  *timestamp = 0;
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", timestamp=" << *timestamp;
  return -EINVAL;
}

static int out_pause(struct audio_stream_out* stream) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  std::unique_lock<std::mutex> lock(out->mutex_);
  int retval = 0;
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", pausing (suspend)";
  if (out->bluetooth_output_->GetState() == BluetoothStreamState::STARTED) {
    out->frames_rendered_ = 0;
    retval = (out->bluetooth_output_->Suspend() ? 0 : -EIO);
  } else if (out->bluetooth_output_->GetState() ==
                 BluetoothStreamState::STARTING ||
             out->bluetooth_output_->GetState() ==
                 BluetoothStreamState::SUSPENDING) {
    LOG(WARNING) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " NOT ready to pause?!";
    retval = -EBUSY;
  } else {
    LOG(DEBUG) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << " paused already";
  }
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", pausing (suspend) retval=" << retval;

  return retval;
}

static int out_resume(struct audio_stream_out* stream) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  std::unique_lock<std::mutex> lock(out->mutex_);
  int retval = 0;

  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", resuming (start)";
  if (out->bluetooth_output_->GetState() == BluetoothStreamState::STANDBY) {
    retval = (out->bluetooth_output_->Start() ? 0 : -EIO);
  } else if (out->bluetooth_output_->GetState() ==
                 BluetoothStreamState::STARTING ||
             out->bluetooth_output_->GetState() ==
                 BluetoothStreamState::SUSPENDING) {
    LOG(WARNING) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " NOT ready to resume?!";
    retval = -EBUSY;
  } else if (out->bluetooth_output_->GetState() ==
             BluetoothStreamState::DISABLED) {
    LOG(WARNING) << __func__ << ": state=" << out->bluetooth_output_->GetState()
                 << " NOT allow to resume?!";
    retval = -EINVAL;
  } else {
    LOG(DEBUG) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << " resumed already";
  }
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", resuming (start) retval=" << retval;

  return retval;
}

static int out_get_presentation_position(const struct audio_stream_out* stream,
                                         uint64_t* frames,
                                         struct timespec* timestamp) {
  if (frames == nullptr || timestamp == nullptr) {
    return -EINVAL;
  }

  const auto* out = reinterpret_cast<const BluetoothStreamOut*>(stream);
  out_calculate_feeding_delay_ms(out, nullptr, frames, timestamp);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", frames=" << *frames << ", timestamp=" << timestamp->tv_sec
               << "." << StringPrintf("%09ld", timestamp->tv_nsec) << "s";
  return 0;
}

static void out_update_source_metadata_v7(
    struct audio_stream_out* stream,
    const struct source_metadata_v7* source_metadata_v7) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  std::unique_lock<std::mutex> lock(out->mutex_);
  if (source_metadata_v7 == nullptr || source_metadata_v7->track_count == 0) {
    return;
  }
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", " << source_metadata_v7->track_count << " track(s)";
  if (!out->is_aidl) {
    struct source_metadata source_metadata;
    source_metadata.track_count = source_metadata_v7->track_count;
    struct playback_track_metadata playback_track;
    playback_track_metadata_from_v7(&playback_track,
                                    source_metadata_v7->tracks);
    source_metadata.tracks = &playback_track;

    static_cast<::android::bluetooth::audio::hidl::BluetoothAudioPortHidl*>(
        out->bluetooth_output_.get())
        ->UpdateTracksMetadata(&source_metadata);
  } else {
    static_cast<::android::bluetooth::audio::aidl::BluetoothAudioPortAidl*>(
        out->bluetooth_output_.get())
        ->UpdateSourceMetadata(source_metadata_v7);
  }
}

int adev_open_output_stream(struct audio_hw_device* dev,
                            audio_io_handle_t handle, audio_devices_t devices,
                            audio_output_flags_t flags,
                            struct audio_config* config,
                            struct audio_stream_out** stream_out,
                            const char* address __unused) {
  *stream_out = nullptr;
  auto out = std::make_unique<BluetoothStreamOut>();
  if (::aidl::android::hardware::bluetooth::audio::BluetoothAudioSession::
          IsAidlAvailable()) {
    out->bluetooth_output_ = std::make_unique<
        ::android::bluetooth::audio::aidl::BluetoothAudioPortAidlOut>();
    out->is_aidl = true;
  } else {
    out->bluetooth_output_ = std::make_unique<
        ::android::bluetooth::audio::hidl::BluetoothAudioPortHidlOut>();
    out->is_aidl = false;
  }
  if (!out->bluetooth_output_->SetUp(devices)) {
    out->bluetooth_output_ = nullptr;
    LOG(ERROR) << __func__ << ": cannot init HAL";
    return -EINVAL;
  }
  LOG(VERBOSE) << __func__ << ": device=" << StringPrintf("%#x", devices);

  out->stream_out_.common.get_sample_rate = out_get_sample_rate;
  out->stream_out_.common.set_sample_rate = out_set_sample_rate;
  out->stream_out_.common.get_buffer_size = out_get_buffer_size;
  out->stream_out_.common.get_channels = out_get_channels;
  out->stream_out_.common.get_format = out_get_format;
  out->stream_out_.common.set_format = out_set_format;
  out->stream_out_.common.standby = out_standby;
  out->stream_out_.common.dump = out_dump;
  out->stream_out_.common.set_parameters = out_set_parameters;
  out->stream_out_.common.get_parameters = out_get_parameters;
  out->stream_out_.common.add_audio_effect = out_add_audio_effect;
  out->stream_out_.common.remove_audio_effect = out_remove_audio_effect;
  out->stream_out_.get_latency = out_get_latency_ms;
  out->stream_out_.set_volume = out_set_volume;
  out->stream_out_.write = out_write;
  out->stream_out_.get_render_position = out_get_render_position;
  out->stream_out_.get_next_write_timestamp = out_get_next_write_timestamp;
  out->stream_out_.pause = out_pause;
  out->stream_out_.resume = out_resume;
  out->stream_out_.get_presentation_position = out_get_presentation_position;
  out->stream_out_.update_source_metadata_v7 = out_update_source_metadata_v7;
  /** Fix Coverity Scan Issue @{ */
  out->channel_mask_ = AUDIO_CHANNEL_NONE;
  /** @} */

  if (!out->bluetooth_output_->LoadAudioConfig(config)) {
    LOG(ERROR) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << " failed to get audio config";
  }
  // WAR to support Mono / 16 bits per sample as the Bluetooth stack required
  if (config->channel_mask == AUDIO_CHANNEL_OUT_MONO &&
      config->format == AUDIO_FORMAT_PCM_16_BIT) {
    LOG(INFO) << __func__
              << ": force channels=" << StringPrintf("%#x", out->channel_mask_)
              << " to be AUDIO_CHANNEL_OUT_STEREO";
    out->bluetooth_output_->ForcePcmStereoToMono(true);
    config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
  }
  out->sample_rate_ = config->sample_rate;
  out->channel_mask_ = config->channel_mask;
  out->format_ = config->format;
  // frame is number of samples per channel

  size_t preferred_data_interval_us = kBluetoothDefaultOutputBufferMs * 1000;
  if (out->bluetooth_output_->GetPreferredDataIntervalUs(
          &preferred_data_interval_us) &&
      preferred_data_interval_us != 0) {
    out->preferred_data_interval_us = preferred_data_interval_us;
  } else {
    out->preferred_data_interval_us = kBluetoothDefaultOutputBufferMs * 1000;
  }

  // Ensure minimum buffer duration for spatialized output
  if ((flags == (AUDIO_OUTPUT_FLAG_FAST | AUDIO_OUTPUT_FLAG_DEEP_BUFFER) ||
       flags == AUDIO_OUTPUT_FLAG_SPATIALIZER) &&
      out->preferred_data_interval_us <
          kBluetoothSpatializerOutputBufferMs * 1000) {
    out->preferred_data_interval_us =
        kBluetoothSpatializerOutputBufferMs * 1000;
    LOG(INFO) << __func__
              << ": adjusting to minimum buffer duration for spatializer: "
              << StringPrintf("%zu", out->preferred_data_interval_us);
  }

  out->frames_count_ =
      FrameCount(out->preferred_data_interval_us, out->sample_rate_);

  out->frames_rendered_ = 0;
  out->frames_presented_ = 0;

  BluetoothStreamOut* out_ptr = out.release();
  {
    auto* bluetooth_device = reinterpret_cast<BluetoothAudioDevice*>(dev);
    std::lock_guard<std::mutex> guard(bluetooth_device->mutex_);
    bluetooth_device->opened_stream_outs_.push_back(out_ptr);
  }

  *stream_out = &out_ptr->stream_out_;
  LOG(INFO) << __func__ << ": state=" << out_ptr->bluetooth_output_->GetState()
            << ", sample_rate=" << out_ptr->sample_rate_
            << ", channels=" << StringPrintf("%#x", out_ptr->channel_mask_)
            << ", format=" << out_ptr->format_
            << ", preferred_data_interval_us="
            << out_ptr->preferred_data_interval_us
            << ", frames=" << out_ptr->frames_count_;
  return 0;
}

void adev_close_output_stream(struct audio_hw_device* dev,
                              struct audio_stream_out* stream) {
  auto* out = reinterpret_cast<BluetoothStreamOut*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", stopping";
  {
    auto* bluetooth_device = reinterpret_cast<BluetoothAudioDevice*>(dev);
    std::lock_guard<std::mutex> guard(bluetooth_device->mutex_);
    bluetooth_device->opened_stream_outs_.remove(out);
  }
  if (out->bluetooth_output_->GetState() != BluetoothStreamState::DISABLED) {
    out->frames_rendered_ = 0;
    out->frames_presented_ = 0;
    out->bluetooth_output_->Stop();
  }
  out->bluetooth_output_->TearDown();
  LOG(VERBOSE) << __func__ << ": state=" << out->bluetooth_output_->GetState()
               << ", stopped";
  delete out;
}

size_t adev_get_input_buffer_size(const struct audio_hw_device* dev,
                                  const struct audio_config* config) {
  /* TODO: Adjust this value */
  LOG(VERBOSE) << __func__;
  return 320;
}

static uint32_t in_get_sample_rate(const struct audio_stream* stream) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);

  return in->sample_rate_;
}

static int in_set_sample_rate(struct audio_stream* stream, uint32_t rate) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);

  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << ", sample_rate=" << in->sample_rate_;
  return (rate == in->sample_rate_ ? 0 : -ENOSYS);
}

static size_t in_get_buffer_size(const struct audio_stream* stream) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  size_t buffer_size =
      in->frames_count_ * audio_stream_in_frame_size(&in->stream_in_);
  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << ", buffer_size=" << buffer_size;
  return buffer_size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream* stream) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  audio_config_t audio_cfg;
  if (in->bluetooth_input_->LoadAudioConfig(&audio_cfg)) {
    LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                 << " audio_cfg=" << audio_cfg;
    return audio_cfg.channel_mask;
  } else {
    LOG(WARNING) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                 << ", channels=" << StringPrintf("%#x", in->channel_mask_)
                 << " failure";
    return in->channel_mask_;
  }
}

static audio_format_t in_get_format(const struct audio_stream* stream) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  audio_config_t audio_cfg;
  if (in->bluetooth_input_->LoadAudioConfig(&audio_cfg)) {
    LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                 << " audio_cfg=" << audio_cfg;
    return audio_cfg.format;
  } else {
    LOG(WARNING) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                 << ", format=" << in->format_ << " failure";
    return in->format_;
  }
}

static int in_set_format(struct audio_stream* stream, audio_format_t format) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << ", format=" << in->format_;
  return (format == in->format_ ? 0 : -ENOSYS);
}

static bool in_state_transition_timeout(BluetoothStreamIn* in,
                                        std::unique_lock<std::mutex>& lock,
                                        const BluetoothStreamState& state,
                                        uint16_t timeout_ms) {
  /* Don't loose suspend request, AF will not retry */
  while (in->bluetooth_input_->GetState() == state) {
    lock.unlock();
    usleep(1000);
    lock.lock();

    /* Don't block AF forever */
    if (--timeout_ms <= 0) {
      LOG(WARNING) << __func__ << ", can't suspend - stucked in: "
                   << static_cast<int>(state) << " state";
      return false;
    }
  }

  return true;
}

static int in_standby(struct audio_stream* stream) {
  auto* in = reinterpret_cast<BluetoothStreamIn*>(stream);
  std::unique_lock<std::mutex> lock(in->mutex_);
  int retval = 0;

  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << " being standby (suspend)";

  /* Give some time to start up */
  if (!in_state_transition_timeout(in, lock, BluetoothStreamState::STARTING,
                                   kBluetoothDefaultInputStateTimeoutMs)) {
    LOG(ERROR) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << " NOT ready to by standby";
    return retval;
  }

  if (in->bluetooth_input_->GetState() == BluetoothStreamState::STARTED) {
    retval = (in->bluetooth_input_->Suspend() ? 0 : -EIO);
  } else if (in->bluetooth_input_->GetState() !=
             BluetoothStreamState::SUSPENDING) {
    LOG(DEBUG) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << " standby already";
    return retval;
  }

  /* Give some time to suspend */
  if (!in_state_transition_timeout(in, lock, BluetoothStreamState::SUSPENDING,
                                   kBluetoothDefaultInputStateTimeoutMs)) {
    LOG(ERROR) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << " NOT ready to by standby";
    return 0;
  }

  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << " standby (suspend) retval=" << retval;

  return retval;
}

static int in_dump(const struct audio_stream* stream, int fd) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState();

  return 0;
}

static int in_set_parameters(struct audio_stream* stream, const char* kvpairs) {
  auto* in = reinterpret_cast<BluetoothStreamIn*>(stream);
  std::unique_lock<std::mutex> lock(in->mutex_);
  int retval = 0;

  LOG(INFO) << __func__
            << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState()
            << ", kvpairs=[" << kvpairs << "]";

  std::unordered_map<std::string, std::string> params =
      ParseAudioParams(kvpairs);

  if (params.empty()) return retval;

  LOG(INFO) << __func__ << ": ParamsMap=[" << GetAudioParamString(params)
            << "]";

  return retval;
}

static char* in_get_parameters(const struct audio_stream* stream,
                               const char* keys) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  std::unique_lock<std::mutex> lock(in->mutex_);

  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState()
               << ", keys=[" << keys << "]";

  std::unordered_map<std::string, std::string> params = ParseAudioParams(keys);
  if (params.empty()) return strdup("");

  audio_config_t audio_cfg;
  if (in->bluetooth_input_->LoadAudioConfig(&audio_cfg)) {
    LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                 << " audio_cfg=" << audio_cfg;
  } else {
    LOG(ERROR) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << " failed to get audio config";
  }

  std::unordered_map<std::string, std::string> return_params;

  /* TODO: Implement parameter getter */

  std::string result;
  for (const auto& ptr : return_params) {
    result += ptr.first + "=" + ptr.second + ";";
  }

  return strdup(result.c_str());
}

static int in_add_audio_effect(const struct audio_stream* stream,
                               effect_handle_t effect) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << ", effect=" << effect;
  return 0;
}

static int in_remove_audio_effect(const struct audio_stream* stream,
                                  effect_handle_t effect) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << ", effect=" << effect;
  return 0;
}

static int in_set_gain(struct audio_stream_in* stream, float gain) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState();

  return 0;
}

static ssize_t in_read(struct audio_stream_in* stream, void* buffer,
                       size_t bytes) {
  auto* in = reinterpret_cast<BluetoothStreamIn*>(stream);
  std::unique_lock<std::mutex> lock(in->mutex_);
  size_t totalRead = 0;

  /* Give some time to start up */
  if (!in_state_transition_timeout(in, lock, BluetoothStreamState::STARTING,
                                   kBluetoothDefaultInputStateTimeoutMs))
    return -EBUSY;

  if (in->bluetooth_input_->GetState() != BluetoothStreamState::STARTED) {
    LOG(INFO) << __func__ << ": state=" << in->bluetooth_input_->GetState()
              << " first time bytes=" << bytes;

    int retval = 0;
    LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                 << ", starting";
    if (in->bluetooth_input_->GetState() == BluetoothStreamState::STANDBY) {
      retval = (in->bluetooth_input_->Start() ? 0 : -EIO);
    } else if (in->bluetooth_input_->GetState() ==
               BluetoothStreamState::SUSPENDING) {
      LOG(WARNING) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                   << " NOT ready to start?!";
      retval = -EBUSY;
    } else if (in->bluetooth_input_->GetState() ==
               BluetoothStreamState::DISABLED) {
      LOG(WARNING) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                   << " NOT allow to start?!";
      retval = -EINVAL;
    } else {
      LOG(DEBUG) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                 << " started already";
    }
    LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                 << ", starting (start) retval=" << retval;

    if (retval) {
      LOG(ERROR) << __func__ << ": state=" << in->bluetooth_input_->GetState()
                 << " failed to start";
      return retval;
    }
  }

  lock.unlock();
  totalRead = in->bluetooth_input_->ReadData(buffer, bytes);
  lock.lock();

  struct timespec ts = {.tv_sec = 0, .tv_nsec = 0};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  in->last_read_time_us_ = (ts.tv_sec * 1000000000LL + ts.tv_nsec) / 1000;

  const size_t frames = totalRead / audio_stream_in_frame_size(stream);
  in->frames_presented_ += frames;

  return totalRead;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in* stream) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState();

  return 0;
}

static int in_get_capture_position(const struct audio_stream_in* stream,
                                   int64_t* frames, int64_t* time) {
  if (stream == NULL || frames == NULL || time == NULL) {
    return -EINVAL;
  }
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);

  if (in->bluetooth_input_->GetState() == BluetoothStreamState::STANDBY) {
    LOG(WARNING) << __func__ << ": state= " << in->bluetooth_input_->GetState();
    return -ENOSYS;
  }

  in_calculate_starving_delay_ms(in, frames, time);

  return 0;
}

static int in_start(const struct audio_stream_in* stream) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState();

  return 0;
}

static int in_stop(const struct audio_stream_in* stream) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState();

  return 0;
}

static int in_create_mmap_buffer(const struct audio_stream_in* stream,
                                 int32_t min_size_frames,
                                 struct audio_mmap_buffer_info* info) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState();

  return -ENOSYS;
}

static int in_get_mmap_position(const struct audio_stream_in* stream,
                                struct audio_mmap_position* position) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState();

  return -ENOSYS;
}

static int in_get_active_microphones(
    const struct audio_stream_in* stream,
    struct audio_microphone_characteristic_t* mic_array, size_t* mic_count) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState();

  return -ENOSYS;
}

static int in_set_microphone_direction(const struct audio_stream_in* stream,
                                       audio_microphone_direction_t direction) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState();

  return -ENOSYS;
}

static int in_set_microphone_field_dimension(
    const struct audio_stream_in* stream, float zoom) {
  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(VERBOSE) << __func__
               << ": NOT HANDLED! state=" << in->bluetooth_input_->GetState();

  return -ENOSYS;
}

static void in_update_sink_metadata_v7(
    struct audio_stream_in* stream,
    const struct sink_metadata_v7* sink_metadata) {
  LOG(INFO) << __func__;
  if (sink_metadata == nullptr || sink_metadata->track_count == 0) {
    return;
  }

  const auto* in = reinterpret_cast<const BluetoothStreamIn*>(stream);
  LOG(INFO) << __func__ << ": state=" << in->bluetooth_input_->GetState()
            << ", " << sink_metadata->track_count << " track(s)";

  if (!in->is_aidl) {
    LOG(WARNING) << __func__
                 << " is only supported in AIDL but using HIDL now!";
    return;
  }
  static_cast<::android::bluetooth::audio::aidl::BluetoothAudioPortAidl*>(
      in->bluetooth_input_.get())
      ->UpdateSinkMetadata(sink_metadata);
}

int adev_open_input_stream(struct audio_hw_device* dev,
                           audio_io_handle_t handle, audio_devices_t devices,
                           struct audio_config* config,
                           struct audio_stream_in** stream_in,
                           audio_input_flags_t flags __unused,
                           const char* address __unused,
                           audio_source_t source __unused) {
  *stream_in = nullptr;
  auto in = std::make_unique<BluetoothStreamIn>();
  if (::aidl::android::hardware::bluetooth::audio::BluetoothAudioSession::
          IsAidlAvailable()) {
    in->bluetooth_input_ = std::make_unique<
        ::android::bluetooth::audio::aidl::BluetoothAudioPortAidlIn>();
    in->is_aidl = true;
  } else {
    in->bluetooth_input_ = std::make_unique<
        ::android::bluetooth::audio::hidl::BluetoothAudioPortHidlIn>();
    in->is_aidl = false;
  }
  if (!in->bluetooth_input_->SetUp(devices)) {
    in->bluetooth_input_ = nullptr;
    LOG(ERROR) << __func__ << ": cannot init HAL";
    return -EINVAL;
  }

  LOG(INFO) << __func__ << ": device=" << StringPrintf("%#x", devices);

  in->stream_in_.common.get_sample_rate = in_get_sample_rate;
  in->stream_in_.common.set_sample_rate = in_set_sample_rate;
  in->stream_in_.common.get_buffer_size = in_get_buffer_size;
  in->stream_in_.common.get_channels = in_get_channels;
  in->stream_in_.common.get_format = in_get_format;
  in->stream_in_.common.set_format = in_set_format;
  in->stream_in_.common.standby = in_standby;
  in->stream_in_.common.dump = in_dump;
  in->stream_in_.common.set_parameters = in_set_parameters;
  in->stream_in_.common.get_parameters = in_get_parameters;
  in->stream_in_.common.add_audio_effect = in_add_audio_effect;
  in->stream_in_.common.remove_audio_effect = in_remove_audio_effect;
  in->stream_in_.set_gain = in_set_gain;
  in->stream_in_.read = in_read;
  in->stream_in_.get_input_frames_lost = in_get_input_frames_lost;
  in->stream_in_.get_capture_position = in_get_capture_position;
  in->stream_in_.start = in_start;
  in->stream_in_.stop = in_stop;
  in->stream_in_.create_mmap_buffer = in_create_mmap_buffer;
  in->stream_in_.get_mmap_position = in_get_mmap_position;
  in->stream_in_.get_active_microphones = in_get_active_microphones;
  in->stream_in_.set_microphone_direction = in_set_microphone_direction;
  in->stream_in_.set_microphone_field_dimension =
      in_set_microphone_field_dimension;
  in->stream_in_.update_sink_metadata_v7 = in_update_sink_metadata_v7;

  if (!in->bluetooth_input_->LoadAudioConfig(config)) {
    LOG(ERROR) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << " failed to get audio config";
    return -EINVAL;
  }

  in->sample_rate_ = config->sample_rate;
  in->channel_mask_ = config->channel_mask;
  in->format_ = config->format;
  // frame is number of samples per channel

  size_t preferred_data_interval_us = kBluetoothDefaultInputBufferMs * 1000;
  if (in->bluetooth_input_->GetPreferredDataIntervalUs(
          &preferred_data_interval_us) &&
      preferred_data_interval_us != 0) {
    in->preferred_data_interval_us = preferred_data_interval_us;
  } else {
    in->preferred_data_interval_us = kBluetoothDefaultInputBufferMs * 1000;
  }

  in->frames_count_ =
      FrameCount(in->preferred_data_interval_us, in->sample_rate_);
  in->frames_presented_ = 0;

  BluetoothStreamIn* in_ptr = in.release();
  *stream_in = &in_ptr->stream_in_;
  LOG(INFO) << __func__ << ": state=" << in_ptr->bluetooth_input_->GetState()
            << ", sample_rate=" << in_ptr->sample_rate_
            << ", channels=" << StringPrintf("%#x", in_ptr->channel_mask_)
            << ", format=" << in_ptr->format_ << ", preferred_data_interval_us="
            << in_ptr->preferred_data_interval_us
            << ", frames=" << in_ptr->frames_count_;

  return 0;
}

void adev_close_input_stream(struct audio_hw_device* dev,
                             struct audio_stream_in* stream) {
  auto* in = reinterpret_cast<BluetoothStreamIn*>(stream);

  if (in->bluetooth_input_->GetState() != BluetoothStreamState::DISABLED) {
    in->bluetooth_input_->Stop();
  }

  in->bluetooth_input_->TearDown();
  LOG(VERBOSE) << __func__ << ": state=" << in->bluetooth_input_->GetState()
               << ", stopped";

  delete in;
}
