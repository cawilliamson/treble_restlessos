#include <iostream>
#include <android/hardware/light/2.0/ILight.h>
#include <android/hardware/light/2.0/types.h>

using ::android::hardware::light::V2_0::ILight;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = ILight::getService();
	svc->getSupportedTypes([](auto types) {
		for(const auto& type: types) {
			std::cout << "Got type " << toString(type) << std::endl;
		}
	});
	if(argc <= 1) return 0;
	if(argc != 3 && argc != 6) return 1;

	std::string typeArg(argv[1]);
	android::hardware::light::V2_0::Type type;
	if(typeArg == "BACKLIGHT")
		type = android::hardware::light::V2_0::Type::BACKLIGHT;
	if(typeArg == "KEYBOARD")
		type = android::hardware::light::V2_0::Type::KEYBOARD;
	if(typeArg == "BUTTONS")
		type = android::hardware::light::V2_0::Type::BUTTONS;
	if(typeArg == "BATTERY")
		type = android::hardware::light::V2_0::Type::BATTERY;
	if(typeArg == "NOTIFICATIONS")
		type = android::hardware::light::V2_0::Type::NOTIFICATIONS;
	if(typeArg == "ATTENTION")
		type = android::hardware::light::V2_0::Type::ATTENTION;
	if(typeArg == "BLUETOOTH")
		type = android::hardware::light::V2_0::Type::BLUETOOTH;
	if(typeArg == "WIFI")
		type = android::hardware::light::V2_0::Type::WIFI;
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

	auto ret = svc->setLight(type, state);
	std::cout << "Set light returned " << toString(ret) << std::endl;
}
