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

#define LOG_TAG "BTAudioProviderSessionCodecsDB"

#include "BluetoothAudioSupportedCodecsDB.h"

#include <android-base/logging.h>

namespace android {
namespace bluetooth {
namespace audio {

using ::android::hardware::bluetooth::audio::V2_0::AacObjectType;
using ::android::hardware::bluetooth::audio::V2_0::AacParameters;
using ::android::hardware::bluetooth::audio::V2_0::AacVariableBitRate;
using ::android::hardware::bluetooth::audio::V2_0::AptxParameters;
using ::android::hardware::bluetooth::audio::V2_0::BitsPerSample;
using ::android::hardware::bluetooth::audio::V2_0::ChannelMode;
using ::android::hardware::bluetooth::audio::V2_0::CodecType;
using ::android::hardware::bluetooth::audio::V2_0::LdacChannelMode;
using ::android::hardware::bluetooth::audio::V2_0::LdacParameters;
using ::android::hardware::bluetooth::audio::V2_0::LdacQualityIndex;
using ::android::hardware::bluetooth::audio::V2_0::SampleRate;
using ::android::hardware::bluetooth::audio::V2_0::SbcAllocMethod;
using ::android::hardware::bluetooth::audio::V2_0::SbcBlockLength;
using ::android::hardware::bluetooth::audio::V2_0::SbcChannelMode;
using ::android::hardware::bluetooth::audio::V2_0::SbcNumSubbands;
using ::android::hardware::bluetooth::audio::V2_0::SbcParameters;

// Default Supported PCM Parameters
static const PcmParameters kDefaultSoftwarePcmCapabilities = {
    .sampleRate = static_cast<SampleRate>(
        SampleRate::RATE_44100 | SampleRate::RATE_48000 |
        SampleRate::RATE_88200 | SampleRate::RATE_96000 |
        SampleRate::RATE_16000 | SampleRate::RATE_24000),
    .channelMode =
        static_cast<ChannelMode>(ChannelMode::MONO | ChannelMode::STEREO),
    .bitsPerSample = static_cast<BitsPerSample>(BitsPerSample::BITS_16 |
                                                BitsPerSample::BITS_24 |
                                                BitsPerSample::BITS_32)};

// Default Supported Codecs
// SBC: mSampleRate:(44100), mBitsPerSample:(16), mChannelMode:(MONO|STEREO)
//      all blocks | subbands 8 | Loudness
static const SbcParameters kDefaultOffloadSbcCapability = {
    .sampleRate = SampleRate::RATE_44100,
    .channelMode = static_cast<SbcChannelMode>(SbcChannelMode::MONO |
                                               SbcChannelMode::JOINT_STEREO),
    .blockLength = static_cast<SbcBlockLength>(
        SbcBlockLength::BLOCKS_4 | SbcBlockLength::BLOCKS_8 |
        SbcBlockLength::BLOCKS_12 | SbcBlockLength::BLOCKS_16),
    .numSubbands = SbcNumSubbands::SUBBAND_8,
    .allocMethod = SbcAllocMethod::ALLOC_MD_L,
    .bitsPerSample = BitsPerSample::BITS_16,
    .minBitpool = 2,
    .maxBitpool = 53};

// AAC: mSampleRate:(44100), mBitsPerSample:(16), mChannelMode:(STEREO)
static const AacParameters kDefaultOffloadAacCapability = {
    .objectType = AacObjectType::MPEG2_LC,
    .sampleRate = SampleRate::RATE_44100,
    .channelMode = ChannelMode::STEREO,
    .variableBitRateEnabled = AacVariableBitRate::ENABLED,
    .bitsPerSample = BitsPerSample::BITS_16};

// LDAC: mSampleRate:(44100|48000|88200|96000), mBitsPerSample:(16|24|32),
//       mChannelMode:(DUAL|STEREO)
static const LdacParameters kDefaultOffloadLdacCapability = {
    .sampleRate = static_cast<SampleRate>(
        SampleRate::RATE_44100 | SampleRate::RATE_48000 |
        SampleRate::RATE_88200 | SampleRate::RATE_96000),
    .channelMode = static_cast<LdacChannelMode>(LdacChannelMode::DUAL |
                                                LdacChannelMode::STEREO),
    .qualityIndex = LdacQualityIndex::QUALITY_HIGH,
    .bitsPerSample = static_cast<BitsPerSample>(BitsPerSample::BITS_16 |
                                                BitsPerSample::BITS_24 |
                                                BitsPerSample::BITS_32)};

// aptX: mSampleRate:(44100|48000), mBitsPerSample:(16), mChannelMode:(STEREO)
static const AptxParameters kDefaultOffloadAptxCapability = {
    .sampleRate = static_cast<SampleRate>(SampleRate::RATE_44100 |
                                          SampleRate::RATE_48000),
    .channelMode = ChannelMode::STEREO,
    .bitsPerSample = BitsPerSample::BITS_16,
};

// aptX HD: mSampleRate:(44100|48000), mBitsPerSample:(24),
//          mChannelMode:(STEREO)
static const AptxParameters kDefaultOffloadAptxHdCapability = {
    .sampleRate = static_cast<SampleRate>(SampleRate::RATE_44100 |
                                          SampleRate::RATE_48000),
    .channelMode = ChannelMode::STEREO,
    .bitsPerSample = BitsPerSample::BITS_24,
};

const std::vector<CodecCapabilities> kDefaultOffloadA2dpCodecCapabilities = {
    {.codecType = CodecType::SBC, .capabilities = {}},
    {.codecType = CodecType::AAC, .capabilities = {}},
    {.codecType = CodecType::LDAC, .capabilities = {}},
    {.codecType = CodecType::APTX, .capabilities = {}},
    {.codecType = CodecType::APTX_HD, .capabilities = {}}};

static bool IsSingleBit(uint32_t bitmasks, uint32_t bitfield) {
  bool single = false;
  uint32_t test_bit = 0x00000001;
  while (test_bit <= bitmasks && test_bit <= bitfield) {
    if (bitfield & test_bit && bitmasks & test_bit) {
      if (single) return false;
      single = true;
    }
    if (test_bit == 0x80000000) break;
    test_bit <<= 1;
  }
  return single;
}

static bool IsOffloadSbcConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific);
static bool IsOffloadAacConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific);
static bool IsOffloadLdacConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific);
static bool IsOffloadAptxConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific);
static bool IsOffloadAptxHdConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific);

