#include <iostream>
#include <stdint.h>
#include <vector>
#include <vendor/samsung/hardware/radio/2.2/ISehRadio.h>
#include <android/hardware/radio/1.6/IRadio.h>

using ::vendor::samsung::hardware::radio::V2_2::ISehRadio;
using ::android::hardware::radio::V1_6::IRadio;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = ISehRadio::getService(argv[1]);

	//setNrMode
#if 0
	android::hardware::hidl_vec<uint8_t> cmd = {
		2, 131, 0, 5, (uint8_t)atoi(argv[2]),
	};
	svc->sendRequestRaw(5556, cmd);
	svc->setNrMode(5555, atoi(argv[1]));
#endif

#if 0
	//notifyRilConnected
	android::hardware::hidl_vec<uint8_t> cmd = {
		11,24,0,5,0,
	};
	svc->sendRequestRaw(5556, cmd);
#endif
	svc->setNrMode_2_2(5555, atoi(argv[2]), true);

#if 0
	auto svcRadio = IRadio::getService(argv[1]);
	svcRadio->setAllowedNetworkTypesBitmap(4444, 0x1bfffe);
	svcRadio->setPreferredNetworkTypeBitmap(4443, 0x1bfffe);
#endif
}
