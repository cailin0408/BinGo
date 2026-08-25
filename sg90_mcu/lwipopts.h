#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// 啟用核心功能 (NO_SYS = 1 代表無作業系統模式)
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// 記憶體與 Buffer 配置
#define MEM_LIBC_MALLOC             0
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24

// ⚠️ 關鍵修復：必須加入這行！否則連線 MQTT 會因為定時器記憶體池耗盡而 PANIC 崩潰
#define MEMP_NUM_SYS_TIMEOUT        16

// 基礎網路協定開啟
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_DHCP                   1

// 關鍵：啟用 MQTT 客戶端支援
#define LWIP_MQTT                   1

// 避免缺少的通用定義
#define SYS_LIGHTWEIGHT_PROT        1
#define TCPIP_THREAD_PRIO           1
#define TCPIP_THREAD_STACKSIZE      1024
#define DEFAULT_THREAD_STACKSIZE    1024

#endif /* _LWIPOPTS_H */