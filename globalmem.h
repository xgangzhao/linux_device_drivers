#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define GLOBALMEM_SIZE 0x1000
// #define MEM_CLEAR 0x1
#define GLOBALMEM_MAJOR 230

#define GLOBALMEM_MAGIC 'g'
#define MEM_CLEAR _IO(GLOBALMEM_MAGIC,0)

static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);

struct globalmem_dev {
    struct cdev cdev;  // char device struct
    unsigned char mem[GLOBALMEM_SIZE];
};

struct globalmem_dev* globalmem_devp;
