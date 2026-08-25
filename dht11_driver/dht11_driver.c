#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/string.h>

#define DEVICE_NAME "dht11"
#define CLASS_NAME  "dht11_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Systems Project");
MODULE_DESCRIPTION("RPi 5 DHT11 IPC Kernel Driver Bridge");
MODULE_VERSION("1.0");

static int majorNumber;
static struct class* dht11Class = NULL;
static struct device* dht11Device = NULL;
static char dht11_buffer[128] = "Temp: 0.0 C, Humidity: 0%\n";

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char __user *, size_t, loff_t *);

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset) {
    int str_len = strlen(dht11_buffer);
    if (*offset >= str_len) return 0;

    if (len > str_len - *offset) {
        len = str_len - *offset;
    }

    if (copy_to_user(buffer, dht11_buffer + *offset, len)) {
        return -EFAULT;
    }

    *offset += len;
    return len;
}

static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    if (len >= sizeof(dht11_buffer)) len = sizeof(dht11_buffer) - 1;

    if (copy_from_user(dht11_buffer, buffer, len)) {
        return -EFAULT;
    }

    dht11_buffer[len] = '\0';
    return len;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    return 0;
}

static int __init dht11_init(void) {
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber < 0) return majorNumber;

    dht11Class = class_create(CLASS_NAME);
    if (IS_ERR(dht11Class)) {
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(dht11Class);
    }

    dht11Device = device_create(dht11Class, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(dht11Device)) {
        class_destroy(dht11Class);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        return PTR_ERR(dht11Device);
    }

    printk(KERN_INFO "DHT11: Driver registered at /dev/%s\n", DEVICE_NAME);
    return 0;
}

static void __exit dht11_exit(void) {
    device_destroy(dht11Class, MKDEV(majorNumber, 0));
    class_unregister(dht11Class);
    class_destroy(dht11Class);
    unregister_chrdev(majorNumber, DEVICE_NAME);
    printk(KERN_INFO "DHT11: Driver unregistered\n");
}

module_init(dht11_init);
module_exit(dht11_exit);
