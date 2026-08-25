# 🤖 Pico W (RP2040) HY-SRF05 超音波測距模組

本專案為「主動式跟隨垃圾桶」之底層避障與防撞感測韌體，運行於 Raspberry Pi Pico W。

## 📌 硬體接線 (Wiring)
- **VCC**: 5V
- **GND**: GND
- **TRIG**: GP2 (GPIO 2)
- **ECHO**: GP3 (GPIO 3) 

## 🛠️ 編譯與建置 (Build)
1. 開啟 VS Code 並載入本專案資料夾。
2. 按下 `F7` 或點擊 VS Code 底部狀態列的 **Build**。
3. 編譯完畢後，將 `build/ultrasonic.uf2` 拖入進入 BOOTSEL 模式的 Pico W (`RPI-RP2` 磁碟機) 即可。