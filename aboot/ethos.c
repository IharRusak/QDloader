/******************************************************************************
  @file    ethos.c
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
#include "ethos.h"

#define ETHOS_FRAME_MTU 2048
#define ETHOS_PREAMBLE_SIZE  4       /* preamble char numbers */
#define ETHOS_PREAMBLE_UUUU  "UUUU"  /* aboot standard */
#define ETHOS_MAC_ADDR_LEN   6

typedef enum {
  WAIT_FRAMESTART,
  IN_FRAME,
  IN_ESCAPE
} line_state_t;

typedef enum {
  STAGE_INIT,
  STAGE_INIT_DONE,
  STAGE_HELLO,
  STAGE_HELLO_DONE,
  STAGE_MAC,
  STAGE_MAC_DONE,
  STAGE_RUNNING,
  STAGE_FAILED,
  STAGE_SUCCEEDED
} running_stage_t;

typedef enum {
  ETHOS_FRAME_TYPE_DATA        = (0x00),
  ETHOS_FRAME_TYPE_TEXT        = (0x01),
  ETHOS_FRAME_TYPE_HELLO       = (0x02),
  ETHOS_FRAME_TYPE_HELLO_REPLY = (0x03),
  ETHOS_FRAME_TYPE_HEART_BEAT  = (0x04),
  ETHOS_FRAME_TYPE_UPLOAD_PROG = (0x05),
  ETHOS_ESC_CHAR               = (0x7D),
  ETHOS_FRAME_DELIMITER        = (0x7E)
} ethos_frame_type_t;

#define SMUX_PREAMBLE_SIZE  4       /* preamble char numbers */
#define SMUX_PREAMBLE_UABT  "UABT"  /* aboot tiny */

typedef enum {
  SMUX_FRAME_TYPE_STDIO       = (0x00),
  SMUX_FRAME_TYPE_HELLO       = (0x01),
  SMUX_FRAME_TYPE_HELLO_REPLY = (0x02),
  SMUX_FRAME_TYPE_ABOOT_CMD   = (0x03),
  SMUX_FRAME_TYPE_ABOOT_DATA  = (0x04),
  SMUX_FRAME_TYPE_HEART_BEAT  = (0x05),
  SMUX_ESC_CHAR               = (0x7D),
  SMUX_FRAME_DELIMITER        = (0x7E)
} smux_frame_type_t;

static uint8_t * _write_escaped(uint8_t *p, const uint8_t *buf, size_t n)
{
  while(n--) {
    uint8_t c = *buf++;
    switch(c) {
    case ETHOS_FRAME_DELIMITER:
      *p++ = ETHOS_ESC_CHAR;
      *p++ = (ETHOS_FRAME_DELIMITER ^ 0x20);
      break;
    case ETHOS_ESC_CHAR:
      *p++ = ETHOS_ESC_CHAR;
      *p++ = (ETHOS_ESC_CHAR ^ 0x20);
      break;
    default:
      *p++ = c;
      break;
    }
  }
  return p;
}

static int _send_frame(aboot_t *aboot, const uint8_t *data, size_t len, uint32_t frame_type)
{
  uint8_t *frame = aboot->usb.txbuf;
  uint8_t *p = frame;

  /* send frame delimiter */
  *p++ = ETHOS_FRAME_DELIMITER;

  /* set frame type */
  if(frame_type) {
    *p++ = ETHOS_ESC_CHAR;
    *p++ = (frame_type ^ 0x20);
  }

  /* send frame content */
  p = _write_escaped(p, data, len);

  /* end of frame */
  *p++ = ETHOS_FRAME_DELIMITER;

  len = p - frame;

  return serial_port_write(aboot, frame, len);
}

static void _reset_state(aboot_t *aboot)
{
  aboot->state = WAIT_FRAMESTART;
  aboot->frametype = 0;
  aboot->framesize = 0;
}

