/* pcap_native.c - Packet capture format helpers (pcap file format parsing) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Parse pcap global header magic to determine endianness/version */
long long xpcap_magic_type(long long magic) {
    if (magic == 0xa1b2c3d4) return 1;  /* microsecond, big-endian read */
    if (magic == 0xd4c3b2a1) return 2;  /* microsecond, little-endian read */
    if (magic == 0xa1b23c4d) return 3;  /* nanosecond, big-endian read */
    if (magic == 0x4d3cb2a1) return 4;  /* nanosecond, little-endian read */
    return 0; /* unknown */
}

/* Link type name */
const char* xpcap_linktype_name(long long linktype) {
    switch (linktype) {
        case 0: return "NULL";
        case 1: return "ETHERNET";
        case 6: return "TOKEN_RING";
        case 9: return "PPP";
        case 101: return "RAW_IP";
        case 105: return "IEEE802_11";
        case 113: return "LINUX_SLL";
        case 228: return "IPV4";
        case 229: return "IPV6";
        default: return "UNKNOWN";
    }
}

/* Parse Ethernet frame header from hex string, returns ethertype */
static long long eth_src_mac_hi, eth_src_mac_lo;
static long long eth_dst_mac_hi, eth_dst_mac_lo;
long long xpcap_eth_type(const char *frame_hex) {
    unsigned int b[14];
    int n = sscanf(frame_hex, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                   &b[0],&b[1],&b[2],&b[3],&b[4],&b[5],&b[6],&b[7],&b[8],&b[9],&b[10],&b[11],&b[12],&b[13]);
    if (n < 14) return 0;
    eth_dst_mac_hi = ((long long)b[0] << 16) | ((long long)b[1] << 8) | b[2];
    eth_dst_mac_lo = ((long long)b[3] << 16) | ((long long)b[4] << 8) | b[5];
    eth_src_mac_hi = ((long long)b[6] << 16) | ((long long)b[7] << 8) | b[8];
    eth_src_mac_lo = ((long long)b[9] << 16) | ((long long)b[10] << 8) | b[11];
    return (long long)((b[12] << 8) | b[13]);
}

/* Get MAC address parts from last eth parse */
long long xpcap_eth_dst_hi(void) { return eth_dst_mac_hi; }
long long xpcap_eth_dst_lo(void) { return eth_dst_mac_lo; }
long long xpcap_eth_src_hi(void) { return eth_src_mac_hi; }
long long xpcap_eth_src_lo(void) { return eth_src_mac_lo; }

/* Format MAC as string from hi/lo */
const char* xpcap_mac_str(long long hi, long long lo) {
    snprintf(str_buf, sizeof(str_buf), "%02x:%02x:%02x:%02x:%02x:%02x",
             (int)((hi >> 16) & 0xFF), (int)((hi >> 8) & 0xFF), (int)(hi & 0xFF),
             (int)((lo >> 16) & 0xFF), (int)((lo >> 8) & 0xFF), (int)(lo & 0xFF));
    return str_buf;
}

/* Ethertype name */
const char* xpcap_ethertype_name(long long ethertype) {
    switch (ethertype) {
        case 0x0800: return "IPv4";
        case 0x0806: return "ARP";
        case 0x86DD: return "IPv6";
        case 0x8100: return "VLAN";
        case 0x8847: return "MPLS";
        case 0x88CC: return "LLDP";
        default: return "UNKNOWN";
    }
}

