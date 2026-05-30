#include <iostream>
#include <android/hardware/light/ILights.h>
#include <binder/IServiceManager.h>

using ::android::hardware::light::ILights;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = android::waitForVintfService<ILights>();
	std::vector<android::hardware::light::HwLight> lights;
	svc->getLights(&lights);
	for(const auto& l : lights) {
		std::cout << "Got light " << l.id << std::endl;
		std::cout << "   " << l.ordinal << std::endl;
		std::cout << "   " << toString(l.type) << std::endl;
	}
	if(argc <= 1) return 0;
	if(argc != 3 && argc != 6) return 1;

	std::string typeArg(argv[1]);
	android::hardware::light::LightType type;
	if(typeArg == "BACKLIGHT")
		type = android::hardware::light::LightType::BACKLIGHT;
	if(typeArg == "KEYBOARD")
		type = android::hardware::light::LightType::KEYBOARD;
	if(typeArg == "BUTTONS")
		type = android::hardware::light::LightType::BUTTONS;
	if(typeArg == "BATTERY")
		type = android::hardware::light::LightType::BATTERY;
	if(typeArg == "NOTIFICATIONS")
		type = android::hardware::light::LightType::NOTIFICATIONS;
	if(typeArg == "ATTENTION")
		type = android::hardware::light::LightType::ATTENTION;
	if(typeArg == "BLUETOOTH")
		type = android::hardware::light::LightType::BLUETOOTH;
	if(typeArg == "WIFI")
		type = android::hardware::light::LightType::WIFI;
	std::cout << "Set request type " << toString(type) << std::endl;

	int lightId = -1;
	for(const auto& l : lights) {
		if(l.type == type) {
			lightId = l.id;
			std::cout << "Got matching light " << l.id << std::endl;
			std::cout << "   " << l.ordinal << std::endl;
			std::cout << "   " << toString(l.type) << std::endl;
		}
	}

	android::hardware::light::HwLightState state;
	state.color = (uint32_t)strtoll(argv[2], NULL, 16);
	state.flashMode = android::hardware::light::FlashMode::NONE;
	state.brightnessMode = android::hardware::light::BrightnessMode::USER;

	if(argc == 6) {
		std::string flashArg(argv[3]);
		if(flashArg == "NONE")
			state.flashMode = android::hardware::light::FlashMode::NONE;
		if(flashArg == "TIMED")
			state.flashMode = android::hardware::light::FlashMode::TIMED;
		if(flashArg == "HARDWARE")
			state.flashMode = android::hardware::light::FlashMode::HARDWARE;

		state.flashOnMs = atoi(argv[4]);
		state.flashOffMs = atoi(argv[5]);
	}
	std::cout << "Set flash type to " << toString(state.flashMode) << std::endl;

	auto ret = svc->setLightState(lightId, state);
	std::cout << "Set light returned " << ret << std::endl;
}
