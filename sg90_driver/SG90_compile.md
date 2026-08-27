# SG90 伺服馬達驅動 — 編譯與使用說明

## 懶人包：一次跑完全部指令

已經編譯過、環境設定好的情況下，重新測試時可以直接複製整段貼上：

```bash
sudo rmmod sg90_driver
make clean
make
sudo insmod sg90_driver.ko          # 現在不用帶參數也會用 595
dmesg | tail -n 3
sudo rm -f /dev/sg90
sudo mknod /dev/sg90 c $(dmesg | grep -oP 'Major \K[0-9]+' | tail -1) 0
sudo chmod 666 /dev/sg90
echo "open" > /dev/sg90
echo "close" > /dev/sg90
```

> 第一次使用、或編譯出現錯誤時，請往下看完整步驟（環境需求、常見錯誤排查等）。若馬達沒反應，先看「六、GPIO 編號查詢方式」確認 `595` 這個編號在你的機器上是否仍然正確。

## 一、環境需求

- Raspberry Pi 5（RP1 晶片）
- 已安裝對應目前執行中 kernel 版本的 header 套件

```bash
sudo apt update
sudo apt install raspberrypi-kernel-headers build-essential
```

確認 header 版本與目前執行中的 kernel 一致：

```bash
uname -r
ls /lib/modules/$(uname -r)/build
```

如果 `/lib/modules/$(uname -r)/build` 不存在，代表 header 版本沒對齊，`make` 會找不到編譯環境，需要重新安裝對應版本的 header。

## 二、檔案結構

```
sg90_driver/
├── sg90_driver.c
└── Makefile
```

`Makefile` 內容：

```makefile
obj-m += sg90_driver.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
```

## 三、編譯

```bash
cd sg90_driver
make
```

編譯成功會看到類似以下輸出，特別注意最後一行 `LD [M] sg90_driver.ko`，代表模組檔案真的產生了：

```
CC [M]  sg90_driver.o
MODPOST Module.symvers
CC [M]  sg90_driver.mod.o
CC [M]  .module-common.o
LD [M]  sg90_driver.ko
```

編譯完成後，資料夾裡會多出 `sg90_driver.ko`。

若要重新乾淨編譯（例如改過程式碼後遇到奇怪的殘留錯誤）：

```bash
make clean
make
```

## 四、載入模組

```bash
sudo insmod sg90_driver.ko
```

不帶參數時，預設會使用已驗證正確的 GPIO 編號（`595`，對應 BCM GPIO26）。

若需要指定其他 GPIO 編號（例如換了硬體、kernel 更新導致 gpiochip base 改變）：

```bash
sudo insmod sg90_driver.ko gpio_pin_param=<正確編號>
```

如何查出正確編號請見「六、GPIO 編號查詢方式」。

確認載入成功：

```bash
dmesg | tail -n 5
```

應該會看到類似：

```
SG90: Using GPIO pin: 595
SG90: Driver registered with Major 509 on RP1 GPIO 595
```

## 五、建立裝置節點並測試

驅動內已包含 `class_create()` / `device_create()`，理論上 `insmod` 後會自動建立 `/dev/sg90_dev`：

```bash
ls -l /dev/sg90_dev
```

如果自動建立成功，可以直接用這個路徑測試，不需要手動 `mknod`。若沒有出現，或想用自訂路徑（例如 `/dev/sg90`），可手動建立：

```bash
sudo rm -f /dev/sg90
sudo mknod /dev/sg90 c $(dmesg | grep -oP 'Major \K[0-9]+' | tail -1) 0
sudo chmod 666 /dev/sg90
```

### 測試指令

```bash
echo "open" > /dev/sg90    # 轉到 90 度
echo "close" > /dev/sg90   # 轉到 0 度
echo "45" > /dev/sg90      # 直接指定角度（0~180）
```

### 除錯用指令

```bash
echo "test" > /dev/sg90    # 持續輸出高電位（不會讓馬達轉動，用於量測電位）
echo "test0" > /dev/sg90   # 持續輸出低電位
```

## 六、GPIO 編號查詢方式

如果馬達沒有反應，先確認目前使用的 GPIO 編號是否正確對應到 RP1 晶片：

```bash
for f in /sys/class/gpio/gpiochip*; do
  echo "$f  label=$(cat $f/label)  base=$(cat $f/base)  ngpio=$(cat $f/ngpio)"
done
```

找到輸出中 `label=pinctrl-rp1` 那一行，記下對應的 `base`。

**正確 GPIO 編號 = base + 目標 BCM 腳位號碼**（例如要控制 BCM GPIO26，且 base 是 569，則正確編號為 `595`）。

> gpiochip 的 base 是 kernel 開機時動態分配的，理論上同一台機器、同一版 kernel 通常穩定，但更新 kernel、加裝其他硬體後仍有可能改變，建議每次遇到「驅動載入正常但馬達沒反應」時，先重新查一次這個對照表。

## 七、卸載模組

```bash
sudo rmmod sg90_driver
```

卸載後會自動把 GPIO 電位拉回低電位並釋放腳位，`dmesg` 會顯示：

```
SG90: Driver unregistered
```

## 八、常見編譯錯誤

| 錯誤訊息關鍵字 | 原因 | 解法 |
|---|---|---|
| `passing argument 1 of 'class_create' from incompatible pointer type` | kernel 6.4+ 的 `class_create()` 拿掉了 `THIS_MODULE` 參數 | 本驅動已用 `#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)` 處理版本相容，若仍出現此錯誤，檢查程式碼是否被覆蓋回舊版 |
| `implicit declaration of function 'gpiochip_find'` | 新版 kernel 已移除該函式 | 本驅動已改用 `gpio_pin_param` module parameter 取代，不應再出現此函式呼叫 |
| `insmod: ERROR: could not load module ...: No such file or directory` | `.ko` 檔案不存在，通常是上次 `make` 失敗中斷 | 重新執行 `make clean && make`，確認有印出 `LD [M] sg90_driver.ko` |
| 出現 `stray '\343'` 或中文字元被當成程式碼 | 程式碼註解裡意外包含 `*/`（例如路徑寫成 `gpiochip*/base`），導致 C 語言註解提前結束 | 檢查註解內容，避免出現連續的 `*/` 字元組合 |
