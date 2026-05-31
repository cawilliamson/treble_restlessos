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

#define LOG_TAG "BTAudioProviderLeAudioHW"

#include "LeAudioOffloadAudioProvider.h"

#include <BluetoothAudioCodecs.h>
#include <BluetoothAudioSessionReport.h>
#include <android-base/logging.h>

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

constexpr uint8_t kLeAudioDirectionSink = 0x01;
constexpr uint8_t kLeAudioDirectionSource = 0x02;

const std::map<CodecSpecificConfigurationLtv::SamplingFrequency, uint32_t>
    freq_to_support_bitmask_map = {
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ8000,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ8000},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ11025,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ11025},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ16000,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ16000},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ22050,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ22050},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ24000,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ24000},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ32000,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ32000},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ48000,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ48000},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ88200,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ88200},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ96000,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ96000},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ176400,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ176400},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ192000,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ192000},
        {CodecSpecificConfigurationLtv::SamplingFrequency::HZ384000,
         CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies::HZ384000},
};

// Helper map from capability's tag to configuration's tag
std::map<CodecSpecificCapabilitiesLtv::Tag, CodecSpecificConfigurationLtv::Tag>
    cap_to_cfg_tag_map = {
        {CodecSpecificCapabilitiesLtv::Tag::supportedSamplingFrequencies,
         CodecSpecificConfigurationLtv::Tag::samplingFrequency},
        {CodecSpecificCapabilitiesLtv::Tag::supportedMaxCodecFramesPerSDU,
         CodecSpecificConfigurationLtv::Tag::codecFrameBlocksPerSDU},
        {CodecSpecificCapabilitiesLtv::Tag::supportedFrameDurations,
         CodecSpecificConfigurationLtv::Tag::frameDuration},
        {CodecSpecificCapabilitiesLtv::Tag::supportedAudioChannelCounts,
         CodecSpecificConfigurationLtv::Tag::audioChannelAllocation},
        {CodecSpecificCapabilitiesLtv::Tag::supportedOctetsPerCodecFrame,
         CodecSpecificConfigurationLtv::Tag::octetsPerCodecFrame},
};

const std::map<CodecSpecificConfigurationLtv::FrameDuration, uint32_t>
    fduration_to_support_fduration_map = {
        {CodecSpecificConfigurationLtv::FrameDuration::US7500,
         CodecSpecificCapabilitiesLtv::SupportedFrameDurations::US7500},
        {CodecSpecificConfigurationLtv::FrameDuration::US10000,
         CodecSpecificCapabilitiesLtv::SupportedFrameDurations::US10000},
};

std::map<int32_t, CodecSpecificConfigurationLtv::SamplingFrequency>
    sampling_freq_map = {
        {16000, CodecSpecificConfigurationLtv::SamplingFrequency::HZ16000},
        {48000, CodecSpecificConfigurationLtv::SamplingFrequency::HZ48000},
        {96000, CodecSpecificConfigurationLtv::SamplingFrequency::HZ96000},
};

std::map<int32_t, CodecSpecificConfigurationLtv::FrameDuration>
    frame_duration_map = {
        {7500, CodecSpecificConfigurationLtv::FrameDuration::US7500},
        {10000, CodecSpecificConfigurationLtv::FrameDuration::US10000},
};

LeAudioOffloadOutputAudioProvider::LeAudioOffloadOutputAudioProvider()
    : LeAudioOffloadAudioProvider() {
  session_type_ = SessionType::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH;
}

LeAudioOffloadInputAudioProvider::LeAudioOffloadInputAudioProvider()
    : LeAudioOffloadAudioProvider() {
  session_type_ = SessionType::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH;
}

LeAudioOffloadBroadcastAudioProvider::LeAudioOffloadBroadcastAudioProvider()
    : LeAudioOffloadAudioProvider() {
  session_type_ =
      SessionType::LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH;
}

LeAudioOffloadAudioProvider::LeAudioOffloadAudioProvider()
    : BluetoothAudioProvider() {}

bool LeAudioOffloadAudioProvider::isValid(const SessionType& sessionType) {
  return (sessionType == session_type_);
}

