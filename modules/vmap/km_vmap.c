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
#include "linux/list.h"
#include "linux/types.h"
#include "linux/scs.h"
#include "linux/thread_info.h"

#include "km_vmap.h"

#define ARM64_THREAD_SIZE 0x4000
#define ARM64_THREAD_ALIGN (ARM64_THREAD_SIZE * 2)
#define ARM64_SCS_SIZE 0x2000

static dev_t devno;
static struct cdev km_vmap_dev;
static struct class *km_vmap_class;

struct km_vmap_allocation {
  struct list_head head;
  void *addr;
  size_t npages;
};

struct km_vmap_fork_allocation {
  struct list_head head;
  void *stack;
  void *scs;
};

struct km_vmap_session {
  struct list_head allocs;
  struct list_head procs;
};

static void km_vmap_allocation_free(struct km_vmap_allocation *alloc, int log) {
  vunmap(alloc->addr);

  if (log) {
    printk(KERN_INFO "[+] Unmapped: %llx - %llx\n", (u64) alloc->addr, 
            (u64) alloc->addr + (alloc->npages << PAGE_SHIFT));
  }

  list_del(&alloc->head);
  kfree(alloc);	
}

static void km_vmap_fork_free(struct km_vmap_fork_allocation *alloc, int log) {
  vfree(alloc->stack);
  vfree(alloc->scs);

  if (log) {
    printk(KERN_INFO "[+] Unmapped stack: %llx - %llx\n", (u64) alloc->stack, 
            (u64) alloc->stack + ARM64_THREAD_SIZE);

    printk(KERN_INFO "[+] Unmapped SCS: %llx - %llx\n", (u64) alloc->scs,
            (u64) alloc->scs + ARM64_SCS_SIZE);
  }

  list_del(&alloc->head);
  kfree(alloc);
}

static int km_vmap_open(struct inode *inode, struct file *f) {
  struct km_vmap_session *session = kzalloc(sizeof(struct km_vmap_session), GFP_KERNEL);
  if (!session)
    return -ENOMEM;

  INIT_LIST_HEAD(&session->allocs);
  INIT_LIST_HEAD(&session->procs);
  f->private_data = session;
  return 0;
}

static int km_vmap_release(struct inode *inode, struct file *f) {
  struct km_vmap_session *session = f->private_data;
  struct km_vmap_allocation *alloc;
  struct km_vmap_fork_allocation *fork;
  struct list_head *entry;
  struct list_head *tmp;

  printk(KERN_INFO "[+] km_vmap_release\n");

  list_for_each_safe(entry, tmp, &session->allocs) {
    alloc = container_of(entry, struct km_vmap_allocation, head);
    km_vmap_allocation_free(alloc, false);
  }

  list_for_each_safe(entry, tmp, &session->procs) {
    fork = container_of(entry, struct km_vmap_fork_allocation, head);
    km_vmap_fork_free(fork, false);
  }

  kfree(session);
  return 0;
}

/**
 * @brief Simulates a fork with CONFIG_SHADOW_CALL_STACK enabled
 * 
 * @param session 
 * @return long 
 */
static long km_vmap_ioctl_fork(struct km_vmap_session *session, 
    struct km_vmap_user_fork *req) {

  long status = 0;
  struct km_vmap_fork_allocation *alloc = kmalloc(sizeof(*alloc), GFP_KERNEL); 
  if (!alloc)
    return -ENOMEM;

  alloc->stack = __vmalloc_node(ARM64_THREAD_SIZE, ARM64_THREAD_ALIGN,				
                                THREADINFO_GFP & ~__GFP_ACCOUNT, 
                                NUMA_NO_NODE,
                                __builtin_return_address(0));
  
  printk("[+] Stack: %llx - %llx", (u64) alloc->stack, 
          (u64) alloc->stack + ARM64_THREAD_SIZE);
  if (!alloc->stack) {
    status = -ENOMEM;
    goto err;
  }

  alloc->scs = __vmalloc_node(ARM64_SCS_SIZE, 1, 
                              THREADINFO_GFP & ~__GFP_ACCOUNT, 
                              NUMA_NO_NODE,
                              __builtin_return_address(0));

  printk("[+] SCS: %llx - %llx", (u64) alloc->scs,
          (u64) alloc->scs + ARM64_SCS_SIZE);
  if (!alloc->scs) {
    status = -ENOMEM;
    goto err_scs;
  }
  
  INIT_LIST_HEAD(&alloc->head);
  list_add(&alloc->head, &session->procs);

  req->stack = (u64) alloc->stack;
  req->scs = (u64) alloc->scs;
  return status;
err_scs:
  vfree(alloc->stack);
err:
  kfree(alloc);
  return status;
}

