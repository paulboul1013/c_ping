#define _POSIX_C_SOURCE 199309L
#include "utils.h"
#include "ping.h"

void getnow(struct s_timestamp *ts) {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    ts->tv_sec = tp.tv_sec;
    ts->tv_usec = tp.tv_nsec / 1000;
}

double difftime_ms(struct s_timestamp *start, struct s_timestamp *end) {
    double sec_diff = (double)(end->tv_sec - start->tv_sec);
    double usec_diff = (double)(end->tv_usec - start->tv_usec);
    return sec_diff * 1000.0 + usec_diff / 1000.0;
}

void copy(int8 *dst, int8 *src, int16 size) {
    int8 *d, *s;
    int16 n;

    for (n=size, d=dst, s=src; n; n--)
        *d++ = *s++;

    return;
}

int16 nstoh(int16 srcport) {
    int16 dstport;  
    int8 a, b;

    a = ((srcport & 0xff00) >> 8);
    b = (srcport & 0xff);
    dstport = 0;
    dstport = (b << 8) + a;

    return dstport;
}

void zero(int8 *str, int16 size) {
    int8 *p;
    int16 n;

    for (n=0, p=str; n<size; n++, p++)
        *p = 0;

    return;
}

void printhex(int8 *str, int16 size, int8 delim) {
    int8 *p;
    int16 n;

    for (p=str, n=size; n; n--, p++) {
        printf("%.02x", *p);
        if (delim)
            printf("%c", delim);
        fflush(stdout);
    }
    printf("\n");

    return;
}


int8 *todotted(in_addr_t ip) {
    int8 a, b, c, d;
    static int8 buf[16];

    d = ((ip & 0xff000000) >> 24);
    c = ((ip & 0xff0000) >> 16);
    b = ((ip & 0xff00) >> 8);
    a = ((ip & 0xff));

    zero(buf, 16);
    snprintf((char *)buf, 15, "%d.%d.%d.%d",
        a, b, c, d);

    return buf;
}



#pragma GCC diagnostic pop
