#include <iostream>
#include <unistd.h>
#include <vendor/xiaomi/hardware/motor/1.0/IMotor.h>

using ::vendor::xiaomi::hardware::motor::V1_0::IMotor;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IMotor::getService();
	if(svc == nullptr) {
		std::cerr << "Failed getting IMotor" << std::endl;
		return -1;
	}
	if(argc<2) {
		std::cerr << "Usage: " << argv[0] << " <init|release|popup|takeback|takebackShortly>" << std::endl;
		return -1;
	}
	std::string cmd(argv[1]);
	if(cmd == "init") {
		svc->init();
	} else if(cmd == "release") {
		svc->release();
	} else if(cmd == "popup") {
		if(argc!=3) {
			std::cerr << "Usage: " << argv[0] << " " << argv[1] << " <cookie>" << std::endl;
			return -1;
		}
		svc->popupMotor(atoi(argv[2]));
	} else if(cmd == "takeback") {
		if(argc!=3) {
			std::cerr << "Usage: " << argv[0] << " " << argv[1] << " <cookie>" << std::endl;
			return -1;
		}
		svc->takebackMotor(atoi(argv[2]));
	} else if(cmd == "takebackShortly") {
		svc->takebackMotorShortly();
	}
}
