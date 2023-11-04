/******************************************************************************
  @file    uip.c
  @brief   uip.

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
#include <netinet/in.h>
#include <sys/time.h>

#define UIP_ETHTYPE_ARP  0x0806
#define UIP_ETHTYPE_IP   0x0800
#define UIP_ETHTYPE_IPV6 0x86dd

#define UIP_PROTO_ICMP  1
#define UIP_PROTO_TCP   6
#define UIP_PROTO_UDP   17
#define UIP_PROTO_ICMP6 58

#define kProtocolVersion    1
#define kMinPacketSize      512
#define kHostMaxPacketSize 2048
#define kHeaderSize         4

typedef enum {
  kFlagNone = 0x00,
  kFlagContinuation = 0x01
} Flag;

typedef enum {
  kIdError = 0x00,
  kIdDeviceQuery = 0x01,
  kIdInitialization = 0x02,
  kIdFastboot = 0x03,
  kIdMax = 0x04
} Id;

/* IPV6 header */
struct uip_ip_hdr {
  uint8_t vtc;
  uint8_t tcflow;
  uint16_t flow;
  uint16_t len;
  uint8_t proto;
  uint8_t ttl;
  uint8_t srcipaddr[16];
  uint8_t destipaddr[16];
} __attribute__ ((packed));
#define UIP_IPH_LEN    40

#define ICMP6_RS                        133  /**< Router Solicitation */
#define ICMP6_RA                        134  /**< Router Advertisement */
#define ICMP6_NS                        135  /**< Neighbor Solicitation */
#define ICMP6_NA                        136  /**< Neighbor advertisement */
#define ICMP6_REDIRECT                  137  /**< Redirect */

struct uip_icmp_hdr {
  uint8_t type, icode;
  uint16_t icmpchksum;
};
#define UIP_ICMPH_LEN   4    /* Size of ICMP header */

struct uip_nd6_ns {
  uint32_t reserved;
  uint8_t tgtipaddr[16];
};
#define UIP_ND6_NS_LEN                  20

struct uip_nd6_na {
  uint8_t flagsreserved;
  uint8_t reserved[3];
  uint8_t tgtipaddr[16];
};

struct uip_udp_hdr {
  uint16_t srcport;
  uint16_t destport;
  uint16_t udplen;
  uint16_t udpchksum;
};

#define UIP_UDPH_LEN    8    /* Size of UDP header */
#define UIP_IPUDPH_LEN (UIP_UDPH_LEN + UIP_IPH_LEN)   /* Size of IP + UDP header */
#define UIP_IP_PAYLOAD(ext)                        ((unsigned char *)ip_hdr + UIP_IPH_LEN + (ext))

#define kClientPort         5553
#define kServerPort         5554

static int uip_ext_len = 0;
static int rx_ns = 0;
static int rx_na = 0;
static int device_state = 0;

static uint16_t chksum(uint16_t sum, const uint8_t *data, uint16_t len)
{
  uint16_t t;
  const uint8_t *dataptr;
  const uint8_t *last_byte;

  dataptr = data;
  last_byte = data + len - 1;

  while(dataptr < last_byte) {   /* At least two more bytes */
    t = (dataptr[0] << 8) + dataptr[1];
    sum += t;
    if(sum < t) {
      sum++;      /* carry */
    }
    dataptr += 2;
  }

  if(dataptr == last_byte) {
    t = (dataptr[0] << 8) + 0;
    sum += t;
    if(sum < t) {
      sum++;      /* carry */
    }
  }

  /* Return sum in host byte order. */
  return sum;
}

static uint16_t upper_layer_chksum(struct uip_ip_hdr *ip_hdr)
{
  volatile uint16_t upper_layer_len;
  uint16_t sum;

  upper_layer_len = ntohs(ip_hdr->len) - uip_ext_len;

  /* First sum pseudoheader. */
  /* IP protocol and length fields. This addition cannot carry. */
  sum = upper_layer_len + ip_hdr->proto;
  /* Sum IP source and destination addresses. */
  sum = chksum(sum, (uint8_t *)&ip_hdr->srcipaddr, 2 * sizeof(ip_hdr->srcipaddr));

  /* Sum upper-layer header and data. */
  sum = chksum(sum, UIP_IP_PAYLOAD(uip_ext_len), upper_layer_len);

  return (sum == 0) ? 0xffff : htons(sum);
}

