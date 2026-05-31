/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "DeviceHAL"

#include "Device.h"
#include "common/all-versions/default/EffectMap.h"
#include "StreamIn.h"
#include "StreamOut.h"
#include "Util.h"

//#define LOG_NDEBUG 0

#include <inttypes.h>
#include <memory.h>
#include <string.h>
#include <algorithm>

#include <android/log.h>
#include <mediautils/MemoryLeakTrackUtil.h>
#include <memunreachable/memunreachable.h>

#include <HidlUtils.h>

namespace android {
namespace hardware {
namespace audio {
namespace CPP_VERSION {
namespace implementation {

using ::android::hardware::audio::common::COMMON_TYPES_CPP_VERSION::implementation::HidlUtils;
namespace util {
using namespace ::android::hardware::audio::CORE_TYPES_CPP_VERSION::implementation::util;
}

Device::Device(audio_hw_device_t* device) : mIsClosed(false), mDevice(device) {}

Device::~Device() {
    (void)doClose();
    mDevice = nullptr;
}

Result Device::analyzeStatus(const char* funcName, int status,
                             const std::vector<int>& ignoreErrors) {
    return util::analyzeStatus("Device", funcName, status, ignoreErrors);
}

void Device::closeInputStream(audio_stream_in_t* stream) {
    mDevice->close_input_stream(mDevice, stream);
    LOG_ALWAYS_FATAL_IF(mOpenedStreamsCount == 0, "mOpenedStreamsCount is already 0");
    --mOpenedStreamsCount;
}

void Device::closeOutputStream(audio_stream_out_t* stream) {
    mDevice->close_output_stream(mDevice, stream);
    LOG_ALWAYS_FATAL_IF(mOpenedStreamsCount == 0, "mOpenedStreamsCount is already 0");
    --mOpenedStreamsCount;
}

char* Device::halGetParameters(const char* keys) {
    return mDevice->get_parameters(mDevice, keys);
}

int Device::halSetParameters(const char* keysAndValues) {
    return mDevice->set_parameters(mDevice, keysAndValues);
}

// Methods from ::android::hardware::audio::CPP_VERSION::IDevice follow.
Return<Result> Device::initCheck() {
    return analyzeStatus("init_check", mDevice->init_check(mDevice));
}

Return<Result> Device::setMasterVolume(float volume) {
    if (mDevice->set_master_volume == NULL) {
        return Result::NOT_SUPPORTED;
    }
    if (!util::isGainNormalized(volume)) {
        ALOGW("Can not set a master volume (%f) outside [0,1]", volume);
        return Result::INVALID_ARGUMENTS;
    }
    return analyzeStatus("set_master_volume", mDevice->set_master_volume(mDevice, volume),
                         {ENOSYS} /*ignore*/);
}

Return<void> Device::getMasterVolume(getMasterVolume_cb _hidl_cb) {
    Result retval(Result::NOT_SUPPORTED);
    float volume = 0;
    if (mDevice->get_master_volume != NULL) {
        retval = analyzeStatus("get_master_volume", mDevice->get_master_volume(mDevice, &volume),
                               {ENOSYS} /*ignore*/);
    }
    _hidl_cb(retval, volume);
    return Void();
}

Return<Result> Device::setMicMute(bool mute) {
    return analyzeStatus("set_mic_mute", mDevice->set_mic_mute(mDevice, mute), {ENOSYS} /*ignore*/);
}

Return<void> Device::getMicMute(getMicMute_cb _hidl_cb) {
    bool mute = false;
    Result retval = analyzeStatus("get_mic_mute", mDevice->get_mic_mute(mDevice, &mute),
                                  {ENOSYS} /*ignore*/);
    _hidl_cb(retval, mute);
    return Void();
}

Return<Result> Device::setMasterMute(bool mute) {
    Result retval(Result::NOT_SUPPORTED);
    if (mDevice->set_master_mute != NULL) {
        retval = analyzeStatus("set_master_mute", mDevice->set_master_mute(mDevice, mute),
                               {ENOSYS} /*ignore*/);
    }
    return retval;
}

Return<void> Device::getMasterMute(getMasterMute_cb _hidl_cb) {
    Result retval(Result::NOT_SUPPORTED);
    bool mute = false;
    if (mDevice->get_master_mute != NULL) {
        retval = analyzeStatus("get_master_mute", mDevice->get_master_mute(mDevice, &mute),
                               {ENOSYS} /*ignore*/);
    }
    _hidl_cb(retval, mute);
    return Void();
}

Return<void> Device::getInputBufferSize(const AudioConfig& config, getInputBufferSize_cb _hidl_cb) {
    audio_config_t halConfig;
    Result retval(Result::INVALID_ARGUMENTS);
    uint64_t bufferSize = 0;
    if (HidlUtils::audioConfigToHal(config, &halConfig) == NO_ERROR) {
        size_t halBufferSize = mDevice->get_input_buffer_size(mDevice, &halConfig);
        if (halBufferSize != 0) {
            retval = Result::OK;
            bufferSize = halBufferSize;
        }
    }
    _hidl_cb(retval, bufferSize);
    return Void();
}

std::tuple<Result, sp<IStreamOut>> Device::openOutputStreamCore(int32_t ioHandle,
                                                                const DeviceAddress& device,
                                                                const AudioConfig& config,
                                                                const AudioOutputFlags& flags,
                                                                AudioConfig* suggestedConfig) {
    audio_config_t halConfig;
    if (HidlUtils::audioConfigToHal(config, &halConfig) != NO_ERROR) {
        return {Result::INVALID_ARGUMENTS, nullptr};
    }
    audio_stream_out_t* halStream;
    audio_devices_t halDevice;
    char halDeviceAddress[AUDIO_DEVICE_MAX_ADDRESS_LEN];
    if (CoreUtils::deviceAddressToHal(device, &halDevice, halDeviceAddress) != NO_ERROR) {
        return {Result::INVALID_ARGUMENTS, nullptr};
    }
    audio_output_flags_t halFlags;
    if (CoreUtils::audioOutputFlagsToHal(flags, &halFlags) != NO_ERROR) {
        return {Result::INVALID_ARGUMENTS, nullptr};
    }
    ALOGV("open_output_stream handle: %d devices: %x flags: %#x "
          "srate: %d format %#x channels %x address %s",
          ioHandle, halDevice, halFlags, halConfig.sample_rate, halConfig.format,
          halConfig.channel_mask, halDeviceAddress);
    int status = mDevice->open_output_stream(mDevice, ioHandle, halDevice, halFlags, &halConfig,
                                             &halStream, halDeviceAddress);
    ALOGV("open_output_stream status %d stream %p", status, halStream);
    sp<IStreamOut> streamOut;
    if (status == OK) {
        streamOut = new StreamOut(this, halStream);
        ++mOpenedStreamsCount;
    }
    status_t convertStatus =
            HidlUtils::audioConfigFromHal(halConfig, false /*isInput*/, suggestedConfig);
    ALOGW_IF(convertStatus != OK, "%s: suggested config with incompatible fields", __func__);
    return {analyzeStatus("open_output_stream", status, {EINVAL} /*ignore*/), streamOut};
}

std::tuple<Result, sp<IStreamIn>> Device::openInputStreamCore(
        int32_t ioHandle, const DeviceAddress& device, const AudioConfig& config,
        const AudioInputFlags& flags, AudioSource source, AudioConfig* suggestedConfig) {
    audio_config_t halConfig;
    if (HidlUtils::audioConfigToHal(config, &halConfig) != NO_ERROR) {
        return {Result::INVALID_ARGUMENTS, nullptr};
    }
    audio_stream_in_t* halStream;
    audio_devices_t halDevice;
    char halDeviceAddress[AUDIO_DEVICE_MAX_ADDRESS_LEN];
    if (CoreUtils::deviceAddressToHal(device, &halDevice, halDeviceAddress) != NO_ERROR) {
        return {Result::INVALID_ARGUMENTS, nullptr};
    }
    audio_input_flags_t halFlags;
    audio_source_t halSource;
    if (CoreUtils::audioInputFlagsToHal(flags, &halFlags) != NO_ERROR ||
        HidlUtils::audioSourceToHal(source, &halSource) != NO_ERROR) {
        return {Result::INVALID_ARGUMENTS, nullptr};
    }
    ALOGV("open_input_stream handle: %d devices: %x flags: %#x "
          "srate: %d format %#x channels %x address %s source %d",
          ioHandle, halDevice, halFlags, halConfig.sample_rate, halConfig.format,
          halConfig.channel_mask, halDeviceAddress, halSource);
    int status = mDevice->open_input_stream(mDevice, ioHandle, halDevice, &halConfig, &halStream,
                                            halFlags, halDeviceAddress, halSource);
    ALOGV("open_input_stream status %d stream %p", status, halStream);
    sp<IStreamIn> streamIn;
    if (status == OK) {
        streamIn = new StreamIn(this, halStream);
        ++mOpenedStreamsCount;
    }
    status_t convertStatus =
            HidlUtils::audioConfigFromHal(halConfig, true /*isInput*/, suggestedConfig);
    ALOGW_IF(convertStatus != OK, "%s: suggested config with incompatible fields", __func__);
    return {analyzeStatus("open_input_stream", status, {EINVAL} /*ignore*/), streamIn};
}

#if MAJOR_VERSION == 2
Return<void> Device::openOutputStream(int32_t ioHandle, const DeviceAddress& device,
                                      const AudioConfig& config, AudioOutputFlags flags,
                                      openOutputStream_cb _hidl_cb) {
    AudioConfig suggestedConfig;
    auto [result, streamOut] =
            openOutputStreamCore(ioHandle, device, config, flags, &suggestedConfig);
    _hidl_cb(result, streamOut, suggestedConfig);
    return Void();
}

Return<void> Device::openInputStream(int32_t ioHandle, const DeviceAddress& device,
                                     const AudioConfig& config, AudioInputFlags flags,
                                     AudioSource source, openInputStream_cb _hidl_cb) {
    AudioConfig suggestedConfig;
    auto [result, streamIn] =
            openInputStreamCore(ioHandle, device, config, flags, source, &suggestedConfig);
    _hidl_cb(result, streamIn, suggestedConfig);
    return Void();
}

#elif MAJOR_VERSION >= 4
std::tuple<Result, sp<IStreamOut>, AudioConfig> Device::openOutputStreamImpl(
        int32_t ioHandle, const DeviceAddress& device, const AudioConfig& config,
        const SourceMetadata& sourceMetadata,
#if MAJOR_VERSION <= 6
        AudioOutputFlags flags) {
    if (status_t status = CoreUtils::sourceMetadataToHal(sourceMetadata, nullptr);
        status != NO_ERROR) {
#else
        const AudioOutputFlags& flags) {
    if (status_t status = CoreUtils::sourceMetadataToHalV7(sourceMetadata,
                                                           false /*ignoreNonVendorTags*/, nullptr);
        status != NO_ERROR) {
#endif
        return {analyzeStatus("sourceMetadataToHal", status), nullptr, {}};
    }
    AudioConfig suggestedConfig;
    auto [result, streamOut] =
            openOutputStreamCore(ioHandle, device, config, flags, &suggestedConfig);
    if (streamOut) {
        streamOut->updateSourceMetadata(sourceMetadata);
    }
    return {result, streamOut, suggestedConfig};
}

Return<void> Device::openOutputStream(int32_t ioHandle, const DeviceAddress& device,
                                      const AudioConfig& config,
#if MAJOR_VERSION <= 6
                                      AudioOutputFlags flags,
#else
                                      const AudioOutputFlags& flags,
#endif
                                      const SourceMetadata& sourceMetadata,
                                      openOutputStream_cb _hidl_cb) {
    auto [result, streamOut, suggestedConfig] =
            openOutputStreamImpl(ioHandle, device, config, sourceMetadata, flags);
    _hidl_cb(result, streamOut, suggestedConfig);
    return Void();
}

std::tuple<Result, sp<IStreamIn>, AudioConfig> Device::openInputStreamImpl(
        int32_t ioHandle, const DeviceAddress& device, const AudioConfig& config,
#if MAJOR_VERSION <= 6
        AudioInputFlags flags,
#else
        const AudioInputFlags& flags,
#endif
        const SinkMetadata& sinkMetadata) {
    if (sinkMetadata.tracks.size() == 0) {
        // This should never happen, the framework must not create as stream
        // if there is no client
        ALOGE("openInputStream called without tracks connected");
        return {Result::INVALID_ARGUMENTS, nullptr, AudioConfig{}};
    }
#if MAJOR_VERSION <= 6
    if (status_t status = CoreUtils::sinkMetadataToHal(sinkMetadata, nullptr); status != NO_ERROR) {
#else
    if (status_t status = CoreUtils::sinkMetadataToHalV7(sinkMetadata,
                                                         false /*ignoreNonVendorTags*/, nullptr);
        status != NO_ERROR) {
#endif
        return {analyzeStatus("sinkMetadataToHal", status), nullptr, AudioConfig{}};
    }
    // Pick the first one as the main.
    AudioSource source = sinkMetadata.tracks[0].source;
    AudioConfig suggestedConfig;
    auto [result, streamIn] =
            openInputStreamCore(ioHandle, device, config, flags, source, &suggestedConfig);
    if (streamIn) {
        streamIn->updateSinkMetadata(sinkMetadata);
    }
    return {result, streamIn, suggestedConfig};
}

Return<void> Device::openInputStream(int32_t ioHandle, const DeviceAddress& device,
                                     const AudioConfig& config,
#if MAJOR_VERSION <= 6
                                     AudioInputFlags flags,
#else
                                     const AudioInputFlags& flags,
#endif
                                     const SinkMetadata& sinkMetadata,
                                     openInputStream_cb _hidl_cb) {
    auto [result, streamIn, suggestedConfig] =
            openInputStreamImpl(ioHandle, device, config, flags, sinkMetadata);
    _hidl_cb(result, streamIn, suggestedConfig);
    return Void();
}
#endif /* MAJOR_VERSION */

#if MAJOR_VERSION == 7 && MINOR_VERSION == 1
Return<void> Device::openOutputStream_7_1(int32_t ioHandle, const DeviceAddress& device,
                                          const AudioConfig& config, const AudioOutputFlags& flags,
                                          const SourceMetadata& sourceMetadata,
                                          openOutputStream_7_1_cb _hidl_cb) {
    auto [result, streamOut, suggestedConfig] =
            openOutputStreamImpl(ioHandle, device, config, sourceMetadata, flags);
    _hidl_cb(result, streamOut, suggestedConfig);
    return Void();
}
#endif  // V7.1

Return<bool> Device::supportsAudioPatches() {
    return version() >= AUDIO_DEVICE_API_VERSION_3_0;
}

Return<void> Device::createAudioPatch(const hidl_vec<AudioPortConfig>& sources,
                                      const hidl_vec<AudioPortConfig>& sinks,
                                      createAudioPatch_cb _hidl_cb) {
    auto [retval, patch] = createOrUpdateAudioPatch(AudioPatchHandle{}, sources, sinks);
    _hidl_cb(retval, patch);
    return Void();
}

std::tuple<Result, AudioPatchHandle> Device::createOrUpdateAudioPatch(
        AudioPatchHandle patch, const hidl_vec<AudioPortConfig>& sources,
        const hidl_vec<AudioPortConfig>& sinks) {
    Result retval(Result::NOT_SUPPORTED);
    if (version() >= AUDIO_DEVICE_API_VERSION_3_0) {
        audio_patch_handle_t halPatch = static_cast<audio_patch_handle_t>(patch);
        std::unique_ptr<audio_port_config[]> halSources;
        if (status_t status = HidlUtils::audioPortConfigsToHal(sources, &halSources);
            status != NO_ERROR) {
            return {analyzeStatus("audioPortConfigsToHal;sources", status), patch};
        }
        std::unique_ptr<audio_port_config[]> halSinks;
        if (status_t status = HidlUtils::audioPortConfigsToHal(sinks, &halSinks);
            status != NO_ERROR) {
            return {analyzeStatus("audioPortConfigsToHal;sinks", status), patch};
        }
        retval = analyzeStatus("create_audio_patch",
                               mDevice->create_audio_patch(mDevice, sources.size(), &halSources[0],
                                                           sinks.size(), &halSinks[0], &halPatch));
        if (retval == Result::OK) {
            patch = static_cast<AudioPatchHandle>(halPatch);
        }
    }
    return {retval, patch};
}

Return<Result> Device::releaseAudioPatch(int32_t patch) {
    if (version() >= AUDIO_DEVICE_API_VERSION_3_0) {
        return analyzeStatus(
            "release_audio_patch",
            mDevice->release_audio_patch(mDevice, static_cast<audio_patch_handle_t>(patch)));
    }
    return Result::NOT_SUPPORTED;
}

template <typename HalPort>
Return<void> Device::getAudioPortImpl(const AudioPort& port, getAudioPort_cb _hidl_cb,
                                      int (*halGetter)(audio_hw_device_t*, HalPort*),
                                      const char* halGetterName) {
    HalPort halPort;
    if (status_t status = HidlUtils::audioPortToHal(port, &halPort); status != NO_ERROR) {
        _hidl_cb(analyzeStatus("audioPortToHal", status), port);
        return Void();
    }
    Result retval = analyzeStatus(halGetterName, halGetter(mDevice, &halPort));
    AudioPort resultPort = port;
    if (retval == Result::OK) {
        if (status_t status = HidlUtils::audioPortFromHal(halPort, &resultPort);
            status != NO_ERROR) {
            _hidl_cb(analyzeStatus("audioPortFromHal", status), port);
            return Void();
        }
    }
    _hidl_cb(retval, resultPort);
    return Void();
}

#if MAJOR_VERSION <= 6
Return<void> Device::getAudioPort(const AudioPort& port, getAudioPort_cb _hidl_cb) {
    return getAudioPortImpl(port, _hidl_cb, mDevice->get_audio_port, "get_audio_port");
}
#else
Return<void> Device::getAudioPort(const AudioPort& port, getAudioPort_cb _hidl_cb) {
    if (version() >= AUDIO_DEVICE_API_VERSION_3_2) {
        // get_audio_port_v7 is mandatory if legacy HAL support this API version.
        return getAudioPortImpl(port, _hidl_cb, mDevice->get_audio_port_v7, "get_audio_port_v7");
    } else {
        return getAudioPortImpl(port, _hidl_cb, mDevice->get_audio_port, "get_audio_port");
    }
}
#endif

Return<Result> Device::setAudioPortConfig(const AudioPortConfig& config) {
    if (version() >= AUDIO_DEVICE_API_VERSION_3_0) {
        struct audio_port_config halPortConfig;
        if (status_t status = HidlUtils::audioPortConfigToHal(config, &halPortConfig);
            status != NO_ERROR) {
            return analyzeStatus("audioPortConfigToHal", status);
        }
        return analyzeStatus("set_audio_port_config",
                             mDevice->set_audio_port_config(mDevice, &halPortConfig));
    }
    return Result::NOT_SUPPORTED;
}

#if MAJOR_VERSION == 2
Return<AudioHwSync> Device::getHwAvSync() {
    int halHwAvSync;
    Result retval = getParam(AudioParameter::keyHwAvSync, &halHwAvSync);
    return retval == Result::OK ? halHwAvSync : AUDIO_HW_SYNC_INVALID;
}
#elif MAJOR_VERSION >= 4
Return<void> Device::getHwAvSync(getHwAvSync_cb _hidl_cb) {
    int halHwAvSync;
    Result retval = getParam(AudioParameter::keyHwAvSync, &halHwAvSync);
    _hidl_cb(retval, halHwAvSync);
    return Void();
}
#endif

Return<Result> Device::setScreenState(bool turnedOn) {
    return setParam(AudioParameter::keyScreenState, turnedOn);
}

#if MAJOR_VERSION == 2
Return<void> Device::getParameters(const hidl_vec<hidl_string>& keys, getParameters_cb _hidl_cb) {
    getParametersImpl({}, keys, _hidl_cb);
    return Void();
}

Return<Result> Device::setParameters(const hidl_vec<ParameterValue>& parameters) {
    return setParametersImpl({} /* context */, parameters);
}
#elif MAJOR_VERSION >= 4
Return<void> Device::getParameters(const hidl_vec<ParameterValue>& context,
                                   const hidl_vec<hidl_string>& keys, getParameters_cb _hidl_cb) {
    getParametersImpl(context, keys, _hidl_cb);
    return Void();
}
Return<Result> Device::setParameters(const hidl_vec<ParameterValue>& context,
                                     const hidl_vec<ParameterValue>& parameters) {
    return setParametersImpl(context, parameters);
}
#endif

#if MAJOR_VERSION == 2
Return<void> Device::debugDump(const hidl_handle& fd) {
    return debug(fd, {});
}
#endif

Return<void> Device::debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) {
    if (fd.getNativeHandle() != nullptr && fd->numFds == 1) {
        const int fd0 = fd->data[0];
        bool dumpMem = false;
        bool unreachableMemory = false;
        for (const auto& option : options) {
            if (option == "-m") {
                dumpMem = true;
            } else if (option == "--unreachable") {
                unreachableMemory = true;
            }
        }

        if (dumpMem) {
            dprintf(fd0, "\nDumping memory:\n");
            std::string s = dumpMemoryAddresses(100 /* limit */);
            write(fd0, s.c_str(), s.size());
        }
        if (unreachableMemory) {
            dprintf(fd0, "\nDumping unreachable memory:\n");
            // TODO - should limit be an argument parameter?
            std::string s = GetUnreachableMemoryString(true /* contents */, 100 /* limit */);
            write(fd0, s.c_str(), s.size());
        }

        analyzeStatus("dump", mDevice->dump(mDevice, fd0));
    }
    return Void();
}

#if MAJOR_VERSION >= 4
Return<void> Device::getMicrophones(getMicrophones_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    size_t actual_mics = AUDIO_MICROPHONE_MAX_COUNT;
    audio_microphone_characteristic_t mic_array[AUDIO_MICROPHONE_MAX_COUNT];

    hidl_vec<MicrophoneInfo> microphones;
    if (mDevice->get_microphones != NULL &&
        mDevice->get_microphones(mDevice, &mic_array[0], &actual_mics) == 0) {
        microphones.resize(actual_mics);
        for (size_t i = 0; i < actual_mics; ++i) {
            (void)CoreUtils::microphoneInfoFromHal(mic_array[i], &microphones[i]);
        }
        retval = Result::OK;
    }
    _hidl_cb(retval, microphones);
    return Void();
}

Return<Result> Device::setConnectedState(const DeviceAddress& address, bool connected) {
    auto key = connected ? AudioParameter::keyDeviceConnect : AudioParameter::keyDeviceDisconnect;
    return setParam(key, address);
}
#endif

Result Device::doClose() {
    if (mIsClosed || mOpenedStreamsCount != 0) return Result::INVALID_STATE;
    mIsClosed = true;
    return analyzeStatus("close", audio_hw_device_close(mDevice));
}

#if MAJOR_VERSION >= 6
Return<Result> Device::close() {
    return doClose();
}

Return<Result> Device::addDeviceEffect(AudioPortHandle device, uint64_t effectId) {
    if (version() < AUDIO_DEVICE_API_VERSION_3_1 || mDevice->add_device_effect == nullptr) {
        return Result::NOT_SUPPORTED;
    }

    effect_handle_t halEffect = EffectMap::getInstance().get(effectId);
    if (halEffect != NULL) {
        return analyzeStatus("add_device_effect",
                             mDevice->add_device_effect(
                                     mDevice, static_cast<audio_port_handle_t>(device), halEffect));
    } else {
        ALOGW("%s Invalid effect ID passed from client: %" PRIu64 "", __func__, effectId);
        return Result::INVALID_ARGUMENTS;
    }
}

Return<Result> Device::removeDeviceEffect(AudioPortHandle device, uint64_t effectId) {
    if (version() < AUDIO_DEVICE_API_VERSION_3_1 || mDevice->remove_device_effect == nullptr) {
        return Result::NOT_SUPPORTED;
    }

    effect_handle_t halEffect = EffectMap::getInstance().get(effectId);
    if (halEffect != NULL) {
        return analyzeStatus("remove_device_effect",
                             mDevice->remove_device_effect(
                                     mDevice, static_cast<audio_port_handle_t>(device), halEffect));
    } else {
        ALOGW("%s Invalid effect ID passed from client: %" PRIu64 "", __func__, effectId);
        return Result::INVALID_ARGUMENTS;
    }
}

Return<void> Device::updateAudioPatch(int32_t previousPatch,
                                      const hidl_vec<AudioPortConfig>& sources,
                                      const hidl_vec<AudioPortConfig>& sinks,
                                      createAudioPatch_cb _hidl_cb) {
    if (previousPatch != static_cast<int32_t>(AudioPatchHandle{})) {
        auto [retval, patch] = createOrUpdateAudioPatch(previousPatch, sources, sinks);
        _hidl_cb(retval, patch);
    } else {
        _hidl_cb(Result::INVALID_ARGUMENTS, previousPatch);
    }
    return Void();
}

#endif

#if MAJOR_VERSION == 7 && MINOR_VERSION == 1
Return<Result> Device::setConnectedState_7_1(const AudioPort& devicePort, bool connected) {
    if (version() >= AUDIO_DEVICE_API_VERSION_3_2 &&
        mDevice->set_device_connected_state_v7 != nullptr) {
        audio_port_v7 halPort;
        if (status_t status = HidlUtils::audioPortToHal(devicePort, &halPort); status != NO_ERROR) {
            return analyzeStatus("audioPortToHal", status);
        }
        return analyzeStatus("set_device_connected_state_v7",
                             mDevice->set_device_connected_state_v7(mDevice, &halPort, connected));
    }
    return Result::NOT_SUPPORTED;
}
#endif

}  // namespace implementation
}  // namespace CPP_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android
