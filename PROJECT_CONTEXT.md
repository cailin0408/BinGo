# Project Directory Rules

本專案的 Root Directory 為：

/home/pi/garbage

所有本專案程式都必須建立在此目錄底下。

專案預計結構：

garbage/
├── PROJECT_CONTEXT.md
├── server/
├── controller/
├── driver/
├── pico/
└── test/

各目錄用途：

## server/

Node.js 相關程式。

例如：

server/server.js
server/package.json

負責：
- Node.js Web Server
- WebSocket
- Command parsing
- FIFO IPC

---

## controller/

Raspberry Pi Linux User Space C 程式。

例如：

controller/main.c
controller/buzzer.c
controller/buzzer.h
controller/Makefile

負責：
- FIFO read
- Command handling
- 呼叫 Device Driver
- Buzzer control logic

---

## driver/

Linux Kernel Device Driver。

例如：

driver/buzzer_driver.c
driver/Makefile

負責：
- Kernel Module
- Character Device
- /dev/buzzer
- GPIO control

---

## pico/

Raspberry Pi Pico W 程式。

例如：

pico/main.c
pico/CMakeLists.txt

負責：
- Pico SDK
- C
- PWM
- Passive Buzzer
- 不同頻率提示音

---

## test/

測試程式與暫時測試檔案。

正式程式不要放在 test/。

---

# AI File Creation Rules

AI 在建立程式之前，必須先確認目前正在開發哪一個 Phase。

不要把程式直接建立在 /home/pi/garbage 根目錄。

Node.js 程式放 server/。

Linux User Space C 程式放 controller/。

Linux Device Driver 放 driver/。

Pico W 程式放 pico/。

測試程式放 test/。

除非使用者要求，不要自行建立其他目錄。

如果需要新增目錄或改變 Project Structure，先說明原因。
# Buzzer Functional Design


本專題使用兩種不同類型的蜂鳴器。

兩顆蜂鳴器不做完全相同的功能，而是分成：

1. **操作回饋音**
2. **安全警報音**

藉此同時展示：

* Pico W
* C
* PWM
* Frequency Control
* Raspberry Pi 5
* Linux Device Driver
* Kernel Module
* GPIO Control

---

# 1. Passive Buzzer — 操作回饋音

## Hardware

使用：

**Passive Buzzer / 無源蜂鳴器**

控制端：

**Raspberry Pi Pico W**

主要技術：

```text
Pico W
  |
  | C / Pico SDK
  v
PWM
  |
  | Frequency / Duty Cycle
  v
Passive Buzzer
```

Passive Buzzer 本身不產生固定音調，因此由 Pico W 產生 PWM 訊號。

透過改變 PWM Frequency 與持續時間，可以產生不同的提示音。

---

# 2. Passive Buzzer Purpose

Passive Buzzer 定位為：

**User Operation Feedback / 使用者操作回饋音**

當系統成功辨識不同手勢 Command 時，使用不同聲音回饋使用者。

例如：

```text
Gesture
   |
   v
MediaPipe
   |
   | WebSocket
   v
Node.js
   |
   v
Command
   |
   v
Pico W
   |
   | PWM
   v
Passive Buzzer
```

---

# 3. Gesture Sound Design

目前 Gesture Command：

```text
COME
OPEN_LID
STOP
UNKNOWN
```

預計聲音：

## COME

代表：

**開始跟隨 / 前進**

聲音：

```text
短嗶一聲
```

例如：

```text
1000 Hz
100 ms
```

概念：

```text
COME
 ↓
Pico W
 ↓
1000 Hz PWM
 ↓
BEEP
```

用途：

讓使用者知道系統已成功收到跟隨指令。

---

## OPEN_LID

代表：

**開啟垃圾桶蓋**

聲音：

```text
短嗶兩聲
```

或：

```text
1000 Hz
   ↓
1500 Hz
```

產生由低到高的提示音。

概念：

```text
OPEN_LID
   ↓
Pico W
   ↓
1000 Hz
   ↓
1500 Hz
   ↓
BEEP BEEP
```

用途：

表示系統收到開蓋指令。

---

## STOP

代表：

**停止移動**

聲音：

```text
較低頻率長嗶一聲
```

例如：

```text
700 Hz
300 ms
```

概念：

```text
STOP
 ↓
Pico W
 ↓
700 Hz PWM
 ↓
Long BEEP
```

用途：

