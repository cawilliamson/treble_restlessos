#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define SET_CUR_VALUE 0

#define Touch_Game_Mode 0
#define Touch_Active_MODE 1
#define Touch_UP_THRESHOLD 2
#define Touch_Tolerance 3
#define Touch_Aim_Sensitivity 4
#define Touch_Tap_Stability 5
#define Touch_Expert_Mode 6
#define Touch_Edge_Filter 7
#define Touch_Panel_Orientation 8
#define Touch_Report_Rate 9
#define Touch_Fod_Enable 10
#define Touch_Aod_Enable 11
#define Touch_Resist_RF 12
#define Touch_Idle_Time 13
#define Touch_Doubletap_Mode 14
#define Touch_Grip_Mode 15
#define Touch_FodIcon_Enable 16
#define Touch_Nonui_Mode 17
#define Touch_Debug_Level 18
#define Touch_Power_Status 19
#define Touch_Mode_NUM 20

#define TOUCH_DEV_PATH "/dev/xiaomi-touch"
#define TOUCH_ID 0
#define TOUCH_MAGIC 0x5400
#define TOUCH_IOC_SETMODE TOUCH_MAGIC + SET_CUR_VALUE

int main(int argc, char **argv) {
    if(argc != 3) {
        fprintf(stderr, "Usage: %s <mode> <0|1>\n", argv[0]);
        return -1;
    }
    int mode = atoi(argv[1]);
    int enabled = atoi(argv[2]);
    if (mode < 0 || mode > 20) return -1;
    if (enabled != 0 && enabled != 1) return -1;
    int fd = open(TOUCH_DEV_PATH, O_RDWR);
    // Xiaomi has two competing ABIs, no idea how to detect it
    int arg[3] = {TOUCH_ID, mode, enabled};
    int arg2[3] = {mode, enabled};
    ioctl(fd, TOUCH_IOC_SETMODE, &arg);
    ioctl(fd, TOUCH_IOC_SETMODE, &arg2);
    close(fd);
}
