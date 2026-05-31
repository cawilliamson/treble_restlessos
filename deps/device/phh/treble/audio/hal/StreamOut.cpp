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

#define LOG_TAG "StreamOutHAL"

#include "StreamOut.h"
#include "Util.h"

//#define LOG_NDEBUG 0
#define ATRACE_TAG ATRACE_TAG_AUDIO

#include <string.h>

#include <memory>

#include <HidlUtils.h>
#include <android/log.h>
#include <audio_utils/Metadata.h>
#include <hardware/audio.h>
#include <util/CoreUtils.h>
#include <utils/Trace.h>

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

class WriteThread : public Thread {
   public:
    // WriteThread's lifespan never exceeds StreamOut's lifespan.
    WriteThread(std::atomic<bool>* stop, audio_stream_out_t* stream,
                StreamOut::CommandMQ* commandMQ, StreamOut::DataMQ* dataMQ,
                StreamOut::StatusMQ* statusMQ, EventFlag* efGroup)
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
    virtual ~WriteThread() {}

   private:
    std::atomic<bool>* mStop;
    audio_stream_out_t* mStream;
    StreamOut::CommandMQ* mCommandMQ;
    StreamOut::DataMQ* mDataMQ;
    StreamOut::StatusMQ* mStatusMQ;
    EventFlag* mEfGroup;
    std::unique_ptr<uint8_t[]> mBuffer;
    IStreamOut::WriteStatus mStatus;

    bool threadLoop() override;

    void doGetLatency();
    void doGetPresentationPosition();
    void doWrite();
};

void WriteThread::doWrite() {
    const size_t availToRead = mDataMQ->availableToRead();
    mStatus.retval = Result::OK;
    mStatus.reply.written = 0;
    if (mDataMQ->read(&mBuffer[0], availToRead)) {
        ssize_t writeResult = mStream->write(mStream, &mBuffer[0], availToRead);
        if (writeResult >= 0) {
            mStatus.reply.written = writeResult;
        } else {
            mStatus.retval = Stream::analyzeStatus("write", writeResult);
        }
    }
}

void WriteThread::doGetPresentationPosition() {
    mStatus.retval =
        StreamOut::getPresentationPositionImpl(mStream, &mStatus.reply.presentationPosition.frames,
                                               &mStatus.reply.presentationPosition.timeStamp);
}

void WriteThread::doGetLatency() {
    mStatus.retval = Result::OK;
    mStatus.reply.latencyMs = mStream->get_latency(mStream);
}

bool WriteThread::threadLoop() {
    // This implementation doesn't return control back to the Thread until it
    // decides to stop,
    // as the Thread uses mutexes, and this can lead to priority inversion.
    while (!std::atomic_load_explicit(mStop, std::memory_order_acquire)) {
        uint32_t efState = 0;
        mEfGroup->wait(static_cast<uint32_t>(MessageQueueFlagBits::NOT_EMPTY), &efState);
        if (!(efState & static_cast<uint32_t>(MessageQueueFlagBits::NOT_EMPTY))) {
            continue;  // Nothing to do.
        }
        if (!mCommandMQ->read(&mStatus.replyTo)) {
            continue;  // Nothing to do.
        }
        switch (mStatus.replyTo) {
            case IStreamOut::WriteCommand::WRITE:
                doWrite();
                break;
            case IStreamOut::WriteCommand::GET_PRESENTATION_POSITION:
                doGetPresentationPosition();
                break;
            case IStreamOut::WriteCommand::GET_LATENCY:
                doGetLatency();
                break;
            default:
                ALOGE("Unknown write thread command code %d", mStatus.replyTo);
                mStatus.retval = Result::NOT_SUPPORTED;
                break;
        }
        if (!mStatusMQ->write(&mStatus)) {
            ALOGE("status message queue write failed");
        }
        mEfGroup->wake(static_cast<uint32_t>(MessageQueueFlagBits::NOT_FULL));
    }

    return false;
}

}  // namespace

