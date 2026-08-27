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
MODULE_DESCRIPTION("SG90 Servo Driver for Raspberry Pi 5");

#define SERVO_PIN_NUM 26
#define DEVICE_NAME   "sg90_dev"
#define CLASS_NAME    "sg90"

static int major_number;
static int rp1_gpio_pin = -1;
static struct class  *sg90_class  = NULL;
static struct device *sg90_device = NULL;

/* -----------------------------------------------------------------
 * gpiochip_find() 在近期 kernel（含你目前的 6.18）已經被移除，
 * legacy 整數式 GPIO API 沒有簡單的替代函式可以在模組內動態列舉
 * gpiochip。改用 module parameter 讓你在 insmod 時直接指定實際
 * 腳位號碼。  我要這個幹嘛?
 *
 * 查詢方式：
 *   for f in /sys/class/gpio/gpiochip*; do
 *     echo "$f label=$(cat $f/label) base=$(cat $f/base)"
 *   done
 * 找到 label=pinctrl-rp1 那一行的 base，加上想控制的 BCM 腳位號碼
 * （例如 GPIO26）即為正確編號。在這台機器上實測結果是 base=569，
 * 所以 GPIO26 對應到的正確編號是 595（已用 open/close 驗證伺服
 * 馬達會正確轉動）。base 值理論上由 kernel 開機時動態分配，若
 * 之後系統更新、kernel 版本更換，仍建議用上面指令重新查一次，
 * 不要完全依賴這個預設值。
 *
 * 用法： sudo insmod sg90_driver.ko gpio_pin_param=595
 * --------------------------------------------------------------- */
static int gpio_pin_param = 595;
module_param(gpio_pin_param, int, 0444);
MODULE_PARM_DESC(gpio_pin_param, "Kernel GPIO number for the servo signal pin (find via /sys/class/gpio/gpiochip*/base, label=pinctrl-rp1)");

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
        /* 週期中剩餘的部分交給 usleep_range，
         * 避免長時間忙等佔用 CPU / 被中斷打斷造成抖動 */
        usleep_range(20000 - pulse_us - 50, 20000 - pulse_us + 50);
    }
    printk(KERN_INFO "SG90: Set angle to %d (Pulse: %d us)\n", angle, pulse_us);
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
        set_servo_angle(90);
    } else if (strncasecmp(kbuf, "close", 5) == 0) {
        set_servo_angle(0);
    } else if (strncasecmp(kbuf, "test", 4) == 0) {
        /* debug：持續輸出高電位。
         * 注意：伺服馬達靠脈衝寬度判斷角度，持續高電位不是合法的
         * PWM 訊號，馬達通常不會轉動——這不代表 GPIO 錯誤，只適合
         * 用電表量測腳位是否真的有電位變化。要驗證馬達動作請用
         * "open"/"close" 或直接輸入角度數字。 */
        if (rp1_gpio_pin >= 0) {
            gpio_direction_output(rp1_gpio_pin, 1);
            printk(KERN_INFO "SG90: TEST MODE - pin %d held HIGH\n", rp1_gpio_pin);
        }
    } else if (strncasecmp(kbuf, "test0", 5) == 0) {
        /* debug：持續輸出低電位 */
        if (rp1_gpio_pin >= 0) {
            gpio_direction_output(rp1_gpio_pin, 0);
            printk(KERN_INFO "SG90: TEST MODE - pin %d held LOW\n", rp1_gpio_pin);
        }
    } else if (kstrtol(kbuf, 10, &angle) == 0) {
        set_servo_angle((int)angle);
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

    /* 1. 註冊字元裝置，取得 major number */
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "SG90: Failed to register major number\n");
        return major_number;
    }

    /* 2. 建立 class / device，讓 udev 自動生成 /dev/sg90_dev
     *    這是原本程式最容易讓人卡住的地方：
     *    沒有這兩步，/dev/sg90_dev 根本不會被建立。 */
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

    /* 3. 決定要控制的 GPIO 編號。
     *    優先使用 insmod 時傳入的 gpio_pin_param（預設 595，已用
     *    open/close 實測驗證伺服馬達正確轉動）。若之後系統更新
     *    導致 base 改變，重新用 gpiochip 底下的 base 檔案查
     *    出新編號，帶入 gpio_pin_param= 覆蓋即可，不需要重編。 */
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