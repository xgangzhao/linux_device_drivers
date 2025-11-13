#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define GLOBALFIFO_SIZE 0x1000
// #define MEM_CLEAR 0x1
#define GLOBALFIFO_MAJOR 230
#define DEVICE_NUM      3

#define GLOBALFIFO_MAGIC 'g'
#define MEM_CLEAR _IO(GLOBALFIFO_MAGIC,0)

static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO);

struct globalfifo_dev {
    struct cdev cdev;
    unsigned int current_len;
    unsigned char mem[GLOBALFIFO_SIZE];
    struct mutex mutex;
    wait_queue_head_t r_wait;
    wait_queue_head_t w_wait;
};

struct globalfifo_dev* globalfifo_devp;
