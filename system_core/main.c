#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <mosquitto.h>
#include <sys/stat.h>

#define SERIAL_PORT "/dev/ttyACM0"
#define MQTT_HOST "127.0.0.1"
#define MQTT_PORT 1883
#define MQTT_SUB_TOPIC "bingo/command"
#define MQTT_PUB_TOPIC "bingo/command"
#define FIFO_PATH "/tmp/dht11_fifo"
#define DRIVER_PATH "/dev/dht11"

// 擴充為完整 FSM 狀態
typedef enum {
    CAR_IDLE,       // 待命狀態
    CAR_MOVING,     // 前進跟隨中
    CAR_ARRIVED,    // 抵達目的地 (開蓋)
    CAR_RETURNING,  // 返航歸位中
    CAR_RETURNED    // 已回到原點
} CarState;

CarState current_state = CAR_IDLE;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;
struct mosquitto *g_mosq = NULL; // 全域 MQTT Client 指標供 Publish 使用

// MQTT 發送輔助函式
void mqtt_publish_status(const char *msg) {
    if (g_mosq) {
        mosquitto_publish(g_mosq, NULL, MQTT_PUB_TOPIC, strlen(msg), msg, 0, false);
    }
}

// 設定 USB 串列埠 (Baudrate 115200)
int init_serial(const char *portname) {
    int fd = open(portname, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) return -1;

    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= (CLOCAL | CREAD | CS8);
    options.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tcsetattr(fd, TCSANOW, &options);
    return fd;
}

// 硬體動作執行模擬
void execute_action(const char *cmd) {
    printf("\n[RPi 5 硬體動作] ---> 執行指令: %s\n", cmd);
}

// 統一的指令處理解析
void process_command(const char *cmd) {
    pthread_mutex_lock(&state_mutex);
    
    if (strstr(cmd, "OPEN_LID")) {
        if (current_state != CAR_ARRIVED) {
            current_state = CAR_ARRIVED;
            printf("\n[系統狀態] 收到 OPEN_LID ➔ 自動掀蓋 (90°)\n");
            execute_action("OPEN_LID");
        }
    } 
    else if (strstr(cmd, "STOP")) {
        if (current_state != CAR_IDLE && current_state != CAR_RETURNED) {
            current_state = CAR_IDLE;
            printf("\n[系統狀態] 收到 STOP ➔ 煞車停止並關蓋 (0°)\n");
            execute_action("STOP");
        }
    } 
    else if (strstr(cmd, "COME")) {
        if (current_state != CAR_MOVING) {
            current_state = CAR_MOVING;
            printf("\n[系統狀態] 收到 COME ➔ 車子啟動前進跟隨...\n");
            execute_action("COME");
        }
    } 
    else if (strstr(cmd, "RETURN")) {
        if (current_state != CAR_RETURNING) {
            current_state = CAR_RETURNING;
            printf("\n[系統狀態] 收到 RETURN ➔ 車子開始返航...\n");
            execute_action("RETURN");
        }
    } 
    else {
        printf("\n⚠️ 未知指令: %s (請輸入: COME, STOP, OPEN_LID, RETURN)\n", cmd);
    }

    pthread_mutex_unlock(&state_mutex);
}

// MQTT 收到訊息時的回呼函式
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    if (msg->payloadlen) {
        char cmd[64] = {0};
        strncpy(cmd, (char*)msg->payload, msg->payloadlen);
        printf("\n🌐 [MQTT 接收] Topic: %s | Payload: %s\n", msg->topic, cmd);
        process_command(cmd);
    }
}

