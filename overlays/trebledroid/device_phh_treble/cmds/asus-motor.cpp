#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct {
    int dir;
    int angle;
    int speed;
} motorDrvManualConfig_t;

#define ASUS_MOTOR_NAME_SIZE 32
#define ASUS_MOTOR_DATA_SIZE 4

#define ASUS_MOTOR_DRV_DEV_PATH    ("/dev/asusMotoDrv")
#define ASUS_MOTOR_DRV_IOC_MAGIC                      ('M')
#define ASUS_MOTOR_DRV_AUTO_MODE _IOW(ASUS_MOTOR_DRV_IOC_MAGIC, 1, int)
#define ASUS_MOTOR_DRV_MANUAL_MODE _IOW(ASUS_MOTOR_DRV_IOC_MAGIC, 2, motorDrvManualConfig_t)

int main(int argc, char **argv) {
	if(argc != 2) return 1;
	int fd = open("/dev/asusMotoDrv", O_RDWR);
	motorDrvManualConfig_t cfg;
	cfg.dir = atoi(argv[1]);
	cfg.angle = 180;
	cfg.speed = 4;

	ioctl(fd, ASUS_MOTOR_DRV_MANUAL_MODE, &cfg);
	close(fd);
}
