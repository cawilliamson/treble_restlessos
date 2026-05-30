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

#define STREAM_TO_UINT8(u8, p) \
  {                            \
    (u8) = (uint8_t)(*(p));    \
    (p) += 1;                  \
  }
#define STREAM_TO_UINT16(u16, p)                                  \
  {                                                               \
    (u16) = ((uint16_t)(*(p)) + (((uint16_t)(*((p) + 1))) << 8)); \
    (p) += 2;                                                     \
  }
#define STREAM_TO_UINT32(u32, p)                                      \
  {                                                                   \
    (u32) = (((uint32_t)(*(p))) + ((((uint32_t)(*((p) + 1)))) << 8) + \
             ((((uint32_t)(*((p) + 2)))) << 16) +                     \
             ((((uint32_t)(*((p) + 3)))) << 24));                     \
    (p) += 4;                                                         \
  }

#define LOG_TAG "BTAudioAseConfigAidl"

#include "BluetoothLeAudioAseConfigurationSettingProvider.h"

#include <aidl/android/hardware/bluetooth/audio/AudioConfiguration.h>
#include <aidl/android/hardware/bluetooth/audio/AudioContext.h>
#include <aidl/android/hardware/bluetooth/audio/BluetoothAudioStatus.h>
#include <aidl/android/hardware/bluetooth/audio/CodecId.h>
#include <aidl/android/hardware/bluetooth/audio/CodecSpecificCapabilitiesLtv.h>
#include <aidl/android/hardware/bluetooth/audio/CodecSpecificConfigurationLtv.h>
#include <aidl/android/hardware/bluetooth/audio/ConfigurationFlags.h>
#include <aidl/android/hardware/bluetooth/audio/LeAudioAseConfiguration.h>
#include <aidl/android/hardware/bluetooth/audio/Phy.h>
#include <android-base/logging.h>

#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

/* Internal structure definition */
std::map<std::string,
         std::tuple<std::vector<std::optional<AseDirectionConfiguration>>,
                    std::vector<std::optional<AseDirectionConfiguration>>,
                    ConfigurationFlags>>
    configurations_;

std::vector<LeAudioAseConfigurationSetting> ase_configuration_settings_;

constexpr uint8_t kIsoDataPathHci = 0x00;
constexpr uint8_t kIsoDataPathPlatformDefault = 0x01;
constexpr uint8_t kIsoDataPathDisabled = 0xFF;

constexpr uint8_t kLeAudioDirectionSink = 0x01;
constexpr uint8_t kLeAudioDirectionSource = 0x02;
constexpr uint8_t kLeAudioDirectionBoth =
    kLeAudioDirectionSink | kLeAudioDirectionSource;

/* Sampling Frequencies */
constexpr uint8_t kLeAudioSamplingFreq8000Hz = 0x01;
constexpr uint8_t kLeAudioSamplingFreq11025Hz = 0x02;
constexpr uint8_t kLeAudioSamplingFreq16000Hz = 0x03;
constexpr uint8_t kLeAudioSamplingFreq22050Hz = 0x04;
constexpr uint8_t kLeAudioSamplingFreq24000Hz = 0x05;
constexpr uint8_t kLeAudioSamplingFreq32000Hz = 0x06;
constexpr uint8_t kLeAudioSamplingFreq44100Hz = 0x07;
constexpr uint8_t kLeAudioSamplingFreq48000Hz = 0x08;
constexpr uint8_t kLeAudioSamplingFreq88200Hz = 0x09;
constexpr uint8_t kLeAudioSamplingFreq96000Hz = 0x0A;
constexpr uint8_t kLeAudioSamplingFreq176400Hz = 0x0B;
constexpr uint8_t kLeAudioSamplingFreq192000Hz = 0x0C;
constexpr uint8_t kLeAudioSamplingFreq384000Hz = 0x0D;

/* Frame Durations */
constexpr uint8_t kLeAudioCodecFrameDur7500us = 0x00;
constexpr uint8_t kLeAudioCodecFrameDur10000us = 0x01;

