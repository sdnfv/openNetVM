#include "onvm_pkt_helper.h"

#include <inttypes.h>

#include <rte_branch_prediction.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_tcp.h>
#include <rte_udp.h>

struct tcp_hdr*  
onvm_pkt_tcp_hdr(struct rte_mbuf* pkt) {
        ipv4_hdr* ipv4 = onvm_pkt_ipv4_hdr(pkt);
        
        if( unlikely(ipv4 == NULL) ) { //Since we aren't dealing with IPv6 packets for now, we can ignore anything that isn't IPv4
                return NULL;
        }
        
        if( ipv4->next_proto_id != IP_PROTOCOL_TCP ) {
                return NULL;
        }
        
       /* In an IP packet, the first 4 bits determine the version.
        * The next 4 bits are called the Internet Header Length, or IHL.
        * DPDK's ipv4_hdr struct combines both the version and the IHL into one uint8_t.
        *
        * The IHL determines the number of 32-bit words that make up the IP header.
        * We need to get this value so that we know where the TCP header starts.
        */
        uint8_t ihl = ipv4->version_ihl & 0b1111;
        
        uint32_t* pkt_data = rte_pktmbuf_mtod(pkt, uint32_t*);
        return (struct tcp_hdr*)(pkt_data[ihl]);
}

struct udp_hdr* 
onvm_pkt_udp_hdr(struct rte_mbuf* pkt) {
        //IP Protocol # for UDP = 17
        ipv4_hdr* ipv4 = onvm_pkt_ipv4_hdr(pkt);
        
        if( unlikely(ipv4 == NULL) ) { //Since we aren't dealing with IPv6 packets for now, we can ignore anything that isn't IPv4
                return NULL;
        }
        
        if( ipv4->next_proto_id != IP_PROTOCOL_UDP ) {
                return NULL;
        }
        
       /* In an IP packet, the first 4 bits determine the version.
        * The next 4 bits are called the Internet Header Length, or IHL.
        * DPDK's ipv4_hdr struct combines both the version and the IHL into one uint8_t.
        *
        * The IHL determines the number of 32-bit words that make up the IP header.
        * We need to get this value so that we know where the UDP header starts.
        */
        uint8_t ihl = ipv4->version_ihl & 0b1111;
        
        uint32_t pkt_data = rte_pktmbuf_mtod(pkt, uint32_t*);
        return (struct udp_hdr*)(pkt_data[ihl]);
}

struct ipv4_hdr* 
onvm_pkt_ipv4_hdr(struct rte_mbuf* pkt) {
        ipv4_hdr* ipv4 = rte_pktmbuf_mtod(pkt, struct ipv4_hdr*);
        
        /* In an IP packet, the first 4 bits determine the version.
         * The next 4 bits are called the Internet Header Length, or IHL.
         * DPDK's ipv4_hdr struct combines both the version and the IHL into one uint8_t.
         */
        
        uint8_t version = (ipv4->version_ihl >> 4) & 0b1111;
        if( unlikely(version != 4) ) {
                return NULL;
        }
        
        return ipv4;
}


int 
onvm_pkt_is_tcp(struct rte_mbuf* pkt) {
        return onvm_pkt_tcp_hdr(pkt) != NULL;
}

int 
onvm_pkt_is_udp(struct rte_mbuf* pkt) {
        return onvm_pkt_udp_hdr(pkt) != NULL;
}

int 
onvm_pkt_is_ipv4(struct rte_mbuf* pkt) {
        return onvm_pkt_ipv4_hdr(pkt) != NULL;
}


void
onvm_pkt_print(struct rte_mbuf* pkt) {
        ipv4_hdr* ipv4 = onvm_pkt_ipv4_hdr(pkt);
        if( likely(ipv4 != NULL) ) {
                onvm_pkt_print_ipv4(ipv4);
        }
        
        tcp_hdr* tcp = onvm_pkt_tcp_hdr(pkt);
        if( tcp != NULL ) {
                onvm_pkt_print_tcp(tcp);
        }
        
        udp_hdr* udp = onvm_pkt_udp_hdr(pkt);
        if( udp != NULL ) {
                onvm_pkt_print_udp(udp);
        }
}