// 【Thread 1】MQTT 接收與發送線路
void* mqtt_thread(void *arg) {
    mosquitto_lib_init();
    g_mosq = mosquitto_new("rpi5_core", true, NULL);

    if (!g_mosq) {
        printf("建立 MQTT Client 失敗！\n");
        return NULL;
    }

    mosquitto_message_callback_set(g_mosq, on_message);

    if (mosquitto_connect(g_mosq, MQTT_HOST, MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) {
        printf("無法連接至本地 MQTT Broker！\n");
        return NULL;
    }

    mosquitto_subscribe(g_mosq, NULL, MQTT_SUB_TOPIC, 0);
    printf("🟢 MQTT 客戶端啟動成功，已訂閱 Topic: %s\n", MQTT_SUB_TOPIC);

    mosquitto_loop_forever(g_mosq, -1, 1);

    mosquitto_destroy(g_mosq);
    mosquitto_lib_cleanup();
    return NULL;
}

// 【Thread 2】Pico W 超音波距離監控線路
void* pico_serial_thread(void *arg) {
    int fd = init_serial(SERIAL_PORT);
    if (fd < 0) {
        printf("⚠️ 無法開啟串口 %s，請檢查 Pico W 是否已以 USB 插上 RPi 5！\n", SERIAL_PORT);
        return NULL;
    }
    printf("📡 成功開啟 %s，超音波避障監控啟用中...\n", SERIAL_PORT);

    char buf[256];
    int idx = 0;

    while (1) {
        char ch;
        if (read(fd, &ch, 1) > 0) {
            if (ch == '\n' || ch == '\r') {
                buf[idx] = '\0';
                
                if (strncmp(buf, "Distance:", 9) == 0) {
                    float dist = atof(buf + 9); 
                    
                    pthread_mutex_lock(&state_mutex);
                    // 前進跟隨中遇障礙 ➔ 煞車並掀蓋 (抵達)
                    if (current_state == CAR_MOVING && dist > 0.0f && dist <= 20.0f) {
                        printf("\n🚨 [超音波觸發] 前方障礙 (%.2f cm)！緊急煞車並開啟桶蓋！\n", dist);
                        execute_action("STOP");
                        execute_action("OPEN_LID");
                        current_state = CAR_ARRIVED;
                        
                        // 發送警報與狀態給前端
                        printf("📡 發送 MQTT 狀態: STOP + OPEN_LID\n");
                        mqtt_publish_status("STOP");
                        mqtt_publish_status("OPEN_LID");
                    } 
                    // 返航中遇障礙/抵達原點 ➔ 僅煞車，不掀蓋
                    else if (current_state == CAR_RETURNING && dist > 0.0f && dist <= 20.0f) {
                        printf("\n🛑 [返航到達/避障] 前方障礙 (%.2f cm)！返航停止！\n", dist);
                        execute_action("STOP");
                        current_state = CAR_RETURNED;
                        
                        // 直接 Publish STOP 指令
                        printf("📡 發送 MQTT 狀態: STOP\n");
                        mqtt_publish_status("STOP");
                    }
                    pthread_mutex_unlock(&state_mutex);
                }
                idx = 0;
            } else if (idx < sizeof(buf) - 1) {
                buf[idx++] = ch;
            }
        } else {
            usleep(10000); // 10ms
        }
    }
    close(fd);
    return NULL;
}

// 【Thread 3】DHT11 溫溼度監控與 Driver 同步線路
void* dht11_thread(void *arg) {
    // 建立 IPC Pipe 管道
    mkfifo(FIFO_PATH, 0666);
    printf("🌡️ DHT11 溫溼度監控 IPC 接收管道已就緒: %s\n", FIFO_PATH);

    while (1) {
        int fd = open(FIFO_PATH, O_RDONLY);
        if (fd < 0) {
            usleep(500000);
            continue;
        }

        char buf[128];
        ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
        if (bytes > 0) {
            buf[bytes] = '\0';
            
            // 1. 同步將原始數據寫入 Kernel Driver 節點 (/dev/dht11)
            int driver_fd = open(DRIVER_PATH, O_WRONLY);
            if (driver_fd >= 0) {
                write(driver_fd, buf, strlen(buf));
                close(driver_fd);
            }

            // 2. 解析溫溼度數值 (支援 "Temp: 26.5, Humidity: 60.0" 或 "TEMP:26.5,HUM:60.0" 等格式)
            float temp = 0.0f, hum = 0.0f;
            if (sscanf(buf, "Temp: %f, Humidity: %f", &temp, &hum) >= 1 || 
                sscanf(buf, "TEMP:%f,HUM:%f", &temp, &hum) >= 1 || 
                sscanf(buf, "%f", &temp) >= 1) {
                
                // 3. 高溫過熱安全機制驗證 (> 26.0°C)
                if (temp > 26.0f) {
                    pthread_mutex_lock(&state_mutex);
                    printf("\n🔥 [高溫警報] 檢測到溫度 %.1f°C > 26°C！執行緊急防護 (STOP + OPEN_LID)\n", temp);
                    
                    execute_action("STOP");
                    execute_action("OPEN_LID");
                    current_state = CAR_ARRIVED;

                    // 回傳警報與狀態給前端網頁
                    char alert_msg[64];
                    snprintf(alert_msg, sizeof(alert_msg), "ALERT:OVERHEAT_TEMP_%.1f", temp);
                    printf("📡 發送 MQTT 警報: %s\n", alert_msg);
                    // mqtt_publish_status(alert_msg);
                    mqtt_publish_status("OPEN_LID");
                    
                    pthread_mutex_unlock(&state_mutex);
                }
            }
        }
        close(fd);
        usleep(1000000); // 每秒檢查輪詢一次
    }
    return NULL;
}

// 【Thread 4】終端機鍵盤輸入監控線路
void* terminal_input_thread(void *arg) {
    char input_buf[128];
    printf("⌨️ 終端機手動控制已就緒！(可輸入 COME / STOP / OPEN_LID / RETURN)\n");

    while (1) {
        if (fgets(input_buf, sizeof(input_buf), stdin) != NULL) {
            input_buf[strcspn(input_buf, "\r\n")] = 0;
            if (strlen(input_buf) > 0) {
                printf("💻 [終端機輸入] 指令: %s\n", input_buf);
                process_command(input_buf);
            }
        }
    }
    return NULL;
}

int main(void) {
    pthread_t t_mqtt, t_serial, t_dht, t_terminal;

    printf("=========================================\n");
    printf(" RPi 5 - BinGo 主控制系統 (FSM + MQTT)   \n");
    printf("=========================================\n");

    pthread_create(&t_mqtt, NULL, mqtt_thread, NULL);
    pthread_create(&t_serial, NULL, pico_serial_thread, NULL);
    pthread_create(&t_dht, NULL, dht11_thread, NULL);
    pthread_create(&t_terminal, NULL, terminal_input_thread, NULL);

    pthread_join(t_mqtt, NULL);
    pthread_join(t_serial, NULL);
    pthread_join(t_dht, NULL);
    pthread_join(t_terminal, NULL);

    return 0;
}