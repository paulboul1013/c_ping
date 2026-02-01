# c_ping

ICMP ping 工具，使用 C 語言編寫，採用模組化設計。

##  特性

-  完整的 ICMP echo request/reply 實作
-  支援主機名解析（域名和 IP 地址）
-  RFC 1071 校驗和計算
-  微秒級 RTT 測量和統計
-  非阻塞 I/O 和超時處理
-  封包統計（發送/接收/丟包率）
-  命令列參數支援（-n 指定次數）
-  優雅的信號處理（Ctrl+C 停止）
-  WSL2 完全兼容
-  模組化程式碼架構

## 快速開始

### 編譯

```bash
make
```

### 執行

**方式一：使用 capabilities（推薦，只需設置一次）**

```bash
make install           # 設置 CAP_NET_RAW 權限
./ping google.com      # 不需要 sudo
./ping -n 10 8.8.8.8   # 發送 10 次
```

**方式二：使用 sudo**

```bash
sudo ./ping google.com
sudo ./ping -n 5 1.1.1.1
```

### 使用範例

```bash
# 預設發送 4 次
./ping google.com

# 指定發送次數
./ping -n 10 8.8.8.8

# 無限發送（按 Ctrl+C 停止）
./ping -n 0 1.1.1.1

# 顯示幫助
./ping -h
```

### 預期輸出

```
PING google.com (172.217.160.78) 56 data bytes
64 bytes from 172.217.160.78: icmp_seq=0 ttl=117 time=12.345 ms
64 bytes from 172.217.160.78: icmp_seq=1 ttl=117 time=11.234 ms
64 bytes from 172.217.160.78: icmp_seq=2 ttl=117 time=13.456 ms
64 bytes from 172.217.160.78: icmp_seq=3 ttl=117 time=12.123 ms

--- google.com ping statistics ---
4 packets transmitted, 4 received, 0.0% packet loss
rtt min/avg/max = 11.234/12.290/13.456 ms
```

### 模組說明

| 模組 | 功能 | 關鍵函數 |
|------|------|----------|
| **ping.c** | 主程式邏輯 | `main()`, `create_icmp_packet()`, `init_stats()` |
| **ping.h** | ICMP 協議定義 | `icmp_header_t`, `icmp_packet_t`, `ping_stats_t` |
| **utils.c** | 通用工具函數 | `get_time_us()`, `compute_checksum()` |
| **utils.h** | 工具函數介面 | 時間和網路相關函數聲明 |

## 技術細節

### ICMP 協議實作

本專案實作了 RFC 792 定義的 ICMP Echo Request/Reply：

- **Type 8 (Echo Request)**: 發送的封包類型
- **Type 0 (Echo Reply)**: 接收的回應類型

### 封包結構

```c
typedef struct {
    uint8_t type;          // ICMP 類型
    uint8_t code;          // ICMP 代碼
    uint16_t checksum;     // RFC 1071 校驗和
    uint16_t identifier;   // 進程 ID
    uint16_t sequence;     // 序列號
} icmp_header_t;

typedef struct {
    icmp_header_t header;
    uint8_t data[56];      // 包含時間戳的 payload
} icmp_packet_t;
```

### 校驗和計算

實作 RFC 1071 定義的網際網路校驗和：
- 累加所有 16 位元字
- 處理進位（fold carries）
- 取 1's complement

### RTT 測量

使用 `gettimeofday()` 進行微秒級時間測量：
1. 發送前在 payload 中嵌入時間戳
2. 接收到回應後提取時間戳
3. 計算往返時間（RTT）

### Socket 設定

- **類型**: `SOCK_RAW` + `IPPROTO_ICMP`
- **模式**: 非阻塞 I/O (`O_NONBLOCK`)
- **超時**: 1 秒接收超時
- **間隔**: 1 秒發送間隔

##  Makefile 命令

```bash
make           # 編譯所有文件
make install   # 設置 capabilities（需要 sudo）
make clean     # 清理編譯產物
make help      # 顯示幫助訊息
```

##  WSL2 支援

本專案完全支援 WSL2 環境，不使用 `IP_HDRINCL` 選項，讓核心處理 IP 層。

### 權限設置（三種方式）

1. **Capabilities（推薦）**:
   ```bash
   make install
   ```

2. **Sudo**:
   ```bash
   sudo ./ping <target>
   ```

3. **Setuid（不推薦，安全風險）**:
   ```bash
   sudo chown root:root ./ping
   sudo chmod u+s ./ping
   ```

## 📚 參考資料

### RFC 標準
- [RFC 792](https://www.rfc-editor.org/rfc/rfc792) - ICMP 協議
- [RFC 1071](https://www.rfc-editor.org/rfc/rfc1071) - 校驗和計算
- [RFC 791](https://www.rfc-editor.org/rfc/rfc791) - IPv4 協議

### 教學資源
- [ICMP - Wikipedia](https://en.wikipedia.org/wiki/Internet_Control_Message_Protocol)
- [IPv4 - Wikipedia](https://en.wikipedia.org/wiki/IPv4)
- [RFC 1071 翻譯及應用](https://dingdingqiuqiu.github.io/2024/11/29/RFC1071%E7%BF%BB%E8%AF%91%E5%8F%8A%E5%85%B6%E5%BA%94%E7%94%A8/)

### 影片教學
- [Internet Protocol - Ping from scratch](https://www.youtube.com/watch?v=3mv7E5kJTA4&list=PLdNUbYq5poiXDcqmOAW4I-U30i9rxUIe7)
- [Network Programming Tutorial](https://www.youtube.com/watch?v=SO-UF8Ggw6k)

### Man Pages
```bash
man 7 raw      # 原始 socket 編程
man 7 icmp     # ICMP 協議
man sendto     # 發送封包
man recvfrom   # 接收封包
```

##  學習重點

### 網路程式設計
- 原始 socket (`SOCK_RAW`) 的使用
- 非阻塞 I/O 和超時處理
- `getaddrinfo()` 主機名解析
- 網路位元組序轉換

### 協議實作
- ICMP 協議封包構建
- 校驗和計算演算法
- 時間戳嵌入和提取
- TTL 解析

### 系統程式設計
- Signal 處理 (`SIGINT`)
- 權限管理（capabilities）
- 命令列參數解析 (`getopt`)
- 模組化程式設計

---

**注意**: 使用原始 socket 需要特殊權限（root 或 `CAP_NET_RAW`）
