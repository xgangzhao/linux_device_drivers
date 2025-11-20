#include "sbull.h"
#include "sbull_utils.h"

static struct sbull_dev* sbdev[SBULL_MAX_DEVICE];
static int sbull_major = 0;
static int request_mode = RM_SIMPLE;
module_param(request_mode, int, 0);

static void block_release(struct gendisk* gd, fmode_t mode) {
    struct sbull_dev* sd = gd->private_data;

    spin_lock(&sd->lock);
    sd->users--;
    if (!sd->users) {
        sd->timer.expires = jiffies + INVALIDATE_DELAY;
        add_timer(&sd->timer);
    }
    spin_unlock(&sd->lock);
}

static int block_media_changed(struct gendisk* gd) {
    struct sbull_dev* sd = gd->private_data;

    return sd->media_change ? 1 : 0;
}

static int block_revalidate(struct gendisk* gd) {
    struct sbull_dev* sd = gd->private_data;

    if (sd->media_change) {
        sd->media_change = false;
        memset(sd->data, 0, sd->size);
    }

    return 0;
}

static int block_open(struct block_device* bdev, fmode_t mode) {
    struct sbull_dev* sd = bdev->bd_disk->private_data;

    del_timer_sync(&sd->timer);
    spin_lock(&sd->lock);
    if (!sd->users) {
        bdev_check_media_change(bdev);
    }
    sd->users++;
    spin_unlock(&sd->lock);

    return 0;
}

static struct block_device_operations block_ops = {
    .owner = THIS_MODULE,
    .open = block_open,
    .release = block_release,
    .media_changed = block_media_changed,
    .revalidate_disk = block_revalidate,
};

static int setup_sbull(struct sbull_dev* sd, int idx) {
    int rv = 0;
    sd->size = SBULL_SIZE;
    sd->users = 0;
    sd->media_change = false;
    sd->data = vmalloc(sd->size);
    if (!sd->data) {
        pr_err("Allocate space failed for sbull\n");
        return -ENOMEM;
    }
    spin_lock_init(&sd->lock);
    timer_setup(&sd->timer, timeout_cb, 0);

    pr_err("REQUEST_MODE = %d\n", request_mode);
    switch (request_mode) {
        case RM_NOQUEUE: 
            sd->queue = blk_alloc_queue(mk_request, NUMA_NO_NODE);
            if (!sd->queue) {
                rv = -ENOMEN;
                goto out_vfree;
            }
            break;
        case RM_FULL:
            sd->queue = blk_mq_init_sq_queue(&sd->tag_set, &mq_ops_full, 256, 
                                             BLK_MQ_F_SHOULD_MERGE);
            if (!sd->queue) {
                rv = -ENOMEN;
                goto out_vfree;
            }
            break;
        default:
            pr_notice("fallbackk to simple!\n");
        case RM_SIMPLE:
            sd->queue = blk_mq_init_sq_queue(&sd->tag_set, &mq_ops_simple, 256, 
                                             BLK_MQ_F_SHOULD_MERGE);
            if (!sd->queue) {
                rv = -ENOMEN;
                goto out_vfree;
            }
            break;
    }

    blk_queue_logical_block_size(sd->queue, SBULL_SECTOR_SIZE);
    sd->queue->queuedata = sd;

    sd->gd = alloc_disk(SBULL_MAX_PARTITIONS);
    if (!sd->gd) {
        pr_err("alloc disk failure\n");
        rv = -ENOMEN;
        goto out_vfree;
    }
    sd->gd->major = sbull_major;
    sd->gd->first_minor = idx * SBULL_MAX_PARTITIONS;
    sd->gd->fops = &block_ops;
    sd->gd->queue = sd->queue;
    sd->gd->private_data = sd;
    sprintf(sd->gd->disk_name, "sbull%c", 'a'+idx);
    set_capacity(sd->gd, SBULL_SECTOR_TOTAL);
    // activate this block dev. always last
    add_disk(sd->gd);

    return 0;

out_vfree:
    if (sd->data) {
        vfree(sd->data);
    }
    return rv;
}

static int __init sbull_init(void) {
    // build ur dev
    for (int i = 0; i < SBULL_MAX_DEVICE; ++i) {
        sbdev[i] = kzalloc(sizeof(struct sbull_dev), GFP_KERNEL);
        if (!sbdev[i]) {
            return -ENOMEM;
        }
    }
    // get a dev number
    sbull_major = register_blkdev(sbull_major, MODULE_NAME);
    if (sbull_major <= 0) {
        printk(KERN_WARNING "sbull: unable to get major number\n");
    }
    // init ur dev
    for (int i = 0; i < SBULL_MAX_DEVICE; ++i) {
        setup_sbull(sbdev[i], i);
    }

    return 0;
}

static void __exit sbull_exit(void) {
    for (int i = 0; i < SBULL_MAX_DEVICE; ++i) {
        if (!sbdev[i]) continue;

        del_gendisk(sbdev[i]->gd);
        put_disk(sbdev[i]->gd);
        if (request_mode == RM_NOQUEUE)
            blk_cleanup_queue(sbdev[i]->queue);
        if (sbdev[i]->data)
            vfree(sbdev[i]->data);
        kfree(sbdev[i]);
        sbdev[i] = NULL;
    }
    unregister_blkdev(sbull_major, MODULE_NAME);
}

module_init(sbull_init);
module_exit(sbull_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("xigang");
MODULE_DESCRIPTION("PCI Driver skel");
