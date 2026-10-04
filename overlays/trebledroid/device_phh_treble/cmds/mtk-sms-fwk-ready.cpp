#include <iostream>
#include <unistd.h>
#include <vendor/mediatek/hardware/radio/2.6/IRadio.h>

using ::vendor::mediatek::hardware::radio::V2_6::IRadio;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IRadio::getService(argv[1]);
	if(svc != nullptr)
		svc->setSmsFwkReady(1);
}
