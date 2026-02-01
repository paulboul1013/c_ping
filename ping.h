#ifndef PING_H
#define PING_H

#include <stdint.h>
#include <signal.h>

/* ========== 常量定義 ========== */

#define PACKET_SIZE 64
#define REQUEST_TIMEOUT_US 1000000  /* 1 second in microseconds */
#define REQUEST_INTERVAL_US 1000000 /* 1 second between requests */

/* ========== ICMP 結構定義 ========== */

/**
 * ICMP 頭部結構
 */
typedef struct {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} icmp_header_t;

/**
 * ICMP 封包結構
 */
typedef struct {
    icmp_header_t header;
    uint8_t data[PACKET_SIZE - sizeof(icmp_header_t)];
} icmp_packet_t;

/* ========== 統計結構 ========== */

/**
 * Ping 統計信息
 */
typedef struct {
    int transmitted;    /* 已發送封包數 */
    int received;       /* 已接收封包數 */
    double min_rtt;     /* 最小 RTT (ms) */
    double max_rtt;     /* 最大 RTT (ms) */
    double sum_rtt;     /* RTT 總和 (ms) */
} ping_stats_t;

/* ========== 全局變量 ========== */

extern volatile sig_atomic_t keep_running;

/* ========== 統計函數 ========== */

/**
 * 初始化統計
 */
void init_stats(ping_stats_t *stats);

/**
 * 更新統計
 */
void update_stats(ping_stats_t *stats, double rtt_ms);

/**
 * 顯示統計
 */
void print_stats(const ping_stats_t *stats, const char *host);

/* ========== ICMP 封包函數 ========== */

/**
 * 創建 ICMP echo request 封包
 */
void create_icmp_packet(icmp_packet_t *packet, uint16_t id, uint16_t seq);

/* ========== Signal 處理 ========== */

/**
 * SIGINT 信號處理函數
 */
void sigint_handler(int signum);

/* ========== 其他工具函數 ========== */

/**
 * 設置 socket 為非阻塞模式
 */
int set_nonblocking(int sockfd);

/**
 * 顯示使用說明
 */
void print_usage(const char *prog_name);

#endif /* PING_H */
