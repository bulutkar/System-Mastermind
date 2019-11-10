#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>

#include <asm/switch_to.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */


//  check if linux/uaccess.h is required for copy_*_user
//instead of asm/uaccess
//required after linux kernel 4.1+ ?
#ifndef __ASM_ASM_UACCESS_H
    #include <linux/uaccess.h>
#endif


#include "scull_ioctl.h"

#define SCULL_MAJOR 0
#define SCULL_NR_DEVS 4
#define SCULL_QUANTUM 4000
#define SCULL_QSET 1000
#define MAX_GUESS_LIMIT 10

int scull_major = SCULL_MAJOR;
int scull_minor = 0;
int scull_nr_devs = SCULL_NR_DEVS;
int scull_quantum = SCULL_QUANTUM;
int scull_qset = SCULL_QSET;
int mmind_max_guesses = MAX_GUESS_LIMIT;

int result_index = 0;
int s_pos_prev = 0;
int q_pos_prev = 0;
char *mmind_number = "4283";

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_nr_devs, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);
module_param(scull_qset, int, S_IRUGO);
module_param(mmind_max_guesses, int, 0); 
module_param(mmind_number, charp, 0);

MODULE_AUTHOR("Alessandro Rubini, Jonathan Corbet");
MODULE_LICENSE("Dual BSD/GPL");

struct scull_dev {
    char **data;
    int quantum;
    int qset;
    unsigned long size;
    struct semaphore sem;
    struct cdev cdev;
};

struct scull_dev *scull_devices;

int scull_trim(struct scull_dev *dev)
{
    int i;

    if (dev->data) {
        for (i = 0; i < dev->qset; i++) {
            if (dev->data[i])
                kfree(dev->data[i]);
        }
        kfree(dev->data);
    }
    dev->data = NULL;
    dev->quantum = scull_quantum;
    dev->qset = scull_qset;
    dev->size = 0;
    return 0;
}


int scull_open(struct inode *inode, struct file *filp)
{
    struct scull_dev *dev;
    //printk(KERN_EMERG "start of scull_open\n");

    dev = container_of(inode->i_cdev, struct scull_dev, cdev);
    filp->private_data = dev;

    /* trim the device if open was write-only */
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) {
        if (down_interruptible(&dev->sem))
            return -ERESTARTSYS;
        up(&dev->sem);
    }
    //printk(KERN_EMERG "end of scull_open\n");
    return 0;
}


int scull_release(struct inode *inode, struct file *filp)
{
    return 0;
}


ssize_t scull_read(struct file *filp, char __user *buf, size_t count,
                   loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    int quantum = dev->quantum;
    int s_pos, q_pos;
    int j;
    ssize_t retval = 0;
    
    printk(KERN_EMERG "start of scull_read\n");

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;
    if (*f_pos >= dev->size)
        goto out;
    if (*f_pos + count > dev->size)
        count = dev->size - *f_pos;

    s_pos = (long) *f_pos / quantum;
    q_pos = (long) *f_pos % quantum;

	s_pos = (long)*f_pos / quantum;
	q_pos = (long)*f_pos % quantum;
	//printk(KERN_EMERG "scull_read= s_pos: %d,  q_pos: %d\n", s_pos, q_pos);

    if (dev->data == NULL || ! dev->data[s_pos])
        goto out;

    /* read only up to the end of this quantum */
    if (count > quantum - q_pos)
        count = quantum - q_pos;


    if (copy_to_user(buf, dev->data[s_pos] + q_pos, count)) {
        retval = -EFAULT;
        goto out;
    }
    *f_pos += count;
    retval = count;

  out:
    up(&dev->sem);
    printk(KERN_EMERG "end of scull_read\n");
    return retval;
}


void fill_result(struct scull_dev* dev, int s_pos, int q_pos, char in_place, char out_place);

