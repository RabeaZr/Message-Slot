#ifndef MESSAGE_SLOT_H
#define MESSAGE_SLOT_H
#include <linux/ioctl.h>

// We don't rely on dynamic registration anymore
// We need ioctls to know that number at the time of compilation

#define MAJOR_NUM 240
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)
#define DEVICE_RANGE_NAME "message_slot"
#define BUF_LEN 128
#define SUCCESS 0

typedef struct channel {
    unsigned int channel_number;
    char message[128];
    int len;
    struct channel* next;
} channel;

// struct representing a message slot.
typedef struct device {
    unsigned int minor;
    channel* channels;
    struct device* next;
} device;


#endif
