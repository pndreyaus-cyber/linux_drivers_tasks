/*
 * mynetchar.c - Minimal Network Driver converted to Character Device
 *
 * CONCEPT: This driver takes a typical network driver's core logic (open, stop, start_xmit)
 * and re-exposes it through a CHARACTER DEVICE interface (/dev/mynetchar) instead of
 * registering as a net_device with the networking stack.
 *
 * NETWORK FUNCTIONS MAPPED TO CHAR DEVICE:
 *   - net_device->ndo_open()    → ioctl(MYNET_IOCTL_UP) or first .open()
 *   - net_device->ndo_stop()    → ioctl(MYNET_IOCTL_DOWN) or last .release()
 *   - net_device->ndo_start_xmit() → .write() system call
 *   
 * NO NETWORKING STACK INTEGRATION: This does NOT register a net_device, so it won't
 * show in 'ip link' or 'ifconfig'. It's purely a char device that REUSES your network
 * driver's internal logic for demonstration/learning purposes.
 *
 * USAGE: Userspace opens /dev/mynetchar, uses ioctl to control UP/DOWN, writes packets
 *        to trigger start_xmit(), reads back simulated RX data.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/ioctl.h>

#define MYNET_MAGIC 'N'
#define MYNET_IOCTL_UP     _IO(MYNET_MAGIC, 1)   /* Bring interface UP (like ifconfig up) */
#define MYNET_IOCTL_DOWN   _IO(MYNET_MAGIC, 2)   /* Bring interface DOWN (like ifconfig down) */
#define MYNET_IOCTL_STATS  _IOR(MYNET_MAGIC, 3, struct mynet_stats)  /* Get stats */

#define DEVICE_NAME "mynetchar"
#define MAX_PKTLEN  1500

/* Private data structure - similar to net_device->priv in real network drivers */
struct mynet_priv {
    struct cdev cdev;                    /* Character device registration */
    int up;                              /* Interface state (UP/DOWN) */
    int mtu;                             /* Maximum packet size */
    unsigned long tx_packets;            /* TX packet counter */
    unsigned long rx_packets;            /* RX packet counter (simulated) */
    char last_tx[MAX_PKTLEN];            /* Store last transmitted packet for read() */
};

static struct mynet_priv mynet_data = {
  .mtu = 1500,
};

/* Stats structure - passed to userspace via ioctl */
struct mynet_stats {
    int up;
    int mtu;
    unsigned long tx_packets;
    unsigned long rx_packets;
};

static dev_t dev_num;                    /* Major:minor device number */
static struct class *mynet_class;        /* Sysfs class for auto /dev node */

/*
 * net_open() - YOUR ORIGINAL NETWORK DRIVER'S ndo_open() equivalent
 * Called when interface goes UP. Initializes hardware/registers interrupts/etc.
 */
static int net_open(void *priv) {
    struct mynet_priv *p = priv;
    pr_info("MYNET: net_open() called - interface UP\n");
    p->up = 1;
    return 0;
}

/*
 * net_stop() - YOUR ORIGINAL NETWORK DRIVER'S ndo_stop() equivalent  
 * Called when interface goes DOWN. Stops hardware/releases resources.
 */
static int net_stop(void *priv) {
    struct mynet_priv *p = priv;
    pr_info("MYNET: net_stop() called - interface DOWN\n");
    p->up = 0;
    return 0;
}

/*
 * start_xmit() - YOUR ORIGINAL NETWORK DRIVER'S ndo_start_xmit() equivalent
 * Called by kernel networking stack (or here by .write()) to transmit a packet.
 * In real driver: DMA to hardware. Here: just log and store.
 */
static int start_xmit(const char *skb_data, size_t len, void *priv) {
    struct mynet_priv *p = priv;
    
    if (len >= MAX_PKTLEN) {
        pr_warn("MYNET: Packet too large: %zu > MTU %d\n", len, p->mtu);
        return 0;  /* Pretend success */
    }
    
    /* Simulate skb->data copy - in real driver this goes to TX ring/DMA */
    if (copy_from_user(p->last_tx, skb_data, len))
        return 0;
    p->last_tx[len] = 0;  /* Null terminate for printing */
    
    p->tx_packets++;      /* Update statistics */
    pr_info("MYNET: start_xmit() %zu bytes: %.20s...\n", len, p->last_tx);
    return 0;  /* Tell networking stack we're done */
}

/*
 * mynet_open() - Character device .open() callback
 * Maps to network driver's reference counting. Calls net_open() on FIRST open.
 */
static int mynet_open(struct inode *inode, struct file *file) {
    struct mynet_priv *priv = container_of(inode->i_cdev, struct mynet_priv, cdev);
    file->private_data = priv;  /* Pass priv to other fops */
    
    /* Reference counting: only call net_open() if not already up */
    if (!priv->up)
        net_open(priv);
    
    pr_info("MYNET: /dev/%s opened (users: %d)\n", DEVICE_NAME, 1);
    return 0;
}