static bool IsOffloadSbcConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific) {
  if (codec_specific.getDiscriminator() !=
      CodecConfiguration::CodecSpecific::hidl_discriminator::sbcConfig) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  }
  const SbcParameters sbc_data = codec_specific.sbcConfig();
  if (!IsSingleBit(static_cast<uint32_t>(sbc_data.sampleRate), 0xff) ||
      !IsSingleBit(static_cast<uint32_t>(sbc_data.channelMode), 0x0f) ||
      !IsSingleBit(static_cast<uint32_t>(sbc_data.blockLength), 0xf0) ||
      !IsSingleBit(static_cast<uint32_t>(sbc_data.numSubbands), 0x0c) ||
      !IsSingleBit(static_cast<uint32_t>(sbc_data.allocMethod), 0x03) ||
      !IsSingleBit(static_cast<uint32_t>(sbc_data.bitsPerSample), 0x07) ||
      sbc_data.minBitpool > sbc_data.maxBitpool) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  } else if ((sbc_data.sampleRate & kDefaultOffloadSbcCapability.sampleRate) &&
             (sbc_data.channelMode &
              kDefaultOffloadSbcCapability.channelMode) &&
             (sbc_data.blockLength &
              kDefaultOffloadSbcCapability.blockLength) &&
             (sbc_data.numSubbands &
              kDefaultOffloadSbcCapability.numSubbands) &&
             (sbc_data.allocMethod &
              kDefaultOffloadSbcCapability.allocMethod) &&
             (sbc_data.bitsPerSample &
              kDefaultOffloadSbcCapability.bitsPerSample) &&
             (kDefaultOffloadSbcCapability.minBitpool <= sbc_data.minBitpool &&
              sbc_data.maxBitpool <= kDefaultOffloadSbcCapability.maxBitpool)) {
    return true;
  }
  LOG(WARNING) << __func__
               << ": Unsupported CodecSpecific=" << toString(codec_specific);
  return false;
}

