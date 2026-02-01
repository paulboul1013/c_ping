#include "utils.h"
#include <sys/time.h>

/* ========== 時間相關函數 ========== */

/**
 * 獲取當前時間（微秒精度）
 */
uint64_t get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* ========== 網絡相關函數 ========== */

/**
 * 計算校驗和（RFC 1071）
 */
uint16_t compute_checksum(const void *buf, size_t len) {
    const uint16_t *data = buf;
    uint32_t sum = 0;
    
    /* 累加所有 16 位元字 */
    while (len > 1) {
        sum += *data++;
        len -= 2;
    }
    
    /* 處理奇數字節 */
    if (len == 1) {
        sum += *(uint8_t *)data;
    }
    
    /* 處理進位 */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return (uint16_t)~sum;
}