讓使用者知道系統已進入停止狀態。

---

## UNKNOWN

UNKNOWN 不直接產生一般操作提示音。

因為 UNKNOWN 可能只是：

* 手勢切換過程
* 短暫辨識失敗
* 手離開 Camera
* MediaPipe 暫時無法判斷

因此：

```text
Single / Short UNKNOWN
        |
        v
No Buzzer
```

最終 UNKNOWN timeout 行為尚未完全決定。

AI 不可自行決定。

---

# 4. Active Buzzer — Safety Alarm

## Hardware

使用：

**Active Buzzer / 有源蜂鳴器**

控制端：

**Raspberry Pi 5**

主要技術：

```text
User Space
    |
    | write() / ioctl()
    v
/dev/buzzer
-------------------------
Kernel Space
-------------------------
Buzzer Device Driver
    |
    | GPIO High / Low
    v
Active Buzzer
```

Active Buzzer 主要不負責產生不同音調。

它的主要功能是：

**Safety Alarm / 安全警報**

---

# 5. Active Buzzer Purpose

Active Buzzer 用於系統發生危險或異常狀況。

可能觸發條件：

```text
Obstacle Detected
Timeout
Communication Lost
Continuous UNKNOWN
System Warning
Emergency Stop
```

實際觸發條件需與組員整合後確認。

不要自行假設最終條件。

---

# 6. Obstacle Warning

專題具有 Ultrasonic Sensor。

當其他模組判斷障礙物距離過近時，可以發出安全事件。

預計流程：

```text
Ultrasonic Sensor
        |
        v
Obstacle Detected
        |
        v
C Controller
        |
        | write()
        v
/dev/buzzer
        |
        v
Linux Buzzer Driver
        |
        | GPIO
        v
Active Buzzer
        |
        v
ALARM
```

此功能主要展示：

**Linux Device Driver 控制實體硬體。**

---

# 7. Communication Timeout

如果控制系統長時間沒有收到有效 Command，未來可以加入 Fail-Safe 機制。

例如：

```text
Last Valid Command
       |
       | No command
       |
       | Timeout
       v
Safe STOP
       +
Active Buzzer
```

Timeout 時間目前尚未決定。

不可自行設定最終數值。

---

# 8. Continuous UNKNOWN

UNKNOWN 不應因單次辨識失敗立即觸發 Alarm。

例如：

```text
COME
COME
UNKNOWN
COME
COME
```

中間一次 UNKNOWN 可以忽略。

但未來可能設計：

```text
UNKNOWN
   ↓
UNKNOWN
   ↓
UNKNOWN
   ↓
Timeout
   ↓
Safe STOP
   +
Active Buzzer Alarm
```

此策略目前屬於預計設計。

最終行為需要與組員確認。

---

# 9. Two-Buzzer Architecture

完整蜂鳴器架構：

```text
                   Buzzer System
                         |
             +-----------+-----------+
             |                       |
             v                       v
      Operation Feedback        Safety Alarm
             |                       |
             v                       v
          Pico W                   RPi5
             |                       |
          C / SDK              User Space
             |                       |
            PWM                 /dev/buzzer
             |                       |
             |                 Kernel Driver
             |                       |
             |                     GPIO
             v                       v
     Passive Buzzer           Active Buzzer
       無源蜂鳴器               有源蜂鳴器
             |                       |
             |                       |
      Different Tones          ON / OFF Alarm
```

---

# 10. Functional Separation

兩種蜂鳴器功能不要混在一起。

## Passive Buzzer

負責：

**一般操作回饋**

例如：

```text
COME       -> Short Beep
OPEN_LID   -> Double Beep
STOP       -> Long / Low Beep
```

重點：

```text
Pico W
C
PWM
Frequency
Tone
```

---

## Active Buzzer

負責：

**安全與異常警報**

例如：

```text
Obstacle
Timeout
Communication Error
Emergency Stop
```

重點：

```text
RPi5
Linux Kernel
Device Driver
Character Device
GPIO
```

---

# 11. Demo Scenario

未來 Demo 可以按照以下方式呈現。

## Scenario 1 — COME

使用者比出 COME 手勢：

```text
Gesture
 ↓
COME
 ↓
WebSocket
 ↓
Node.js
 ↓
Controller
 ↓
開始跟隨

同時：

Pico W
 ↓
Passive Buzzer
 ↓
短嗶一聲
```

表示：

**Command Accepted**

---

