#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/device/class.h>

#define DEVICE_NAME "simple_char"

static struct class  *simple_class;
static dev_t dev_num;
static struct cdev simple_cdev;
static struct device *simple_device;

static int dev_open(struct inode *inode, struct file *file) {
	printk(KERN_INFO "simple_char: device opened\n");
	return 0;
}

static int dev_release(struct inode *inode, struct file *file){
	printk(KERN_INFO "simple_char: device closed\n");
	return 0;
}

static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset){
	char msg[] = "Hello from kernel (with dev_t)!\n";
	int bytes = sizeof(msg);

	if(*offset >= bytes){
		return 0;
	}

	if(copy_to_user(buf, msg, bytes))
		return -EFAULT;

	*offset += bytes;
	printk(KERN_INFO "simple_char: read %d bytes\n", bytes);
	return bytes;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = dev_open,
	.release = dev_release,
	.read = dev_read,
};

static int __init simple_init(void)
{
	int res = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	if (res < 0){
		printk(KERN_ALERT "simple_char: alloc_chrdev_region failed\n");
		return res;
	}

	printk(KERN_INFO "simple_char: registered <major %d, minor %d>\n", MAJOR(dev_num), MINOR(dev_num));

	cdev_init(&simple_cdev, &fops);
	simple_cdev.owner = THIS_MODULE;
	res = cdev_add(&simple_cdev, dev_num, 1);
	if (res < 0) {
		unregister_chrdev_region(dev_num, 1);
		printk(KERN_ALERT "simple_char: cdev_add failed\n");
		return res;
	}

	simple_class = class_create(DEVICE_NAME);
	if (IS_ERR(simple_class)) {
		cdev_del(&simple_cdev);
		unregister_chrdev_region(dev_num, 1);
		printk(KERN_ALERT "simple_char: class_create failed\n");
		return PTR_ERR(simple_class);
	}

	simple_device = device_create(simple_class, NULL, dev_num, NULL, DEVICE_NAME);
	if(IS_ERR(simple_device)) {
		class_destroy(simple_class);
		cdev_del(&simple_cdev);
		unregister_chrdev_region(dev_num, 1);
		printk(KERN_ALERT "simple_char: device_create failed\n");
		return PTR_ERR(simple_device);
	}

	printk(KERN_INFO "simple_char: device created successfully\n");
	return 0;

}

static void __exit simple_exit(void)
{
	device_destroy(simple_class, dev_num);
	class_destroy(simple_class);
	cdev_del(&simple_cdev);
	unregister_chrdev_region(dev_num, 1);
	printk(KERN_INFO "simple_char: unregistered and cleaned up\n");
}

module_init(simple_init);
module_exit(simple_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andrey");
MODULE_DESCRIPTION("Character driver using dev_t, cdev, and class_create");
