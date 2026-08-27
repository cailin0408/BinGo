# SG90 伺服馬達 Linux Kernel 驅動除錯報告

## 一、問題現象

原始的 `sg90_driver.c` 可以正常編譯、`insmod` 也不報錯，`dmesg` 顯示驅動成功註冊並取得 GPIO，但寫入指令（`echo "open" > /dev/sg90`）後**伺服馬達完全沒有反應**，且線路已事先確認接線正確。

## 二、根本原因（共三個，疊加在一起）

### 原因 1：GPIO 編號用寫死的猜測值，指到了錯誤的實體腳位（核心問題）

原始程式碼假設 Raspberry Pi 5 的 RP1 晶片 gpiochip base 固定是 `512`：

```c
int gpio_base = 512; // Raspberry Pi 5 RP1 晶片 GPIO4 的預設起始 Base
rp1_gpio_pin = gpio_base + SERVO_PIN_NUM;  // 512 + 26 = 538
```

但 Linux kernel 的 gpiochip base 是**開機時依各驅動註冊順序動態分配**的，不是一個固定值。實際查詢後發現，這台機器上：

| gpiochip | label | base |
|---|---|---|
| gpiochip512 | gpio-brcmstb@107d517c00（電源/SD 卡相關） | 512 |
| gpiochip527 | gpio-brcmstb@107d517c20（HDMI/PMIC 相關） | 527 |
| gpiochip533 | gpio-brcmstb@107d508500（藍牙/WiFi 相關） | 533 |
| gpiochip565 | gpio-brcmstb@107d508520（WiFi SDIO） | 565 |
| **gpiochip569** | **pinctrl-rp1（伺服馬達訊號腳所在的晶片）** | **569** |

`512 + 26 = 538` 這個編號，實際上落在 `gpiochip533`（藍牙/WiFi 相關的晶片）範圍內的第 5 號腳，而且那個腳位本身是空的（未接線）。也就是說：**驅動程式確實有把某根 GPIO 拉高，只是拉高的是一根完全沒接線、與伺服馬達無關的腳位。**

正確算法應該是：`569（pinctrl-rp1 的 base） + 26（GPIO26） = 595`。

### 原因 2：`/dev` 裝置節點沒有自動建立

原始程式碼只呼叫了 `register_chrdev()` 取得 major number，但沒有呼叫 `class_create()` / `device_create()`。這代表 `/dev/sg90_dev`（或任何裝置節點）**不會被 udev 自動建立**，必須每次手動用 `mknod` 建立，且每次 `insmod` 重新分配的 major number 若沒更新節點，`echo` 寫入就會失敗或指到錯誤的裝置。

### 原因 3：kernel API 版本差異導致編譯失敗（除錯過程中額外遇到的阻礙）

在依序修正上述問題、加入 `class_create()` 與動態尋找 gpiochip（`gpiochip_find()`）的過程中，發現這台機器的 kernel（6.18.39）已經：
- 將 `class_create()` 的參數從 `class_create(THIS_MODULE, name)` 改為 `class_create(name)`（6.4+ 版本變更）
- 完全移除 `gpiochip_find()`，legacy 整數式 GPIO API 已不再提供簡單的方式在模組內動態列舉 gpiochip

這代表原本設計的「自動偵測」思路在新版 kernel 上行不通，必須改變策略。

## 三、除錯過程（如何一步步縮小範圍）

除錯的核心邏輯是：**把「軟體邏輯有沒有執行」「電位有沒有真的改變」「硬體本身能不能動」這三層問題逐一隔離**，而不是直接對著整份程式碼猜。

1. **確認 `/dev` 節點是否存在**：發現原始碼缺少 `device_create()`，這是第一個修正點。

2. **確認 `write()` 系統呼叫是否真的觸發到驅動邏輯**：在 `dev_write()` 裡本來就有的 `printk`（`SG90: Set angle to %d`）派上用場，執行 `echo "open" > /dev/sg90` 後檢查 `dmesg`，確認這行有印出來 → 證明驅動邏輯層完全正常，問題不在 `write()` 綁定。

