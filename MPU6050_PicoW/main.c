#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "hardware/i2c.h"

#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/apps/mqtt.h"

// =====================================================
// Wi-Fi 設定
// =====================================================

#define WIFI_SSID "iPhone"
#define WIFI_PASSWORD "22222222"

// =====================================================
// MQTT 設定
// =====================================================

// Raspberry Pi 5 / Mosquitto Broker
#define MQTT_BROKER_IP "172.20.10.4"

#define MQTT_BROKER_PORT 1883

#define MQTT_TOPIC "garbage/pico/status"

// =====================================================
// MPU6050 / I2C 設定
// =====================================================

#define I2C_PORT i2c0

// Pico W
// GP4 -> SDA
// GP5 -> SCL
#define SDA_PIN 4
#define SCL_PIN 5

// MPU6050 預設 Address
#define MPU6050_ADDR 0x68

// MPU6050 Register
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B

// =====================================================
// MQTT Global
// =====================================================

static mqtt_client_t *mqtt_client = NULL;

static volatile bool mqtt_connected = false;

// =====================================================
// MQTT Publish Callback
// =====================================================

static void mqtt_publish_callback(
    void *arg,
    err_t err)
{
    if (err == ERR_OK)
    {
        printf("[MQTT] Publish OK\n");
    }
    else
    {
        printf(
            "[MQTT] Publish FAILED err=%d\n",
            err);
    }
}

// =====================================================
// MQTT Connection Callback
// =====================================================

static void mqtt_connection_callback(
    mqtt_client_t *client,
    void *arg,
    mqtt_connection_status_t status)
{
    if (status == MQTT_CONNECT_ACCEPTED)
    {
        mqtt_connected = true;

        printf("\n");
        printf("========================================\n");
        printf("[MQTT] CONNECTED!\n");
        printf("========================================\n");
    }
    else
    {
        mqtt_connected = false;

        printf("\n");
        printf("========================================\n");
        printf(
            "[MQTT] DISCONNECTED status=%d\n",
            status);
        printf("========================================\n");
    }
}

// =====================================================
// MPU6050 Write
// =====================================================

static int mpu6050_write(
    uint8_t reg,
    uint8_t value)
{
    uint8_t data[2];

    data[0] = reg;
    data[1] = value;

    int result =
        i2c_write_blocking(
            I2C_PORT,
            MPU6050_ADDR,
            data,
            2,
            false);

    return result;
}

// =====================================================
// MPU6050 Read Accelerometer
// =====================================================

static bool mpu6050_read_accel(
    int16_t *accel_x,
    int16_t *accel_y,
    int16_t *accel_z)
{
    uint8_t reg =
        ACCEL_XOUT_H;

    uint8_t data[6];

    // 告訴 MPU6050 從 0x3B 開始讀
    int write_result =
        i2c_write_blocking(
            I2C_PORT,
            MPU6050_ADDR,
            &reg,
            1,
            true);

    if (write_result != 1)
    {
        printf(
            "[MPU6050] Register select FAILED: %d\n",
            write_result);

        return false;
    }

    // 讀 X/Y/Z，共 6 bytes
    int read_result =
        i2c_read_blocking(
            I2C_PORT,
            MPU6050_ADDR,
            data,
            6,
            false);

    if (read_result != 6)
    {
        printf(
            "[MPU6050] Read FAILED: %d\n",
            read_result);

        return false;
    }

    // High Byte + Low Byte
    *accel_x =
        (int16_t)(((uint16_t)data[0] << 8) |
                  data[1]);

    *accel_y =
        (int16_t)(((uint16_t)data[2] << 8) |
                  data[3]);

    *accel_z =
        (int16_t)(((uint16_t)data[4] << 8) |
                  data[5]);

    return true;
}

// =====================================================
// main
// =====================================================

