# BinGo 專題 Demo 啟動指令

## 系統架構

目前有兩個 Demo：

### Demo 1：MPU6050 Sensor 自動警示

```text
MPU6050
  ↓ I2C
Pico W
  ↓ Wi-Fi / MQTT
Mosquitto Broker
  ↓
controller.py
  ↓
/dev/buzzer
  ↓
Linux Device Driver
  ↓
GPIO18
  ↓
Buzzer
```

功能：

- NORMAL → Buzzer OFF
- TILT → Buzzer ON
- COLLISION → Buzzer ON


### Demo 2：手勢辨識控制

```text
Camera
  ↓
MediaPipe Hands
  ↓
handTracking.html
  ↓ MQTT over WebSocket
Mosquitto Broker
  ↓
mqtt_server.js
  ↓
COME → 播放垃圾車音樂
STOP → 停止音樂 + Buzzer BEEP
```

手勢：

- 布 ✋ → COME → 播放垃圾車音樂
- 石頭 ✊ → STOP → 停止音樂 + 蜂鳴器 BEEP
- 讚 👍 → OPEN_LID


---

# 1. 開機後先確認 Raspberry Pi IP

```bash
hostname -I
```

目前專案設定的 Raspberry Pi IP：

```text
172.20.10.4
```

⚠️ 如果 IP 不是 `172.20.10.4`，先不要執行 Demo。

需要確認：

- handTracking.html 的 RPI_IP
- Pico W 的 MQTT Broker IP
- controller.py 的 BROKER_IP


---

# 2. 確認 Mosquitto Broker

```bash
sudo systemctl status mosquitto
```

正常應該看到：

```text
active (running)
```

按：

```text
q
```

離開 status 畫面。


---

# 3. 確認 Buzzer Driver

```bash
lsmod | grep buzzer
```

正常應該看到類似：

```text
buzzer_driver    49152    0
```

## 如果完全沒有東西

代表 Driver 還沒有載入。

執行：

```bash
cd ~/garbage/driver
sudo insmod buzzer_driver.ko
```

再次確認：

```bash
lsmod | grep buzzer
```

確認 Character Device：

```bash
ls -l /dev/buzzer
```

正常要存在：

```text
/dev/buzzer
```

給 User Space 程式讀寫權限：

```bash
sudo chmod 666 /dev/buzzer
```


---

# 4. 手動測試 Buzzer Driver

開啟：

```bash
echo 1 > /dev/buzzer
```

Buzzer 應該開始叫。

關閉：

```bash
echo 0 > /dev/buzzer
```

Buzzer 應該停止。


---

# Demo 1：MPU6050 Sensor 自動警示

## Terminal 1：查看 MQTT 原始資料

```bash
mosquitto_sub -h 172.20.10.4 -t "garbage/pico/status" -v
```

正常會看到：

```text
garbage/pico/status {"ax":0.04,"ay":0.04,"az":-0.99,"total":1.00,"status":"NORMAL"}
```

代表：

```text
MPU6050
↓
Pico W
↓
Wi-Fi
↓
MQTT
↓
Raspberry Pi
```

資料傳輸成功。


## Terminal 2：啟動 Controller

```bash
cd ~/garbage/controller
python3 controller.py
```

正常狀態：

```text
Status : NORMAL
[ACTION] System normal
[BUZZER] OFF
```

將 MPU6050 傾斜：

```text
Status : TILT
[ACTION] TILT warning!
[BUZZER] ON
```

發生碰撞：

```text
Status : COLLISION
[ACTION] COLLISION ALARM!
[BUZZER] ON
```

恢復正常：

```text
Status : NORMAL
[ACTION] System normal
[BUZZER] OFF
```


---

# Demo 1 口述

這個 Terminal 是用來確認 MQTT 的資料傳輸。

Pico W 會持續讀取 MPU6050 的三軸加速度資料，並把 X、Y、Z、總加速度以及判斷後的狀態封裝成 JSON，再 Publish 到 `garbage/pico/status` 這個 MQTT Topic。

第二個 Terminal 是我寫的 `controller.py`。

Controller 會訂閱相同的 MQTT Topic，收到資料之後解析 JSON，取得 MPU6050 的感測數值以及目前狀態。

現在收到的是 NORMAL，所以 Controller 判斷系統正常，可以看到：

```text
[ACTION] System normal
[BUZZER] OFF
```

因此蜂鳴器目前不會發出警示。

接著實際將 MPU6050 傾斜。

Pico W 判斷目前為 TILT，也就是傾斜異常，並透過 MQTT 傳送到 Raspberry Pi。

Controller 收到 TILT 後，就會透過 `/dev/buzzer` 呼叫 Linux Device Driver，由 Driver 控制 GPIO18，最後啟動蜂鳴器。


---

# Demo 2：手勢辨識 + MQTT + 音樂/Buzzer

## Terminal 1：確認 MQTT WebSocket Port

```bash
ss -lnt | grep 9001
```

有看到 Port 9001 代表 Mosquitto WebSocket 有開啟。


## Terminal 2：啟動 Node.js MQTT Server

先確認 Buzzer 權限：

```bash
sudo chmod 666 /dev/buzzer
```

