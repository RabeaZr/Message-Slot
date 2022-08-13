/*
the idea is to make a linked list that contains every device that is charictarized by it's minor number
and for every device we save all the channels of this device
*/
#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE
#include <linux/kernel.h>   /* We're doing kernel work */
#include <linux/module.h>   /* Specifically, a module */
#include <linux/fs.h>       /* for register_chrdev */
#include <linux/uaccess.h>  /* for get_user and put_user */
#include <linux/string.h>   /* for memset. NOTE - not string.h!*/
#include <linux/list.h>
#include <linux/types.h>
#include <linux/slab.h>
MODULE_LICENSE("GPL");
#include "message_slot.h"

struct chardev_info
{
    spinlock_t lock;
};

static struct chardev_info device_info;

// The channels of all the devices
static device * devices=NULL;
//device struct functionality
device * build_device(unsigned int minor)
{
    //device struct allocation
    device *res = (device*)kmalloc(sizeof(device),GFP_KERNEL);
    if(res == NULL)
    {
        return NULL;
    }
    res->minor=minor;
    res->channels=NULL;
    res->next=NULL;
    return res;
}
//get the device from the linked list by it's number
// if the device is not exisit then return NONE
device* get_device(unsigned int minor)
{
    device* res = devices;
    while(res!= NULL && res->minor!= minor)
    {
        res=res->next;
    }
    return res;
}
//add device to the end of devices list
//and return pointer to it if the device is already exisit then return it
device* add_get_device(unsigned int minor)
{
    device* res;
    device* exis_dev;
    device* dev_end;
    exis_dev = get_device(minor);
    if(exis_dev!=NULL)
    {
        return exis_dev;
    }
    //in this case the device does not exist so we need to build it
    res=build_device(minor);
    if(res==NULL)
    {//that means we can't create the device
        return NULL;
    }
    dev_end=devices;
    if(devices==NULL)
    {
        devices=res;
    }
    else
    {
        while(dev_end->next!=NULL)
        {
            dev_end=dev_end->next;
        }
        dev_end->next=res;
    }
    return res;
}
// The functionality of the channel
channel* build_channel(unsigned int channel_number)
{
    channel* res=(channel*)kmalloc(sizeof(channel),GFP_KERNEL);
    if(res==NULL)
    {
        return NULL;
    }
    res->next=NULL;
    res->len=0;
    res->channel_number=channel_number;
    return res;
}

channel* get_channel_from_device(unsigned int minor,unsigned int channel_number)
{
    device* dev;
    channel* res;
    dev=get_device(minor);
    if(dev==NULL)
    {
        return NULL;
    }
    res=dev->channels;
    while(res !=NULL && res->channel_number!=channel_number)
    {
        res=res->next;
    }
    return res;
}
//add new channel to the end of the channels list of some device
//if the device minor is not exisit, then create it
channel* add_get_channel(int device_minor,int channel_number)
{
    channel* exisit_channel;
    channel* new_channel;
    channel* last_channel;
    device* dev;
    exisit_channel=get_channel_from_device(device_minor,channel_number);
    if(exisit_channel !=NULL)
    {
        return exisit_channel;
    }
    dev=get_device(device_minor);
    new_channel=build_channel(channel_number);
    if (dev==NULL)
    {
        return NULL;
    }
    if (new_channel==NULL)
    {
        return NULL;
    }
    last_channel=dev->channels;
    if(last_channel==NULL)
    {// the device has zero channels
        dev->channels=new_channel;
        return new_channel;
    }
    while(last_channel->next!=NULL)
    {
        last_channel=last_channel->next;
    }
    last_channel->next=new_channel;
    return new_channel;
}
//given some device free it's channels
void free_channels(device *device)
{
    channel* chan,*next_chan;
    chan = device->channels;
    while(chan != NULL)
    {
        next_chan = chan->next;
        kfree(chan);
        chan=next_chan;
    }
}
void free_devices(void)
{
    device* dev,*next_dev;
    dev=devices;
    while(dev!=NULL)
    {
        next_dev = dev->next;
        free_channels(dev);
        kfree(dev);
        dev = dev->next;
    }
}
//================== DEVICE FUNCTIONS ===========================
static int device_open(struct inode* inode, struct file* file)
{
    unsigned int minor = iminor(inode);
    device* dev = add_get_device(minor);
    if(dev==NULL)
    {
        printk("error happened while creating the new device\n");
        return -ENOMEM;
    }
    if(inode==NULL)
    {
        printk("file is NULL");
        printk("INODE is NULL");
        return -EINVAL;
    }
    return SUCCESS;
}

