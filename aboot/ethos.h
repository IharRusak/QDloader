/******************************************************************************
  @file    ethos.h
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
#ifndef _ABOOT_ETHOS_H_
#define _ABOOT_ETHOS_H_
#include "../std_c.h"

#define msleep(_t) usleep(_t*1000)

typedef struct {
  uint8_t Id;
  uint8_t Flag;
  uint16_t Seq;
  union {
    uint8_t u8[0];
    uint16_t u16[0];
  };
} Header;

struct uip_eth_hdr {
  uint8_t dest_mac[6];
  uint8_t src_mac[6];
  uint16_t type;
};

typedef struct {
  uint16_t sequence;
  Header header;
  uint8_t header_data[2048];
  struct uip_eth_hdr eth_hdr;
  uint8_t eth_data[2048];
} uip_t;

typedef struct {
  uint8_t mac_addr[6];            /**< this device's MAC address */
  uint8_t remote_mac_addr[6];     /**< this device's MAC address */
  uint8_t ipv6_addr[16];     /**< this device's MAC address */
  uint8_t remote_ipv6_addr[16];     /**< this device's MAC address */
  uip_t uip;
} ethos_t;

typedef struct {
  int fd;
  uint8_t txbuf[64*1024];
  uint8_t rxbuf[2048];
} usb_t;

struct aboot;
typedef struct aboot {
  FILE *fw_fp;
  void (*process_incoming_data)(struct aboot *aboot, const uint8_t *data, size_t len);

  uint32_t aboot_device_type;
  uint32_t stage;
  uint32_t state;             /**< Line status variable */
  size_t framesize;               /**< size of currently incoming frame */
  uint32_t frametype;   /**< type of currently incoming frame */
  uint16_t mtu;
  char cmd_response[128];

  usb_t usb;
  ethos_t ethos;

  uint8_t txbuf[2048];
  uint8_t rxbuf[2048];
} aboot_t;

extern void aboot_transport_rx_cmd(aboot_t *aboot, const uint8_t *cmd, size_t size);
extern int serial_port_write(aboot_t *aboot, uint8_t *buf, size_t len);
extern int uip_decode(aboot_t *aboot, const void *data, int len, int is_out);
extern int udp_transport_init(aboot_t *aboot);

extern int smux_init(aboot_t *aboot);
extern int smux_write_aboot_data(aboot_t *aboot, const uint8_t *data, size_t len);
extern int smux_write_aboot_cmd(aboot_t *aboot, const uint8_t *data, size_t len);

extern int ethos_init(aboot_t *aboot);
extern int ethos_write_aboot_data(aboot_t *aboot, const void *data, int len, int end_of_data);
extern int ethos_write_aboot_cmd(aboot_t *aboot, const uint8_t *cmd, size_t len);
extern int ethos_send_data_request(aboot_t *aboot, size_t size);
extern int ethos_send_data(aboot_t *aboot, const uint8_t *data, size_t size);
extern int ethos_send_text(aboot_t *aboot, const uint8_t *data, size_t size);
#endif
