/**********************************************************************************
                       NFD project
   A C++ based NF developing framework designed by Wenfei's group
   from IIIS, Tsinghua University, China.
******************************************************************************/

/************************************************************************************
* Filename:   decode.h
* Author:     Hongyi Huang(hhy17 AT mails.tsinghua.edu.cn), Bangwen Deng, Wenfei Wu
* Copyright:
* Disclaimer: This code is presented "as is" without any guarantees.
* Details:    This file is a supprot file for NFD project, containing the methods maybe
              used in NFD NF.
*************************************************************************************/

#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
typedef struct _EtherHdr {
        unsigned char ether_dst[6];
        unsigned char ether_src[6];
        unsigned short ether_type;
} EtherHdr;

typedef struct _IPHdr {
#if defined(WORDS_BIGENDIAN)
        u_char ip_ver : 4, /* IP version */
            ip_hlen : 4;   /* IP header length */
#else
        u_char ip_hlen : 4, ip_ver : 4;
#endif
        u_char ip_tos;  /* type of service */
        u_short ip_len; /* datagram length */
        u_short ip_id;  /* identification  */
        u_short ip_off;
        u_char ip_ttl;         /* time to live field */
        u_char ip_proto;       /* datagram protocol */
        u_short ip_csum;       /* checksum */
        struct in_addr ip_src; /* source IP */
        struct in_addr ip_dst; /* dest IP */
} IPHdr;
typedef struct _TCPHdr {
        u_short th_sport; /* source port */
        u_short th_dport; /* destination port */
        uint32_t th_seq;    /* sequence number */
        uint32_t th_ack;    /* acknowledgement number */
#ifdef WORDS_BIGENDIAN
        u_char th_off : 4, /* data offset */
            th_x2 : 4;     /* (unused) */
#else
        u_char th_x2 : 4, /* (unused) */
            th_off : 4;   /* data offset */
#endif
        u_char th_flags;
        u_short th_win; /* window */
        u_short th_sum; /* checksum */
        u_short th_urp; /* urgent pointer */
} TCPHdr;
