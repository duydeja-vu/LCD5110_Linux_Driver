#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include "lcd_device.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NGUYEN DUC DUY");
MODULE_DESCRIPTION("NOKIA LCD5110 Driver for Linux");
MODULE_VERSION("0.1");

typedef struct _lcd_device
{
	unsigned char *control_regs;
	unsigned char *status_regs;
	unsigned char *data_regs;
} lcd_device;

struct _lcd_driver
{
	dev_t dev_num;
	struct class *dev_class;
	struct device *dev;
	lcd_device *lcd_hw;
	struct cdev *vcdev;
	unsigned int open_count;

} lcd_driver;

static int lcd_driver_open(struct inode *inode, struct file *filp)
{
	lcd_driver.open_count++;
	printk(KERN_INFO "Open count %d \n", lcd_driver.open_count);
	return 0;
}

static int lcd_driver_release(struct inode *inode, struct file *flip)
{
	printk(KERN_INFO "Release driver\n");
	return 0;
}

static ssize_t lcd_driver_read(struct file *flip, char __user *user_buf, ssize_t len, loff_t *off)
{
	char *kernel_buf = NULL;
	int num_bytes = 0;
	printk(KERN_INFO "Handle read data from use\n");
	kernel_buf = kzalloc(len, GFP_KERNEL);
	if (kernel_buf == NULL)
	{
		return 0;
	}

	num_bytes = lcd_hw_read_data(lcd_driver.lcd_hw, *off, len, kernel_buf);
	printk(KERN_INFO "Read %d bytes from lcd hardwware\n", num_bytes);
	if (num_bytes < 0)
	{
		return -EFAULT;
	}
	if (copy_to_user(user_buf, kernel_buf, num_bytes))
	{
		return -EFAULT;
	}
	*off += num_bytes;
	return num_bytes;
}

static struct file_operations fops =
	{
		.owner = THIS_MODULE,
		.open = lcd_driver_open,
		.release = lcd_driver_release,
};

int lcd_hw_init(lcd_device *lcd_hw)
{
	printk(KERN_INFO "In lcd_hw_init");
	char *buf;
	buf = kzalloc(NUM_DEV_REGS * REG_SIZE, GFP_KERNEL);
	if (!buf)
	{
		return -ENOMEM;
	}
	lcd_hw->control_regs = buf;
	lcd_hw->status_regs = lcd_hw->control_regs + NUM_CTRL_REGS;
	lcd_hw->data_regs = lcd_hw->status_regs + NUM_STS_REGS;

	lcd_hw->control_regs[CONTROL_ACCESS_REG] = 0x03;
	lcd_hw->data_regs[DEVICE_STATUS_REG] = 0x03;

	return 0;
}

void lcd_hw_exit(lcd_device *lcd_hw)
{
	printk(KERN_INFO "In lcd_hw_exit");
	kfree(lcd_hw->control_regs);
}

static int __init lcd_driver_init(void)
{
	int ret = 0;
	lcd_driver.dev_num = 0;
	ret = alloc_chrdev_region(&lcd_driver.dev_num, 0, 1, "Device Number");
	if (ret < 0)
	{
		printk("Failed to register device number\n");
		goto failed_register_devnum;
	}
	printk(KERN_INFO "Allocated device number (%d, %d)\n ", MAJOR(lcd_driver.dev_num), MINOR(lcd_driver.dev_num));

	lcd_driver.dev_class = class_create(THIS_MODULE, "Class LCD Device");
	if (lcd_driver.dev_class == NULL)
	{
		printk("Failed to create a device class\n");
		goto failed_create_class;
	}

	lcd_driver.dev = device_create(lcd_driver.dev_class, NULL, lcd_driver.dev_num, NULL, "LCD5110");
	if (IS_ERR(lcd_driver.dev))
	{
		printk(KERN_INFO "Failed to create a device file\n");
		goto failed_create_device;
	}

	lcd_driver.lcd_hw = kzalloc(sizeof(lcd_device), GFP_KERNEL);
	if (!lcd_driver.lcd_hw)
	{
		printk(KERN_INFO "Failed to allocate data structe of the driver");
		ret = -ENOMEM;
		goto failed_allocate_structure;
	}

	ret = lcd_hw_init(lcd_driver.lcd_hw);
	if (ret < 0)
	{
		printk(KERN_INFO "Failed to init LCD hardware\n");
		goto failed_init_lcd_hw;
	}

	lcd_driver.vcdev = cdev_alloc();
	if (lcd_driver.vcdev == NULL)
	{
		printk(KERN_INFO "Failed to allocate cdev structure\n");
		goto failed_alloc_cdev;
	}

	cdev_init(lcd_driver.vcdev, &fops);

	ret = cdev_add(lcd_driver.vcdev, lcd_driver.dev_num, 1);
	if (ret < 0)
	{
		printk(KERN_INFO "Failed to add cdev structure\n");
		goto failed_alloc_cdev;
	}

	printk(KERN_INFO "Init LCD5110 driver successfully\n");
	return 0;

failed_alloc_cdev:
	lcd_hw_exit(lcd_driver.lcd_hw);
failed_init_lcd_hw:
	kfree(lcd_driver.lcd_hw);
failed_allocate_structure:
	device_destroy(lcd_driver.dev_class, lcd_driver.dev);
failed_create_device:
	class_destroy(lcd_driver.dev_class);
failed_create_class:
	unregister_chrdev_region(lcd_driver.dev_num, 1);
failed_register_devnum:
	return ret;
}

static void __exit lcd_driver_exit(void)
{

	lcd_hw_exit(lcd_driver.lcd_hw);
	kfree(lcd_driver.lcd_hw);
	device_destroy(lcd_driver.dev_class, lcd_driver.dev_num);
	class_destroy(lcd_driver.dev_class);
	unregister_chrdev_region(lcd_driver.dev_num, 1);
	printk(KERN_INFO "Removed LCD5110 Driver\n");
}

module_init(lcd_driver_init);
module_exit(lcd_driver_exit);
