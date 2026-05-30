#include <iostream>
#include <vendor/goodix/extend/service/2.0/IGoodixFPExtendService.h>

using ::vendor::goodix::extend::service::V2_0::IGoodixFPExtendService;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = IGoodixFPExtendService::getService();
	if(argc!=3) return 1;
	int a = atoi(argv[1]);
	int b = atoi(argv[2]);
	auto ret = svc->goodixExtendCommand(a, b);
	if(!ret.isOk()) {
		std::cerr << "HWBinder call failed" << std::endl;
	}
	std::cout << "updateStatus returned " << toString(ret) << std::endl;
}