/* Audio Allocations */
constexpr uint32_t kLeAudioLocationNotAllowed = 0x00000000;
constexpr uint32_t kLeAudioLocationFrontLeft = 0x00000001;
constexpr uint32_t kLeAudioLocationFrontRight = 0x00000002;
constexpr uint32_t kLeAudioLocationFrontCenter = 0x00000004;
constexpr uint32_t kLeAudioLocationLowFreqEffects1 = 0x00000008;
constexpr uint32_t kLeAudioLocationBackLeft = 0x00000010;
constexpr uint32_t kLeAudioLocationBackRight = 0x00000020;
constexpr uint32_t kLeAudioLocationFrontLeftOfCenter = 0x00000040;
constexpr uint32_t kLeAudioLocationFrontRightOfCenter = 0x00000080;
constexpr uint32_t kLeAudioLocationBackCenter = 0x00000100;
constexpr uint32_t kLeAudioLocationLowFreqEffects2 = 0x00000200;
constexpr uint32_t kLeAudioLocationSideLeft = 0x00000400;
constexpr uint32_t kLeAudioLocationSideRight = 0x00000800;
constexpr uint32_t kLeAudioLocationTopFrontLeft = 0x00001000;
constexpr uint32_t kLeAudioLocationTopFrontRight = 0x00002000;
constexpr uint32_t kLeAudioLocationTopFrontCenter = 0x00004000;
constexpr uint32_t kLeAudioLocationTopCenter = 0x00008000;
constexpr uint32_t kLeAudioLocationTopBackLeft = 0x00010000;
constexpr uint32_t kLeAudioLocationTopBackRight = 0x00020000;
constexpr uint32_t kLeAudioLocationTopSideLeft = 0x00040000;
constexpr uint32_t kLeAudioLocationTopSideRight = 0x00080000;
constexpr uint32_t kLeAudioLocationTopBackCenter = 0x00100000;
constexpr uint32_t kLeAudioLocationBottomFrontCenter = 0x00200000;
constexpr uint32_t kLeAudioLocationBottomFrontLeft = 0x00400000;
constexpr uint32_t kLeAudioLocationBottomFrontRight = 0x00800000;
constexpr uint32_t kLeAudioLocationFrontLeftWide = 0x01000000;
constexpr uint32_t kLeAudioLocationFrontRightWide = 0x02000000;
constexpr uint32_t kLeAudioLocationLeftSurround = 0x04000000;
constexpr uint32_t kLeAudioLocationRightSurround = 0x08000000;

constexpr uint32_t kLeAudioLocationAnyLeft =
    kLeAudioLocationFrontLeft | kLeAudioLocationBackLeft |
    kLeAudioLocationFrontLeftOfCenter | kLeAudioLocationSideLeft |
    kLeAudioLocationTopFrontLeft | kLeAudioLocationTopBackLeft |
    kLeAudioLocationTopSideLeft | kLeAudioLocationBottomFrontLeft |
    kLeAudioLocationFrontLeftWide | kLeAudioLocationLeftSurround;

constexpr uint32_t kLeAudioLocationAnyRight =
    kLeAudioLocationFrontRight | kLeAudioLocationBackRight |
    kLeAudioLocationFrontRightOfCenter | kLeAudioLocationSideRight |
    kLeAudioLocationTopFrontRight | kLeAudioLocationTopBackRight |
    kLeAudioLocationTopSideRight | kLeAudioLocationBottomFrontRight |
    kLeAudioLocationFrontRightWide | kLeAudioLocationRightSurround;

constexpr uint32_t kLeAudioLocationStereo =
    kLeAudioLocationFrontLeft | kLeAudioLocationFrontRight;

/* Octets Per Frame */
constexpr uint16_t kLeAudioCodecFrameLen30 = 30;
constexpr uint16_t kLeAudioCodecFrameLen40 = 40;
constexpr uint16_t kLeAudioCodecFrameLen60 = 60;
constexpr uint16_t kLeAudioCodecFrameLen80 = 80;
constexpr uint16_t kLeAudioCodecFrameLen100 = 100;
constexpr uint16_t kLeAudioCodecFrameLen120 = 120;

