cmake_minimum_required(VERSION 3.13)

# 載入 Pico SDK
include(pico_sdk_import.cmake OPTIONAL)

project(l298n_mqtt C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# ⚠️ 必須指定為 pico_w (否則無法使用 CYW43 Wi-Fi 模組)
set(PICO_BOARD pico_w)

pico_sdk_init()

# 建立執行檔 (來源檔名為 l298n.c)
add_executable(l298n
    l298n.c
)

# 連結程式所需的 Pico SDK 與 lwIP / MQTT 函式庫
target_link_libraries(l298n
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background  # 包含 CYW43 驅動與 lwIP 網路協定棧
    pico_lwip_mqtt                              # lwIP 的 MQTT 客戶端函式庫
)

# 啟用 USB Serial 輸出（方便從電腦看 printf log）
pico_enable_stdio_usb(l298n 1)
pico_enable_stdio_uart(l298n 0)

# 自動生成 .uf2 燒錄檔
pico_add_extra_outputs(l298n)