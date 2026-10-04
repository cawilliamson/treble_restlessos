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

#ifndef ANDROID_HARDWARE_AUDIO_DEVICESFACTORY_H
#define ANDROID_HARDWARE_AUDIO_DEVICESFACTORY_H

#include PATH(android/hardware/audio/FILE_VERSION/IDevicesFactory.h)

#include <hardware/audio.h>

#include <hidl/Status.h>

#include <hidl/MQDescriptor.h>
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
using namespace ::android::hardware::audio::CPP_VERSION;

struct DevicesFactory : public IDevicesFactory {
#if MAJOR_VERSION == 2
    Return<void> openDevice(IDevicesFactory::Device device, openDevice_cb _hidl_cb) override;
#elif MAJOR_VERSION >= 4
    Return<void> openDevice(const hidl_string& device, openDevice_cb _hidl_cb) override;
    Return<void> openPrimaryDevice(openPrimaryDevice_cb _hidl_cb) override;
#endif
#if MAJOR_VERSION == 7 && MINOR_VERSION == 1
    Return<void> openDevice_7_1(const hidl_string& device, openDevice_7_1_cb _hidl_cb) override;
    Return<void> openPrimaryDevice_7_1(openPrimaryDevice_7_1_cb _hidl_cb) override;
#endif

  private:
    template <class DeviceShim, class Callback>
    Return<void> openDevice(const char* moduleName, Callback _hidl_cb);
#if MAJOR_VERSION == 2
    Return<void> openDevice(const char* moduleName, openDevice_cb _hidl_cb);
#endif

    static int loadAudioInterface(const char* if_name, audio_hw_device_t** dev);
};

extern "C" IDevicesFactory* HIDL_FETCH_IDevicesFactory(const char* name);

}  // namespace implementation
}  // namespace CPP_VERSION
}  // namespace audio
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_AUDIO_DEVICESFACTORY_H
