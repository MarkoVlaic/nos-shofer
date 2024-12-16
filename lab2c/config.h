#ifndef SHOFER_CONFIG_H
#define SHOFER_CONFIG_H

#define DRIVER_NAME "shofer"

#define AUTHOR "Marko Vlaic"
#define LICENSE "Dual BSD/GPL"

#define PIPE_SIZE 64
#define MAX_THREADS 10

struct pipe {
  size_t pipe_size;
  size_t max_threads;
  atomic_t thread_cnt;

  struct kfifo fifo;
  uint8_t *buffer;

  // reader level sync
  struct mutex reader_lock;
  struct completion not_empty;
  
  // writer level sync
  struct mutex writer_lock;
  struct completion not_full;
  
  // pipe level sync
  struct mutex lock;
};

struct shofer_dev {
  dev_t dev_no;
  struct cdev cdev;
  struct pipe pipe;
};

#define klog(LEVEL, format, ...)	\
printk(LEVEL "[shofer] %d: " format "\n", __LINE__, ##__VA_ARGS__)

#ifdef SHOFER_DEBUG
#define LOG(format, ...)	klog(KERN_DEBUG, format,  ##__VA_ARGS__)
#else /* !SHOFER_DEBUG */
#warning Debug not activated
#define LOG(format, ...)
#endif /* SHOFER_DEBUG */

#endif