static bool IsOffloadAacConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific) {
  if (codec_specific.getDiscriminator() !=
      CodecConfiguration::CodecSpecific::hidl_discriminator::aacConfig) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  }
  const AacParameters aac_data = codec_specific.aacConfig();
  if (!IsSingleBit(static_cast<uint32_t>(aac_data.objectType), 0xf0) ||
      !IsSingleBit(static_cast<uint32_t>(aac_data.sampleRate), 0xff) ||
      !IsSingleBit(static_cast<uint32_t>(aac_data.channelMode), 0x03) ||
      !IsSingleBit(static_cast<uint32_t>(aac_data.bitsPerSample), 0x07)) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  } else if ((aac_data.objectType & kDefaultOffloadAacCapability.objectType) &&
             (aac_data.sampleRate & kDefaultOffloadAacCapability.sampleRate) &&
             (aac_data.channelMode &
              kDefaultOffloadAacCapability.channelMode) &&
             (aac_data.variableBitRateEnabled == AacVariableBitRate::DISABLED ||
              kDefaultOffloadAacCapability.variableBitRateEnabled ==
                  AacVariableBitRate::ENABLED) &&
             (aac_data.bitsPerSample &
              kDefaultOffloadAacCapability.bitsPerSample)) {
    return true;
  }
  LOG(WARNING) << __func__
               << ": Unsupported CodecSpecific=" << toString(codec_specific);
  return false;
}

static bool IsOffloadLdacConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific) {
  if (codec_specific.getDiscriminator() !=
      CodecConfiguration::CodecSpecific::hidl_discriminator::ldacConfig) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  }
  const LdacParameters ldac_data = codec_specific.ldacConfig();
  if (!IsSingleBit(static_cast<uint32_t>(ldac_data.sampleRate), 0xff) ||
      !IsSingleBit(static_cast<uint32_t>(ldac_data.channelMode), 0x07) ||
      (ldac_data.qualityIndex > LdacQualityIndex::QUALITY_LOW &&
       ldac_data.qualityIndex != LdacQualityIndex::QUALITY_ABR) ||
      !IsSingleBit(static_cast<uint32_t>(ldac_data.bitsPerSample), 0x07)) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  } else if ((ldac_data.sampleRate &
              kDefaultOffloadLdacCapability.sampleRate) &&
             (ldac_data.channelMode &
              kDefaultOffloadLdacCapability.channelMode) &&
             (ldac_data.bitsPerSample &
              kDefaultOffloadLdacCapability.bitsPerSample)) {
    return true;
  }
  LOG(WARNING) << __func__
               << ": Unsupported CodecSpecific=" << toString(codec_specific);
  return false;
}

static bool IsOffloadAptxConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific) {
  if (codec_specific.getDiscriminator() !=
      CodecConfiguration::CodecSpecific::hidl_discriminator::aptxConfig) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  }
  const AptxParameters aptx_data = codec_specific.aptxConfig();
  if (!IsSingleBit(static_cast<uint32_t>(aptx_data.sampleRate), 0xff) ||
      !IsSingleBit(static_cast<uint32_t>(aptx_data.channelMode), 0x03) ||
      !IsSingleBit(static_cast<uint32_t>(aptx_data.bitsPerSample), 0x07)) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  } else if ((aptx_data.sampleRate &
              kDefaultOffloadAptxCapability.sampleRate) &&
             (aptx_data.channelMode &
              kDefaultOffloadAptxCapability.channelMode) &&
             (aptx_data.bitsPerSample &
              kDefaultOffloadAptxCapability.bitsPerSample)) {
    return true;
  }
  LOG(WARNING) << __func__
               << ": Unsupported CodecSpecific=" << toString(codec_specific);
  return false;
}

static bool IsOffloadAptxHdConfigurationValid(
    const CodecConfiguration::CodecSpecific& codec_specific) {
  if (codec_specific.getDiscriminator() !=
      CodecConfiguration::CodecSpecific::hidl_discriminator::aptxConfig) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  }
  const AptxParameters aptx_data = codec_specific.aptxConfig();
  if (!IsSingleBit(static_cast<uint32_t>(aptx_data.sampleRate), 0xff) ||
      !IsSingleBit(static_cast<uint32_t>(aptx_data.channelMode), 0x03) ||
      !IsSingleBit(static_cast<uint32_t>(aptx_data.bitsPerSample), 0x07)) {
    LOG(WARNING) << __func__
                 << ": Invalid CodecSpecific=" << toString(codec_specific);
    return false;
  } else if ((aptx_data.sampleRate &
              kDefaultOffloadAptxHdCapability.sampleRate) &&
             (aptx_data.channelMode &
              kDefaultOffloadAptxHdCapability.channelMode) &&
             (aptx_data.bitsPerSample &
              kDefaultOffloadAptxHdCapability.bitsPerSample)) {
    return true;
  }
  LOG(WARNING) << __func__
               << ": Unsupported CodecSpecific=" << toString(codec_specific);
  return false;
}

