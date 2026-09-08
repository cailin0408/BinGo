# Hand Tracking - 手勢追蹤與即時影像串流系統

本目錄包含 Hand Tracking 專案的手勢追蹤前端網頁與樹莓派（Raspberry Pi）串流服務配置。此系統整合了 MediaPipe/WebCam 手勢辨識、uStreamer 即時影像串流，並支援 **電腦內建鏡頭** 與 **樹莓派外接鏡頭** 之動態切換與跨來源安全存取設定。

---

## 📁 檔案說明與使用情境

* **`handTracking.html`**：使用**電腦內建鏡頭**直接讀取本機攝影機影像進行手勢辨識與追蹤。  
* **`handTrackingWithRPi.html`**：使用**樹莓派外接鏡頭**透過網路讀取樹莓派所發布的 uStreamer MJPEG 影像串流進行手勢辨識與追蹤。

---

## 🚀 使用指南

- ### 使用電腦鏡頭

1. 雙擊開啟 `handTracking.html`，並確認影像來源設定為 **樹莓派的 IP 位址** 與連接埠。
2. 允許瀏覽器存取攝影機權限即可開始使用。

- ### 使用樹莓派外接鏡頭

  在使用 `handTrackingWithRPi.html` 前，需先於樹莓派上安裝並啟動 **ustreamer** 影像串流服務。

1. 安裝 ustreamer

    在樹莓派終端機中執行以下指令： 
    ```bash
    sudo apt update && sudo apt install -y ustreamer
    ```

2. 啟動串流服務
    
    執行以下指令啟動影像串流（預設使用 /dev/video0，Port 8081）：  
    ```bash
    ustreamer -d /dev/video0 -s 0.0.0.0 -p 8081 -r 640x480 -m MJPEG --allow-origin="*"
    ```

    💡 快捷方式（使用 Makefile）
    專案內附 `Makefile`，可以直接執行：
    * **啟動服務：** `make` 或 `make run`
    * **停止服務：** `make stop`

3. 雙擊開啟 `handTrackingWithRPi.html`，並確認影像來源設定為 **樹莓派的 IP 位址** 與連接埠

---

## ⚙️ 疑難排解：Chrome 瀏覽器跨來源與不安全來源設定 (Chrome Flags)

若開啟 `handTrackingWithRPi.html` 後**無法讀取或顯示樹莓派攝影機畫面**，請依下列步驟調整 Chrome 瀏覽器設定：

1. 在 Chrome 網址列輸入並前往：  
  `chrome://flags/#unsafely-treat-insecure-origin-as-secure`  
2. 找到 **"Insecure origins treated as secure"** 設定項目。  
3. 將狀態切換為 **Enabled（已啟用）**。  
4. 在下方文字輸入框中填入樹莓派的服務網址，例如：
    ```text
    http://192.168.69.144:3000, http://192.168.69.144:8081
    ```
    *(多個網址請用逗號 `,` 隔開)*
5. 點擊右下角 **Relaunch** 按鈕重新啟動瀏覽器以使設定生效。