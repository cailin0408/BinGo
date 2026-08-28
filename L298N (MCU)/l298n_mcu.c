#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"

// ================= 1. Wi-Fi 與 MQTT 設定 =================
#define WIFI_SSID     "YOUR_WIFI_NAME"        // ⚠️ 你的 Wi-Fi 名稱
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"    // ⚠️ 你的 Wi-Fi 密碼
#define MQTT_BROKER_IP "100.75.196.124"        // ⚠️ 你的 Mosquitto Broker IP (樹莓派/筆電 IP)
#define MQTT_TOPIC     "bingo/command"

// ================= 2. GPIO 腳位設定 (Pico 物理腳位對應 GPIO) =================
#define GPIO_ENA  12  // 左馬達 Enable
#define GPIO_IN1  17
#define GPIO_IN2  27
#define GPIO_IN3  22
#define GPIO_IN4  23
#define GPIO_ENB  13  // 右馬達 Enable

static mqtt_client_t *mqtt_client;

// ================= 3. 馬達控制硬體 API (替代原本的 linux gpio_set_value) =================
static void set_motor_control(int ena, int in1, int in2, int in3, int in4, int enb) {
    gpio_put(GPIO_ENA, ena);
    gpio_put(GPIO_IN1, in1);
    gpio_put(GPIO_IN2, in2);
    gpio_put(GPIO_IN3, in3);
    gpio_put(GPIO_IN4, in4);
    gpio_put(GPIO_ENB, enb);
}

// 初始化馬達 GPIO 腳位 (替代原本的 l298n_init 中的 gpio_request)
static void init_motor_gpios(void) {
    uint gpios[] = {GPIO_ENA, GPIO_IN1, GPIO_IN2, GPIO_IN3, GPIO_IN4, GPIO_ENB};
    for (int i = 0; i < 6; i++) {
        gpio_init(gpios[i]);
        gpio_set_dir(gpios[i], GPIO_OUT);
        gpio_put(gpios[i], 0); // 預設全部拉低停止
    }
    printf("L298N Motor GPIOs Initialized.\n");
}

// 指令解析處理 (替代原本的 l298n_write)
static void process_command(const char *cmd, u16_t len) {
    if (strncmp(cmd, "COME", 4) == 0) {
        // 前進跟隨：ENA=1, IN1=1, IN2=0 / IN3=1, IN4=0, ENB=1
        set_motor_control(1, 1, 0, 1, 0, 1);
        printf("L298N Driver: Moving Forward (COME)\n");
    } else if (strncmp(cmd, "STOP", 4) == 0) {
        // 煞車停止：全低電位
        set_motor_control(0, 0, 0, 0, 0, 0);
        printf("L298N Driver: Stopped (STOP)\n");
    } else if (strncmp(cmd, "OPEN_LID", 8) == 0) {
        // 開蓋時馬達保持停止
        set_motor_control(0, 0, 0, 0, 0, 0);
        printf("L298N Driver: Lid Open Event (STOP Motors)\n");
    }
}

// ================= 4. MQTT 網路 Callbacks =================
// 收到 MQTT Payloads 時觸發
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    char buf[32] = {0};
    if (len < sizeof(buf)) {
        memcpy(buf, data, len);
        printf("📥 收到 MQTT 指令: %s\n", buf);
        process_command(buf, len);
    }
}

// 訂閱 Topic 結果 Callback
static void mqtt_sub_request_cb(void *arg, err_t err) {
    if (err == ERR_OK) {
        printf("✅ 成功訂閱 Topic: %s\n", MQTT_TOPIC);
    } else {
        printf("❌ 訂閱失敗, 錯誤碼: %d\n", err);
    }
}

// 連線 Broker 結果 Callback
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("✅ 已成功連線至 MQTT Broker!\n");
        // 設定接收資料的 Callback
        mqtt_set_inpub_callback(client, NULL, mqtt_incoming_data_cb, NULL);
        // 開始訂閱 Topic
        mqtt_sub_request_cb(client, mqtt_subscribe(client, MQTT_TOPIC, 0, mqtt_sub_request_cb, NULL));
    } else {
        printf("❌ MQTT 連線失敗, 狀態碼: %d\n", status);
    }
}

// 建立 MQTT 連線
static void start_mqtt(void) {
    ip_addr_t broker_ip;
    ip4addr_aton(MQTT_BROKER_IP, &broker_ip);

    mqtt_client = mqtt_client_new();
    struct mqtt_connect_client_info_t ci = {
        .client_id = "pico_w_l298n_car",
        .keep_alive = 60
    };

    printf("連線至 MQTT Broker (%s)...\n", MQTT_BROKER_IP);
    mqtt_client_connect(mqtt_client, &broker_ip, 1883, mqtt_connection_cb, NULL, &ci);
}

// ================= 5. 主程式入口 (main) =================
int main() {
    stdio_init_all();
    sleep_ms(2000); // 等待 Serial Monitor 連線

    // 1. 初始化 GPIO
    init_motor_gpios();

    // 2. 初始化 Pico W 晶片上的 Wi-Fi 模組
    if (cyw43_arch_init()) {
        printf("❌ CYW43 初始化失敗！\n");
        return 1;
    }

    cyw43_arch_enable_sta_mode();

    // 3. 連線 Wi-Fi
    printf("連線至 Wi-Fi: %s ...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("❌ Wi-Fi 連線失敗！\n");
        return 1;
    }
    printf("✅ Wi-Fi 已連線！\n");

    // 4. 啟動 MQTT 連線
    start_mqtt();

    // 5. 無窮迴圈 (背景處理網路封包)
    while (true) {
        #if PICO_CYW43_ARCH_POLL
        cyw43_arch_poll();
        #endif
        sleep_ms(10);
    }

    return 0;
}