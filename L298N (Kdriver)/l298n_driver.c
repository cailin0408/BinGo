#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>  // 修正：引進傳統 integer-based GPIO API (gpio_request, gpio_set_value)
#include <linux/platform_device.h>

#define DEVICE_NAME "l298n"
#define CLASS_NAME  "l298n_class"

// 定義樹莓派 BCM GPIO 腳位
#define GPIO_ENA  12  // 左馬達 Enable
#define GPIO_IN1  17
#define GPIO_IN2  27
#define GPIO_IN3  22
#define GPIO_IN4  23
#define GPIO_ENB  13  // 右馬達 Enable

static int major_number;
static struct class*  l298n_class  = NULL;
static struct device* l298n_device = NULL;

static int l298n_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static int l298n_release(struct inode *inodep, struct file *filep) {
    return 0;
}

// 設置 GPIO 方向與使能狀態邏輯
static void set_motor_control(int ena, int in1, int in2, int in3, int in4, int enb) {
    gpio_set_value(GPIO_ENA, ena);
    gpio_set_value(GPIO_IN1, in1);
    gpio_set_value(GPIO_IN2, in2);
    gpio_set_value(GPIO_IN3, in3);
    gpio_set_value(GPIO_IN4, in4);
    gpio_set_value(GPIO_ENB, enb);
}

static ssize_t l298n_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    char cmd[16] = {0};

    if (len > sizeof(cmd) - 1)
        len = sizeof(cmd) - 1;

    if (copy_from_user(cmd, buffer, len))
        return -EFAULT;

    // 處理指令
    if (strncmp(cmd, "COME", 4) == 0) {
        // 前進跟隨：ENA=1, IN1=1, IN2=0 / IN3=1, IN4=0, ENB=1
        set_motor_control(1, 1, 0, 1, 0, 1);
        pr_info("L298N Driver: Moving Forward (COME)\n");
    } else if (strncmp(cmd, "STOP", 4) == 0) {
        // 煞車停止：全低電位
        set_motor_control(0, 0, 0, 0, 0, 0);
        pr_info("L298N Driver: Stopped (STOP)\n");
    } else if (strncmp(cmd, "OPEN_LID", 8) == 0) {
        // 開蓋時馬達保持停止
        set_motor_control(0, 0, 0, 0, 0, 0);
        pr_info("L298N Driver: Lid Open Event (STOP Motors)\n");
    }

    return len;
}

static struct file_operations fops = {
    .open = l298n_open,
    .write = l298n_write,
    .release = l298n_release,
};

static int __init l298n_init(void) {
    pr_info("L298N Driver: Initializing\n");

    // 1. 註冊字元設備
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) return major_number;

    // 2. 建立 device class (修正：相容 Linux 6.4+ API，移除第一個參數 THIS_MODULE)
    l298n_class = class_create(CLASS_NAME);
    if (IS_ERR(l298n_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(l298n_class);
    }

    // 3. 建立 /dev/l298n 節點
    l298n_device = device_create(l298n_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(l298n_device)) {
        class_destroy(l298n_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(l298n_device);
    }

    // 4. 申請 GPIO (包含 ENA / ENB)
    gpio_request(GPIO_ENA, "ENA"); gpio_direction_output(GPIO_ENA, 0);
    gpio_request(GPIO_IN1, "IN1"); gpio_direction_output(GPIO_IN1, 0);
    gpio_request(GPIO_IN2, "IN2"); gpio_direction_output(GPIO_IN2, 0);
    gpio_request(GPIO_IN3, "IN3"); gpio_direction_output(GPIO_IN3, 0);
    gpio_request(GPIO_IN4, "IN4"); gpio_direction_output(GPIO_IN4, 0);
    gpio_request(GPIO_ENB, "ENB"); gpio_direction_output(GPIO_ENB, 0);

    return 0;
}

static void __exit l298n_exit(void) {
    // 清除 GPIO 與裝置
    gpio_set_value(GPIO_ENA, 0); gpio_free(GPIO_ENA);
    gpio_set_value(GPIO_IN1, 0); gpio_free(GPIO_IN1);
    gpio_set_value(GPIO_IN2, 0); gpio_free(GPIO_IN2);
    gpio_set_value(GPIO_IN3, 0); gpio_free(GPIO_IN3);
    gpio_set_value(GPIO_IN4, 0); gpio_free(GPIO_IN4);
    gpio_set_value(GPIO_ENB, 0); gpio_free(GPIO_ENB);

    device_destroy(l298n_class, MKDEV(major_number, 0));
    class_unregister(l298n_class);
    class_destroy(l298n_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("L298N Driver: Unloaded\n");
}

module_init(l298n_init);
module_exit(l298n_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BinGo Team");
MODULE_DESCRIPTION("L298N Motor Driver with ENA/ENB Control for RPi 5");