/**
 * @file dispatch.c
 * @author Isaac Peka
 * @brief Integration with custom port command
 * @version 0.1
 * @date 2023-02-18
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include "linux/module.h"
#include "linux/delay.h"
#include "linux/types.h"
#include "linux/minmax.h"
#include "linux/console.h"
#include "linux/pci.h"
#include "linux/fs.h"
#include "linux/init.h"
#include "linux/module.h"
#include "linux/dma-mapping.h"
#include "linux/dma-direction.h"
#include "linux/cdev.h"
#include "linux/device.h"
#include "linux/blkdev.h"
#include "linux/string.h"
#include "linux/vmalloc.h"

static dev_t devno;
static struct cdev km_vmap_dev;
static struct class *km_vmap_class;

static int km_vmap_open(struct inode *inode, struct file *f)
{
	// struct pci_dev *pdev;
	return 0;
}

static long km_vmap_ioctl(struct file *f, unsigned int ioctl,
			    unsigned long arg)
{
	// struct pci_dev *pdev;
	int i;
	size_t nturns = 0x40;
	size_t npages = 0x10;
	struct page **pages;
	void **maps;

	pages = vmalloc(sizeof(struct page) * npages);
	if (!pages) {
		printk(KERN_WARNING "[-] km_vmap: failed to vmalloc");
		return -1;
	}

	for (i = 0; i < npages; i++) {
		pages[i] = alloc_pages(GFP_HIGHUSER | __GFP_ZERO, 0);
	}

	maps = vmalloc(sizeof(void *) * nturns);
	if (!maps) {
		printk(KERN_WARNING "[-] km_vmap: failed to vmalloc");
		return -1;
	}

	printk(KERN_INFO "[+] km_vmap: pages=%llx, maps=%llx\n", (uint64_t) pages, (uint64_t) maps);
	for (i = 0; i < nturns; i++) {
		maps[i] = vmap(pages, npages, VM_MAP, PAGE_KERNEL);
		if (maps[i] == NULL) {
			printk(KERN_WARNING "[-] km_vmap: failed to vmap: %d", i);
			continue;
		}
		printk(KERN_INFO "[+] km_vmap maps[%d]: %llx\n", i, (uint64_t) maps[i]);
	}

	for (i = 0; i < nturns; i++) {
		vunmap(maps[i]);
	}

	vfree(pages);
	vfree(maps);
	return 0;
}

static const struct file_operations km_vmap_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = km_vmap_ioctl,
	.open           = km_vmap_open,
};

static int __init km_vmap_init(void)
{
	printk(KERN_INFO "[+] km_vmap initializing\n");
	if (alloc_chrdev_region(&devno, 0, 1, "km_vmap") < 0) {
		printk(KERN_ALERT "Failed to allocate chrdev\n");
		return -1;
	}

	printk(KERN_INFO "Initialized devno: major=%d, minor=%d\n", MAJOR(devno), MINOR(devno));

	if ((km_vmap_class = class_create("km_vmap")) == NULL) {
		printk(KERN_ALERT "Failed to set up class\n"); 
		goto cleanup_chrdev;
	}

	if (device_create(km_vmap_class, NULL, devno, NULL, "km_vmap") == NULL) {
		printk(KERN_ALERT "Failed to create device\n");
		goto cleanup_class;
	}

	cdev_init(&km_vmap_dev, &km_vmap_fops);

	if (cdev_add(&km_vmap_dev, devno, 1) < 0) {
		printk(KERN_ALERT "Failed to set up device\n");
		goto cleanup_device;
	}
	
	return 0;

cleanup_device:
	device_destroy(km_vmap_class, devno);
cleanup_class:
	class_destroy(km_vmap_class);
cleanup_chrdev:
	unregister_chrdev_region(devno, 1);
	return -1; 
}

static void __exit km_vmap_exit(void)
{
	printk(KERN_INFO "[+] kvmap exiting\n");
	device_destroy(km_vmap_class, devno);
	class_destroy(km_vmap_class);
	unregister_chrdev_region(devno, 1);
}

module_init(km_vmap_init);
module_exit(km_vmap_exit);

MODULE_AUTHOR("Isaac Peka");
MODULE_LICENSE("GPL");
