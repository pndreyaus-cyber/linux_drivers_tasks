#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#define DEVICE_NAME "simple_char"

static dev_t dev_num;
static struct cdev my_cdev;

static int my_open(struct inode *inode, struct file *file){
	pr_info("simple_char: device opened\n");
	return 0;
}

static int my_release(struct inode *inode, struct file *file){
	pr_info("simple_char: device closed\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = my_open,
	.release = my_release
};

static int __init simple_init(void){
	alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
	cdev_init(&my_cdev, &fops);
	cdev_add(&my_cdev, dev_num, 1);
	pr_info("simple_char: loaded (major=%d minor=%d)\n", MAJOR(dev_num), MINOR(dev_num));
	return 0;

}

static void __exit simple_exit(void){
	cdev_del(&my_cdev);
	unregister_chrdev_region(dev_num, 1);
	pr_info("simple_char: unoloaded\n");
}

module_init(simple_init);
module_exit(simple_exit);

MODULE_DESCRIPTION("Minimal character driver");
MODULE_LICENSE("GPL");
