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

#ifndef ANDROID_HARDWARE_AUDIO_STREAMIN_H
#define ANDROID_HARDWARE_AUDIO_STREAMIN_H

// clang-format off
#include PATH(android/hardware/audio/CORE_TYPES_FILE_VERSION/IStreamIn.h)
// clang-format on

#include "Device.h"
#include "Stream.h"

#include <atomic>
#include <memory>

#include <fmq/EventFlag.h>
#include <fmq/MessageQueue.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <utils/Thread.h>

namespace android {
namespace hardware {
namespace audio {
namespace CPP_VERSION {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using namespace ::android::hardware::audio::common::COMMON_TYPES_CPP_VERSION;
using namespace ::android::hardware::audio::CORE_TYPES_CPP_VERSION;
using namespace ::android::hardware::audio::CPP_VERSION;

struct StreamIn : public IStreamIn {
    typedef MessageQueue<ReadParameters, kSynchronizedReadWrite> CommandMQ;
    typedef MessageQueue<uint8_t, kSynchronizedReadWrite> DataMQ;
    typedef MessageQueue<ReadStatus, kSynchronizedReadWrite> StatusMQ;

    StreamIn(const sp<Device>& device, audio_stream_in_t* stream);

    // Methods from ::android::hardware::audio::CPP_VERSION::IStream follow.
    Return<uint64_t> getFrameSize() override;
    Return<uint64_t> getFrameCount() override;
    Return<uint64_t> getBufferSize() override;
#if MAJOR_VERSION <= 6
    Return<uint32_t> getSampleRate() override;
#if MAJOR_VERSION == 2
    Return<void> getSupportedSampleRates(getSupportedSampleRates_cb _hidl_cb) override;
    Return<void> getSupportedChannelMasks(getSupportedChannelMasks_cb _hidl_cb) override;
#endif
    Return<void> getSupportedSampleRates(AudioFormat format, getSupportedSampleRates_cb _hidl_cb);
    Return<void> getSupportedChannelMasks(AudioFormat format, getSupportedChannelMasks_cb _hidl_cb);
    Return<Result> setSampleRate(uint32_t sampleRateHz) override;
    Return<AudioChannelBitfield> getChannelMask() override;
    Return<Result> setChannelMask(AudioChannelBitfield mask) override;
    Return<AudioFormat> getFormat() override;
    Return<void> getSupportedFormats(getSupportedFormats_cb _hidl_cb) override;
    Return<Result> setFormat(AudioFormat format) override;
#else
    Return<void> getSupportedProfiles(getSupportedProfiles_cb _hidl_cb) override;
    Return<Result> setAudioProperties(const AudioConfigBaseOptional& config) override;
#endif  // MAJOR_VERSION <= 6
    Return<void> getAudioProperties(getAudioProperties_cb _hidl_cb) override;
    Return<Result> addEffect(uint64_t effectId) override;
    Return<Result> removeEffect(uint64_t effectId) override;
    Return<Result> standby() override;
#if MAJOR_VERSION == 2
    Return<AudioDevice> getDevice() override;
    Return<Result> setDevice(const DeviceAddress& address) override;
    Return<void> getParameters(const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& parameters) override;
    Return<Result> setConnectedState(const DeviceAddress& address, bool connected) override;
#elif MAJOR_VERSION >= 4
    Return<void> getDevices(getDevices_cb _hidl_cb) override;
    Return<Result> setDevices(const hidl_vec<DeviceAddress>& devices) override;
    Return<void> getParameters(const hidl_vec<ParameterValue>& context,
                               const hidl_vec<hidl_string>& keys,
                               getParameters_cb _hidl_cb) override;
    Return<Result> setParameters(const hidl_vec<ParameterValue>& context,
                                 const hidl_vec<ParameterValue>& parameters) override;
#endif
    Return<Result> setHwAvSync(uint32_t hwAvSync) override;
    Return<Result> close() override;

    Return<void> debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) override;
#if MAJOR_VERSION == 2
    Return<void> debugDump(const hidl_handle& fd) override;
#endif

    // Methods from ::android::hardware::audio::CPP_VERSION::IStreamIn follow.
    Return<void> getAudioSource(getAudioSource_cb _hidl_cb) override;
    Return<Result> setGain(float gain) override;
    Return<void> prepareForReading(uint32_t frameSize, uint32_t framesCount,
                                   prepareForReading_cb _hidl_cb) override;
    Return<uint32_t> getInputFramesLost() override;
    Return<void> getCapturePosition(getCapturePosition_cb _hidl_cb) override;
    Return<Result> start() override;
    Return<Result> stop() override;
    Return<void> createMmapBuffer(int32_t minSizeFrames, createMmapBuffer_cb _hidl_cb) override;
    Return<void> getMmapPosition(getMmapPosition_cb _hidl_cb) override;
#if MAJOR_VERSION >= 4
#if MAJOR_VERSION <= 6
    Return<void> updateSinkMetadata(const SinkMetadata& sinkMetadata) override;
#else
    Return<Result> updateSinkMetadata(const SinkMetadata& sinkMetadata) override;
#endif
    Return<void> getActiveMicrophones(getActiveMicrophones_cb _hidl_cb) override;
#endif  // MAJOR_VERSION >= 4
#if MAJOR_VERSION >= 5
    Return<Result> setMicrophoneDirection(MicrophoneDirection direction) override;
    Return<Result> setMicrophoneFieldDimension(float zoom) override;
#endif
    static Result getCapturePositionImpl(audio_stream_in_t* stream, uint64_t* frames,
                                         uint64_t* time);

  private:
#if MAJOR_VERSION >= 4
    Result doUpdateSinkMetadata(const SinkMetadata& sinkMetadata);
#if MAJOR_VERSION >= 7
    Result doUpdateSinkMetadataV7(const SinkMetadata& sinkMetadata);
#endif
#endif  // MAJOR_VERSION >= 4

    const sp<Device> mDevice;
    audio_stream_in_t* mStream;
    const sp<Stream> mStreamCommon;
    const sp<StreamMmap<audio_stream_in_t>> mStreamMmap;
    std::unique_ptr<CommandMQ> mCommandMQ;
    std::unique_ptr<DataMQ> mDataMQ;
    std::unique_ptr<StatusMQ> mStatusMQ;
    EventFlag* mEfGroup;
    std::atomic<bool> mStopReadThread;
    sp<Thread> mReadThread;

    virtual ~StreamIn();
};

}  // namespace implementation
}  // namespace CPP_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_STREAMIN_H
