/******************************************************************************
  @file    std_c.h
  @brief   define.

  DESCRIPTION
  QDloader Tool for USB and PCIE of Quectel wireless cellular modules.

  INITIALIZATION AND SEQUENCING REQUIREMENTS
  None.

  ---------------------------------------------------------------------------
  Copyright (c) 2016 - 2021 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
  ---------------------------------------------------------------------------
******************************************************************************/
#ifndef _STD_C_INCLUDE__
#define _STD_C_INCLUDE__
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <endian.h>
#include <pthread.h>
#include <sys/types.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#define min(x,y) (((x) < (y)) ? (x) : (y))

#if __BYTE_ORDER == __LITTLE_ENDIAN
#ifndef le32toh
static __inline uint16_t __bswap16(uint16_t __x)
{
        return __x<<8 | __x>>8;
}

#define le16toh(x) (uint16_t)(x)
#define le32toh(x) (uint32_t)(x)
#define be16toh(x) __bswap16(x)
#endif
#endif

extern void set_transfer_allbytes(long long bytes);
extern void update_transfer_bytes(long long bytes_cur);
extern void update_transfer_result(int is_succ);
extern int strStartsWith(const char *line, const char *prefix);
extern int verbose;
extern const char *get_time();
extern pthread_mutex_t log_mutex;
extern FILE *log_fp;
extern char log_buf[];
#define dprintf(fmt, args...) do { \
    int log_size = 0; \
    pthread_mutex_lock(&log_mutex); \
    log_size = snprintf(log_buf, 1024, "%s " fmt, get_time(), ##args); \
    if (log_fp) fwrite(log_buf, log_size, 1, log_fp); \
    if (log_fp != stdout) fwrite(log_buf, log_size, 1, stdout); \
    pthread_mutex_unlock(&log_mutex); \
} while(0);

//#define EC200T_SHINCO "/data/update/meig_linux_update_for_arm/updatelog"
#endif
