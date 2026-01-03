#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include "utils.h"

//packed : tell compiler not to padding the structure
#define packed __attribute__((packed))

typedef unsigned char int8;
typedef unsigned short int16;
typedef unsigned int int32;
typedef unsigned long long int64;


//icmp type
enum e_type {
    unassigned,
    echo,
    echoreply
};

typedef enum e_type type;

// ICMP 協議原始格式（網絡層）：符合 RFC 792 標準的 ICMP 報文格式
// 用於實際在網絡上傳輸的數據包，必須與協議規範完全一致
// type: ICMP 類型（8=echo request, 0=echo reply）
// code: ICMP 代碼（通常為 0）
// checksum: 16位校驗和
// data[]: 可變長度的數據負載（payload）
struct s_rawicmp{
    int8 type;
    int8 code;
    int16 checksum;
    int8 data[];
}packed;

// ICMP 應用層抽象結構：用於程序內部處理和操作
// 提供更高層次的抽象，便於程序邏輯處理
// kind: 使用枚舉類型表示 ICMP 類型（echo/echoreply），比原始數字更易讀
// size: 數據負載的大小（字節數）
// data: 指向數據負載的指針
struct s_icmp{
    type kind;
    int16 size;
    int8 *data;
}packed;
typedef struct s_icmp icmp;



int16 checksum(int8*,int16);//calculate checksum of icmp packet
int16 endian16(int16);

//icmp
icmp *mkicmp(type,const int8*,int16);//create icmp packet
int8 *evalicmp(icmp*);//evaluate 's_icmp icmp' to raw icmp
void showicmp(icmp*);//show icmp packet imfomration

