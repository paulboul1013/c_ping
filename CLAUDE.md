# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 專案概述

這是一個從頭開始實現的 ICMP ping 工具，使用 C 語言編寫。專案實作了 IP 層和 ICMP 層的數據包構建、校驗和計算，以及原始 socket 操作。

## 建置與執行

```bash
# 建置專案（會先清理舊檔案）
make

# 或僅清理
make clean

# 執行 ping（需要 root 權限使用原始 socket）
sudo ./ping <target_ip>
# 例如：sudo ./ping 127.0.0.1

# 停止 ping：按 Ctrl+C
```

編譯器標誌：`-O2 -Wall -std=c2x`

**注意**：程式需要 root 權限或 `CAP_NET_RAW` capability 來創建原始 socket。

## 架構設計

### 雙層抽象模型

專案對網路協議採用雙層抽象：

1. **原始協議結構 (raw structures)**：`s_rawip` 和 `s_rawicmp`
   - 嚴格符合網路協議規範（RFC 792, RFC 791）
   - 使用 `packed` attribute 防止編譯器填充
   - 用於實際網路傳輸的二進制格式

2. **應用層抽象結構**：`s_ip` 和 `s_icmp`
   - 提供更高層次的抽象，便於程式邏輯處理
   - 使用枚舉類型 `type` 提高可讀性（echo/echoreply vs 0/8）
   - 包含指標和大小資訊，方便記憶體管理

### 模組分離

- **ping.c/ping.h**：核心 ICMP/IP 協議實作
  - 數據包創建（`mkicmp`, `mkip`）
  - 協議轉換（`evalicmp`, `evalip`）：將應用層結構轉換為原始網路格式
  - 校驗和計算（`checksum`）：RFC 1071 實作
  - 位元組序轉換（`endian16`）
  - Socket 操作（`initsocket`, `sendip`, `recvicmp`）

- **utils.c/utils.h**：通用工具函數
  - 記憶體操作（`copy`, `zero`）
  - 網路位元組序轉換（`nstoh`）
  - 格式化輸出（`printhex`, `todotted`）
  - 時間戳操作（`getnow`, `difftime_ms`）：用於 RTT 計算

### 關鍵設計模式

**泛型顯示巨集（Generic show macro）**：
```c
#define show(x) _Generic((x), \
    ip*:showip(# x,(ip*)x), \
    icmp*:showicmp(# x,(icmp*)x) \
)
```
使用 C11 `_Generic` 實現類型感知的除錯輸出。

**評估模式（Eval pattern）**：
- `mkXXX()` 函數創建應用層抽象結構
- `evalXXX()` 函數將抽象結構轉換為可傳輸的原始格式
- 這種分離使得協議邏輯清晰，易於測試和修改

## 重要實作細節

### ICMP Echo Request/Reply
- 實作完整的 RFC 792 ICMP echo 協議
- Identifier：使用 process ID（`getpid()`）
- Sequence：遞增序列號，用於追蹤每個封包
- Payload：56 字節（包含 16 字節時間戳 + 40 字節填充）

### 校驗和計算
- 實作 RFC 1071 定義的網際網路校驗和演算法
- 使用延遲進位（deferred carries）和 1's complement
- 處理奇數長度封包（填充到偶數）
- ping.c:158-176

### 位元組序處理
- 網路位元組序（big-endian）vs 主機位元組序
- `endian16()` 用於 16 位元欄位轉換
- IP 位址使用 `inet_addr()` 處理（已處理位元組序）
- ICMP identifier/sequence 需要位元組序轉換
- ping.c:149-156

### 記憶體管理
- 動態分配需要 `assert()` 檢查
- `evalXXX()` 函數回傳的指標需要呼叫者釋放
- 封包結構使用可變長度陣列（flexible array members）

### RTT 計算
- 使用 `CLOCK_MONOTONIC` 避免系統時間調整影響
- 時間戳嵌入 ICMP payload 中傳輸
- 精度：毫秒（ms）

### Socket 配置
- 類型：`SOCK_RAW` + `IPPROTO_ICMP`
- 選項：`IP_HDRINCL`（自建 IP 頭）
- 接收超時：2 秒（`SO_RCVTIMEO`）

### 類型系統注意事項
- `int32`：無符號 32 位元整數
- `sint32`：有符號 32 位元整數（用於 socket fd 和可能為負的值）
- Socket 相關函數必須使用 `sint32` 以正確處理錯誤返回值 (-1)

## 參考資料

專案參考以下規範和資源（見 README.md）：
- RFC 792：ICMP 協議
- RFC 1071：校驗和計算
- RFC 791：IPv4
- `man 7 raw`：原始 socket 程式設計
- YouTube 教學影片系列

## 開發狀態

目前實作進度：
- ✅ ICMP/IP 數據包結構定義（包含 identifier 和 sequence）
- ✅ 校驗和計算（RFC 1071）
- ✅ 封包序列化（eval 函數）
- ✅ 除錯輸出（show 巨集）
- ✅ Socket 創建和配置（`initsocket()`）
- ✅ Socket 傳送功能（`sendip()`）
- ✅ 接收 ICMP echo reply（`recvicmp()`）
- ✅ RTT 計算（使用 CLOCK_MONOTONIC）
- ✅ 命令列介面（基本功能）
- ✅ 統計信息（transmitted/received/loss/min/avg/max RTT）
- ✅ Signal 處理（Ctrl+C 優雅退出）

### 功能完整性

核心功能已完成，工具可以：
- 發送 ICMP echo request 封包
- 接收 echo reply 並驗證 identifier/sequence
- 計算並顯示 RTT
- 顯示封包統計（類似標準 ping）
- 支援 Ctrl+C 中斷並顯示統計

### 已知限制

- 只接受 IP 地址作為參數（不支援主機名解析）
- 沒有命令列選項（如 `-c count`, `-i interval`）
- 固定 1 秒發送間隔
- 不支援 IPv6

### WSL2 環境已知問題

在 WSL2 環境中，使用 `IP_HDRINCL` 選項和自定義 IP 頭髮送封包到 **loopback 接口 (127.0.0.1)** 時可能會遇到 `sendto: Operation not permitted` 錯誤。

**原因：**
- WSL2 使用虛擬化網絡棧，對原始 socket 的某些操作有限制
- Loopback 接口在 WSL2 中有特殊處理，可能拒絕帶有自定義 IP 頭的封包

**解決方案：**

1. **測試非 loopback 地址**（推薦）：
   ```bash
   sudo ./ping 8.8.8.8    # Google DNS
   sudo ./ping 1.1.1.1    # Cloudflare DNS
   ```

2. **在原生 Linux 環境中運行**：
   - 在真實 Linux 系統或虛擬機中測試
   - 原生 Linux 通常不會有這個問題

3. **檢查內核參數**：
   ```bash
   # 某些情況下可能需要啟用
   sudo sysctl -w net.ipv4.ping_group_range="0 2147483647"
   ```

4. **替代實現（如果只需要 ping 功能）**：
   - 可以修改為使用 `SOCK_DGRAM` + `IPPROTO_ICMP`（ping socket）
   - 這種方式不需要構建 IP 頭，但失去了學習 IP 層構建的目的

**驗證環境：**
```bash
# 檢查是否在 WSL2 中
uname -r | grep -i wsl

# 測試標準 ping 是否工作
ping -c 1 127.0.0.1

# 如果標準 ping 工作但我們的程式不工作，很可能是 WSL2 限制
```
