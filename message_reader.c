#include "message_slot.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char** argv)
{
    int fd, ret;
    char buf[BUF_LEN];
    if (argc != 3) {
        fprintf(stderr, "you didn't provide 3 arguments, ERROR : %s\n", strerror(EINVAL));
        exit(1);
    }

    fd = open(argv[1], O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "there was a problem while opening the device file %s\n ERROR: %s\n", DEVICE_RANGE_NAME, strerror(errno));
        exit(1);
    }

    ret = ioctl(fd, MSG_SLOT_CHANNEL, atoi(argv[2]));
    if (ret != 0) {
        fprintf(stderr, "there was a problem with ioctl, ERROR : %s\n", strerror(errno));
        exit(1);
    }

    ret = read(fd, buf, BUF_LEN);
    if (ret <= 0) {
        fprintf(stderr, "there was a problem with read, ERROR : %s\n", strerror(errno));
        exit(1);
    }
    if (write(1, buf, ret) != ret) {
        fprintf(stderr, "there was a problem with write, ERROR : %s\n", strerror(errno));
        exit(1);
    }
    close(fd);
    exit(0);
}