## Scenario 2 — OPEN_LID

使用者比出 OPEN_LID：

```text
Gesture
 ↓
OPEN_LID
 ↓
開啟垃圾桶蓋

同時：

Pico W
 ↓
Passive Buzzer
 ↓
嗶嗶兩聲
```

表示：

**Open Lid Command Accepted**

---

## Scenario 3 — STOP

使用者比出 STOP：

```text
Gesture
 ↓
STOP
 ↓
停止移動

同時：

Pico W
 ↓
Passive Buzzer
 ↓
較低頻率長嗶
```

表示：

**Stop Command Accepted**

---

## Scenario 4 — Obstacle

車子移動過程偵測到障礙物：

```text
Ultrasonic Sensor
       |
       v
Obstacle Detected
       |
       +------> Motor STOP
       |
       v
C Controller
       |
       v
/dev/buzzer
       |
       v
Linux Device Driver
       |
       v
Active Buzzer
       |
       v
ALARM
```

表示：

**Safety Warning**

---

# 12. Why Two Different Buzzers?

本專題刻意使用兩種不同蜂鳴器，目的不是重複相同功能。

兩種蜂鳴器分別展示不同 Embedded System 技術。

```text
Passive Buzzer
      |
      v
Pico W
      |
      v
PWM / Frequency
      |
      v
Operation Feedback


Active Buzzer
      |
      v
Raspberry Pi 5
      |
      v
Linux Device Driver
      |
      v
GPIO
      |
      v
Safety Alarm
```

因此可以同時展示：

### MCU Side

```text
Pico W
C
Pico SDK
PWM
GPIO
Frequency Control
```

### Embedded Linux Side

```text
Raspberry Pi 5
Linux Kernel
Kernel Module
Character Device
Device Driver
GPIO
User Space / Kernel Space
```

---

# 13. Presentation Explanation

報告時可以使用以下概念說明：

「我的蜂鳴器模組分成兩種用途。

第一種是 Pico W 控制的無源蜂鳴器，主要作為使用者操作回饋。因為無源蜂鳴器需要外部產生頻率，所以我會利用 Pico W 的 PWM，針對 COME、OPEN_LID、STOP 等不同指令產生不同頻率與節奏的提示音。

第二種則是 Raspberry Pi 5 控制的有源蜂鳴器，主要作為系統安全警報。這部分會透過我自己實作的 Linux Device Driver 控制 GPIO，在偵測到障礙物或其他異常狀況時啟動警報。

因此兩顆蜂鳴器不是做重複功能，而是分別展示 Pico W 的 PWM 控制，以及 Embedded Linux Kernel Device Driver 的硬體控制。」

---

# 14. Current Implementation Priority

目前不要同時實作兩顆蜂鳴器。

建議依序完成：

```text
Step 1
確認兩顆 Buzzer 實際規格

        ↓

Step 2
Pico W + Passive Buzzer
PWM 發聲測試

        ↓

Step 3
完成不同 Tone

COME
OPEN_LID
STOP

        ↓

Step 4
RPi5 + Active Buzzer
基本 GPIO 測試

        ↓

Step 5
Linux Buzzer Driver

        ↓

Step 6
/dev/buzzer

        ↓

Step 7
Node.js / FIFO / C

        ↓

Step 8
MediaPipe Integration

        ↓

Step 9
Obstacle / Safety Integration
```

每完成一個 Step 必須先測試成功，再進下一個 Step。

# Gesture Module

## File Location

目前已取得組員提供的 MediaPipe 手勢辨識程式。

實際檔案位置：

```text
/home/pi/garbage/gesture/handTracking.html
```

目前 Project Structure：

```text
/home/pi/garbage/
│
├── PROJECT_CONTEXT.md
│
├── gesture/
│   └── handTracking.html
│
├── server/
│   ├── server.js
│   └── package.json
│
├── controller/
│   ├── main.c
│   ├── buzzer.c
│   ├── buzzer.h
│   └── Makefile
│
├── driver/
│   ├── buzzer_driver.c
│   └── Makefile
│
├── pico/
│   ├── main.c
│   └── CMakeLists.txt
│
└── test/
```

注意：

目前除了：

```text
PROJECT_CONTEXT.md
gesture/handTracking.html
```

之外，其餘目錄與檔案可能尚未建立。

AI 必須依照目前 Development Phase 逐步建立，不要一次建立完整專案。

---

# Gesture Module Ownership

