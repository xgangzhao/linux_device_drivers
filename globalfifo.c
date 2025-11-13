#include "globalfifo.h"

static int globalfifo_open(struct inode *inode, struct file *filp) {
    struct globalfifo_dev* dev = container_of(inode->i_cdev, struct globalfifo_dev, cdev);
    filp->private_data = dev;
    return 0;
}

static int globalfifo_release(struct inode *inode, struct file *filp) {
    return 0;
}

static ssize_t globalfifo_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos) {
    // unsigned long p = *ppos;
    // unsigned int count = size;
    int ret = 0;
    // get device from file struct
    struct globalfifo_dev *dev = filp->private_data;
    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->r_wait, &wait);

    while (dev->current_len == 0) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }
        __set_current_state(TASK_INTERRUPTIBLE);
        mutex_unlock(&dev->mutex);
        schedule();  // sleep, release mutex before that
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            goto out2;
        }
        
        mutex_lock(&dev->mutex);
    }
    
    // get count of readable
    if (count > dev->current_len)
        count = dev->current_len;

    // copy data from device to user space
    if (copy_to_user(buf, dev->mem, count)) {
        ret = -EFAULT;
        goto out;
    } else {
        // move the rest
        memcpy(dev->mem, dev->mem+count, dev->current_len - count);
        dev->current_len -= count;
        printk(KERN_INFO "read %zu bytes(s), current_len: %u\n", count, dev->current_len);
        wake_up_interruptible(&dev->w_wait);
        ret = count;
    }

    out:
    mutex_unlock(&dev->mutex);

    out2:
    remove_wait_queue(&dev->w_wait, &wait);
    set_current_state(TASK_RUNNING);

    return ret;
}

static ssize_t globalfifo_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos) {
    int ret = 0;
    // get device from file struct
    struct globalfifo_dev *dev = filp->private_data;
    DECLARE_WAITQUEUE(wait, current);

    mutex_lock(&dev->mutex);
    add_wait_queue(&dev->w_wait, &wait);

    while (dev->current_len == GLOBALFIFO_SIZE) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }
        __set_current_state(TASK_INTERRUPTIBLE);
        mutex_unlock(&dev->mutex);
        schedule();
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            goto out2;
        }
        
        mutex_lock(&dev->mutex);
    }
    
    // get count of readable
    if (count > GLOBALFIFO_SIZE - dev->current_len)
        count = GLOBALFIFO_SIZE - dev->current_len;

    if (copy_from_user(dev->mem+dev->current_len, buf, count)) {
        ret = -EFAULT;
        goto out;
    } else {
        dev->current_len += count;
        printk(KERN_INFO "written %zu bytes(s), current_len: %u\n", count, dev->current_len);
        wake_up_interruptible(&dev->r_wait);
        ret = count;
    }

    out:
    mutex_unlock(&dev->mutex);

    out2:
    remove_wait_queue(&dev->w_wait, &wait);
    set_current_state(TASK_RUNNING);

    return ret;
}

static loff_t globalfifo_llseek(struct file *filp, loff_t offset, int orig) {
    loff_t ret = 0;
    switch (orig) {
    case 0: /* 从文件开头位置seek */
        if (offset< 0) {
            ret = -EINVAL;
            break;
        }
        if ((unsigned int)offset > GLOBALFIFO_SIZE) {
            ret = -EINVAL;
            break;
        }
        filp->f_pos = (unsigned int)offset;
        ret = filp->f_pos;
        break;
    case 1: /* 从文件当前位置开始seek */
        if ((filp->f_pos + offset) > GLOBALFIFO_SIZE) {
            ret = -EINVAL;
            break;
        }
        if ((filp->f_pos + offset) < 0) {
            ret = -EINVAL;
            break;
        }
        filp->f_pos += offset;
        ret = filp->f_pos;
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret;
}

static long globalfifo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    struct globalfifo_dev *dev = filp->private_data;

    switch (cmd) {
    case MEM_CLEAR:
        mutex_lock(&dev->mutex);
        memset(dev->mem, 0, GLOBALFIFO_SIZE);
        mutex_unlock(&dev->mutex);
        printk(KERN_INFO "globalfifo is set to zero\n");
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static unsigned int globalfifo_poll(struct file *filp, poll_table * wait)
{
    unsigned int mask = 0;
    struct globalfifo_dev *dev = filp->private_data;

    mutex_lock(&dev->mutex);;

    poll_wait(filp, &dev->r_wait, wait);
    poll_wait(filp, &dev->w_wait, wait);

    if (dev->current_len != 0) {
        mask |= POLLIN | POLLRDNORM;
    }

    if (dev->current_len != GLOBALFIFO_SIZE) {
        mask |= POLLOUT | POLLWRNORM;
    }

    mutex_unlock(&dev->mutex);;
    return mask;
}


static const struct file_operations globalfifo_fops = {
    .owner = THIS_MODULE,
    .llseek = globalfifo_llseek,
    .read = globalfifo_read,
    .write = globalfifo_write,
    .unlocked_ioctl = globalfifo_ioctl,
    .open = globalfifo_open,
    .release = globalfifo_release,
    .poll = globalfifo_poll,
};

static void globalfifo_setup_cdev(struct globalfifo_dev* dev, int index) {
    int err, devno = MKDEV(globalfifo_major, index);

    // init cdev, connect file_operation to cdev
    cdev_init(&dev->cdev, &globalfifo_fops);
    dev->cdev.owner = THIS_MODULE;
    // rigister a cdev to system
    err = cdev_add(&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_NOTICE "Error %d adding globalfifo%d", err, index);
    }
}

static int __init globalfifo_init(void) {
    int ret = 0;
    int i = 0;
    dev_t devno = MKDEV(globalfifo_major, 0);

    // request a devno
    if (globalfifo_major) {
        ret = register_chrdev_region(devno, DEVICE_NUM, "globalfifo");
    } else {
        ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalfifo");
        globalfifo_major = MAJOR(devno);
    }

    if (ret < 0) return ret;

    globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev) * DEVICE_NUM, GFP_KERNEL);
    if (!globalfifo_devp) {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    // init mutex
    mutex_init(&globalfifo_devp->mutex);
    for (i = 0; i < DEVICE_NUM; ++i) {
        globalfifo_setup_cdev(globalfifo_devp + i, i);
        init_waitqueue_head(&((globalfifo_devp+i)->r_wait));
        init_waitqueue_head(&((globalfifo_devp+i)->w_wait));
    }
    
    return 0;

fail_malloc:
    unregister_chrdev_region(devno, DEVICE_NUM);
    return ret;
}

static void __exit globalfifo_exit(void) {
    int i = 0; 
    for (; i < DEVICE_NUM; ++i)
        cdev_del(&(globalfifo_devp + i)->cdev);  // unrigister cdev obj
    kfree(globalfifo_devp);
    // release dev number
    unregister_chrdev_region(MKDEV(globalfifo_major, 0), DEVICE_NUM);
}

module_init(globalfifo_init);
module_exit(globalfifo_exit);

MODULE_AUTHOR("Xigang Zhao");
MODULE_LICENSE("GPL v2");
