#include <iostream>
#include <unistd.h>
#include <vendor/oneplus/hardware/motorcontrol/1.0/IOPMotorControl.h>

using ::vendor::oneplus::hardware::motorcontrol::V1_0::IOPMotorControl;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IOPMotorControl::getService();
	if(svc == nullptr) {
		std::cerr << "Failed getting IMotor" << std::endl;
		return -1;
	}
	if(argc<2) {
		std::cerr << "Usage: " << argv[0] << " <read|down|up>" << std::endl;
		return -1;
	}
	std::string cmd(argv[1]);
	if(cmd == "read") {
		int ret = svc->readMotorData(1, 16);
		std::cout << "Read motor data 1/16 returned " << ret << std::endl;
		return 0;
	} else if(cmd == "down") {
		int ret = svc->writeMotorData(1, 0, 1);
		std::cout << "Down motor control data 1/0/1 returned " << ret << std::endl;
		return 0;
	} else if(cmd == "up") {
		int ret = svc->writeMotorData(1, 1, 1);
		std::cout << "Down motor control data 1/1/1 returned " << ret << std::endl;
		return 0;
	}
}
