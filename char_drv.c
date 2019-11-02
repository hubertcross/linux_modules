/**
 * char_drv.c
 * An introductory character driver
 * This module maps to /dev/char_drv.  Run C program in user space to 
 * communicate with module.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>  // Macros used to mark up functions e.g. __init __exit
#include <linux/kernel.h>
#include <asm/uaccess.h>          // Required for the copy to user function

#define NAME "hubert" // using this prefix for what shows up in proc, dev, etc

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Linux Class");
MODULE_DESCRIPTION("Linux char driver");
// MODULE_VERSION("0.1");

static int major = -1;
static char   message[256] = {0};  // Memory for the string that is passed from userspace
static short  size_of_message;     // hold the size of the string stored
static int    numberOpens = 0;    // Counts the number of times the device is opened

static struct class*  char_drvClass  = NULL; // The device-driver class struct pointer
static struct device* char_drvDevice = NULL; // The device-driver device struct pointer

static struct cdev mycdev;

/** 
 * Function - dev_open: 
 * The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *
 * Prameters:
 *  inodep - A pointer to an inode object (defined in linux/fs.h)
 *  filep  - A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep)
{
   numberOpens++;
   printk(KERN_INFO "ATMELChar: Device has been opened %d time(s)\n", numberOpens);

   return 0;
}

/** 
 * Function - dev_read:
 * This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *
 * Parameters:
 *  filep  - A pointer to a file object (defined in linux/fs.h)
 *  buffer - The pointer to the buffer to which this function writes the data
 *  len    - The length of the b
 *  offset - The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
   int error_count = 0;
   // copy_to_user has the format ( * to, *from, size) and returns 0 on success
   error_count = copy_to_user(buffer, message, size_of_message);

   if (error_count == 0 ) {            // if true then have success
      printk(KERN_INFO "ATMELChar: Sent %d characters to the user\n", size_of_message);
      return (size_of_message=0);  // clear the position to the start and return 0
   }
   else {
      printk(KERN_INFO "ATMELChar: Failed to send %d characters to the user\n", error_count);
      return -EFAULT;              // Failed -- return a bad address message (i.e. -14)
   }

}

/** 
 * Function - dev_write:
 * This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *
 * Parameters:
 *  filep  - A pointer to a file object
 *  buffer - The buffer to that contains the string to write to the device
 *  len    - The length of the array of data that is being passed in the const char buffer
 *  offset - The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
   printk(KERN_INFO "buffer, %x\n", buffer);
   copy_from_user(message, buffer, len);
   printk(KERN_INFO "buffer, %s\n", message);
   // sprintf(message, "%s(%d letters)", buffer, len);   // appending received string with its length
   // size_of_message = strlen(message);                 // store the length of the stored message
   printk(KERN_INFO "ATMELChar: Received %d characters from the user\n", len);

   return len;
}

/** 
 * Function - dev_release:
 * The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *
 * Parameters:
 *  inodep - A pointer to an inode object (defined in linux/fs.h)
 *  filep  - A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep)
{
   printk(KERN_INFO "ATMELChar: Device successfully closed\n");
   return 0;
}

/** 
 *  Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions.
 */
static struct file_operations fops =
{
	.owner = THIS_MODULE,
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/** 
 *  Module initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  Return:
 *   returns 0 if successful
 */

static void cleanup(int device_created)
{
   if (device_created) {
      device_destroy(char_drvClass, major);
      cdev_del(&mycdev);
   }
   if (char_drvClass) {
      class_destroy(char_drvClass);
   }
   if (major != -1) {
      unregister_chrdev_region(major, 1);
   }
}

static int __init char_drv_init(void)
{
   printk(KERN_INFO "ATMELChar: Initializing the ATMELChar LKM\n");

   int device_created = 0;

   // verified working
   if (alloc_chrdev_region(&major, 0, 1, NAME "_proc") < 0)
      goto error;

   if ((char_drvClass = class_create(THIS_MODULE, NAME "_sys")) == NULL)
      goto error;

   if (device_create(char_drvClass, NULL, major, NULL, NAME "_dev") == NULL )
      goto error;

   device_created = 1;
   cdev_init(&mycdev, &fops);

   if (cdev_add(&mycdev, major, 1) == -1)
      goto error;

   return 0;

error:
   printk(KERN_INFO "HubertModule: Something went wrong. Cleaning up\n");
   cleanup(device_created);

   return -1;

}

/** 
 * Module cleanup function
 * Similar to the initialization function, it is static. The __exit macro notifies that if this
 * code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit char_drv_exit(void)
{
   cleanup(1);
}

/** 
 * A module must use the module_init() module_exit() macros from linux/init.h, these 
 * macros identify the initialization function at module insertion and the cleanup function when
 * module is remove.
 */
module_init(char_drv_init);
module_exit(char_drv_exit);
