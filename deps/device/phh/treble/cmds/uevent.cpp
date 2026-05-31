typedef unsigned short int sa_family_t;
#define __KERNEL_STRICT_NAMES
#include <sys/types.h>
#include <linux/netlink.h>
#include <sys/socket.h> 
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

//From a uevent patch for hal
#define HOTPLUG_BUFFER_SIZE             1024
#define HOTPLUG_NUM_ENVP                32
#define OBJECT_SIZE                     512

#ifndef NETLINK_KOBJECT_UEVENT
#error Your kernel headers are too old, and do not define NETLINK_KOBJECT_UEVENT. You need Linux 2.6.10 or higher for KOBJECT_UEVENT support.
#endif

int main(int argc, char **argv, char **envp) {
        //Start listening
        int fd;
        struct sockaddr_nl ksnl;
        memset(&ksnl, 0x00, sizeof(struct sockaddr_nl));
        ksnl.nl_family=AF_NETLINK;
        ksnl.nl_pid=getpid();
        ksnl.nl_groups=0xffffffff;
        fd=socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
        if (fd==-1) {
                printf("Couldn't open kobject-uevent netlink socket");
                perror("");
                exit(1);
        }
        if (bind(fd, (struct sockaddr *) &ksnl, sizeof(struct sockaddr_nl))<0) {
                fprintf (stderr, "Error binding to netlink socket");
                close(fd);
                exit(1);
        }

        while(1) {
                char buffer[HOTPLUG_BUFFER_SIZE + OBJECT_SIZE];
                int buflen;
                buflen=recv(fd, &buffer, sizeof(buffer), 0);
                if (buflen<0) {
                        exit(1);
                }

                if(argc>=2) {
                    if(!strstr(buffer, argv[1])) continue;
                }

                printf("%s\n", buffer);
                char *pos = buffer + strlen(buffer);
                char *end = buffer + buflen;
                while(pos < end) {
                    int l = strlen(pos);
                    printf("\t%s\n", pos);
                    pos += l+1;
                }
        }

}