std::vector<PcmParameters> GetSoftwarePcmCapabilities() {
  return std::vector<PcmParameters>(1, kDefaultSoftwarePcmCapabilities);
}

std::vector<CodecCapabilities> GetOffloadCodecCapabilities(
    const SessionType& session_type) {
  if (session_type != SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH) {
    return std::vector<CodecCapabilities>(0);
  }
  std::vector<CodecCapabilities> offload_a2dp_codec_capabilities =
      kDefaultOffloadA2dpCodecCapabilities;
  for (auto& codec_capability : offload_a2dp_codec_capabilities) {
    switch (codec_capability.codecType) {
      case CodecType::SBC:
        codec_capability.capabilities.sbcCapabilities(
            kDefaultOffloadSbcCapability);
        break;
      case CodecType::AAC:
        codec_capability.capabilities.aacCapabilities(
            kDefaultOffloadAacCapability);
        break;
      case CodecType::LDAC:
        codec_capability.capabilities.ldacCapabilities(
            kDefaultOffloadLdacCapability);
        break;
      case CodecType::APTX:
        codec_capability.capabilities.aptxCapabilities(
            kDefaultOffloadAptxCapability);
        break;
      case CodecType::APTX_HD:
        codec_capability.capabilities.aptxCapabilities(
            kDefaultOffloadAptxHdCapability);
        break;
      case CodecType::UNKNOWN:
        codec_capability = {};
        break;
    }
  }
  return offload_a2dp_codec_capabilities;
}

bool IsSoftwarePcmConfigurationValid(const PcmParameters& pcm_config) {
  if ((pcm_config.sampleRate != SampleRate::RATE_44100 &&
       pcm_config.sampleRate != SampleRate::RATE_48000 &&
       pcm_config.sampleRate != SampleRate::RATE_88200 &&
       pcm_config.sampleRate != SampleRate::RATE_96000 &&
       pcm_config.sampleRate != SampleRate::RATE_16000 &&
       pcm_config.sampleRate != SampleRate::RATE_24000) ||
      (pcm_config.bitsPerSample != BitsPerSample::BITS_16 &&
       pcm_config.bitsPerSample != BitsPerSample::BITS_24 &&
       pcm_config.bitsPerSample != BitsPerSample::BITS_32) ||
      (pcm_config.channelMode != ChannelMode::MONO &&
       pcm_config.channelMode != ChannelMode::STEREO)) {
    LOG(WARNING) << __func__
                 << ": Invalid PCM Configuration=" << toString(pcm_config);
    return false;
  } else if (pcm_config.sampleRate &
                 kDefaultSoftwarePcmCapabilities.sampleRate &&
             pcm_config.bitsPerSample &
                 kDefaultSoftwarePcmCapabilities.bitsPerSample &&
             pcm_config.channelMode &
                 kDefaultSoftwarePcmCapabilities.channelMode) {
    return true;
  }
  LOG(WARNING) << __func__
               << ": Unsupported PCM Configuration=" << toString(pcm_config);
  return false;
}

bool IsOffloadCodecConfigurationValid(const SessionType& session_type,
                                      const CodecConfiguration& codec_config) {
  if (session_type != SessionType::A2DP_HARDWARE_OFFLOAD_DATAPATH) {
    LOG(ERROR) << __func__
               << ": Invalid SessionType=" << toString(session_type);
    return false;
  } else if (codec_config.encodedAudioBitrate < 0x00000001 ||
             0x00ffffff < codec_config.encodedAudioBitrate) {
    LOG(ERROR) << __func__ << ": Unsupported Codec Configuration="
               << toString(codec_config);
    return false;
  }
  const CodecConfiguration::CodecSpecific& codec_specific = codec_config.config;
  switch (codec_config.codecType) {
    case CodecType::SBC:
      if (IsOffloadSbcConfigurationValid(codec_specific)) {
        return true;
      }
      return false;
    case CodecType::AAC:
      if (IsOffloadAacConfigurationValid(codec_specific)) {
        return true;
      }
      return false;
    case CodecType::LDAC:
      if (IsOffloadLdacConfigurationValid(codec_specific)) {
        return true;
      }
      return false;
    case CodecType::APTX:
      if (IsOffloadAptxConfigurationValid(codec_specific)) {
        return true;
      }
      return false;
    case CodecType::APTX_HD:
      if (IsOffloadAptxHdConfigurationValid(codec_specific)) {
        return true;
      }
      return false;
    case CodecType::UNKNOWN:
      return false;
  }
  return false;
}

}  // namespace audio
}  // namespace bluetooth
}  // namespace android
