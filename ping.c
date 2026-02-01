#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>

#include "ping.h"
#include "utils.h"

/* ========== 全局變量 ========== */

volatile sig_atomic_t keep_running = 1;

/* ========== Signal 處理 ========== */

void sigint_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

/* ========== 統計函數 ========== */

/**
 * 初始化統計
 */
void init_stats(ping_stats_t *stats) {
    stats->transmitted = 0;
    stats->received = 0;
    stats->min_rtt = 999999.0;
    stats->max_rtt = 0.0;
    stats->sum_rtt = 0.0;
}

/**
 * 更新統計
 */
void update_stats(ping_stats_t *stats, double rtt_ms) {
    stats->received++;
    stats->sum_rtt += rtt_ms;
    if (rtt_ms < stats->min_rtt) stats->min_rtt = rtt_ms;
    if (rtt_ms > stats->max_rtt) stats->max_rtt = rtt_ms;
}

/**
 * 顯示統計
 */
void print_stats(const ping_stats_t *stats, const char *host) {
    double loss = 0.0;
    
    printf("\n--- %s ping statistics ---\n", host);
    printf("%d packets transmitted, %d received, ", 
           stats->transmitted, stats->received);
    
    if (stats->transmitted > 0) {
        loss = (double)(stats->transmitted - stats->received) * 100.0 / 
               stats->transmitted;
    }
    printf("%.1f%% packet loss\n", loss);
    
    if (stats->received > 0) {
        double avg = stats->sum_rtt / stats->received;
        printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n",
               stats->min_rtt, avg, stats->max_rtt);
    }
}

/* ========== ICMP 封包函數 ========== */

/**
 * 創建 ICMP echo request 封包
 */
void create_icmp_packet(icmp_packet_t *packet, uint16_t id, uint16_t seq) {
    memset(packet, 0, sizeof(*packet));
    
    packet->header.type = ICMP_ECHO;
    packet->header.code = 0;
    packet->header.identifier = htons(id);
    packet->header.sequence = htons(seq);
    
    /* 在 data 中填入時間戳 */
    uint64_t timestamp = get_time_us();
    memcpy(packet->data, &timestamp, sizeof(timestamp));
    
    /* 計算校驗和 */
    packet->header.checksum = 0;
    packet->header.checksum = compute_checksum(packet, sizeof(*packet));
}

/* ========== 其他工具函數 ========== */

/**
 * 設置 socket 為非阻塞模式
 */
int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

/**
 * 顯示使用說明
 */
void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [-n count] <hostname or IP>\n", prog_name);
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -n count    發送封包的次數（預設: 4）\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s google.com           # 發送 4 次\n", prog_name);
    fprintf(stderr, "  %s -n 10 8.8.8.8        # 發送 10 次\n", prog_name);
    fprintf(stderr, "  %s -n 0 1.1.1.1         # 無限發送（Ctrl+C 停止）\n", prog_name);
}

/* ========== 主函數 ========== */

/**
 * 主函數
 */
