# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 專案概述

這是一個從頭開始實現的 ICMP ping 工具，使用 C 語言編寫，採用模組化設計。專案實作了 ICMP 協議、校驗和計算、RTT 測量以及原始 socket 操作。

## 建置與執行

```bash
# 建置專案
make

# 設置 capabilities（推薦，只需一次）
make install

# 執行 ping（預設發送 4 次）
./ping google.com
./ping 8.8.8.8

# 指定發送次數
./ping -n 10 1.1.1.1

# 無限發送（Ctrl+C 停止）
./ping -n 0 8.8.8.8

# 顯示幫助
./ping -h

# 清理
make clean
```

編譯器標誌：`-O2 -Wall -std=c99`

**權限需求**：程式需要 root 權限或 `CAP_NET_RAW` capability 來創建原始 socket。

## 架構設計

### 模組化結構

專案採用清晰的模組化設計，將功能分離到不同文件：

#### 1. ping.h - ICMP 協議定義
**職責**：定義 ICMP 相關的數據結構、常量和函數接口

**關鍵結構**：
```c
// ICMP 頭部（8 字節）
typedef struct {
    uint8_t type;          // ICMP 類型
    uint8_t code;          // ICMP 代碼
    uint16_t checksum;     // 校驗和
    uint16_t identifier;   // 識別符（通常用 PID）
    uint16_t sequence;     // 序列號
} icmp_header_t;

// 完整 ICMP 封包（64 字節）
typedef struct {
    icmp_header_t header;
    uint8_t data[56];      // Payload（包含時間戳）
} icmp_packet_t;

// 統計信息
typedef struct {
    int transmitted;       // 已發送
    int received;          // 已接收
    double min_rtt;        // 最小 RTT
    double max_rtt;        // 最大 RTT
    double sum_rtt;        // RTT 總和
} ping_stats_t;
```

**常量定義**：
- `PACKET_SIZE`: 64 字節（標準 ping 封包大小）
- `REQUEST_TIMEOUT_US`: 1,000,000 微秒（1 秒超時）
- `REQUEST_INTERVAL_US`: 1,000,000 微秒（1 秒間隔）

#### 2. ping.c - 主程式實作
**職責**：ICMP ping 核心邏輯、統計、signal 處理

**關鍵函數**：

- **統計函數**：
  - `init_stats()`: 初始化統計結構
  - `update_stats()`: 更新統計（RTT）
  - `print_stats()`: 顯示最終統計

- **ICMP 封包函數**：
  - `create_icmp_packet()`: 創建 ICMP echo request
    - 設置 type=8, code=0
    - 填入 identifier 和 sequence
    - 嵌入時間戳到 data
    - 計算校驗和

- **工具函數**：
  - `set_nonblocking()`: 設置 socket 為非阻塞模式
  - `print_usage()`: 顯示使用說明

- **Signal 處理**：
  - `sigint_handler()`: 處理 Ctrl+C，設置 `keep_running = 0`

- **主函數** (`main()`):
  1. 解析命令列參數（`-n count`, `-h`）
  2. 使用 `getaddrinfo()` 解析主機名/IP
  3. 創建原始 socket (`SOCK_RAW` + `IPPROTO_ICMP`)
  4. 設置非阻塞模式
  5. 主循環：發送 → 接收 → 計算 RTT → 更新統計
  6. 顯示統計並清理

#### 3. utils.h / utils.c - 工具函數
**職責**：通用的時間和網路工具函數

**關鍵函數**：

- `get_time_us()`: 獲取當前時間（微秒精度）
  - 使用 `gettimeofday()`
  - 返回 `uint64_t` 微秒時間戳

- `compute_checksum()`: 計算 RFC 1071 校驗和
  - 累加所有 16 位元字
  - 處理奇數長度（最後一個字節）
  - 處理進位（fold carries）
  - 返回 1's complement

#### 4. Makefile - 建置系統
**職責**：編譯、安裝、清理

**目標**：
- `all`: 編譯所有文件（預設）
- `install`: 設置 `CAP_NET_RAW` capability
- `clean`: 清理編譯產物
- `help`: 顯示幫助訊息

**依賴關係**：
```makefile
OBJS = ping.o utils.o

ping.o: ping.c ping.h utils.h
utils.o: utils.c utils.h
```

### 設計理念

#### 簡化實作
與早期的雙層抽象設計不同，當前實作選擇了更簡化、更實用的方法：

- **不構建 IP 層**：讓 kernel 處理 IP 頭，專注於 ICMP 層
- **直接使用標準結構**：使用 `<netinet/ip_icmp.h>` 標準定義
- **模組化分離**：清晰的職責劃分（協議 / 主邏輯 / 工具）

#### WSL2 兼容性
不使用 `IP_HDRINCL` socket 選項，避免 WSL2 虛擬化網路棧的限制：

```c
// ❌ 舊方法（WSL2 不兼容）
setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

// ✅ 新方法（WSL2 兼容）
// 讓 kernel 自動添加 IP 頭，我們只構建 ICMP 層
```

