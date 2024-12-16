#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/wait.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/completion.h>

#include "config.h"

static int pipe_size = PIPE_SIZE;
static int max_threads = MAX_THREADS;

module_param(pipe_size, int, S_IRUGO);
MODULE_PARM_DESC(pipe_size, "Buffer size in bytes");
module_param(max_threads, int, S_IRUGO);
MODULE_PARM_DESC(max_threads, "Max number of simoltaneous threads accessing the pipeline");

MODULE_AUTHOR(AUTHOR);
MODULE_LICENSE(LICENSE);

static dev_t Dev_no = 0;
static struct shofer_dev *Shofer = NULL;

// prototypes
static struct shofer_dev *shofer_create(dev_t, struct file_operations *, int *);
static void shofer_delete(struct shofer_dev *);
static int pipe_init(struct pipe *pipe, size_t size, size_t max_threads);
static void pipe_delete(struct pipe *pipe);

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
  struct shofer_dev *shofer;
  dev_t dev_no = 0;

  klog(KERN_NOTICE, "Module started initialization");

  retval = alloc_chrdev_region(&dev_no, 0, 1, DRIVER_NAME);
  if(retval < 0) {
    klog(KERN_WARNING, "Can't get major device number");
    return retval;
  }
  Dev_no = dev_no;

  shofer = shofer_create(dev_no, &shofer_fops, &retval);
  if(!shofer)
    goto no_driver;

  klog(KERN_NOTICE, "Module initialized");

  Shofer = shofer;
  Dev_no = dev_no;

  return 0;

no_driver:
  cleanup();
  return retval;
}

static void __exit shofer_module_exit(void) {
  cleanup();
  klog(KERN_NOTICE, "Module unloaded");
}

module_init(shofer_module_init);
module_exit(shofer_module_exit);

void cleanup() {
  if(Dev_no)
    unregister_chrdev_region(Dev_no, 1);
  if(Shofer)
    shofer_delete(Shofer);
}

static struct shofer_dev *shofer_create(dev_t dev_no, struct file_operations *fops, int *retval) {
  struct shofer_dev *shofer = kmalloc(sizeof(struct shofer_dev), GFP_KERNEL);
  if(!shofer) {
    *retval = -ENOMEM;
    klog(KERN_ERR, "kmalloc failed");
    return NULL;
  }
  memset(shofer, 0, sizeof(struct shofer_dev));

  cdev_init(&shofer->cdev, fops);
  shofer->cdev.owner = THIS_MODULE;
  shofer->cdev.ops = fops;
  *retval = cdev_add(&shofer->cdev, dev_no, 1);
  shofer->dev_no = dev_no;
  if(*retval) {
    klog(KERN_ERR, "Error (%d) adding device shofer", *retval);
    kfree(shofer);
    shofer = NULL;
  }

  *retval = pipe_init(&shofer->pipe, pipe_size, max_threads);
  if(*retval != 0) {
    klog(KERN_ERR, "Error (%d) initializing pipe", *retval);
    kfree(shofer);
    shofer = NULL;
  }

  return shofer;
}

static void shofer_delete(struct shofer_dev *shofer) {
  cdev_del(&shofer->cdev);
  pipe_delete(&shofer->pipe);
  kfree(shofer);
}

static int pipe_init(struct pipe *pipe, size_t size, size_t max_threads) {
  pipe->max_threads = max_threads;
  pipe->pipe_size = size;
  pipe->thread_cnt = (atomic_t) ATOMIC_INIT(0);

  pipe->buffer = kmalloc(size, GFP_KERNEL);
  if(!pipe->buffer) {
    return -ENOMEM;
  }
  kfifo_init(&pipe->fifo, pipe->buffer, size);

  mutex_init(&pipe->reader_lock);
  init_completion(&pipe->not_empty);
  mutex_init(&pipe->writer_lock);
  init_completion(&pipe->not_full);
  mutex_init(&pipe->lock);

  return 0;
}

static void pipe_delete(struct pipe *pipe) {
  kfree(pipe->buffer);
}