StreamOut::StreamOut(const sp<Device>& device, audio_stream_out_t* stream)
    : mDevice(device),
      mStream(stream),
      mStreamCommon(new Stream(false /*isInput*/, &stream->common)),
      mStreamMmap(new StreamMmap<audio_stream_out_t>(stream)),
      mEfGroup(nullptr),
      mStopWriteThread(false) {}

StreamOut::~StreamOut() {
    ATRACE_CALL();
    (void)close();
    if (mWriteThread.get()) {
        ATRACE_NAME("mWriteThread->join");
        status_t status = mWriteThread->join();
        ALOGE_IF(status, "write thread exit error: %s", strerror(-status));
    }
    if (mEfGroup) {
        status_t status = EventFlag::deleteEventFlag(&mEfGroup);
        ALOGE_IF(status, "write MQ event flag deletion error: %s", strerror(-status));
    }
    mCallback = nullptr;
#if MAJOR_VERSION <= 5
    mDevice->closeOutputStream(mStream);
    // Closing the output stream in the HAL waits for the callback to finish,
    // and joins the callback thread. Thus is it guaranteed that the callback
    // thread will not be accessing our object anymore.
#endif
    mStream = nullptr;
}

// Methods from ::android::hardware::audio::CPP_VERSION::IStream follow.
Return<uint64_t> StreamOut::getFrameSize() {
    return audio_stream_out_frame_size(mStream);
}

Return<uint64_t> StreamOut::getFrameCount() {
    return mStreamCommon->getFrameCount();
}

Return<uint64_t> StreamOut::getBufferSize() {
    return mStreamCommon->getBufferSize();
}

#if MAJOR_VERSION <= 6
Return<uint32_t> StreamOut::getSampleRate() {
    return mStreamCommon->getSampleRate();
}

#if MAJOR_VERSION == 2
Return<void> StreamOut::getSupportedChannelMasks(getSupportedChannelMasks_cb _hidl_cb) {
    return mStreamCommon->getSupportedChannelMasks(_hidl_cb);
}
Return<void> StreamOut::getSupportedSampleRates(getSupportedSampleRates_cb _hidl_cb) {
    return mStreamCommon->getSupportedSampleRates(_hidl_cb);
}
#endif

Return<void> StreamOut::getSupportedChannelMasks(AudioFormat format,
                                                 getSupportedChannelMasks_cb _hidl_cb) {
    return mStreamCommon->getSupportedChannelMasks(format, _hidl_cb);
}
Return<void> StreamOut::getSupportedSampleRates(AudioFormat format,
                                                getSupportedSampleRates_cb _hidl_cb) {
    return mStreamCommon->getSupportedSampleRates(format, _hidl_cb);
}

Return<Result> StreamOut::setSampleRate(uint32_t sampleRateHz) {
    return mStreamCommon->setSampleRate(sampleRateHz);
}

Return<AudioChannelBitfield> StreamOut::getChannelMask() {
    return mStreamCommon->getChannelMask();
}

Return<Result> StreamOut::setChannelMask(AudioChannelBitfield mask) {
    return mStreamCommon->setChannelMask(mask);
}

Return<AudioFormat> StreamOut::getFormat() {
    return mStreamCommon->getFormat();
}

Return<void> StreamOut::getSupportedFormats(getSupportedFormats_cb _hidl_cb) {
    return mStreamCommon->getSupportedFormats(_hidl_cb);
}

Return<Result> StreamOut::setFormat(AudioFormat format) {
    return mStreamCommon->setFormat(format);
}

#else

Return<void> StreamOut::getSupportedProfiles(getSupportedProfiles_cb _hidl_cb) {
    return mStreamCommon->getSupportedProfiles(_hidl_cb);
}

Return<Result> StreamOut::setAudioProperties(const AudioConfigBaseOptional& config) {
    return mStreamCommon->setAudioProperties(config);
}