/* Helper map for matching various sampling frequency notations */
const std::map<uint8_t, CodecSpecificConfigurationLtv::SamplingFrequency>
    sampling_freq_map = {
        {kLeAudioSamplingFreq8000Hz,
         CodecSpecificConfigurationLtv::SamplingFrequency::HZ8000},
        {kLeAudioSamplingFreq16000Hz,
         CodecSpecificConfigurationLtv::SamplingFrequency::HZ16000},
        {kLeAudioSamplingFreq24000Hz,
         CodecSpecificConfigurationLtv::SamplingFrequency::HZ24000},
        {kLeAudioSamplingFreq32000Hz,
         CodecSpecificConfigurationLtv::SamplingFrequency::HZ32000},
        {kLeAudioSamplingFreq44100Hz,
         CodecSpecificConfigurationLtv::SamplingFrequency::HZ44100},
        {kLeAudioSamplingFreq48000Hz,
         CodecSpecificConfigurationLtv::SamplingFrequency::HZ48000}};

/* Helper map for matching various frame durations notations */
const std::map<uint8_t, CodecSpecificConfigurationLtv::FrameDuration>
    frame_duration_map = {
        {kLeAudioCodecFrameDur7500us,
         CodecSpecificConfigurationLtv::FrameDuration::US7500},
        {kLeAudioCodecFrameDur10000us,
         CodecSpecificConfigurationLtv::FrameDuration::US10000}};

/* Helper map for matching various audio channel allocation notations */
std::map<uint32_t, uint32_t> audio_channel_allocation_map = {
    {kLeAudioLocationNotAllowed,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::NOT_ALLOWED},
    {kLeAudioLocationFrontLeft,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::FRONT_LEFT},
    {kLeAudioLocationFrontRight,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::FRONT_RIGHT},
    {kLeAudioLocationFrontCenter,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::FRONT_CENTER},
    {kLeAudioLocationLowFreqEffects1,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::
         LOW_FREQUENCY_EFFECTS_1},
    {kLeAudioLocationBackLeft,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::BACK_LEFT},
    {kLeAudioLocationBackRight,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::BACK_RIGHT},
    {kLeAudioLocationFrontLeftOfCenter,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::
         FRONT_LEFT_OF_CENTER},
    {kLeAudioLocationFrontRightOfCenter,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::
         FRONT_RIGHT_OF_CENTER},
    {kLeAudioLocationBackCenter,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::BACK_CENTER},
    {kLeAudioLocationLowFreqEffects2,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::
         LOW_FREQUENCY_EFFECTS_2},
    {kLeAudioLocationSideLeft,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::SIDE_LEFT},
    {kLeAudioLocationSideRight,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::SIDE_RIGHT},
    {kLeAudioLocationTopFrontLeft,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::TOP_FRONT_LEFT},
    {kLeAudioLocationTopFrontRight,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::TOP_FRONT_RIGHT},
    {kLeAudioLocationTopFrontCenter,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::TOP_FRONT_CENTER},
    {kLeAudioLocationTopCenter,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::TOP_CENTER},
    {kLeAudioLocationTopBackLeft,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::TOP_BACK_LEFT},
    {kLeAudioLocationTopBackRight,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::TOP_BACK_RIGHT},
    {kLeAudioLocationTopSideLeft,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::TOP_SIDE_LEFT},
    {kLeAudioLocationTopSideRight,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::TOP_SIDE_RIGHT},
    {kLeAudioLocationTopBackCenter,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::TOP_BACK_CENTER},
    {kLeAudioLocationBottomFrontCenter,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::
         BOTTOM_FRONT_CENTER},
    {kLeAudioLocationBottomFrontLeft,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::BOTTOM_FRONT_LEFT},
    {kLeAudioLocationBottomFrontRight,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::BOTTOM_FRONT_RIGHT},
    {kLeAudioLocationFrontLeftWide,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::FRONT_LEFT_WIDE},
    {kLeAudioLocationFrontRightWide,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::FRONT_RIGHT_WIDE},
    {kLeAudioLocationLeftSurround,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::LEFT_SURROUND},
    {kLeAudioLocationRightSurround,
     CodecSpecificConfigurationLtv::AudioChannelAllocation::RIGHT_SURROUND},
};

static const std::vector<
    std::pair<const char* /*schema*/, const char* /*content*/>>
    kLeAudioSetConfigs = {{"/vendor/etc/aidl/le_audio/"
                           "aidl_audio_set_configurations.bfbs",
                           "/vendor/etc/aidl/le_audio/"
                           "aidl_audio_set_configurations.json"}};