進入 MQTT Server：

```bash
cd ~/garbage/MQTT-Server
```

啟動：

```bash
node mqtt_server.js
```

正常應該看到：

```text
=================================
MQTT connected
=================================
Subscribed: bingo/command
Waiting for gesture command...
```

Terminal 不要關閉。


---

# 5. 開啟 handTracking.html

確認網頁顯示：

```text
🟢 MQTT 已成功連線至 ws://172.20.10.4:9001
```

代表：

```text
Browser
↓
MQTT WebSocket :9001
↓
Mosquitto Broker
```

連線成功。


---

# 6. Demo 手勢

## 布 ✋

辨識：

```text
COME
```

MQTT Topic：

```text
bingo/command
```

Terminal：

```text
Message : COME
>>> COME：垃圾桶開始跟隨
>>> MUSIC：播放垃圾車音樂
>>> 開始播放垃圾車音樂
```

結果：

```text
播放 garbage.mp3
```


## 石頭 ✊

辨識：

```text
STOP
```

Terminal：

```text
Message : STOP
>>> STOP：垃圾桶停止
>>> 停止垃圾車音樂
>>> Buzzer：BEEP
```

結果：

```text
停止垃圾車音樂
+
Buzzer BEEP
```


## 讚 👍

辨識：

```text
OPEN_LID
```

目前：

```text
OPEN_LID 為垃圾桶開蓋控制 Command
```


---

# Demo 2 口述

第二個 Demo 是手勢辨識與 MQTT 控制。

這邊使用 Camera 搭配 MediaPipe Hands 進行手勢辨識，辨識完成後會將不同的手勢轉換成對應的控制指令。

目前設定：

```text
布 ✋   → COME
石頭 ✊ → STOP
讚 👍   → OPEN_LID
```

辨識完成後，網頁會透過 MQTT over WebSocket，把指令 Publish 到 `bingo/command`。

當我比出「布」的手勢時，系統辨識為 COME。

Raspberry Pi 上的 Node.js Server 收到 COME 後，就會開始播放垃圾車音樂。

接著我比出「石頭」，系統辨識為 STOP。

Node.js Server 收到 STOP 後會停止垃圾車音樂，並透過 `/dev/buzzer` 呼叫 Linux Device Driver。

最後由 Driver 控制 GPIO18，讓實體蜂鳴器 BEEP 一聲。


---

# 報告當天快速啟動

## Step 1：共通檢查

```bash
hostname -I
sudo systemctl status mosquitto
```

按 `q` 離開。

```bash
lsmod | grep buzzer
```

如果沒有 Driver：

```bash
cd ~/garbage/driver
sudo insmod buzzer_driver.ko
```

然後：

```bash
lsmod | grep buzzer
ls -l /dev/buzzer
sudo chmod 666 /dev/buzzer
```


## Step 2：Demo 1

Terminal A：

```bash
mosquitto_sub -h 172.20.10.4 -t "garbage/pico/status" -v
```

Terminal B：

```bash
cd ~/garbage/controller
python3 controller.py
```


## Step 3：Demo 2

先停止 Demo 1 的程式：

```text
Ctrl + C
```

確認 WebSocket：

```bash
ss -lnt | grep 9001
```

啟動 Node.js：

```bash
cd ~/garbage/MQTT-Server
node mqtt_server.js
```

然後開啟：

```text
handTracking.html
```

開始測試：

```text
✋ 布   → COME → 垃圾車音樂
✊ 石頭 → STOP → 停音樂 + Buzzer BEEP
```


---

# 常見問題 Debug

## 1. lsmod | grep buzzer 沒東西

```bash
cd ~/garbage/driver
sudo insmod buzzer_driver.ko
```

再確認：

```bash
lsmod | grep buzzer
```


## 2. /dev/buzzer Permission denied

```bash
sudo chmod 666 /dev/buzzer
```


## 3. MQTT 沒資料

確認 Broker：

```bash
sudo systemctl status mosquitto
```

確認 IP：

```bash
hostname -I
```


## 4. 手勢網頁 MQTT 連不上

確認 WebSocket Port：

```bash
ss -lnt | grep 9001
```

確認 handTracking.html：

```text
RPI_IP = 172.20.10.4
MQTT_PORT = 9001
MQTT_TOPIC = bingo/command
```


## 5. 垃圾車音樂手動測試

```bash
cd ~/garbage/MQTT-Server
mpg123 garbage.mp3
```


---

# 最終兩個 Demo Data Flow

## Demo 1

```text
MPU6050
↓ I2C
Pico W
↓ Wi-Fi
MQTT Publish
↓
garbage/pico/status
↓
Mosquitto
↓
controller.py
↓
/dev/buzzer
↓
Linux Device Driver
↓
GPIO18
↓
Buzzer
```


## Demo 2

```text
Camera
↓
MediaPipe Hands
↓
handTracking.html
↓ MQTT over WebSocket :9001
bingo/command
↓
Mosquitto
↓
mqtt_server.js
↓
COME ──→ garbage.mp3

STOP ──→ Stop Music
         ↓
      /dev/buzzer
         ↓
   Linux Device Driver
         ↓
      GPIO18
         ↓
      Buzzer
```