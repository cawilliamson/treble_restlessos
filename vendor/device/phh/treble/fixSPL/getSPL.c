#define _GNU_SOURCE
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
	if(argc!=3) {
		fprintf(stderr, "Usage: %s <bootimg> <android|spl>\n", argv[0]);
		exit(-1);
	}
	int fd = open(argv[1], O_RDONLY);
	lseek(fd, 11*4, SEEK_SET);
	uint32_t val = 0;
	read(fd, &val, sizeof(val));
	int android = val >> 11;
	int a = android >> 14;
	int b = (android >> 7) & 0x7f;
	int c = android & 0x7f;

	int spl = val & 0x7ff;
	int y = 2000 + (spl >> 4);
	int m = spl & 0xf;


    if (spl != 0) {
        fprintf(stderr, "Android: %d.%d.%d\n", a, b, c);
        fprintf(stderr, "SPL: %d-%d-01\n", y, m);
        if(strcmp(argv[2], "android") == 0) {
            printf("%d.%d.%d", a, b, c);
        } else if(strcmp(argv[2], "spl") == 0) {
            printf("%04d-%02d-%02d", y, m, 1);
        }
        return 0;
    }

	off_t size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
    char *buf = malloc(size);
    read(fd, buf, size);

#define PROP_OS_VERSION "com.android.build.boot.os_version"
#define PROP_SPL "com.android.build.boot.security_patch"
    char* p = memmem(buf, size, PROP_OS_VERSION, sizeof(PROP_OS_VERSION));
    if(p != NULL) {
          p += sizeof(PROP_OS_VERSION);
          fprintf(stderr, "Android: %s\n", p);
          if(strcmp(argv[2], "android") == 0) {
              printf("%s.0.0", p);
          }
    }

    p = memmem(buf, size, PROP_SPL, sizeof(PROP_SPL));
    if(p != NULL) {
          p += sizeof(PROP_SPL);
          fprintf(stderr, "SPL: %s\n", p);
          if(strcmp(argv[2], "spl") == 0) {
              printf("%s", p);
          }
    }

	return 0;
}
