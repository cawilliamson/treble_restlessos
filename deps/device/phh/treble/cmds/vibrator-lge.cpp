#include <iostream>
#include <vendor/lge/hardware/vibrator/1.0/IVibratorEx.h>

using ::vendor::lge::hardware::vibrator::V1_0::IVibratorEx;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IVibratorEx::getService();

	auto supportsAmplitude = svc->supportsAmplitudeControl();
	if(supportsAmplitude.isOk())
		std::cerr << "supportsAmplitudeControl? " << supportsAmplitude << std::endl;

	/*
public int on(int timeoutMs) throws RemoteException {
public int off() throws RemoteException {
public int setAmplitude(byte amplitude) throws RemoteException {
public void perform(int effect, byte strength, performCallback _hidl_cb) throws RemoteException {
public int playEffectWithStrength(ArrayList<Byte> effectData, int effectIndex, int strength) throws RemoteException {
*/


	if(strcmp(argv[1], "on") == 0) {
		int v = 100;
		if(argc>=3)
			v = atoi(argv[2]);
		auto ret = svc->on(v);
		if(ret.isOk()) {
			android::hardware::vibrator::V1_0::Status r = ret;
			std::cout << "vibrator on returned " << (int)r << std::endl;
		} else {
			std::cerr << "Binder failed request" << std::endl;
		}
	} else if(strcmp(argv[1], "amplitude") == 0) {
		int v = 127;
		if(argc>=3)
			v = atoi(argv[2]);
		auto ret = svc->setAmplitude(v);
		if(ret.isOk()) {
			android::hardware::vibrator::V1_0::Status r = ret;
			std::cout << "vibrator amplitude returned " << (int)r << std::endl;
		} else {
			std::cerr << "Binder failed request" << std::endl;
		}
	} else {
		std::cerr << "Not supported (yet)" << std::endl;
	}
}
