#include <iostream>
#include <vendor/samsung/hardware/light/2.0/ISecLight.h>
#include <vendor/samsung/hardware/light/2.0/types.h>

using ::vendor::samsung::hardware::light::V2_0::ISecLight;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = ISecLight::getService();
	svc->getSupportedTypes([](auto types) {
		for(const auto& type: types) {
			std::cout << "Got type " << toString(type) << std::endl;
		}
	});
	if(argc <= 1) return 0;
	if(argc != 3 && argc != 6) return 1;

	std::string typeArg(argv[1]);
	vendor::samsung::hardware::light::V2_0::SecType type;
	if(typeArg == "BACKLIGHT")
		type = vendor::samsung::hardware::light::V2_0::SecType::BACKLIGHT;
	if(typeArg == "KEYBOARD")
		type = vendor::samsung::hardware::light::V2_0::SecType::KEYBOARD;
	if(typeArg == "BUTTONS")
		type = vendor::samsung::hardware::light::V2_0::SecType::BUTTONS;
	if(typeArg == "BATTERY")
		type = vendor::samsung::hardware::light::V2_0::SecType::BATTERY;
	if(typeArg == "NOTIFICATIONS")
		type = vendor::samsung::hardware::light::V2_0::SecType::NOTIFICATIONS;
	if(typeArg == "ATTENTION")
		type = vendor::samsung::hardware::light::V2_0::SecType::ATTENTION;
	if(typeArg == "BLUETOOTH")
		type = vendor::samsung::hardware::light::V2_0::SecType::BLUETOOTH;
	if(typeArg == "WIFI")
		type = vendor::samsung::hardware::light::V2_0::SecType::WIFI;
	std::cout << "Set request type " << toString(type) << std::endl;

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

	auto ret = svc->setLightSec(type, state);
	std::cout << "Set light returned " << toString(ret) << std::endl;
}
