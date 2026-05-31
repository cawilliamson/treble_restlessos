
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

#include <aidl/android/hardware/bluetooth/audio/IBluetoothAudioProvider.h>

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>

#include "audio_set_configurations_generated.h"
#include "audio_set_scenarios_generated.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

using LeAudioAseConfigurationSetting =
    IBluetoothAudioProvider::LeAudioAseConfigurationSetting;
using AseDirectionConfiguration = IBluetoothAudioProvider::
    LeAudioAseConfigurationSetting::AseDirectionConfiguration;
using LeAudioAseQosConfiguration =
    IBluetoothAudioProvider::LeAudioAseQosConfiguration;
using LeAudioDataPathConfiguration =
    IBluetoothAudioProvider::LeAudioDataPathConfiguration;

enum class CodecLocation {
  HOST,
  ADSP,
  CONTROLLER,
};

class AudioSetConfigurationProviderJson {
 public:
  static std::vector<LeAudioAseConfigurationSetting>
  GetLeAudioAseConfigurationSettings();

 private:
  static void LoadAudioSetConfigurationProviderJson();

  static const le_audio::CodecSpecificConfiguration* LookupCodecSpecificParam(
      const flatbuffers::Vector<flatbuffers::Offset<
          le_audio::CodecSpecificConfiguration>>* flat_codec_specific_params,
      le_audio::CodecSpecificLtvGenericTypes type);

  static void populateAudioChannelAllocation(
      CodecSpecificConfigurationLtv::AudioChannelAllocation&
          audio_channel_allocation,
      uint32_t audio_location);

  static void populateConfigurationData(
      LeAudioAseConfiguration& ase,
      const flatbuffers::Vector<
          flatbuffers::Offset<le_audio::CodecSpecificConfiguration>>*
          flat_codec_specific_params);

  static void populateAseConfiguration(
      LeAudioAseConfiguration& ase,
      const le_audio::AudioSetSubConfiguration* flat_subconfig,
      const le_audio::QosConfiguration* qos_cfg);

  static void populateAseQosConfiguration(
      LeAudioAseQosConfiguration& qos,
      const le_audio::QosConfiguration* qos_cfg);

  static AseDirectionConfiguration SetConfigurationFromFlatSubconfig(
      const le_audio::AudioSetSubConfiguration* flat_subconfig,
      const le_audio::QosConfiguration* qos_cfg, CodecLocation location);

  static void processSubconfig(
      const le_audio::AudioSetSubConfiguration* subconfig,
      const le_audio::QosConfiguration* qos_cfg,
      std::vector<std::optional<AseDirectionConfiguration>>&
          directionAseConfiguration,
      CodecLocation location);

  static void PopulateAseConfigurationFromFlat(
      const le_audio::AudioSetConfiguration* flat_cfg,
      std::vector<const le_audio::CodecConfiguration*>* codec_cfgs,
      std::vector<const le_audio::QosConfiguration*>* qos_cfgs,
      CodecLocation location,
      std::vector<std::optional<AseDirectionConfiguration>>&
          sourceAseConfiguration,
      std::vector<std::optional<AseDirectionConfiguration>>&
          sinkAseConfiguration,
      ConfigurationFlags& configurationFlags);

  static bool LoadConfigurationsFromFiles(const char* schema_file,
                                          const char* content_file,
                                          CodecLocation location);

  static bool LoadScenariosFromFiles(const char* schema_file,
                                     const char* content_file);

  static bool LoadContent(
      std::vector<std::pair<const char* /*schema*/, const char* /*content*/>>
          config_files,
      std::vector<std::pair<const char* /*schema*/, const char* /*content*/>>
          scenario_files,
      CodecLocation location);
};

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
