#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/blkdev.h>
#include <linux/vmalloc.h>
#include <linux/blk-mq.h>

struct sbull_dev {
    int size; /* Device size in sectors */
    u8 *data; /* The data array */
    short users; /* How many users */
    short media_change; /* Flag a media change? */
    spinlock_t lock; /* For mutual exclusion */
    struct blk_mq_tag_set tag_set;
    struct request_queue *queue; /* The device request queue */
    struct gendisk *gd; /* The gendisk structure */
    struct timer_list timer; /* For simulated media changes */
};

enum {
	RM_SIMPLE  = 0,	/* The extra-simple request function */
	RM_FULL    = 1,	/* The full-blown version */
	RM_NOQUEUE = 2,	/* Use make_request */
};

#define INVALIDATE_DELAY	(30 * HZ)
#define MODULE_NAME            "sbull"
#define SBULL_MAX_DEVICE       2
#define SBULL_MAX_PARTITIONS   4
#define SBULL_SECTOR_SIZE      512
#define SBULL_SECTORS          16
#define SBULL_HEADS            4
#define SBULL_CYLINDERS        256
#define SBULL_SECTOR_TOTAL (SBULL_SECTORS * SBULL_HEADS * SBULL_CYLINDERS)
#define SBULL_SIZE          (SBULL_SECTOR_SIZE*SBULL_SECTOR_TOTAL)//8MB