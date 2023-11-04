/******************************************************************************
  @file    aboot.c
  @brief   aboot.

  DESCRIPTION
  QDloader Tool for USB and PCIE of Quectel wireless cellular modules.

  INITIALIZATION AND SEQUENCING REQUIREMENTS
  None.

  ---------------------------------------------------------------------------
  Copyright (c) 2016 - 2021 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
  Quectel Wireless Solution Proprietary and Confidential.
  ---------------------------------------------------------------------------
******************************************************************************/
#include "ethos.h"
#include "../usbfs/usbfs.h"
#include <sys/time.h>

#define ABOOT_PREAMBLE_SIZE     4       /* preamble char numbers */
#define ABOOT_PREAMBLE_UUUU     "UUUU"  /* aboot standard */
#define ABOOT_PREAMBLE_UABT     "UABT"  /* aboot tiny */

typedef enum {
  ABOOT_DEVICE_TYPE_NONE,
  ABOOT_DEVICE_TYPE_SMUX,
  ABOOT_DEVICE_TYPE_ETHOS,
} aboot_device_type_t;


//static const char cmd_getvar[] = "getvar:";
static const char cmd_download[] = "download:";
static const char cmd_call[] = "call";
static const char cmd_nop[] = "nop";
static const char cmd_reboot[] = "reboot";
static const char cmd_complete[] = "complete";
static const char cmd_disconnect[] = "disconnect";
static const char crane_version_bootrom[] = "2019.01.15";
//static int reboot_after_completed = 0;
static int is_crane_bootrom;
extern int is_R03;

#define ABOOT_COMMAND_SZ        128
#define ABOOT_RESPONSE_SZ       128
#define SPARSE_HEADER_MAGIC 0xed26ff3a

static pthread_mutex_t cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cmd_cond = PTHREAD_COND_INITIALIZER;

static void setTimespecRelative(struct timespec *p_ts, long long msec)
{
    struct timeval tv;

    gettimeofday(&tv, (struct timezone *) NULL);

    p_ts->tv_sec = tv.tv_sec + (msec / 1000);
    p_ts->tv_nsec = (tv.tv_usec + (msec % 1000) * 1000L ) * 1000L;
    if ((unsigned long)p_ts->tv_nsec >= 1000000000UL) {
        p_ts->tv_sec += 1;
        p_ts->tv_nsec -= 1000000000UL;
    }
}

int pthread_cond_timeout_np(pthread_cond_t *cond, pthread_mutex_t * mutex, unsigned msecs) {
  unsigned i;
  unsigned t = msecs/4;
  int ret = 0;

  if (!msecs)
    msecs = 15000;

  if (t == 0)
   t = 1;

    for (i = 0; i < msecs; i += t) {
      struct timespec ts;
      setTimespecRelative(&ts, t);
      //very old uclibc do not support pthread_condattr_setclock(CLOCK_MONOTONIC)
      ret = pthread_cond_timedwait(cond, mutex, &ts); //to advoid system time change
      if (ret != ETIMEDOUT) {
        if(ret) dprintf("ret=%d, msecs=%u, t=%u", ret, msecs, t);
        break;
      }
    }

  return ret;
}


int serial_port_write(aboot_t *aboot, uint8_t *buf, size_t len) {
    if (sendSync(aboot->usb.fd, buf, len, 0) == len)
        return 0;
    return -1;
}

static void preamble_callback(struct aboot *aboot, const uint8_t *data, size_t size)
{
    size_t i;

    if (aboot->aboot_device_type != ABOOT_DEVICE_TYPE_NONE)
        return;

    for (i = 0; (i + 4) < size; i++) {
        if (data[i] != 0x7e)
            continue;

        if (data[i+1] == 0x7e
            && !strncmp((char *)data + 2, ABOOT_PREAMBLE_UUUU, ABOOT_PREAMBLE_SIZE)) {
          aboot->aboot_device_type = ABOOT_DEVICE_TYPE_ETHOS;
          break;
        }
        else if (!strncmp((char *)data + 1, ABOOT_PREAMBLE_UABT, ABOOT_PREAMBLE_SIZE)) {
          aboot->aboot_device_type = ABOOT_DEVICE_TYPE_SMUX;
          break;
        }
    }
}

