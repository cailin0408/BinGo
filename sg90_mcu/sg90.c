#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

#define SERVO_PIN 15

// 全域 PWM Slice 與 Channel
static uint g_slice_num;
static uint g_chan;

// 設定角度 (0° ~ 180°)，對應脈衝寬度 500us ~ 2500us[cite: 1]
void set_servo_angle(float angle)
{
    if (angle < 0.0f)
        angle = 0.0f;
    if (angle > 180.0f)
        angle = 180.0f;

    // 0° -> 500us, 180° -> 2500us[cite: 1]
    uint16_t pulse_width = 500 + (uint16_t)((angle / 180.0f) * 2000.0f);
    pwm_set_chan_level(g_slice_num, g_chan, pulse_width);
}

// 去除字串前後的空白與換行
void trim_str(char *str)
{
    char *p = str;
    int l = strlen(p);
    while (l > 0 && isspace((unsigned char)p[l - 1]))
        p[--l] = 0;
    while (*p && isspace((unsigned char)*p))
        p++, l--;
    memmove(str, p, l + 1);
}

int main()
{
    // 1. 初始化 USB Serial
    stdio_init_all();
    sleep_ms(2000); // 等待 USB 串口連線建立

    // 2. 初始化 PWM 設定 (50Hz，週期 20000us)[cite: 1]
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    g_slice_num = pwm_gpio_to_slice_num(SERVO_PIN);
    g_chan = pwm_gpio_to_channel(SERVO_PIN);

    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f);  // 125MHz / 125 = 1MHz (每個 tick 為 1us)
    pwm_config_set_wrap(&config, 20000 - 1); // 週期 20000us (50Hz)[cite: 1]
    pwm_init(g_slice_num, &config, true);

    // 預設歸零 (0 度)
    set_servo_angle(0.0f);

    printf("\n=== Pico W SG90 控制系統已就緒 (C SDK) ===\n");
    printf("請輸入指令 (例如: open, close, 或直接輸入角度數字 0~180):\n");

    char rx_buffer[64];
    int rx_idx = 0;

    // 3. 主迴圈：接收指令 (支援手勢指令與輸入)
    while (true)
    {
        int c = getchar_timeout_us(0); // 非阻塞讀取字元
        if (c != PICO_ERROR_TIMEOUT)
        {
            if (c == '\n' || c == '\r')
            {
                if (rx_idx > 0)
                {
                    rx_buffer[rx_idx] = '\0';
                    trim_str(rx_buffer);

                    // --- 1. 開蓋指令比對 (支援 open, OPEN_LID) ---[cite: 1, 2]
                    if (strcasecmp(rx_buffer, "open") == 0 || strcasecmp(rx_buffer, "OPEN_LID") == 0)
                    {
                        set_servo_angle(90.0f);
                        printf(">> 收到開蓋指令：轉至 90 度\n");
                    }
                    // --- 2. 關蓋指令比對 (支援 close, STOP, RETURN) ---[cite: 1, 2]
                    else if (strcasecmp(rx_buffer, "close") == 0 ||
                             strcasecmp(rx_buffer, "STOP") == 0 ||
                             strcasecmp(rx_buffer, "RETURN") == 0)
                    {
                        set_servo_angle(0.0f);
                        printf(">> 收到關閉/歸位指令：轉至 0 度\n");
                    }
                    // --- 3. 數字角度控制 (0~180) ---
                    else
                    {
                        char *endptr;
                        long angle = strtol(rx_buffer, &endptr, 10);
                        if (*endptr == '\0')
                        {
                            if (angle >= 0 && angle <= 180)
                            {
                                set_servo_angle((float)angle);
                                printf(">> 轉至自訂角度：%ld 度\n", angle);
                            }
                            else
                            {
                                printf("!! 角度超出範圍 (0~180)\n");
                            }
                        }
                        else
                        {
                            printf("!! 未知指令 [%s]\n", rx_buffer);
                        }
                    }
                    rx_idx = 0; // 清空緩衝區
                }
            }
            else if (rx_idx < (int)sizeof(rx_buffer) - 1)
            {
                rx_buffer[rx_idx++] = (char)c;
            }
        }
        sleep_ms(1);
    }

    return 0;
}