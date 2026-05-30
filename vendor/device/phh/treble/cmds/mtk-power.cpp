#include <iostream>
#include <unistd.h>
#include <vendor/mediatek/hardware/mtkpower/1.0/IMtkPower.h>

using ::vendor::mediatek::hardware::mtkpower::V1_0::IMtkPower;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IMtkPower::getService();
	if(svc == nullptr) {
		std::cerr << "Failed getting IMtkPower" << std::endl;
		return -1;
	}
	svc->mtkPowerHint(atoi(argv[1]), atoi(argv[2]));
}