static int preamble_start(aboot_t *aboot)
{
  uint8_t buffer[ABOOT_PREAMBLE_SIZE + 1];
  uint32_t t = 0;

  memcpy(buffer, ABOOT_PREAMBLE_UUUU, ABOOT_PREAMBLE_SIZE);
  buffer[ABOOT_PREAMBLE_SIZE] = 0x7e;

  aboot->process_incoming_data = preamble_callback;
  aboot->aboot_device_type = ABOOT_DEVICE_TYPE_NONE;

  for (t = 0; t < 2000; t += 100) {
    //if (t == 0 || t == 1000)
    {
        if (serial_port_write(aboot, buffer, ABOOT_PREAMBLE_SIZE + 1))
            break;
    }

    msleep(100);
    if (aboot->aboot_device_type != ABOOT_DEVICE_TYPE_NONE)
        break;
  }

  dprintf("aboot_device_type is %d\n", aboot->aboot_device_type);

  return (aboot->aboot_device_type == ABOOT_DEVICE_TYPE_NONE) ? -1 : 0;
}

static void *  serial_port_read(void *param) {
  aboot_t *aboot = (aboot_t *)param;

  while(1) {
    int ret = recvSync(aboot->usb.fd, aboot->usb.rxbuf, sizeof(aboot->usb.rxbuf), -1);

    if (ret <= 0)
        break;

    if (aboot->process_incoming_data)
        aboot->process_incoming_data(aboot, aboot->usb.rxbuf, ret);
  }

  return NULL;
}

static int download_read_line(FILE *fp, char *line)
{
  while(1) {
    if(!fgets(line, ABOOT_COMMAND_SZ, fp)) {
      return -1;
    }

    if(line[0] == '\n') {
      continue;
    }

    line[strlen(line) - 1] = '\0'; /* replace '\n' to '\0' */
    return 1;
  }

  return 0;
}

static int download_read_cmd_response(FILE *fp, char *cmd_line, char *response)
{
  int rc = download_read_line(fp, cmd_line);
  if(rc < 0) {
    return -1;
  } else if(rc == 0) {
    return 0;
  }

  rc = download_read_line(fp, response);
  if(rc < 0) {
    return -1;
  } else if(rc == 0) {
    return 0;
  }

  return 1;
}

static int download_handle_download(uint32_t size, char *expect, const char *response)
{
  if(memcmp(expect, response, 4)) {
    return -1;
  }
  expect += 4;
  response += 4;

  uint32_t expect_size = (uint32_t)strtoul(expect, NULL, 16);
  uint32_t response_size = (uint32_t)strtoul(response, NULL, 16);
  if(size != response_size || size != expect_size) {
    return -1;
  }

  return 0;
}

static int download_do_download_sparse_file(aboot_t *aboot, uint32_t size)
{
  int ret;
  FILE *fp = aboot->fw_fp;

  ret = ethos_send_data_request(aboot, size);
  if (ret) goto out;

  while (size) {
    uint32_t len = size > sizeof(aboot->usb.txbuf) ? sizeof(aboot->usb.txbuf) : size;

    if(len != fread(aboot->usb.txbuf, 1, len, fp)) {
      goto out;
    }

    size -= len;
    ret = serial_port_write(aboot, aboot->usb.txbuf, len);
    if (ret) goto out;
  }

  ret = ethos_send_data_request(aboot, 0);

out:
  return ret;
}