#endif  // MAJOR_VERSION <= 6

Return<void> StreamOut::getAudioProperties(getAudioProperties_cb _hidl_cb) {
    return mStreamCommon->getAudioProperties(_hidl_cb);
}

Return<Result> StreamOut::addEffect(uint64_t effectId) {
    return mStreamCommon->addEffect(effectId);
}

Return<Result> StreamOut::removeEffect(uint64_t effectId) {
    return mStreamCommon->removeEffect(effectId);
}

Return<Result> StreamOut::standby() {
    return mStreamCommon->standby();
}

Return<Result> StreamOut::setHwAvSync(uint32_t hwAvSync) {
    return mStreamCommon->setHwAvSync(hwAvSync);
}

#if MAJOR_VERSION == 2
Return<Result> StreamOut::setConnectedState(const DeviceAddress& address, bool connected) {
    return mStreamCommon->setConnectedState(address, connected);
}

Return<AudioDevice> StreamOut::getDevice() {
    return mStreamCommon->getDevice();
}

Return<Result> StreamOut::setDevice(const DeviceAddress& address) {
    return mStreamCommon->setDevice(address);
}

Return<void> StreamOut::getParameters(const hidl_vec<hidl_string>& keys,
                                      getParameters_cb _hidl_cb) {
    return mStreamCommon->getParameters(keys, _hidl_cb);
}

Return<Result> StreamOut::setParameters(const hidl_vec<ParameterValue>& parameters) {
    return mStreamCommon->setParameters(parameters);
}

Return<void> StreamOut::debugDump(const hidl_handle& fd) {
    return mStreamCommon->debugDump(fd);
}
#elif MAJOR_VERSION >= 4
Return<void> StreamOut::getDevices(getDevices_cb _hidl_cb) {
    return mStreamCommon->getDevices(_hidl_cb);
}

Return<Result> StreamOut::setDevices(const hidl_vec<DeviceAddress>& devices) {
    return mStreamCommon->setDevices(devices);
}
Return<void> StreamOut::getParameters(const hidl_vec<ParameterValue>& context,
                                      const hidl_vec<hidl_string>& keys,
                                      getParameters_cb _hidl_cb) {
    return mStreamCommon->getParameters(context, keys, _hidl_cb);
}

Return<Result> StreamOut::setParameters(const hidl_vec<ParameterValue>& context,
                                        const hidl_vec<ParameterValue>& parameters) {
    return mStreamCommon->setParameters(context, parameters);
}
#endif

Return<Result> StreamOut::close() {
    if (mStopWriteThread.load(std::memory_order_relaxed)) {  // only this thread writes
        return Result::INVALID_STATE;
    }
    mStopWriteThread.store(true, std::memory_order_release);
    if (mEfGroup) {
        mEfGroup->wake(static_cast<uint32_t>(MessageQueueFlagBits::NOT_EMPTY));
    }
#if MAJOR_VERSION >= 6
    mDevice->closeOutputStream(mStream);
#endif
    return Result::OK;
}

// Methods from ::android::hardware::audio::CPP_VERSION::IStreamOut follow.
Return<uint32_t> StreamOut::getLatency() {
    return mStream->get_latency(mStream);
}

Return<Result> StreamOut::setVolume(float left, float right) {
    if (mStream->set_volume == NULL) {
        return Result::NOT_SUPPORTED;
    }
    if (!util::isGainNormalized(left)) {
        ALOGW("Can not set a stream output volume {%f, %f} outside [0,1]", left, right);
        return Result::INVALID_ARGUMENTS;
    }
    return Stream::analyzeStatus("set_volume", mStream->set_volume(mStream, left, right),
                                 {ENOSYS} /*ignore*/);
}

