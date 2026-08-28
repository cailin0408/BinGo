# 1. 建立並進入 build 資料夾
mkdir build && cd build

# 2. 讓 CMake 根據 CMakeLists.txt 產生 Makefile
cmake ..

# 3. 開始編譯（產出 .uf2 檔案）
make -j4