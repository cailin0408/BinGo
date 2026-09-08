# 變數設定（可依需求自行修改）
DEVICE = /dev/video0
HOST = 0.0.0.0
PORT = 8081
RESOLUTION = 640x480
FORMAT = MJPEG
ORIGIN = "*"

.PHONY: run stop

# 預設執行的目標
run:
	ustreamer -d $(DEVICE) -s $(HOST) -p $(PORT) -r $(RESOLUTION) -m $(FORMAT) --allow-origin=$(ORIGIN)

# 停止 ustreamer 服務的快捷指令
stop:
	pkill -f ustreamer