Return<void> StreamOut::prepareForWriting(uint32_t frameSize, uint32_t framesCount,
                                          prepareForWriting_cb _hidl_cb) {
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
        ALOGE("the client attempts to call prepareForWriting twice");
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
    auto tempWriteThread =
            sp<WriteThread>::make(&mStopWriteThread, mStream, tempCommandMQ.get(), tempDataMQ.get(),
                                  tempStatusMQ.get(), tempElfGroup.get());
    if (!tempWriteThread->init()) {
        ALOGW("failed to start writer thread: %s", strerror(-status));
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }
    status = tempWriteThread->run("writer", PRIORITY_URGENT_AUDIO);
    if (status != OK) {
        ALOGW("failed to start writer thread: %s", strerror(-status));
        sendError(Result::INVALID_ARGUMENTS);
        return Void();
    }

    mCommandMQ = std::move(tempCommandMQ);
    mDataMQ = std::move(tempDataMQ);
    mStatusMQ = std::move(tempStatusMQ);
    mWriteThread = tempWriteThread;
    mEfGroup = tempElfGroup.release();
#if MAJOR_VERSION <= 6
    threadInfo.pid = getpid();
    threadInfo.tid = mWriteThread->getTid();
#else
    threadInfo = mWriteThread->getTid();
#endif
    _hidl_cb(Result::OK, *mCommandMQ->getDesc(), *mDataMQ->getDesc(), *mStatusMQ->getDesc(),
             threadInfo);
    return Void();
}

Return<void> StreamOut::getRenderPosition(getRenderPosition_cb _hidl_cb) {
    uint32_t halDspFrames;
    Result retval = Stream::analyzeStatus("get_render_position",
                                          mStream->get_render_position(mStream, &halDspFrames),
                                          {ENOSYS} /*ignore*/);
    _hidl_cb(retval, halDspFrames);
    return Void();
}

Return<void> StreamOut::getNextWriteTimestamp(getNextWriteTimestamp_cb _hidl_cb) {
    Result retval(Result::NOT_SUPPORTED);
    int64_t timestampUs = 0;
    if (mStream->get_next_write_timestamp != NULL) {
        retval = Stream::analyzeStatus("get_next_write_timestamp",
                                       mStream->get_next_write_timestamp(mStream, &timestampUs),
                                       {ENOSYS} /*ignore*/);
    }
    _hidl_cb(retval, timestampUs);
    return Void();
}

Return<Result> StreamOut::setCallback(const sp<IStreamOutCallback>& callback) {
    if (mStream->set_callback == NULL) return Result::NOT_SUPPORTED;
    // Safe to pass 'this' because it is guaranteed that the callback thread
    // is joined prior to exit from StreamOut's destructor.
    int result = mStream->set_callback(mStream, StreamOut::asyncCallback, this);
    if (result == 0) {
        mCallback = callback;
    }
    return Stream::analyzeStatus("set_callback", result, {ENOSYS} /*ignore*/);
}

Return<Result> StreamOut::clearCallback() {
    if (mStream->set_callback == NULL) return Result::NOT_SUPPORTED;
    mCallback = nullptr;
    return Result::OK;
}

// static
int StreamOut::asyncCallback(stream_callback_event_t event, void*, void* cookie) {
    // It is guaranteed that the callback thread is joined prior
    // to exiting from StreamOut's destructor. Must *not* use sp<StreamOut>
    // here because it can make this code the last owner of StreamOut,
    // and an attempt to run the destructor on the callback thread
    // will cause a deadlock in the legacy HAL code.
    StreamOut* self = reinterpret_cast<StreamOut*>(cookie);
    // It's correct to hold an sp<> to callback because the reference
    // in the StreamOut instance can be cleared in the meantime. There is
    // no difference on which thread to run IStreamOutCallback's destructor.
    sp<IStreamOutCallback> callback = self->mCallback.load();
    if (callback.get() == nullptr) return 0;
    ALOGV("asyncCallback() event %d", event);
    Return<void> result;
    switch (event) {
        case STREAM_CBK_EVENT_WRITE_READY:
            result = callback->onWriteReady();
            break;
        case STREAM_CBK_EVENT_DRAIN_READY:
            result = callback->onDrainReady();
            break;
        case STREAM_CBK_EVENT_ERROR:
            result = callback->onError();
            break;
        default:
            ALOGW("asyncCallback() unknown event %d", event);
            break;
    }
    ALOGW_IF(!result.isOk(), "Client callback failed: %s", result.description().c_str());
    return 0;
}

