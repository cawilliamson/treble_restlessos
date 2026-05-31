#include <iostream>
#include <vendor/samsung/hardware/camera/provider/4.0/ISehCameraProvider.h>
#include <vendor/samsung/hardware/camera/device/5.0/ISehCameraDevice.h>

using ::vendor::samsung::hardware::camera::provider::V4_0::ISehCameraProvider;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = ISehCameraProvider::getService("legacy/0");
	android::hardware::camera::common::V1_0::Status status;
	android::sp<android::hardware::camera::device::V3_2::ICameraDevice> camera_old;
	android::hardware::Return<void> ret = svc->getCameraDeviceInterface_V3_x(argv[3], [&status, &camera_old](android::hardware::camera::common::V1_0::Status s, android::sp<android::hardware::camera::device::V3_2::ICameraDevice> intf) {
			status = s;
			if(intf == nullptr) {
				fprintf(stderr, "Failed getting camera intf\n");
			}
			camera_old = intf;
			});
	if(!ret.isOk())
		fprintf(stderr, "Failed getting camera 0\n");

	auto cameraResult = vendor::samsung::hardware::camera::device::V5_0::ISehCameraDevice::castFrom(camera_old);
	android::sp<vendor::samsung::hardware::camera::device::V5_0::ISehCameraDevice> camera = cameraResult;
	if(camera == nullptr) {
		fprintf(stderr, "Coulnd't get sammy device\n");
	}
	if(camera != nullptr) {
		fprintf(stderr, "Got sammy camera device\n");
		if(strcmp(argv[1], "on") == 0) {
			fprintf(stderr, "Truning torch mode on power %d\n", atoi(argv[2]));
			camera->sehSetTorchModeStrength(android::hardware::camera::common::V1_0::TorchMode::ON, atoi(argv[2]));
		} else {
			camera->sehSetTorchModeStrength(android::hardware::camera::common::V1_0::TorchMode::OFF, 0);
		}
	}

}