static uint16_t uip_icmp6chksum(struct uip_ip_hdr *ip_hdr)
{
  return upper_layer_chksum(ip_hdr);
}

static uint16_t uip_udpchksum(struct uip_ip_hdr *ip_hdr)
{
  return upper_layer_chksum(ip_hdr);
}

void header_set(Header *header, uint8_t id, uint16_t sequence, Flag flag)
{
  header->Id = id;
  header->Flag = flag;
  header->Seq = htobe16(sequence);
}

static int neighbor_solicitation_send(aboot_t *aboot)
{
  ethos_t *ethos = &aboot->ethos;
  struct uip_eth_hdr *eth_hdr = &ethos->uip.eth_hdr;
  struct uip_ip_hdr *ip_hdr = (struct uip_ip_hdr *)(eth_hdr + 1);
  struct uip_icmp_hdr *icmp_hdr = (struct uip_icmp_hdr *)(ip_hdr + 1);
  struct uip_nd6_ns *ns = (struct uip_nd6_ns *)(icmp_hdr + 1);
  uint8_t *llao = (uint8_t *)(ns + 1);
  uint32_t udp_len;
  int ret;

  memcpy(&eth_hdr->dest_mac, ethos->remote_mac_addr, sizeof(eth_hdr->dest_mac));
  memcpy(&eth_hdr->src_mac, ethos->mac_addr, sizeof(eth_hdr->src_mac));
  eth_hdr->type = htons(UIP_ETHTYPE_IPV6);

  eth_hdr->dest_mac[0] = 0x33;
  eth_hdr->dest_mac[1] = 0x33;
  eth_hdr->dest_mac[2] = 0xff;
  eth_hdr->dest_mac[3] = ethos->remote_ipv6_addr[13];
  eth_hdr->dest_mac[4] = ethos->remote_ipv6_addr[14];
  eth_hdr->dest_mac[5] = ethos->remote_ipv6_addr[15];

  ip_hdr->vtc = 0x60;
  ip_hdr->tcflow = 0x00;
  ip_hdr->flow = htons(0x00);
  ip_hdr->len = htons(UIP_ICMPH_LEN + UIP_ND6_NS_LEN + 8);
  ip_hdr->proto = UIP_PROTO_ICMP6;
  ip_hdr->ttl = 0xff;
  memcpy(ip_hdr->srcipaddr, ethos->ipv6_addr, sizeof(ip_hdr->srcipaddr));
  memset(ip_hdr->destipaddr, 0x00, 16);
  ip_hdr->destipaddr[0] = 0xFF;
  ip_hdr->destipaddr[1] = 0x02; 
  ip_hdr->destipaddr[11] = 0x01;
  ip_hdr->destipaddr[12] = 0xFF;
  memcpy(ip_hdr->destipaddr + 13, ethos->remote_ipv6_addr + 13, 3);
  
  icmp_hdr->type = ICMP6_NS;
  icmp_hdr->icode = 0;
  ns->reserved = 0;
  memcpy(ns->tgtipaddr, ethos->remote_ipv6_addr, sizeof(ns->tgtipaddr));
  llao[0] = 1; //ND6_OPT_SLLAO
  llao[1] = 1;
  memcpy(llao + 2, ethos->mac_addr, sizeof(ethos->mac_addr));

  icmp_hdr->icmpchksum = 0;
  icmp_hdr->icmpchksum = ~(uip_icmp6chksum(ip_hdr));

  udp_len = sizeof(struct uip_eth_hdr) +  sizeof(struct uip_ip_hdr) + ntohs(ip_hdr->len);

  uip_decode(aboot, eth_hdr, udp_len, 1);
  ret = ethos_send_data(aboot, (uint8_t *)eth_hdr, udp_len);

  return ret;
}

