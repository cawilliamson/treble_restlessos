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

#define LOG_TAG "BTAudioProviderFactoryAIDL"

#include "BluetoothAudioProviderFactory.h"

#include <BluetoothAudioCodecs.h>
#include <android-base/logging.h>

#include "A2dpOffloadAudioProvider.h"
#include "A2dpSoftwareAudioProvider.h"
#include "BluetoothAudioProvider.h"
#include "HearingAidAudioProvider.h"
#include "HfpOffloadAudioProvider.h"
#include "HfpSoftwareAudioProvider.h"
#include "LeAudioOffloadAudioProvider.h"
#include "LeAudioSoftwareAudioProvider.h"

namespace aidl {
namespace android {
namespace hardware {
namespace bluetooth {
namespace audio {

static const std::string kLeAudioOffloadProviderName =
    "LE_AUDIO_OFFLOAD_HARDWARE_OFFLOAD_PROVIDER";

BluetoothAudioProviderFactory::BluetoothAudioProviderFactory() {}

ndk::ScopedAStatus BluetoothAudioProviderFactory::openProvider(
    const SessionType session_type,
    std::shared_ptr<IBluetoothAudioProvider>* _aidl_return) {
  LOG(INFO) << __func__ << " - SessionType=" << toString(session_type);
  std::shared_ptr<BluetoothAudioProvider> provider = nullptr;

  switch (session_type) {
    case SessionType::A2DP_SOFTWARE_ENCODING_DATAPATH:
      provider = ndk::SharedRefBase::make<A2dpSoftwareEncodingAudioProvider>();
      break;
    case SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH:
      provider = ndk::SharedRefBase::make<A2dpOffloadEncodingAudioProvider>(
          a2dp_offload_codec_factory_);
      break;
    case SessionType::HEARING_AID_SOFTWARE_ENCODING_DATAPATH:
      provider = ndk::SharedRefBase::make<HearingAidAudioProvider>();
      break;
    case SessionType::LE_AUDIO_SOFTWARE_ENCODING_DATAPATH:
      provider = ndk::SharedRefBase::make<LeAudioSoftwareOutputAudioProvider>();
      break;
    case SessionType::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH:
      provider = ndk::SharedRefBase::make<LeAudioOffloadOutputAudioProvider>();
      break;
    case SessionType::LE_AUDIO_SOFTWARE_DECODING_DATAPATH:
      provider = ndk::SharedRefBase::make<LeAudioSoftwareInputAudioProvider>();
      break;
    case SessionType::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH:
      provider = ndk::SharedRefBase::make<LeAudioOffloadInputAudioProvider>();
      break;
    case SessionType::LE_AUDIO_BROADCAST_SOFTWARE_ENCODING_DATAPATH:
      provider =
          ndk::SharedRefBase::make<LeAudioSoftwareBroadcastAudioProvider>();
      break;
    case SessionType::LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH:
      provider =
          ndk::SharedRefBase::make<LeAudioOffloadBroadcastAudioProvider>();
      break;
    case SessionType::A2DP_SOFTWARE_DECODING_DATAPATH:
      provider = ndk::SharedRefBase::make<A2dpSoftwareDecodingAudioProvider>();
      break;
    case SessionType::A2DP_HARDWARE_OFFLOAD_DECODING_DATAPATH:
      provider = ndk::SharedRefBase::make<A2dpOffloadDecodingAudioProvider>(
          a2dp_offload_codec_factory_);
      break;
    case SessionType::HFP_SOFTWARE_ENCODING_DATAPATH:
      provider = ndk::SharedRefBase::make<HfpSoftwareOutputAudioProvider>();
      break;
    case SessionType::HFP_SOFTWARE_DECODING_DATAPATH:
      provider = ndk::SharedRefBase::make<HfpSoftwareInputAudioProvider>();
      break;
    case SessionType::HFP_HARDWARE_OFFLOAD_DATAPATH:
      provider = ndk::SharedRefBase::make<HfpOffloadAudioProvider>();
      break;
    default:
      provider = nullptr;
      break;
  }

  if (provider == nullptr || !provider->isValid(session_type)) {
    provider = nullptr;
    LOG(ERROR) << __func__ << " - SessionType=" << toString(session_type);
    return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
  }
  *_aidl_return = provider;

  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BluetoothAudioProviderFactory::getProviderCapabilities(
    const SessionType session_type,
    std::vector<AudioCapabilities>* _aidl_return) {
  if (session_type == SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH ||
      session_type == SessionType::A2DP_HARDWARE_OFFLOAD_DECODING_DATAPATH) {
    auto codec_capabilities =
        BluetoothAudioCodecs::GetA2dpOffloadCodecCapabilities(session_type);
    _aidl_return->resize(codec_capabilities.size());
    for (int i = 0; i < codec_capabilities.size(); i++) {
      _aidl_return->at(i).set<AudioCapabilities::a2dpCapabilities>(
          codec_capabilities[i]);
    }
  } else if (session_type ==
                 SessionType::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH ||
             session_type ==
                 SessionType::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH ||
             session_type ==
                 SessionType::
                     LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH) {
    std::vector<LeAudioCodecCapabilitiesSetting> db_codec_capabilities =
        BluetoothAudioCodecs::GetLeAudioOffloadCodecCapabilities(session_type);
    if (db_codec_capabilities.size()) {
      _aidl_return->resize(db_codec_capabilities.size());
      for (int i = 0; i < db_codec_capabilities.size(); ++i) {
        _aidl_return->at(i).set<AudioCapabilities::leAudioCapabilities>(
            db_codec_capabilities[i]);
      }
    }
  } else if (session_type != SessionType::UNKNOWN) {
    auto pcm_capabilities = BluetoothAudioCodecs::GetSoftwarePcmCapabilities();
    _aidl_return->resize(pcm_capabilities.size());
    for (int i = 0; i < pcm_capabilities.size(); i++) {
      _aidl_return->at(i).set<AudioCapabilities::pcmCapabilities>(
          pcm_capabilities[i]);
    }
  }

  LOG(INFO) << __func__ << " - SessionType=" << toString(session_type)
            << " supports " << _aidl_return->size() << " codecs";
  return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus BluetoothAudioProviderFactory::getProviderInfo(
    SessionType session_type, std::optional<ProviderInfo>* _aidl_return) {
  *_aidl_return = std::nullopt;

  LOG(INFO) << __func__ << " - SessionType=" << toString(session_type);

  if (session_type == SessionType::A2DP_HARDWARE_OFFLOAD_ENCODING_DATAPATH ||
      session_type == SessionType::A2DP_HARDWARE_OFFLOAD_DECODING_DATAPATH) {
    if (!kEnableA2dpCodecExtensibility) {
      // Implementing getProviderInfo equates supporting
      // A2dp codec extensibility.
      return ndk::ScopedAStatus::fromStatus(STATUS_UNKNOWN_TRANSACTION);
    }

    auto& provider_info = _aidl_return->emplace();

    provider_info.name = a2dp_offload_codec_factory_.name;
    for (auto codec : a2dp_offload_codec_factory_.codecs)
      provider_info.codecInfos.push_back(codec->info);
  }

  if (session_type ==
          SessionType::LE_AUDIO_HARDWARE_OFFLOAD_ENCODING_DATAPATH ||
      session_type ==
          SessionType::LE_AUDIO_HARDWARE_OFFLOAD_DECODING_DATAPATH ||
      session_type ==
          SessionType::LE_AUDIO_BROADCAST_HARDWARE_OFFLOAD_ENCODING_DATAPATH) {
    std::vector<CodecInfo> db_codec_info =
        BluetoothAudioCodecs::GetLeAudioOffloadCodecInfo(session_type);
    if (!db_codec_info.empty()) {
      auto& provider_info = _aidl_return->emplace();
      provider_info.name = kLeAudioOffloadProviderName;
      provider_info.codecInfos = db_codec_info;
      *_aidl_return = provider_info;
      return ndk::ScopedAStatus::ok();
    }
  }

  return ndk::ScopedAStatus::ok();
}

}  // namespace audio
}  // namespace bluetooth
}  // namespace hardware
}  // namespace android
}  // namespace aidl
