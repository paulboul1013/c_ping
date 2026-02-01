#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

/* ========== 時間相關函數 ========== */

/**
 * 獲取當前時間（微秒精度）
 */
uint64_t get_time_us(void);

/* ========== 網絡相關函數 ========== */

/**
 * 計算校驗和（RFC 1071）
 */
uint16_t compute_checksum(const void *buf, size_t len);

#endif /* UTILS_H */
