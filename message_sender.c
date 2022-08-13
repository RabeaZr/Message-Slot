#include "message_slot.h"

#include <fcntl.h>      /* open */
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char** argv)
{
    int fd, ret;
    char *message = argv[3];
    int msg_size = strlen(message);
    unsigned long id_ch = atoi(argv[2]);

    if (argc != 4) {
        fprintf(stderr, "you didn't provide 4 arguments : ERROR %s\n", strerror(EINVAL));
        exit(1);
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "there was a problem while opening device file : %s\n Error: %s\n", argv[1], strerror(errno));
        exit(1);
    }

    ret = ioctl(fd, MSG_SLOT_CHANNEL, id_ch);
    if (ret != 0) {
        fprintf(stderr, "there was a problem with ioctl, ERROR : %s\n", strerror(errno));
        exit(1);
    }
    ret = write(fd, message, msg_size);
    if (ret != msg_size) {
        fprintf(stderr, "there was a problem with write, ERROR : %s\n", strerror(errno));
        exit(1);
    }
    close(fd);
    exit(0);
}