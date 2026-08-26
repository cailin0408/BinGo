#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eileen");
MODULE_DESCRIPTION("SG90 Servo Motor Driver for Raspberry Pi");

static int major_number;

static int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "SG90: Device opened\n");
    return 0;
}

static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    char kbuf[16];
    if (len > sizeof(kbuf) - 1) len = sizeof(kbuf) - 1;
    
    // 將數據從 User Space 複製到 Kernel Space
    if (copy_from_user(kbuf, buffer, len)) return -EFAULT;
    kbuf[len] = '\0';

    // TODO: 解析 kbuf 中的指令/角度，並呼叫 Linux PWM 控制 API
    printk(KERN_INFO "SG90: Received command %s\n", kbuf);
    return len;
}

static struct file_operations fops = {
    .open = dev_open,
    .write = dev_write,
};

static int __init sg90_init(void) {
    major_number = register_chrdev(0, "sg90_dev", &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "SG90: Failed to register a major number\n");
        return major_number;
    }
    printk(KERN_INFO "SG90: Driver registered with major number %d\n", major_number);
    return 0;
}

static void __exit sg90_exit(void) {
    unregister_chrdev(major_number, "sg90_dev");
    printk(KERN_INFO "SG90: Driver unregistered\n");
}

module_init(sg90_init);
module_exit(sg90_exit);