3. **排除 PWM 時序問題**：伺服馬達需要精確的脈衝寬度，用肉眼或簡單測試很難判斷「脈衝送出去了但角度不對」還是「訊號根本沒送出去」。因此新增一個除錯用指令 `test`，讓驅動直接把腳位鎖在**持續高電位**（不是 PWM），方便單獨驗證這根腳位是否真的有訊號。

4. **沒有三用電表可用時，改用「已知一定正常的工具」做交叉驗證**：使用 Python 的 `gpiozero` 函式庫直接控制同一根 GPIO26（`Servo(26)`），結果**馬達會動**。這一步是關鍵的隔離測試：
   - 如果 `gpiozero` 也不能讓馬達動 → 問題在硬體（接線、供電、馬達本身）
   - 如果 `gpiozero` 能讓馬達動，但我們的驅動不行 → 問題 100% 在驅動程式的 GPIO 編號算錯

   測試結果證明是後者。

5. **找出正確的 GPIO 編號**：透過
   ```bash
   for f in /sys/class/gpio/gpiochip*; do
     echo "$f label=$(cat $f/label) base=$(cat $f/base)"
   done
   ```
   逐一列出所有 gpiochip 的 `label` 與 `base`，找到 `label=pinctrl-rp1`（即 RP1 晶片，伺服馬達訊號腳所在的控制器）對應的 `base=569`，算出正確編號 `569+26=595`。

6. **驗證修正結果**：改用 `595` 重新測試，`open`／`close` 指令讓馬達正確轉動；`test`（持續高電位）沒有反應，但這是**正常現象**——伺服馬達靠脈衝寬度判斷角度，持續高電位不是合法的 PWM 訊號，不代表 GPIO 錯誤，只是不適合拿來讓馬達真的轉動（適合拿來配合電表量測電位用）。

## 四、最終修改內容總結

| 項目 | 原始程式碼 | 修正後 |
|---|---|---|
| GPIO 編號 | 寫死猜測 `512 + 26 = 538`（錯誤，指到 WiFi/藍牙晶片的空腳位） | 改為可由 `insmod` 傳入的 module parameter `gpio_pin_param`，預設值改為實測驗證正確的 `595` |
| `/dev` 節點 | 無，需手動 `mknod` | 加入 `class_create()` + `device_create()`，`insmod` 後自動建立裝置節點 |
| `class_create()` 呼叫 | 舊版寫法 `class_create(THIS_MODULE, name)`，新版 kernel 編譯失敗 | 用 `#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)` 判斷式相容新舊 kernel |
| RP1 gpiochip 動態偵測 | 無此機制 | 曾嘗試用 `gpiochip_find()` 動態尋找，但該函式在新版 kernel 已移除，最終改用「module parameter + 文件記錄查詢方式」取代自動偵測 |
| 除錯輔助 | 無 | 新增 `test` / `test0` 指令，可讓腳位持續輸出高/低電位，方便用電表或替代工具驗證電位變化 |
| PWM 忙等機制 | 全程用 `udelay()` 忙等，長延遲時佔用 CPU | 高電位脈衝維持用 `udelay()`（需要精確時序），低電位空檔改用 `usleep_range()` 讓出 CPU，減少不必要的忙等 |

## 五、心得 / 可帶入報告的結論

這次問題的根本原因**並非程式邏輯錯誤**（PWM 計算、指令解析都是對的），而是**對硬體平台特性的假設錯誤**——把一個應該動態查詢的數值（gpiochip base）寫死成猜測值。這類問題的特徵是「程式碼本身完全不報錯、日誌看起來一切正常，但實際行為就是不對」，因此無法單靠讀程式碼發現，必須透過**分層隔離測試**（驗證每一層：write 有沒有觸發 → 電位有沒有變化 → 硬體本身能不能動）才能定位問題所在。這也是一般韌體/驅動除錯中常見且重要的方法論。