Return<void> StreamOut::supportsPauseAndResume(supportsPauseAndResume_cb _hidl_cb) {
    _hidl_cb(mStream->pause != NULL, mStream->resume != NULL);
    return Void();
}

Return<Result> StreamOut::pause() {
    return mStream->pause != NULL
                   ? Stream::analyzeStatus("pause", mStream->pause(mStream), {ENOSYS} /*ignore*/)
                   : Result::NOT_SUPPORTED;
}

Return<Result> StreamOut::resume() {
    return mStream->resume != NULL
                   ? Stream::analyzeStatus("resume", mStream->resume(mStream), {ENOSYS} /*ignore*/)
                   : Result::NOT_SUPPORTED;
}

Return<bool> StreamOut::supportsDrain() {
    return mStream->drain != NULL;
}

Return<Result> StreamOut::drain(AudioDrain type) {
    audio_drain_type_t halDrainType =
            type == AudioDrain::EARLY_NOTIFY ? AUDIO_DRAIN_EARLY_NOTIFY : AUDIO_DRAIN_ALL;
    return mStream->drain != NULL
                   ? Stream::analyzeStatus("drain", mStream->drain(mStream, halDrainType),
                                           {ENOSYS} /*ignore*/)
                   : Result::NOT_SUPPORTED;
}

Return<Result> StreamOut::flush() {
    return mStream->flush != NULL
                   ? Stream::analyzeStatus("flush", mStream->flush(mStream), {ENOSYS} /*ignore*/)
                   : Result::NOT_SUPPORTED;
}

// static
Result StreamOut::getPresentationPositionImpl(audio_stream_out_t* stream, uint64_t* frames,
                                              TimeSpec* timeStamp) {
    // Don't logspam on EINVAL--it's normal for get_presentation_position
    // to return it sometimes. EAGAIN may be returned by A2DP audio HAL
    // implementation. ENODATA can also be reported while the writer is
    // continuously querying it, but the stream has been stopped.
    static const std::vector<int> ignoredErrors{EINVAL, EAGAIN, ENODATA, ENOSYS};
    Result retval(Result::NOT_SUPPORTED);
    if (stream->get_presentation_position == NULL) return retval;
    struct timespec halTimeStamp;
    retval = Stream::analyzeStatus("get_presentation_position",
                                   stream->get_presentation_position(stream, frames, &halTimeStamp),
                                   ignoredErrors);
    if (retval == Result::OK) {
        timeStamp->tvSec = halTimeStamp.tv_sec;
        timeStamp->tvNSec = halTimeStamp.tv_nsec;
    }
    return retval;
}

Return<void> StreamOut::getPresentationPosition(getPresentationPosition_cb _hidl_cb) {
    uint64_t frames = 0;
    TimeSpec timeStamp = {0, 0};
    Result retval = getPresentationPositionImpl(mStream, &frames, &timeStamp);
    _hidl_cb(retval, frames, timeStamp);
    return Void();
}

Return<Result> StreamOut::start() {
    return mStreamMmap->start();
}

Return<Result> StreamOut::stop() {
    return mStreamMmap->stop();
}

Return<void> StreamOut::createMmapBuffer(int32_t minSizeFrames, createMmapBuffer_cb _hidl_cb) {
    return mStreamMmap->createMmapBuffer(minSizeFrames, audio_stream_out_frame_size(mStream),
                                         _hidl_cb);
}

