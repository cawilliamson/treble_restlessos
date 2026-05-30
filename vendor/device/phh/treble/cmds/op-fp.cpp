#include <iostream>
#include <vendor/oneplus/fingerprint/extension/1.0/IVendorFingerprintExtensions.h>

using ::vendor::oneplus::fingerprint::extension::V1_0::IVendorFingerprintExtensions;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IVendorFingerprintExtensions::getService();
	if(argc!=2) return 1;
	int v = atoi(argv[0]);
	auto ret = svc->updateStatus(v);
	if(!ret.isOk()) {
		std::cerr << "HWBinder call failed" << std::endl;
	}
	std::cout << "updateStatus returned " << ret << std::endl;
}
