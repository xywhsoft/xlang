/* mqtt_native.c - MQTT 3.1.1 packet encoding/parsing */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];
static char str_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}
static int from_hex(const char *hex, unsigned char *out) {
    int len = (int)strlen(hex)/2, i;
    for (i = 0; i < len; i++) { unsigned int v; sscanf(hex+i*2, "%02x", &v); out[i] = (unsigned char)v; }
    return len;
}

/* Encode MQTT remaining length (variable-length encoding) */
static int enc_remaining_len(int len, unsigned char *out) {
    int pos = 0;
    do {
        unsigned char b = len % 128;
        len /= 128;
        if (len > 0) b |= 0x80;
        out[pos++] = b;
    } while (len > 0);
    return pos;
}

/* Encode UTF-8 string (2-byte length prefix) */
static int enc_str(unsigned char *out, const char *s) {
    int slen = (int)strlen(s);
    out[0] = (unsigned char)(slen >> 8);
    out[1] = (unsigned char)(slen & 0xFF);
    memcpy(out + 2, s, slen);
    return 2 + slen;
}

/* Build CONNECT packet */
const char *xmqtt_connect(const char *client_id, long long keepalive, long long clean_session) {
    unsigned char pkt[512]; int pos = 0;
    /* Variable header: protocol name + level + flags + keepalive */
    unsigned char varhdr[64]; int vpos = 0;
    vpos += enc_str(varhdr + vpos, "MQTT");
    varhdr[vpos++] = 4; /* protocol level 3.1.1 */
    varhdr[vpos++] = (unsigned char)((clean_session ? 0x02 : 0) | 0x00);
    varhdr[vpos++] = (unsigned char)(keepalive >> 8);
    varhdr[vpos++] = (unsigned char)(keepalive & 0xFF);
    /* Payload: client ID */
    unsigned char payload[256]; int ppos = 0;
    ppos += enc_str(payload + ppos, client_id);
    int remaining = vpos + ppos;
    pkt[pos++] = 0x10; /* CONNECT */
    pos += enc_remaining_len(remaining, pkt + pos);
    memcpy(pkt + pos, varhdr, vpos); pos += vpos;
    memcpy(pkt + pos, payload, ppos); pos += ppos;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build PUBLISH packet (QoS 0) */
const char *xmqtt_publish(const char *topic, const char *payload) {
    unsigned char pkt[4096]; int pos = 0;
    int tlen = (int)strlen(topic);
    int plen = (int)strlen(payload);
    int remaining = 2 + tlen + plen;
    pkt[pos++] = 0x30; /* PUBLISH, QoS 0 */
    pos += enc_remaining_len(remaining, pkt + pos);
    pkt[pos++] = (unsigned char)(tlen >> 8);
    pkt[pos++] = (unsigned char)(tlen & 0xFF);
    memcpy(pkt + pos, topic, tlen); pos += tlen;
    memcpy(pkt + pos, payload, plen); pos += plen;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build SUBSCRIBE packet (QoS 0) */
const char *xmqtt_subscribe(long long packet_id, const char *topic) {
    unsigned char pkt[512]; int pos = 0;
    int tlen = (int)strlen(topic);
    int remaining = 2 + 2 + tlen + 1; /* packet_id + topic_len + topic + qos */
    pkt[pos++] = 0x82; /* SUBSCRIBE */
    pos += enc_remaining_len(remaining, pkt + pos);
    pkt[pos++] = (unsigned char)(packet_id >> 8);
    pkt[pos++] = (unsigned char)(packet_id & 0xFF);
    pkt[pos++] = (unsigned char)(tlen >> 8);
    pkt[pos++] = (unsigned char)(tlen & 0xFF);
    memcpy(pkt + pos, topic, tlen); pos += tlen;
    pkt[pos++] = 0x00; /* QoS 0 */
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Build PINGREQ packet */
const char *xmqtt_pingreq(void) { return "c000"; }

/* Build DISCONNECT packet */
const char *xmqtt_disconnect(void) { return "e000"; }

/* Build CONNACK packet */
const char *xmqtt_connack(long long return_code) {
    unsigned char pkt[4];
    pkt[0] = 0x20; pkt[1] = 0x02; pkt[2] = 0x00; pkt[3] = (unsigned char)return_code;
    to_hex(pkt, 4, hex_buf);
    return hex_buf;
}

/* Get packet type from hex */
long long xmqtt_packet_type(const char *pkt_hex) {
    unsigned int v; sscanf(pkt_hex, "%02x", &v);
    return (long long)(v >> 4);
}

/* Get packet type name */
const char *xmqtt_type_name(long long type) {
    switch(type) {
        case 1: return "CONNECT";
        case 2: return "CONNACK";
        case 3: return "PUBLISH";
        case 4: return "PUBACK";
        case 8: return "SUBSCRIBE";
        case 9: return "SUBACK";
        case 10: return "UNSUBSCRIBE";
        case 12: return "PINGREQ";
        case 13: return "PINGRESP";
        case 14: return "DISCONNECT";
        default: return "UNKNOWN";
    }
}

/* Get QoS level from PUBLISH packet */
long long xmqtt_get_qos(const char *pkt_hex) {
    unsigned int v; sscanf(pkt_hex, "%02x", &v);
    return (long long)((v >> 1) & 0x03);
}

/* Validate topic (no wildcards in publish, basic check) */
long long xmqtt_valid_topic(const char *topic) {
    int len = (int)strlen(topic);
    if (len < 1 || len > 65535) return 0;
    if (strchr(topic, '#') || strchr(topic, '+')) return 0;
    return 1;
}

/* Validate topic filter (allows wildcards) */
long long xmqtt_valid_filter(const char *filter) {
    int len = (int)strlen(filter);
    if (len < 1) return 0;
    /* # must be last char and preceded by / or be alone */
    const char *hash = strchr(filter, '#');
    if (hash && hash[1] != '\0') return 0;
    return 1;
}
