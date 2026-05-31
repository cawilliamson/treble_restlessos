#include <iostream>
#include <unistd.h>
#include <vendor/xiaomi/hardware/fingerprintextension/1.0/IXiaomiFingerprint.h>

using ::vendor::xiaomi::hardware::fingerprintextension::V1_0::IXiaomiFingerprint;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IXiaomiFingerprint::getService();
	if(svc == nullptr) {
		std::cerr << "Failed getting IDisplayFeature" << std::endl;
		return -1;
	}
	if(argc != 3) {
		fprintf(stderr, "Usage: %s <command> <value>\n", argv[0]);
		return -2;
	}
	uint32_t cmd = (uint32_t)strtoll(argv[1], NULL, 16);
	uint32_t value = (uint32_t)strtoll(argv[2], NULL, 16);
	svc->extCmd(cmd, value);
}