//---------------------------------------------------------------
// a process which has already opened
// the device file attempts to read from a process that has already been opened
static ssize_t device_read(struct file* file,char __user* buffer, size_t length, loff_t* offset)
{
    int i;
    unsigned int minor,channel_number;
    channel* channel;
    if(file==NULL)
    {
        printk("file is NULL");
        return -EINVAL;
    }
    if(file->f_inode==NULL)
    {
        printk("INODE is NULL");
        return -EINVAL;
    }
    minor=iminor(file->f_inode);
    channel_number=(unsigned long)file->private_data;
    if (channel_number==0)
    {
        printk("Error in the number of the channel");
        return -EINVAL;
    }
    channel=get_channel_from_device(minor,channel_number);
    if(channel == NULL)
    {
        printk("channel does not exist");
        return -EINVAL;
    }
    if(!channel->len)
    {
        printk("empty message");
        return -EWOULDBLOCK;
    }
    if(channel->len>length)
    {
        printk("message is bigger that the limit\n");
        return -ENOSPC;
    }
    for(i=0;i<length && i<(channel->len);i++)
    {
        if(put_user(channel->message[i],&buffer[i]) != 0)
        {
            return -EFAULT;
        }
    }
    return i;
}

//---------------------------------------------------------------
// the device file attempts to write to a process that has already ben opened
static ssize_t device_write( struct file* file, const char __user* buffer, size_t length, loff_t* offset)
{
    channel *channel;
    unsigned int minor;
    unsigned int channel_number;
    int i;
    if(length<1)
    {
        return -EMSGSIZE;
    }
    if(length > 128)
    {
        return -EMSGSIZE
    }
    if(file==NULL)
    {
        printk("file is NULL");
        return -EINVAL;
    }
    if(file->f_inode==NULL)
    {
        printk("INODE is NULL");
        return -EINVAL;
    }
    minor=iminor(file->f_inode);
    channel_number=(unsigned long)file->private_data;
    if (channel_number < 0)
    {
        printk("negative channel number");
        return -EINVAL;
    }
    channel=get_channel_from_device(minor,channel_number);
    if(channel==NULL)
    {
        printk("the channel does not exist\n");
        return -EINVAL;
    }

    for(i=0;i<length;i++)
    {
        if(get_user(channel->message[i],&buffer[i]) !=0)
            return -EFAULT;
    }
    channel->len=length;
    return i;
}

//----------------------------------------------------------------
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param)
{
    unsigned int minor;
    channel* res;
    if(ioctl_param == 0)
    {
        printk("the channel is invalid");
        return -EINVAL;
    }
    if(file==NULL || file->f_inode==NULL ||ioctl_command_id!= MSG_SLOT_CHANNEL)
    // Switch according to the ioctl called
    {
        printk("the parameters are invalid");
        return -EINVAL;
    }
    minor=iminor(file->f_inode);
    res=add_get_channel(minor,(int) ioctl_param);
    file->private_data = (void*) ioctl_param;
    printk("the channel id in octl is : %ld\n",ioctl_param);
    printk("finish octl\n");
    return res!=NULL?SUCCESS:-ENOMEM;
}
//==================== DEVICE SETUP =============================
// This structure will hold the functions to be called
// when a process does something to the device we created
struct file_operations Fops =
        {
                .owner	  = THIS_MODULE,
                .read           = device_read,
                .write          = device_write,
                .open           = device_open,
                .unlocked_ioctl = device_ioctl,
        };

//---------------------------------------------------------------
// Initialize the module - Register the character device
static int __init simple_init(void)
{
    int rc = -1;
    memset(&device_info, 0, sizeof(struct chardev_info));
    spin_lock_init(&device_info.lock);
    rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops );
    if(rc < 0)
    {
        printk(KERN_ALERT "registraion failed for %d\n", MAJOR_NUM);
        return rc;
    }
    return 0;
}
//---------------------------------------------------------------
static void __exit simple_cleanup(void)
{
    // Unregister the device
    // Should always succeed
    free_devices();
    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}
//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================
