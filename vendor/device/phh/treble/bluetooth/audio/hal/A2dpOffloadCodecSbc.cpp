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

#include "A2dpOffloadCodecSbc.h"

#include <algorithm>

#include "A2dpBits.h"

namespace aidl::android::hardware::bluetooth::audio {

/**
 * SBC Local Capabilities
 */

enum : bool {
  kEnableSamplingFrequency44100 = true,
  kEnableSamplingFrequency48000 = true,
};

enum : bool {
  kEnableChannelModeMono = true,
  kEnableChannelModeDualChannel = true,
  kEnableChannelModeStereo = true,
  kEnableChannelModeJointStereo = true,
};

enum : bool {
  kEnableBlockLength4 = true,
  kEnableBlockLength8 = true,
  kEnableBlockLength12 = true,
  kEnableBlockLength16 = true,
};

enum : bool {
  kEnableSubbands4 = true,
  kEnableSubbands8 = true,
};

enum : bool {
  kEnableAllocationMethodSnr = true,
  kEnableAllocationMethodLoudness = true,
};

enum : uint8_t {
  kDefaultMinimumBitpool = 2,
  kDefaultMaximumBitpool = 250,
};

enum : int {
  kBitdepth = 16,
};

/**
 * SBC Signaling format [A2DP - 4.3]
 */

// clang-format off

constexpr A2dpBits::Range kSamplingFrequency (  0,  3 );
constexpr A2dpBits::Range kChannelMode       (  4,  7 );
constexpr A2dpBits::Range kBlockLength       (  8, 11 );
constexpr A2dpBits::Range kSubbands          ( 12, 13 );
constexpr A2dpBits::Range kAllocationMethod  ( 14, 15 );
constexpr A2dpBits::Range kMinimumBitpool    ( 16, 23 );
constexpr A2dpBits::Range kMaximumBitpool    ( 24, 31 );
constexpr size_t kCapabilitiesSize = 32/8;

// clang-format on

enum {
  kSamplingFrequency16000 = kSamplingFrequency.first,
  kSamplingFrequency32000,
  kSamplingFrequency44100,
  kSamplingFrequency48000
};

enum {
  kChannelModeMono = kChannelMode.first,
  kChannelModeDualChannel,
  kChannelModeStereo,
  kChannelModeJointStereo
};

enum {
  kBlockLength4 = kBlockLength.first,
  kBlockLength8,
  kBlockLength12,
  kBlockLength16
};

enum { kSubbands8 = kSubbands.first, kSubbands4 };

enum {
  kAllocationMethodSnr = kAllocationMethod.first,
  kAllocationMethodLoudness
};

/**
 * SBC Conversion functions
 */

static int GetSamplingFrequencyBit(int32_t sampling_frequency) {
  switch (sampling_frequency) {
    case 16000:
      return kSamplingFrequency16000;
    case 32000:
      return kSamplingFrequency32000;
    case 44100:
      return kSamplingFrequency44100;
    case 48000:
      return kSamplingFrequency48000;
    default:
      return -1;
  }
}

static int32_t GetSamplingFrequencyValue(int sampling_frequency) {
  switch (sampling_frequency) {
    case kSamplingFrequency16000:
      return 16000;
    case kSamplingFrequency32000:
      return 32000;
    case kSamplingFrequency44100:
      return 44100;
    case kSamplingFrequency48000:
      return 48000;
    default:
      return 0;
  }
}

static int GetChannelModeBit(ChannelMode channel_mode) {
  switch (channel_mode) {
    case ChannelMode::STEREO:
      return kChannelModeJointStereo | kChannelModeStereo;
    case ChannelMode::DUALMONO:
      return kChannelModeDualChannel;
    case ChannelMode::MONO:
      return kChannelModeMono;
    default:
      return -1;
  }
}

static ChannelMode GetChannelModeEnum(int channel_mode) {
  switch (channel_mode) {
    case kChannelModeMono:
      return ChannelMode::MONO;
    case kChannelModeDualChannel:
      return ChannelMode::DUALMONO;
    case kChannelModeStereo:
    case kChannelModeJointStereo:
      return ChannelMode::STEREO;
    default:
      return ChannelMode::UNKNOWN;
  }
}

static int32_t GetBlockLengthValue(int block_length) {
  switch (block_length) {
    case kBlockLength4:
      return 4;
    case kBlockLength8:
      return 8;
    case kBlockLength12:
      return 12;
    case kBlockLength16:
      return 16;
    default:
      return 0;
  }
}

static int32_t GetSubbandsValue(int subbands) {
  switch (subbands) {
    case kSubbands4:
      return 4;
    case kSubbands8:
      return 8;
    default:
      return 0;
  }
}

static SbcParameters::AllocationMethod GetAllocationMethodEnum(
    int allocation_method) {
  switch (allocation_method) {
    case kAllocationMethodSnr:
      return SbcParameters::AllocationMethod::SNR;
    case kAllocationMethodLoudness:
    default:
      return SbcParameters::AllocationMethod::LOUDNESS;
  }
}

static int32_t GetSamplingFrequencyValue(const A2dpBits& configuration) {
  return GetSamplingFrequencyValue(
      configuration.find_active_bit(kSamplingFrequency));
}

static int32_t GetBlockLengthValue(const A2dpBits& configuration) {
  return GetBlockLengthValue(configuration.find_active_bit(kBlockLength));
}

static int32_t GetSubbandsValue(const A2dpBits& configuration) {
  return GetSubbandsValue(configuration.find_active_bit(kSubbands));
}

static int GetFrameSize(const A2dpBits& configuration, int bitpool) {
  const int kSbcHeaderSize = 4;
  int subbands = GetSubbandsValue(configuration);
  int blocks = GetBlockLengthValue(configuration);

  unsigned bits =
      ((4 * subbands) << !configuration.get(kChannelModeMono)) +
      ((blocks * bitpool) << configuration.get(kChannelModeDualChannel)) +
      ((configuration.get(kChannelModeJointStereo) ? subbands : 0));

  return kSbcHeaderSize + ((bits + 7) >> 3);
}

static int GetBitrate(const A2dpBits& configuration, int bitpool) {
  int sampling_frequency = GetSamplingFrequencyValue(configuration);
  int subbands = GetSubbandsValue(configuration);
  int blocks = GetBlockLengthValue(configuration);
  int bits = 8 * GetFrameSize(configuration, bitpool);

  return (bits * sampling_frequency) / (blocks * subbands);
}

static uint8_t GetBitpool(const A2dpBits& configuration, int bitrate) {
  int bitpool = 0;

  for (int i = 128; i; i >>= 1)
    if (bitrate > GetBitrate(configuration, bitpool + i)) {
      bitpool += i;
    }

  return std::clamp(bitpool, 2, 250);
}

/**
 * SBC Class implementation
 */

A2dpOffloadCodecSbc::A2dpOffloadCodecSbc()
    : A2dpOffloadCodec(info_),
      info_({.id = CodecId(CodecId::A2dp::SBC), .name = "SBC"}) {
  info_.transport.set<CodecInfo::Transport::Tag::a2dp>();
  auto& a2dp_info = info_.transport.get<CodecInfo::Transport::Tag::a2dp>();

  /* --- Setup Capabilities --- */

  a2dp_info.capabilities.resize(kCapabilitiesSize);
  std::fill(begin(a2dp_info.capabilities), end(a2dp_info.capabilities), 0);

  auto capabilities = A2dpBits(a2dp_info.capabilities);

  capabilities.set(kSamplingFrequency44100, kEnableSamplingFrequency44100);
  capabilities.set(kSamplingFrequency48000, kEnableSamplingFrequency48000);

  capabilities.set(kChannelModeMono, kEnableChannelModeMono);
  capabilities.set(kChannelModeDualChannel, kEnableChannelModeDualChannel);
  capabilities.set(kChannelModeStereo, kEnableChannelModeStereo);
  capabilities.set(kChannelModeJointStereo, kEnableChannelModeJointStereo);

  capabilities.set(kBlockLength4, kEnableBlockLength4);
  capabilities.set(kBlockLength8, kEnableBlockLength8);
  capabilities.set(kBlockLength12, kEnableBlockLength12);
  capabilities.set(kBlockLength16, kEnableBlockLength16);

  capabilities.set(kSubbands4, kEnableSubbands4);
  capabilities.set(kSubbands8, kEnableSubbands8);

  capabilities.set(kSubbands4, kEnableSubbands4);
  capabilities.set(kSubbands8, kEnableSubbands8);

  capabilities.set(kAllocationMethodSnr, kEnableAllocationMethodSnr);
  capabilities.set(kAllocationMethodLoudness, kEnableAllocationMethodLoudness);

  capabilities.set(kMinimumBitpool, kDefaultMinimumBitpool);
  capabilities.set(kMaximumBitpool, kDefaultMaximumBitpool);

  /* --- Setup Sampling Frequencies --- */

  auto& sampling_frequency = a2dp_info.samplingFrequencyHz;

  for (auto v : {16000, 32000, 44100, 48000})
    if (capabilities.get(GetSamplingFrequencyBit(int32_t(v))))
      sampling_frequency.push_back(v);

  /* --- Setup Channel Modes --- */

  auto& channel_modes = a2dp_info.channelMode;

  for (auto v : {ChannelMode::MONO, ChannelMode::DUALMONO, ChannelMode::STEREO})
    if (capabilities.get(GetChannelModeBit(v))) channel_modes.push_back(v);

  /* --- Setup Bitdepth --- */

  a2dp_info.bitdepth.push_back(kBitdepth);
}

A2dpStatus A2dpOffloadCodecSbc::ParseConfiguration(
    const std::vector<uint8_t>& configuration,
    CodecParameters* codec_parameters, SbcParameters* sbc_parameters) const {
  auto& a2dp_info = info.transport.get<CodecInfo::Transport::Tag::a2dp>();

  if (configuration.size() != a2dp_info.capabilities.size())
    return A2dpStatus::BAD_LENGTH;

  auto config = A2dpBits(configuration);
  auto lcaps = A2dpBits(a2dp_info.capabilities);

  /* --- Check Sampling Frequency --- */

  int sampling_frequency = config.find_active_bit(kSamplingFrequency);
  if (sampling_frequency < 0) return A2dpStatus::INVALID_SAMPLING_FREQUENCY;
  if (!lcaps.get(sampling_frequency))
    return A2dpStatus::NOT_SUPPORTED_SAMPLING_FREQUENCY;

  /* --- Check Channel Mode --- */

  int channel_mode = config.find_active_bit(kChannelMode);
  if (channel_mode < 0) return A2dpStatus::INVALID_CHANNEL_MODE;
  if (!lcaps.get(channel_mode)) return A2dpStatus::NOT_SUPPORTED_CHANNEL_MODE;

  /* --- Check Block Length --- */

  int block_length = config.find_active_bit(kBlockLength);
  if (block_length < 0) return A2dpStatus::INVALID_BLOCK_LENGTH;

  /* --- Check Subbands --- */

  int subbands = config.find_active_bit(kSubbands);
  if (subbands < 0) return A2dpStatus::INVALID_SUBBANDS;
  if (!lcaps.get(subbands)) return A2dpStatus::NOT_SUPPORTED_SUBBANDS;

  /* --- Check Allocation Method --- */

  int allocation_method = config.find_active_bit(kAllocationMethod);
  if (allocation_method < 0) return A2dpStatus::INVALID_ALLOCATION_METHOD;
  if (!lcaps.get(allocation_method))
    return A2dpStatus::NOT_SUPPORTED_ALLOCATION_METHOD;

  /* --- Check Bitpool --- */

  uint8_t min_bitpool = config.get(kMinimumBitpool);
  if (min_bitpool < 2 || min_bitpool > 250)
    return A2dpStatus::INVALID_MINIMUM_BITPOOL_VALUE;
  if (min_bitpool < lcaps.get(kMinimumBitpool))
    return A2dpStatus::NOT_SUPPORTED_MINIMUM_BITPOOL_VALUE;

  uint8_t max_bitpool = config.get(kMaximumBitpool);
  if (max_bitpool < 2 || max_bitpool > 250)
    return A2dpStatus::INVALID_MAXIMUM_BITPOOL_VALUE;
  if (max_bitpool > lcaps.get(kMaximumBitpool))
    return A2dpStatus::NOT_SUPPORTED_MAXIMUM_BITPOOL_VALUE;

  /* --- Return --- */

  codec_parameters->channelMode = GetChannelModeEnum(channel_mode);
  codec_parameters->samplingFrequencyHz =
      GetSamplingFrequencyValue(sampling_frequency);
  codec_parameters->bitdepth = kBitdepth;

  codec_parameters->minBitrate = GetBitrate(config, min_bitpool);
  codec_parameters->maxBitrate = GetBitrate(config, max_bitpool);

  if (sbc_parameters) {
    sbc_parameters->block_length = GetBlockLengthValue(block_length);
    sbc_parameters->subbands = GetSubbandsValue(subbands);
    sbc_parameters->allocation_method =
        GetAllocationMethodEnum(allocation_method);
    sbc_parameters->min_bitpool = min_bitpool;
    sbc_parameters->max_bitpool = max_bitpool;
  }

  return A2dpStatus::OK;
}

bool A2dpOffloadCodecSbc::BuildConfiguration(
    const std::vector<uint8_t>& remote_capabilities,
    const std::optional<CodecParameters>& hint,
    std::vector<uint8_t>* configuration) const {
  auto& a2dp_info = info.transport.get<CodecInfo::Transport::Tag::a2dp>();

  if (remote_capabilities.size() != a2dp_info.capabilities.size()) return false;

  auto lcaps = A2dpBits(a2dp_info.capabilities);
  auto rcaps = A2dpBits(remote_capabilities);

  configuration->resize(a2dp_info.capabilities.size());
  std::fill(begin(*configuration), end(*configuration), 0);
  auto config = A2dpBits(*configuration);

  /* --- Select Sampling Frequency --- */

  auto sf_hint = hint ? GetSamplingFrequencyBit(hint->samplingFrequencyHz) : -1;

  if (sf_hint >= 0 && lcaps.get(sf_hint) && rcaps.get(sf_hint))
    config.set(sf_hint);
  else if (lcaps.get(kSamplingFrequency44100) &&
           rcaps.get(kSamplingFrequency44100))
    config.set(kSamplingFrequency44100);
  else if (lcaps.get(kSamplingFrequency48000) &&
           rcaps.get(kSamplingFrequency48000))
    config.set(kSamplingFrequency48000);
  else
    return false;

  /* --- Select Channel Mode --- */

  auto cm_hint = hint ? GetChannelModeBit(hint->channelMode) : -1;

  if (cm_hint >= 0 && lcaps.get(cm_hint) && rcaps.get(cm_hint))
    config.set(cm_hint);
  else if (lcaps.get(kChannelModeJointStereo) &&
           rcaps.get(kChannelModeJointStereo))
    config.set(kChannelModeJointStereo);
  else if (lcaps.get(kChannelModeStereo) && rcaps.get(kChannelModeStereo))
    config.set(kChannelModeStereo);
  else if (lcaps.get(kChannelModeDualChannel) &&
           rcaps.get(kChannelModeDualChannel))
    config.set(kChannelModeDualChannel);
  else if (lcaps.get(kChannelModeMono) && rcaps.get(kChannelModeMono))
    config.set(kChannelModeMono);
  else
    return false;

  /* --- Select Block Length --- */

  if (lcaps.get(kBlockLength16) && rcaps.get(kBlockLength16))
    config.set(kBlockLength16);
  else if (lcaps.get(kBlockLength12) && rcaps.get(kBlockLength12))
    config.set(kBlockLength12);
  else if (lcaps.get(kBlockLength8) && rcaps.get(kBlockLength8))
    config.set(kBlockLength8);
  else if (lcaps.get(kBlockLength4) && rcaps.get(kBlockLength4))
    config.set(kBlockLength4);
  else
    return false;

  /* --- Select Subbands --- */

  if (lcaps.get(kSubbands8) && rcaps.get(kSubbands8))
    config.set(kSubbands8);
  else if (lcaps.get(kSubbands4) && rcaps.get(kSubbands4))
    config.set(kSubbands4);
  else
    return false;

  /* --- Select Allocation method --- */

  if (lcaps.get(kAllocationMethodLoudness) &&
      rcaps.get(kAllocationMethodLoudness))
    config.set(kAllocationMethodLoudness);
  else if (lcaps.get(kAllocationMethodSnr) && rcaps.get(kAllocationMethodSnr))
    config.set(kAllocationMethodSnr);
  else
    return false;

  /* --- Select Bitpool --- */

  uint8_t min_bitpool = rcaps.get(kMinimumBitpool);
  uint8_t max_bitpool = rcaps.get(kMaximumBitpool);

  if (min_bitpool < 2 || min_bitpool > 250 || max_bitpool < 2 ||
      max_bitpool > 250 || min_bitpool > max_bitpool) {
    min_bitpool = 2;
    max_bitpool = 250;
  }

  min_bitpool = std::max(min_bitpool, uint8_t(lcaps.get(kMinimumBitpool)));
  max_bitpool = std::max(max_bitpool, uint8_t(lcaps.get(kMaximumBitpool)));

  if (hint) {
    min_bitpool =
        std::max(min_bitpool, GetBitpool(*configuration, hint->minBitrate));
    if (hint->maxBitrate && hint->maxBitrate >= hint->minBitrate)
      max_bitpool =
          std::min(max_bitpool, GetBitpool(*configuration, hint->maxBitrate));
  }

  config.set(kMinimumBitpool, min_bitpool);
  config.set(kMaximumBitpool, max_bitpool);

  return true;
}

}  // namespace aidl::android::hardware::bluetooth::audio