static void ethos_handle_char(aboot_t *aboot, char c)
{
  switch(aboot->frametype) {
  case ETHOS_FRAME_TYPE_DATA:
  case ETHOS_FRAME_TYPE_TEXT:
  case ETHOS_FRAME_TYPE_HELLO:
  case ETHOS_FRAME_TYPE_HELLO_REPLY:
  case ETHOS_FRAME_TYPE_HEART_BEAT:
  case ETHOS_FRAME_TYPE_UPLOAD_PROG:
    if(aboot->framesize < aboot->mtu) {
      aboot->rxbuf[aboot->framesize++] = c;
    } else {
      dprintf("lost frame\n");
      _reset_state(aboot);
    }
    break;
  default:
    break;
  }
}

static void ethos_end_of_frame(aboot_t *aboot)
{
  switch(aboot->frametype) {
  case ETHOS_FRAME_TYPE_DATA:
    if(aboot->stage == STAGE_INIT) {
      if(aboot->framesize == ETHOS_PREAMBLE_SIZE &&
         !strncmp((const char *)aboot->rxbuf, ETHOS_PREAMBLE_UUUU, ETHOS_PREAMBLE_SIZE)) {
        aboot->stage = STAGE_INIT_DONE;
        break;
      }
    }
    else if (aboot->stage == STAGE_RUNNING) {
      uip_decode(aboot, aboot->rxbuf, aboot->framesize, 0);
    }
    break;
  case ETHOS_FRAME_TYPE_TEXT:
    aboot->rxbuf[aboot->framesize] = '\0';
    dprintf("%s %s", "[log] <", aboot->rxbuf);
    break;
  case ETHOS_FRAME_TYPE_HELLO:
    break;
  case ETHOS_FRAME_TYPE_HELLO_REPLY:
    if(aboot->stage == STAGE_HELLO) {
      if(aboot->framesize == ETHOS_MAC_ADDR_LEN) {
        memcpy(aboot->ethos.remote_mac_addr, aboot->rxbuf, aboot->framesize);
        dprintf("%s remote_mac_addr\n", "[cmd] <");
        aboot->stage = STAGE_HELLO_DONE;
      }
    } else {
      dprintf("Unexpected HELLO REPLY frame in stage %d\n", aboot->stage);
    }
    break;
  case ETHOS_FRAME_TYPE_UPLOAD_PROG:
    break;
  case ETHOS_FRAME_TYPE_HEART_BEAT:
    break;
  default:
    break;
  }

  _reset_state(aboot);
}

static void ethos_process_incoming_data(struct aboot *ethos, const uint8_t *data, size_t len)
{
  size_t i;

  for(i = 0; i < len; i++) {
    uint8_t c = data[i];

    switch(ethos->state) {
    case WAIT_FRAMESTART:
      if(c == ETHOS_FRAME_DELIMITER) {
        _reset_state(ethos);
        ethos->state = IN_FRAME;
      }
      break;
    case IN_FRAME:
      if(c == ETHOS_ESC_CHAR) {
        ethos->state = IN_ESCAPE;
      } else if(c == ETHOS_FRAME_DELIMITER) {
        if(ethos->framesize) {
          ethos_end_of_frame(ethos);
        }
      } else {
        ethos_handle_char(ethos, c);
      }
      break;
    case IN_ESCAPE:
      switch(c) {
      case (ETHOS_FRAME_DELIMITER ^ 0x20):
        ethos_handle_char(ethos, ETHOS_FRAME_DELIMITER);
        break;
      case (ETHOS_ESC_CHAR ^ 0x20):
        ethos_handle_char(ethos, ETHOS_ESC_CHAR);
        break;
      case (ETHOS_FRAME_TYPE_TEXT ^ 0x20):
        ethos->frametype = ETHOS_FRAME_TYPE_TEXT;
        break;
      case (ETHOS_FRAME_TYPE_HELLO ^ 0x20):
        ethos->frametype = ETHOS_FRAME_TYPE_HELLO;
        break;
      case (ETHOS_FRAME_TYPE_HELLO_REPLY ^ 0x20):
        ethos->frametype = ETHOS_FRAME_TYPE_HELLO_REPLY;
        break;
      case (ETHOS_FRAME_TYPE_HEART_BEAT ^ 0x20):
        ethos->frametype = ETHOS_FRAME_TYPE_HEART_BEAT;
        break;
      case (ETHOS_FRAME_TYPE_UPLOAD_PROG ^ 0x20):
        ethos->frametype = ETHOS_FRAME_TYPE_UPLOAD_PROG;
        break;
      }
      ethos->state = IN_FRAME;
      break;
    }
  }
}

