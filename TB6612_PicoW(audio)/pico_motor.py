"""
BinGo!! - Pico W + TB6612 馬達控制程式 (MicroPython)
訂閱 MQTT topic，收到指令後控制馬達方向與PWM調速。

============== 接線對照表 (TB6612 <-> Pico W) ==============
TB6612接腳    Pico W GPIO    Pico W物理Pin腳位
--------------------------------------------------------
STBY          GP8            Pin 11   (務必接，沒接高電位馬達完全不會動)
AIN1          GP2            Pin 4
AIN2          GP3            Pin 5
PWMA          GP6            Pin 9
BIN1          GP4            Pin 6
BIN2          GP5            Pin 7
PWMB          GP7            Pin 10
VM (馬達電源)  接電池盒正極(不經過Pico W)
GND           與Pico W GND、電池盒負極 全部共地(這點非常重要，缺這步會不穩定)
VCC (邏輯電源) 接 Pico W 3V3(OUT), physical Pin 36

TT馬達分別接到 TB6612 的 A01/A02(左馬達) 與 B01/B02(右馬達)
==============================================================

需求：
- Pico W 已燒錄 MicroPython 韌體
- 已透過 Thonny「管理套件」安裝 umqtt.simple
"""

import utime
import machine
import network
from umqtt.simple import MQTTClient

# ===================== 使用者設定區 =====================
WIFI_SSID = "你的WiFi名稱"
WIFI_PASSWORD = "你的WiFi密碼"

MQTT_BROKER = "192.168.1.100"        # 樹莓派（MQTT broker主機）IP
MQTT_PORT = 1883                      # Pico W 用一般 TCP
MQTT_TOPIC = b"bingo/pico_practice"   # 練習用topic，跟正式系統bingo/command分開
CLIENT_ID = "pico_w_tb6612"

DEFAULT_DUTY = 60   # COME/FORWARD時，若尚未指定過SPEED，預設使用的duty(0-100)
MIN_EFFECTIVE_DUTY = 30  # 依實測調整：低於此值TT馬達可能因靜摩擦而動不了
PWM_FREQ = 1000      # PWM頻率(Hz)，TB6612效率高，1kHz通常安靜且穩定
# =========================================================

# ---- 方向控制腳位 ----
ain1 = machine.Pin(2, machine.Pin.OUT)
ain2 = machine.Pin(3, machine.Pin.OUT)
bin1 = machine.Pin(4, machine.Pin.OUT)
bin2 = machine.Pin(5, machine.Pin.OUT)

# ---- STBY：TB6612專屬，一定要拉高才能讓驅動板工作 ----
stby = machine.Pin(8, machine.Pin.OUT)
stby.value(1)  # 開機就先拉高，讓驅動板隨時待命

# ---- PWM調速腳位 ----
pwma = machine.PWM(machine.Pin(6))
pwmb = machine.PWM(machine.Pin(7))
pwma.freq(PWM_FREQ)
pwmb.freq(PWM_FREQ)

led = machine.Pin(25, machine.Pin.OUT)  # 開發板內建LED，當作動作指示燈

current_duty = DEFAULT_DUTY


def _apply_duty(duty_percent):
    """把0-100的百分比轉成Pico W PWM要求的16-bit數值(0-65535)"""
    duty_percent = max(0, min(100, duty_percent))
    duty_u16 = int(duty_percent / 100 * 65535)
    pwma.duty_u16(duty_u16)
    pwmb.duty_u16(duty_u16)


def forward():
    led.value(1)
    ain1.high(); ain2.low()
    bin1.high(); bin2.low()
    _apply_duty(current_duty)


def backward():
    led.value(1)
    ain1.low(); ain2.high()
    bin1.low(); bin2.high()
    _apply_duty(current_duty)


def left():
    # 左輪反轉、右輪正轉 -> 原地左轉；若想改成「差速轉彎」而非原地轉，
    # 可以只降低左輪duty而不反轉，依你實際測試手感調整
    led.value(1)
    ain1.low(); ain2.high()
    bin1.high(); bin2.low()
    _apply_duty(current_duty)


def right():
    led.value(1)
    ain1.high(); ain2.low()
    bin1.low(); bin2.high()
    _apply_duty(current_duty)


def stop():
    led.value(0)
    ain1.low(); ain2.low()
    bin1.low(); bin2.low()
    _apply_duty(0)


def set_speed(duty_percent):
    global current_duty
    duty_percent = max(0, min(100, duty_percent))

    # 避免落在「有訊號但馬達動不了」的無效區間(跟樹莓派那邊L298N一樣的教訓)
    if 0 < duty_percent < MIN_EFFECTIVE_DUTY:
        duty_percent = MIN_EFFECTIVE_DUTY

    current_duty = duty_percent
    _apply_duty(current_duty)
    print("設定速度 duty=%d%%" % current_duty)


# ===================== WiFi 連線 =====================
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)
    print("正在連接WiFi:", WIFI_SSID)

    timeout = 15
    while not wlan.isconnected() and timeout > 0:
        utime.sleep(1)
        timeout -= 1
        print("等待中...", timeout)

    if wlan.isconnected():
        print("WiFi已連接，IP:", wlan.ifconfig()[0])
        return True
    else:
        print("WiFi連接失敗")
        return False


# ===================== MQTT 訊息處理 =====================
def on_message(topic, msg):
    command = msg.decode("utf-8").strip()
    print("收到指令:", command)

    if command in ("FORWARD", "COME"):
        forward()
    elif command == "BACKWARD":
        backward()
    elif command == "LEFT":
        left()
    elif command == "RIGHT":
        right()
    elif command == "STOP":
        stop()
    elif command.startswith("SPEED:"):
        try:
            duty = int(command.split(":")[1])
            set_speed(duty)
        except (IndexError, ValueError):
            print("無效的SPEED格式:", command)
    else:
        print("未知指令，忽略:", command)


def main():
    if not connect_wifi():
        return

    client = MQTTClient(CLIENT_ID, MQTT_BROKER, port=MQTT_PORT)
    client.set_last_will(MQTT_TOPIC, b"STOP", retain=False, qos=0)
    client.set_callback(on_message)

    try:
        client.connect()
        print("已連接MQTT Broker:", MQTT_BROKER)
        client.subscribe(MQTT_TOPIC)
        print("已訂閱:", MQTT_TOPIC)
    except Exception as e:
        print("MQTT連接失敗:", e)
        return

    while True:
        try:
            client.wait_msg()
        except Exception as e:
            print("MQTT錯誤，嘗試重連:", e)
            stop()
            utime.sleep(3)
            try:
                client.connect()
                client.subscribe(MQTT_TOPIC)
            except Exception as reconnect_err:
                print("重連失敗:", reconnect_err)


if __name__ == "__main__":
    main()