static const std::vector<
    std::pair<const char* /*schema*/, const char* /*content*/>>
    kLeAudioSetScenarios = {{"/vendor/etc/aidl/le_audio/"
                             "aidl_audio_set_scenarios.bfbs",
                             "/vendor/etc/aidl/le_audio/"
                             "aidl_audio_set_scenarios.json"}};

/* Implementation */

std::vector<LeAudioAseConfigurationSetting>
AudioSetConfigurationProviderJson::GetLeAudioAseConfigurationSettings() {
  AudioSetConfigurationProviderJson::LoadAudioSetConfigurationProviderJson();
  return ase_configuration_settings_;
}

void AudioSetConfigurationProviderJson::
    LoadAudioSetConfigurationProviderJson() {
  if (configurations_.empty() || ase_configuration_settings_.empty()) {
    ase_configuration_settings_.clear();
    configurations_.clear();
    auto loaded = LoadContent(kLeAudioSetConfigs, kLeAudioSetScenarios,
                              CodecLocation::HOST);
    if (!loaded)
      LOG(ERROR) << ": Unable to load le audio set configuration files.";
  } else
    LOG(INFO) << ": Reusing loaded le audio set configuration";
}

const le_audio::CodecSpecificConfiguration*
AudioSetConfigurationProviderJson::LookupCodecSpecificParam(
    const flatbuffers::Vector<flatbuffers::Offset<
        le_audio::CodecSpecificConfiguration>>* flat_codec_specific_params,
    le_audio::CodecSpecificLtvGenericTypes type) {
  auto it = std::find_if(
      flat_codec_specific_params->cbegin(), flat_codec_specific_params->cend(),
      [&type](const auto& csc) { return (csc->type() == type); });
  return (it != flat_codec_specific_params->cend()) ? *it : nullptr;
}

void AudioSetConfigurationProviderJson::populateAudioChannelAllocation(
    CodecSpecificConfigurationLtv::AudioChannelAllocation&
        audio_channel_allocation,
    uint32_t audio_location) {
  audio_channel_allocation.bitmask = 0;
  for (auto [allocation, bitmask] : audio_channel_allocation_map) {
    if (audio_location & allocation)
      audio_channel_allocation.bitmask |= bitmask;
  }
}

void AudioSetConfigurationProviderJson::populateConfigurationData(
    LeAudioAseConfiguration& ase,
    const flatbuffers::Vector<
        flatbuffers::Offset<le_audio::CodecSpecificConfiguration>>*
        flat_codec_specific_params) {
  uint8_t sampling_frequency = 0;
  uint8_t frame_duration = 0;
  uint32_t audio_channel_allocation = 0;
  uint16_t octets_per_codec_frame = 0;
  uint8_t codec_frames_blocks_per_sdu = 0;

  auto param = LookupCodecSpecificParam(
      flat_codec_specific_params,
      le_audio::CodecSpecificLtvGenericTypes_SUPPORTED_SAMPLING_FREQUENCY);
  if (param) {
    auto ptr = param->compound_value()->value()->data();
    STREAM_TO_UINT8(sampling_frequency, ptr);
  }

  param = LookupCodecSpecificParam(
      flat_codec_specific_params,
      le_audio::CodecSpecificLtvGenericTypes_SUPPORTED_FRAME_DURATION);
  if (param) {
    auto ptr = param->compound_value()->value()->data();
    STREAM_TO_UINT8(frame_duration, ptr);
  }

  param = LookupCodecSpecificParam(
      flat_codec_specific_params,
      le_audio::
          CodecSpecificLtvGenericTypes_SUPPORTED_AUDIO_CHANNEL_ALLOCATION);
  if (param) {
    auto ptr = param->compound_value()->value()->data();
    STREAM_TO_UINT32(audio_channel_allocation, ptr);
  }

  param = LookupCodecSpecificParam(
      flat_codec_specific_params,
      le_audio::CodecSpecificLtvGenericTypes_SUPPORTED_OCTETS_PER_CODEC_FRAME);
  if (param) {
    auto ptr = param->compound_value()->value()->data();
    STREAM_TO_UINT16(octets_per_codec_frame, ptr);
  }

  param = LookupCodecSpecificParam(
      flat_codec_specific_params,
      le_audio::
          CodecSpecificLtvGenericTypes_SUPPORTED_CODEC_FRAME_BLOCKS_PER_SDU);
  if (param) {
    auto ptr = param->compound_value()->value()->data();
    STREAM_TO_UINT8(codec_frames_blocks_per_sdu, ptr);
  }

  // Make the correct value
  ase.codecConfiguration = std::vector<CodecSpecificConfigurationLtv>();

  auto sampling_freq_it = sampling_freq_map.find(sampling_frequency);
  if (sampling_freq_it != sampling_freq_map.end())
    ase.codecConfiguration.push_back(sampling_freq_it->second);
  auto frame_duration_it = frame_duration_map.find(frame_duration);
  if (frame_duration_it != frame_duration_map.end())
    ase.codecConfiguration.push_back(frame_duration_it->second);

  CodecSpecificConfigurationLtv::AudioChannelAllocation channel_allocation;
  populateAudioChannelAllocation(channel_allocation, audio_channel_allocation);
  ase.codecConfiguration.push_back(channel_allocation);

  auto octet_structure = CodecSpecificConfigurationLtv::OctetsPerCodecFrame();
  octet_structure.value = octets_per_codec_frame;
  ase.codecConfiguration.push_back(octet_structure);

  auto frame_sdu_structure =
      CodecSpecificConfigurationLtv::CodecFrameBlocksPerSDU();
  frame_sdu_structure.value = codec_frames_blocks_per_sdu;
  ase.codecConfiguration.push_back(frame_sdu_structure);
  // TODO: Channel count
}