## 重要實作細節

### ICMP Echo 協議（RFC 792）

**發送封包**（Type 8 - Echo Request）：
```c
void create_icmp_packet(icmp_packet_t *packet, uint16_t id, uint16_t seq) {
    packet->header.type = ICMP_ECHO;  // 8
    packet->header.code = 0;
    packet->header.identifier = htons(id);     // 網路位元組序
    packet->header.sequence = htons(seq);

    // 嵌入時間戳到 data
    uint64_t timestamp = get_time_us();
    memcpy(packet->data, &timestamp, sizeof(timestamp));

    // 計算校驗和
    packet->header.checksum = 0;
    packet->header.checksum = compute_checksum(packet, sizeof(*packet));
}
```

**接收封包**（Type 0 - Echo Reply）：
- 從接收緩衝區解析 IP 頭（獲取 TTL）
- 解析 ICMP 頭（驗證 type=0）
- 驗證 identifier 和 sequence 匹配
- 提取時間戳並計算 RTT

### 校驗和計算（RFC 1071）

Internet Checksum 演算法：

```c
uint16_t compute_checksum(const void *buf, size_t len) {
    const uint16_t *data = buf;
    uint32_t sum = 0;

    // 1. 累加所有 16 位元字
    while (len > 1) {
        sum += *data++;
        len -= 2;
    }

    // 2. 處理奇數字節
    if (len == 1) {
        sum += *(uint8_t *)data;
    }

    // 3. 處理進位（fold carries）
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // 4. 1's complement
    return (uint16_t)~sum;
}
```

**重要**：校驗和計算時，checksum 欄位必須設為 0。

### RTT 測量

**時間戳嵌入**：
```c
// 發送時：嵌入當前時間到 payload
uint64_t send_time = get_time_us();
memcpy(packet->data, &send_time, sizeof(send_time));
```

**RTT 計算**：
```c
// 接收時：提取時間戳並計算差值
uint64_t recv_time = get_time_us();
uint64_t request_time;
memcpy(&request_time, reply->data, sizeof(request_time));
double rtt_ms = (recv_time - request_time) / 1000.0;  // 轉為毫秒
```

**精度**：微秒級時間戳，毫秒級顯示。

### 非阻塞 I/O 和超時處理

**設置非阻塞模式**：
```c
int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}
```

**接收循環**：
```c
uint64_t send_time = get_time_us();

while (!received) {
    ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL);
    uint64_t elapsed = get_time_us() - send_time;

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 非阻塞，暫時無數據
            if (elapsed > REQUEST_TIMEOUT_US) {
                printf("Request timeout for icmp_seq %d\n", seq);
                break;
            }
            usleep(1000);  // 等待 1ms 後重試
            continue;
        }
    }

    // 解析封包...
}
```

### 位元組序處理

**網路位元組序（大端序）vs 主機位元組序**：

需要轉換的欄位：
- `identifier`：`htons()` 發送，`ntohs()` 接收
- `sequence`：`htons()` 發送，`ntohs()` 接收
- `checksum`：自動處理（因為是按字節累加）

不需要轉換：
- `type` 和 `code`：單字節
- IP 地址：`getaddrinfo()` 已處理

### 主機名解析

使用 `getaddrinfo()` 支援域名和 IP 地址：

```c
struct addrinfo hints, *addrinfo_list, *ai;

memset(&hints, 0, sizeof(hints));
hints.ai_family = AF_INET;        // IPv4
hints.ai_socktype = SOCK_RAW;
hints.ai_protocol = IPPROTO_ICMP;

getaddrinfo(hostname, NULL, &hints, &addrinfo_list);

// 遍歷結果創建 socket
for (ai = addrinfo_list; ai != NULL; ai = ai->ai_next) {
    sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (sockfd >= 0) break;
}
```

### 命令列參數解析

使用 `getopt()` 解析參數：

```c
int count = 4;  // 預設 4 次

while ((opt = getopt(argc, argv, "n:h")) != -1) {
    switch (opt) {
        case 'n':
            count = atoi(optarg);
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
    }
}

// count=0 表示無限循環
while (keep_running && (count == 0 || seq < count)) {
    // 發送 ping...
}
```

### Signal 處理

優雅的中斷處理：

```c
volatile sig_atomic_t keep_running = 1;

void sigint_handler(int signum) {
    keep_running = 0;  // 安全地設置標誌
}

int main() {
    signal(SIGINT, sigint_handler);

    while (keep_running && ...) {
        // 主循環
    }

    // 顯示統計後退出
    print_stats(&stats, hostname);
}
```

## 關鍵代碼位置

### ping.c
- 統計函數：37-76 行
- ICMP 封包創建：83-98 行
- 非阻塞設置：105-111 行
- 主函數：133-320 行
  - 參數解析：148-164 行
  - 主機名解析：178-187 行
  - Socket 創建：190-205 行
  - 主循環：235-313 行

### utils.c
- 時間函數：9-13 行
- 校驗和計算：20-41 行