static long km_vmap_ioctl_unfork(struct km_vmap_session *session, struct km_vmap_user_fork *req) {
  int found = false;
  struct list_head *entry;
  struct km_vmap_fork_allocation *alloc;

  list_for_each(entry, &session->procs) {
    alloc = container_of(entry, struct km_vmap_fork_allocation, head);
    if ((u64) alloc->stack == req->stack) {
      found = 1;
      break;
    }
  }

  if (!found)
    return -EINVAL;

  km_vmap_fork_free(alloc, true);
  return 0;
}

static long km_vmap_ioctl_unmap(struct km_vmap_session *session,
    struct km_vmap_user_unmap *req) {
  int found = false;
  struct list_head *entry;
  struct km_vmap_allocation *alloc;

  list_for_each(entry, &session->allocs) {
    alloc = container_of(entry, struct km_vmap_allocation, head);
    if ((u64) alloc->addr == req->addr) {
      found = 1;
      break;
    }
  }

  if (!found)
    return -EINVAL;

  km_vmap_allocation_free(alloc, true);
  return 0;
}

static long km_vmap_ioctl_map(struct km_vmap_session *session, 
    struct km_vmap_user_map *req) {
  int i;
  long status = 0;
  struct page *page;
  struct page **pages;
  struct km_vmap_allocation *alloc;
  unsigned int order = get_order(req->npages);

  if (!(pages = vmalloc(req->npages))) 
    return -ENOMEM;

  if (!(page = alloc_pages(GFP_KERNEL, order))) {
    status = -ENOMEM;
    goto out;
  }

  for (i = 0; i < req->npages; i++)
    pages[i] = &page[i];

  alloc = kmalloc(sizeof(*alloc), GFP_KERNEL);
  if (!alloc) {
    status = -ENOMEM;
    goto err_kalloc;
  }

  alloc->addr = vmap(pages, req->npages, VM_MAP, PAGE_KERNEL);
  if (!alloc->addr) {
    status = -ENOMEM;
    goto err_vmap;
  }

  alloc->npages = req->npages;
  printk(KERN_INFO "[+] Allocated: %llx - %llx\n", (u64) alloc->addr, 
          (u64) alloc->addr + (alloc->npages << PAGE_SHIFT));

  req->addr = (u64) alloc->addr;
  list_add(&alloc->head, &session->allocs);
  goto out;
err_vmap:
  if (!list_is_head(&session->allocs, &alloc->head))
    kfree(alloc);
err_kalloc:
  __free_pages(page, order);
out:
  vfree(pages);
  return status;
}


static long km_vmap_ioctl(struct file *f, unsigned int ioctl,
          unsigned long arg) {
  long status = 0;
  struct km_vmap_session *session = f->private_data;

  switch(ioctl) {
    case KM_VMAP_IOCTL_MAP: {
      struct km_vmap_user_map request;
      if (copy_from_user(&request, (void *) arg, sizeof(request)))
        return -EINVAL;
      if ((status = km_vmap_ioctl_map(session, &request)))
        return status;
      if (copy_to_user((void *) arg, &request, sizeof(request)))
        return -EINVAL;
      break;
    }
    case KM_VMAP_IOCTL_UNMAP: {
      struct km_vmap_user_unmap request;
      if (copy_from_user(&request, (void *) arg, sizeof(request)))
        return -EINVAL;
      if ((status = km_vmap_ioctl_unmap(session, &request)))
        return status;
      break;
    }
    case KM_VMAP_IOCTL_FORK: {
      struct km_vmap_user_fork request;
      if ((status = km_vmap_ioctl_fork(session, &request)))
        return status;
      if (copy_to_user((void *) arg, &request, sizeof(request)))
        return -EINVAL;
      break;
    }
    case KM_VMAP_IOCTL_UNFORK: {
      struct km_vmap_user_fork request;
      if (copy_from_user(&request, (void *) arg, sizeof(request)))
        return -EINVAL;
      if ((status = km_vmap_ioctl_unfork(session, &request)))
        return status;
      break;
    }
  }

  return 0;
}

static const struct file_operations km_vmap_fops = {
  .owner          = THIS_MODULE,
  .unlocked_ioctl = km_vmap_ioctl,
  .open           = km_vmap_open,
  .release				= km_vmap_release
};

static int __init km_vmap_init(void) {
  printk(KERN_INFO "[+] km_vmap initializing\n");
  if (alloc_chrdev_region(&devno, 0, 1, "km_vmap") < 0) {
    printk(KERN_ALERT "Failed to allocate chrdev\n");
    return -1;
  }

  printk(KERN_INFO "Initialized devno: major=%d, minor=%d\n", MAJOR(devno), MINOR(devno));

  if ((km_vmap_class = class_create(THIS_MODULE, "km_vmap")) == NULL) {
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

static void __exit km_vmap_exit(void) {
  printk(KERN_INFO "[+] kvmap exiting\n");
  device_destroy(km_vmap_class, devno);
  class_destroy(km_vmap_class);
  unregister_chrdev_region(devno, 1);
}

module_init(km_vmap_init);
module_exit(km_vmap_exit);

MODULE_AUTHOR("Isaac Peka");
MODULE_LICENSE("GPL");
