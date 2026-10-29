/* enet_native.c - ENet reliable UDP protocol helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];
static char str_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}

/* ENet protocol header: command(4) + channelID(1) + flags(1) + ... */
/* Command types */
#define ENET_CMD_NONE 0
#define ENET_CMD_ACK 1
#define ENET_CMD_CONNECT 2
#define ENET_CMD_VERIFY 3
#define ENET_CMD_DISCONNECT 4
#define ENET_CMD_PING 5
#define ENET_CMD_SEND_RELIABLE 6
#define ENET_CMD_SEND_UNRELIABLE 7
#define ENET_CMD_SEND_UNSEQ 8

/* Build protocol header */
const char *xenet_header(long long peer_id, long long sent_time) {
    unsigned char pkt[4];
    pkt[0] = (unsigned char)(peer_id >> 8);
    pkt[1] = (unsigned char)(peer_id & 0xFF);
    pkt[2] = (unsigned char)(sent_time >> 8);
    pkt[3] = (unsigned char)(sent_time & 0xFF);
    to_hex(pkt, 4, hex_buf);
    return hex_buf;
}

/* Build CONNECT command */
const char *xenet_connect(long long channel_count, long long data) {
    unsigned char pkt[12]; int pos = 0;
    pkt[pos++] = ENET_CMD_CONNECT;
    pkt[pos++] = 0; /* channelID */
    pkt[pos++] = 0; /* flags */
    pkt[pos++] = 0; /* reserved */
    /* outgoingPeerID (2 bytes) */
    pkt[pos++] = 0; pkt[pos++] = 1;
    /* incomingSessionID (1 byte) */
    pkt[pos++] = 0xFF;
    /* outgoingSessionID (1 byte) */
    pkt[pos++] = 0xFF;
    /* mtu (4 bytes) */
    pkt[pos++] = 0; pkt[pos++] = 0; pkt[pos++] = 0x05; pkt[pos++] = 0xDC; /* 1500 */
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build DISCONNECT command */
const char *xenet_disconnect(long long data) {
    unsigned char pkt[8]; int pos = 0;
    pkt[pos++] = ENET_CMD_DISCONNECT;
    pkt[pos++] = 0; pkt[pos++] = 0; pkt[pos++] = 0;
    pkt[pos++] = (unsigned char)(data >> 24);
    pkt[pos++] = (unsigned char)(data >> 16);
    pkt[pos++] = (unsigned char)(data >> 8);
    pkt[pos++] = (unsigned char)(data & 0xFF);
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build PING command */
const char *xenet_ping(void) {
    unsigned char pkt[4];
    pkt[0] = ENET_CMD_PING; pkt[1] = 0; pkt[2] = 0; pkt[3] = 0;
    to_hex(pkt, 4, hex_buf);
    return hex_buf;
}

/* Build SEND_RELIABLE command with data */
const char *xenet_send_reliable(long long channel, const char *data) {
    unsigned char pkt[4096]; int pos = 0;
    int dlen = (int)strlen(data);
    pkt[pos++] = ENET_CMD_SEND_RELIABLE;
    pkt[pos++] = (unsigned char)channel;
    pkt[pos++] = 0; /* flags */
    pkt[pos++] = 0; /* reserved */
    /* dataLength (2 bytes) */
    pkt[pos++] = (unsigned char)(dlen >> 8);
    pkt[pos++] = (unsigned char)(dlen & 0xFF);
    /* sequenceNumber (2 bytes) */
    pkt[pos++] = 0; pkt[pos++] = 1;
    memcpy(pkt + pos, data, dlen); pos += dlen;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build SEND_UNRELIABLE command with data */
const char *xenet_send_unreliable(long long channel, const char *data) {
    unsigned char pkt[4096]; int pos = 0;
    int dlen = (int)strlen(data);
    pkt[pos++] = ENET_CMD_SEND_UNRELIABLE;
    pkt[pos++] = (unsigned char)channel;
    pkt[pos++] = 0; pkt[pos++] = 0;
    pkt[pos++] = (unsigned char)(dlen >> 8);
    pkt[pos++] = (unsigned char)(dlen & 0xFF);
    pkt[pos++] = 0; pkt[pos++] = 1; /* sequence */
    memcpy(pkt + pos, data, dlen); pos += dlen;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Get command type from hex packet */
long long xenet_get_command(const char *pkt_hex) {
    unsigned int v; sscanf(pkt_hex, "%02x", &v);
    return (long long)v;
}

/* Command name */
const char *xenet_command_name(long long cmd) {
    switch(cmd) {
        case 0: return "NONE";
        case 1: return "ACK";
        case 2: return "CONNECT";
        case 3: return "VERIFY_CONNECT";
        case 4: return "DISCONNECT";
        case 5: return "PING";
        case 6: return "SEND_RELIABLE";
        case 7: return "SEND_UNRELIABLE";
        case 8: return "SEND_UNSEQUENCED";
        default: return "UNKNOWN";
    }
}

/* Event type names */
const char *xenet_event_name(long long event) {
    switch(event) {
        case 0: return "NONE";
        case 1: return "CONNECT";
        case 2: return "DISCONNECT";
        case 3: return "RECEIVE";
        default: return "UNKNOWN";
    }
}

/* Packet flags */
long long xenet_flag_reliable(void) { return 1; }
long long xenet_flag_unsequenced(void) { return 2; }
long long xenet_flag_no_allocate(void) { return 4; }

/* Default ENet port */
long long xenet_default_port(void) { return 7777; }

/* Protocol version */
long long xenet_protocol_version(void) { return 1; }

/* Validate channel count (max 255) */
long long xenet_valid_channels(long long count) {
    return (count >= 1 && count <= 255) ? 1 : 0;
}
