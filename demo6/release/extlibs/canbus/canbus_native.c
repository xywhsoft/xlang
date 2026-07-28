/* canbus_native.c - CAN 2.0 frame encoding/decoding */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[256];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}
static int from_hex(const char *hex, unsigned char *out) {
    int len = (int)strlen(hex)/2, i;
    for (i = 0; i < len; i++) { unsigned int v; sscanf(hex+i*2, "%02x", &v); out[i] = (unsigned char)v; }
    return len;
}

/* Build standard CAN frame (11-bit ID): returns hex representation
   Format: [ID_hi(3bits)+ID_lo(8bits)] [DLC] [data 0-8 bytes] */
const char *xcan_build_std(long long id, long long dlc, const char *data_hex) {
    unsigned char frame[16]; int pos = 0;
    if (id > 0x7FF) id = 0x7FF;
    if (dlc > 8) dlc = 8;
    frame[pos++] = (unsigned char)(id >> 8);
    frame[pos++] = (unsigned char)(id & 0xFF);
    frame[pos++] = (unsigned char)dlc;
    if (dlc > 0 && data_hex[0]) {
        unsigned char data[8]; int dlen = from_hex(data_hex, data);
        if (dlen > dlc) dlen = (int)dlc;
        memcpy(frame + pos, data, dlen); pos += dlen;
    }
    to_hex(frame, pos, hex_buf);
    return hex_buf;
}

/* Build extended CAN frame (29-bit ID):
   Format: [ID(4 bytes, bit31=IDE flag)] [DLC] [data] */
const char *xcan_build_ext(long long id, long long dlc, const char *data_hex) {
    unsigned char frame[16]; int pos = 0;
    if (dlc > 8) dlc = 8;
    unsigned long eid = ((unsigned long)id & 0x1FFFFFFF) | 0x80000000UL; /* set IDE bit */
    frame[pos++] = (unsigned char)(eid >> 24);
    frame[pos++] = (unsigned char)(eid >> 16);
    frame[pos++] = (unsigned char)(eid >> 8);
    frame[pos++] = (unsigned char)(eid & 0xFF);
    frame[pos++] = (unsigned char)dlc;
    if (dlc > 0 && data_hex[0]) {
        unsigned char data[8]; int dlen = from_hex(data_hex, data);
        if (dlen > dlc) dlen = (int)dlc;
        memcpy(frame + pos, data, dlen); pos += dlen;
    }
    to_hex(frame, pos, hex_buf);
    return hex_buf;
}

/* Get frame ID (handles both std and ext) */
long long xcan_get_id(const char *frame_hex) {
    unsigned char frame[16]; from_hex(frame_hex, frame);
    /* Check if extended (4-byte header with IDE bit) */
    unsigned long first4 = ((unsigned long)frame[0]<<24)|((unsigned long)frame[1]<<16)|
                           ((unsigned long)frame[2]<<8)|frame[3];
    if (first4 & 0x80000000UL) return (long long)(first4 & 0x1FFFFFFF);
    /* Standard: 2-byte header */
    return (long long)(((unsigned long)frame[0]<<8)|frame[1]);
}

/* Get DLC */
long long xcan_get_dlc(const char *frame_hex) {
    unsigned char frame[16]; from_hex(frame_hex, frame);
    unsigned long first4 = ((unsigned long)frame[0]<<24)|((unsigned long)frame[1]<<16)|
                           ((unsigned long)frame[2]<<8)|frame[3];
    if (first4 & 0x80000000UL) return (long long)frame[4]; /* ext: DLC at byte 4 */
    return (long long)frame[2]; /* std: DLC at byte 2 */
}

/* Get data as hex */
const char *xcan_get_data(const char *frame_hex) {
    unsigned char frame[16]; int flen = from_hex(frame_hex, frame);
    unsigned long first4 = ((unsigned long)frame[0]<<24)|((unsigned long)frame[1]<<16)|
                           ((unsigned long)frame[2]<<8)|frame[3];
    int hdr, dlc;
    if (first4 & 0x80000000UL) { hdr = 5; dlc = frame[4]; }
    else { hdr = 3; dlc = frame[2]; }
    if (dlc > 8) dlc = 8;
    if (hdr + dlc > flen) dlc = flen - hdr;
    if (dlc <= 0) return "";
    to_hex(frame + hdr, dlc, hex_buf);
    return hex_buf;
}

/* Is extended frame? */
long long xcan_is_extended(const char *frame_hex) {
    unsigned char frame[16]; from_hex(frame_hex, frame);
    unsigned long first4 = ((unsigned long)frame[0]<<24)|((unsigned long)frame[1]<<16)|
                           ((unsigned long)frame[2]<<8)|frame[3];
    return (first4 & 0x80000000UL) ? 1 : 0;
}

/* Get total frame byte length */
long long xcan_frame_len(const char *frame_hex) {
    return (long long)(strlen(frame_hex) / 2);
}
