#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "buzzer"
#define CLASS_NAME  "buzzer_class"

static struct gpio_desc *buzzer_gpio;
static int major_number;
static struct class *buzzer_class;
static struct device *buzzer_device;

static ssize_t buzzer_write(struct file *file,
                            const char __user *buffer,
                            size_t len,
                            loff_t *offset)
{
    char command;

    if (len < 1)
        return -EINVAL;

    if (copy_from_user(&command, buffer, 1))
        return -EFAULT;

    if (command == '1') {
        gpiod_set_value_cansleep(buzzer_gpio, 1);
        pr_info("buzzer: ON\n");
    }
    else if (command == '0') {
        gpiod_set_value_cansleep(buzzer_gpio, 0);
        pr_info("buzzer: OFF\n");
    }
    else {
        return -EINVAL;
    }

    return len;
}

static const struct file_operations buzzer_fops = {
    .owner = THIS_MODULE,
    .write = buzzer_write,
};

static int buzzer_probe(struct platform_device *pdev)
{
    int ret;

    pr_info("buzzer: probe started\n");

    buzzer_gpio = devm_gpiod_get(
        &pdev->dev,
        "buzzer",
        GPIOD_OUT_LOW
    );

    if (IS_ERR(buzzer_gpio)) {
        ret = PTR_ERR(buzzer_gpio);
        dev_err(&pdev->dev,
                "failed to get buzzer GPIO: %d\n",
                ret);
        return ret;
    }

    major_number =
        register_chrdev(0, DEVICE_NAME, &buzzer_fops);

    if (major_number < 0)
        return major_number;

    buzzer_class = class_create(CLASS_NAME);

    if (IS_ERR(buzzer_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(buzzer_class);
    }

    buzzer_device = device_create(
        buzzer_class,
        NULL,
        MKDEV(major_number, 0),
        NULL,
        DEVICE_NAME
    );

    if (IS_ERR(buzzer_device)) {
        class_destroy(buzzer_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(buzzer_device);
    }

    pr_info("buzzer: /dev/buzzer created\n");

    return 0;
}

static void buzzer_remove(struct platform_device *pdev)
{
    gpiod_set_value_cansleep(buzzer_gpio, 0);

    device_destroy(
        buzzer_class,
        MKDEV(major_number, 0)
    );

    class_destroy(buzzer_class);

    unregister_chrdev(
        major_number,
        DEVICE_NAME
    );

    pr_info("buzzer: removed\n");
}

static const struct of_device_id buzzer_of_match[] = {
    { .compatible = "bingo,buzzer" },
    { }
};

MODULE_DEVICE_TABLE(of, buzzer_of_match);

static struct platform_driver buzzer_driver = {
    .probe  = buzzer_probe,
    .remove = buzzer_remove,
    .driver = {
        .name = "bingo_buzzer",
        .of_match_table = buzzer_of_match,
    },
};

module_platform_driver(buzzer_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ben02160216");
MODULE_DESCRIPTION("BinGo Raspberry Pi 5 GPIO Buzzer Driver");
MODULE_VERSION("1.0");
