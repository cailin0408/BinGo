# System Core - BinGo! 智慧跟隨垃圾桶主控制系統

本目錄包含 **BinGo!** 專案的系統核心 C 語言主程式 (`main.c`)。此系統整合了多執行緒控制、狀態機 (FSM) 管理、Mosquitto MQTT 通訊、即時安全避障機制，以及測試用之終端機輸入監控。

## 📌 系統核心功能

- **pthread 多執行緒架構**：
  - **Thread 1 (主邏輯與 FSM 控制)**：監聽 IPC / MQTT 指令，驅動系統狀態轉移（IDLE → MOVING → ARRIVED → RETURN）。
  - **Thread 2 (高頻安全監控)**：背景高頻率讀取超音波感測器數據，實現即時避障。
  - **Thread 3 (鍵盤輸入監控 / 偵錯模式)**：當環境無相機鏡頭或硬體測試階段時，提供終端機鍵盤輸入監控，可手動輸入指令測試狀態機轉移。
- **Mosquitto MQTT 通訊**：連結 MQTT Broker 進行動態指令訂閱與系統狀態回傳。
- **Mutex 記憶體鎖 (Data Synchronization)**：保護共享之 FSM 狀態變數與車輛控制指令，防止 Data Race。
- **即時安全避障 (Safety Interrupt)**：當感測距離小於防撞安全閾值 (20 cm) 時，Thread 2 補充或搶佔控制鎖，將系統強制切換至緊急煞車狀態。

## 📁 檔案說明

- `main.c`：系統核心主程式碼，包含多執行緒初始化、MQTT 通訊、Mutex 鎖控制、FSM 邏輯及安全避障機制。

## 🛠️ 編譯與執行

使用 `gcc` 編譯並連結 `mosquitto` 與 `pthread` 函式庫：

```bash
gcc main.c -o main -lmosquitto -lpthread
./main