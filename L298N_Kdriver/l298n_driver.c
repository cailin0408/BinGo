#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio/consumer.h>   // 現代 gpiod descriptor-based API
#include <linux/gpio/machine.h>    // gpiod_lookup_table / GPIO_LOOKUP
#include <linux/platform_device.h>
#include <linux/string.h>

#define DEVICE_NAME "l298n"
#define CLASS_NAME  "l298n_class"
#define PDEV_NAME   "l298n_pdev"

// RPi5 上 RP1 南橋晶片的 gpiochip label，用 `gpiodetect` 確認過是 "pinctrl-rp1"
#define GPIOCHIP_LABEL "pinctrl-rp1"

// 樹莓派 BCM GPIO 腳位（對應到 pinctrl-rp1 上的 line offset，Pi5 上兩者數字相同）
#define GPIO_ENA_OFFSET  16  // 左馬達 Enable
#define GPIO_IN1_OFFSET  17
#define GPIO_IN2_OFFSET  27
#define GPIO_IN3_OFFSET  22
#define GPIO_IN4_OFFSET  23
#define GPIO_ENB_OFFSET  13  // 右馬達 Enable

static int major_number;
static struct class*  l298n_class  = NULL;
static struct device* l298n_device = NULL;
static struct platform_device *l298n_pdev = NULL;

// GPIO descriptor（取代原本 legacy API 裡的整數腳位編號）
static struct gpio_desc *gpio_ena;
static struct gpio_desc *gpio_in1;
static struct gpio_desc *gpio_in2;
static struct gpio_desc *gpio_in3;
static struct gpio_desc *gpio_in4;
static struct gpio_desc *gpio_enb;

// 手動註冊 lookup table，把 con_id（"ena"/"in1"/...）對應到
// pinctrl-rp1 上的實際 line offset。這一步取代了原本 device tree
// 該做的工作——因為這個 kernel module 沒有搭配 device tree overlay，
// 沒有這個 lookup table 的話 gpiod_get() 會找不到對應的腳位。
static struct gpiod_lookup_table l298n_gpio_table = {
    .dev_id = PDEV_NAME,
    .table = {
        GPIO_LOOKUP(GPIOCHIP_LABEL, GPIO_ENA_OFFSET, "ena", GPIO_ACTIVE_HIGH),
        GPIO_LOOKUP(GPIOCHIP_LABEL, GPIO_IN1_OFFSET, "in1", GPIO_ACTIVE_HIGH),
        GPIO_LOOKUP(GPIOCHIP_LABEL, GPIO_IN2_OFFSET, "in2", GPIO_ACTIVE_HIGH),
        GPIO_LOOKUP(GPIOCHIP_LABEL, GPIO_IN3_OFFSET, "in3", GPIO_ACTIVE_HIGH),
        GPIO_LOOKUP(GPIOCHIP_LABEL, GPIO_IN4_OFFSET, "in4", GPIO_ACTIVE_HIGH),
        GPIO_LOOKUP(GPIOCHIP_LABEL, GPIO_ENB_OFFSET, "enb", GPIO_ACTIVE_HIGH),
        { },
    },
};

static int l298n_open(struct inode *inodep, struct file *filep) {
    return 0;
}

static int l298n_release(struct inode *inodep, struct file *filep) {
    return 0;
}

// 設置 GPIO 方向與使能狀態邏輯
static void set_motor_control(int ena, int in1, int in2, int in3, int in4, int enb) {
    gpiod_set_value(gpio_ena, ena);
    gpiod_set_value(gpio_in1, in1);
    gpiod_set_value(gpio_in2, in2);
    gpiod_set_value(gpio_in3, in3);
    gpiod_set_value(gpio_in4, in4);
    gpiod_set_value(gpio_enb, enb);

    pr_info("L298N Driver: set_motor_control ENA=%d IN1=%d IN2=%d IN3=%d IN4=%d ENB=%d\n",
            ena, in1, in2, in3, in4, enb);
}

static ssize_t l298n_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset) {
    char cmd[16] = {0};

    if (len > sizeof(cmd) - 1)
        len = sizeof(cmd) - 1;

    if (copy_from_user(cmd, buffer, len))
        return -EFAULT;

    pr_info("L298N Driver: raw received (%zu bytes): \"%s\"\n", len, cmd);

    if (strncmp(cmd, "COME", 4) == 0) {
        set_motor_control(1, 1, 0, 1, 0, 1);
        pr_info("L298N Driver: Moving Forward (COME)\n");
    } else if (strncmp(cmd, "STOP", 4) == 0) {
        set_motor_control(0, 0, 0, 0, 0, 0);
        pr_info("L298N Driver: Stopped (STOP)\n");
    } else if (strncmp(cmd, "OPEN_LID", 8) == 0) {
        set_motor_control(0, 0, 0, 0, 0, 0);
        pr_info("L298N Driver: Lid Open Event (STOP Motors)\n");
    } else {
        pr_warn("L298N Driver: Unknown command received: \"%s\"\n", cmd);
    }

    return len;
}

static struct file_operations fops = {
    .open = l298n_open,
    .write = l298n_write,
    .release = l298n_release,
};

