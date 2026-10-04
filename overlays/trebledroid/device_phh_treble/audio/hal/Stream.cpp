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

#define LOG_TAG "StreamHAL"

#include "Stream.h"
#include "common/all-versions/HidlSupport.h"
#include "common/all-versions/default/EffectMap.h"
#include "Util.h"

#include <inttypes.h>

#include <HidlUtils.h>
#include <android/log.h>
#include <hardware/audio.h>
#include <hardware/audio_effect.h>
#include <media/AudioContainers.h>
#include <media/TypeConverter.h>
#include <util/CoreUtils.h>

namespace android {
namespace hardware {
namespace audio {
namespace CPP_VERSION {
namespace implementation {

using ::android::hardware::audio::common::COMMON_TYPES_CPP_VERSION::implementation::HidlUtils;
using ::android::hardware::audio::common::utils::splitString;
using ::android::hardware::audio::CORE_TYPES_CPP_VERSION::implementation::CoreUtils;
namespace util {
using namespace ::android::hardware::audio::CORE_TYPES_CPP_VERSION::implementation::util;
}

Stream::Stream(bool isInput, audio_stream_t* stream) : mIsInput(isInput), mStream(stream) {
    (void)mIsInput;  // prevent 'unused field' warnings in pre-V7 versions.
}

Stream::~Stream() {
    mStream = nullptr;
}

// static
Result Stream::analyzeStatus(const char* funcName, int status) {
    return util::analyzeStatus("stream", funcName, status);
}

// static
Result Stream::analyzeStatus(const char* funcName, int status,
                             const std::vector<int>& ignoreErrors) {
    return util::analyzeStatus("stream", funcName, status, ignoreErrors);
}

char* Stream::halGetParameters(const char* keys) {
    return mStream->get_parameters(mStream, keys);
}

int Stream::halSetParameters(const char* keysAndValues) {
    return mStream->set_parameters(mStream, keysAndValues);
}

// Methods from ::android::hardware::audio::CPP_VERSION::IStream follow.
Return<uint64_t> Stream::getFrameSize() {
    // Needs to be implemented by interface subclasses. But can't be declared as pure virtual,
    // since interface subclasses implementation do not inherit from this class.
    LOG_ALWAYS_FATAL("Stream::getFrameSize is pure abstract");
    return uint64_t{};
}

Return<uint64_t> Stream::getFrameCount() {
    int halFrameCount;
    Result retval = getParam(AudioParameter::keyFrameCount, &halFrameCount);
    return retval == Result::OK ? halFrameCount : 0;
}

Return<uint64_t> Stream::getBufferSize() {
    return mStream->get_buffer_size(mStream);
}

#if MAJOR_VERSION <= 6
Return<uint32_t> Stream::getSampleRate() {
    return mStream->get_sample_rate(mStream);
}

#if MAJOR_VERSION == 2
Return<void> Stream::getSupportedSampleRates(getSupportedSampleRates_cb _hidl_cb) {
    return getSupportedSampleRates(getFormat(), _hidl_cb);
}
Return<void> Stream::getSupportedChannelMasks(getSupportedChannelMasks_cb _hidl_cb) {
    return getSupportedChannelMasks(getFormat(), _hidl_cb);
}
#endif

Return<void> Stream::getSupportedSampleRates(AudioFormat format,
                                             getSupportedSampleRates_cb _hidl_cb) {
    AudioParameter context;
    context.addInt(String8(AUDIO_PARAMETER_STREAM_FORMAT), int(format));
    String8 halListValue;
    Result result =
        getParam(AudioParameter::keyStreamSupportedSamplingRates, &halListValue, context);
    hidl_vec<uint32_t> sampleRates;
    SampleRateSet halSampleRates;
    if (result == Result::OK) {
        halSampleRates =
            samplingRatesFromString(halListValue.c_str(), AudioParameter::valueListSeparator);
        sampleRates = hidl_vec<uint32_t>(halSampleRates.begin(), halSampleRates.end());
        // Legacy get_parameter does not return a status_t, thus can not advertise of failure.
        // Note that this method must succeed (non empty list) if the format is supported.
        if (sampleRates.size() == 0) {
            result = Result::NOT_SUPPORTED;
        }
    }
#if MAJOR_VERSION == 2
    _hidl_cb(sampleRates);
#elif MAJOR_VERSION >= 4
    _hidl_cb(result, sampleRates);
#endif
    return Void();
}

Return<void> Stream::getSupportedChannelMasks(AudioFormat format,
                                              getSupportedChannelMasks_cb _hidl_cb) {
    AudioParameter context;
    context.addInt(String8(AUDIO_PARAMETER_STREAM_FORMAT), int(format));
    String8 halListValue;
    Result result = getParam(AudioParameter::keyStreamSupportedChannels, &halListValue, context);
    hidl_vec<AudioChannelBitfield> channelMasks;
    ChannelMaskSet halChannelMasks;
    if (result == Result::OK) {
        halChannelMasks =
            channelMasksFromString(halListValue.c_str(), AudioParameter::valueListSeparator);
        channelMasks.resize(halChannelMasks.size());
        size_t i = 0;
        for (auto channelMask : halChannelMasks) {
            channelMasks[i++] = AudioChannelBitfield(channelMask);
        }
        // Legacy get_parameter does not return a status_t, thus can not advertise of failure.
        // Note that this method must succeed (non empty list) if the format is supported.
        if (channelMasks.size() == 0) {
            result = Result::NOT_SUPPORTED;
        }
    }
#if MAJOR_VERSION == 2
    _hidl_cb(channelMasks);
#elif MAJOR_VERSION >= 4
    _hidl_cb(result, channelMasks);
#endif
    return Void();
}

Return<Result> Stream::setSampleRate(uint32_t sampleRateHz) {
    return setParam(AudioParameter::keySamplingRate, static_cast<int>(sampleRateHz));
}

Return<AudioChannelBitfield> Stream::getChannelMask() {
    return AudioChannelBitfield(mStream->get_channels(mStream));
}

Return<Result> Stream::setChannelMask(AudioChannelBitfield mask) {
    return setParam(AudioParameter::keyChannels, static_cast<int>(mask));
}

Return<AudioFormat> Stream::getFormat() {
    return AudioFormat(mStream->get_format(mStream));
}

Return<void> Stream::getSupportedFormats(getSupportedFormats_cb _hidl_cb) {
    String8 halListValue;
    Result result = getParam(AudioParameter::keyStreamSupportedFormats, &halListValue);
    hidl_vec<AudioFormat> formats;
    FormatVector halFormats;
    if (result == Result::OK) {
        halFormats = formatsFromString(halListValue.c_str(), AudioParameter::valueListSeparator);
        formats.resize(halFormats.size());
        for (size_t i = 0; i < halFormats.size(); ++i) {
            formats[i] = AudioFormat(halFormats[i]);
        }
        // Legacy get_parameter does not return a status_t, thus can not advertise of failure.
        // Note that the method must not return an empty list if this capability is supported.
        if (formats.size() == 0) {
            result = Result::NOT_SUPPORTED;
        }
    }
#if MAJOR_VERSION <= 5
    _hidl_cb(formats);
#elif MAJOR_VERSION >= 6
    _hidl_cb(result, formats);
#endif
    return Void();
}

Return<Result> Stream::setFormat(AudioFormat format) {
    return setParam(AudioParameter::keyFormat, static_cast<int>(format));
}

Return<void> Stream::getAudioProperties(getAudioProperties_cb _hidl_cb) {
    uint32_t halSampleRate = mStream->get_sample_rate(mStream);
    audio_channel_mask_t halMask = mStream->get_channels(mStream);
    audio_format_t halFormat = mStream->get_format(mStream);
    _hidl_cb(halSampleRate, AudioChannelBitfield(halMask), AudioFormat(halFormat));
    return Void();
}

#else  // MAJOR_VERSION <= 6

Return<void> Stream::getSupportedProfiles(getSupportedProfiles_cb _hidl_cb) {
    String8 halListValue;
    Result result = getParam(AudioParameter::keyStreamSupportedFormats, &halListValue);
    hidl_vec<AudioProfile> profiles;
    if (result != Result::OK) {
        _hidl_cb(result, profiles);
        return Void();
    }
    // Ensure that the separator is one character, despite that it's defined as a C string.
    static_assert(sizeof(AUDIO_PARAMETER_VALUE_LIST_SEPARATOR) == 2);
    std::vector<std::string> halFormats =
            splitString(halListValue.c_str(), AUDIO_PARAMETER_VALUE_LIST_SEPARATOR[0]);
    hidl_vec<AudioFormat> formats;
    (void)HidlUtils::audioFormatsFromHal(halFormats, &formats);
    std::vector<AudioProfile> tempProfiles;
    for (const auto& format : formats) {
        audio_format_t halFormat;
        if (status_t status = HidlUtils::audioFormatToHal(format, &halFormat); status != NO_ERROR) {
            continue;
        }
        AudioParameter context;
        context.addInt(String8(AUDIO_PARAMETER_STREAM_FORMAT), int(halFormat));
        // Query supported sample rates for the format.
        result = getParam(AudioParameter::keyStreamSupportedSamplingRates, &halListValue, context);
        if (result != Result::OK) break;
        std::vector<std::string> halSampleRates =
                splitString(halListValue.c_str(), AUDIO_PARAMETER_VALUE_LIST_SEPARATOR[0]);
        hidl_vec<uint32_t> sampleRates;
        sampleRates.resize(halSampleRates.size());
        for (size_t i = 0; i < sampleRates.size(); ++i) {
            sampleRates[i] = std::stoi(halSampleRates[i]);
        }
        // Query supported channel masks for the format.
        result = getParam(AudioParameter::keyStreamSupportedChannels, &halListValue, context);
        if (result != Result::OK) break;
        std::vector<std::string> halChannelMasks =
                splitString(halListValue.c_str(), AUDIO_PARAMETER_VALUE_LIST_SEPARATOR[0]);
        hidl_vec<AudioChannelMask> channelMasks;
        (void)HidlUtils::audioChannelMasksFromHal(halChannelMasks, &channelMasks);
        // Create a profile.
        if (channelMasks.size() != 0 && sampleRates.size() != 0) {
            tempProfiles.push_back({.format = format,
                                    .sampleRates = std::move(sampleRates),
                                    .channelMasks = std::move(channelMasks)});
        }
    }
    // Legacy get_parameter does not return a status_t, thus can not advertise of failure.
    // Note that the method must not return an empty list if this capability is supported.
    if (!tempProfiles.empty()) {
        profiles = tempProfiles;
    } else {
        result = Result::NOT_SUPPORTED;
    }
    _hidl_cb(result, profiles);
    return Void();
}

Return<void> Stream::getAudioProperties(getAudioProperties_cb _hidl_cb) {
    audio_config_base_t halConfigBase = {mStream->get_sample_rate(mStream),
                                         mStream->get_channels(mStream),
                                         mStream->get_format(mStream)};
    AudioConfigBase configBase = {};
    status_t status = HidlUtils::audioConfigBaseFromHal(halConfigBase, mIsInput, &configBase);
    _hidl_cb(Stream::analyzeStatus("get_audio_properties", status), configBase);
    return Void();
}

Return<Result> Stream::setAudioProperties(const AudioConfigBaseOptional& config) {
    audio_config_base_t halConfigBase = AUDIO_CONFIG_BASE_INITIALIZER;
    bool formatSpecified, sRateSpecified, channelMaskSpecified;
    status_t status = HidlUtils::audioConfigBaseOptionalToHal(
            config, &halConfigBase, &formatSpecified, &sRateSpecified, &channelMaskSpecified);
    if (status != NO_ERROR) {
        return Stream::analyzeStatus("set_audio_properties", status);
    }
    if (sRateSpecified) {
        if (Result result = setParam(AudioParameter::keySamplingRate,
                                     static_cast<int>(halConfigBase.sample_rate));
            result != Result::OK) {
            return result;
        }
    }
    if (channelMaskSpecified) {
        if (Result result = setParam(AudioParameter::keyChannels,
                                     static_cast<int>(halConfigBase.channel_mask));
            result != Result::OK) {
            return result;
        }
    }
    if (formatSpecified) {
        if (Result result =
                    setParam(AudioParameter::keyFormat, static_cast<int>(halConfigBase.format));
            result != Result::OK) {
            return result;
        }
    }
    return Result::OK;
}

#endif  // MAJOR_VERSION <= 6

Return<Result> Stream::addEffect(uint64_t effectId) {
    effect_handle_t halEffect = EffectMap::getInstance().get(effectId);
    if (halEffect != NULL) {
        return analyzeStatus("add_audio_effect", mStream->add_audio_effect(mStream, halEffect));
    } else {
        ALOGW("Invalid effect ID passed from client: %" PRIu64, effectId);
        return Result::INVALID_ARGUMENTS;
    }
}

Return<Result> Stream::removeEffect(uint64_t effectId) {
    effect_handle_t halEffect = EffectMap::getInstance().get(effectId);
    if (halEffect != NULL) {
        return analyzeStatus("remove_audio_effect",
                             mStream->remove_audio_effect(mStream, halEffect));
    } else {
        ALOGW("Invalid effect ID passed from client: %" PRIu64, effectId);
        return Result::INVALID_ARGUMENTS;
    }
}

Return<Result> Stream::standby() {
    return analyzeStatus("standby", mStream->standby(mStream));
}

Return<Result> Stream::setHwAvSync(uint32_t hwAvSync) {
    return setParam(AudioParameter::keyStreamHwAvSync, static_cast<int>(hwAvSync));
}

#if MAJOR_VERSION == 2
Return<AudioDevice> Stream::getDevice() {
    int device = 0;
    Result retval = getParam(AudioParameter::keyRouting, &device);
    return retval == Result::OK ? static_cast<AudioDevice>(device) : AudioDevice::NONE;
}

Return<Result> Stream::setDevice(const DeviceAddress& address) {
    return setParam(AudioParameter::keyRouting, address);
}

Return<void> Stream::getParameters(const hidl_vec<hidl_string>& keys, getParameters_cb _hidl_cb) {
    getParametersImpl({} /* context */, keys, _hidl_cb);
    return Void();
}

Return<Result> Stream::setParameters(const hidl_vec<ParameterValue>& parameters) {
    return setParametersImpl({} /* context */, parameters);
}

Return<Result> Stream::setConnectedState(const DeviceAddress& address, bool connected) {
    return setParam(
            connected ? AudioParameter::keyDeviceConnect : AudioParameter::keyDeviceDisconnect,
            address);
}
#elif MAJOR_VERSION >= 4
Return<void> Stream::getDevices(getDevices_cb _hidl_cb) {
    int halDevice = 0;
    Result retval = getParam(AudioParameter::keyRouting, &halDevice);
    hidl_vec<DeviceAddress> devices;
    if (retval == Result::OK) {
        devices.resize(1);
        retval = Stream::analyzeStatus(
                "get_devices",
                CoreUtils::deviceAddressFromHal(static_cast<audio_devices_t>(halDevice), nullptr,
                                                &devices[0]));
    }
    _hidl_cb(retval, devices);
    return Void();
}

Return<Result> Stream::setDevices(const hidl_vec<DeviceAddress>& devices) {
    // FIXME: can the legacy API set multiple device with address ?
    if (devices.size() > 1) {
        return Result::NOT_SUPPORTED;
    }
    DeviceAddress address{};
    if (devices.size() == 1) {
        address = devices[0];
    }
    return setParam(AudioParameter::keyRouting, address);
}

Return<void> Stream::getParameters(const hidl_vec<ParameterValue>& context,
                                   const hidl_vec<hidl_string>& keys, getParameters_cb _hidl_cb) {
    getParametersImpl(context, keys, _hidl_cb);
    return Void();
}

Return<Result> Stream::setParameters(const hidl_vec<ParameterValue>& context,
                                     const hidl_vec<ParameterValue>& parameters) {
    return setParametersImpl(context, parameters);
}
#endif

Return<Result> Stream::start() {
    return Result::NOT_SUPPORTED;
}

Return<Result> Stream::stop() {
    return Result::NOT_SUPPORTED;
}

Return<void> Stream::createMmapBuffer(int32_t minSizeFrames __unused,
                                      createMmapBuffer_cb _hidl_cb) {
    Result retval(Result::NOT_SUPPORTED);
    MmapBufferInfo info;
    _hidl_cb(retval, info);
    return Void();
}

Return<void> Stream::getMmapPosition(getMmapPosition_cb _hidl_cb) {
    Result retval(Result::NOT_SUPPORTED);
    MmapPosition position;
    _hidl_cb(retval, position);
    return Void();
}

Return<Result> Stream::close() {
    return Result::NOT_SUPPORTED;
}

Return<void> Stream::debug(const hidl_handle& fd, const hidl_vec<hidl_string>& /* options */) {
    if (fd.getNativeHandle() != nullptr && fd->numFds == 1) {
        analyzeStatus("dump", mStream->dump(mStream, fd->data[0]));
    }
    return Void();
}

#if MAJOR_VERSION == 2
Return<void> Stream::debugDump(const hidl_handle& fd) {
    return debug(fd, {} /* options */);
}
#endif

}  // namespace implementation
}  // namespace CPP_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android
