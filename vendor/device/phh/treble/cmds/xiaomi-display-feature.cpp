#include <iostream>
#include <unistd.h>
#include <vendor/xiaomi/hardware/displayfeature/1.0/IDisplayFeature.h>

using ::vendor::xiaomi::hardware::displayfeature::V1_0::IDisplayFeature;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IDisplayFeature::getService();
	if(svc == nullptr) {
		std::cerr << "Failed getting IDisplayFeature" << std::endl;
		return -1;
	}
	if(argc != 3) {
		fprintf(stderr, "Usage: %s <ADAPT|ENHANCE|STANDARD|EYECARE|MONOCHROME|SUNLIGHT|NIGHTLIGHT|HIGHLIGHT> <value>\n", argv[0]);
		return -2;
	}
	int mode = -1;
	std::string modeArg(argv[1]);
	if(modeArg == "ADAPT")
		mode = 0;
	if(modeArg == "ENHANCE")
		mode = 1;
	if(modeArg == "STANDARD")
		mode = 2;
	if(modeArg == "EYECARE")
		mode = 3;
	if(modeArg == "MONOCHROME")
		mode = 4;
	if(modeArg == "SUNLIGHT")
		mode = 8;
	if(modeArg == "NIGHTLIGHT")
		mode = 9;
	if(modeArg == "HIGHLIGHT")
		mode = 11;
    else
        mode = atoi(argv[1]);

	svc->setFeature(0, mode, atoi(argv[2]), 255);
}
