#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/version.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eileen");
MODULE_DESCRIPTION("SG90 Servo Driver with Fast & Speed-Up Smooth modes for Raspberry Pi 5");

#define SERVO_PIN_NUM 26
#define DEVICE_NAME   "sg90_dev"
#define CLASS_NAME    "sg90"

static int major_number;
static int rp1_gpio_pin = -1;
static struct class  *sg90_class  = NULL;
static struct device *sg90_device = NULL;

// 記錄馬達當前的角度（預設 0 度）
static int current_angle = 0;

static int gpio_pin_param = 595;
module_param(gpio_pin_param, int, 0444);
MODULE_PARM_DESC(gpio_pin_param, "Kernel GPIO number for the servo signal pin");

/* -------------------------------------------------------------
 * 1. 原本做法：全速爆衝 (Direct Jump)
 * 用於對比展示：馬達全速跳到位（蓋子容易噴飛）。
 * ------------------------------------------------------------- */
static void set_servo_angle(int angle) {
    int pulse_us;
    int i;

    if (rp1_gpio_pin < 0) return;

    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    pulse_us = 600 + (angle * 1000 / 90);

    for (i = 0; i < 50; i++) {
        gpio_set_value(rp1_gpio_pin, 1);
        udelay(pulse_us);
        gpio_set_value(rp1_gpio_pin, 0);
        usleep_range(20000 - pulse_us - 50, 20000 - pulse_us + 50);
    }

    current_angle = angle;
    printk(KERN_INFO "SG90 [FAST]: Set angle directly to %d\n", angle);
}

/* -------------------------------------------------------------
 * 2. 加速版平滑做法：加大步階與微縮延遲
 * 每次跨越 3 度，脈衝週期減為 1 次，反應極快且有緩和效果。
 * ------------------------------------------------------------- */
static void set_servo_angle_smooth(int target_angle, int delay_ms) {
    int angle;
    int pulse_us;

    if (rp1_gpio_pin < 0) return;

    if (target_angle < 0) target_angle = 0;
    if (target_angle > 180) target_angle = 180;

    // 將單步跨度加大為 3 度 (大幅提升移動速度)
    int step = (target_angle > current_angle) ? 3 : -3;

    for (angle = current_angle; (step > 0 ? angle <= target_angle : angle >= target_angle); angle += step) {
        pulse_us = 600 + (angle * 1000 / 90);

        // 每個步階僅輸出 1 個 PWM 脈衝
        gpio_set_value(rp1_gpio_pin, 1);
        udelay(pulse_us);
        gpio_set_value(rp1_gpio_pin, 0);
        usleep_range(20000 - pulse_us - 50, 20000 - pulse_us + 50);

        if (delay_ms > 0) {
            msleep(delay_ms);
        }
    }

    current_angle = target_angle;
    printk(KERN_INFO "SG90 [SMOOTH]: Speed-up moved to angle %d\n", current_angle);
}

static int dev_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    char kbuf[16];
    long angle;

    if (len > sizeof(kbuf) - 1) len = sizeof(kbuf) - 1;
    if (copy_from_user(kbuf, buffer, len)) return -EFAULT;
    kbuf[len] = '\0';

    if (strncasecmp(kbuf, "open", 4) == 0) {
        // 延遲設為 2ms，開蓋快速俐落且不爆衝
        set_servo_angle_smooth(90, 2);
    } else if (strncasecmp(kbuf, "close", 5) == 0) {
        // 延遲設為 1ms，關蓋快速到位
        set_servo_angle_smooth(0, 1);
    } else if (strncasecmp(kbuf, "fast_open", 9) == 0) {
        // 對比展示：原本的全速爆衝開蓋
        set_servo_angle(90);
    } else if (strncasecmp(kbuf, "fast_close", 10) == 0) {
        // 對比展示：原本的全速爆衝關蓋
        set_servo_angle(0);
    } else if (strncasecmp(kbuf, "test", 4) == 0) {
        if (rp1_gpio_pin >= 0) {
            gpio_direction_output(rp1_gpio_pin, 1);
            printk(KERN_INFO "SG90: TEST MODE - pin %d held HIGH\n", rp1_gpio_pin);
        }
    } else if (strncasecmp(kbuf, "test0", 5) == 0) {
        if (rp1_gpio_pin >= 0) {
            gpio_direction_output(rp1_gpio_pin, 0);
            printk(KERN_INFO "SG90: TEST MODE - pin %d held LOW\n", rp1_gpio_pin);
        }
    } else if (kstrtol(kbuf, 10, &angle) == 0) {
        set_servo_angle_smooth((int)angle, 2);
    } else {
        printk(KERN_WARNING "SG90: Invalid command\n");
    }

    return len;
}

static struct file_operations fops = {
    .open  = dev_open,
    .write = dev_write,
};

static int __init sg90_init(void) {
    int ret;

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "SG90: Failed to register major number\n");
        return major_number;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    sg90_class = class_create(CLASS_NAME);
#else
    sg90_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(sg90_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "SG90: Failed to create class\n");
        return PTR_ERR(sg90_class);
    }

    sg90_device = device_create(sg90_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(sg90_device)) {
        class_destroy(sg90_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ALERT "SG90: Failed to create device\n");
        return PTR_ERR(sg90_device);
    }

    rp1_gpio_pin = gpio_pin_param;
    printk(KERN_INFO "SG90: Using GPIO pin: %d\n", rp1_gpio_pin);

    gpio_free(rp1_gpio_pin);
    ret = gpio_request(rp1_gpio_pin, "SG90_PWM_PIN");
    if (ret) {
        printk(KERN_ALERT "SG90: Failed to request GPIO %d (err: %d)\n", rp1_gpio_pin, ret);
        device_destroy(sg90_class, MKDEV(major_number, 0));
        class_destroy(sg90_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    gpio_direction_output(rp1_gpio_pin, 0);

    printk(KERN_INFO "SG90: Driver registered with Major %d on RP1 GPIO %d\n", major_number, rp1_gpio_pin);
    return 0;
}

static void __exit sg90_exit(void) {
    if (rp1_gpio_pin >= 0) {
        gpio_set_value(rp1_gpio_pin, 0);
        gpio_free(rp1_gpio_pin);
    }
    if (sg90_device)
        device_destroy(sg90_class, MKDEV(major_number, 0));
    if (sg90_class)
        class_destroy(sg90_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "SG90: Driver unregistered\n");
}

module_init(sg90_init);
module_exit(sg90_exit);