void AudioSetConfigurationProviderJson::populateAseConfiguration(
    LeAudioAseConfiguration& ase,
    const le_audio::AudioSetSubConfiguration* flat_subconfig,
    const le_audio::QosConfiguration* qos_cfg) {
  // Target latency
  switch (qos_cfg->target_latency()) {
    case le_audio::AudioSetConfigurationTargetLatency::
        AudioSetConfigurationTargetLatency_BALANCED_RELIABILITY:
      ase.targetLatency =
          LeAudioAseConfiguration::TargetLatency::BALANCED_LATENCY_RELIABILITY;
      break;
    case le_audio::AudioSetConfigurationTargetLatency::
        AudioSetConfigurationTargetLatency_HIGH_RELIABILITY:
      ase.targetLatency =
          LeAudioAseConfiguration::TargetLatency::HIGHER_RELIABILITY;
      break;
    case le_audio::AudioSetConfigurationTargetLatency::
        AudioSetConfigurationTargetLatency_LOW:
      ase.targetLatency = LeAudioAseConfiguration::TargetLatency::LOWER;
      break;
    default:
      ase.targetLatency = LeAudioAseConfiguration::TargetLatency::UNDEFINED;
      break;
  };

  ase.targetPhy = Phy::TWO_M;
  // Making CodecId
  if (flat_subconfig->codec_id()->coding_format() ==
      (uint8_t)CodecId::Core::LC3) {
    ase.codecId = CodecId::Core::LC3;
  } else {
    auto vendorC = CodecId::Vendor();
    vendorC.codecId = flat_subconfig->codec_id()->vendor_codec_id();
    vendorC.id = flat_subconfig->codec_id()->vendor_company_id();
    ase.codecId = vendorC;
  }
  // Codec configuration data
  populateConfigurationData(ase, flat_subconfig->codec_configuration());
}

void AudioSetConfigurationProviderJson::populateAseQosConfiguration(
    LeAudioAseQosConfiguration& qos,
    const le_audio::QosConfiguration* qos_cfg) {
  qos.maxTransportLatencyMs = qos_cfg->max_transport_latency();
  qos.retransmissionNum = qos_cfg->retransmission_number();
}

