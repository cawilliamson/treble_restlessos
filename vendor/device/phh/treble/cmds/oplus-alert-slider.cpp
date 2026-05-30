#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <linux/input.h>

int read_tristate() {
    int fd = open("/proc/tristatekey/tri_state", O_RDONLY);
    char p[16];
    int ret = read(fd, p, sizeof(p) - 1);
    p[ret] = 0;
    return atoi(p);
}

int main() {
    int fd = -1;
    for(int i=0; i<255; i++) {
        char path[256];
        snprintf(path, 256, "/dev/input/event%d", i);
        fd = open(path, O_RDWR);
        if(fd == -1) continue;
        char name[256];
        ioctl(fd, EVIOCGNAME(256), name);
        printf("Got input name %s\n", name);
        if(strcmp(name, "oplus,hall_tri_state_key") == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }
    if(fd == -1) return 0;

    ioctl(fd, EVIOCGRAB, 1);

    struct input_event ev;
    while(read(fd, &ev, sizeof(ev)) != 0) {
        if(!(ev.code == 61 && ev.value == 0)) continue;
        int state = read_tristate();
        if(state == 1) {
        }
        printf("State %d\n", read_tristate());
        if(state == 1) {
            system("service call audio 41 i32 0 s16 android");
        } else if(state == 2) {
            system("service call audio 41 i32 1 s16 android");
        } else if(state == 3) {
            system("service call audio 41 i32 2 s16 android");
        }
    }
}
