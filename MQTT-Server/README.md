# BinGo MQTT Server

BinGo 智慧垃圾桶專題的 MQTT Server 模組。

此模組運行於 Raspberry Pi，透過 Mosquitto MQTT Broker 接收手勢辨識端傳送的控制指令，並由 Node.js MQTT Server 進行後續處理。

這份 README 是我 MQTT Server 的使用說明，主要寫 MQTT 架構、怎麼安裝環境、怎麼啟動 Server、Topic 跟指令格式。之後你們如果要在別台 Raspberry Pi 跑我的 MQTT Server，照README的步驟就可以把環境架起來。
目前支援的控制指令：

- `COME`：垃圾桶開始跟隨
- `STOP`：垃圾桶停止
- `OPEN_LID`：開啟垃圾桶蓋

---

## 系統架構

```text
Camera / Hand Tracking
        |
        | MQTT over WebSocket
        | Topic: bingo/command
        v
+---------------------------+
| Raspberry Pi              |
|                           |
| Mosquitto MQTT Broker     |
| Port 1883 / 9001          |
|        |                  |
|        v                  |
| mqtt_server.js            |
|        |                  |
|        v                  |
| COME / STOP / OPEN_LID    |
+---------------------------+
```

- `1883`：一般 MQTT TCP 連線
- `9001`：MQTT over WebSocket，提供瀏覽器端連線

---

## 1. Clone 專案

```bash
git clone https://github.com/cailin0408/BinGo.git
cd BinGo/MQTT-Server
```

---

## 2. 安裝 Node.js 與 Mosquitto

```bash
sudo apt update
sudo apt install -y nodejs npm mosquitto mosquitto-clients
```

確認版本：

```bash
node -v
npm -v
mosquitto -h
```

---

## 3. 安裝 Node.js 套件

進入 `MQTT-Server` 目錄後執行：

```bash
npm ci
```

如果 `npm ci` 無法使用，也可以執行：

```bash
npm install
```

---

## 4. 啟動 Mosquitto

```bash
sudo systemctl enable --now mosquitto
```

確認 Broker 狀態：

```bash
sudo systemctl status mosquitto
```

---

## 5. 啟動 MQTT Server

```bash
node mqtt_server.js
```

正常啟動後會看到：

```text
=================================
MQTT connected
=================================
Subscribed: bingo/command
Waiting for gesture command...
```

---

## 6. MQTT Topic

目前使用：

```text
bingo/command
```

手勢辨識端會將控制指令 Publish 到此 Topic。

---

## 7. 支援指令

| MQTT Message | 功能 |
|---|---|
| `COME` | 垃圾桶開始跟隨 |
| `STOP` | 垃圾桶停止 |
| `OPEN_LID` | 開啟垃圾桶蓋 |

---

## 8. 使用 mosquitto_pub 測試

可以不用啟動 Camera，直接手動發送 MQTT 指令測試。

開啟 MQTT Server：

```bash
node mqtt_server.js
```

另外開一個 Terminal：

```bash
mosquitto_pub -h localhost -t bingo/command -m "COME"
```

測試停止：

```bash
mosquitto_pub -h localhost -t bingo/command -m "STOP"
```

測試開蓋：

```bash
mosquitto_pub -h localhost -t bingo/command -m "OPEN_LID"
```

Server Terminal 應該會顯示：

```text
-----------------------------
Topic   : bingo/command
Message : COME
>>> COME：垃圾桶開始跟隨
-----------------------------
```

---

## 9. WebSocket

若手勢辨識程式運行於其他電腦的瀏覽器，需要透過 Raspberry Pi 的 IP 連線到 Mosquitto WebSocket。

例如 Raspberry Pi IP：

```text
192.168.69.65
```

則瀏覽器端 MQTT WebSocket：

```text
ws://192.168.69.65:9001
```

Raspberry Pi IP 可以使用以下指令確認：

```bash
hostname -I
```

> Raspberry Pi IP 可能因網路環境改變，請以 `hostname -I` 顯示的 IP 為準。

---

## 10. Mosquitto WebSocket 設定

如果是新的 Raspberry Pi，需要設定 Mosquitto WebSocket listener。

建立設定：

```bash
sudo nano /etc/mosquitto/conf.d/websocket.conf
```

加入：

```text
listener 1883
protocol mqtt

listener 9001
protocol websockets

allow_anonymous true
```

重新啟動 Mosquitto：

```bash
sudo systemctl restart mosquitto
```

確認 Port：

```bash
sudo ss -lntp | grep -E '1883|9001'
```

正常應該可以看到：

```text
0.0.0.0:1883
0.0.0.0:9001
```

---

## 專案目錄

```text
MQTT-Server/
├── mqtt_server.js
├── package.json
├── package-lock.json
├── .gitignore
└── README.md
```

`node_modules/` 不會上傳至 GitHub，clone 專案後請執行：

```bash
npm ci
```

重新安裝所需套件。