// Parse into AseDirectionConfiguration
AseDirectionConfiguration
AudioSetConfigurationProviderJson::SetConfigurationFromFlatSubconfig(
    const le_audio::AudioSetSubConfiguration* flat_subconfig,
    const le_audio::QosConfiguration* qos_cfg, CodecLocation location) {
  AseDirectionConfiguration direction_conf;

  LeAudioAseConfiguration ase;
  LeAudioAseQosConfiguration qos;
  LeAudioDataPathConfiguration path;

  // Translate into LeAudioAseConfiguration
  populateAseConfiguration(ase, flat_subconfig, qos_cfg);

  // Translate into LeAudioAseQosConfiguration
  populateAseQosConfiguration(qos, qos_cfg);

  // Translate location to data path id
  switch (location) {
    case CodecLocation::ADSP:
      path.isoDataPathConfiguration.isTransparent = true;
      path.dataPathId = kIsoDataPathPlatformDefault;
      break;
    case CodecLocation::HOST:
      path.isoDataPathConfiguration.isTransparent = true;
      path.dataPathId = kIsoDataPathHci;
      break;
    case CodecLocation::CONTROLLER:
      path.isoDataPathConfiguration.isTransparent = false;
      path.dataPathId = kIsoDataPathPlatformDefault;
      break;
  }

  direction_conf.aseConfiguration = ase;
  direction_conf.qosConfiguration = qos;
  direction_conf.dataPathConfiguration = path;

  return direction_conf;
}

// Parse into AseDirectionConfiguration and the ConfigurationFlags
// and put them in the given list.
void AudioSetConfigurationProviderJson::processSubconfig(
    const le_audio::AudioSetSubConfiguration* subconfig,
    const le_audio::QosConfiguration* qos_cfg,
    std::vector<std::optional<AseDirectionConfiguration>>&
        directionAseConfiguration,
    CodecLocation location) {
  directionAseConfiguration.push_back(
      SetConfigurationFromFlatSubconfig(subconfig, qos_cfg, location));
}

void AudioSetConfigurationProviderJson::PopulateAseConfigurationFromFlat(
    const le_audio::AudioSetConfiguration* flat_cfg,
    std::vector<const le_audio::CodecConfiguration*>* codec_cfgs,
    std::vector<const le_audio::QosConfiguration*>* qos_cfgs,
    CodecLocation location,
    std::vector<std::optional<AseDirectionConfiguration>>&
        sourceAseConfiguration,
    std::vector<std::optional<AseDirectionConfiguration>>& sinkAseConfiguration,
    ConfigurationFlags& /*configurationFlags*/) {
  if (flat_cfg == nullptr) {
    LOG(ERROR) << "flat_cfg cannot be null";
    return;
  }
  std::string codec_config_key = flat_cfg->codec_config_name()->str();
  auto* qos_config_key_array = flat_cfg->qos_config_name();

  constexpr std::string_view default_qos = "QoS_Config_Balanced_Reliability";

  std::string qos_sink_key(default_qos);
  std::string qos_source_key(default_qos);

  /* We expect maximum two QoS settings. First for Sink and second for Source
   */
  if (qos_config_key_array->size() > 0) {
    qos_sink_key = qos_config_key_array->Get(0)->str();
    if (qos_config_key_array->size() > 1) {
      qos_source_key = qos_config_key_array->Get(1)->str();
    } else {
      qos_source_key = qos_sink_key;
    }
  }

  LOG(INFO) << "Audio set config " << flat_cfg->name()->c_str()
            << ": codec config " << codec_config_key.c_str() << ", qos_sink "
            << qos_sink_key.c_str() << ", qos_source "
            << qos_source_key.c_str();

  // Find the first qos config that match the name
  const le_audio::QosConfiguration* qos_sink_cfg = nullptr;
  for (auto i = qos_cfgs->begin(); i != qos_cfgs->end(); ++i) {
    if ((*i)->name()->str() == qos_sink_key) {
      qos_sink_cfg = *i;
      break;
    }
  }

  const le_audio::QosConfiguration* qos_source_cfg = nullptr;
  for (auto i = qos_cfgs->begin(); i != qos_cfgs->end(); ++i) {
    if ((*i)->name()->str() == qos_source_key) {
      qos_source_cfg = *i;
      break;
    }
  }

  // First codec_cfg with the same name
  const le_audio::CodecConfiguration* codec_cfg = nullptr;
  for (auto i = codec_cfgs->begin(); i != codec_cfgs->end(); ++i) {
    if ((*i)->name()->str() == codec_config_key) {
      codec_cfg = *i;
      break;
    }
  }

  // Process each subconfig and put it into the correct list
  if (codec_cfg != nullptr && codec_cfg->subconfigurations()) {
    /* Load subconfigurations */
    for (auto subconfig : *codec_cfg->subconfigurations()) {
      if (subconfig->direction() == kLeAudioDirectionSink) {
        processSubconfig(subconfig, qos_sink_cfg, sinkAseConfiguration,
                         location);
      } else {
        processSubconfig(subconfig, qos_source_cfg, sourceAseConfiguration,
                         location);
      }
    }
  } else {
    if (codec_cfg == nullptr) {
      LOG(ERROR) << "No codec config matching key " << codec_config_key.c_str()
                 << " found";
    } else {
      LOG(ERROR) << "Configuration '" << flat_cfg->name()->c_str()
                 << "' has no valid subconfigurations.";
    }
  }

  // TODO: Populate information for ConfigurationFlags
}

