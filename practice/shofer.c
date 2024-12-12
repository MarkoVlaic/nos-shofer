#include <linux/module.h>
#include "config.h"

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

static int __init shofer_module_init(void) {
	printk(KERN_NOTICE "Module " MODULE_NAME " started\n");
	return 0;
}

static void __exit shofer_module_exit(void) {
	printk(KERN_NOTICE "Module " MODULE_NAME " exit\n");
}

module_init(shofer_module_init);
module_exit(shofer_module_exit);