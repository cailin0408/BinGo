cmake_minimum_required(VERSION 3.13)

# Pico W
set(PICO_BOARD pico_w CACHE STRING "Pico board type")

# Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_minimal_test C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# 初始化 Pico SDK
pico_sdk_init()

# 編譯 main.c
add_executable(pico_minimal_test
    main.c
)

# 讓專案可以找到目前目錄的 lwipopts.h
target_include_directories(pico_minimal_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

# Library
target_link_libraries(pico_minimal_test
    pico_stdlib
    hardware_i2c
    pico_cyw43_arch_lwip_poll
    pico_lwip_mqtt
)

# 使用 USB Serial
pico_enable_stdio_usb(pico_minimal_test 1)

# 關閉 UART Serial
pico_enable_stdio_uart(pico_minimal_test 0)

# 產生 UF2
pico_add_extra_outputs(pico_minimal_test)