/*
 * mynet_release() - Character device .release() callback  
 * Maps to network driver's reference counting. Calls net_stop() on LAST close.
 */
static int mynet_release(struct inode *inode, struct file *file) {
    struct mynet_priv *priv = file->private_data;
    
    /* Reference counting: only call net_stop() if this is last close */
    if (priv->up)
        net_stop(priv);
    
    pr_info("MYNET: /dev/%s closed\n", DEVICE_NAME);
    return 0;
}

/*
 * mynet_write() - Character device .write() callback
 * Userspace: echo "packet data" > /dev/mynetchar
 * This directly calls YOUR start_xmit() logic!
 */
static ssize_t mynet_write(struct file *file, const char __user *buf,
                          size_t count, loff_t *ppos) {
    struct mynet_priv *priv = file->private_data;
    
    if (!priv->up) {
        pr_warn("MYNET: Interface down - can't transmit\n");
        return -ENETDOWN;
    }
    
    /* REUSE YOUR ORIGINAL start_xmit() - just like kernel networking stack would! */
    start_xmit(buf, count, priv);
    return count;  /* Tell userspace we consumed all bytes */
}

/*
 * mynet_read() - Character device .read() callback (RX simulation)
 * Userspace: cat /dev/mynetchar
 * For demo: echoes back last TX packet (loopback simulation).
 */
static ssize_t mynet_read(struct file *file, char __user *buf,
                         size_t count, loff_t *ppos) {
    struct mynet_priv *priv = file->private_data;
    int len = strlen(priv->last_tx);
    
    if (len == 0) return 0;  /* Nothing to read */
    
    /* Simulate RX packet - copy last TX to userspace */
    if (copy_to_user(buf, priv->last_tx, len))
        return -EFAULT;
    
    priv->rx_packets++;  /* Update RX stats */
    return len;
}

/*
 * mynet_ioctl() - Character device .unlocked_ioctl() callback
 * Userspace control commands: UP/DOWN/GET_STATS (like ifconfig/ip link)
 */
static long mynet_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct mynet_priv *priv = file->private_data;
    struct mynet_stats stats;
    
    switch (cmd) {
    case MYNET_IOCTL_UP:
        pr_info("MYNET: ioctl UP\n");
        return net_open(priv);  /* Your original ndo_open() */
        
    case MYNET_IOCTL_DOWN:
        pr_info("MYNET: ioctl DOWN\n");
        return net_stop(priv);  /* Your original ndo_stop() */
        
    case MYNET_IOCTL_STATS:
        /* Copy stats to userspace */
        stats.up = priv->up;
        stats.mtu = priv->mtu;
        stats.tx_packets = priv->tx_packets;
        stats.rx_packets = priv->rx_packets;
        
        if (copy_to_user((struct mynet_stats __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;
        return 0;
        
    default:
        return -ENOTTY;  /* Unknown command */
    }
}

/* Character device file operations table */
static const struct file_operations mynet_fops = {
    .owner = THIS_MODULE,
    .open = mynet_open,           /* Maps to net_open() */
    .release = mynet_release,     /* Maps to net_stop() */
    .read = mynet_read,           /* Simulate RX */
    .write = mynet_write,         /* Calls start_xmit() */
    .unlocked_ioctl = mynet_ioctl, /* UP/DOWN/Stats control */
};

/*
 * Module init - Register character device /dev/mynetchar
 * Creates: /dev/mynetchar (major=XXX minor=0)
 */
static int __init mynet_init(void) {
    int ret;
    
    /* 1. Get unique device numbers (like major:minor) */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        pr_err("MYNET: Failed to alloc chrdev region\n");
        return ret;
    }
    
    /* 3. Setup character device */
    cdev_init(&mynet_data.cdev, &mynet_fops);
    mynet_data.cdev.owner = THIS_MODULE;
    mynet_data.mtu = 1500;  /* Default MTU */
    
    /* 4. Register cdev */
    ret = cdev_add(&mynet_data.cdev, dev_num, 1);
    //if (ret < 0)
   //     goto err_data;
    
    /* 5. Create sysfs class + device node (/dev/mynetchar) */
    mynet_class = class_create(DEVICE_NAME);
    if (IS_ERR(mynet_class)) {
        ret = PTR_ERR(mynet_class);
        goto err_cdev;
    }
    
    if (IS_ERR(device_create(mynet_class, NULL, dev_num, NULL, DEVICE_NAME))) {
        ret = -ENODEV;
        goto err_class;
    }
    
    pr_info("MYNET: /dev/%s loaded (major=%d)\n", DEVICE_NAME, MAJOR(dev_num));
    return 0;

err_class:
    class_destroy(mynet_class);
err_cdev:
    cdev_del(&mynet_data.cdev);
err_chrdev:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

/* Module exit - Cleanup everything */
static void __exit mynet_exit(void) {
    device_destroy(mynet_class, dev_num);
    class_destroy(mynet_class);
    cdev_del(&mynet_data.cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("MYNET: Module unloaded\n");
}

module_init(mynet_init);
module_exit(mynet_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Network driver logic as character device");

