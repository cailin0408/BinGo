import time
import board
import adafruit_dht
import os

FIFO_PATH = "/tmp/dht11_fifo"

# 建立 IPC Named Pipe 管道
if not os.path.exists(FIFO_PATH):
    os.mkfifo(FIFO_PATH)

# 初始化 DHT11 裝置 (GPIO 4 / Board D4，可依實際接線調整)
dhtDevice = adafruit_dht.DHT11(board.D4)

print("🟢 [Python DHT11] IPC 發送端已啟動，等待 C 主程式連接...")

while True:
    try:
        temperature_c = dhtDevice.temperature
        humidity = dhtDevice.humidity

        if temperature_c is not None and humidity is not None:
            data_str = f"Temp: {temperature_c:.1f} C, Humidity: {humidity}%"
            print(f"[DHT11 讀取成功]: {data_str}")

            # 開啟 FIFO 並寫入數據給 C 語言端
            try:
                # O_NONBLOCK 防止沒有 C 端連接時卡住 Python
                pipe_fd = os.open(FIFO_PATH, os.O_WRONLY | os.O_NONBLOCK)
                os.write(pipe_fd, f"{data_str}\n".encode('utf-8'))
                os.close(pipe_fd)
            except OSError:
                pass # 無 C 端監聽時自動略過

    except RuntimeError as error:
        # DHT11 經常會有讀取失敗狀況，持續重試
        time.sleep(2.0)
        continue
    except Exception as error:
        dhtDevice.exit()
        raise error

    time.sleep(2.0)