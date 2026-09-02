import json
import paho.mqtt.client as mqtt


# ==========================================
# MQTT 設定
# ==========================================

BROKER_IP = "172.20.10.4"
BROKER_PORT = 1883

TOPIC = "garbage/pico/status"


# ==========================================
# Buzzer Device
# ==========================================

BUZZER_DEVICE = "/dev/buzzer"


# ==========================================
# Buzzer 控制
# ==========================================

def buzzer_on():

    try:

        with open(BUZZER_DEVICE, "w") as f:
            f.write("1")

        print("[BUZZER] ON")

    except Exception as e:

        print("[BUZZER ERROR] ON failed:", e)


def buzzer_off():

    try:

        with open(BUZZER_DEVICE, "w") as f:
            f.write("0")

        print("[BUZZER] OFF")

    except Exception as e:

        print("[BUZZER ERROR] OFF failed:", e)


# ==========================================
# MQTT 連線成功 Callback
# ==========================================

def on_connect(client, userdata, flags, rc):

    print()
    print("========================================")
    print("MQTT Controller")
    print("========================================")

    if rc == 0:

        print("[MQTT] Connected!")

        client.subscribe(TOPIC)

        print("[MQTT] Subscribe:", TOPIC)

    else:

        print("[MQTT] Connection failed:", rc)


# ==========================================
# 收到 MQTT Message
# ==========================================

def on_message(client, userdata, msg):

    payload = msg.payload.decode()

    print()
    print("----------------------------------------")
    print("[MQTT] Received")
    print("Topic   :", msg.topic)
    print("Payload :", payload)

    try:

        # JSON String -> Python Dictionary
        data = json.loads(payload)

        ax = data["ax"]
        ay = data["ay"]
        az = data["az"]

        total = data["total"]

        status = data["status"]

        print()
        print("[SENSOR]")
        print("X      :", ax)
        print("Y      :", ay)
        print("Z      :", az)
        print("Total  :", total)
        print("Status :", status)

        # ==================================
        # 狀態處理
        # ==================================

        if status == "NORMAL":

            print("[ACTION] System normal")

            buzzer_off()

        elif status == "TILT":

            print("[ACTION] TILT warning!")

            buzzer_on()

        elif status == "COLLISION":

            print("[ACTION] COLLISION ALARM!")

            buzzer_on()

        else:

            print("[ACTION] Unknown status")

            # 未知狀態時，安全起見關閉蜂鳴器
            buzzer_off()

    except Exception as e:

        print("[ERROR] Message processing failed:", e)


# ==========================================
# Main
# ==========================================

client = mqtt.Client()

client.on_connect = on_connect
client.on_message = on_message

print("Connecting to MQTT Broker...")
print("Broker =", BROKER_IP)
print("Port   =", BROKER_PORT)

client.connect(
    BROKER_IP,
    BROKER_PORT,
    60
)

client.loop_forever()