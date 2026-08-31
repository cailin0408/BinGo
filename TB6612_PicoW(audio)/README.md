# TB6612_PicoW (audio)

透過瀏覽器語音辨識控制 Pico W + TB6612 驅動板的馬達模組。

## 檔案說明

- `voice_control.html`：瀏覽器語音辨識前端頁面，透過 Web Speech API 將語音轉換為文字，
  比對 FORWARD / BACKWARD / LEFT / RIGHT / STOP / FASTER / SLOWER 等關鍵字，
  透過 MQTT 發送指令到 mosquitto broker。

- `pico_motor.py`：燒錄至 Pico W 的 MicroPython 程式，訂閱 MQTT topic 後，
  直接呼叫 `machine.Pin` 與 `machine.PWM` 控制 TB6612 驅動板，驅動左右輪馬達。

## 接線對照表

| TB6612接腳 | Pico W GPIO |
|---|---|
| STBY | GP8 |
| AIN1 | GP2 |
| AIN2 | GP3 |
| PWMA | GP6 |
| BIN1 | GP4 |
| BIN2 | GP5 |
| PWMB | GP7 |

## 使用方式

1. 將 `pico_motor.py` 存成 `main.py` 燒錄至 Pico W
2. 修改 `voice_control.html` 中的 MQTT broker IP 與 topic
3. 用瀏覽器開啟 `voice_control.html`，連接 MQTT 後開始語音操控