int main(void)
{
    // =================================================
    // USB Serial
    // =====================================================

    stdio_init_all();

    sleep_ms(3000);

    printf("\n");
    printf("========================================\n");
    printf("Pico W + MPU6050 + MQTT\n");
    printf("========================================\n");

    // =================================================
    // STEP 1：初始化 Wi-Fi
    // =====================================================

    printf("[STEP 1] Initializing WiFi...\n");

    int init_result =
        cyw43_arch_init();

    printf(
        "[STEP 1] cyw43_arch_init = %d\n",
        init_result);

    if (init_result != 0)
    {
        printf("[ERROR] WiFi init FAILED\n");

        while (true)
        {
            sleep_ms(1000);
        }
    }

    cyw43_arch_enable_sta_mode();

    printf("[STEP 1] WiFi init OK\n");

    // =================================================
    // STEP 2：連接 iPhone
    // =====================================================

    printf(
        "[STEP 2] Connecting to %s...\n",
        WIFI_SSID);

    int wifi_result =
        cyw43_arch_wifi_connect_timeout_ms(
            WIFI_SSID,
            WIFI_PASSWORD,
            CYW43_AUTH_WPA2_AES_PSK,
            30000);

    printf(
        "[STEP 2] WiFi result = %d\n",
        wifi_result);

    if (wifi_result != 0)
    {
        printf(
            "[ERROR] WiFi FAILED err=%d\n",
            wifi_result);

        while (true)
        {
            cyw43_arch_poll();
            sleep_ms(100);
        }
    }

    // =================================================
    // STEP 3：顯示 Pico IP
    // =====================================================

    cyw43_arch_poll();

    struct netif *netif =
        &cyw43_state.netif[CYW43_ITF_STA];

    printf("\n");
    printf("========================================\n");
    printf("WIFI = CONNECTED!\n");

    printf(
        "Pico IP = %s\n",
        ip4addr_ntoa(
            netif_ip4_addr(netif)));

    printf("========================================\n");

    // =================================================
    // STEP 4：初始化 MPU6050 I2C
    // =====================================================

    printf("[STEP 4] Initializing I2C...\n");

    i2c_init(
        I2C_PORT,
        100 * 1000);

    gpio_set_function(
        SDA_PIN,
        GPIO_FUNC_I2C);

    gpio_set_function(
        SCL_PIN,
        GPIO_FUNC_I2C);

    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    printf(
        "[STEP 4] I2C ready: SDA=GP%d SCL=GP%d\n",
        SDA_PIN,
        SCL_PIN);

    // =================================================
    // STEP 5：喚醒 MPU6050
    // =====================================================

    printf("[STEP 5] Starting MPU6050...\n");

    int mpu_result =
        mpu6050_write(
            PWR_MGMT_1,
            0x00);

    if (mpu_result != 2)
    {
        printf(
            "[ERROR] MPU6050 init FAILED result=%d\n",
            mpu_result);

        while (true)
        {
            cyw43_arch_poll();
            sleep_ms(1000);
        }
    }

    sleep_ms(100);

    printf("[STEP 5] MPU6050 READY\n");

    // =================================================
    // STEP 6：建立 MQTT Broker IP
    // =====================================================

    ip_addr_t broker_ip;

    if (!ipaddr_aton(
            MQTT_BROKER_IP,
            &broker_ip))
    {
        printf("[ERROR] Invalid Broker IP\n");

        while (true)
        {
            cyw43_arch_poll();
            sleep_ms(100);
        }
    }

    printf(
        "[STEP 6] MQTT Broker = %s:%d\n",
        MQTT_BROKER_IP,
        MQTT_BROKER_PORT);

    // =================================================
    // STEP 7：建立 MQTT Client
    // =====================================================

    mqtt_client =
        mqtt_client_new();

    if (mqtt_client == NULL)
    {
        printf(
            "[ERROR] mqtt_client_new FAILED\n");

        while (true)
        {
            cyw43_arch_poll();
            sleep_ms(100);
        }
    }

    printf("[STEP 7] MQTT client created\n");

    // =================================================
    // STEP 8：MQTT Client Information
    // =====================================================

    static struct mqtt_connect_client_info_t client_info;

    memset(
        &client_info,
        0,
        sizeof(client_info));

    client_info.client_id =
        "garbage_pico_w";

    // =================================================
    // STEP 9：連接 Mosquitto
    // =====================================================

    printf(
        "[STEP 9] Connecting MQTT to %s:%d...\n",
        MQTT_BROKER_IP,
        MQTT_BROKER_PORT);

    err_t mqtt_result =
        mqtt_client_connect(
            mqtt_client,
            &broker_ip,
            MQTT_BROKER_PORT,
            mqtt_connection_callback,
            NULL,
            &client_info);

    printf(
        "[STEP 9] mqtt_client_connect returned = %d\n",
        mqtt_result);

    if (mqtt_result == ERR_OK)
    {
        printf(
            "[STEP 9] MQTT connect request sent\n");
    }
    else
    {
        printf(
            "[ERROR] MQTT connect request FAILED\n");
    }

    // =================================================
    // Main Loop
    // =====================================================

    int sensor_counter = 0;

    while (true)
    {
        // =============================================
        // lwIP Poll
        // =============================================

        cyw43_arch_poll();

        sleep_ms(100);

        sensor_counter++;

        // 每 500ms 讀一次 MPU6050
        if (sensor_counter < 5)
        {
            continue;
        }

        sensor_counter = 0;

        // =============================================
        // 讀 MPU6050
        // =============================================

        int16_t accel_x;
        int16_t accel_y;
        int16_t accel_z;

        bool sensor_ok =
            mpu6050_read_accel(
                &accel_x,
                &accel_y,
                &accel_z);

        if (!sensor_ok)
        {
            printf(
                "[MPU6050] Sensor read failed\n");

            continue;
        }

        // =============================================
        // Raw Data -> g
        // =============================================

        float ax =
            accel_x / 16384.0f;

        float ay =
            accel_y / 16384.0f;

        float az =
            accel_z / 16384.0f;

        // =============================================
        // Total acceleration
        // =============================================

        float total_g =
            sqrtf(
                ax * ax +
                ay * ay +
                az * az);

        // =============================================
        // 判斷狀態
        // =============================================

        const char *status;

        // 碰撞優先
        if (total_g > 1.8f)
        {
            status =
                "COLLISION";
        }

        // 明顯傾斜
        else if (fabsf(az) < 0.70f)
        {
            status =
                "TILT";
        }

        // 正常
        else
        {
            status =
                "NORMAL";
        }

        // =============================================
        // Serial 顯示 Sensor
        // =============================================

        printf(
            "[SENSOR] "
            "X:%6.2f | "
            "Y:%6.2f | "
            "Z:%6.2f | "
            "Total:%6.2f | "
            "%s\n",
            ax,
            ay,
            az,
            total_g,
            status);

        // =============================================
        // MQTT 尚未連線
        // =============================================

        if (!mqtt_connected)
        {
            printf(
                "[MQTT] Waiting for connection...\n");

            continue;
        }

        // =============================================
        // 建立 MQTT Message
        // =============================================

        char message[160];

        snprintf(
            message,
            sizeof(message),
            "{\"ax\":%.2f,"
            "\"ay\":%.2f,"
            "\"az\":%.2f,"
            "\"total\":%.2f,"
            "\"status\":\"%s\"}",
            ax,
            ay,
            az,
            total_g,
            status);

        // =============================================
        // Publish
        // =============================================

        printf(
            "[MQTT] Publishing: %s\n",
            message);

        err_t publish_result =
            mqtt_publish(
                mqtt_client,
                MQTT_TOPIC,
                message,
                strlen(message),
                0,
                0,
                mqtt_publish_callback,
                NULL);

        if (publish_result != ERR_OK)
        {
            printf(
                "[MQTT] Publish call FAILED err=%d\n",
                publish_result);
        }
    }

    return 0;
}