### ping.h
- ICMP 結構：18-32 行
- 統計結構：39-45 行
- 函數聲明：52-92 行

## Socket 配置

```c
// Socket 類型
socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)

// 非阻塞模式
fcntl(sockfd, F_SETFL, flags | O_NONBLOCK)

// 不使用 IP_HDRINCL（讓 kernel 處理 IP 層）
```

## 參考資料

### RFC 標準
- **RFC 792**：ICMP 協議規範
- **RFC 1071**：Internet Checksum 計算
- **RFC 791**：IPv4 協議

### Man Pages
- `man 7 raw`：原始 socket 編程
- `man 7 icmp`：ICMP 協議
- `man getaddrinfo`：主機名解析
- `man sendto`、`man recvfrom`：封包收發
- `man fcntl`：檔案控制操作

### 學習資源
- YouTube 教學：Internet Protocol - Ping from scratch
- ICMP Wikipedia：協議概述
- IPv4 Wikipedia：IP 層理解

## 已實作功能

### 核心功能
- ✅ ICMP echo request/reply（RFC 792）
- ✅ RFC 1071 校驗和計算
- ✅ 微秒級時間戳和 RTT 計算
- ✅ 非阻塞 I/O 和超時處理
- ✅ 主機名解析（`getaddrinfo()`）
- ✅ 命令列參數（`-n count`，預設 4 次）
- ✅ 完整統計（transmitted/received/loss/min/avg/max）
- ✅ Signal 處理（Ctrl+C 優雅退出）
- ✅ WSL2 完全兼容

### 命令列選項
- `-n count`：指定發送次數（0 表示無限）
- `-h`：顯示幫助訊息
- 預設：發送 4 次（類似 Windows ping）

### 輸出格式
```
PING google.com (172.217.160.78) 56 data bytes
64 bytes from 172.217.160.78: icmp_seq=0 ttl=117 time=12.345 ms
...
--- google.com ping statistics ---
4 packets transmitted, 4 received, 0.0% packet loss
rtt min/avg/max = 11.234/12.290/13.456 ms
```

## 已知限制

### 功能限制
- 僅支援 IPv4（不支援 IPv6）
- 固定 1 秒發送間隔（無法自定義）
- 固定超時時間 1 秒
- 無法指定封包大小
- 無進階選項（如 `-i interval`、`-W timeout`）

### 簡化設計
這些限制是有意為之，專案目標是：
1. **學習 ICMP 協議**：理解 echo request/reply 機制
2. **掌握原始 socket**：學習 `SOCK_RAW` 使用
3. **實作核心功能**：發送、接收、RTT、統計
4. **保持簡潔**：避免過度工程化

## WSL2 環境

### 完全兼容
本實作已解決所有 WSL2 兼容性問題：
- ✅ 不使用 `IP_HDRINCL`
- ✅ 支援 loopback (127.0.0.1)
- ✅ 支援公網 IP
- ✅ 支援域名解析

### 權限配置
三種方式（按推薦順序）：

1. **Capabilities（最佳）**：
   ```bash
   make install
   ```

2. **Sudo**：
   ```bash
   sudo ./ping <target>
   ```

3. **配置無密碼 sudo**：
   ```powershell
   # Windows PowerShell（管理員）
   wsl -u root bash -c "echo '$(wsl whoami) ALL=(ALL) NOPASSWD:ALL' | tee /etc/sudoers.d/$(wsl whoami)"
   ```

## 開發指南

### 修改代碼時注意

1. **位元組序**：所有多字節網路欄位需要 `htons()`/`ntohs()`
2. **校驗和**：修改 ICMP 結構後重新計算
3. **記憶體**：`getaddrinfo()` 結果記得 `freeaddrinfo()`
4. **Signal 安全**：handler 中只使用 `sig_atomic_t` 類型
5. **錯誤處理**：Socket 操作檢查返回值和 `errno`

### 編譯警告
使用 `-Wall` 編譯，確保無警告：
```bash
make clean && make
# 應該看到 "✓ 編譯成功！" 且無警告
```

### 測試建議
```bash
# 本地測試
./ping 127.0.0.1

# 公網測試
./ping 8.8.8.8

# 域名測試
./ping google.com

# 不可達測試（應該顯示 timeout）
./ping 192.0.2.1  # TEST-NET-1，不應路由

# 長時間測試（觀察 Ctrl+C 處理）
./ping -n 0 8.8.8.8
# 按 Ctrl+C 應該顯示統計後退出
```

## 專案演進

### 從雙層抽象到簡化設計
早期版本採用雙層抽象（raw structures + application structures），目標是深入理解 IP 層構建。

當前版本簡化為：
- 讓 kernel 處理 IP 層
- 專注於 ICMP 協議實作
- 更好的實用性和兼容性
- 保持教育價值

這個演進過程本身就是一個學習經驗：從理論完整性到實用兼容性的權衡。

---

**最後更新**：2026-02-01
**專案狀態**：功能完整，可用於學習和日常使用
