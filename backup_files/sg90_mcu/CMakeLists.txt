# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.0)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(sg90 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# -------------------------------------------------------------
# 1. 原本的 USB Serial 版本 (生成 sg90.uf2)
# -------------------------------------------------------------
add_executable(sg90 sg90.c)

pico_set_program_name(sg90 "sg90")
pico_set_program_version(sg90 "0.1")

# 啟動 USB 與 UART 輸出
pico_enable_stdio_uart(sg90 1)
pico_enable_stdio_usb(sg90 1)

target_include_directories(sg90 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(sg90 
        pico_stdlib
        hardware_pwm
)

pico_add_extra_outputs(sg90)


# -------------------------------------------------------------
# 2. 新增的 Wi-Fi + MQTT 版本 (生成 sg90_wifi.uf2)
# -------------------------------------------------------------
add_executable(sg90_wifi sg90_wifi.c)

pico_set_program_name(sg90_wifi "sg90_wifi")
pico_set_program_version(sg90_wifi "0.1")

pico_enable_stdio_uart(sg90_wifi 1)
pico_enable_stdio_usb(sg90_wifi 1)

target_include_directories(sg90_wifi PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# 關鍵修改：為 Wi-Fi 版連結 cyw43 (Wi-Fi晶片) 與 lwIP MQTT 函式庫
target_link_libraries(sg90_wifi 
        pico_stdlib
        hardware_pwm
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_mqtt
)

pico_add_extra_outputs(sg90_wifi)