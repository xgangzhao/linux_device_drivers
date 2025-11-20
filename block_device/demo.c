#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#define DRV_NAME "vblk"

struct vblk_dev {
    struct device *dev;
    struct request_queue *queue;
    struct gendisk *disk;
    void *data;                     /* 虚拟磁盘内存 */
    int disk_size;                  /* 磁盘大小（字节） */
};

/* 处理单个请求 */
static blk_status_t vblk_queue_rq(struct blk_mq_hw_ctx *hctx,
                                  const struct blk_mq_queue_data *bd)
{
    struct request *req = bd->rq;
    struct vblk_dev *vblk = req->rq_disk->private_data;
    struct bio_vec bvec;
    struct req_iterator iter;
    loff_t pos = blk_rq_pos(req) << SECTOR_SHIFT;
    u32 *buf;
    blk_status_t ret = BLK_STS_OK;

    blk_mq_start_request(req);

    /* 简单检查越界 */
    if (pos + blk_rq_bytes(req) > vblk->disk_size) {
        ret = BLK_STS_IOERR;
        goto done;
    }

    /* 读写数据 */
    rq_for_each_segment(bvec, req, iter) {
        buf = kmap_atomic(bvec.bv_page);
        if (rq_data_dir(req) == WRITE) {
            memcpy(vblk->data + pos, buf + bvec.bv_offset, bvec.bv_len);
        } else {
            memcpy(buf + bvec.bv_offset, vblk->data + pos, bvec.bv_len);
        }
        kunmap_atomic(buf);
        pos += bvec.bv_len;
    }

done:
    blk_mq_end_request(req, ret);
    return BLK_STS_OK;
}

/* 块设备操作 */
static const struct block_device_operations vblk_fops = {
    .owner = THIS_MODULE,
};

/* 多队列配置 */
static struct blk_mq_ops vblk_mq_ops = {
    .queue_rq = vblk_queue_rq,
};

static const struct blk_mq_tag_set_cfg vblk_mq_tag_set_cfg = {
    .ops = &vblk_mq_ops,
    .nr_hw_queues = 1,
    .queue_depth = 128,
    .numa_node = NUMA_NO_NODE,
};

/* probe：初始化磁盘 */
static int vblk_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct vblk_dev *vblk;
    struct blk_mq_tag_set *set;
    int ret;

    vblk = devm_kzalloc(dev, sizeof(*vblk), GFP_KERNEL);
    if (!vblk)
        return -ENOMEM;
    vblk->dev = dev;

    /* 从设备树读磁盘大小 */
    if (of_property_read_u32(dev->of_node, "disk-size", &vblk->disk_size))
        vblk->disk_size = 8 * 1024 * 1024; /* 默认 8MB */

    /* 分配虚拟磁盘内存 */
    vblk->data = devm_vmalloc(dev, vblk->disk_size);
    if (!vblk->data)
        return -ENOMEM;

    /* 创建多队列 tag_set */
    set = devm_blk_mq_alloc_tag_set(dev, &vblk_mq_tag_set_cfg);
    if (IS_ERR(set))
        return PTR_ERR(set);

    /* 创建请求队列 */
    vblk->queue = blk_mq_init_queue(set, NULL, NULL);
    if (IS_ERR(vblk->queue))
        return PTR_ERR(vblk->queue);
    vblk->queue->queuedata = vblk;

    /* 创建 gendisk */
    vblk->disk = devm_alloc_disk(dev, 0);
    if (IS_ERR(vblk->disk))
        return PTR_ERR(vblk->disk);

    vblk->disk->major = 0;                     /* 动态申请主设备号 */
    vblk->disk->first_minor = 0;
    vblk->disk->fops = &vblk_fops;
    vblk->disk->private_data = vblk;
    vblk->disk->queue = vblk->queue;
    snprintf(vblk->disk->disk_name, DISK_NAME_LEN, "vblk%d", pdev->id);

    /* 设置容量（扇区数） */
    set_capacity(vblk->disk, vblk->disk_size >> SECTOR_SHIFT);

    /* 添加到内核 */
    ret = device_add_disk(dev, vblk->disk);
    if (ret)
        return ret;

    platform_set_drvdata(pdev, vblk);
    dev_info(dev, "virtual block device %s: %d MB\n",
             vblk->disk->disk_name, vblk->disk_size >> 20);
    return 0;
}

static int vblk_remove(struct platform_device *pdev)
{
    struct vblk_dev *vblk = platform_get_drvdata(pdev);
    del_gendisk(vblk->disk);        /* 删除 gendisk */
    blk_cleanup_queue(vblk->queue); /* 清理队列 */
    return 0;
}

static const struct of_device_id vblk_of_match[] = {
    { .compatible = "demo,vblk" },
    { }
};
MODULE_DEVICE_TABLE(of, vblk_of_match);

static struct platform_driver vblk_driver = {
    .probe  = vblk_probe,
    .remove = vblk_remove,
    .driver = {
        .name           = DRV_NAME,
        .of_match_table = vblk_of_match,
    },
};
module_platform_driver(vblk_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Platform-based virtual block device driver");