static int neighbor_advertisement_send(aboot_t *aboot)
{
  ethos_t *ethos = &aboot->ethos;
  struct uip_eth_hdr *eth_hdr = &ethos->uip.eth_hdr;
  struct uip_ip_hdr *ip_hdr = (struct uip_ip_hdr *)(eth_hdr + 1);
  struct uip_icmp_hdr *icmp_hdr = (struct uip_icmp_hdr *)(ip_hdr + 1);
  struct uip_nd6_na *na = (struct uip_nd6_na *)(icmp_hdr + 1);
  uint8_t *llao = (uint8_t *)(na + 1);
  uint32_t udp_len;

  //dprintf("%s\n", __func__);
/*
0040   7e 02 c4 4f 11 e8 ff 02 e3 96 6c 7c 41 86 dd 60   ~..O......l|A..`
0050   00 00 00 00 20 3a ff fe 80 00 00 00 00 00 00 00   .... :..........
0060   e3 96 ff fe 6c 7c 41 fe 80 00 00 00 00 00 00 00   ....l|A.........
0070   c4 4f ff fe 11 e8 ff 88 00 27 98 e0 00 00 00 fe   .O.......'......
0080   80 00 00 00 00 00 00 00 e3 96 ff fe 6c 7c 41 02   ............l|A.
0090   01 02 e3 96 6c 7c 41 7e                           ....l|A~
*/

  memcpy(&eth_hdr->dest_mac, ethos->remote_mac_addr, sizeof(eth_hdr->dest_mac));
  memcpy(&eth_hdr->src_mac, ethos->mac_addr, sizeof(eth_hdr->src_mac));
  eth_hdr->type = htons(UIP_ETHTYPE_IPV6);

  ip_hdr->vtc = 0x60;
  ip_hdr->tcflow = 0x00;
  ip_hdr->flow = htons(0x00);
  ip_hdr->len = htons(UIP_ICMPH_LEN + UIP_ND6_NS_LEN + 8);
  ip_hdr->proto = UIP_PROTO_ICMP6;
  ip_hdr->ttl = 0xff;
  memcpy(ip_hdr->srcipaddr, ethos->ipv6_addr, sizeof(ip_hdr->srcipaddr));
  memcpy(ip_hdr->destipaddr, ethos->remote_ipv6_addr, sizeof(ip_hdr->destipaddr));

  icmp_hdr->type = ICMP6_NA;
  icmp_hdr->icode = 0;
  na->flagsreserved = 0x40;
  memset(na->reserved, 0x00, sizeof(na->reserved));
  memcpy(na->tgtipaddr, ethos->ipv6_addr, sizeof(na->tgtipaddr));
  llao[0] = 2; //UIP_ND6_OPT_TLLAO
  llao[1] = 1;
  memcpy(llao + 2, ethos->mac_addr, sizeof(ethos->mac_addr));

  icmp_hdr->icmpchksum = 0;
  icmp_hdr->icmpchksum = ~(uip_icmp6chksum(ip_hdr));

  udp_len = sizeof(struct uip_eth_hdr) +  sizeof(struct uip_ip_hdr) + ntohs(ip_hdr->len);

  uip_decode(aboot, eth_hdr, udp_len, 1);
  return ethos_send_data(aboot, (const uint8_t *)eth_hdr, udp_len);
}

