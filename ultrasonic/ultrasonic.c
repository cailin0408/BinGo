#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// UART defines
// By default the stdout UART is `uart0`
// #define UART_ID uart0
// #define BAUD_RATE 115200

// Pico W 接線定義
#define TRIG_PIN 2 // Pin 4 (GP2)
#define ECHO_PIN 3 // Pin 5 (GP3)

// 超音波測距函式
float get_distance_cm(void) {
    gpio_put(TRIG_PIN, 0);
    sleep_us(2);
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);

    uint32_t timeout = 30000; // 30ms 超時限制
    uint32_t start_time = time_us_32();

    // 等待 Echo 變高電位 (加上防死鎖迴圈)
    while (gpio_get(ECHO_PIN) == 0) {
        if (time_us_32() - start_time > timeout) {
            return -1.0f; // 超時，返回錯誤值
        }
    }
    uint32_t echo_start = time_us_32();

    // 等待 Echo 變低電位
    while (gpio_get(ECHO_PIN) == 1) {
        if (time_us_32() - start_time > timeout) {
            return -1.0f; // 超時，返回錯誤值
        }
    }
    uint32_t echo_end = time_us_32();

    return (float)(echo_end - echo_start) * 0.0343f / 2.0f;
}

int main()
{
    // 初始化 Pico 的標準 I/O (用於 USB Serial 印出診斷資訊)
    stdio_init_all();

    // Set up our UART
    // uart_init(UART_ID, BAUD_RATE);

    // 初始化 GPIO
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_put(TRIG_PIN, 0);

    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    sleep_ms(2000); // 等待 USB Serial 準備就緒

    while (1) {
        float distance = get_distance_cm();
        if (distance > 0.0f) {
            // 印出格式與 RPi 5 的 pico_serial_thread 匹配
            printf("Distance: %.2f cm\n", distance);
        } else {
            // 超時或讀取失敗時，印出預設跳過值
            printf("Distance: -1.00 cm\n");
        }
        // 關鍵修改：改為 50ms (20Hz) 採樣一次，確保微秒級的即時防撞反應
        sleep_ms(50); // 每50ms測量一次
    }
    return 0;
}