Return<void> StreamOut::getMmapPosition(getMmapPosition_cb _hidl_cb) {
    return mStreamMmap->getMmapPosition(_hidl_cb);
}

Return<void> StreamOut::debug(const hidl_handle& fd, const hidl_vec<hidl_string>& options) {
    return mStreamCommon->debug(fd, options);
}

#if MAJOR_VERSION >= 4
Result StreamOut::doUpdateSourceMetadata(const SourceMetadata& sourceMetadata) {
    std::vector<playback_track_metadata_t> halTracks;
#if MAJOR_VERSION <= 6
    (void)CoreUtils::sourceMetadataToHal(sourceMetadata, &halTracks);
#else
    // Validate whether a conversion to V7 is possible. This is needed
    // to have a consistent behavior of the HAL regardless of the API
    // version of the legacy HAL (and also to be consistent with openOutputStream).
    std::vector<playback_track_metadata_v7> halTracksV7;
    if (status_t status = CoreUtils::sourceMetadataToHalV7(
                sourceMetadata, false /*ignoreNonVendorTags*/, &halTracksV7);
        status == NO_ERROR) {
        halTracks.reserve(halTracksV7.size());
        for (auto metadata_v7 : halTracksV7) {
            halTracks.push_back(std::move(metadata_v7.base));
        }
    } else {
        return Stream::analyzeStatus("sourceMetadataToHal", status);
    }
#endif  // MAJOR_VERSION <= 6
    const source_metadata_t halMetadata = {
        .track_count = halTracks.size(),
        .tracks = halTracks.data(),
    };
    mStream->update_source_metadata(mStream, &halMetadata);
    return Result::OK;
}

#if MAJOR_VERSION >= 7
Result StreamOut::doUpdateSourceMetadataV7(const SourceMetadata& sourceMetadata) {
    std::vector<playback_track_metadata_v7> halTracks;
    if (status_t status = CoreUtils::sourceMetadataToHalV7(
                sourceMetadata, false /*ignoreNonVendorTags*/, &halTracks);
        status != NO_ERROR) {
        return Stream::analyzeStatus("sourceMetadataToHal", status);
    }
    const source_metadata_v7_t halMetadata = {
            .track_count = halTracks.size(),
            .tracks = halTracks.data(),
    };
    mStream->update_source_metadata_v7(mStream, &halMetadata);
    return Result::OK;
}
#endif  //  MAJOR_VERSION >= 7

#if MAJOR_VERSION <= 6
Return<void> StreamOut::updateSourceMetadata(const SourceMetadata& sourceMetadata) {
    if (mStream->update_source_metadata == nullptr) {
        return Void();  // not supported by the HAL
    }
    (void)doUpdateSourceMetadata(sourceMetadata);
    return Void();
}
#elif MAJOR_VERSION >= 7
Return<Result> StreamOut::updateSourceMetadata(const SourceMetadata& sourceMetadata) {
    if (mDevice->version() < AUDIO_DEVICE_API_VERSION_3_2) {
        if (mStream->update_source_metadata == nullptr) {
            return Result::NOT_SUPPORTED;
        }
        return doUpdateSourceMetadata(sourceMetadata);
    } else {
        if (mStream->update_source_metadata_v7 == nullptr) {
            return Result::NOT_SUPPORTED;
        }
        return doUpdateSourceMetadataV7(sourceMetadata);
    }
}
#endif

Return<Result> StreamOut::selectPresentation(int32_t /*presentationId*/, int32_t /*programId*/) {
    return Result::NOT_SUPPORTED;  // TODO: propagate to legacy
}
#endif

#if MAJOR_VERSION >= 6
Return<void> StreamOut::getDualMonoMode(getDualMonoMode_cb _hidl_cb) {
    audio_dual_mono_mode_t mode = AUDIO_DUAL_MONO_MODE_OFF;
    Result retval = mStream->get_dual_mono_mode != nullptr
                            ? Stream::analyzeStatus("get_dual_mono_mode",
                                                    mStream->get_dual_mono_mode(mStream, &mode))
                            : Result::NOT_SUPPORTED;
    _hidl_cb(retval, DualMonoMode(mode));
    return Void();
}