int ethos_send_data(aboot_t *aboot, const uint8_t *data, size_t size)
{
  return _send_frame(aboot, data, size, ETHOS_FRAME_TYPE_DATA);
}

int ethos_send_text(aboot_t *aboot, const uint8_t *data, size_t size)
{
  return _send_frame(aboot, data, size, ETHOS_FRAME_TYPE_TEXT);
}

int ethos_init(aboot_t *aboot)
{
  uint32_t t;
  uint8_t addr[] = {0x00, 0x72, 0xca, 0xac, 0x95, 0x2c};

  aboot->mtu = ETHOS_FRAME_MTU;
  aboot->stage = STAGE_INIT;
  aboot->process_incoming_data = ethos_process_incoming_data;
  _send_frame(aboot, (uint8_t *)ETHOS_PREAMBLE_UUUU, ETHOS_PREAMBLE_SIZE, ETHOS_FRAME_TYPE_DATA);

  for (t = 0; t < 2000 && aboot->stage == STAGE_INIT; t += 100) {
    msleep(100);
  }

  if (aboot->stage != STAGE_INIT_DONE)
    return -1;

  aboot->stage = STAGE_HELLO;
  memcpy(aboot->ethos.mac_addr, addr, ETHOS_MAC_ADDR_LEN);
  _send_frame(aboot, aboot->ethos.mac_addr, ETHOS_MAC_ADDR_LEN, ETHOS_FRAME_TYPE_HELLO);

  for (t = 0; t < 2000 && aboot->stage == STAGE_HELLO; t += 100) {
    msleep(100);
  }

  if (aboot->stage != STAGE_HELLO_DONE)
    return -1;

  aboot->stage = STAGE_RUNNING;
  return 0;
}

static void smux_handle_char(aboot_t *smux, char c)
{
  switch(smux->frametype) {
  case SMUX_FRAME_TYPE_STDIO:
  case SMUX_FRAME_TYPE_HELLO:
  case SMUX_FRAME_TYPE_HELLO_REPLY:
  case SMUX_FRAME_TYPE_ABOOT_CMD:
  case SMUX_FRAME_TYPE_ABOOT_DATA:
  case SMUX_FRAME_TYPE_HEART_BEAT:
    if(smux->framesize < smux->mtu) {
      smux->rxbuf[smux->framesize++] = c;
    } else {
      dprintf("lost frame\n");
      _reset_state(smux);
    }
    break;
  default:
    break;
  }
}

static void smux_end_of_frame(aboot_t *smux)
{
  switch(smux->frametype) {
  case SMUX_FRAME_TYPE_STDIO:
    if(smux->stage == STAGE_INIT) {
      if(smux->framesize == SMUX_PREAMBLE_SIZE &&
         !strncmp((const char *)smux->rxbuf, SMUX_PREAMBLE_UABT, SMUX_PREAMBLE_SIZE)) {
        smux->stage = STAGE_INIT_DONE;
        break;
      }
    }
    smux->rxbuf[smux->framesize] = '\0';
    dprintf("%s", smux->rxbuf);
    break;
  case SMUX_FRAME_TYPE_HELLO_REPLY:
    if(smux->stage == STAGE_HELLO) {
      if(smux->framesize == sizeof(smux->mtu) && smux->stage == STAGE_HELLO) {
        smux->mtu = (uint8_t)smux->rxbuf[0] << 8 | (uint8_t)smux->rxbuf[1];
        dprintf("mtu is %d\n", smux->mtu);
        smux->stage = STAGE_HELLO_DONE;
      }
    } else {
      dprintf("Unexpected HELLO REPLY frame in stage %d\n", smux->stage);
    }
    break;
  case SMUX_FRAME_TYPE_ABOOT_CMD:
    aboot_transport_rx_cmd(smux, smux->rxbuf, smux->framesize);
    break;
  case SMUX_FRAME_TYPE_HEART_BEAT:
    break;
  case SMUX_FRAME_TYPE_ABOOT_DATA:
    //if(smux->data_cb) {
    //  smux->data_cb(smux->rxbuf, smux->framesize);
    //}
    break;
  default:
    break;
  }

  _reset_state(smux);
}

