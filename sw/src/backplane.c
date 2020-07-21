#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#define DEVICE_NAME "backplane"

MODULE_AUTHOR("Nickolas Upole");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.0");

static int  backplane_init(void);
static void backplane_exit(void);

static int     backplane_open(struct inode* i, struct file* f);
static int     backplane_release(struct inode* i, struct file* f);
static ssize_t backplane_read(struct file* f, char* buf, size_t len, loff_t* off);
static ssize_t backplane_write(struct file* f, const char* buf, size_t len, loff_t* off);

#define SUCCESS  0

static int majorNum;

static DEFINE_MUTEX(backplaneMutex);

static struct file_operations fops = {.read    = backplane_read,
                                      .write   = backplane_write,
                                      .open    = backplane_open,
                                      .release = backplane_release};

static struct backplane_instructions
{
    unsigned char opcode;
    unsigned char addr;
    unsigned char data;
} bp_inst;

#define REQ 17
#define ACK 18
#define WNR 27

#define ADDR7 22
#define ADDR6 10
#define ADDR5  9
#define ADDR4 11
#define ADDR3 23
#define ADDR2 24
#define ADDR1 25
#define ADDR0  8

#define DATA7 12
#define DATA6 16
#define DATA5 20
#define DATA4 21
#define DATA3  6
#define DATA2 13
#define DATA1 19
#define DATA0 26

static char* data;

static int backplane_init(void)
{
    printk(KERN_INFO "backplane: Initializing the backplane Linux Kernel Module\n");

    majorNum = register_chrdev(0, DEVICE_NAME, &fops);
    if(majorNum < 0)
    {
        printk(KERN_ALERT "backplane: Failed to register the backplane major number\n");
    }
    printk(KERN_INFO "backplane: Registered the backplane with major number %d\n", majorNum);

    mutex_init(&backplaneMutex);

    gpio_request(REQ, "req");
    gpio_direction_output(REQ, 0);

    gpio_request(ACK, "ack");
    gpio_direction_input(ACK);

    gpio_request(WNR, "wnr");

    gpio_request(ADDR7, "addr7");
    gpio_request(ADDR6, "addr6");
    gpio_request(ADDR5, "addr5");
    gpio_request(ADDR4, "addr4");
    gpio_request(ADDR3, "addr3");
    gpio_request(ADDR2, "addr2");
    gpio_request(ADDR1, "addr1");
    gpio_request(ADDR0, "addr0");

    gpio_request(DATA7, "data7");
    gpio_request(DATA6, "data6");
    gpio_request(DATA5, "data5");
    gpio_request(DATA4, "data4");
    gpio_request(DATA3, "data3");
    gpio_request(DATA2, "data2");
    gpio_request(DATA1, "data1");
    gpio_request(DATA0, "data0");

    return SUCCESS;
}

static void backplane_exit(void)
{
    unregister_chrdev(majorNum, DEVICE_NAME);

    gpio_free(REQ);
    gpio_free(ACK);
    gpio_free(WNR);

    gpio_free(ADDR7);
    gpio_free(ADDR6);
    gpio_free(ADDR5);
    gpio_free(ADDR4);
    gpio_free(ADDR3);
    gpio_free(ADDR2);
    gpio_free(ADDR1);
    gpio_free(ADDR0);

    gpio_free(DATA7);
    gpio_free(DATA6);
    gpio_free(DATA5);
    gpio_free(DATA4);
    gpio_free(DATA3);
    gpio_free(DATA2);
    gpio_free(DATA1);
    gpio_free(DATA0);

    mutex_destroy(&backplaneMutex);
    printk(KERN_INFO "backplane: Leaving the backplane Linux Kernel Module with major number %d\n", majorNum);
}

static int backplane_open(struct inode* i, struct file* f)
{
    if(!mutex_trylock(&backplaneMutex))
    {
        printk(KERN_ALERT "backplane: Device currently used by another process\n");
        return -EBUSY;
    }

    printk(KERN_INFO "backplane: Device successfully opened\n");

    return SUCCESS;
}

static int backplane_release(struct inode* i, struct file* f)
{
    mutex_unlock(&backplaneMutex);
    printk(KERN_INFO "backplane: Device successfully closed\n");

    return SUCCESS;
}

static ssize_t backplane_read(struct file* f, char* buf, size_t len, loff_t* off)
{
    data = kmalloc(len, GFP_KERNEL);
    data[0] = bp_inst.data;
    if(!data)
    {
        printk(KERN_ALERT "backplane: Kernel did not allocate memory to data\n");
        return -ENOMEM;
    }

    if(copy_to_user(buf, data, len))
    {
        kfree(data);
        return -EFAULT;
    }

    kfree(data);
    return len;
}