/* Parse IPv4 header from hex (starting at IP header), returns protocol */
static long long ip_src, ip_dst, ip_ttl, ip_len;
long long xpcap_ipv4_proto(const char *ip_hex) {
    unsigned int b[20];
    int n = sscanf(ip_hex,
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        &b[0],&b[1],&b[2],&b[3],&b[4],&b[5],&b[6],&b[7],&b[8],&b[9],
        &b[10],&b[11],&b[12],&b[13],&b[14],&b[15],&b[16],&b[17],&b[18],&b[19]);
    if (n < 20) return 0;
    ip_len = (long long)((b[2] << 8) | b[3]);
    ip_ttl = (long long)b[8];
    ip_src = ((long long)b[12] << 24) | ((long long)b[13] << 16) | ((long long)b[14] << 8) | b[15];
    ip_dst = ((long long)b[16] << 24) | ((long long)b[17] << 16) | ((long long)b[18] << 8) | b[19];
    return (long long)b[9];
}
long long xpcap_ip_src(void) { return ip_src; }
long long xpcap_ip_dst(void) { return ip_dst; }
long long xpcap_ip_ttl(void) { return ip_ttl; }
long long xpcap_ip_len(void) { return ip_len; }

/* Format IPv4 address from integer */
const char* xpcap_ip_str(long long addr) {
    snprintf(str_buf, sizeof(str_buf), "%d.%d.%d.%d",
             (int)((addr >> 24) & 0xFF), (int)((addr >> 16) & 0xFF),
             (int)((addr >> 8) & 0xFF), (int)(addr & 0xFF));
    return str_buf;
}

/* IP protocol name */
const char* xpcap_proto_name(long long proto) {
    switch (proto) {
        case 1: return "ICMP";
        case 2: return "IGMP";
        case 6: return "TCP";
        case 17: return "UDP";
        case 47: return "GRE";
        case 50: return "ESP";
        case 58: return "ICMPv6";
        case 89: return "OSPF";
        default: return "UNKNOWN";
    }
}

/* Parse TCP header from hex, returns dst port; src port via getter */
static long long tcp_src_port, tcp_seq, tcp_ack, tcp_flags;
long long xpcap_tcp_dst_port(const char *tcp_hex) {
    unsigned int b[20];
    int n = sscanf(tcp_hex,
        "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        &b[0],&b[1],&b[2],&b[3],&b[4],&b[5],&b[6],&b[7],&b[8],&b[9],
        &b[10],&b[11],&b[12],&b[13],&b[14],&b[15],&b[16],&b[17],&b[18],&b[19]);
    if (n < 20) return 0;
    tcp_src_port = (long long)((b[0] << 8) | b[1]);
    tcp_seq = ((long long)b[4] << 24) | ((long long)b[5] << 16) | ((long long)b[6] << 8) | b[7];
    tcp_ack = ((long long)b[8] << 24) | ((long long)b[9] << 16) | ((long long)b[10] << 8) | b[11];
    tcp_flags = (long long)b[13];
    return (long long)((b[2] << 8) | b[3]);
}
long long xpcap_tcp_src_port(void) { return tcp_src_port; }
long long xpcap_tcp_seq(void) { return tcp_seq; }
long long xpcap_tcp_ack(void) { return tcp_ack; }
long long xpcap_tcp_flags(void) { return tcp_flags; }

/* TCP flags decode */
const char* xpcap_tcp_flags_str(long long flags) {
    static char fbuf[32];
    int pos = 0;
    if (flags & 0x02) fbuf[pos++] = 'S';
    if (flags & 0x10) fbuf[pos++] = 'A';
    if (flags & 0x01) fbuf[pos++] = 'F';
    if (flags & 0x04) fbuf[pos++] = 'R';
    if (flags & 0x08) fbuf[pos++] = 'P';
    if (flags & 0x20) fbuf[pos++] = 'U';
    fbuf[pos] = '\0';
    return fbuf;
}

/* Well-known port name */
const char* xpcap_port_name(long long port) {
    switch (port) {
        case 20: return "FTP-DATA";
        case 21: return "FTP";
        case 22: return "SSH";
        case 23: return "TELNET";
        case 25: return "SMTP";
        case 53: return "DNS";
        case 80: return "HTTP";
        case 110: return "POP3";
        case 143: return "IMAP";
        case 443: return "HTTPS";
        case 993: return "IMAPS";
        case 3306: return "MYSQL";
        case 5432: return "POSTGRESQL";
        case 6379: return "REDIS";
        case 8080: return "HTTP-ALT";
        default: return "UNKNOWN";
    }
}