`gesture/handTracking.html` 為其他組員提供的手勢辨識模組。

主要負責：

- Camera
- MediaPipe Hands
- Hand Landmark Detection
- Gesture Recognition
- Gesture UI
- Command Generation

此檔案的 Gesture Recognition 核心邏輯由其他組員負責。

AI 不要自行重寫或大幅修改：

```text
predictGesture()
MediaPipe Hands configuration
Hand Landmark logic
Gesture Recognition logic
UI layout
```

除非使用者明確要求。

未來我的工作主要是將此模組產生的 Command 接到我的 Node.js Server。

---

# Gesture Recognition Current Status

目前 `gesture/handTracking.html` 已經可以產生以下 Command：

```text
COME
OPEN_LID
STOP
UNKNOWN
```

目前手勢對應：

```text
布 / 五指張開
        ↓
COME
        ↓
前進跟隨


讚 / Thumb Up
        ↓
OPEN_LID
        ↓
自動開蓋


石頭 / 握拳
        ↓
STOP
        ↓
煞車停止


其他無法辨識的手勢
        ↓
UNKNOWN
```

目前程式核心輸出形式為：

```javascript
{
    name: "...",
    cmd: "COME"
}
```

例如：

```javascript
{
    name: "布 ✋ (前進跟隨)",
    cmd: "COME"
}
```

---

# Current Gesture Data Flow

目前實際完成的資料流：

```text
Camera
   ↓
MediaPipe Hands
   ↓
Hand Landmarks
   ↓
predictGesture()
   ↓
Gesture Command
   ↓
COME / OPEN_LID / STOP / UNKNOWN
   ↓
UI Update
   +
console.log()
```

目前資料流到這裡結束。

也就是：

```text
Gesture Recognition
        ↓
Command
        ↓
Console / UI
```

目前尚未真正傳送到 Raspberry Pi Node.js Server。

---

# Current Gesture Change Logic

目前 `handTracking.html` 使用：

```javascript
if (gesture.cmd !== lastGestureCmd)
```

判斷 Gesture 是否改變。

因此目前行為概念為：

```text
UNKNOWN
   ↓
COME
   ↓
處理一次 COME

COME
 ↓
COME
 ↓
COME
 ↓
不會重複進入 Gesture Change Block

COME
 ↓
STOP
 ↓
處理一次 STOP
```

這個機制目前主要用於：

- UI 更新
- Console Log
- Card Highlight

目前不要直接把 WebSocket Send 綁死在這個 Gesture Change Block 裡。

原因：

如果未來只在 Gesture Change 時傳送 Command：

```text
UNKNOWN
   ↓
COME
   ↓
Send COME once
```

如果該次 Command 因連線或其他問題沒有成功送達，持續保持 COME 時可能不會再次傳送。

---

# Planned Gesture Transmission Architecture

後續預計將：

```text
Gesture Recognition
```

與：

```text
Command Transmission
```

分開處理。

概念：

```text
MediaPipe
可能持續高頻率辨識
        ↓
predictGesture()
        ↓
currentGestureCmd
        ↓
儲存目前狀態
        ↓
Transmission Timer
        ↓
固定週期讀取 currentGestureCmd
        ↓
WebSocket
        ↓
Node.js Server
```

例如持續辨識到 COME：

```text
MediaPipe:

COME COME COME COME COME COME COME ...
                │
                ↓
       currentGestureCmd
              COME
                │
                │ 固定週期
                ↓
            WebSocket
                │
                ├── COME
                │
                ├── COME
                │
                └── COME
```

目前考慮的傳送頻率約：

```text
500 ms ~ 1 second
```

但：

**最終傳送頻率尚未確定。**

AI 不可自行將 500 ms 或 1 second 當成最終規格。

---

# No Hand Handling

目前 `handTracking.html` 在完全沒有偵測到手掌時，主要行為是：

```text
UI 顯示：
等待手掌進入畫面...

lastGestureCmd = ""
```

因此目前：

```text
無法辨識手勢
```

與：

```text
完全沒有手
```

並不是完全相同的程式狀態。

目前：

```text
無法辨識手勢
→ UNKNOWN

沒有偵測到手
→ ""
```

後續整合前需要決定：

是否統一成：

```text
No Hand
   ↓
UNKNOWN
```

此行為目前尚未定案。

AI 不可自行修改。

---

# WebSocket Integration Status

目前 `gesture/handTracking.html`：

