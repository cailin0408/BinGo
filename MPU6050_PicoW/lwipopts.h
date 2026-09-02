#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#define NO_SYS 1

#define LWIP_SOCKET 0
#define LWIP_NETCONN 0

#define MEM_ALIGNMENT 4

#define MEM_SIZE 4000

#define MEMP_NUM_TCP_PCB 10
#define MEMP_NUM_TCP_PCB_LISTEN 8

#define MEMP_NUM_TCP_SEG 32

#define PBUF_POOL_SIZE 24

#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1

#define LWIP_RAW 1
#define LWIP_UDP 1
#define LWIP_TCP 1

#define TCP_WND (8 * TCP_MSS)
#define TCP_SND_BUF (8 * TCP_MSS)

#define LWIP_DHCP 1
#define LWIP_DNS 1

#define LWIP_NETIF_STATUS_CALLBACK 1

#define LWIP_CHKSUM_ALGORITHM 3

#define TCP_QUEUE_OOSEQ 0

#define LWIP_DHCP_DOES_ACD_CHECK 0

#define MEMP_NUM_SYS_TIMEOUT 16

#endif