Return<Result> StreamOut::setDualMonoMode(DualMonoMode mode) {
    return mStream->set_dual_mono_mode != nullptr
                   ? Stream::analyzeStatus(
                             "set_dual_mono_mode",
                             mStream->set_dual_mono_mode(mStream,
                                                         static_cast<audio_dual_mono_mode_t>(mode)))
                   : Result::NOT_SUPPORTED;
}

Return<void> StreamOut::getAudioDescriptionMixLevel(getAudioDescriptionMixLevel_cb _hidl_cb) {
    float leveldB = -std::numeric_limits<float>::infinity();
    Result retval = mStream->get_audio_description_mix_level != nullptr
                            ? Stream::analyzeStatus(
                                      "get_audio_description_mix_level",
                                      mStream->get_audio_description_mix_level(mStream, &leveldB))
                            : Result::NOT_SUPPORTED;
    _hidl_cb(retval, leveldB);
    return Void();
}

Return<Result> StreamOut::setAudioDescriptionMixLevel(float leveldB) {
    return mStream->set_audio_description_mix_level != nullptr
                   ? Stream::analyzeStatus(
                             "set_audio_description_mix_level",
                             mStream->set_audio_description_mix_level(mStream, leveldB))
                   : Result::NOT_SUPPORTED;
}

Return<void> StreamOut::getPlaybackRateParameters(getPlaybackRateParameters_cb _hidl_cb) {
    audio_playback_rate_t rate = AUDIO_PLAYBACK_RATE_INITIALIZER;
    Result retval =
            mStream->get_playback_rate_parameters != nullptr
                    ? Stream::analyzeStatus("get_playback_rate_parameters",
                                            mStream->get_playback_rate_parameters(mStream, &rate))
                    : Result::NOT_SUPPORTED;
    _hidl_cb(retval,
             PlaybackRate{rate.mSpeed, rate.mPitch, static_cast<TimestretchMode>(rate.mStretchMode),
                          static_cast<TimestretchFallbackMode>(rate.mFallbackMode)});
    return Void();
}

Return<Result> StreamOut::setPlaybackRateParameters(const PlaybackRate& playbackRate) {
    audio_playback_rate_t rate = {
            playbackRate.speed, playbackRate.pitch,
            static_cast<audio_timestretch_stretch_mode_t>(playbackRate.timestretchMode),
            static_cast<audio_timestretch_fallback_mode_t>(playbackRate.fallbackMode)};
    return mStream->set_playback_rate_parameters != nullptr
                   ? Stream::analyzeStatus("set_playback_rate_parameters",
                                           mStream->set_playback_rate_parameters(mStream, &rate))
                   : Result::NOT_SUPPORTED;
}

Return<Result> StreamOut::setEventCallback(const sp<IStreamOutEventCallback>& callback) {
    if (mStream->set_event_callback == nullptr) return Result::NOT_SUPPORTED;
    int result = mStream->set_event_callback(mStream, StreamOut::asyncEventCallback, this);
    if (result == 0) {
        mEventCallback = callback;
    }
    return Stream::analyzeStatus("set_stream_out_callback", result, {ENOSYS} /*ignore*/);
}

