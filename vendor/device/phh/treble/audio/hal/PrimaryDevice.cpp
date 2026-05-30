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

#define LOG_TAG "PrimaryDeviceHAL"

#include "PrimaryDevice.h"
#include "Util.h"

#if MAJOR_VERSION >= 4
#include <cmath>
#endif

namespace android {
namespace hardware {
namespace audio {
namespace CPP_VERSION {
namespace implementation {

namespace util {
using namespace ::android::hardware::audio::CORE_TYPES_CPP_VERSION::implementation::util;
}

PrimaryDevice::PrimaryDevice(audio_hw_device_t* device) : mDevice(new Device(device)) {}

PrimaryDevice::~PrimaryDevice() {
    // Do not call mDevice->close here. If there are any unclosed streams,
    // they only hold IDevice instance, not IPrimaryDevice, thus IPrimaryDevice
    // "part" of a device can be destroyed before the streams.
}

// Methods from ::android::hardware::audio::CPP_VERSION::IDevice follow.
Return<Result> PrimaryDevice::initCheck() {
    return mDevice->initCheck();
}

Return<Result> PrimaryDevice::setMasterVolume(float volume) {
    return mDevice->setMasterVolume(volume);
}

Return<void> PrimaryDevice::getMasterVolume(getMasterVolume_cb _hidl_cb) {
    return mDevice->getMasterVolume(_hidl_cb);
}

Return<Result> PrimaryDevice::setMicMute(bool mute) {
    return mDevice->setMicMute(mute);
}

Return<void> PrimaryDevice::getMicMute(getMicMute_cb _hidl_cb) {
    return mDevice->getMicMute(_hidl_cb);
}

Return<Result> PrimaryDevice::setMasterMute(bool mute) {
    return mDevice->setMasterMute(mute);
}

Return<void> PrimaryDevice::getMasterMute(getMasterMute_cb _hidl_cb) {
    return mDevice->getMasterMute(_hidl_cb);
}

Return<void> PrimaryDevice::getInputBufferSize(const AudioConfig& config,
                                               getInputBufferSize_cb _hidl_cb) {
    return mDevice->getInputBufferSize(config, _hidl_cb);
}

#if MAJOR_VERSION == 2
Return<void> PrimaryDevice::openOutputStream(int32_t ioHandle, const DeviceAddress& device,
                                             const AudioConfig& config, AudioOutputFlags flags,
                                             openOutputStream_cb _hidl_cb) {
    return mDevice->openOutputStream(ioHandle, device, config, flags, _hidl_cb);
}

Return<void> PrimaryDevice::openInputStream(int32_t ioHandle, const DeviceAddress& device,
                                            const AudioConfig& config, AudioInputFlags flags,
                                            AudioSource source, openInputStream_cb _hidl_cb) {
    return mDevice->openInputStream(ioHandle, device, config, flags, source, _hidl_cb);
}
#elif MAJOR_VERSION >= 4
Return<void> PrimaryDevice::openOutputStream(int32_t ioHandle, const DeviceAddress& device,
                                             const AudioConfig& config,
#if MAJOR_VERSION <= 6
                                             AudioOutputFlags flags,
#else
                                             const AudioOutputFlags& flags,
#endif
                                             const SourceMetadata& sourceMetadata,
                                             openOutputStream_cb _hidl_cb) {
    return mDevice->openOutputStream(ioHandle, device, config, flags, sourceMetadata, _hidl_cb);
}

Return<void> PrimaryDevice::openInputStream(int32_t ioHandle, const DeviceAddress& device,
                                            const AudioConfig& config,
#if MAJOR_VERSION <= 6
                                            AudioInputFlags flags,
#else
                                            const AudioInputFlags& flags,
#endif
                                            const SinkMetadata& sinkMetadata,
                                            openInputStream_cb _hidl_cb) {
    return mDevice->openInputStream(ioHandle, device, config, flags, sinkMetadata, _hidl_cb);
}
#endif

Return<bool> PrimaryDevice::supportsAudioPatches() {
    return mDevice->supportsAudioPatches();
}

Return<void> PrimaryDevice::createAudioPatch(const hidl_vec<AudioPortConfig>& sources,
                                             const hidl_vec<AudioPortConfig>& sinks,
                                             createAudioPatch_cb _hidl_cb) {
    return mDevice->createAudioPatch(sources, sinks, _hidl_cb);
}

Return<Result> PrimaryDevice::releaseAudioPatch(int32_t patch) {
    return mDevice->releaseAudioPatch(patch);
}

Return<void> PrimaryDevice::getAudioPort(const AudioPort& port, getAudioPort_cb _hidl_cb) {
    return mDevice->getAudioPort(port, _hidl_cb);
}

Return<Result> PrimaryDevice::setAudioPortConfig(const AudioPortConfig& config) {
    return mDevice->setAudioPortConfig(config);
}

Return<Result> PrimaryDevice::setScreenState(bool turnedOn) {
    return mDevice->setScreenState(turnedOn);
}

#if MAJOR_VERSION == 2
Return<AudioHwSync> PrimaryDevice::getHwAvSync() {
    return mDevice->getHwAvSync();
}

Return<void> PrimaryDevice::getParameters(const hidl_vec<hidl_string>& keys,
                                          getParameters_cb _hidl_cb) {
    return mDevice->getParameters(keys, _hidl_cb);
}

Return<Result> PrimaryDevice::setParameters(const hidl_vec<ParameterValue>& parameters) {
    return mDevice->setParameters(parameters);
}

Return<void> PrimaryDevice::debugDump(const hidl_handle& fd) {
    return mDevice->debugDump(fd);
}
#elif MAJOR_VERSION >= 4
Return<void> PrimaryDevice::getHwAvSync(getHwAvSync_cb _hidl_cb) {
    return mDevice->getHwAvSync(_hidl_cb);
}
Return<void> PrimaryDevice::getParameters(const hidl_vec<ParameterValue>& context,
                                          const hidl_vec<hidl_string>& keys,
                                          getParameters_cb _hidl_cb) {
    return mDevice->getParameters(context, keys, _hidl_cb);
}
Return<Result> PrimaryDevice::setParameters(const hidl_vec<ParameterValue>& context,
                                            const hidl_vec<ParameterValue>& parameters) {
    return mDevice->setParameters(context, parameters);
}
Return<void> PrimaryDevice::getMicrophones(getMicrophones_cb _hidl_cb) {
    return mDevice->getMicrophones(_hidl_cb);
}
Return<Result> PrimaryDevice::setConnectedState(const DeviceAddress& address, bool connected) {
    return mDevice->setConnectedState(address, connected);
}
#endif
#if MAJOR_VERSION >= 6
Return<Result> PrimaryDevice::close() {
    return mDevice->close();
}

Return<Result> PrimaryDevice::addDeviceEffect(AudioPortHandle device, uint64_t effectId) {
    return mDevice->addDeviceEffect(device, effectId);
}

Return<Result> PrimaryDevice::removeDeviceEffect(AudioPortHandle device, uint64_t effectId) {
    return mDevice->removeDeviceEffect(device, effectId);
}

Return<void> PrimaryDevice::updateAudioPatch(int32_t previousPatch,
                                             const hidl_vec<AudioPortConfig>& sources,
                                             const hidl_vec<AudioPortConfig>& sinks,
                                             updateAudioPatch_cb _hidl_cb) {
    return mDevice->updateAudioPatch(previousPatch, sources, sinks, _hidl_cb);
}
#endif

// Methods from ::android::hardware::audio::CPP_VERSION::IPrimaryDevice follow.
Return<Result> PrimaryDevice::setVoiceVolume(float volume) {
    if (!util::isGainNormalized(volume)) {
        ALOGW("Can not set a voice volume (%f) outside [0,1]", volume);
        return Result::INVALID_ARGUMENTS;
    }
    return mDevice->analyzeStatus("set_voice_volume",
                                  mDevice->device()->set_voice_volume(mDevice->device(), volume));
}

Return<Result> PrimaryDevice::setMode(AudioMode mode) {
    // INVALID, CURRENT, CNT, MAX are reserved for internal use.
    // TODO: remove the values from the HIDL interface
    switch (mode) {
        case AudioMode::NORMAL:
        case AudioMode::RINGTONE:
        case AudioMode::IN_CALL:
        case AudioMode::IN_COMMUNICATION:
#if MAJOR_VERSION >= 6
        case AudioMode::CALL_SCREEN:
#endif
            break;  // Valid values
        default:
            return Result::INVALID_ARGUMENTS;
    };

    return mDevice->analyzeStatus(
        "set_mode",
        mDevice->device()->set_mode(mDevice->device(), static_cast<audio_mode_t>(mode)));
}

Return<void> PrimaryDevice::getBtScoNrecEnabled(getBtScoNrecEnabled_cb _hidl_cb) {
    bool enabled;
    Result retval = mDevice->getParam(AudioParameter::keyBtNrec, &enabled);
    _hidl_cb(retval, enabled);
    return Void();
}

Return<Result> PrimaryDevice::setBtScoNrecEnabled(bool enabled) {
    return mDevice->setParam(AudioParameter::keyBtNrec, enabled);
}

Return<void> PrimaryDevice::getBtScoWidebandEnabled(getBtScoWidebandEnabled_cb _hidl_cb) {
    bool enabled;
    Result retval = mDevice->getParam(AUDIO_PARAMETER_KEY_BT_SCO_WB, &enabled);
    _hidl_cb(retval, enabled);
    return Void();
}

Return<Result> PrimaryDevice::setBtScoWidebandEnabled(bool enabled) {
    return mDevice->setParam(AUDIO_PARAMETER_KEY_BT_SCO_WB, enabled);
}

static const char* convertTtyModeFromHIDL(IPrimaryDevice::TtyMode mode) {
    switch (mode) {
        case IPrimaryDevice::TtyMode::OFF:
            return AUDIO_PARAMETER_VALUE_TTY_OFF;
        case IPrimaryDevice::TtyMode::VCO:
            return AUDIO_PARAMETER_VALUE_TTY_VCO;
        case IPrimaryDevice::TtyMode::HCO:
            return AUDIO_PARAMETER_VALUE_TTY_HCO;
        case IPrimaryDevice::TtyMode::FULL:
            return AUDIO_PARAMETER_VALUE_TTY_FULL;
        default:
            return nullptr;
    }
}
static IPrimaryDevice::TtyMode convertTtyModeToHIDL(const char* halMode) {
    if (strcmp(halMode, AUDIO_PARAMETER_VALUE_TTY_OFF) == 0)
        return IPrimaryDevice::TtyMode::OFF;
    else if (strcmp(halMode, AUDIO_PARAMETER_VALUE_TTY_VCO) == 0)
        return IPrimaryDevice::TtyMode::VCO;
    else if (strcmp(halMode, AUDIO_PARAMETER_VALUE_TTY_HCO) == 0)
        return IPrimaryDevice::TtyMode::HCO;
    else if (strcmp(halMode, AUDIO_PARAMETER_VALUE_TTY_FULL) == 0)
        return IPrimaryDevice::TtyMode::FULL;
    return IPrimaryDevice::TtyMode(-1);
}

Return<void> PrimaryDevice::getTtyMode(getTtyMode_cb _hidl_cb) {
    String8 halMode;
    Result retval = mDevice->getParam(AUDIO_PARAMETER_KEY_TTY_MODE, &halMode);
    if (retval != Result::OK) {
        _hidl_cb(retval, TtyMode::OFF);
        return Void();
    }
    TtyMode mode = convertTtyModeToHIDL(halMode);
    if (mode == TtyMode(-1)) {
        ALOGE("HAL returned invalid TTY value: %s", halMode.c_str());
        _hidl_cb(Result::INVALID_STATE, TtyMode::OFF);
        return Void();
    }
    _hidl_cb(Result::OK, mode);
    return Void();
}

Return<Result> PrimaryDevice::setTtyMode(IPrimaryDevice::TtyMode mode) {
    const char* modeStr = convertTtyModeFromHIDL(mode);
    if (modeStr == nullptr) {
        ALOGW("Can not set an invalid TTY value: %d", mode);
        return Result::INVALID_ARGUMENTS;
    }
    return mDevice->setParam(AUDIO_PARAMETER_KEY_TTY_MODE, modeStr);
}

Return<void> PrimaryDevice::getHacEnabled(getHacEnabled_cb _hidl_cb) {
    bool enabled;
    Result retval = mDevice->getParam(AUDIO_PARAMETER_KEY_HAC, &enabled);
    _hidl_cb(retval, enabled);
    return Void();
}

Return<Result> PrimaryDevice::setHacEnabled(bool enabled) {
    return mDevice->setParam(AUDIO_PARAMETER_KEY_HAC, enabled);
}

#if MAJOR_VERSION >= 4
Return<Result> PrimaryDevice::setBtScoHeadsetDebugName(const hidl_string& name) {
    return mDevice->setParam(AUDIO_PARAMETER_KEY_BT_SCO_HEADSET_NAME, name.c_str());
}
Return<void> PrimaryDevice::getBtHfpEnabled(getBtHfpEnabled_cb _hidl_cb) {
    bool enabled;
    Result retval = mDevice->getParam(AUDIO_PARAMETER_KEY_HFP_ENABLE, &enabled);
    _hidl_cb(retval, enabled);
    return Void();
}
Return<Result> PrimaryDevice::setBtHfpEnabled(bool enabled) {
    return mDevice->setParam(AUDIO_PARAMETER_KEY_HFP_ENABLE, enabled);
}
Return<Result> PrimaryDevice::setBtHfpSampleRate(uint32_t sampleRateHz) {
    return mDevice->setParam(AUDIO_PARAMETER_KEY_HFP_SET_SAMPLING_RATE, int(sampleRateHz));
}
Return<Result> PrimaryDevice::setBtHfpVolume(float volume) {
    if (!util::isGainNormalized(volume)) {
        ALOGW("Can not set BT HFP volume (%f) outside [0,1]", volume);
        return Result::INVALID_ARGUMENTS;
    }
    // Map the normalized volume onto the range of [0, 15]
    return mDevice->setParam(AUDIO_PARAMETER_KEY_HFP_VOLUME,
                             static_cast<int>(std::round(volume * 15)));
}
Return<Result> PrimaryDevice::updateRotation(IPrimaryDevice::Rotation rotation) {
    // legacy API expects the rotation in degree
    return mDevice->setParam(AUDIO_PARAMETER_KEY_ROTATION, int(rotation) * 90);
}
#endif

Return<void> PrimaryDevice::debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) {
    return mDevice->debug(fd, options);
}

}  // namespace implementation
}  // namespace CPP_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android
