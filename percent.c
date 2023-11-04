/******************************************************************************
  @file    percent.c
  @brief   percent.

  DESCRIPTION
  QDloader Tool for USB and PCIE of Quectel wireless cellular modules.

  INITIALIZATION AND SEQUENCING REQUIREMENTS
  None.

  ---------------------------------------------------------------------------
  Copyright (c) 2016 - 2021 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
  ---------------------------------------------------------------------------
******************************************************************************/
#include "std_c.h"

static long long transfer_bytes;
static long long all_bytes_to_transfer;
static int last_percent = -1;

#ifdef EC200T_SHINCO
static int ipcfd = -1;
static void update_log(int percent) {
	char buf[128];

	//dprintf("percent=%d\n", percent);

	if (ipcfd == -1)
		ipcfd = open("/sdcard/updatelog", O_CREAT | O_TRUNC | O_WRONLY | O_NONBLOCK, 0444);
	
	if (ipcfd == -1)
		return;
	snprintf(buf, sizeof(buf), "[result]\r\nresult = %d\r\n[process]\r\npercent = %d\r\n",
            percent == 100 ? 1 : 0, percent);
        lseek(ipcfd, 0, SEEK_SET);
	if (write(ipcfd, buf, strlen(buf)) == -1) {};
	fsync(ipcfd);
}

static void update_result(int result) {
#if 0
	if (ipcfd == -1)
		return;
	snprintf(buf, sizeof(buf), "result = %d\r\n", result);
	if (write(ipcfd, buf, strlen(buf)) == -1) {};
	fsync(ipcfd);
        close(ipcfd);
        ipcfd = -1;
#endif
}
#endif

void set_transfer_allbytes(long long bytes)
{
    transfer_bytes = 0;
    all_bytes_to_transfer = bytes;
}

void update_transfer_bytes(long long bytes_cur) {
    int percent = 0;

    transfer_bytes += bytes_cur;
    if (bytes_cur == 0 || all_bytes_to_transfer == 0)
        percent = 0;
    else if (transfer_bytes >= all_bytes_to_transfer)
        percent = 100;
    else
        percent = (transfer_bytes * 100) / all_bytes_to_transfer;

    if (percent != last_percent) {
        last_percent = percent;
#ifdef EC200T_SHINCO
        if (!access(EC200T_SHINCO, W_OK)) update_log(percent);
#endif
    }
}

void update_transfer_result(int is_succ) {
#ifdef EC200T_SHINCO
        if (!access(EC200T_SHINCO, W_OK)) update_result(!!is_succ);
#endif
}