ssize_t scull_write(struct file *filp, const char __user *buf, size_t count,
                    loff_t *f_pos)
{
    struct scull_dev *dev = filp->private_data;
    int quantum = dev->quantum, qset = dev->qset;
    int s_pos, q_pos;
    // int in_place = 0, out_place = 0;
    char in_place = '0';
    char out_place = '0';
    int x, y;
    ssize_t retval = -ENOMEM;
    
    printk(KERN_EMERG "start of scull_write\n");

    if (down_interruptible(&dev->sem))
        return -ERESTARTSYS;

    if (*f_pos >= quantum * qset) {
        retval = 0;
        goto out;
    }

    s_pos = (long) *f_pos / quantum;
    q_pos = (long) *f_pos % quantum;
	s_pos += s_pos_prev;
	q_pos += q_pos_prev;
	if (q_pos >= 4000) {
		s_pos += 1;
		q_pos %= 4000;
	}

    if (!dev->data) {
        dev->data = kmalloc(qset * sizeof(char *), GFP_KERNEL);
        if (!dev->data)
            goto out;
        memset(dev->data, 0, qset * sizeof(char *));
    }
    if (!dev->data[s_pos]) {
        dev->data[s_pos] = kmalloc(quantum, GFP_KERNEL);
        if (!dev->data[s_pos])
            goto out;
    }
    /* write only up to the end of this quantum */
    if (count > quantum - q_pos)
        count = quantum - q_pos;

    if (copy_from_user(dev->data[s_pos] + q_pos, buf, count)) {
        retval = -EFAULT;
        goto out;
    }
    
        if(result_index+1>mmind_max_guesses){
		printk(KERN_ALERT "Sorry. Guess limit has been reached.Limit is %d\n", mmind_max_guesses);
		retval = -EDQUOT;
        goto out;
		}
	printk(KERN_ALERT "Guess limit: %d.\n", mmind_max_guesses);
		
    for (x = 0; x < 4; x++) {
		for (y = 0; y < 4; y++) {
			if ((dev->data[s_pos] + q_pos)[x] == mmind_number[y]) {
				if (x == y) {
					if (in_place == '0')
						in_place = '1';
					else if (in_place == '1')
						in_place = '2';
					else if (in_place == '2')
						in_place = '3';
					else if (in_place == '3')
						in_place = '4';
				}
				else {
					if (out_place == '0')
						out_place = '1';
					else if (out_place == '1')
						out_place = '2';
					else if (out_place == '2')
						out_place = '3';
					else if (out_place == '3')
						out_place = '4';
				}
			}
		}
	}
    
	fill_result(dev,s_pos, q_pos, in_place, out_place);
    result_index++;
    
    *f_pos += count;
    retval = count;
    
    /* update the size */
	dev->size += 16;
	q_pos += 16;
	if (q_pos >= 4000) {
		s_pos += 1;
		q_pos %= 4000;
	}

  out:
    up(&dev->sem);
    printk(KERN_EMERG "end of scull_write\n");
    return retval;
}

void fill_result(struct scull_dev* dev, int s_pos, int q_pos, char in_place, char out_place)
{
	(dev->data[s_pos] + q_pos)[4] = ' ';
	(dev->data[s_pos] + q_pos)[5] = in_place;
	(dev->data[s_pos] + q_pos)[6] = '+';
	(dev->data[s_pos] + q_pos)[7] = ' ';
	(dev->data[s_pos] + q_pos)[8] = out_place;
	(dev->data[s_pos] + q_pos)[9] = '-';
	(dev->data[s_pos] + q_pos)[10] = ' ';
	if (result_index < 9) {
		int guess_count = result_index + 1;
		(dev->data[s_pos] + q_pos)[11] = '0';
		(dev->data[s_pos] + q_pos)[12] = '0';
		(dev->data[s_pos] + q_pos)[13] = '0';
		(dev->data[s_pos] + q_pos)[14] = guess_count + '0';
	}
	else if (result_index < 99) {
		int guess_count = result_index + 1;
		(dev->data[s_pos] + q_pos)[11] = '0';
		(dev->data[s_pos] + q_pos)[12] = '0';
		(dev->data[s_pos] + q_pos)[13] = guess_count / 10 + '0';
		(dev->data[s_pos] + q_pos)[14] = guess_count % 10 + '0';
	}
	else if (result_index < 999) {
		int guess_count = result_index + 1;
		(dev->data[s_pos] + q_pos)[11] = '0';
		(dev->data[s_pos] + q_pos)[12] = guess_count / 100 + '0';
		(dev->data[s_pos] + q_pos)[13] = guess_count / 10 % 10 + '0';
		(dev->data[s_pos] + q_pos)[14] = guess_count % 10 + '0';
	}
	else {
		int guess_count = result_index + 1;
		(dev->data[s_pos] + q_pos)[11] = guess_count / 1000 + '0';
		(dev->data[s_pos] + q_pos)[12] = guess_count / 100 % 10 + '0';
		(dev->data[s_pos] + q_pos)[13] = guess_count / 10 % 10 + '0';
		(dev->data[s_pos] + q_pos)[14] = guess_count % 10 + '0';
	}
	(dev->data[s_pos] + q_pos)[15] = '\n';
}