static void smux_process_incoming_data(struct aboot *smux, const uint8_t *data, size_t len)
{
  size_t i;
  for(i = 0; i < len; i++) {
    uint8_t c = data[i];

    switch(smux->state) {
    case WAIT_FRAMESTART:
      if(c == SMUX_FRAME_DELIMITER) {
        _reset_state(smux);
        smux->state = IN_FRAME;
      }
      break;
    case IN_FRAME:
      if(c == SMUX_ESC_CHAR) {
        smux->state = IN_ESCAPE;
      } else if(c == SMUX_FRAME_DELIMITER) {
        if(smux->framesize) {
          smux_end_of_frame(smux);
        }
      } else {
        smux_handle_char(smux, c);
      }
      break;
    case IN_ESCAPE:
      switch(c) {
      case (SMUX_FRAME_DELIMITER ^ 0x20):
        smux_handle_char(smux, SMUX_FRAME_DELIMITER);
        break;
      case (SMUX_ESC_CHAR ^ 0x20):
        smux_handle_char(smux, SMUX_ESC_CHAR);
        break;
      case (SMUX_FRAME_TYPE_STDIO ^ 0x20):
        smux->frametype = SMUX_FRAME_TYPE_STDIO;
        break;
      case (SMUX_FRAME_TYPE_HELLO ^ 0x20):
        smux->frametype = SMUX_FRAME_TYPE_HELLO;
        break;
      case (SMUX_FRAME_TYPE_HELLO_REPLY ^ 0x20):
        smux->frametype = SMUX_FRAME_TYPE_HELLO_REPLY;
        break;
      case (SMUX_FRAME_TYPE_ABOOT_CMD ^ 0x20):
        smux->frametype = SMUX_FRAME_TYPE_ABOOT_CMD;
        break;
      case (SMUX_FRAME_TYPE_ABOOT_DATA ^ 0x20):
        smux->frametype = SMUX_FRAME_TYPE_ABOOT_DATA;
        break;
      case (SMUX_FRAME_TYPE_HEART_BEAT ^ 0x20):
        smux->frametype = SMUX_FRAME_TYPE_HEART_BEAT;
        break;
      }
      smux->state = IN_FRAME;
      break;
    }
  }
}

int smux_write_aboot_data(aboot_t *aboot, const uint8_t *data, size_t len)
{
  return _send_frame(aboot, data, len, SMUX_FRAME_TYPE_ABOOT_DATA);
}

int  smux_write_aboot_cmd(aboot_t *aboot, const uint8_t *data, size_t len)
{
  return _send_frame(aboot, data, len, SMUX_FRAME_TYPE_ABOOT_CMD);
}

int smux_init(aboot_t *aboot)
{
  uint32_t t;
  uint16_t mtu;

  aboot->mtu = 1024;
  aboot->stage = STAGE_INIT;
  aboot->process_incoming_data = smux_process_incoming_data;
  _send_frame(aboot, (uint8_t *)SMUX_PREAMBLE_UABT, SMUX_PREAMBLE_SIZE, SMUX_FRAME_TYPE_STDIO);

  for (t = 0; t < 2000 && aboot->stage == STAGE_INIT; t += 100) {
    msleep(100);
  }

  if (aboot->stage != STAGE_INIT_DONE)
    return -1;

  aboot->stage = STAGE_HELLO;
  mtu = le16toh((uint16_t)((uint16_t)aboot->mtu << 8 | (uint16_t)aboot->mtu >> 8));
  _send_frame(aboot, (uint8_t *)&mtu, 2, SMUX_FRAME_TYPE_HELLO);

  for (t = 0; t < 2000 && aboot->stage == STAGE_HELLO; t += 100) {
    msleep(100);
  }

  if (aboot->stage != STAGE_HELLO_DONE)
    return -1;

  aboot->stage = STAGE_RUNNING;
  return 0;
}
