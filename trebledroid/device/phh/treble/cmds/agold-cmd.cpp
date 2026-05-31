#include <iostream>
#include <vendor/mediatek/hardware/agolddaemon/IAgoldDaemon.h>
#include <binder/IServiceManager.h>

using ::vendor::mediatek::hardware::agolddaemon::IAgoldDaemon;
using ::android::sp;

int main(int argc, char **argv) {
	auto svc = android::checkVintfService<IAgoldDaemon>();
    android::String16 a;
    svc->getNotGsi(&a);
    std::cout << "notgsi is " << a << std::endl;
    if (argc >= 2) {
        android::String16 b(argv[1]);
        svc->setNotGsi(b);
    }
    svc->getNotGsi(&a);
    std::cout << "notgsi is " << a << std::endl;

    svc->setCameraClientPackageName(android::String16("com.mediatek.camera"));

}