static int download_do_download_data(aboot_t *aboot, uint32_t size)
{
  int ret = -1;
  FILE *fp = aboot->fw_fp;

  if (aboot->aboot_device_type == ABOOT_DEVICE_TYPE_ETHOS) {
    uint32_t is_sparse_file;

    if(4 != fread(&is_sparse_file, 1, 4, aboot->fw_fp)) {
      return -1;
    }
    fseek(fp, -4, SEEK_CUR);

    if (le32toh(is_sparse_file) == SPARSE_HEADER_MAGIC) {
      return download_do_download_sparse_file(aboot, size);
    }
  }

  while (size) {
    uint32_t len = size > aboot->mtu ? aboot->mtu : size;

    if(len != fread(aboot->txbuf, 1, len, fp)) {
      goto out;
    }

    size -= len;

    if (aboot->aboot_device_type == ABOOT_DEVICE_TYPE_ETHOS) {
      ret = ethos_write_aboot_data(aboot, aboot->txbuf, len, size == 0);
      if (ret) goto out;

      pthread_mutex_lock(&cmd_mutex);
      ret = pthread_cond_timeout_np(&cmd_cond, &cmd_mutex, 1000);
      pthread_mutex_unlock(&cmd_mutex);
      if (ret) goto out;
    }
    else if (aboot->aboot_device_type == ABOOT_DEVICE_TYPE_SMUX)
      ret = smux_write_aboot_data(aboot, aboot->txbuf, len);
    else
        ret = -1;

    if (ret) goto out;
  }

  return 0;

out:
  return ret;
}

void aboot_transport_rx_cmd(aboot_t *aboot, const uint8_t *cmd, size_t size) {
  pthread_mutex_lock(&cmd_mutex);

  if (cmd && size) {
    char rsp[128];

    memcpy(rsp, cmd, size);
    rsp[size] = 0;
    dprintf("[cmd] < %s\n", rsp);

    if (!memcmp(rsp, "OKAY", 4) && !strcmp(rsp + 4, crane_version_bootrom)) {
      is_crane_bootrom = 1;
    }

    if (!memcmp(rsp, "OKAY", 4) || !memcmp(rsp, "DATA", 4) || !memcmp(rsp, "FAIL", 4)) {
      strcpy(aboot->cmd_response, rsp);
    }
  }

  pthread_cond_signal(&cmd_cond);
  pthread_mutex_unlock(&cmd_mutex);
}

static int aboot_transport_send_cmd(aboot_t *aboot, const uint8_t *cmd, size_t size)
{
  int ret = -1;

  if (cmd) dprintf("[cmd] > %s\n", cmd);

  if (cmd) aboot->cmd_response[0] = 0;
  switch (aboot->aboot_device_type) {
  case ABOOT_DEVICE_TYPE_SMUX:
    ret = cmd ? smux_write_aboot_cmd(aboot, cmd, size) : 0;
    break;

  case ABOOT_DEVICE_TYPE_ETHOS:
    ret = ethos_write_aboot_cmd(aboot, cmd, size);
    break;

  default:
    break;
  }

  pthread_mutex_lock(&cmd_mutex);
  while (ret == 0 && aboot->cmd_response[0] == 0) {
    ret = pthread_cond_timeout_np(&cmd_cond, &cmd_mutex, 3000);
    pthread_mutex_unlock(&cmd_mutex);
    if ((ret == 0 && aboot->cmd_response[0] == 0)
      && aboot->aboot_device_type == ABOOT_DEVICE_TYPE_ETHOS) {
        ret = ethos_write_aboot_cmd(aboot, NULL, 0);
    }
    pthread_mutex_lock(&cmd_mutex);
  }
  pthread_mutex_unlock(&cmd_mutex);

  if (!memcmp(aboot->cmd_response, "protocol error", 14))
      return -1;

  return ret;
}

