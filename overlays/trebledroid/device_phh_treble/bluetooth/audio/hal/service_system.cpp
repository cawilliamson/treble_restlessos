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

#define LOG_TAG "BtAudioAIDLServiceSystem"

#include <dlfcn.h>
#include <signal.h>

#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <utils/Log.h>
#include <hidl/HidlTransportSupport.h>
#include <hwbinder/ProcessState.h>
#include <binder/ProcessState.h>

#include "BluetoothAudioProviderFactory.h"

using ::aidl::android::hardware::bluetooth::audio::
    BluetoothAudioProviderFactory;

#if defined(__LP64__)
#define HAL_LIBRARY_PATH "/system/lib64/hw"
#else
#define HAL_LIBRARY_PATH "/system/lib/hw"
#endif

void registerAudioInterfaces() {
  const char *interface_libs[] = {
    "android.hardware.audio@2.0-impl-system.so",
    "android.hardware.audio@4.0-impl-system.so",
    "android.hardware.audio@5.0-impl-system.so",
    "android.hardware.audio@6.0-impl-system.so",
    "android.hardware.audio@7.0-impl-system.so",
    "android.hardware.audio@7.1-impl-system.so",
  };

  for (auto& lib : interface_libs) {
    void* handle = dlopen((std::string() + HAL_LIBRARY_PATH + "/" + lib).c_str(), RTLD_NOW);
    if (handle == nullptr) {
      ALOGW("Failed to load %s, skipping", lib);
      continue;
    }

    ::android::status_t (*entry_func)(const char*);
    entry_func = reinterpret_cast<::android::status_t (*)(const char*)>(dlsym(handle, "createIDevicesFactory"));

    if (entry_func == nullptr) {
      ALOGW("Cannot find entry symbol in %s, skipping", lib);
      continue;
    }

    ::android::status_t status = entry_func("sysbta");
    ALOGW_IF(status != STATUS_OK, "Could not register sysbta for %s, status=%d", lib, status);
  }
}

int main() {
  signal(SIGPIPE, SIG_IGN);

  ::android::hardware::configureRpcThreadpool(16, true);
  ::android::ProcessState::initWithDriver("/dev/binder");
  // start a threadpool for binder / hwbinder interactions
  ::android::ProcessState::self()->startThreadPool();
  ::android::hardware::ProcessState::self()->startThreadPool();

  auto factory = ::ndk::SharedRefBase::make<BluetoothAudioProviderFactory>();
  const std::string instance_name =
      std::string() + BluetoothAudioProviderFactory::descriptor + "/sysbta";
  binder_status_t aidl_status = AServiceManager_addService(
      factory->asBinder().get(), instance_name.c_str());
  ALOGW_IF(aidl_status != STATUS_OK, "Could not register %s, status=%d",
           instance_name.c_str(), aidl_status);

  // We must also implement audio HAL interfaces in order to serve audio.sysbta.default.so
  // It must be served in the *same* process to access the same libbluetooth_audio_session
  registerAudioInterfaces();

  ::android::hardware::joinRpcThreadpool();
}