bool AudioSetConfigurationProviderJson::LoadConfigurationsFromFiles(
    const char* schema_file, const char* content_file, CodecLocation location) {
  flatbuffers::Parser configurations_parser_;
  std::string configurations_schema_binary_content;
  bool ok = flatbuffers::LoadFile(schema_file, true,
                                  &configurations_schema_binary_content);
  LOG(INFO) << __func__ << ": Loading file " << schema_file;
  if (!ok) return ok;

  /* Load the binary schema */
  ok = configurations_parser_.Deserialize(
      (uint8_t*)configurations_schema_binary_content.c_str(),
      configurations_schema_binary_content.length());
  if (!ok) return ok;

  /* Load the content from JSON */
  std::string configurations_json_content;
  LOG(INFO) << __func__ << ": Loading file " << content_file;
  ok = flatbuffers::LoadFile(content_file, false, &configurations_json_content);
  if (!ok) return ok;

  /* Parse */
  LOG(INFO) << __func__ << ": Parse JSON content";
  ok = configurations_parser_.Parse(configurations_json_content.c_str());
  if (!ok) return ok;

  /* Import from flatbuffers */
  LOG(INFO) << __func__ << ": Build flat buffer structure";
  auto configurations_root = le_audio::GetAudioSetConfigurations(
      configurations_parser_.builder_.GetBufferPointer());
  if (!configurations_root) return false;

  auto flat_qos_configs = configurations_root->qos_configurations();
  if ((flat_qos_configs == nullptr) || (flat_qos_configs->size() == 0))
    return false;

  LOG(DEBUG) << ": Updating " << flat_qos_configs->size()
             << " qos config entries.";
  std::vector<const le_audio::QosConfiguration*> qos_cfgs;
  for (auto const& flat_qos_cfg : *flat_qos_configs) {
    qos_cfgs.push_back(flat_qos_cfg);
  }

  auto flat_codec_configs = configurations_root->codec_configurations();
  if ((flat_codec_configs == nullptr) || (flat_codec_configs->size() == 0))
    return false;

  LOG(DEBUG) << ": Updating " << flat_codec_configs->size()
             << " codec config entries.";
  std::vector<const le_audio::CodecConfiguration*> codec_cfgs;
  for (auto const& flat_codec_cfg : *flat_codec_configs) {
    codec_cfgs.push_back(flat_codec_cfg);
  }

  auto flat_configs = configurations_root->configurations();
  if ((flat_configs == nullptr) || (flat_configs->size() == 0)) return false;

  LOG(DEBUG) << ": Updating " << flat_configs->size() << " config entries.";
  for (auto const& flat_cfg : *flat_configs) {
    // Create 3 vector to use
    std::vector<std::optional<AseDirectionConfiguration>>
        sourceAseConfiguration;
    std::vector<std::optional<AseDirectionConfiguration>> sinkAseConfiguration;
    ConfigurationFlags configurationFlags;
    PopulateAseConfigurationFromFlat(flat_cfg, &codec_cfgs, &qos_cfgs, location,
                                     sourceAseConfiguration,
                                     sinkAseConfiguration, configurationFlags);
    if (sourceAseConfiguration.empty() && sinkAseConfiguration.empty())
      continue;
    configurations_[flat_cfg->name()->str()] = std::make_tuple(
        sourceAseConfiguration, sinkAseConfiguration, configurationFlags);
  }

  return true;
}

