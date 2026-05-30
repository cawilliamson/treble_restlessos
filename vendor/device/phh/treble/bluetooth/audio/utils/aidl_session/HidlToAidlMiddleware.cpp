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

#define LOG_TAG "BtAudioNakahara"

#include <aidl/android/hardware/bluetooth/audio/AudioConfiguration.h>
#include <aidl/android/hardware/bluetooth/audio/BluetoothAudioStatus.h>
#include <aidl/android/hardware/bluetooth/audio/SessionType.h>
#include <android-base/logging.h>

#include <functional>
#include <unordered_map>

#include "../aidl_session/BluetoothAudioSession.h"
#include "../aidl_session/BluetoothAudioSessionControl.h"
#include "HidlToAidlMiddleware_2_0.h"
#include "HidlToAidlMiddleware_2_1.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

using HidlStatus = ::android::hardware::bluetooth::audio::V2_0::Status;
using PcmConfig_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::PcmParameters;
using SampleRate_2_0 = ::android::hardware::bluetooth::audio::V2_0::SampleRate;
using ChannelMode_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::ChannelMode;
using BitsPerSample_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::BitsPerSample;
using CodecConfig_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::CodecConfiguration;
using CodecType_2_0 = ::android::hardware::bluetooth::audio::V2_0::CodecType;
using SbcConfig_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::SbcParameters;
using AacConfig_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::AacParameters;
using LdacConfig_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::LdacParameters;
using AptxConfig_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::AptxParameters;
using SbcAllocMethod_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::SbcAllocMethod;
using SbcBlockLength_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::SbcBlockLength;
using SbcChannelMode_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::SbcChannelMode;
using SbcNumSubbands_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::SbcNumSubbands;
using AacObjectType_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::AacObjectType;
using AacVarBitRate_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::AacVariableBitRate;
using LdacChannelMode_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::LdacChannelMode;
using LdacQualityIndex_2_0 =
    ::android::hardware::bluetooth::audio::V2_0::LdacQualityIndex;

using PcmConfig_2_1 =
    ::android::hardware::bluetooth::audio::V2_1::PcmParameters;
using SampleRate_2_1 = ::android::hardware::bluetooth::audio::V2_1::SampleRate;
using Lc3CodecConfig_2_1 =
    ::android::hardware::bluetooth::audio::V2_1::Lc3CodecConfiguration;
using Lc3Config_2_1 =
    ::android::hardware::bluetooth::audio::V2_1::Lc3Parameters;
using Lc3FrameDuration_2_1 =
    ::android::hardware::bluetooth::audio::V2_1::Lc3FrameDuration;

std::mutex legacy_callback_lock;
std::unordered_map<
    SessionType,
    std::unordered_map<uint16_t, std::shared_ptr<PortStatusCallbacks_2_0>>>
    legacy_callback_table;

const static std::unordered_map<SessionType_2_1, SessionType>
    session_type_2_1_to_aidl_map{
        {SessionType_2_1::A2DP_SOFTWARE_ENCODING_DATAPATH,
         SessionType::A2DP_SOFTWARE_ENCODING_DATAPATH},
        {SessionType_2_1::A2DP_HARDWARE_OFFLOAD_DATAPATH,
         SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH},
        {SessionType_2_1::HEARING_AID_SOFTWARE_ENCODING_DATAPATH,
         SessionType::HEARING_AID_SOFTWARE_ENCODING_DATAPATH},
        {SessionType_2_1::LE_AUDIO_SOFTWARE_ENCODING_DATAPATH,
         SessionType::LE_AUDIO_SOFTWARE_ENCODING_DATAPATH},
        {SessionType_2_1::LE_AUDIO_SOFTWARE_DECODED_DATAPATH,
         SessionType::LE_AUDIO_SOFTWARE_DECODING_DATAPATH},
        {SessionType_2_1::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH,
         SessionType::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH},
        {SessionType_2_1::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH,
         SessionType::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH},
    };