static int uip_sendto(aboot_t *aboot, const void *data, int len)
{
  ethos_t *ethos = &aboot->ethos;
  struct uip_eth_hdr *eth_hdr = &ethos->uip.eth_hdr;
  struct uip_ip_hdr *ip_hdr = (struct uip_ip_hdr *)(eth_hdr + 1);
  struct uip_udp_hdr *udp_hdr = (struct uip_udp_hdr *)(ip_hdr + 1);
  uint32_t udp_len;
  int ret;

  if (rx_ns) {
    neighbor_advertisement_send(aboot);
    rx_ns = 0;
  }

  memcpy(&eth_hdr->dest_mac, ethos->remote_mac_addr, sizeof(eth_hdr->dest_mac));
  memcpy(&eth_hdr->src_mac, ethos->mac_addr, sizeof(eth_hdr->src_mac));
  eth_hdr->type = htons(UIP_ETHTYPE_IPV6);

  ip_hdr->vtc = 0x60;
  ip_hdr->tcflow = 0x00;
  ip_hdr->flow = htons(0x00);
  ip_hdr->len = htons(UIP_UDPH_LEN + len);
  ip_hdr->proto = UIP_PROTO_UDP;
  ip_hdr->ttl = 0x40;
  memcpy(ip_hdr->srcipaddr, ethos->ipv6_addr, sizeof(ip_hdr->srcipaddr));
  memcpy(ip_hdr->destipaddr, ethos->remote_ipv6_addr, sizeof(ip_hdr->destipaddr));

  udp_hdr->srcport = htons(kClientPort);
  udp_hdr->destport = htons(kServerPort);
  udp_hdr->udplen = htons(UIP_UDPH_LEN + len);
  memcpy(udp_hdr + 1, data, len);
  udp_hdr->udpchksum = 0;
  udp_hdr->udpchksum = ~(uip_udpchksum(ip_hdr));

  udp_len = sizeof(struct uip_eth_hdr) +  sizeof(struct uip_ip_hdr) + ntohs(ip_hdr->len);
  uip_decode(aboot, eth_hdr, udp_len, 1);
  ret = ethos_send_data(aboot, (const uint8_t *)eth_hdr, udp_len);

  return ret;
}

int uip_decode(aboot_t *aboot, const void *data, int len, int is_out)
{
  ethos_t *ethos = &aboot->ethos;
  struct uip_eth_hdr *eth_hdr = (struct uip_eth_hdr *)(data);
  struct uip_ip_hdr *ip_hdr = (struct uip_ip_hdr *)(eth_hdr + 1);
  const char *flag = is_out ? "[cmd] >" : "[cmd] <";

  if (eth_hdr->type != htons(UIP_ETHTYPE_IPV6)) {
    dprintf("ipv6 un-match!\n");
    return 0;
  }

  //dprintf("ip_hdr: len=%d, proto=%d ", ntohs(ip_hdr->len), ip_hdr->proto);

  if (ip_hdr->proto == UIP_PROTO_ICMP6) {
    struct uip_icmp_hdr *icmp_hdr = (struct uip_icmp_hdr *)(ip_hdr + 1);

    if (icmp_hdr->type == ICMP6_NS) {
      dprintf("%s ICMP6: Neighbor Solicitation\n", flag);
      if (!is_out) rx_ns++;
    }
    else if (icmp_hdr->type == ICMP6_NA) {
      //struct uip_nd6_na *na = (struct uip_nd6_na *)(icmp_hdr + 1);
      dprintf("%s ICMP6: Neighbor advertisement\n", flag);
      rx_na = 1;
    }
    else if (icmp_hdr->type == ICMP6_RS) {
      dprintf("%s ICMP6: Router Solicitation\n", flag);
    }
    else if (icmp_hdr->type == ICMP6_RA) {
      dprintf("%s ICMP6: Router advertisement\n", flag);
    }
  }
  else if (ip_hdr->proto == UIP_PROTO_UDP)
  {
    struct uip_udp_hdr *udp_hdr = (struct uip_udp_hdr *)(ip_hdr + 1);

    if (udp_hdr->srcport == htons(kServerPort) && udp_hdr->destport == htons(kClientPort))
    {
      Header *header = (Header *)(udp_hdr + 1);
      uint16_t seq = ethos->uip.sequence - 1;

      if (header->Id != kIdDeviceQuery && seq != be16toh(header->Seq))
      {
        dprintf("sequence: req %d, rsp %d\n", seq, be16toh(header->Seq));
        return 0;
      }

      switch (header->Id)
      {
        case kIdDeviceQuery:
          ethos->uip.sequence = be16toh(header->u16[0]);
          dprintf("%s sequence = %04x\n", flag, ethos->uip.sequence);
          device_state = kIdDeviceQuery;
          break;
          
        case kIdInitialization:
        {
            uint16_t version = be16toh(header->u16[0]);
            uint16_t packet_size = be16toh(header->u16[1]);
            dprintf("%s version = %04x, packet_size=%04x\n", flag, version, packet_size);
            aboot->mtu = packet_size - kHeaderSize;
            device_state = kIdInitialization;
        }
        break;
          
        case kIdFastboot:
        {
          aboot_transport_rx_cmd(aboot, header->u8, ntohs(udp_hdr->udplen) - UIP_UDPH_LEN - kHeaderSize);
        }
        break;

       case kIdError:
       {
          aboot_transport_rx_cmd(aboot, header->u8, ntohs(udp_hdr->udplen) - UIP_UDPH_LEN - kHeaderSize);
        }
       default:
       break;
      }
    }
  }
      
  return 1;
}