int main(int argc, char *argv[]) {
    const char *hostname = NULL;
    int count = 4;  /* 預設發送 4 次，像 Windows ping */
    struct addrinfo hints, *addrinfo_list, *ai;
    int sockfd = -1;
    struct sockaddr_storage dest_addr;
    socklen_t dest_addr_len;
    char addr_str[INET6_ADDRSTRLEN];
    uint16_t id = (uint16_t)getpid();
    uint16_t seq = 0;
    ping_stats_t stats;
    int error;
    int opt;

    /* 解析命令列參數 */
    while ((opt = getopt(argc, argv, "n:h")) != -1) {
        switch (opt) {
            case 'n':
                count = atoi(optarg);
                if (count < 0) {
                    fprintf(stderr, "錯誤: count 必須 >= 0\n");
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    /* 獲取主機名（最後一個非選項參數） */
    if (optind >= argc) {
        fprintf(stderr, "錯誤: 缺少主機名或 IP 地址\n\n");
        print_usage(argv[0]);
        return 1;
    }
    hostname = argv[optind];
    
    /* 設置 signal handler */
    signal(SIGINT, sigint_handler);
    
    /* 解析主機名 */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;      /* IPv4 */
    hints.ai_socktype = SOCK_RAW;
    hints.ai_protocol = IPPROTO_ICMP;
    
    error = getaddrinfo(hostname, NULL, &hints, &addrinfo_list);
    if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return 1;
    }
    
    /* 創建 socket */
    for (ai = addrinfo_list; ai != NULL; ai = ai->ai_next) {
        sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sockfd >= 0) {
            memcpy(&dest_addr, ai->ai_addr, ai->ai_addrlen);
            dest_addr_len = ai->ai_addrlen;
            break;
        }
    }
    
    if (sockfd < 0) {
        perror("socket");
        fprintf(stderr, "Error: Need root privileges or CAP_NET_RAW capability\n");
        fprintf(stderr, "Try: sudo %s %s\n", argv[0], hostname);
        freeaddrinfo(addrinfo_list);
        return 1;
    }
    
    /* 設置非阻塞模式 */
    if (set_nonblocking(sockfd) < 0) {
        perror("set_nonblocking");
        close(sockfd);
        freeaddrinfo(addrinfo_list);
        return 1;
    }
    
    /* 轉換 IP 地址為字串 */
    inet_ntop(dest_addr.ss_family,
              &((struct sockaddr_in *)&dest_addr)->sin_addr,
              addr_str, sizeof(addr_str));
    
    freeaddrinfo(addrinfo_list);
    
    /* Drop privileges（如果使用 setuid） */
    if (getuid() != geteuid()) {
        if (setuid(getuid()) != 0) {
            perror("setuid");
        }
    }
    
    printf("PING %s (%s) %zu data bytes\n",
           hostname, addr_str, PACKET_SIZE - sizeof(icmp_header_t));

    init_stats(&stats);

    /* 主循環 - count=0 表示無限循環 */
    while (keep_running && (count == 0 || seq < count)) {
        icmp_packet_t request;
        uint64_t send_time, recv_time;
        
        /* 創建並發送 ICMP echo request */
        create_icmp_packet(&request, id, seq);
        
        send_time = get_time_us();
        
        ssize_t sent = sendto(sockfd, &request, sizeof(request), 0,
                             (struct sockaddr *)&dest_addr, dest_addr_len);
        if (sent < 0) {
            perror("sendto");
            break;
        }
        stats.transmitted++;
        
        /* 接收 reply（帶超時） */
        int received = 0;
        while (!received) {
            char buf[1024];
            ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL);
            
            recv_time = get_time_us();
            uint64_t elapsed = recv_time - send_time;
            
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    /* No data yet */
                    if (elapsed > REQUEST_TIMEOUT_US) {
                        printf("Request timeout for icmp_seq %d\n", seq);
                        break;
                    }
                    usleep(1000); /* Wait 1ms before trying again */
                    continue;
                } else {
                    perror("recvfrom");
                    break;
                }
            }
            
            /* 解析 IP 頭 */
            struct ip *ip_hdr = (struct ip *)buf;
            size_t ip_hdr_len = ip_hdr->ip_hl * 4;
            
            /* 解析 ICMP 頭 */
            icmp_packet_t *reply = (icmp_packet_t *)(buf + ip_hdr_len);
            
            /* 驗證是 echo reply */
            if (reply->header.type != ICMP_ECHOREPLY) {
                continue;
            }
            
            /* 驗證 ID 和 sequence */
            if (ntohs(reply->header.identifier) != id ||
                ntohs(reply->header.sequence) != seq) {
                continue;
            }
            
            /* 計算 RTT */
            uint64_t request_time;
            memcpy(&request_time, reply->data, sizeof(request_time));
            double rtt_ms = (recv_time - request_time) / 1000.0;
            
            printf("%zd bytes from %s: icmp_seq=%d ttl=%d time=%.3f ms\n",
                   n - ip_hdr_len, addr_str, seq, ip_hdr->ip_ttl, rtt_ms);
            
            update_stats(&stats, rtt_ms);
            received = 1;
        }
        
        seq++;
        
        /* 等待間隔 */
        uint64_t wait_time = REQUEST_INTERVAL_US - (get_time_us() - send_time);
        if (wait_time > 0 && wait_time < REQUEST_INTERVAL_US) {
            usleep(wait_time);
        }
    }
    
    /* 顯示統計 */
    print_stats(&stats, hostname);
    
    close(sockfd);
    return 0;
}
