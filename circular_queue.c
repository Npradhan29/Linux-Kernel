#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define DEVICE_NAME "vicharak"
#define SET_SIZE_OF_QUEUE _IOW('a', 'a', int)
#define PUSH_DATA _IOW('a', 'b', struct data)
#define POP_DATA _IOR('a', 'c', struct data)

struct data {
    int length;
    char *data;
};

struct circular_queue {
    char *buffer;
    int size;
    int head;
    int tail;
    int count;
    struct mutex lock;
    wait_queue_head_t wq;
};

static struct circular_queue queue;
static int device_open = 0;

static int device_open(struct inode *inode, struct file *file) {
    if (device_open)
        return -EBUSY;
    device_open++;
    try_module_get(THIS_MODULE);
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    device_open--;
    module_put(THIS_MODULE);
    return 0;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct data user_data;
    int ret = 0;

    switch (cmd) {
        case SET_SIZE_OF_QUEUE:
            mutex_lock(&queue.lock);
            if (queue.buffer)
                kfree(queue.buffer);
            queue.size = arg;
            queue.buffer = kmalloc(queue.size, GFP_KERNEL);
            if (!queue.buffer) {
                mutex_unlock(&queue.lock);
                return -ENOMEM;
            }
            queue.head = queue.tail = queue.count = 0;
            mutex_unlock(&queue.lock);
            break;

        case PUSH_DATA:
            if (copy_from_user(&user_data, (struct data *)arg, sizeof(user_data)))
                return -EFAULT;

            mutex_lock(&queue.lock);
            while (queue.count == queue.size) {
                mutex_unlock(&queue.lock);
                if (file->f_flags & O_NONBLOCK)
                    return -EAGAIN;
                if (wait_event_interruptible(queue.wq, queue.count < queue.size))
                    return -ERESTARTSYS;
                mutex_lock(&queue.lock);
            }

            if (user_data.length > queue.size - queue.count)
                user_data.length = queue.size - queue.count;

            if (queue.tail + user_data.length > queue.size) {
                int first_part = queue.size - queue.tail;
                memcpy(queue.buffer + queue.tail, user_data.data, first_part);
                memcpy(queue.buffer, user_data.data + first_part, user_data.length - first_part);
            } else {
                memcpy(queue.buffer + queue.tail, user_data.data, user_data.length);
            }

            queue.tail = (queue.tail + user_data.length) % queue.size;
            queue.count += user_data.length;
            mutex_unlock(&queue.lock);
            wake_up_interruptible(&queue.wq);
            break;

        case POP_DATA:
            if (copy_from_user(&user_data, (struct data *)arg, sizeof(user_data)))
                return -EFAULT;

            mutex_lock(&queue.lock);
            while (queue.count == 0) {
                mutex_unlock(&queue.lock);
                if (file->f_flags & O_NONBLOCK)
                    return -EAGAIN;
                if (wait_event_interruptible(queue.wq, queue.count > 0))
                    return -ERESTARTSYS;
                mutex_lock(&queue.lock);
            }

            if (user_data.length > queue.count)
                user_data.length = queue.count;

            if (queue.head + user_data.length > queue.size) {
                int first_part = queue.size - queue.head;
                memcpy(user_data.data, queue.buffer + queue.head, first_part);
                memcpy(user_data.data + first_part, queue.buffer, user_data.length - first_part);
            } else {
                memcpy(user_data.data, queue.buffer + queue.head, user_data.length);
            }

            queue.head = (queue.head + user_data.length) % queue.size;
            queue.count -= user_data.length;
            mutex_unlock(&queue.lock);
            wake_up_interruptible(&queue.wq);
            break;

        default:
            return -EINVAL;
    }

    return ret;
}

static struct file_operations fops = {
    .unlocked_ioctl = device_ioctl,
    .open = device_open,
    .release = device_release,
};

static int __init circular_queue_init(void) {
    int ret;
    ret = register_chrdev(0, DEVICE_NAME, &fops);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register device\n");
        return ret;
    }
    printk(KERN_INFO "Device registered with major number %d\n", ret);

    mutex_init(&queue.lock);
    init_waitqueue_head(&queue.wq);
    queue.buffer = NULL;
    queue.size = 0;
    queue.head = queue.tail = queue.count = 0;

    return 0;
}

static void __exit circular_queue_exit(void) {
    unregister_chrdev(0, DEVICE_NAME);
    if (queue.buffer)
        kfree(queue.buffer);
    printk(KERN_INFO "Device unregistered\n");
}

module_init(circular_queue_init);
module_exit(circular_queue_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Dynamic Circular Queue Char Device");