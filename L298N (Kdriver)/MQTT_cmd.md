[ 步驟 A：樹莓派安裝並設定 Mosquitto Broker ]

1.  安裝 Mosquitto：
sudo apt update
sudo apt install -y mosquitto mosquitto-clients

2.  配置 Mosquitto 開啟 WebSocket (埠號 9001)：
    編輯 /etc/mosquitto/conf.d/default.conf：

# 傳統 MQTT TCP 埠號
listener 1883
allow_anonymous true

# Web 端使用的 WebSocket 埠號
listener 9001
protocol websockets
allow_anonymous true


3.  重啟 MQTT 服務：
sudo systemctl restart mosquitto


--------------------------------------------
[ 步驟 B：樹莓派建立 MQTT Subscriber Client ]

MQTT 服務收到前端網頁發送的 bingo/command 訊息後，Subscriber 程式會負責將該指令寫入剛剛載入的內核驅動 /dev/l298n。

以下提供 Python（建議，最方便處理舵機開蓋邏輯與設備檔案寫入）與 C 語言 兩種實作方案：


方案一：Python MQTT Client (mqtt_bridge.py)
需先安裝套件：pip install paho-mqtt

import paho.mqtt.client as mqtt

MQTT_BROKER = "127.0.0.1"
MQTT_PORT = 1883
MQTT_TOPIC = "bingo/command"
DEV_PATH = "/dev/l298n"

def on_connect(client, userdata, flags, rc):
    print(f"Connected to MQTT Broker with code {rc}")
    client.subscribe(MQTT_TOPIC)

def on_message(client, userdata, msg):
    command = msg.payload.decode('utf-8')
    print(f"[MQTT Received] Topic: {msg.topic} -> Cmd: {command}")
    
    # 將指令寫入 Kernel Driver /dev/l298n
    try:
        with open(DEV_PATH, "w") as dev:
            dev.write(command)
    except Exception as e:
        print(f"Failed to write to {DEV_PATH}: {e}")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()