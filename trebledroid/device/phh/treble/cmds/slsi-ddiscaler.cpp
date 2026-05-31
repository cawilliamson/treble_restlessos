#include <iostream>
#include <vendor/samsung_slsi/hardware/ExynosHWCServiceTW/1.0/IExynosHWCServiceTW.h>

using ::vendor::samsung_slsi::hardware::ExynosHWCServiceTW::V1_0::IExynosHWCServiceTW;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IExynosHWCServiceTW::getService();
    svc->setDDIScaler(atoi(argv[1]), atoi(argv[2]));
}
