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

#define LOG_TAG "StreamInHAL"

#include "StreamIn.h"
#include "Util.h"
#include "common/all-versions/HidlSupport.h"

//#define LOG_NDEBUG 0
#define ATRACE_TAG ATRACE_TAG_AUDIO

#include <HidlUtils.h>
#include <android/log.h>
#include <hardware/audio.h>
#include <util/CoreUtils.h>
#include <utils/Trace.h>
#include <cmath>
#include <memory>

namespace android {
namespace hardware {
namespace audio {
namespace CPP_VERSION {
namespace implementation {

using ::android::hardware::audio::common::COMMON_TYPES_CPP_VERSION::implementation::HidlUtils;
using ::android::hardware::audio::CORE_TYPES_CPP_VERSION::implementation::CoreUtils;
namespace util {
using namespace ::android::hardware::audio::CORE_TYPES_CPP_VERSION::implementation::util;
}

namespace {

class ReadThread : public Thread {
   public:
    // ReadThread's lifespan never exceeds StreamIn's lifespan.
    ReadThread(std::atomic<bool>* stop, audio_stream_in_t* stream, StreamIn::CommandMQ* commandMQ,
               StreamIn::DataMQ* dataMQ, StreamIn::StatusMQ* statusMQ, EventFlag* efGroup)
        : Thread(false /*canCallJava*/),
          mStop(stop),
          mStream(stream),
          mCommandMQ(commandMQ),
          mDataMQ(dataMQ),
          mStatusMQ(statusMQ),
          mEfGroup(efGroup),
          mBuffer(nullptr) {}
    bool init() {
        mBuffer.reset(new (std::nothrow) uint8_t[mDataMQ->getQuantumCount()]);
        return mBuffer != nullptr;
    }
    virtual ~ReadThread() {}

   private:
    std::atomic<bool>* mStop;
    audio_stream_in_t* mStream;
    StreamIn::CommandMQ* mCommandMQ;
    StreamIn::DataMQ* mDataMQ;
    StreamIn::StatusMQ* mStatusMQ;
    EventFlag* mEfGroup;
    std::unique_ptr<uint8_t[]> mBuffer;
    IStreamIn::ReadParameters mParameters;
    IStreamIn::ReadStatus mStatus;

    bool threadLoop() override;

    void doGetCapturePosition();
    void doRead();
};

void ReadThread::doRead() {
    size_t availableToWrite = mDataMQ->availableToWrite();
    size_t requestedToRead = mParameters.params.read;
    if (requestedToRead > availableToWrite) {
        ALOGW(
            "truncating read data from %d to %d due to insufficient data queue "
            "space",
            (int32_t)requestedToRead, (int32_t)availableToWrite);
        requestedToRead = availableToWrite;
    }
    ssize_t readResult = mStream->read(mStream, &mBuffer[0], requestedToRead);
    mStatus.retval = Result::OK;
    if (readResult >= 0) {
        mStatus.reply.read = readResult;
        if (!mDataMQ->write(&mBuffer[0], readResult)) {
            ALOGW("data message queue write failed");
        }
    } else {
        mStatus.retval = Stream::analyzeStatus("read", readResult);
    }
}

void ReadThread::doGetCapturePosition() {
    mStatus.retval = StreamIn::getCapturePositionImpl(
        mStream, &mStatus.reply.capturePosition.frames, &mStatus.reply.capturePosition.time);
}

bool ReadThread::threadLoop() {
    // This implementation doesn't return control back to the Thread until it
    // decides to stop,
    // as the Thread uses mutexes, and this can lead to priority inversion.
    while (!std::atomic_load_explicit(mStop, std::memory_order_acquire)) {
        uint32_t efState = 0;
        mEfGroup->wait(static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL), &efState);
        if (!(efState & static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL))) {
            continue;  // Nothing to do.
        }
        if (!mCommandMQ->read(&mParameters)) {
            continue;  // Nothing to do.
        }
        mStatus.replyTo = mParameters.command;
        switch (mParameters.command) {
            case IStreamIn::ReadCommand::READ:
                doRead();
                break;
            case IStreamIn::ReadCommand::GET_CAPTURE_POSITION:
                doGetCapturePosition();
                break;
            default:
                ALOGE("Unknown read thread command code %d", mParameters.command);
                mStatus.retval = Result::NOT_SUPPORTED;
                break;
        }
        if (!mStatusMQ->write(&mStatus)) {
            ALOGW("status message queue write failed");
        }
        mEfGroup->wake(static_cast<uint32_t>(MessageQueueFlagBits::NOT_EMPTY));
    }

