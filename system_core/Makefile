# 設定編譯器與編譯旗標
CC = gcc
CFLAGS = -Wall -O2
LIBS = -lmosquitto -lpthread

# 設定目標執行檔與來源檔案
TARGET = main
SRCS = main.c

# 預設執行的 target
all: $(TARGET)

# 編譯規則
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LIBS)

# 清除編譯檔案
clean:
	rm -f $(TARGET)

# 宣告偽目標，避免與同名檔案衝突
.PHONY: all clean