const static std::unordered_map<int32_t, SampleRate_2_1>
    sample_rate_to_hidl_2_1_map{
        {44100, SampleRate_2_1::RATE_44100},
        {48000, SampleRate_2_1::RATE_48000},
        {88200, SampleRate_2_1::RATE_88200},
        {96000, SampleRate_2_1::RATE_96000},
        {176400, SampleRate_2_1::RATE_176400},
        {192000, SampleRate_2_1::RATE_192000},
        {16000, SampleRate_2_1::RATE_16000},
        {24000, SampleRate_2_1::RATE_24000},
        {8000, SampleRate_2_1::RATE_8000},
        {32000, SampleRate_2_1::RATE_32000},
    };

const static std::unordered_map<CodecType, CodecType_2_0>
    codec_type_to_hidl_2_0_map{
        {CodecType::UNKNOWN, CodecType_2_0::UNKNOWN},
        {CodecType::SBC, CodecType_2_0::SBC},
        {CodecType::AAC, CodecType_2_0::AAC},
        {CodecType::APTX, CodecType_2_0::APTX},
        {CodecType::APTX_HD, CodecType_2_0::APTX_HD},
        {CodecType::LDAC, CodecType_2_0::LDAC},
        {CodecType::LC3, CodecType_2_0::UNKNOWN},
    };

const static std::unordered_map<SbcChannelMode, SbcChannelMode_2_0>
    sbc_channel_mode_to_hidl_2_0_map{
        {SbcChannelMode::UNKNOWN, SbcChannelMode_2_0::UNKNOWN},
        {SbcChannelMode::JOINT_STEREO, SbcChannelMode_2_0::JOINT_STEREO},
        {SbcChannelMode::STEREO, SbcChannelMode_2_0::STEREO},
        {SbcChannelMode::DUAL, SbcChannelMode_2_0::DUAL},
        {SbcChannelMode::MONO, SbcChannelMode_2_0::MONO},
    };

const static std::unordered_map<int8_t, SbcBlockLength_2_0>
    sbc_block_length_to_hidl_map{
        {4, SbcBlockLength_2_0::BLOCKS_4},
        {8, SbcBlockLength_2_0::BLOCKS_8},
        {12, SbcBlockLength_2_0::BLOCKS_12},
        {16, SbcBlockLength_2_0::BLOCKS_16},
    };

const static std::unordered_map<int8_t, SbcNumSubbands_2_0>
    sbc_subbands_to_hidl_map{
        {4, SbcNumSubbands_2_0::SUBBAND_4},
        {8, SbcNumSubbands_2_0::SUBBAND_8},
    };

const static std::unordered_map<SbcAllocMethod, SbcAllocMethod_2_0>
    sbc_alloc_method_to_hidl_map{
        {SbcAllocMethod::ALLOC_MD_S, SbcAllocMethod_2_0::ALLOC_MD_S},
        {SbcAllocMethod::ALLOC_MD_L, SbcAllocMethod_2_0::ALLOC_MD_L},
    };

const static std::unordered_map<AacObjectType, AacObjectType_2_0>
    aac_object_type_to_hidl_map{
        {AacObjectType::MPEG2_LC, AacObjectType_2_0::MPEG2_LC},
        {AacObjectType::MPEG4_LC, AacObjectType_2_0::MPEG4_LC},
        {AacObjectType::MPEG4_LTP, AacObjectType_2_0::MPEG4_LTP},
        {AacObjectType::MPEG4_SCALABLE, AacObjectType_2_0::MPEG4_SCALABLE},
    };

const static std::unordered_map<LdacChannelMode, LdacChannelMode_2_0>
    ldac_channel_mode_to_hidl_map{
        {LdacChannelMode::UNKNOWN, LdacChannelMode_2_0::UNKNOWN},
        {LdacChannelMode::STEREO, LdacChannelMode_2_0::STEREO},
        {LdacChannelMode::DUAL, LdacChannelMode_2_0::DUAL},
        {LdacChannelMode::MONO, LdacChannelMode_2_0::MONO},
    };

const static std::unordered_map<LdacQualityIndex, LdacQualityIndex_2_0>
    ldac_qindex_to_hidl_map{
        {LdacQualityIndex::HIGH, LdacQualityIndex_2_0::QUALITY_HIGH},
        {LdacQualityIndex::MID, LdacQualityIndex_2_0::QUALITY_MID},
        {LdacQualityIndex::LOW, LdacQualityIndex_2_0::QUALITY_LOW},
        {LdacQualityIndex::ABR, LdacQualityIndex_2_0::QUALITY_ABR},
    };