// static
int StreamOut::asyncEventCallback(stream_event_callback_type_t event, void* param, void* cookie) {
    StreamOut* self = reinterpret_cast<StreamOut*>(cookie);
    sp<IStreamOutEventCallback> eventCallback = self->mEventCallback.load();
    if (eventCallback.get() == nullptr) return 0;
    ALOGV("%s event %d", __func__, event);
    Return<void> result;
    switch (event) {
        case STREAM_EVENT_CBK_TYPE_CODEC_FORMAT_CHANGED: {
            hidl_vec<uint8_t> audioMetadata;
            // void* param is the byte string buffer from byte_string_from_audio_metadata().
            // As the byte string buffer may have embedded zeroes, we cannot use strlen()
            // but instead use audio_utils::metadata::dataByteStringLen().
            audioMetadata.setToExternal((uint8_t*)param, audio_utils::metadata::dataByteStringLen(
                                                                 (const uint8_t*)param));
            result = eventCallback->onCodecFormatChanged(audioMetadata);
        } break;
        default:
            ALOGW("%s unknown event %d", __func__, event);
            break;
    }
    ALOGW_IF(!result.isOk(), "Client callback failed: %s", result.description().c_str());
    return 0;
}

#if MAJOR_VERSION == 7 && MINOR_VERSION == 1
Return<Result> StreamOut::setLatencyMode(LatencyMode mode) {
    return mStream->set_latency_mode != nullptr
                   ? Stream::analyzeStatus(
                             "set_latency_mode",
                             mStream->set_latency_mode(mStream,
                                                       static_cast<audio_latency_mode_t>(mode)))
                   : Result::NOT_SUPPORTED;
};

Return<void> StreamOut::getRecommendedLatencyModes(getRecommendedLatencyModes_cb _hidl_cb) {
    Result retval = Result::NOT_SUPPORTED;
    hidl_vec<LatencyMode> hidlModes;
    size_t num_modes = AUDIO_LATENCY_MODE_CNT;
    audio_latency_mode_t modes[AUDIO_LATENCY_MODE_CNT];

    if (mStream->get_recommended_latency_modes != nullptr &&
        mStream->get_recommended_latency_modes(mStream, &modes[0], &num_modes) == 0) {
        if (num_modes == 0 || num_modes > AUDIO_LATENCY_MODE_CNT) {
            ALOGW("%s invalid number of modes returned: %zu", __func__, num_modes);
            retval = Result::INVALID_STATE;
        } else {
            hidlModes.resize(num_modes);
            for (size_t i = 0; i < num_modes; ++i) {
                hidlModes[i] = static_cast<LatencyMode>(modes[i]);
            }
            retval = Result::OK;
        }
    }
    _hidl_cb(retval, hidlModes);
    return Void();
};

// static
void StreamOut::latencyModeCallback(audio_latency_mode_t* modes, size_t num_modes, void* cookie) {
    StreamOut* self = reinterpret_cast<StreamOut*>(cookie);
    sp<IStreamOutLatencyModeCallback> callback = self->mLatencyModeCallback.load();
    if (callback.get() == nullptr) return;

    ALOGV("%s", __func__);

    if (num_modes == 0 || num_modes > AUDIO_LATENCY_MODE_CNT) {
        ALOGW("%s invalid number of modes returned: %zu", __func__, num_modes);
        return;
    }

    hidl_vec<LatencyMode> hidlModes(num_modes);
    for (size_t i = 0; i < num_modes; ++i) {
        hidlModes[i] = static_cast<LatencyMode>(modes[i]);
    }
    Return<void> result = callback->onRecommendedLatencyModeChanged(hidlModes);
    ALOGW_IF(!result.isOk(), "Client callback failed: %s", result.description().c_str());
}

Return<Result> StreamOut::setLatencyModeCallback(
        const sp<IStreamOutLatencyModeCallback>& callback) {
    if (mStream->set_latency_mode_callback == nullptr) return Result::NOT_SUPPORTED;
    int result = mStream->set_latency_mode_callback(mStream, StreamOut::latencyModeCallback, this);
    if (result == 0) {
        mLatencyModeCallback = callback;
    }
    return Stream::analyzeStatus("set_latency_mode_callback", result, {ENOSYS} /*ignore*/);
};

#endif

#endif

}  // namespace implementation
}  // namespace CPP_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android
