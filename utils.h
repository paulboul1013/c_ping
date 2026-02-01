#pragma once
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#pragma GCC diagnostic ignored "-Wformat-truncation="
#pragma GCC diagnostic push

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

typedef unsigned char int8;
typedef unsigned short int int16;
typedef unsigned int int32;
typedef unsigned long long int64;

// 前向聲明（結構在 ping.h）
struct s_timestamp;

/* C */
void copy(int8*,int8*,int16);

/* D */
double difftime_ms(struct s_timestamp*, struct s_timestamp*);

/* G */
void getnow(struct s_timestamp*);

/* N */
int16 nstoh(int16);

/* P */
void printhex(int8*,int16,int8);

/* T */
int8 *todotted(in_addr_t);

/* Z */
void zero(int8*,int16);