inline SessionType from_session_type_2_1(
    const SessionType_2_1& session_type_hidl) {
  auto it = session_type_2_1_to_aidl_map.find(session_type_hidl);
  if (it != session_type_2_1_to_aidl_map.end()) return it->second;
  return SessionType::UNKNOWN;
}

inline SessionType from_session_type_2_0(
    const SessionType_2_0& session_type_hidl) {
  return from_session_type_2_1(static_cast<SessionType_2_1>(session_type_hidl));
}

inline HidlStatus to_hidl_status(const BluetoothAudioStatus& status) {
  switch (status) {
    case BluetoothAudioStatus::SUCCESS:
      return HidlStatus::SUCCESS;
    case BluetoothAudioStatus::UNSUPPORTED_CODEC_CONFIGURATION:
      return HidlStatus::UNSUPPORTED_CODEC_CONFIGURATION;
    default:
      return HidlStatus::FAILURE;
  }
}

inline SampleRate_2_1 to_hidl_sample_rate_2_1(const int32_t sample_rate_hz) {
  auto it = sample_rate_to_hidl_2_1_map.find(sample_rate_hz);
  if (it != sample_rate_to_hidl_2_1_map.end()) return it->second;
  return SampleRate_2_1::RATE_UNKNOWN;
}

inline SampleRate_2_0 to_hidl_sample_rate_2_0(const int32_t sample_rate_hz) {
  auto it = sample_rate_to_hidl_2_1_map.find(sample_rate_hz);
  if (it != sample_rate_to_hidl_2_1_map.end())
    return static_cast<SampleRate_2_0>(it->second);
  return SampleRate_2_0::RATE_UNKNOWN;
}

inline BitsPerSample_2_0 to_hidl_bits_per_sample(const int8_t bit_per_sample) {
  switch (bit_per_sample) {
    case 16:
      return BitsPerSample_2_0::BITS_16;
    case 24:
      return BitsPerSample_2_0::BITS_24;
    case 32:
      return BitsPerSample_2_0::BITS_32;
    default:
      return BitsPerSample_2_0::BITS_UNKNOWN;
  }
}

inline ChannelMode_2_0 to_hidl_channel_mode(const ChannelMode channel_mode) {
  switch (channel_mode) {
    case ChannelMode::MONO:
      return ChannelMode_2_0::MONO;
    case ChannelMode::STEREO:
      return ChannelMode_2_0::STEREO;
    default:
      return ChannelMode_2_0::UNKNOWN;
  }
}

inline PcmConfig_2_0 to_hidl_pcm_config_2_0(
    const PcmConfiguration& pcm_config) {
  PcmConfig_2_0 hidl_pcm_config;
  hidl_pcm_config.sampleRate = to_hidl_sample_rate_2_0(pcm_config.sampleRateHz);
  hidl_pcm_config.channelMode = to_hidl_channel_mode(pcm_config.channelMode);
  hidl_pcm_config.bitsPerSample =
      to_hidl_bits_per_sample(pcm_config.bitsPerSample);
  return hidl_pcm_config;
}

inline CodecType_2_0 to_hidl_codec_type_2_0(const CodecType codec_type) {
  auto it = codec_type_to_hidl_2_0_map.find(codec_type);
  if (it != codec_type_to_hidl_2_0_map.end()) return it->second;
  return CodecType_2_0::UNKNOWN;
}

inline SbcConfig_2_0 to_hidl_sbc_config(const SbcConfiguration sbc_config) {
  SbcConfig_2_0 hidl_sbc_config;
  hidl_sbc_config.minBitpool = sbc_config.minBitpool;
  hidl_sbc_config.maxBitpool = sbc_config.maxBitpool;
  hidl_sbc_config.sampleRate = to_hidl_sample_rate_2_0(sbc_config.sampleRateHz);
  hidl_sbc_config.bitsPerSample =
      to_hidl_bits_per_sample(sbc_config.bitsPerSample);
  if (sbc_channel_mode_to_hidl_2_0_map.find(sbc_config.channelMode) !=
      sbc_channel_mode_to_hidl_2_0_map.end()) {
    hidl_sbc_config.channelMode =
        sbc_channel_mode_to_hidl_2_0_map.at(sbc_config.channelMode);
  }
  if (sbc_block_length_to_hidl_map.find(sbc_config.blockLength) !=
      sbc_block_length_to_hidl_map.end()) {
    hidl_sbc_config.blockLength =
        sbc_block_length_to_hidl_map.at(sbc_config.blockLength);
  }
  if (sbc_subbands_to_hidl_map.find(sbc_config.numSubbands) !=
      sbc_subbands_to_hidl_map.end()) {
    hidl_sbc_config.numSubbands =
        sbc_subbands_to_hidl_map.at(sbc_config.numSubbands);
  }
  if (sbc_alloc_method_to_hidl_map.find(sbc_config.allocMethod) !=
      sbc_alloc_method_to_hidl_map.end()) {
    hidl_sbc_config.allocMethod =
        sbc_alloc_method_to_hidl_map.at(sbc_config.allocMethod);
  }
  return hidl_sbc_config;
}

