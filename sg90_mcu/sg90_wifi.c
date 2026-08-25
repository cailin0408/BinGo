#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/pwm.h"
#include "lwip/apps/mqtt.h"

// ================= 網路與硬體設定 =================
#define WIFI_SSID "iSpan-R309"       // 填入你的 Wi-Fi 名稱
#define WIFI_PASS "66316588"         // 填入你的 Wi-Fi 密碼
#define MQTT_BROKER_IP "192.168.69.144"  // 樹莓派 IP (MQTT Broker)

// #define WIFI_SSID "Yuki"       // 填入你的 Wi-Fi 名稱
// #define WIFI_PASS "0952719452"         // 填入你的 Wi-Fi 密碼
// #define MQTT_BROKER_IP "192.168.0.135"  // 樹莓派 IP (MQTT Broker)

#define SERVO_PIN 15                     // SG90 訊號線 GPIO

#define PWM_FREQ 50
#define MIN_DUTY 500   // 0度 (us)
#define MAX_DUTY 2500  // 180度 (us)

static mqtt_client_t* mqtt_client;

// ================= PWM 馬達控制 =================
void set_servo_angle(float angle) {
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;

    uint slice_num = pwm_gpio_to_slice_num(SERVO_PIN);
    uint32_t duty_us = MIN_DUTY + (uint32_t)((MAX_DUTY - MIN_DUTY) * (angle / 180.0f));
    uint32_t clock_freq = 125000000;
    uint32_t divider = 125;
    uint32_t wrap = (clock_freq / divider) / PWM_FREQ;
    uint32_t level = (wrap * duty_us) / 20000;

    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(SERVO_PIN), level);
    printf(">> 馬達轉至 %.1f 度 (Duty: %d us)\n", angle, duty_us);
}

// ================= MQTT Payload 處理 =================
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    char payload[64] = {0};
    if (len < sizeof(payload)) {
        memcpy(payload, data, len);
        payload[len] = '\0';
    }

    printf("📩 [Wi-Fi MQTT 收到指令]: %s\n", payload);

    if (strcasecmp(payload, "OPEN_LID") == 0 || strcasecmp(payload, "open") == 0) {
        set_servo_angle(90.0f); // 掀蓋 90 度
    } else if (strcasecmp(payload, "STOP") == 0 || strcasecmp(payload, "close") == 0) {
        set_servo_angle(0.0f);  // 關蓋 0 度
    }
}

static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    printf("📡 收到 Topic: %s (長度: %d)\n", topic, tot_len);
}

// 訂閱結果回應 Callback
static void mqtt_sub_request_cb(void *arg, err_t err) {
    if (err == ERR_OK) {
        printf("✅ MQTT Topic 訂閱確認成功！\n");
    } else {
        printf("❌ MQTT 訂閱失敗，錯誤碼: %d\n", err);
    }
}

// ================= MQTT 連線 Callback =================
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("✅ 成功連線至 Mosquitto MQTT Broker！\n");
        mqtt_set_inpub_callback(client, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, NULL);
        
        // 修改：傳入正確的 mqtt_sub_request_cb
        mqtt_sub_unsub(client, "bingo/command", 0, mqtt_sub_request_cb, NULL, 1);
        printf("📡 已發送訂閱請求 Topic: [bingo/command]\n");
    } else {
        printf("❌ MQTT 連線失敗，錯誤代碼: %d\n", status);
    }
}

// ================= 主程式 =================
int main() {
    stdio_init_all();
    sleep_ms(2000);

    // 1. 初始化 PWM
    gpio_set_function(SERVO_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(SERVO_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f);
    pwm_config_set_wrap(&config, 20000);
    pwm_init(slice_num, &config, true);
    set_servo_angle(0.0f); // 初期關蓋

    // 2. 初始化 Wi-Fi 晶片
    if (cyw43_arch_init()) {
        printf("❌ Wi-Fi 晶片初始化失敗\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    printf("⏳ 正在連接 Wi-Fi: %s ...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("❌ Wi-Fi 連線失敗！\n");
        return 1;
    }
    printf("✅ Wi-Fi 連線成功！\n");

    // 3. 連接 MQTT Broker
    ip_addr_t broker_ip;
    ipaddr_aton(MQTT_BROKER_IP, &broker_ip);

    mqtt_client = mqtt_client_new();
    struct mqtt_connect_client_info_t ci = {
        .client_id = "PicoW_Servo_Wifi_Client",
        .client_user = NULL,
        .client_pass = NULL,
        .keep_alive = 60
    };

    mqtt_client_connect(mqtt_client, &broker_ip, 1883, mqtt_connection_cb, NULL, &ci);

    // 點亮板載 LED 代表系統運作正常
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    // 4. 主迴圈
    while (true) {
        cyw43_arch_poll();
        sleep_ms(10); // 縮短輪詢間隔，讓 MQTT 接收反應更靈敏
    }

    return 0;
}