bool AudioSetConfigurationProviderJson::LoadScenariosFromFiles(
    const char* schema_file, const char* content_file) {
  flatbuffers::Parser scenarios_parser_;
  std::string scenarios_schema_binary_content;
  bool ok = flatbuffers::LoadFile(schema_file, true,
                                  &scenarios_schema_binary_content);
  LOG(INFO) << __func__ << ": Loading file " << schema_file;
  if (!ok) return ok;

  /* Load the binary schema */
  ok = scenarios_parser_.Deserialize(
      (uint8_t*)scenarios_schema_binary_content.c_str(),
      scenarios_schema_binary_content.length());
  if (!ok) return ok;

  /* Load the content from JSON */
  LOG(INFO) << __func__ << ": Loading file " << content_file;
  std::string scenarios_json_content;
  ok = flatbuffers::LoadFile(content_file, false, &scenarios_json_content);
  if (!ok) return ok;

  /* Parse */
  LOG(INFO) << __func__ << ": Parse json content";
  ok = scenarios_parser_.Parse(scenarios_json_content.c_str());
  if (!ok) return ok;

  /* Import from flatbuffers */
  LOG(INFO) << __func__ << ": Build flat buffer structure";
  auto scenarios_root = le_audio::GetAudioSetScenarios(
      scenarios_parser_.builder_.GetBufferPointer());
  if (!scenarios_root) return false;

  auto flat_scenarios = scenarios_root->scenarios();
  if ((flat_scenarios == nullptr) || (flat_scenarios->size() == 0))
    return false;

  LOG(INFO) << __func__ << ": Turn flat buffer into structure";
  AudioContext media_context = AudioContext();
  media_context.bitmask =
      (AudioContext::ALERTS | AudioContext::INSTRUCTIONAL |
       AudioContext::NOTIFICATIONS | AudioContext::EMERGENCY_ALARM |
       AudioContext::UNSPECIFIED | AudioContext::MEDIA);

  AudioContext conversational_context = AudioContext();
  conversational_context.bitmask =
      (AudioContext::RINGTONE_ALERTS | AudioContext::CONVERSATIONAL);

  AudioContext live_context = AudioContext();
  live_context.bitmask = AudioContext::LIVE_AUDIO;

  AudioContext game_context = AudioContext();
  game_context.bitmask = AudioContext::GAME;

  AudioContext voice_assistants_context = AudioContext();
  voice_assistants_context.bitmask = AudioContext::VOICE_ASSISTANTS;

  LOG(DEBUG) << "Updating " << flat_scenarios->size() << " scenarios.";
  for (auto const& scenario : *flat_scenarios) {
    LOG(DEBUG) << "Scenario " << scenario->name()->c_str()
               << " configs: " << scenario->configurations()->size();

    if (!scenario->configurations()) continue;
    std::string scenario_name = scenario->name()->c_str();
    AudioContext context;
    if (scenario_name == "Media")
      context = AudioContext(media_context);
    else if (scenario_name == "Conversational")
      context = AudioContext(conversational_context);
    else if (scenario_name == "Live")
      context = AudioContext(live_context);
    else if (scenario_name == "Game")
      context = AudioContext(game_context);
    else if (scenario_name == "VoiceAssistants")
      context = AudioContext(voice_assistants_context);

    for (auto it = scenario->configurations()->begin();
         it != scenario->configurations()->end(); ++it) {
      auto config_name = it->str();
      auto configuration = configurations_.find(config_name);
      if (configuration == configurations_.end()) continue;
      LOG(DEBUG) << "Getting configuration with name: " << config_name;
      auto [source, sink, flags] = configuration->second;
      // Each configuration will create a LeAudioAseConfigurationSetting
      // with the same {context, packing}
      // and different data
      LeAudioAseConfigurationSetting setting;
      setting.audioContext = context;
      // TODO: Packing
      setting.sourceAseConfiguration = source;
      setting.sinkAseConfiguration = sink;
      setting.flags = flags;
      // Add to list of setting
      LOG(DEBUG) << "Pushing configuration to list: " << config_name;
      ase_configuration_settings_.push_back(setting);
    }
  }

  return true;
}

bool AudioSetConfigurationProviderJson::LoadContent(
    std::vector<std::pair<const char* /*schema*/, const char* /*content*/>>
        config_files,
    std::vector<std::pair<const char* /*schema*/, const char* /*content*/>>
        scenario_files,
    CodecLocation location) {
  for (auto [schema, content] : config_files) {
    if (!LoadConfigurationsFromFiles(schema, content, location)) return false;
  }

  for (auto [schema, content] : scenario_files) {
    if (!LoadScenariosFromFiles(schema, content)) return false;
  }
  return true;
}

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
