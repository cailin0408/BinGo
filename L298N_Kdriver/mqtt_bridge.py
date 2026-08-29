import paho.mqtt.client as mqtt
import signal
import sys

MQTT_BROKER = "127.0.0.1"  # 因為 Client 與 Broker 同在樹莓派上，寫 localhost 即可
MQTT_PORT = 1883           # 使用傳統 MQTT 通道 1883
MQTT_TOPIC = "bingo/command"
DEV_PATH = "/dev/l298n"

# 白名單：只允許這些合法指令寫入驅動節點，避免任意/錯誤字串寫壞裝置
ALLOWED_COMMANDS = {"COME", "STOP", "OPEN_LID"}


def write_to_driver(command):
    """寫入 L298N 驅動程式節點"""
    try:
        with open(DEV_PATH, "w") as dev:
            dev.write(command)
    except Exception as e:
        print(f"❌ 寫入 {DEV_PATH} 失敗: {e}")


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ 已成功連線至 MQTT Broker (代碼: {rc})")
        client.subscribe(MQTT_TOPIC)
        print(f"📡 已開始訂閱 Topic: {MQTT_TOPIC}")
    else:
        print(f"❌ 連線失敗，代碼: {rc}")


def on_disconnect(client, userdata, rc):
    print(f"⚠️ 與 Broker 斷線 (代碼: {rc})，paho 將自動嘗試重連...")


def on_message(client, userdata, msg):
    command = msg.payload.decode("utf-8").strip()
    print(f"📥 收到指令: {command}")

    if command not in ALLOWED_COMMANDS:
        print(f"⚠️ 忽略未知指令: {command!r}")
        return

    write_to_driver(command)


client = mqtt.Client()
client.on_connect = on_connect
client.on_disconnect = on_disconnect
client.on_message = on_message

# 斷線重連的退避秒數，避免瘋狂重連
client.reconnect_delay_set(min_delay=1, max_delay=30)


def handle_exit(signum, frame):
    print("\n🛑 收到結束信號，先送出 STOP 保護馬達再離開...")
    write_to_driver("STOP")
    client.disconnect()
    sys.exit(0)


signal.signal(signal.SIGINT, handle_exit)
signal.signal(signal.SIGTERM, handle_exit)

client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()