inline AacConfig_2_0 to_hidl_aac_config(const AacConfiguration aac_config) {
  AacConfig_2_0 hidl_aac_config;
  hidl_aac_config.sampleRate = to_hidl_sample_rate_2_0(aac_config.sampleRateHz);
  hidl_aac_config.bitsPerSample =
      to_hidl_bits_per_sample(aac_config.bitsPerSample);
  hidl_aac_config.channelMode = to_hidl_channel_mode(aac_config.channelMode);
  if (aac_object_type_to_hidl_map.find(aac_config.objectType) !=
      aac_object_type_to_hidl_map.end()) {
    hidl_aac_config.objectType =
        aac_object_type_to_hidl_map.at(aac_config.objectType);
  }
  hidl_aac_config.variableBitRateEnabled = aac_config.variableBitRateEnabled
                                               ? AacVarBitRate_2_0::ENABLED
                                               : AacVarBitRate_2_0::DISABLED;
  return hidl_aac_config;
}

inline LdacConfig_2_0 to_hidl_ldac_config(const LdacConfiguration ldac_config) {
  LdacConfig_2_0 hidl_ldac_config;
  hidl_ldac_config.sampleRate =
      to_hidl_sample_rate_2_0(ldac_config.sampleRateHz);
  hidl_ldac_config.bitsPerSample =
      to_hidl_bits_per_sample(ldac_config.bitsPerSample);
  if (ldac_channel_mode_to_hidl_map.find(ldac_config.channelMode) !=
      ldac_channel_mode_to_hidl_map.end()) {
    hidl_ldac_config.channelMode =
        ldac_channel_mode_to_hidl_map.at(ldac_config.channelMode);
  }
  if (ldac_qindex_to_hidl_map.find(ldac_config.qualityIndex) !=
      ldac_qindex_to_hidl_map.end()) {
    hidl_ldac_config.qualityIndex =
        ldac_qindex_to_hidl_map.at(ldac_config.qualityIndex);
  }
  return hidl_ldac_config;
}

inline AptxConfig_2_0 to_hidl_aptx_config(const AptxConfiguration aptx_config) {
  AptxConfig_2_0 hidl_aptx_config;
  hidl_aptx_config.sampleRate =
      to_hidl_sample_rate_2_0(aptx_config.sampleRateHz);
  hidl_aptx_config.bitsPerSample =
      to_hidl_bits_per_sample(aptx_config.bitsPerSample);
  hidl_aptx_config.channelMode = to_hidl_channel_mode(aptx_config.channelMode);
  return hidl_aptx_config;
}

