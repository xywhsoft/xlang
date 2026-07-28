/* modbus_native.c - Modbus RTU/TCP frame encoding/parsing */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}
static int from_hex(const char *hex, unsigned char *out) {
    int len = (int)strlen(hex)/2, i;
    for (i = 0; i < len; i++) { unsigned int v; sscanf(hex+i*2, "%02x", &v); out[i] = (unsigned char)v; }
    return len;
}

/* Modbus CRC16 */
static unsigned short crc16(const unsigned char *data, int len) {
    unsigned short crc = 0xFFFF;
    int i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 1) crc = (crc >> 1) ^ 0xA001;
            else crc >>= 1;
        }
    }
    return crc;
}

/* Compute CRC16 of hex data, return as "lo hi" hex */
const char *xmodbus_crc16(const char *data_hex) {
    unsigned char data[512]; int dlen = from_hex(data_hex, data);
    unsigned short crc = crc16(data, dlen);
    sprintf(hex_buf, "%02x%02x", crc & 0xFF, (crc >> 8) & 0xFF);
    return hex_buf;
}

/* Build RTU frame: addr + func + payload + crc */
const char *xmodbus_build_rtu(long long addr, long long func_code, const char *payload_hex) {
    unsigned char frame[512]; int pos = 0;
    frame[pos++] = (unsigned char)addr;
    frame[pos++] = (unsigned char)func_code;
    if (payload_hex[0]) {
        unsigned char payload[256]; int plen = from_hex(payload_hex, payload);
        memcpy(frame + pos, payload, plen); pos += plen;
    }
    unsigned short crc = crc16(frame, pos);
    frame[pos++] = crc & 0xFF;
    frame[pos++] = (crc >> 8) & 0xFF;
    to_hex(frame, pos, hex_buf);
    return hex_buf;
}

/* Build TCP frame (MBAP header): trans_id(2) + proto(2) + len(2) + unit(1) + func(1) + payload */
const char *xmodbus_build_tcp(long long trans_id, long long unit_id, long long func_code, const char *payload_hex) {
    unsigned char frame[512]; int pos = 0;
    unsigned char payload[256]; int plen = 0;
    if (payload_hex[0]) plen = from_hex(payload_hex, payload);
    int pdu_len = 1 + plen; /* func + payload */
    /* MBAP header */
    frame[pos++] = (unsigned char)(trans_id >> 8); frame[pos++] = (unsigned char)trans_id;
    frame[pos++] = 0; frame[pos++] = 0; /* protocol ID */
    frame[pos++] = (unsigned char)((pdu_len + 1) >> 8); frame[pos++] = (unsigned char)(pdu_len + 1); /* length: unit + pdu */
    frame[pos++] = (unsigned char)unit_id;
    frame[pos++] = (unsigned char)func_code;
    memcpy(frame + pos, payload, plen); pos += plen;
    to_hex(frame, pos, hex_buf);
    return hex_buf;
}

/* Parse RTU frame: returns address */
long long xmodbus_rtu_addr(const char *frame_hex) {
    unsigned char frame[512]; from_hex(frame_hex, frame);
    return (long long)frame[0];
}

/* Parse RTU frame: returns function code */
long long xmodbus_rtu_func(const char *frame_hex) {
    unsigned char frame[512]; from_hex(frame_hex, frame);
    return (long long)frame[1];
}

/* Parse RTU frame: returns payload hex (between func and crc) */
const char *xmodbus_rtu_payload(const char *frame_hex) {
    unsigned char frame[512]; int flen = from_hex(frame_hex, frame);
    int plen = flen - 4; /* minus addr(1) + func(1) + crc(2) */
    if (plen <= 0) return "";
    to_hex(frame + 2, plen, hex_buf);
    return hex_buf;
}

/* Verify RTU CRC (returns 1 if valid) */
long long xmodbus_verify_crc(const char *frame_hex) {
    unsigned char frame[512]; int flen = from_hex(frame_hex, frame);
    if (flen < 4) return 0;
    unsigned short calc = crc16(frame, flen - 2);
    unsigned short recv = frame[flen-2] | (frame[flen-1] << 8);
    return calc == recv ? 1 : 0;
}

/* Build Read Coils request (FC01): addr + start_reg(2) + count(2) */
const char *xmodbus_read_coils_req(long long slave, long long start, long long count) {
    char payload[16];
    sprintf(payload, "%02x%02x%02x%02x", (int)(start>>8), (int)(start&0xFF), (int)(count>>8), (int)(count&0xFF));
    return xmodbus_build_rtu(slave, 1, payload);
}

/* Build Read Holding Registers request (FC03) */
const char *xmodbus_read_holding_req(long long slave, long long start, long long count) {
    char payload[16];
    sprintf(payload, "%02x%02x%02x%02x", (int)(start>>8), (int)(start&0xFF), (int)(count>>8), (int)(count&0xFF));
    return xmodbus_build_rtu(slave, 3, payload);
}

/* Build Write Single Register request (FC06) */
const char *xmodbus_write_register_req(long long slave, long long reg, long long value) {
    char payload[16];
    sprintf(payload, "%02x%02x%02x%02x", (int)(reg>>8), (int)(reg&0xFF), (int)(value>>8), (int)(value&0xFF));
    return xmodbus_build_rtu(slave, 6, payload);
}

/* Get function code name */
const char *xmodbus_func_name(long long code) {
    switch(code) {
        case 1: return "Read Coils";
        case 2: return "Read Discrete Inputs";
        case 3: return "Read Holding Registers";
        case 4: return "Read Input Registers";
        case 5: return "Write Single Coil";
        case 6: return "Write Single Register";
        case 15: return "Write Multiple Coils";
        case 16: return "Write Multiple Registers";
        default: return "Unknown";
    }
}
