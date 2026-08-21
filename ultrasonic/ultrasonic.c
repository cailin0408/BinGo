#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

// UART defines
// By default the stdout UART is `uart0`
#define UART_ID uart0
#define BAUD_RATE 115200

// Pico W 接線定義
#define TRIG_PIN 2 // Pin 4 (GP2)
#define ECHO_PIN 3 // Pin 5 (GP3)

// 超音波測距函式
float get_distance_cm() {
    gpio_put(TRIG_PIN, 1);
    sleep_us(10);
    gpio_put(TRIG_PIN, 0);

    while (gpio_get(ECHO_PIN) == 0);
    uint32_t start = time_us_32();
    while (gpio_get(ECHO_PIN) == 1);
    uint32_t end = time_us_32();

    return (float)(end - start) * 0.0343f / 2.0f;
}

int main()
{
    // 初始化 Pico 的標準 I/O (用於 USB Serial 印出診斷資訊)
    stdio_init_all();

    // Set up our UART
    uart_init(UART_ID, BAUD_RATE);

    // 初始化 GPIO
    gpio_init(TRIG_PIN);
    gpio_set_dir(TRIG_PIN, GPIO_OUT);
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);

    while (1) {
        float distance = get_distance_cm();
        printf("Distance: %.2f cm\n", distance); // 透過 UART 傳給樹莓派
        sleep_ms(1000); // 每秒測量一次
    }
}