int aboot_download_process(aboot_t *aboot)
{
  char cmd_line[ABOOT_COMMAND_SZ];
  char response[ABOOT_RESPONSE_SZ];
  FILE *fp = aboot->fw_fp;
  char magic[9];
  uint32_t size;
  int num;
  long firmware_size = 0;
  int ret = -1;

  fseek(fp, 0, SEEK_END);
  firmware_size = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  if (is_R03 == 1)
      firmware_size -= 64;

  if (download_read_line(fp, cmd_line) <= 0) {
    return -1;
  }

  num = sscanf(cmd_line, "%08s%08x", magic, &size);

  if (num != 2 || size != firmware_size) {
      return  -1;
  }

  if (strcmp(magic, "!CRANE!")) {
    return  -1;
  }

  dprintf("Found a crane firmware with size %u\n", (uint32_t)size);

  while (download_read_cmd_response(fp, cmd_line, response) == 1) {

    if(!strcmp(cmd_line, cmd_call) && is_crane_bootrom &&
           aboot->aboot_device_type == ABOOT_DEVICE_TYPE_SMUX) {
          strcpy(cmd_line, cmd_nop);
    }

    ret = aboot_transport_send_cmd(aboot, (uint8_t *)cmd_line, strlen(cmd_line));
    if (ret) goto out;

    if (!strcmp(cmd_line, cmd_nop) && !strncmp(aboot->cmd_response, "FAIL", 4)) {
        /* If the original FW version is old,
	   means there is no firmware.bin in the FW zip package.
           we need skip this nop command.
        */
    }
    else if (strncmp(response, aboot->cmd_response, 4)) {
       dprintf("expect '%s', but get '%s'\n", response, aboot->cmd_response);
       return -1;
    }

    if (!strncmp(cmd_line, cmd_download, strlen(cmd_download))) {
        uint32_t size = (uint32_t)strtoul(cmd_line + strlen(cmd_download), NULL, 16);

        if (download_handle_download(size, response, aboot->cmd_response)) {
          break;
        }

        aboot->cmd_response[0] = 0;
        ret = download_do_download_data(aboot, size);
        if (ret) goto out;

        if (download_read_line(fp, response) <= 0) {
          return -1;
        }

        ret = aboot_transport_send_cmd(aboot, NULL, 0);
        if (ret) goto out;

        if (strncmp(response, aboot->cmd_response, 4)) {
            dprintf("expect '%s', but get '%s'\n", response, aboot->cmd_response);
            return -1;
        }
    }
  }

  strcpy(cmd_line, cmd_reboot);
  strcpy(response, "OKAY");
  ret = aboot_transport_send_cmd(aboot, (uint8_t *)cmd_line, strlen(cmd_line));
  if (ret) goto out;

  strcpy(cmd_line, cmd_complete);
  strcpy(response, "OKAY");
  ret = aboot_transport_send_cmd(aboot, (uint8_t *)cmd_line, strlen(cmd_line));
  if (ret) goto out;

  sleep(1);
  strcpy(cmd_line, cmd_disconnect);
  strcpy(response, "OKAY");
  ret = aboot_transport_send_cmd(aboot, (uint8_t *)cmd_line, strlen(cmd_line));
  if (ret != 0) ret = 0;
  if (ret) goto out;

out:
  return ret;
}

int aboot_main(int usbfd, FILE *fw_fp, const char *modem_name)
{
    int ret;
    pthread_t tid;
    aboot_t *aboot = (aboot_t *)malloc(sizeof(aboot_t));

    memset(aboot, 0x0, sizeof(aboot_t));

    aboot->usb.fd = usbfd;
    aboot->fw_fp = fw_fp;

    pthread_create(&tid, NULL, serial_port_read, (void *)aboot);

    ret = preamble_start(aboot);
    if (ret) goto out;

    if (aboot->aboot_device_type == ABOOT_DEVICE_TYPE_SMUX) {
      ret = smux_init(aboot);
    }
    else if (aboot->aboot_device_type == ABOOT_DEVICE_TYPE_ETHOS) {
      ret = ethos_init(aboot);
      if (ret) goto out;

      ret = udp_transport_init(aboot);
      if (ret) goto out;
    }

    ret = aboot_download_process(aboot);
    if (ret) goto out;

out:
    free(aboot);
    return ret;
}
