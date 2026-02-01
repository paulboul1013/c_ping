CC = gcc
CFLAGS = -O2 -Wall -std=c99
TARGET = ping
OBJS = ping.o utils.o

.PHONY: all clean install help

# 預設目標
all: $(TARGET)

# 連結目標文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@
	@echo ""
	@echo "✓ 編譯成功！"
	@echo ""
	@echo "運行方式："
	@echo "  1. 設置 capabilities（推薦，只需一次）："
	@echo "     make install"
	@echo "     ./$(TARGET) google.com"
	@echo ""
	@echo "  2. 或使用 sudo："
	@echo "     sudo ./$(TARGET) 8.8.8.8"
	@echo ""

# 編譯 ping.o
ping.o: ping.c ping.h utils.h
	$(CC) $(CFLAGS) -c $<

# 編譯 utils.o
utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c $<

# 設置 capabilities（需要 root）
install:
	@if [ ! -f $(TARGET) ]; then \
		echo "錯誤: $(TARGET) 不存在，請先執行 'make'"; \
		exit 1; \
	fi
	@echo "正在設置 capabilities..."
	sudo setcap cap_net_raw+ep ./$(TARGET)
	@echo "✓ 設置成功！"
	@echo ""
	@echo "現在可以直接運行（不需要 sudo）："
	@echo "  ./$(TARGET) google.com"
	@echo "  ./$(TARGET) -n 10 8.8.8.8"

# 清理
clean:
	rm -f *.o $(TARGET)
	@echo "✓ 清理完成"

# 幫助
help:
	@echo "模組化 Ping - Makefile 使用說明"
	@echo ""
	@echo "專案結構："
	@echo "  ping.c    - 主程式"
	@echo "  ping.h    - ICMP ping 相關定義"
	@echo "  utils.c   - 工具函數實作"
	@echo "  utils.h   - 工具函數聲明"
	@echo ""
	@echo "可用的 make 目標："
	@echo "  make         - 編譯所有文件"
	@echo "  make install - 設置 capabilities（需要 sudo）"
	@echo "  make clean   - 清理編譯產物"
	@echo "  make help    - 顯示此幫助訊息"
	@echo ""
	@echo "使用範例："
	@echo "  make clean && make      # 重新編譯"
	@echo "  make install            # 設置權限"
	@echo "  ./ping google.com       # 運行（預設 4 次）"
	@echo "  ./ping -n 10 8.8.8.8    # 指定次數"