long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

	int err = 0, tmp;
	int retval = 0;

	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err =  !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {
	  case SCULL_IOCRESET:
		scull_quantum = SCULL_QUANTUM;
		scull_qset = SCULL_QSET;
		break;

	  case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		retval = __get_user(scull_quantum, (int __user *)arg);
		break;

	  case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		scull_quantum = arg;
		break;

	  case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
		retval = __put_user(scull_quantum, (int __user *)arg);
		break;

	  case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
		return scull_quantum;

	  case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_quantum;
		retval = __get_user(scull_quantum, (int __user *)arg);
		if (retval == 0)
			retval = __put_user(tmp, (int __user *)arg);
		break;

	  case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_quantum;
		scull_quantum = arg;
		return tmp;

	  case SCULL_IOCSQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		retval = __get_user(scull_qset, (int __user *)arg);
		break;

	  case SCULL_IOCTQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		scull_qset = arg;
		break;

	  case SCULL_IOCGQSET:
		retval = __put_user(scull_qset, (int __user *)arg);
		break;

	  case SCULL_IOCQQSET:
		return scull_qset;

	  case SCULL_IOCXQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_qset;
		retval = __get_user(scull_qset, (int __user *)arg);
		if (retval == 0)
			retval = put_user(tmp, (int __user *)arg);
		break;

	  case SCULL_IOCHQSET:
		if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		tmp = scull_qset;
		scull_qset = arg;
		return tmp;
	
	  case SET_GUESS_LIMIT:
	     if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
		mmind_max_guesses = arg;
		break;

      case MMIND_REMAINING:
          if (! capable (CAP_SYS_ADMIN))
			return -EPERM;
            retval = __put_user(mmind_max_guesses - result_index, (int __user *)arg);
            break;
        

	  default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;
}


loff_t scull_llseek(struct file *filp, loff_t off, int whence)
{
    struct scull_dev *dev = filp->private_data;
    loff_t newpos;

    switch(whence) {
        case 0: /* SEEK_SET */
            newpos = off;
            break;

        case 1: /* SEEK_CUR */
            newpos = filp->f_pos + off;
            break;

        case 2: /* SEEK_END */
            newpos = dev->size + off;
            break;

        default: /* can't happen */
            return -EINVAL;
    }
    if (newpos < 0)
        return -EINVAL;
    filp->f_pos = newpos;
    return newpos;
}


struct file_operations scull_fops = {
    .owner =    THIS_MODULE,
    .llseek =   scull_llseek,
    .read =     scull_read,
    .write =    scull_write,
    .unlocked_ioctl =  scull_ioctl,
    .open =     scull_open,
    .release =  scull_release,
};


void scull_cleanup_module(void)
{
    int i;
    dev_t devno = MKDEV(scull_major, scull_minor);

	// printk(KERN_EMERG "start of cleanup module\n");
    if (scull_devices) {
        for (i = 0; i < scull_nr_devs; i++) {
            scull_trim(scull_devices + i);
            cdev_del(&scull_devices[i].cdev);
        }
    kfree(scull_devices);
    }

    unregister_chrdev_region(devno, scull_nr_devs);
    // printk(KERN_EMERG "end of cleanup module\n");
}


int scull_init_module(void)
{
    int result, i;
    int err;
    dev_t devno = 0;
    struct scull_dev *dev;

    if (scull_major) {
        devno = MKDEV(scull_major, scull_minor);
        result = register_chrdev_region(devno, scull_nr_devs, "scull");
    } else {
        result = alloc_chrdev_region(&devno, scull_minor, scull_nr_devs,
                                     "scull");
        scull_major = MAJOR(devno);
    }
    if (result < 0) {
        printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
        return result;
    }

    scull_devices = kmalloc(scull_nr_devs * sizeof(struct scull_dev),
                            GFP_KERNEL);
    if (!scull_devices) {
        result = -ENOMEM;
        goto fail;
    }
    memset(scull_devices, 0, scull_nr_devs * sizeof(struct scull_dev));

    /* Initialize each device. */
    for (i = 0; i < scull_nr_devs; i++) {
        dev = &scull_devices[i];
        dev->quantum = scull_quantum;
        dev->qset = scull_qset;
        sema_init(&dev->sem,1);
        devno = MKDEV(scull_major, scull_minor + i);
        cdev_init(&dev->cdev, &scull_fops);
        dev->cdev.owner = THIS_MODULE;
        dev->cdev.ops = &scull_fops;
        err = cdev_add(&dev->cdev, devno, 1);
        if (err)
            printk(KERN_NOTICE "Error %d adding scull%d", err, i);
    }

    return 0; /* succeed */

  fail:
    scull_cleanup_module();
    return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
