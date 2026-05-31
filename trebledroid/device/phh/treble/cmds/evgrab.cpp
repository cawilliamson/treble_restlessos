#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

int main(int argc, char **argv) {
    std::string s = android::base::GetProperty("persist.sys.phh.evgrab", "");
    s = android::base::Trim(s);
    if(s == "") return 0;
    auto devs = android::base::Split(s, ";");

    int fd = -1;
    for(int i=0; i < 30; i++) {
        auto path = android::base::StringPrintf("/dev/input/event%d", i);
        fd = open(path.c_str(), O_RDWR);
        if(fd < 0) continue;

        char name[256];
        memset(name, 0, sizeof(name));
        int ret = ioctl(fd, EVIOCGNAME(sizeof(name)), name);
        bool found = false;
        for(const auto& v: devs) {
            printf("Got name %s, trying %s\n", name, v.c_str());
            if(strcmp(v.c_str(), name) == 0) {
                found = true;
                break;
            }
        }
        if(found) break;
        close(fd);
        fd = -1;
    }
    if (fd == -1) return 0;

    int grab = 1;
    ioctl(fd, EVIOCGRAB, &grab);
    struct input_event ev;
    while(true) {
        read(fd, &ev, sizeof(ev));
    }
    return 0;
}