int ethos_write_aboot_data(aboot_t *aboot, const void *data, int len, int end_of_data)
{
  ethos_t *ethos = &aboot->ethos;
  Header *header = &ethos->uip.header;
  
  header_set(header, kIdFastboot, ethos->uip.sequence++, end_of_data ? kFlagNone : kFlagContinuation);
  if (data && len) {
    memcpy(header->u8, data, len);
  }

  return uip_sendto(aboot, (uint8_t *)header, kHeaderSize + len);
}

int ethos_write_aboot_cmd(aboot_t *aboot, const uint8_t *cmd, size_t len)
{
  return ethos_write_aboot_data(aboot, cmd, len, 1);  
}

int ethos_send_data_request(aboot_t *aboot, size_t size)
{
  ethos_t *ethos = &aboot->ethos;
  Header *header = &ethos->uip.header;

  header_set(header, kIdFastboot, ethos->uip.sequence++, kFlagNone);
  sprintf((char *)header->u8, "data-request:%08x", (uint32_t)size);
  dprintf("[cmd] > %s\n", (char *)header->u8);
  return ethos_send_text(aboot, (uint8_t *)header, strlen((const char *)header->u8) + 4);
}

int udp_transport_init(aboot_t *aboot)
{
  uint32_t t;
  int ret;
  ethos_t *ethos = &aboot->ethos;
  Header *header = &ethos->uip.header;

  memset(ethos->remote_ipv6_addr, 0x00, 16);
  ethos->remote_ipv6_addr[0] = 0xfe;
  ethos->remote_ipv6_addr[1] = 0x80;
  memcpy(ethos->remote_ipv6_addr + 8, ethos->remote_mac_addr, 3);
  ethos->remote_ipv6_addr[11] = 0xff;
  ethos->remote_ipv6_addr[12] = 0xfe;
  memcpy(ethos->remote_ipv6_addr + 13, ethos->remote_mac_addr + 3, 3);
  ethos->remote_ipv6_addr[8] ^= 0x02;

  memset(ethos->ipv6_addr, 0x00, 16);
  ethos->ipv6_addr[0] = 0xfe;
  ethos->ipv6_addr[1] = 0x80;
  memcpy(ethos->ipv6_addr + 8, ethos->mac_addr, 3);
  ethos->ipv6_addr[11] = 0xff;
  ethos->ipv6_addr[12] = 0xfe;
  memcpy(ethos->ipv6_addr + 13, ethos->mac_addr + 3, 3);
  ethos->ipv6_addr[8] ^= 0x02;

  msleep(300); //from usbmon
  rx_na = 0;
  ret = neighbor_solicitation_send(aboot);
  if (ret) return ret;
  for (t = 0; t < 2000; t += 100) {
    if (rx_na) break;
    msleep(10);
  }
  if (!rx_na) return -1;

  header_set(header, kIdDeviceQuery, ethos->uip.sequence++, kFlagNone);
  device_state = 0;
  ret = uip_sendto(aboot, (uint8_t *)header, kHeaderSize);
  if (ret) return ret;
  for (t = 0; t < 2000 && device_state == 0; t += 100) {
    msleep(100);
  }
  if (device_state != kIdDeviceQuery) return -1;

  header_set(header, kIdInitialization, ethos->uip.sequence++, kFlagNone);
  header->u16[0] = htobe16(kProtocolVersion);
  header->u16[1] = htobe16(kHostMaxPacketSize);
  device_state = 0;
  ret = uip_sendto(aboot, (uint8_t *)header, kHeaderSize + 4);
  if (ret) return ret;
  for (t = 0; t < 2000 && device_state == 0; t += 100) {
    msleep(100);
  }
  if (device_state != kIdInitialization) return  -1;

  return ret;
}
