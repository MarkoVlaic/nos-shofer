#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/kfifo.h>
#include <linux/log2.h>

#include "config.h"

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

struct shofer_dev *Shofer = NULL;
static dev_t Dev_no = 0;

static struct shofer_dev *shofer_create(dev_t, struct file_operations *, int *);
static void shofer_delete(struct shofer_dev *);
static void cleanup(void);

static int shofer_open(struct inode *, struct file *);
static int shofer_release(struct inode *, struct file *);
static ssize_t shofer_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t shofer_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations shofer_fops = {
	.owner = THIS_MODULE,
	.open = shofer_open,
	.release = shofer_release,
	.read = shofer_read,
	.write = shofer_write
};

static int __init shofer_module_init(void) {
	int retval;
	dev_t dev_no = 0;
	struct shofer_dev *shofer;
	printk(KERN_NOTICE "Module " MODULE_NAME " started initialization\n");

	retval = alloc_chrdev_region(&dev_no, 0, 1, DRIVER_NAME);
	if(retval < 0) {
		printk(KERN_WARNING "%s: cannot get mahor device number %d\n", DRIVER_NAME, MAJOR(dev_no));
		return retval;
	}

	shofer = shofer_create(dev_no, &shofer_fops, &retval);
	if(!shofer)
		goto no_driver;

	Dev_no = dev_no;
	Shofer = shofer;

	return 0;

no_driver:
	cleanup();
	return retval;
}

static void __exit shofer_module_exit(void) {
	printk(KERN_NOTICE "Module " MODULE_NAME " exit\n");
}

static void cleanup(void) {
	if(Shofer) {
		shofer_delete(Shofer);
	}
}

static struct shofer_dev *shofer_create(dev_t dev_no, struct file_operations *fops, int *retval) {
	struct shofer_dev *shofer = kmalloc(sizeof(struct shofer_dev), GFP_KERNEL);
	if(!shofer) {
		*retval = -ENOMEM;
		printk(KERN_NOTICE "shofer: kmalloc failed\n");
		return NULL;
	}

	memset(shofer, 0, sizeof(struct shofer_dev));
	
	cdev_init(&shofer->cdev, fops);
	shofer->cdev.owner = THIS_MODULE;
	shofer->cdev.ops = fops;
	*retval = cdev_add(&shofer->cdev, dev_no, 1);
	shofer->dev_no = dev_no;
	if(*retval) {
		printk(KERN_NOTICE "Error (%d) when adding device shofer\n", *retval);
		kfree(shofer);
		shofer = NULL;
	}

	return shofer;
}

static void shofer_delete(struct shofer_dev *shofer) {
	cdev_del(&shofer->cdev);
	kfree(shofer);
}

module_init(shofer_module_init);
module_exit(shofer_module_exit);