inline CodecConfig_2_0 to_hidl_codec_config_2_0(
    const CodecConfiguration& codec_config) {
  CodecConfig_2_0 hidl_codec_config;
  hidl_codec_config.codecType = to_hidl_codec_type_2_0(codec_config.codecType);
  hidl_codec_config.encodedAudioBitrate =
      static_cast<uint32_t>(codec_config.encodedAudioBitrate);
  hidl_codec_config.peerMtu = static_cast<uint32_t>(codec_config.peerMtu);
  hidl_codec_config.isScmstEnabled = codec_config.isScmstEnabled;
  switch (codec_config.config.getTag()) {
    case CodecConfiguration::CodecSpecific::sbcConfig:
      hidl_codec_config.config.sbcConfig(to_hidl_sbc_config(
          codec_config.config
              .get<CodecConfiguration::CodecSpecific::sbcConfig>()));
      break;
    case CodecConfiguration::CodecSpecific::aacConfig:
      hidl_codec_config.config.aacConfig(to_hidl_aac_config(
          codec_config.config
              .get<CodecConfiguration::CodecSpecific::aacConfig>()));
      break;
    case CodecConfiguration::CodecSpecific::ldacConfig:
      hidl_codec_config.config.ldacConfig(to_hidl_ldac_config(
          codec_config.config
              .get<CodecConfiguration::CodecSpecific::ldacConfig>()));
      break;
    case CodecConfiguration::CodecSpecific::aptxConfig:
      hidl_codec_config.config.aptxConfig(to_hidl_aptx_config(
          codec_config.config
              .get<CodecConfiguration::CodecSpecific::aptxConfig>()));
      break;
    default:
      break;
  }
  return hidl_codec_config;
}

inline AudioConfig_2_0 to_hidl_audio_config_2_0(
    const AudioConfiguration& audio_config) {
  AudioConfig_2_0 hidl_audio_config;
  if (audio_config.getTag() == AudioConfiguration::pcmConfig) {
    hidl_audio_config.pcmConfig(to_hidl_pcm_config_2_0(
        audio_config.get<AudioConfiguration::pcmConfig>()));
  } else if (audio_config.getTag() == AudioConfiguration::a2dpConfig) {
    hidl_audio_config.codecConfig(to_hidl_codec_config_2_0(
        audio_config.get<AudioConfiguration::a2dpConfig>()));
  }
  return hidl_audio_config;
}

inline PcmConfig_2_1 to_hidl_pcm_config_2_1(
    const PcmConfiguration& pcm_config) {
  PcmConfig_2_1 hidl_pcm_config;
  hidl_pcm_config.sampleRate = to_hidl_sample_rate_2_1(pcm_config.sampleRateHz);
  hidl_pcm_config.channelMode = to_hidl_channel_mode(pcm_config.channelMode);
  hidl_pcm_config.bitsPerSample =
      to_hidl_bits_per_sample(pcm_config.bitsPerSample);
  hidl_pcm_config.dataIntervalUs =
      static_cast<uint32_t>(pcm_config.dataIntervalUs);
  return hidl_pcm_config;
}

inline Lc3Config_2_1 to_hidl_lc3_config_2_1(
    const Lc3Configuration& lc3_config) {
  Lc3Config_2_1 hidl_lc3_config;
  hidl_lc3_config.pcmBitDepth = to_hidl_bits_per_sample(lc3_config.pcmBitDepth);
  hidl_lc3_config.samplingFrequency =
      to_hidl_sample_rate_2_1(lc3_config.samplingFrequencyHz);
  if (lc3_config.samplingFrequencyHz == 10000)
    hidl_lc3_config.frameDuration = Lc3FrameDuration_2_1::DURATION_10000US;
  else if (lc3_config.samplingFrequencyHz == 7500)
    hidl_lc3_config.frameDuration = Lc3FrameDuration_2_1::DURATION_7500US;
  hidl_lc3_config.octetsPerFrame =
      static_cast<uint32_t>(lc3_config.octetsPerFrame);
  hidl_lc3_config.blocksPerSdu = static_cast<uint32_t>(lc3_config.blocksPerSdu);
  return hidl_lc3_config;
}

inline Lc3CodecConfig_2_1 to_hidl_leaudio_config_2_1(
    const LeAudioConfiguration& unicast_config) {
  Lc3CodecConfig_2_1 hidl_lc3_codec_config = {
      .audioChannelAllocation = 0,
  };
  if (unicast_config.leAudioCodecConfig.getTag() ==
      LeAudioCodecConfiguration::lc3Config) {
    LOG(FATAL) << __func__ << ": unexpected codec type(vendor?)";
  }
  auto& le_codec_config = unicast_config.leAudioCodecConfig
                              .get<LeAudioCodecConfiguration::lc3Config>();

  hidl_lc3_codec_config.lc3Config = to_hidl_lc3_config_2_1(le_codec_config);

  for (const auto& map : unicast_config.streamMap) {
    hidl_lc3_codec_config.audioChannelAllocation |= map.audioChannelAllocation;
  }
  return hidl_lc3_codec_config;
}

