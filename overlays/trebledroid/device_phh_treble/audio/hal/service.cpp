#include "DevicesFactory.h"
#include PATH(android/hardware/audio/FILE_VERSION/IDevicesFactory.h)

#include <hidl/Status.h>

using ::android::status_t;
using ::android::hardware::audio::CPP_VERSION::implementation::DevicesFactory;
using namespace ::android::hardware::audio::CPP_VERSION;

extern "C" __attribute__((visibility("default")))
status_t createIDevicesFactory(const char *instance_name) {
  ::android::sp<IDevicesFactory> audio_factory = new DevicesFactory();
  ::android::status_t hidl_status = audio_factory->registerAsService(instance_name);
  return hidl_status;
}