static int shofer_open(struct inode *inode, struct file *filp) {
  struct shofer_dev *shofer;
  int accmode;

  shofer = container_of(inode->i_cdev, struct shofer_dev, cdev);
  filp->private_data = shofer;

  accmode = filp->f_flags & O_ACCMODE; 
  if(accmode != O_RDONLY || accmode != O_WRONLY || accmode != O_RDWR) {
    LOG("not permitted %d", accmode);
    return -EPERM;
  }

  if(atomic_read(&shofer->pipe.thread_cnt) == shofer->pipe.max_threads) {
    return -EBUSY;
  }
  atomic_inc(&shofer->pipe.thread_cnt);

  return 0;
}

static int shofer_release(struct inode *inode, struct file *filp) {
  struct shofer_dev *shofer;
  shofer = filp->private_data;
  atomic_dec(&shofer->pipe.thread_cnt);
  return 0;
}

static ssize_t shofer_read(struct file *filp, char __user *ubuf, size_t count, loff_t *fpos) {
  struct shofer_dev *shofer;
  struct pipe *pipe;
  struct kfifo *fifo;
  int accmode;
  size_t to_read, len;
  unsigned int copied;
  ssize_t retval;

  shofer = filp->private_data;
  pipe = &shofer->pipe;
  fifo = &pipe->fifo;
  accmode = filp->f_flags & O_ACCMODE;

  if(accmode != O_RDONLY || accmode != O_RDWR) {
    return -EPERM;
  }

  if(mutex_lock_interruptible(&pipe->reader_lock)) {
    return -ERESTARTSYS;
  }

  while(1) {
    if(mutex_lock_interruptible(&pipe->lock)) {
      mutex_unlock(&pipe->reader_lock);
      return -ERESTARTSYS;
    }

    if(kfifo_is_empty(fifo)) {
      mutex_unlock(&pipe->lock);
      if(wait_for_completion_interruptible(&pipe->not_empty)) {
        mutex_unlock(&pipe->reader_lock);
        return -ERESTARTSYS;
      }
    } else {
      break;
    }
  }

  len = kfifo_len(fifo);
  to_read = min(count, len);
  retval = kfifo_to_user(fifo, ubuf, to_read, &copied);
  if(retval) {
    klog(KERN_WARNING, "kfifo_to_user failed");
  } else {
    retval = copied;
  }

  complete(&pipe->not_full);
  mutex_unlock(&pipe->lock);
  mutex_unlock(&pipe->reader_lock);

  return copied;
}

static ssize_t shofer_write(struct file *filp, const char __user *ubuf, size_t count, loff_t *fpos) {
  struct shofer_dev *shofer;
  struct pipe *pipe;
  struct kfifo *fifo;
  int accmode;
  unsigned int copied;
  ssize_t retval;

  shofer = filp->private_data;
  pipe = &shofer->pipe;
  fifo = &pipe->fifo;
  accmode = filp->f_flags & O_ACCMODE;

  if(accmode != O_WRONLY || accmode != O_RDWR) {
    return -EPERM;
  }

  if(mutex_lock_interruptible(&pipe->writer_lock)) {
    return -ERESTARTSYS;
  }

  while(1) {
    if(mutex_lock_interruptible(&pipe->lock)) {
      mutex_unlock(&pipe->writer_lock);
      return -ERESTARTSYS;
    }

    if(kfifo_avail(fifo) < count) {
      mutex_unlock(&pipe->lock);
      if(wait_for_completion_interruptible(&pipe->not_full)) {
        mutex_unlock(&pipe->writer_lock);
        return -ERESTARTSYS;
      }
    } else {
      break;
    }
  }

  retval = kfifo_from_user(fifo, ubuf, count, &copied);
  if(retval) {
    klog(KERN_WARNING, "kfifo_to_user failed");
  } else {
    retval = copied;
  }

  complete(&pipe->not_empty);
  mutex_unlock(&pipe->lock);
  mutex_unlock(&pipe->writer_lock);

  return copied;
}