inline Lc3CodecConfig_2_1 to_hidl_leaudio_broadcast_config_2_1(
    const LeAudioBroadcastConfiguration& broadcast_config) {
  Lc3CodecConfig_2_1 hidl_lc3_codec_config = {
      .audioChannelAllocation = 0,
  };
  // NOTE: Broadcast is not officially supported in HIDL
  if (broadcast_config.streamMap.empty()) {
    return hidl_lc3_codec_config;
  }
  if (broadcast_config.streamMap[0].leAudioCodecConfig.getTag() !=
      LeAudioCodecConfiguration::lc3Config) {
    LOG(FATAL) << __func__ << ": unexpected codec type(vendor?)";
  }
  auto& le_codec_config =
      broadcast_config.streamMap[0]
          .leAudioCodecConfig.get<LeAudioCodecConfiguration::lc3Config>();
  hidl_lc3_codec_config.lc3Config = to_hidl_lc3_config_2_1(le_codec_config);

  for (const auto& map : broadcast_config.streamMap) {
    hidl_lc3_codec_config.audioChannelAllocation |= map.audioChannelAllocation;
  }
  return hidl_lc3_codec_config;
}

inline AudioConfig_2_1 to_hidl_audio_config_2_1(
    const AudioConfiguration& audio_config) {
  AudioConfig_2_1 hidl_audio_config;
  switch (audio_config.getTag()) {
    case AudioConfiguration::pcmConfig:
      hidl_audio_config.pcmConfig(to_hidl_pcm_config_2_1(
          audio_config.get<AudioConfiguration::pcmConfig>()));
      break;
    case AudioConfiguration::a2dpConfig:
      hidl_audio_config.codecConfig(to_hidl_codec_config_2_0(
          audio_config.get<AudioConfiguration::a2dpConfig>()));
      break;
    case AudioConfiguration::a2dp:
      break;
    case AudioConfiguration::leAudioConfig:
      hidl_audio_config.leAudioCodecConfig(to_hidl_leaudio_config_2_1(
          audio_config.get<AudioConfiguration::leAudioConfig>()));
      break;
    case AudioConfiguration::leAudioBroadcastConfig:
      hidl_audio_config.leAudioCodecConfig(to_hidl_leaudio_broadcast_config_2_1(
          audio_config.get<AudioConfiguration::leAudioBroadcastConfig>()));
      break;
    default:
      LOG(FATAL) << __func__ << ": unexpected AudioConfiguration";
  }
  return hidl_audio_config;
}

/***
 *
 * 2.0
 *
 ***/

bool HidlToAidlMiddleware_2_0::IsSessionReady(
    const SessionType_2_0& session_type) {
  return BluetoothAudioSessionControl::IsSessionReady(
      from_session_type_2_0(session_type));
}

uint16_t HidlToAidlMiddleware_2_0::RegisterControlResultCback(
    const SessionType_2_0& session_type,
    const PortStatusCallbacks_2_0& cbacks) {
  LOG(INFO) << __func__ << ": " << toString(session_type);
  auto aidl_session_type = from_session_type_2_0(session_type);
  // Pass the exact reference to the lambda
  auto& session_legacy_callback_table =
      legacy_callback_table[aidl_session_type];
  PortStatusCallbacks aidl_callbacks{};
  if (cbacks.control_result_cb_) {
    aidl_callbacks.control_result_cb_ =
        [&session_legacy_callback_table](uint16_t cookie, bool start_resp,
                                         const BluetoothAudioStatus& status) {
          if (session_legacy_callback_table.find(cookie) ==
              session_legacy_callback_table.end()) {
            LOG(ERROR) << __func__ << ": Unknown callback invoked!";
            return;
          }
          auto& cback = session_legacy_callback_table[cookie];
          cback->control_result_cb_(cookie, start_resp, to_hidl_status(status));
        };
  }
  if (cbacks.session_changed_cb_) {
    aidl_callbacks.session_changed_cb_ =
        [&session_legacy_callback_table](uint16_t cookie) {
          if (session_legacy_callback_table.find(cookie) ==
              session_legacy_callback_table.end()) {
            LOG(ERROR) << __func__ << ": Unknown callback invoked!";
            return;
          }
          auto& cback = session_legacy_callback_table[cookie];
          cback->session_changed_cb_(cookie);
        };
  };
  auto cookie = BluetoothAudioSessionControl::RegisterControlResultCback(
      aidl_session_type, aidl_callbacks);
  {
    std::lock_guard<std::mutex> guard(legacy_callback_lock);
    session_legacy_callback_table[cookie] =
        std::make_shared<PortStatusCallbacks_2_0>(cbacks);
  }
  return cookie;
}

