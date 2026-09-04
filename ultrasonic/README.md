# 🤖 Pico W (RP2040) HY-SRF05 超音波測距模組

本專案為「BinGo 智慧跟隨垃圾桶」之底層避障與防撞感測韌體，運行於 Raspberry Pi Pico W (RP2040)。負責以 20Hz 高頻率提供微秒級延遲的距離數據，供主控端 (Raspberry Pi 5) 進行預防性緊急煞車與狀態控制。

---

## 📌 硬體連接 (Hardware Wiring & Connection)

### 1. 超音波感測器接線 (HY-SRF05)
| HY-SRF05 腳位 | Pico W 腳位 | 腳位編號 | 說明 |
| :--- | :--- | :--- | :--- |
| **VCC** | VBUS / 5V | Pin 40 | 5V 供電 |
| **GND** | GND | Pin 3 / 8 / 38 | 共地 |
| **TRIG** | GP2 | Pin 4 | 觸發訊號輸出 (Output) |
| **ECHO** | GP3 | Pin 5 | 回波訊號輸入 (Input) |

### 2. Pico W 與 RPi 5 對接方式 (Micro-USB to USB)
- **實體連接**：使用具備資料傳輸功能的 **Micro-USB 轉 USB-A 線材**，將 Pico W 的 Micro-USB 埠直接插入 Raspberry Pi 5 的 USB 埠。
- **供電與通訊**：RPi 5 會透過 USB 同時為 Pico W 供電，並建立 **USB CDC 虛擬序列埠 (`/dev/ttyACM0`)**。
- **通訊優勢**：無需額外拉線或進行 3.3V/5V 電位邏輯轉換，兼具數據傳輸穩定度與極低延遲。

---

## ⚙️ 韌體核心機制

- **高頻採樣 (20Hz)**：採樣間隔調優為 `50ms`，確保小車移動時的防撞安全反應時間[cite: 1, 2, 3]。
- **逾時防卡死保護**：內建 `30000us` (30ms) Echo 訊號逾時判定，避免因訊號吸收或未收到回波導致 MCU 阻塞。
- **USB CDC 序列埠通訊**：透過標準 USB 虛擬串口與 Raspberry Pi 5 (`/dev/ttyACM0`) 進行傳輸，傳輸速率為 115200 Baud Rate。
- **資料輸出格式**：
  - 正常測距：`Distance: XX.XX cm\n`
  - 讀取超時：`Distance: -1.00 cm`

---

## 🛠️ 編譯與建置 (Build)

1. 開啟 VS Code 並載入本 `ultrasonic` 專案資料夾。
2. 按下 `F7` 或點擊 VS Code 底部狀態列的 **Build**。
3. 編譯完成後，會在 `build/` 目錄下生成 `ultrasonic.uf2` 韌體檔案。

---

## 🚀 燒錄步驟 (Flash to Pico W)

將編譯好的 `ultrasonic.uf2` 燒錄進連接在 Raspberry Pi 5 上的 Pico W：

1. 拔下連接 Pico W 的 Micro-USB 傳輸線。
2. 按住 Pico W 板面上的 **BOOTSEL 按鈕** 不放，將 USB 插回 RPi 5 後鬆開按鈕。
3. RPi 5 系統將偵測並掛載名為 `RPI-RP2` 的隨身碟裝置 (`/dev/sda1`)。
4. 在 RPi 5 終端機執行以下命令進行掛載與寫入：

```bash
# 1. 切換到存放 ultrasonic.uf2 的目錄 (例如 /home/pi/projects)
cd /home/pi/projects

# 2. 建立掛載點並掛載 Pico W 隨身碟
sudo mkdir -p /mnt/pico
sudo mount /dev/sda1 /mnt/pico

# 3. 複製 .uf2 檔進行燒錄
sudo cp ultrasonic.uf2 /mnt/pico/

# 4. 強制寫入 (複製完後 Pico W 會自動重啟，並自動卸載磁碟)
sync
```