ndk::ScopedAStatus LeAudioOffloadAudioProvider::startSession(
    const std::shared_ptr<IBluetoothAudioPort>& host_if,
    const AudioConfiguration& audio_config,
    const std::vector<LatencyMode>& latency_modes, DataMQDesc* _aidl_return) {
  if (session_type_ ==
      SessionType::LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH) {
    if (audio_config.getTag() != AudioConfiguration::leAudioBroadcastConfig) {
      LOG(WARNING) << __func__ << " - Invalid Audio Configuration="
                   << audio_config.toString();
      *_aidl_return = DataMQDesc();
      return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
  } else if (audio_config.getTag() != AudioConfiguration::leAudioConfig) {
    LOG(WARNING) << __func__ << " - Invalid Audio Configuration="
                 << audio_config.toString();
    *_aidl_return = DataMQDesc();
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }

  return BluetoothAudioProvider::startSession(host_if, audio_config,
                                              latency_modes, _aidl_return);
}

ndk::ScopedAStatus LeAudioOffloadAudioProvider::onSessionReady(
    DataMQDesc* _aidl_return) {
  BluetoothAudioSessionReport::OnSessionStarted(
      session_type_, stack_iface_, nullptr, *audio_config_, latency_modes_);
  *_aidl_return = DataMQDesc();
  return ndk::ScopedAStatus::ok();
}
ndk::ScopedAStatus LeAudioOffloadAudioProvider::setCodecPriority(
    const CodecId& in_codecId, int32_t in_priority) {
  codec_priority_map_[in_codecId] = in_priority;
  return ndk::ScopedAStatus::ok();
};

bool LeAudioOffloadAudioProvider::isMatchedValidCodec(CodecId cfg_codec,
                                                      CodecId req_codec) {
  auto priority = codec_priority_map_.find(cfg_codec);
  if (priority != codec_priority_map_.end() &&
      priority->second ==
          LeAudioOffloadAudioProvider::CODEC_PRIORITY_DISABLED) {
    return false;
  }
  return cfg_codec == req_codec;
}

bool LeAudioOffloadAudioProvider::isCapabilitiesMatchedContext(
    AudioContext setting_context,
    const IBluetoothAudioProvider::LeAudioDeviceCapabilities& capabilities) {
  // If has no metadata, assume match
  if (!capabilities.metadata.has_value()) return true;

  for (auto metadata : capabilities.metadata.value()) {
    if (!metadata.has_value()) continue;
    if (metadata.value().getTag() == MetadataLtv::Tag::preferredAudioContexts) {
      // Check all pref audio context to see if anything matched
      auto& context = metadata.value()
                          .get<MetadataLtv::Tag::preferredAudioContexts>()
                          .values;
      if (setting_context.bitmask & context.bitmask) return true;
    }
  }

  return false;
}

bool LeAudioOffloadAudioProvider::isMatchedSamplingFreq(
    CodecSpecificConfigurationLtv::SamplingFrequency& cfg_freq,
    CodecSpecificCapabilitiesLtv::SupportedSamplingFrequencies&
        capability_freq) {
  for (auto [freq, bitmask] : freq_to_support_bitmask_map)
    if (cfg_freq == freq) return (capability_freq.bitmask & bitmask);
  return false;
}

bool LeAudioOffloadAudioProvider::isMatchedFrameDuration(
    CodecSpecificConfigurationLtv::FrameDuration& cfg_fduration,
    CodecSpecificCapabilitiesLtv::SupportedFrameDurations&
        capability_fduration) {
  for (auto [fduration, bitmask] : fduration_to_support_fduration_map)
    if (cfg_fduration == fduration)
      return (capability_fduration.bitmask & bitmask);
  return false;
}

bool LeAudioOffloadAudioProvider::isMatchedAudioChannel(
    CodecSpecificConfigurationLtv::AudioChannelAllocation&
    /*cfg_channel*/,
    CodecSpecificCapabilitiesLtv::SupportedAudioChannelCounts&
    /*capability_channel*/) {
  bool isMatched = true;
  // TODO: how to match?
  return isMatched;
}

bool LeAudioOffloadAudioProvider::isMatchedCodecFramesPerSDU(
    CodecSpecificConfigurationLtv::CodecFrameBlocksPerSDU& cfg_frame_sdu,
    CodecSpecificCapabilitiesLtv::SupportedMaxCodecFramesPerSDU&
        capability_frame_sdu) {
  return cfg_frame_sdu.value <= capability_frame_sdu.value;
}

bool LeAudioOffloadAudioProvider::isMatchedOctetsPerCodecFrame(
    CodecSpecificConfigurationLtv::OctetsPerCodecFrame& cfg_octets,
    CodecSpecificCapabilitiesLtv::SupportedOctetsPerCodecFrame&
        capability_octets) {
  return cfg_octets.value >= capability_octets.min &&
         cfg_octets.value <= capability_octets.max;
}

bool LeAudioOffloadAudioProvider::isCapabilitiesMatchedCodecConfiguration(
    std::vector<CodecSpecificConfigurationLtv>& codec_cfg,
    std::vector<CodecSpecificCapabilitiesLtv> codec_capabilities) {
  // Convert all codec_cfg into a map of tags -> correct data
  std::map<CodecSpecificConfigurationLtv::Tag, CodecSpecificConfigurationLtv>
      cfg_tag_map;
  for (auto codec_cfg_data : codec_cfg)
    cfg_tag_map[codec_cfg_data.getTag()] = codec_cfg_data;

  for (auto& codec_capability : codec_capabilities) {
    auto cfg = cfg_tag_map.find(cap_to_cfg_tag_map[codec_capability.getTag()]);
    // Cannot find tag for the capability:
    if (cfg == cfg_tag_map.end()) return false;

    // Matching logic for sampling frequency
    if (codec_capability.getTag() ==
        CodecSpecificCapabilitiesLtv::Tag::supportedSamplingFrequencies) {
      if (!isMatchedSamplingFreq(
              cfg->second
                  .get<CodecSpecificConfigurationLtv::Tag::samplingFrequency>(),
              codec_capability.get<CodecSpecificCapabilitiesLtv::Tag::
                                       supportedSamplingFrequencies>()))
        return false;
    } else if (codec_capability.getTag() ==
               CodecSpecificCapabilitiesLtv::Tag::supportedFrameDurations) {
      if (!isMatchedFrameDuration(
              cfg->second
                  .get<CodecSpecificConfigurationLtv::Tag::frameDuration>(),
              codec_capability.get<CodecSpecificCapabilitiesLtv::Tag::
                                       supportedFrameDurations>()))
        return false;
    } else if (codec_capability.getTag() ==
               CodecSpecificCapabilitiesLtv::Tag::supportedAudioChannelCounts) {
      if (!isMatchedAudioChannel(
              cfg->second.get<
                  CodecSpecificConfigurationLtv::Tag::audioChannelAllocation>(),
              codec_capability.get<CodecSpecificCapabilitiesLtv::Tag::
                                       supportedAudioChannelCounts>()))
        return false;
    } else if (codec_capability.getTag() == CodecSpecificCapabilitiesLtv::Tag::
                                                supportedMaxCodecFramesPerSDU) {
      if (!isMatchedCodecFramesPerSDU(
              cfg->second.get<
                  CodecSpecificConfigurationLtv::Tag::codecFrameBlocksPerSDU>(),
              codec_capability.get<CodecSpecificCapabilitiesLtv::Tag::
                                       supportedMaxCodecFramesPerSDU>()))
        return false;
    } else if (codec_capability.getTag() == CodecSpecificCapabilitiesLtv::Tag::
                                                supportedOctetsPerCodecFrame) {
      if (!isMatchedOctetsPerCodecFrame(
              cfg->second.get<
                  CodecSpecificConfigurationLtv::Tag::octetsPerCodecFrame>(),
              codec_capability.get<CodecSpecificCapabilitiesLtv::Tag::
                                       supportedOctetsPerCodecFrame>()))
        return false;
    }
  }

  return true;
}

bool LeAudioOffloadAudioProvider::isMatchedAseConfiguration(
    LeAudioAseConfiguration setting_cfg,
    LeAudioAseConfiguration requirement_cfg) {
  // Check matching for codec configuration <=> requirement ASE codec
  // Also match if no CodecId requirement
  if (requirement_cfg.codecId.has_value()) {
    if (!setting_cfg.codecId.has_value()) return false;
    if (!isMatchedValidCodec(setting_cfg.codecId.value(),
                             requirement_cfg.codecId.value()))
      return false;
  }

  if (setting_cfg.targetLatency != requirement_cfg.targetLatency) return false;
  // Ignore PHY requirement

  // Check all codec configuration
  std::map<CodecSpecificConfigurationLtv::Tag, CodecSpecificConfigurationLtv>
      cfg_tag_map;
  for (auto cfg : setting_cfg.codecConfiguration)
    cfg_tag_map[cfg.getTag()] = cfg;

  for (auto requirement_cfg : requirement_cfg.codecConfiguration) {
    // Directly compare CodecSpecificConfigurationLtv
    auto cfg = cfg_tag_map.find(requirement_cfg.getTag());
    if (cfg == cfg_tag_map.end()) return false;

    if (cfg->second != requirement_cfg) return false;
  }
  // Ignore vendor configuration and metadata requirement

  return true;
}

bool LeAudioOffloadAudioProvider::isMatchedBISConfiguration(
    LeAudioBisConfiguration bis_cfg,
    const IBluetoothAudioProvider::LeAudioDeviceCapabilities& capabilities) {
  if (!isMatchedValidCodec(bis_cfg.codecId, capabilities.codecId)) return false;
  if (!isCapabilitiesMatchedCodecConfiguration(
          bis_cfg.codecConfiguration, capabilities.codecSpecificCapabilities))
    return false;
  return true;
}

void LeAudioOffloadAudioProvider::filterCapabilitiesAseDirectionConfiguration(
    std::vector<std::optional<AseDirectionConfiguration>>&
        direction_configurations,
    const IBluetoothAudioProvider::LeAudioDeviceCapabilities& capabilities,
    std::vector<std::optional<AseDirectionConfiguration>>&
        valid_direction_configurations) {
  for (auto direction_configuration : direction_configurations) {
    if (!direction_configuration.has_value()) continue;
    if (!direction_configuration.value().aseConfiguration.codecId.has_value())
      continue;
    if (!isMatchedValidCodec(
            direction_configuration.value().aseConfiguration.codecId.value(),
            capabilities.codecId))
      continue;
    // Check matching for codec configuration <=> codec capabilities
    if (!isCapabilitiesMatchedCodecConfiguration(
            direction_configuration.value().aseConfiguration.codecConfiguration,
            capabilities.codecSpecificCapabilities))
      continue;
    valid_direction_configurations.push_back(direction_configuration);
  }
}

void LeAudioOffloadAudioProvider::filterRequirementAseDirectionConfiguration(
    std::vector<std::optional<AseDirectionConfiguration>>&
        direction_configurations,
    const std::optional<std::vector<std::optional<AseDirectionRequirement>>>&
        requirements,
    std::vector<std::optional<AseDirectionConfiguration>>&
        valid_direction_configurations) {
  for (auto direction_configuration : direction_configurations) {
    if (!requirements.has_value()) {
      // If there's no requirement, all are valid
      valid_direction_configurations.push_back(direction_configuration);
      continue;
    }
    if (!direction_configuration.has_value()) continue;

    for (auto& requirement : requirements.value()) {
      if (!requirement.has_value()) continue;
      if (!isMatchedAseConfiguration(
              direction_configuration.value().aseConfiguration,
              requirement.value().aseConfiguration))
        continue;
      // Valid if match any requirement.
      valid_direction_configurations.push_back(direction_configuration);
      break;
    }
  }
}

/* Get a new LeAudioAseConfigurationSetting by matching a setting with a
 * capabilities. The new setting will have a filtered list of
 * AseDirectionConfiguration that matched the capabilities */
std::optional<LeAudioAseConfigurationSetting>
LeAudioOffloadAudioProvider::getCapabilitiesMatchedAseConfigurationSettings(
    IBluetoothAudioProvider::LeAudioAseConfigurationSetting& setting,
    const IBluetoothAudioProvider::LeAudioDeviceCapabilities& capabilities,
    uint8_t direction) {
  // Try to match context in metadata.
  if (!isCapabilitiesMatchedContext(setting.audioContext, capabilities))
    return std::nullopt;

  // Get a list of all matched AseDirectionConfiguration
  // for the input direction
  std::vector<std::optional<AseDirectionConfiguration>>*
      direction_configuration = nullptr;
  if (direction == kLeAudioDirectionSink) {
    if (!setting.sinkAseConfiguration.has_value()) return std::nullopt;
    direction_configuration = &setting.sinkAseConfiguration.value();
  } else {
    if (!setting.sourceAseConfiguration.has_value()) return std::nullopt;
    direction_configuration = &setting.sourceAseConfiguration.value();
  }
  std::vector<std::optional<AseDirectionConfiguration>>
      valid_direction_configuration;
  filterCapabilitiesAseDirectionConfiguration(
      *direction_configuration, capabilities, valid_direction_configuration);
  if (valid_direction_configuration.empty()) return std::nullopt;

  // Create a new LeAudioAseConfigurationSetting and return
  LeAudioAseConfigurationSetting filtered_setting;
  filtered_setting.audioContext = setting.audioContext;
  filtered_setting.packing = setting.packing;
  if (direction == kLeAudioDirectionSink) {
    filtered_setting.sinkAseConfiguration = valid_direction_configuration;
  } else {
    filtered_setting.sourceAseConfiguration = valid_direction_configuration;
  }
  filtered_setting.flags = setting.flags;

  return filtered_setting;
}

/* Get a new LeAudioAseConfigurationSetting by matching a setting with a
 * requirement. The new setting will have a filtered list of
 * AseDirectionConfiguration that matched the requirement */
std::optional<LeAudioAseConfigurationSetting>
LeAudioOffloadAudioProvider::getRequirementMatchedAseConfigurationSettings(
    IBluetoothAudioProvider::LeAudioAseConfigurationSetting& setting,
    const IBluetoothAudioProvider::LeAudioConfigurationRequirement&
        requirement) {
  // Try to match context in metadata.
  if (setting.audioContext != requirement.audioContext) return std::nullopt;

  // Check requirement for the correct direction
  const std::optional<std::vector<std::optional<AseDirectionRequirement>>>*
      direction_requirement;
  std::vector<std::optional<AseDirectionConfiguration>>*
      direction_configuration;
  if (setting.sinkAseConfiguration.has_value()) {
    direction_configuration = &setting.sinkAseConfiguration.value();
    direction_requirement = &requirement.sinkAseRequirement;
  } else {
    direction_configuration = &setting.sourceAseConfiguration.value();
    direction_requirement = &requirement.sourceAseRequirement;
  }

  std::vector<std::optional<AseDirectionConfiguration>>
      valid_direction_configuration;
  filterRequirementAseDirectionConfiguration(*direction_configuration,
                                             *direction_requirement,
                                             valid_direction_configuration);
  if (valid_direction_configuration.empty()) return std::nullopt;

  // Create a new LeAudioAseConfigurationSetting and return
  LeAudioAseConfigurationSetting filtered_setting;
  filtered_setting.audioContext = setting.audioContext;
  filtered_setting.packing = setting.packing;
  if (setting.sinkAseConfiguration.has_value())
    filtered_setting.sinkAseConfiguration = valid_direction_configuration;
  else
    filtered_setting.sourceAseConfiguration = valid_direction_configuration;
  filtered_setting.flags = setting.flags;

  return filtered_setting;
}

ndk::ScopedAStatus LeAudioOffloadAudioProvider::getLeAudioAseConfiguration(
    const std::optional<std::vector<
        std::optional<IBluetoothAudioProvider::LeAudioDeviceCapabilities>>>&
        in_remoteSinkAudioCapabilities,
    const std::optional<std::vector<
        std::optional<IBluetoothAudioProvider::LeAudioDeviceCapabilities>>>&
        in_remoteSourceAudioCapabilities,
    const std::vector<IBluetoothAudioProvider::LeAudioConfigurationRequirement>&
        in_requirements,
    std::vector<IBluetoothAudioProvider::LeAudioAseConfigurationSetting>*
        _aidl_return) {
  // Get all configuration settings
  std::vector<IBluetoothAudioProvider::LeAudioAseConfigurationSetting>
      ase_configuration_settings =
          BluetoothAudioCodecs::GetLeAudioAseConfigurationSettings();

  // Currently won't handle case where both sink and source capabilities
  // are passed in. Only handle one of them.
  const std::optional<std::vector<
      std::optional<IBluetoothAudioProvider::LeAudioDeviceCapabilities>>>*
      in_remoteAudioCapabilities;
  uint8_t direction = 0;
  if (in_remoteSinkAudioCapabilities.has_value()) {
    direction = kLeAudioDirectionSink;
    in_remoteAudioCapabilities = &in_remoteSinkAudioCapabilities;
  } else {
    direction = kLeAudioDirectionSource;
    in_remoteAudioCapabilities = &in_remoteSourceAudioCapabilities;
  }

  std::vector<IBluetoothAudioProvider::LeAudioAseConfigurationSetting>
      capability_matched_ase_configuration_settings;
  // Matching with remote capabilities
  for (auto& setting : ase_configuration_settings) {
    for (auto& capability : in_remoteAudioCapabilities->value()) {
      if (!capability.has_value()) continue;
      auto filtered_ase_configuration_setting =
          getCapabilitiesMatchedAseConfigurationSettings(
              setting, capability.value(), direction);
      if (filtered_ase_configuration_setting.has_value()) {
        capability_matched_ase_configuration_settings.push_back(
            filtered_ase_configuration_setting.value());
      }
    }
  }

  // Matching with requirements
  std::vector<IBluetoothAudioProvider::LeAudioAseConfigurationSetting> result;
  for (auto& setting : capability_matched_ase_configuration_settings) {
    for (auto& requirement : in_requirements) {
      auto filtered_ase_configuration_setting =
          getRequirementMatchedAseConfigurationSettings(setting, requirement);
      if (filtered_ase_configuration_setting.has_value()) {
        result.push_back(filtered_ase_configuration_setting.value());
      }
    }
  }

  *_aidl_return = result;
  return ndk::ScopedAStatus::ok();
};

bool LeAudioOffloadAudioProvider::isMatchedQosRequirement(
    LeAudioAseQosConfiguration setting_qos,
    AseQosDirectionRequirement requirement_qos) {
  if (setting_qos.retransmissionNum !=
      requirement_qos.preferredRetransmissionNum)
    return false;
  if (setting_qos.maxTransportLatencyMs > requirement_qos.maxTransportLatencyMs)
    return false;
  // Ignore other parameters, as they are not populated in the setting_qos
  return true;
}

ndk::ScopedAStatus LeAudioOffloadAudioProvider::getLeAudioAseQosConfiguration(
    const IBluetoothAudioProvider::LeAudioAseQosConfigurationRequirement&
        in_qosRequirement,
    IBluetoothAudioProvider::LeAudioAseQosConfigurationPair* _aidl_return) {
  IBluetoothAudioProvider::LeAudioAseQosConfigurationPair result;
  // Get all configuration settings
  std::vector<IBluetoothAudioProvider::LeAudioAseConfigurationSetting>
      ase_configuration_settings =
          BluetoothAudioCodecs::GetLeAudioAseConfigurationSettings();

  // Direction QoS matching
  // Only handle one direction input case
  uint8_t direction = 0;
  std::optional<AseQosDirectionRequirement> direction_qos_requirement =
      std::nullopt;
  if (in_qosRequirement.sinkAseQosRequirement.has_value()) {
    direction_qos_requirement = in_qosRequirement.sinkAseQosRequirement.value();
    direction = kLeAudioDirectionSink;
  } else if (in_qosRequirement.sourceAseQosRequirement.has_value()) {
    direction_qos_requirement =
        in_qosRequirement.sourceAseQosRequirement.value();
    direction = kLeAudioDirectionSource;
  }

  for (auto& setting : ase_configuration_settings) {
    // Context matching
    if (setting.audioContext != in_qosRequirement.audioContext) continue;

    // Match configuration flags
    // Currently configuration flags are not populated, ignore.

    // Get a list of all matched AseDirectionConfiguration
    // for the input direction
    std::vector<std::optional<AseDirectionConfiguration>>*
        direction_configuration = nullptr;
    if (direction == kLeAudioDirectionSink) {
      if (!setting.sinkAseConfiguration.has_value()) continue;
      direction_configuration = &setting.sinkAseConfiguration.value();
    } else {
      if (!setting.sourceAseConfiguration.has_value()) continue;
      direction_configuration = &setting.sourceAseConfiguration.value();
    }

    for (auto cfg : *direction_configuration) {
      if (!cfg.has_value()) continue;
      // If no requirement, return the first QoS
      if (!direction_qos_requirement.has_value()) {
        result.sinkQosConfiguration = cfg.value().qosConfiguration;
        result.sourceQosConfiguration = cfg.value().qosConfiguration;
        *_aidl_return = result;
        return ndk::ScopedAStatus::ok();
      }

      // If has requirement, return the first matched QoS
      // Try to match the ASE configuration
      // and QoS with requirement
      if (!cfg.value().qosConfiguration.has_value()) continue;
      if (isMatchedAseConfiguration(
              cfg.value().aseConfiguration,
              direction_qos_requirement.value().aseConfiguration) &&
          isMatchedQosRequirement(cfg.value().qosConfiguration.value(),
                                  direction_qos_requirement.value())) {
        if (direction == kLeAudioDirectionSink)
          result.sinkQosConfiguration = cfg.value().qosConfiguration;
        else
          result.sourceQosConfiguration = cfg.value().qosConfiguration;
        *_aidl_return = result;
        return ndk::ScopedAStatus::ok();
      }
    }
  }

  // No match, return empty QoS
  *_aidl_return = result;
  return ndk::ScopedAStatus::ok();
};

ndk::ScopedAStatus LeAudioOffloadAudioProvider::onSinkAseMetadataChanged(
    IBluetoothAudioProvider::AseState in_state, int32_t /*in_cigId*/,
    int32_t /*in_cisId*/,
    const std::optional<std::vector<std::optional<MetadataLtv>>>& in_metadata) {
  (void)in_state;
  (void)in_metadata;
  return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
};

ndk::ScopedAStatus LeAudioOffloadAudioProvider::onSourceAseMetadataChanged(
    IBluetoothAudioProvider::AseState in_state, int32_t /*in_cigId*/,
    int32_t /*in_cisId*/,
    const std::optional<std::vector<std::optional<MetadataLtv>>>& in_metadata) {
  (void)in_state;
  (void)in_metadata;
  return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
};

void LeAudioOffloadAudioProvider::getBroadcastSettings() {
  if (!broadcast_settings.empty()) return;

  LOG(INFO) << __func__ << ": Loading broadcast settings from provider info";

  std::vector<CodecInfo> db_codec_info =
      BluetoothAudioCodecs::GetLeAudioOffloadCodecInfo(
          SessionType::LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH);
  broadcast_settings.clear();
  CodecSpecificConfigurationLtv::AudioChannelAllocation default_allocation;
  default_allocation.bitmask =
      CodecSpecificConfigurationLtv::AudioChannelAllocation::FRONT_CENTER;

  for (auto& codec_info : db_codec_info) {
    if (codec_info.transport.getTag() != CodecInfo::Transport::leAudio)
      continue;
    auto& transport = codec_info.transport.get<CodecInfo::Transport::leAudio>();
    LeAudioBroadcastConfigurationSetting setting;
    // Default setting
    setting.numBis = 1;
    setting.phy = {Phy::TWO_M};
    // Populate BIS configuration info using codec_info
    LeAudioBisConfiguration bis_cfg;
    bis_cfg.codecId = codec_info.id;

    CodecSpecificConfigurationLtv::OctetsPerCodecFrame octets;
    octets.value = transport.bitdepth[0];

    bis_cfg.codecConfiguration = {
        sampling_freq_map[transport.samplingFrequencyHz[0]], octets,
        frame_duration_map[transport.frameDurationUs[0]], default_allocation};

    // Add information to structure
    IBluetoothAudioProvider::LeAudioSubgroupBisConfiguration sub_bis_cfg;
    sub_bis_cfg.numBis = 1;
    sub_bis_cfg.bisConfiguration = bis_cfg;
    IBluetoothAudioProvider::LeAudioBroadcastSubgroupConfiguration sub_cfg;
    sub_cfg.bisConfigurations = {sub_bis_cfg};
    setting.subgroupsConfigurations = {sub_cfg};

    broadcast_settings.push_back(setting);
  }

  LOG(INFO) << __func__
            << ": Done loading broadcast settings from provider info";
}

/* Get a new LeAudioAseConfigurationSetting by matching a setting with a
 * capabilities. The new setting will have a filtered list of
 * AseDirectionConfiguration that matched the capabilities */
std::optional<LeAudioBroadcastConfigurationSetting>
LeAudioOffloadAudioProvider::
    getCapabilitiesMatchedBroadcastConfigurationSettings(
        LeAudioBroadcastConfigurationSetting& setting,
        const IBluetoothAudioProvider::LeAudioDeviceCapabilities&
            capabilities) {
  std::vector<IBluetoothAudioProvider::LeAudioBroadcastSubgroupConfiguration>
      filter_subgroup;
  for (auto& sub_cfg : setting.subgroupsConfigurations) {
    std::vector<IBluetoothAudioProvider::LeAudioSubgroupBisConfiguration>
        filtered_bis_cfg;
    for (auto& bis_cfg : sub_cfg.bisConfigurations)
      if (isMatchedBISConfiguration(bis_cfg.bisConfiguration, capabilities)) {
        filtered_bis_cfg.push_back(bis_cfg);
      }
    if (!filtered_bis_cfg.empty()) {
      IBluetoothAudioProvider::LeAudioBroadcastSubgroupConfiguration
          subgroup_cfg;
      subgroup_cfg.bisConfigurations = filtered_bis_cfg;
      filter_subgroup.push_back(subgroup_cfg);
    }
  }
  if (filter_subgroup.empty()) return std::nullopt;

  // Create a new LeAudioAseConfigurationSetting and return
  LeAudioBroadcastConfigurationSetting filtered_setting(setting);
  filtered_setting.subgroupsConfigurations = filter_subgroup;

  return filtered_setting;
}

ndk::ScopedAStatus
LeAudioOffloadAudioProvider::getLeAudioBroadcastConfiguration(
    const std::optional<std::vector<
        std::optional<IBluetoothAudioProvider::LeAudioDeviceCapabilities>>>&
        in_remoteSinkAudioCapabilities,
    const IBluetoothAudioProvider::LeAudioBroadcastConfigurationRequirement&
        in_requirement,
    LeAudioBroadcastConfigurationSetting* _aidl_return) {
  getBroadcastSettings();
  _aidl_return = nullptr;

  // Match and filter capability
  std::vector<LeAudioBroadcastConfigurationSetting> filtered_settings;
  if (!in_remoteSinkAudioCapabilities.has_value()) {
    LOG(WARNING) << __func__ << ": Empty capability";
    return ndk::ScopedAStatus::ok();
  }
  for (auto& setting : broadcast_settings) {
    for (auto& capability : in_remoteSinkAudioCapabilities.value()) {
      if (!capability.has_value()) continue;
      auto filtered_setting =
          getCapabilitiesMatchedBroadcastConfigurationSettings(
              setting, capability.value());
      if (filtered_setting.has_value())
        filtered_settings.push_back(filtered_setting.value());
    }
  }

  if (filtered_settings.empty()) {
    LOG(WARNING) << __func__ << ": Cannot match any remote capability";
    return ndk::ScopedAStatus::ok();
  }

  // Match and return the first matched requirement
  if (in_requirement.subgroupConfigurationRequirements.empty()) {
    LOG(INFO) << __func__ << ": Empty requirement";
    *_aidl_return = filtered_settings[0];
    return ndk::ScopedAStatus::ok();
  }

  for (auto& setting : filtered_settings) {
    // Further filter out bis configuration
    LeAudioBroadcastConfigurationSetting filtered_setting(setting);
    filtered_setting.subgroupsConfigurations.clear();
    for (auto& sub_cfg : setting.subgroupsConfigurations) {
      bool isMatched = false;
      for (auto& sub_req : in_requirement.subgroupConfigurationRequirements) {
        // Matching number of BIS
        if (sub_req.bisNumPerSubgroup != sub_cfg.bisConfigurations.size())
          continue;
        // Currently will ignore quality and context hint.
        isMatched = true;
        break;
      }
      if (isMatched)
        filtered_setting.subgroupsConfigurations.push_back(sub_cfg);
    }
    // Return the first match
    if (!filtered_setting.subgroupsConfigurations.empty()) {
      LOG(INFO) << __func__ << ": Matched requirement";
      *_aidl_return = filtered_setting;
      return ndk::ScopedAStatus::ok();
    }
  }

  LOG(WARNING) << __func__ << ": Cannot match any requirement";
  return ndk::ScopedAStatus::ok();
};

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
