/* unqlite_native.c - UnQLite embedded database helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];
static char str_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}

/* Page header: magic(4) + page_size(2) + page_count(4) + free_pages(4) */
const char *xunqlite_db_header(long long page_size, long long page_count) {
    unsigned char pkt[16]; int pos = 0;
    /* Magic: "UQDB" */
    pkt[pos++] = 'U'; pkt[pos++] = 'Q'; pkt[pos++] = 'D'; pkt[pos++] = 'B';
    pkt[pos++] = (unsigned char)(page_size >> 8);
    pkt[pos++] = (unsigned char)(page_size & 0xFF);
    pkt[pos++] = (unsigned char)(page_count >> 24);
    pkt[pos++] = (unsigned char)(page_count >> 16);
    pkt[pos++] = (unsigned char)(page_count >> 8);
    pkt[pos++] = (unsigned char)(page_count & 0xFF);
    /* Free pages (0 initially) */
    pkt[pos++] = 0; pkt[pos++] = 0; pkt[pos++] = 0; pkt[pos++] = 0;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Record header: key_len(2) + val_len(4) + flags(1) + next_page(4) */
const char *xunqlite_record(const char *key, const char *value) {
    unsigned char pkt[4096]; int pos = 0;
    int klen = (int)strlen(key);
    int vlen = (int)strlen(value);
    pkt[pos++] = (unsigned char)(klen >> 8);
    pkt[pos++] = (unsigned char)(klen & 0xFF);
    pkt[pos++] = (unsigned char)(vlen >> 24);
    pkt[pos++] = (unsigned char)(vlen >> 16);
    pkt[pos++] = (unsigned char)(vlen >> 8);
    pkt[pos++] = (unsigned char)(vlen & 0xFF);
    pkt[pos++] = 0; /* flags */
    pkt[pos++] = 0; pkt[pos++] = 0; pkt[pos++] = 0; pkt[pos++] = 0; /* next_page */
    memcpy(pkt + pos, key, klen); pos += klen;
    memcpy(pkt + pos, value, vlen); pos += vlen;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Extract key from record hex */
const char *xunqlite_rec_key(const char *rec_hex) {
    unsigned char pkt[4096]; int len = (int)strlen(rec_hex)/2, i;
    for (i = 0; i < len && i < 4096; i++) { unsigned int v; sscanf(rec_hex+i*2, "%02x", &v); pkt[i] = (unsigned char)v; }
    int klen = (pkt[0] << 8) | pkt[1];
    memcpy(str_buf, pkt + 11, klen); /* header is 11 bytes */
    str_buf[klen] = '\0';
    return str_buf;
}

/* Extract value from record hex */
const char *xunqlite_rec_value(const char *rec_hex) {
    unsigned char pkt[4096]; int len = (int)strlen(rec_hex)/2, i;
    for (i = 0; i < len && i < 4096; i++) { unsigned int v; sscanf(rec_hex+i*2, "%02x", &v); pkt[i] = (unsigned char)v; }
    int klen = (pkt[0] << 8) | pkt[1];
    int vlen = (pkt[2] << 24) | (pkt[3] << 16) | (pkt[4] << 8) | pkt[5];
    int voff = 11 + klen;
    memcpy(str_buf, pkt + voff, vlen);
    str_buf[vlen] = '\0';
    return str_buf;
}

/* Calculate pages needed for record */
long long xunqlite_pages_for_record(long long key_len, long long val_len, long long page_size) {
    long long rec_size = 11 + key_len + val_len;
    if (page_size <= 0) page_size = 4096;
    return (rec_size + page_size - 1) / page_size;
}

/* Hash function for key (simple FNV-1a) */
long long xunqlite_hash_key(const char *key) {
    unsigned long long hash = 14695981039346656037ULL;
    int i, len = (int)strlen(key);
    for (i = 0; i < len; i++) {
        hash ^= (unsigned char)key[i];
        hash *= 1099511628211ULL;
    }
    return (long long)(hash & 0x7FFFFFFFFFFFFFFFLL);
}

/* Cursor direction names */
const char *xunqlite_cursor_name(long long dir) {
    switch(dir) {
        case 0: return "FIRST";
        case 1: return "LAST";
        case 2: return "NEXT";
        case 3: return "PREV";
        case 4: return "SEEK";
        default: return "UNKNOWN";
    }
}

/* Transaction state names */
const char *xunqlite_tx_state(long long state) {
    switch(state) {
        case 0: return "NONE";
        case 1: return "ACTIVE";
        case 2: return "COMMITTED";
        case 3: return "ROLLED_BACK";
        default: return "UNKNOWN";
    }
}

/* Default page size */
long long xunqlite_page_size(void) { return 4096; }

/* Max key size */
long long xunqlite_max_key_size(void) { return 65535; }

/* Validate key */
long long xunqlite_valid_key(const char *key) {
    int len = (int)strlen(key);
    return (len >= 1 && len <= 65535) ? 1 : 0;
}
