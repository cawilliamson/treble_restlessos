#include <iostream>
#include <vendor/samsung/hardware/camera/provider/4.0/ISehCameraProvider.h>

using ::vendor::samsung::hardware::camera::provider::V4_0::ISehCameraProvider;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = ISehCameraProvider::getService("legacy/0");
    auto cb = [](::android::hardware::camera::common::V1_0::Status status, ::android::hardware::hidl_vec<::android::hardware::hidl_string> ids) {
        for(auto id: ids) {
            std::cerr << "id = " << id << std::endl;
        }
    };
    svc->sehGetCameraIdList(cb);
}