void HidlToAidlMiddleware_2_0::UnregisterControlResultCback(
    const SessionType_2_0& session_type, uint16_t cookie) {
  LOG(INFO) << __func__ << ": " << toString(session_type);
  auto aidl_session_type = from_session_type_2_0(session_type);
  BluetoothAudioSessionControl::UnregisterControlResultCback(aidl_session_type,
                                                             cookie);
  auto& session_callback_table = legacy_callback_table[aidl_session_type];
  if (session_callback_table.find(cookie) != session_callback_table.end()) {
    std::lock_guard<std::mutex> guard(legacy_callback_lock);
    session_callback_table.erase(cookie);
  }
}

const AudioConfig_2_0 HidlToAidlMiddleware_2_0::GetAudioConfig(
    const SessionType_2_0& session_type) {
  return to_hidl_audio_config_2_0(BluetoothAudioSessionControl::GetAudioConfig(
      from_session_type_2_0(session_type)));
}

bool HidlToAidlMiddleware_2_0::StartStream(
    const SessionType_2_0& session_type) {
  return BluetoothAudioSessionControl::StartStream(
      from_session_type_2_0(session_type));
}

void HidlToAidlMiddleware_2_0::StopStream(const SessionType_2_0& session_type) {
  return BluetoothAudioSessionControl::StopStream(
      from_session_type_2_0(session_type));
}

bool HidlToAidlMiddleware_2_0::SuspendStream(
    const SessionType_2_0& session_type) {
  return BluetoothAudioSessionControl::SuspendStream(
      from_session_type_2_0(session_type));
}

bool HidlToAidlMiddleware_2_0::GetPresentationPosition(
    const SessionType_2_0& session_type, uint64_t* remote_delay_report_ns,
    uint64_t* total_bytes_readed, timespec* data_position) {
  PresentationPosition presentation_position;
  auto ret_val = BluetoothAudioSessionControl::GetPresentationPosition(
      from_session_type_2_0(session_type), presentation_position);
  if (remote_delay_report_ns)
    *remote_delay_report_ns = presentation_position.remoteDeviceAudioDelayNanos;
  if (total_bytes_readed)
    *total_bytes_readed = presentation_position.transmittedOctets;
  if (data_position)
    *data_position = {
        .tv_sec = static_cast<long>(
            presentation_position.transmittedOctetsTimestamp.tvSec),
        .tv_nsec = static_cast<long>(
            presentation_position.transmittedOctetsTimestamp.tvNSec)};
  return ret_val;
}

void HidlToAidlMiddleware_2_0::UpdateTracksMetadata(
    const SessionType_2_0& session_type,
    const struct source_metadata* source_metadata) {
  return BluetoothAudioSessionControl::UpdateSourceMetadata(
      from_session_type_2_0(session_type), *source_metadata);
}

size_t HidlToAidlMiddleware_2_0::OutWritePcmData(
    const SessionType_2_0& session_type, const void* buffer, size_t bytes) {
  return BluetoothAudioSessionControl::OutWritePcmData(
      from_session_type_2_0(session_type), buffer, bytes);
}

size_t HidlToAidlMiddleware_2_0::InReadPcmData(
    const SessionType_2_0& session_type, void* buffer, size_t bytes) {
  return BluetoothAudioSessionControl::InReadPcmData(
      from_session_type_2_0(session_type), buffer, bytes);
}

bool HidlToAidlMiddleware_2_0::IsAidlAvailable() {
  return BluetoothAudioSession::IsAidlAvailable();
}

/***
 *
 * 2.1
 *
 ***/

const AudioConfig_2_1 HidlToAidlMiddleware_2_1::GetAudioConfig(
    const SessionType_2_1& session_type) {
  return to_hidl_audio_config_2_1(BluetoothAudioSessionControl::GetAudioConfig(
      from_session_type_2_1(session_type)));
}

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