static int __init l298n_init(void) {
    int ret;

    pr_info("L298N Driver: Initializing (gpiod descriptor API)\n");

    // 1. 註冊字元設備
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_err("L298N Driver: register_chrdev failed (%d)\n", major_number);
        return major_number;
    }

    // 2. 建立 device class
    l298n_class = class_create(CLASS_NAME);
    if (IS_ERR(l298n_class)) {
        pr_err("L298N Driver: class_create failed\n");
        ret = PTR_ERR(l298n_class);
        goto fail_chrdev;
    }

    // 3. 建立 /dev/l298n 節點
    l298n_device = device_create(l298n_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(l298n_device)) {
        pr_err("L298N Driver: device_create failed\n");
        ret = PTR_ERR(l298n_device);
        goto fail_class;
    }

    // 4. 建立一個「虛擬」platform_device，純粹當作 gpiod_get() 需要的
    //    struct device* 掛靠對象。沒有真的對應到硬體，只是給 gpiod
    //    lookup table 一個 dev_id 可以匹配。
    l298n_pdev = platform_device_register_simple(PDEV_NAME, -1, NULL, 0);
    if (IS_ERR(l298n_pdev)) {
        pr_err("L298N Driver: platform_device_register_simple failed\n");
        ret = PTR_ERR(l298n_pdev);
        goto fail_device;
    }

    // 5. 註冊 lookup table，把 "ena"/"in1"/... 對應到 pinctrl-rp1 的實際腳位
    gpiod_add_lookup_table(&l298n_gpio_table);

    // 6. 透過 gpiod_get() 依 con_id 取得 descriptor，並直接設成輸出、預設為 0
    gpio_ena = gpiod_get(&l298n_pdev->dev, "ena", GPIOD_OUT_LOW);
    if (IS_ERR(gpio_ena)) {
        ret = PTR_ERR(gpio_ena);
        pr_err("L298N Driver: gpiod_get(ena) failed: %d\n", ret);
        goto fail_lookup;
    }

    gpio_in1 = gpiod_get(&l298n_pdev->dev, "in1", GPIOD_OUT_LOW);
    if (IS_ERR(gpio_in1)) {
        ret = PTR_ERR(gpio_in1);
        pr_err("L298N Driver: gpiod_get(in1) failed: %d\n", ret);
        goto fail_ena;
    }

    gpio_in2 = gpiod_get(&l298n_pdev->dev, "in2", GPIOD_OUT_LOW);
    if (IS_ERR(gpio_in2)) {
        ret = PTR_ERR(gpio_in2);
        pr_err("L298N Driver: gpiod_get(in2) failed: %d\n", ret);
        goto fail_in1;
    }

    gpio_in3 = gpiod_get(&l298n_pdev->dev, "in3", GPIOD_OUT_LOW);
    if (IS_ERR(gpio_in3)) {
        ret = PTR_ERR(gpio_in3);
        pr_err("L298N Driver: gpiod_get(in3) failed: %d\n", ret);
        goto fail_in2;
    }

    gpio_in4 = gpiod_get(&l298n_pdev->dev, "in4", GPIOD_OUT_LOW);
    if (IS_ERR(gpio_in4)) {
        ret = PTR_ERR(gpio_in4);
        pr_err("L298N Driver: gpiod_get(in4) failed: %d\n", ret);
        goto fail_in3;
    }

    gpio_enb = gpiod_get(&l298n_pdev->dev, "enb", GPIOD_OUT_LOW);
    if (IS_ERR(gpio_enb)) {
        ret = PTR_ERR(gpio_enb);
        pr_err("L298N Driver: gpiod_get(enb) failed: %d\n", ret);
        goto fail_in4;
    }

    pr_info("L298N Driver: All GPIOs (gpiod) requested successfully via chip \"%s\" "
            "(ENA=%d IN1=%d IN2=%d IN3=%d IN4=%d ENB=%d)\n",
            GPIOCHIP_LABEL, GPIO_ENA_OFFSET, GPIO_IN1_OFFSET, GPIO_IN2_OFFSET,
            GPIO_IN3_OFFSET, GPIO_IN4_OFFSET, GPIO_ENB_OFFSET);

    return 0;

fail_in4:
    gpiod_put(gpio_in4);
fail_in3:
    gpiod_put(gpio_in3);
fail_in2:
    gpiod_put(gpio_in2);
fail_in1:
    gpiod_put(gpio_in1);
fail_ena:
    gpiod_put(gpio_ena);
fail_lookup:
    gpiod_remove_lookup_table(&l298n_gpio_table);
    platform_device_unregister(l298n_pdev);
fail_device:
    device_destroy(l298n_class, MKDEV(major_number, 0));
fail_class:
    class_destroy(l298n_class);
fail_chrdev:
    unregister_chrdev(major_number, DEVICE_NAME);
    return ret;
}

static void __exit l298n_exit(void) {
    // 離開前先讓馬達停止，再依序釋放資源
    gpiod_set_value(gpio_ena, 0);
    gpiod_set_value(gpio_in1, 0);
    gpiod_set_value(gpio_in2, 0);
    gpiod_set_value(gpio_in3, 0);
    gpiod_set_value(gpio_in4, 0);
    gpiod_set_value(gpio_enb, 0);

    gpiod_put(gpio_ena);
    gpiod_put(gpio_in1);
    gpiod_put(gpio_in2);
    gpiod_put(gpio_in3);
    gpiod_put(gpio_in4);
    gpiod_put(gpio_enb);

    gpiod_remove_lookup_table(&l298n_gpio_table);
    platform_device_unregister(l298n_pdev);

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
MODULE_DESCRIPTION("L298N Motor Driver with ENA/ENB Control for RPi 5 (gpiod descriptor API)");