void
onvm_pkt_print_tcp(struct tcp_hdr* hdr) {
        printf("Source Port: %" PRIu16 "\n", hdr->src_port);
        printf("Destination Port: %" PRIu16 "\n", hdr->dst_port);
        printf("Sequence number: %" PRIu32 "\n", hdr->sent_seq);
        printf("Acknowledgement number: %" PRIu32 "\n", hdr->recv_ack);
        printf("Data offset: %" PRIu8 "\n", hdr->data_off);
        
        /* TCP defines 9 different 1-bit flags, but DPDK's flags field only leaves room for 8.
         * I think DPDK's struct puts the first flag in the last bit of the data offset field.
         */
        uint16_t flags = ((hdr->data_off << 8) | hdr->tcp_flags) & 0b111111111;
        
        printf("Flags: %" PRIx16 "\n", flags);
        printf("\t(");
        if( (flags >> 8) & 0x1 ) printf("NS,");
        if( (flags >> 7) & 0x1 ) printf("CWR,");
        if( (flags >> 6) & 0x1 ) printf("ECE,");
        if( (flags >> 5) & 0x1 ) printf("URG,");
        if( (flags >> 4) & 0x1 ) printf("ACK,");
        if( (flags >> 3) & 0x1 ) printf("PSH,");
        if( (flags >> 2) & 0x1 ) printf("RST,");
        if( (flags >> 1) & 0x1 ) printf("SYN,");
        if(  flags       & 0x1 ) printf("FIN,");
        printf(")\n");
        
        printf("Window Size: %" PRIu16 "\n", hdr->rx_win);
        printf("Checksum: %" PRIu16 "\n", hdr->cksum);
        printf("Urgent Pointer: %" PRIu16 "\n", hdr->tcp_urp);
}

void
onvm_pkt_print_udp(struct udp_hdr* hdr) {
        printf("Source Port: %" PRIu16 "\n", hdr->src_port);
        printf("Destination Port: %" PRIu16 "\n", hdr->dst_port);
        printf("Length: %" PRIu16 "\n", hdr->dgram_len);
        printf("Checksum: %" PRIu16 "\n", hdr->dgram_cksum);
}

void
onvm_pkt_print_ipv4(struct ipv4_hdr* hdr) {
        printf("IHL: %" PRIu8 "\n", hdr->version_ihl & 0b1111);
        printf("DSCP: %" PRIu8 "\n", (hdr->type_of_service >> 2) & 0b111111);
        printf("ECN: %" PRIu8 "\n", hdr->type_of_service & 0b11);
        printf("Total Length: %" PRIu16 "\n", hdr->total_length);
        printf("Identification: %" PRIu16 "\n", hdr->packet_id );
        
        uint8_t flags = (hdr->fragment_offset >> 13) & 0b111; //there are three 1-bit flags, but only 2 are used
        printf("Flags: %" PRIx8 "\n", flags);
        printf("\t(");
        if( (flags >> 1) & 0x1 ) printf("DF,");
        if(  flags       & 0x1 ) printf("MF,");
        printf("\n");
        
        printf("Fragment Offset: %" PRIu16 "\n", hdr->fragment_offset & 0b1111111111111);
        printf("TTL: %" PRIu8 "\n", hdr->time_to_live);
        printf("Protocol: " PRIu8, hdr->next_proto_id);
        
        if( hdr->next_proto_id == IP_PROTOCOL_TCP ) {
                printf(" (TCP)");
        }
        
        if( hdr->next_proto_id == IP_PROTOCOL_UDP ) {
                printf(" (UDP)");
        }
        
        printf("\n");
        
        printf("Header Checksum: %" PRIu16 "\n", hdr->hdr_checksum);
        printf("Source IP: %" PRIu32 " (%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ")\n", hdr->src_addr,
                (hdr->src_addr >> 24) & 0xFF, (hdr->src_addr >> 16) & 0xFF, (hdr->src_addr >> 8) & 0xFF, hdr->src_addr & 0xFF);
        printf("Destination IP: %" PRIu32 " (%" PRIu8 ".%" PRIu8 ".%" PRIu8 ".%" PRIu8 ")\n", hdr->dst_addr,
                (hdr->dst_addr >> 24) & 0xFF, (hdr->dst_addr >> 16) & 0xFF, (hdr->dst_addr >> 8) & 0xFF, hdr->dst_addr & 0xFF);
}