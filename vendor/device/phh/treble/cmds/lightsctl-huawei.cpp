#include <iostream>
#include <vendor/huawei/hardware/light/2.0/ILight.h>
#include <android/hardware/light/2.0/types.h>

using ::vendor::huawei::hardware::light::V2_0::ILight;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = ILight::getService();
	svc->HWgetSupportedTypes([](auto types) {
		for(const auto& type: types) {
			std::cout << "Got type " << type << std::endl;
		}
	});

	uint32_t a = (uint32_t)strtoll(argv[1], NULL, 10);

	android::hardware::light::V2_0::LightState state;
	state.color = (uint32_t)strtoll(argv[2], NULL, 16);
	state.flashMode = android::hardware::light::V2_0::Flash::NONE;
	state.brightnessMode = android::hardware::light::V2_0::Brightness::USER;

	if(argc == 6) {
		std::string flashArg(argv[3]);
		if(flashArg == "NONE")
			state.flashMode = android::hardware::light::V2_0::Flash::NONE;
		if(flashArg == "TIMED")
			state.flashMode = android::hardware::light::V2_0::Flash::TIMED;
		if(flashArg == "HARDWARE")
			state.flashMode = android::hardware::light::V2_0::Flash::HARDWARE;

		state.flashOnMs = atoi(argv[4]);
		state.flashOffMs = atoi(argv[5]);
	}
	std::cout << "Set flash type to " << toString(state.flashMode) << std::endl;

	svc->HWsetLight(a, state);
}
