#include <iostream>
#include <vendor/samsung/hardware/light/3.0/ISehLight.h>
#include <vendor/samsung/hardware/light/3.0/types.h>

using ::vendor::samsung::hardware::light::V3_0::ISehLight;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = ISehLight::getService();
	svc->getSupportedTypes([](auto types) {
		for(const auto& type: types) {
			std::cout << "Got type " << toString(type) << std::endl;
		}
	});
	if(argc <= 1) return 0;
	if(argc != 4 && argc != 7) return 1;

	std::string typeArg(argv[1]);
	vendor::samsung::hardware::light::V3_0::SehType type;
	if(typeArg == "BACKLIGHT")
		type = vendor::samsung::hardware::light::V3_0::SehType::BACKLIGHT;
	if(typeArg == "KEYBOARD")
		type = vendor::samsung::hardware::light::V3_0::SehType::KEYBOARD;
	if(typeArg == "BUTTONS")
		type = vendor::samsung::hardware::light::V3_0::SehType::BUTTONS;
	if(typeArg == "BATTERY")
		type = vendor::samsung::hardware::light::V3_0::SehType::BATTERY;
	if(typeArg == "NOTIFICATIONS")
		type = vendor::samsung::hardware::light::V3_0::SehType::NOTIFICATIONS;
	if(typeArg == "ATTENTION")
		type = vendor::samsung::hardware::light::V3_0::SehType::ATTENTION;
	if(typeArg == "BLUETOOTH")
		type = vendor::samsung::hardware::light::V3_0::SehType::BLUETOOTH;
	if(typeArg == "WIFI")
		type = vendor::samsung::hardware::light::V3_0::SehType::WIFI;
	if(typeArg == "SUB_BACKLIGHT")
		type = vendor::samsung::hardware::light::V3_0::SehType::SUB_BACKLIGHT;
	std::cout << "Set request type " << toString(type) << std::endl;

	vendor::samsung::hardware::light::V3_0::SehLightState state;
	state.color = (uint32_t)strtoll(argv[2], NULL, 16);
	state.flashMode = android::hardware::light::V2_0::Flash::NONE;
	state.brightnessMode = android::hardware::light::V2_0::Brightness::USER;
	state.extendedBrightness = (uint32_t)strtoll(argv[3], NULL, 0);

	if(argc == 7) {
		std::string flashArg(argv[4]);
		if(flashArg == "NONE")
			state.flashMode = android::hardware::light::V2_0::Flash::NONE;
		if(flashArg == "TIMED")
			state.flashMode = android::hardware::light::V2_0::Flash::TIMED;
		if(flashArg == "HARDWARE")
			state.flashMode = android::hardware::light::V2_0::Flash::HARDWARE;

		state.flashOnMs = atoi(argv[5]);
		state.flashOffMs = atoi(argv[6]);
	}
	std::cout << "Set flash type to " << toString(state.flashMode) << std::endl;

	auto ret = svc->sehSetLight(type, state);
	std::cout << "Set light returned " << toString(ret) << std::endl;
}
