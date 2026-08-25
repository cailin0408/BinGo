# BinGo! - Raspberry Pi 5 DHT11 Kernel Driver & IPC 整合模組

本模組為 **BinGo! 智慧跟隨垃圾桶** 專案中，於 Raspberry Pi 5 (RPi 5) 上實作的 **DHT11 溫濕度感測器驅動與 IPC 數據傳輸系統**[cite: 2]。

由於 RPi 5 的 RP1 晶片架構變更，傳統 Linux Kernel Driver 中的微秒級 (`udelay`) 微時序處理易造成訊號讀取失真；因此本專案採用 **Hybrid IPC 架構**：
1. **Python 端 (`dht11_publisher.py`)**：利用 `adafruit_dht` (`libgpiod` 事件捕捉) 穩定擷取 DHT11 時序數據，並寫入 IPC Named Pipe (`/tmp/dht11_fifo`)[cite: 2]。
2. **Kernel Driver 端 (`dht11_driver.c`)**：建立字元裝置節點 `/dev/dht11`，作為核心驅動介面。
3. **C 語言主程式 (`main.c`)**：透過 `Thread 4` 讀取 IPC 管道數據，印出即時溫濕度並同步推播至 `/dev/dht11`。

---

## 📌 硬體腳位接線 (GPIO 4)

| DHT11 感測器 Pin | 樹莓派 RPi 5 腳位 | 說明 |
| :--- | :--- | :--- |
| **VCC** | **Pin 1 (3.3V)** 或 **Pin 2 (5V)** | 電源輸入 |
| **DATA** | **Pin 7 (GPIO 4)** | 資料信號線 |
| **GND** | **Pin 6 (GND)** | 共地 |

---

## 📁 檔案結構

- `dht11_driver.c`：RPi 5 Linux Character Kernel Driver 原始碼。
- `Makefile`：Kernel Driver 編譯檔。
- `dht11_publisher.py`：Python 端的 DHT11 時序讀取與 IPC (FIFO) 發射腳本 (GPIO 4)。
- `main.c`：整合 MQTT (Thread 1)、Pico W 超音波避障 (Thread 2)、終端機手動控制 (Thread 3) 及 DHT11 IPC 接收與 Driver 同步 (Thread 4) 的 C 語言系統核心程式。

---

## 🛠️ 環境準備與套件安裝

請在 RPi 5 Terminal 依序執行以下命令安裝編譯器、Kernel Header 與 Python 工具：

```bash
# 1. 安裝 RPi 5 (BCM2712) 核心標頭檔與 C 語言編譯工具鏈
sudo apt update && sudo apt install linux-headers-rpi-2712 build-essential -y

# 2. 安裝 Python3 pip 與 Linux GPIO 核心系統工具 (gpiod)
sudo apt update && sudo apt install python3-pip gpiod -y

# 3. 安裝 Adafruit DHT Python 程式庫 (通過 libgpiod 讀取 GPIO 時序)
pip3 install adafruit-circuitpython-dht --break-system-packages
```

---

## 🚀 執行步驟 (Step-by-Step)

### Step 1: 編譯與載入 Kernel Driver

開一頁 Terminal，進入專案目錄：

```bash
# 1. 編譯 Kernel Module (.ko)
make

# 2. 載入驅動程式
sudo insmod dht11_driver.ko

# 3. 開放 /dev/dht11 讀寫權限給系統主程式
sudo chmod 666 /dev/dht11

# (可選) 確認裝置檔案已成功建立
ls -l /dev/dht11
```

### Step 2: 編譯與啟動 C 語言系統主程式 (main.c)

在 Terminal 中執行：

```bash
# 1. 編譯 main.c (連結 pthread 與 mosquitto 庫)
gcc main.c -o main -lpthread -lmosquitto

# 2. 執行主程式
./main
```

### Step 3: 啟動 Python DHT11 IPC 發送端 (dht11_publisher.py)

另開一頁新的 Terminal 視窗執行 Python 腳本：

```bash
python3 dht11_publisher.py
```

---

## 📊 預期執行結果

當系統正常運作時，畫面將呈現如下：

1. Python Terminal (dht11_publisher.py)
```bash
🟢 [Python DHT11] IPC 發送端已啟動，等待 C 主程式連接...
[DHT11 讀取成功]: Temp: 24.3 C, Humidity: 58%
[DHT11 讀取成功]: Temp: 24.5 C, Humidity: 58%
```

2. C 主程式 Terminal (./main)
```bash
=========================================
 RPi 5 - BinGo 主控制系統 (MQTT + Pico W + DHT11 Driver) 
=========================================
🟢 MQTT 客戶端啟動成功，已訂閱 Topic: bingo/command
📡 成功開啟 /dev/ttyACM0，超音波避障監控啟用中...
⌨️  終端機手動控制已就緒！(可直接輸入 COME / STOP / OPEN_LID 並按 Enter)
🌡️ DHT11 IPC 接收管道已就緒: /tmp/dht11_fifo
🌡️ [DHT11 即時數據]: Temp: 24.3 C, Humidity: 58%
```

3. Driver 節點讀取驗證

- 在任何 Terminal 執行以下命令均可測試直接讀取 Driver
```bash
cat /dev/dht11
```