```text
WebSocket Client        NOT IMPLEMENTED
```

目前尚未完成：

- WebSocket Connection
- Raspberry Pi Server IP
- WebSocket Port
- WebSocket Message Format
- Command Send
- Periodic Send
- Connection Status
- Disconnect Handling
- Reconnect Handling

因此目前：

```text
handTracking.html
       X
       X 尚未連線
       X
Node.js Server
```

---

# Planned WebSocket Architecture

未來預計：

```text
gesture/handTracking.html
        |
        | WebSocket
        v
server/server.js
        |
        | FIFO
        v
controller/main.c
```

完整概念：

```text
Camera
   ↓
MediaPipe
   ↓
Gesture
   ↓
COME / OPEN_LID / STOP / UNKNOWN
   ↓
WebSocket
   ↓
Node.js Server
   ↓
Command Mapping
   ↓
FIFO
   ↓
C Controller
   ↓
Hardware
```

---

# WebSocket Message Format

目前尚未確定最終 WebSocket Message Format。

可能採用：

```json
{
    "cmd": "COME"
}
```

其他 Command：

```json
{
    "cmd": "OPEN_LID"
}
```

```json
{
    "cmd": "STOP"
}
```

```json
{
    "cmd": "UNKNOWN"
}
```

注意：

以上 JSON Format 目前只是預計方案。

正式實作前需要與組員確認。

AI 不可自行將此格式視為最終 Protocol。

---

# Integration Boundary

我的主要整合邊界為：

```text
[其他組員]

gesture/handTracking.html
MediaPipe
Gesture Recognition
predictGesture()

        ↓

COME
OPEN_LID
STOP
UNKNOWN

=============================
       Integration Boundary
=============================

        ↓

[我的部分]

WebSocket
        ↓
Node.js Server
        ↓
FIFO
        ↓
C Controller
        ↓
Buzzer Control
```

因此：

**原則上不要修改組員的 Gesture Recognition Algorithm。**

未來如果需要修改 `handTracking.html`，應只針對：

```text
WebSocket Client
Command Transmission
Connection Handling
```

進行最小必要修改。

---

# Updated Current Development Status

目前已完成/取得：

```text
Raspberry Pi 5             READY
Raspberry Pi OS            READY
VS Code Remote SSH         READY
Project Root               READY
PROJECT_CONTEXT.md         READY

Gesture HTML               RECEIVED
MediaPipe Hands            IMPLEMENTED
Gesture Recognition        IMPLEMENTED

COME                       IMPLEMENTED
OPEN_LID                   IMPLEMENTED
STOP                       IMPLEMENTED
UNKNOWN                    IMPLEMENTED
```

目前尚未完成：

```text
Pico W Buzzer Test         NOT STARTED
Passive Buzzer PWM         NOT STARTED
Active Buzzer GPIO         NOT STARTED
Linux Buzzer Driver        NOT STARTED
Kernel Building            NOT STARTED

Node.js Server             NOT STARTED
WebSocket Client           NOT STARTED
FIFO                       NOT STARTED
C Controller               NOT STARTED

Full Integration           NOT STARTED
```

---

# Current Phase

目前仍位於：

```text
Phase 1
Environment / Hardware Confirmation
```

雖然已經取得：

```text
gesture/handTracking.html
```

但目前不要因為取得 Gesture Code 就提前開始完整 WebSocket Integration。

目前優先順序仍然：

```text
Phase 1
確認硬體
    ↓
Phase 2
Pico W + Passive Buzzer
    ↓
第一次成功發聲
    ↓
Phase 3
COME / OPEN_LID / STOP Tone
    ↓
Phase 4
RPi5 + Active Buzzer
    ↓
Phase 5
Linux Device Driver
    ↓
...
    ↓
Node.js / WebSocket Integration
```

---

# AI Rule for handTracking.html

AI 在閱讀：

```text
gesture/handTracking.html
```

後可以：

- 分析程式
- 解釋程式
- 找出 Command Output
- 規劃 WebSocket Interface
- 規劃 Integration

但目前：

**不要修改 handTracking.html。**

除非使用者明確說：

```text
開始進行 WebSocket Integration
```

才允許對此檔案進行最小必要修改。

任何修改前必須先說明：

1. 要修改哪個區塊
2. 為什麼修改
3. 是否影響組員原本 Gesture Recognition
4. 修改後資料如何傳送到 Node.js