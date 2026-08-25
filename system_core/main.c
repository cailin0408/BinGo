#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <mosquitto.h>

#define SERIAL_PORT "/dev/ttyACM0"
#define MQTT_HOST "127.0.0.1"
#define MQTT_PORT 1883
#define MQTT_TOPIC "bingo/command"

typedef enum {
    CAR_STOPPED,
    CAR_MOVING
} CarState;

CarState current_state = CAR_STOPPED;
pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

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

// 統一的指令處理解析 (MQTT、終端機輸入與超音波共享)
void process_command(const char *cmd) {
    pthread_mutex_lock(&state_mutex);
    if (strstr(cmd, "COME")) {
        current_state = CAR_MOVING;
        printf("\n[系統狀態] 收到 COME ➔ 車子啟動前進跟隨...\n");
        execute_action("COME");
    } else if (strstr(cmd, "STOP")) {
        current_state = CAR_STOPPED;
        printf("\n[系統狀態] 收到 STOP ➔ 煞車停止並關蓋 (0°)\n");
        execute_action("STOP");
    } else if (strstr(cmd, "OPEN_LID")) {
        current_state = CAR_STOPPED;
        printf("\n[系統狀態] 收到 OPEN_LID ➔ 自動掀蓋 (90°)\n");
        execute_action("OPEN_LID");
    } else {
        printf("\n⚠️ 未知指令: %s (請輸入: COME, STOP, 或 OPEN_LID)\n", cmd);
    }
    pthread_mutex_unlock(&state_mutex);
}

// MQTT 收到訊息時的回呼函式 (Callback)
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *msg) {
    if (msg->payloadlen) {
        char cmd[64] = {0};
        strncpy(cmd, (char*)msg->payload, msg->payloadlen);
        printf("\n🌐 [MQTT 接收] Topic: %s | Payload: %s\n", msg->topic, cmd);
        process_command(cmd);
    }
}

// 【Thread 1】MQTT 接收線路 (保留網頁/鏡頭控制)
void* mqtt_thread(void *arg) {
    mosquitto_lib_init();
    struct mosquitto *mosq = mosquitto_new("rpi5_core", true, NULL);

    if (!mosq) {
        printf("建立 MQTT Client 失敗！\n");
        return NULL;
    }

    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) {
        printf("無法連接至本地 MQTT Broker！\n");
        return NULL;
    }

    mosquitto_subscribe(mosq, NULL, MQTT_TOPIC, 0);
    printf("🟢 MQTT 客戶端啟動成功，已訂閱 Topic: %s\n", MQTT_TOPIC);

    mosquitto_loop_forever(mosq, -1, 1);

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return NULL;
}

// 【Thread 2】Pico W 超音波距離監控線路 (配合 Distance: XX.XX cm 格式)
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
                
                // 比對 "Distance:" 開頭 (Pico W 傳出的格式)
                if (strncmp(buf, "Distance:", 9) == 0) {
                    // atof(buf + 9) 會自動跳過 "Distance:" 與空格，直接解析數字
                    float dist = atof(buf + 9); 
                    
                    pthread_mutex_lock(&state_mutex);
                    // 當車子處於 CAR_MOVING 狀態且超音波測得距離 <= 20cm 時觸發
                    if (current_state == CAR_MOVING && dist > 0.0f && dist <= 20.0f) {
                        printf("\n🚨 [超音波觸發] 檢測到前方障礙物 (距離: %.2f cm <= 20cm)！緊急搶佔控制權！\n", dist);
                        execute_action("STOP");
                        execute_action("OPEN_LID");
                        current_state = CAR_STOPPED;
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

// 【Thread 3】新增：終端機鍵盤輸入監控線路 (無鏡頭時測試用)
void* terminal_input_thread(void *arg) {
    char input_buf[128];
    printf("⌨️  終端機手動控制已就緒！(可直接輸入 COME / STOP / OPEN_LID 並按 Enter)\n");

    while (1) {
        if (fgets(input_buf, sizeof(input_buf), stdin) != NULL) {
            // 移除換行符號
            input_buf[strcspn(input_buf, "\r\n")] = 0;
            
            if (strlen(input_buf) > 0) {
                printf("💻 [終端機輸入] 手動發送指令: %s\n", input_buf);
                process_command(input_buf);
            }
        }
    }
    return NULL;
}

int main(void) {
    pthread_t t_mqtt, t_serial, t_terminal;

    printf("=========================================\n");
    printf(" RPi 5 - BinGo 主控制系統 (MQTT + Pico W) \n");
    printf("=========================================\n");

    // 建立 3 個平行運行的 Thread
    pthread_create(&t_mqtt, NULL, mqtt_thread, NULL);
    pthread_create(&t_serial, NULL, pico_serial_thread, NULL);
    pthread_create(&t_terminal, NULL, terminal_input_thread, NULL);

    // 等待 Thread
    pthread_join(t_mqtt, NULL);
    pthread_join(t_serial, NULL);
    pthread_join(t_terminal, NULL);

    return 0;
}