static ssize_t backplane_write(struct file* f, const char* buf, size_t len, loff_t* off)
{
    int i = 0;

    data = kmalloc(len, GFP_KERNEL);
    if(!data)
    {
        printk(KERN_ALERT "backplane: Kernel did not allocate memory to data\n");
        return -ENOMEM;
    }

    if(copy_from_user(data, buf, len))
    {
        kfree(data);
        return -EFAULT;
    }

    while(i < (len - 1))
    {
        bp_inst.opcode = data[i++] & 0x01;
        gpio_direction_output(WNR, bp_inst.opcode);

        bp_inst.addr = data[i++];
        gpio_direction_output(ADDR7, (bp_inst.addr >> 7) & 0x01);
        gpio_direction_output(ADDR6, (bp_inst.addr >> 6) & 0x01);
        gpio_direction_output(ADDR5, (bp_inst.addr >> 5) & 0x01);
        gpio_direction_output(ADDR4, (bp_inst.addr >> 4) & 0x01);
        gpio_direction_output(ADDR3, (bp_inst.addr >> 3) & 0x01);
        gpio_direction_output(ADDR2, (bp_inst.addr >> 2) & 0x01);
        gpio_direction_output(ADDR1, (bp_inst.addr >> 1) & 0x01);
        gpio_direction_output(ADDR0,  bp_inst.addr       & 0x01);

        if(bp_inst.opcode && (i < len))
        {
            bp_inst.data = data[i++];
            gpio_direction_output(DATA7, (bp_inst.data >> 7) & 0x01);
            gpio_direction_output(DATA6, (bp_inst.data >> 6) & 0x01);
            gpio_direction_output(DATA5, (bp_inst.data >> 5) & 0x01);
            gpio_direction_output(DATA4, (bp_inst.data >> 4) & 0x01);
            gpio_direction_output(DATA3, (bp_inst.data >> 3) & 0x01);
            gpio_direction_output(DATA2, (bp_inst.data >> 2) & 0x01);
            gpio_direction_output(DATA1, (bp_inst.data >> 1) & 0x01);
            gpio_direction_output(DATA0,  bp_inst.data       & 0x01);

            printk(KERN_INFO "backplane: Beginning a write cycle");
            printk(KERN_INFO "backplane: Writing to address 0x%02x", bp_inst.addr);
            printk(KERN_INFO "backplane: Writing value 0x%02x", bp_inst.data);

            gpio_direction_output(REQ, 1);

            while(!gpio_get_value(ACK))
            {
                while(!gpio_get_value(ACK))
                {
                    ndelay(10);
                }
            }

            gpio_direction_output(REQ, 0);

            while(gpio_get_value(ACK))
            {
                while(gpio_get_value(ACK))
                {
                    ndelay(10);
                }
            }

            printk(KERN_INFO "backplane: Finished write cycle");
        }

        else if(!bp_inst.opcode)
        {
            gpio_direction_input(DATA7);
            gpio_direction_input(DATA6);
            gpio_direction_input(DATA5);
            gpio_direction_input(DATA4);
            gpio_direction_input(DATA3);
            gpio_direction_input(DATA2);
            gpio_direction_input(DATA1);
            gpio_direction_input(DATA0);

            printk(KERN_INFO "backplane: Beginning a read cycle");
            printk(KERN_INFO "backplane: Reading from address 0x%02x", bp_inst.addr);

            gpio_direction_output(REQ, 1);

            while(!gpio_get_value(ACK))
            {
                while(!gpio_get_value(ACK))
                {
                    ndelay(10);
                }
            }

            bp_inst.data = 0x00;
            bp_inst.data += gpio_get_value(DATA7);
            bp_inst.data <<= 1;
            bp_inst.data += gpio_get_value(DATA6);
            bp_inst.data <<= 1;
            bp_inst.data += gpio_get_value(DATA5);
            bp_inst.data <<= 1;
            bp_inst.data += gpio_get_value(DATA4);
            bp_inst.data <<= 1;
            bp_inst.data += gpio_get_value(DATA3);
            bp_inst.data <<= 1;
            bp_inst.data += gpio_get_value(DATA2);
            bp_inst.data <<= 1;
            bp_inst.data += gpio_get_value(DATA1);
            bp_inst.data <<= 1;
            bp_inst.data += gpio_get_value(DATA0);

            gpio_direction_output(REQ, 0);

            while(gpio_get_value(ACK))
            {
                while(gpio_get_value(ACK))
                {
                    ndelay(10);
                }
            }

            printk(KERN_INFO "backplane: Value read is 0x%02x", bp_inst.data);
            printk(KERN_INFO "backplane: Finished read cycle");
        }
    }

    kfree(data);
    return len;
}

module_init(backplane_init);
module_exit(backplane_exit);