    return false;
}

}  // namespace

StreamIn::StreamIn(const sp<Device>& device, audio_stream_in_t* stream)
    : mDevice(device),
      mStream(stream),
      mStreamCommon(new Stream(true /*isInput*/, &stream->common)),
      mStreamMmap(new StreamMmap<audio_stream_in_t>(stream)),
      mEfGroup(nullptr),
      mStopReadThread(false) {}

StreamIn::~StreamIn() {
    ATRACE_CALL();
    close();
    if (mReadThread.get()) {
        ATRACE_NAME("mReadThread->join");
        status_t status = mReadThread->join();
        ALOGE_IF(status, "read thread exit error: %s", strerror(-status));
    }
    if (mEfGroup) {
        status_t status = EventFlag::deleteEventFlag(&mEfGroup);
        ALOGE_IF(status, "read MQ event flag deletion error: %s", strerror(-status));
    }
#if MAJOR_VERSION <= 5
    mDevice->closeInputStream(mStream);
#endif
    mStream = nullptr;
}

// Methods from ::android::hardware::audio::CPP_VERSION::IStream follow.
Return<uint64_t> StreamIn::getFrameSize() {
    return audio_stream_in_frame_size(mStream);
}

Return<uint64_t> StreamIn::getFrameCount() {
    return mStreamCommon->getFrameCount();
}

Return<uint64_t> StreamIn::getBufferSize() {
    return mStreamCommon->getBufferSize();
}

#if MAJOR_VERSION <= 6
Return<uint32_t> StreamIn::getSampleRate() {
    return mStreamCommon->getSampleRate();
}

#if MAJOR_VERSION == 2
Return<void> StreamIn::getSupportedChannelMasks(getSupportedChannelMasks_cb _hidl_cb) {
    return mStreamCommon->getSupportedChannelMasks(_hidl_cb);
}
Return<void> StreamIn::getSupportedSampleRates(getSupportedSampleRates_cb _hidl_cb) {
    return mStreamCommon->getSupportedSampleRates(_hidl_cb);
}
#endif

Return<void> StreamIn::getSupportedChannelMasks(AudioFormat format,
                                                getSupportedChannelMasks_cb _hidl_cb) {
    return mStreamCommon->getSupportedChannelMasks(format, _hidl_cb);
}
Return<void> StreamIn::getSupportedSampleRates(AudioFormat format,
                                               getSupportedSampleRates_cb _hidl_cb) {
    return mStreamCommon->getSupportedSampleRates(format, _hidl_cb);
}

Return<Result> StreamIn::setSampleRate(uint32_t sampleRateHz) {
    return mStreamCommon->setSampleRate(sampleRateHz);
}

Return<AudioChannelBitfield> StreamIn::getChannelMask() {
    return mStreamCommon->getChannelMask();
}

Return<Result> StreamIn::setChannelMask(AudioChannelBitfield mask) {
    return mStreamCommon->setChannelMask(mask);
}

Return<AudioFormat> StreamIn::getFormat() {
    return mStreamCommon->getFormat();
}

Return<void> StreamIn::getSupportedFormats(getSupportedFormats_cb _hidl_cb) {
    return mStreamCommon->getSupportedFormats(_hidl_cb);
}

Return<Result> StreamIn::setFormat(AudioFormat format) {
    return mStreamCommon->setFormat(format);
}

#else

Return<void> StreamIn::getSupportedProfiles(getSupportedProfiles_cb _hidl_cb) {
    return mStreamCommon->getSupportedProfiles(_hidl_cb);
}

Return<Result> StreamIn::setAudioProperties(const AudioConfigBaseOptional& config) {
    return mStreamCommon->setAudioProperties(config);
}

#endif  // MAJOR_VERSION <= 6

Return<void> StreamIn::getAudioProperties(getAudioProperties_cb _hidl_cb) {
    return mStreamCommon->getAudioProperties(_hidl_cb);
}

Return<Result> StreamIn::addEffect(uint64_t effectId) {
    return mStreamCommon->addEffect(effectId);
}

Return<Result> StreamIn::removeEffect(uint64_t effectId) {
    return mStreamCommon->removeEffect(effectId);
}

Return<Result> StreamIn::standby() {
    return mStreamCommon->standby();
}

Return<Result> StreamIn::setHwAvSync(uint32_t hwAvSync) {
    return mStreamCommon->setHwAvSync(hwAvSync);
}

#if MAJOR_VERSION == 2
Return<Result> StreamIn::setConnectedState(const DeviceAddress& address, bool connected) {
    return mStreamCommon->setConnectedState(address, connected);
}

Return<AudioDevice> StreamIn::getDevice() {
    return mStreamCommon->getDevice();
}

Return<Result> StreamIn::setDevice(const DeviceAddress& address) {
    return mStreamCommon->setDevice(address);
}

Return<void> StreamIn::getParameters(const hidl_vec<hidl_string>& keys, getParameters_cb _hidl_cb) {
    return mStreamCommon->getParameters(keys, _hidl_cb);
}

Return<Result> StreamIn::setParameters(const hidl_vec<ParameterValue>& parameters) {
    return mStreamCommon->setParameters(parameters);
}

Return<void> StreamIn::debugDump(const hidl_handle& fd) {
    return mStreamCommon->debugDump(fd);
}
#elif MAJOR_VERSION >= 4
Return<void> StreamIn::getDevices(getDevices_cb _hidl_cb) {
    return mStreamCommon->getDevices(_hidl_cb);
}

Return<Result> StreamIn::setDevices(const hidl_vec<DeviceAddress>& devices) {
    return mStreamCommon->setDevices(devices);
}
Return<void> StreamIn::getParameters(const hidl_vec<ParameterValue>& context,
                                     const hidl_vec<hidl_string>& keys, getParameters_cb _hidl_cb) {
    return mStreamCommon->getParameters(context, keys, _hidl_cb);
}

Return<Result> StreamIn::setParameters(const hidl_vec<ParameterValue>& context,
                                       const hidl_vec<ParameterValue>& parameters) {
    return mStreamCommon->setParameters(context, parameters);
}
#endif

Return<Result> StreamIn::start() {
    return mStreamMmap->start();
}

Return<Result> StreamIn::stop() {
    return mStreamMmap->stop();
}

Return<void> StreamIn::createMmapBuffer(int32_t minSizeFrames, createMmapBuffer_cb _hidl_cb) {
    return mStreamMmap->createMmapBuffer(minSizeFrames, audio_stream_in_frame_size(mStream),
                                         _hidl_cb);
}

Return<void> StreamIn::getMmapPosition(getMmapPosition_cb _hidl_cb) {
    return mStreamMmap->getMmapPosition(_hidl_cb);
}

Return<Result> StreamIn::close() {
    if (mStopReadThread.load(std::memory_order_relaxed)) {  // only this thread writes
        return Result::INVALID_STATE;
    }
    mStopReadThread.store(true, std::memory_order_release);
    if (mEfGroup) {
        mEfGroup->wake(static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL));
    }
#if MAJOR_VERSION >= 6
    mDevice->closeInputStream(mStream);
#endif
    return Result::OK;
}

// Methods from ::android::hardware::audio::CPP_VERSION::IStreamIn follow.
Return<void> StreamIn::getAudioSource(getAudioSource_cb _hidl_cb) {
    int halSource;
    Result retval = mStreamCommon->getParam(AudioParameter::keyInputSource, &halSource);
    AudioSource source = {};
    if (retval == Result::OK) {
        retval = Stream::analyzeStatus(
                "get_audio_source",
                HidlUtils::audioSourceFromHal(static_cast<audio_source_t>(halSource), &source));
    }
    _hidl_cb(retval, source);
    return Void();
}

Return<Result> StreamIn::setGain(float gain) {
    if (!util::isGainNormalized(gain)) {
        ALOGW("Can not set a stream input gain (%f) outside [0,1]", gain);
        return Result::INVALID_ARGUMENTS;
    }
    return Stream::analyzeStatus("set_gain", mStream->set_gain(mStream, gain));
}

Return<void> StreamIn::prepareForReading(uint32_t frameSize, uint32_t framesCount,
                                         prepareForReading_cb _hidl_cb) {
    status_t status;
#if MAJOR_VERSION <= 6
    ThreadInfo threadInfo = {0, 0};
#else
    int32_t threadInfo = 0;
#endif

    // Wrap the _hidl_cb to return an error
    auto sendError = [&threadInfo, &_hidl_cb](Result result) {
        _hidl_cb(result, CommandMQ::Descriptor(), DataMQ::Descriptor(), StatusMQ::Descriptor(),
                 threadInfo);
    };

    // Create message queues.
    if (mDataMQ) {
        ALOGE("the client attempts to call prepareForReading twice");
        sendError(Result::INVALID_STATE);
        return Void();
    }
    std::unique_ptr<CommandMQ> tempCommandMQ(new CommandMQ(1));

    // Check frameSize and framesCount
    if (frameSize == 0 || framesCount == 0) {
        ALOGE("Null frameSize (%u) or framesCount (%u)", frameSize, framesCount);
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }

    if (frameSize > Stream::MAX_BUFFER_SIZE / framesCount) {
        ALOGE("Buffer too big: %u*%u bytes > MAX_BUFFER_SIZE (%u)", frameSize, framesCount,
              Stream::MAX_BUFFER_SIZE);
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }
    std::unique_ptr<DataMQ> tempDataMQ(new DataMQ(frameSize * framesCount, true /* EventFlag */));

    std::unique_ptr<StatusMQ> tempStatusMQ(new StatusMQ(1));
    if (!tempCommandMQ->isValid() || !tempDataMQ->isValid() || !tempStatusMQ->isValid()) {
        ALOGE_IF(!tempCommandMQ->isValid(), "command MQ is invalid");
        ALOGE_IF(!tempDataMQ->isValid(), "data MQ is invalid");
        ALOGE_IF(!tempStatusMQ->isValid(), "status MQ is invalid");
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }
    EventFlag* tempRawEfGroup{};
    status = EventFlag::createEventFlag(tempDataMQ->getEventFlagWord(), &tempRawEfGroup);
    std::unique_ptr<EventFlag, void (*)(EventFlag*)> tempElfGroup(
        tempRawEfGroup, [](auto* ef) { EventFlag::deleteEventFlag(&ef); });
    if (status != OK || !tempElfGroup) {
        ALOGE("failed creating event flag for data MQ: %s", strerror(-status));
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }

    // Create and launch the thread.
    auto tempReadThread =
            sp<ReadThread>::make(&mStopReadThread, mStream, tempCommandMQ.get(), tempDataMQ.get(),
                                 tempStatusMQ.get(), tempElfGroup.get());
    if (!tempReadThread->init()) {
        ALOGW("failed to start reader thread: %s", strerror(-status));
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }
    status = tempReadThread->run("reader", PRIORITY_URGENT_AUDIO);
    if (status != OK) {
        ALOGW("failed to start reader thread: %s", strerror(-status));
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }

    mCommandMQ = std::move(tempCommandMQ);
    mDataMQ = std::move(tempDataMQ);
    mStatusMQ = std::move(tempStatusMQ);
    mReadThread = tempReadThread;
    mEfGroup = tempElfGroup.release();
#if MAJOR_VERSION <= 6
    threadInfo.pid = getpid();
    threadInfo.tid = mReadThread->getTid();
#else
    threadInfo = mReadThread->getTid();
#endif
    _hidl_cb(Result::OK, *mCommandMQ->getDesc(), *mDataMQ->getDesc(), *mStatusMQ->getDesc(),
             threadInfo);
    return Void();
}

Return<uint32_t> StreamIn::getInputFramesLost() {
    return mStream->get_input_frames_lost(mStream);
}

// static
Result StreamIn::getCapturePositionImpl(audio_stream_in_t* stream, uint64_t* frames,
                                        uint64_t* time) {
    // HAL may have a stub function, always returning ENOSYS, don't
    // spam the log in this case.
    static const std::vector<int> ignoredErrors{ENOSYS};
    Result retval(Result::NOT_SUPPORTED);
    if (stream->get_capture_position == NULL) return retval;
    int64_t halFrames, halTime;
    retval = Stream::analyzeStatus("get_capture_position",
                                   stream->get_capture_position(stream, &halFrames, &halTime),
                                   ignoredErrors);
    if (retval == Result::OK) {
        *frames = halFrames;
        *time = halTime;
    }
    return retval;
};

Return<void> StreamIn::getCapturePosition(getCapturePosition_cb _hidl_cb) {
    uint64_t frames = 0, time = 0;
    Result retval = getCapturePositionImpl(mStream, &frames, &time);
    _hidl_cb(retval, frames, time);
    return Void();
}

Return<void> StreamIn::debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) {
    return mStreamCommon->debug(fd, options);
}

#if MAJOR_VERSION >= 4
Result StreamIn::doUpdateSinkMetadata(const SinkMetadata& sinkMetadata) {
    std::vector<record_track_metadata> halTracks;
#if MAJOR_VERSION <= 6
    (void)CoreUtils::sinkMetadataToHal(sinkMetadata, &halTracks);
#else
    // Validate whether a conversion to V7 is possible. This is needed
    // to have a consistent behavior of the HAL regardless of the API
    // version of the legacy HAL (and also to be consistent with openInputStream).
    std::vector<record_track_metadata_v7> halTracksV7;
    if (status_t status = CoreUtils::sinkMetadataToHalV7(
                sinkMetadata, false /*ignoreNonVendorTags*/, &halTracksV7);
        status == NO_ERROR) {
        halTracks.reserve(halTracksV7.size());
        for (auto metadata_v7 : halTracksV7) {
            halTracks.push_back(std::move(metadata_v7.base));
        }
    } else {
        return Stream::analyzeStatus("sinkMetadataToHal", status);
    }
#endif  // MAJOR_VERSION <= 6
    const sink_metadata_t halMetadata = {
        .track_count = halTracks.size(),
        .tracks = halTracks.data(),
    };
    mStream->update_sink_metadata(mStream, &halMetadata);
    return Result::OK;
}

#if MAJOR_VERSION >= 7
Result StreamIn::doUpdateSinkMetadataV7(const SinkMetadata& sinkMetadata) {
    std::vector<record_track_metadata_v7> halTracks;
    if (status_t status = CoreUtils::sinkMetadataToHalV7(sinkMetadata,
                                                         false /*ignoreNonVendorTags*/, &halTracks);
        status != NO_ERROR) {
        return Stream::analyzeStatus("sinkMetadataToHal", status);
    }
    const sink_metadata_v7_t halMetadata = {
            .track_count = halTracks.size(),
            .tracks = halTracks.data(),
    };
    mStream->update_sink_metadata_v7(mStream, &halMetadata);
    return Result::OK;
}
#endif  //  MAJOR_VERSION >= 7

#if MAJOR_VERSION <= 6
Return<void> StreamIn::updateSinkMetadata(const SinkMetadata& sinkMetadata) {
    if (mStream->update_sink_metadata == nullptr) {
        return Void();  // not supported by the HAL
    }
    (void)doUpdateSinkMetadata(sinkMetadata);
    return Void();
}
#elif MAJOR_VERSION >= 7
Return<Result> StreamIn::updateSinkMetadata(const SinkMetadata& sinkMetadata) {
    if (mDevice->version() < AUDIO_DEVICE_API_VERSION_3_2) {
        if (mStream->update_sink_metadata == nullptr) {
            return Result::NOT_SUPPORTED;
        }
        return doUpdateSinkMetadata(sinkMetadata);
    } else {
        if (mStream->update_sink_metadata_v7 == nullptr) {
            return Result::NOT_SUPPORTED;
        }
        return doUpdateSinkMetadataV7(sinkMetadata);
    }
}
#endif

Return<void> StreamIn::getActiveMicrophones(getActiveMicrophones_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    size_t actual_mics = AUDIO_MICROPHONE_MAX_COUNT;
    audio_microphone_characteristic_t mic_array[AUDIO_MICROPHONE_MAX_COUNT];

    hidl_vec<MicrophoneInfo> microphones;
    if (mStream->get_active_microphones != NULL &&
        mStream->get_active_microphones(mStream, &mic_array[0], &actual_mics) == 0) {
        microphones.resize(actual_mics);
        for (size_t i = 0; i < actual_mics; ++i) {
            (void)CoreUtils::microphoneInfoFromHal(mic_array[i], &microphones[i]);
        }
        retval = Result::OK;
    }

    _hidl_cb(retval, microphones);
    return Void();
}
#endif

#if MAJOR_VERSION >= 5
Return<Result> StreamIn::setMicrophoneDirection(MicrophoneDirection direction) {
    if (mStream->set_microphone_direction == nullptr) {
        return Result::NOT_SUPPORTED;
    }
    if (!common::utils::isValidHidlEnum(direction)) {
        ALOGE("%s: Invalid direction %d", __func__, direction);
        return Result::INVALID_ARGUMENTS;
    }
    return Stream::analyzeStatus(
            "set_microphone_direction",
            mStream->set_microphone_direction(
                    mStream, static_cast<audio_microphone_direction_t>(direction)));
}

Return<Result> StreamIn::setMicrophoneFieldDimension(float zoom) {
    if (mStream->set_microphone_field_dimension == nullptr) {
        return Result::NOT_SUPPORTED;
    }
    if (std::isnan(zoom) || zoom < -1 || zoom > 1) {
        ALOGE("%s: Invalid zoom %f", __func__, zoom);
        return Result::INVALID_ARGUMENTS;
    }
    return Stream::analyzeStatus("set_microphone_field_dimension",
                                 mStream->set_microphone_field_dimension(mStream, zoom));
}

#endif

}  // namespace implementation
}  // namespace CPP_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android
