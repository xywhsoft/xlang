/* lmdb_native.c - LMDB-style key-value store helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];
static char str_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}

/* LMDB page size (default 4096) */
long long xlmdb_page_size(void) { return 4096; }

/* Calculate number of pages needed for data */
long long xlmdb_pages_needed(long long data_size, long long page_size) {
    if (page_size <= 0) page_size = 4096;
    return (data_size + page_size - 1) / page_size;
}

/* Encode key-value pair as hex (key_len:2 + key + val_len:4 + val) */
const char *xlmdb_encode_kv(const char *key, const char *value) {
    unsigned char pkt[4096]; int pos = 0;
    int klen = (int)strlen(key);
    int vlen = (int)strlen(value);
    pkt[pos++] = (unsigned char)(klen >> 8);
    pkt[pos++] = (unsigned char)(klen & 0xFF);
    memcpy(pkt + pos, key, klen); pos += klen;
    pkt[pos++] = (unsigned char)(vlen >> 24);
    pkt[pos++] = (unsigned char)(vlen >> 16);
    pkt[pos++] = (unsigned char)(vlen >> 8);
    pkt[pos++] = (unsigned char)(vlen & 0xFF);
    memcpy(pkt + pos, value, vlen); pos += vlen;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Decode key from encoded KV hex */
const char *xlmdb_decode_key(const char *kv_hex) {
    unsigned char pkt[4096]; int len = (int)strlen(kv_hex)/2, i;
    for (i = 0; i < len && i < 4096; i++) { unsigned int v; sscanf(kv_hex+i*2, "%02x", &v); pkt[i] = (unsigned char)v; }
    int klen = (pkt[0] << 8) | pkt[1];
    memcpy(str_buf, pkt + 2, klen);
    str_buf[klen] = '\0';
    return str_buf;
}

/* Decode value from encoded KV hex */
const char *xlmdb_decode_value(const char *kv_hex) {
    unsigned char pkt[4096]; int len = (int)strlen(kv_hex)/2, i;
    for (i = 0; i < len && i < 4096; i++) { unsigned int v; sscanf(kv_hex+i*2, "%02x", &v); pkt[i] = (unsigned char)v; }
    int klen = (pkt[0] << 8) | pkt[1];
    int voff = 2 + klen;
    int vlen = (pkt[voff] << 24) | (pkt[voff+1] << 16) | (pkt[voff+2] << 8) | pkt[voff+3];
    memcpy(str_buf, pkt + voff + 4, vlen);
    str_buf[vlen] = '\0';
    return str_buf;
}

/* Compare keys (lexicographic) */
long long xlmdb_key_compare(const char *key1, const char *key2) {
    int cmp = strcmp(key1, key2);
    if (cmp < 0) return -1;
    if (cmp > 0) return 1;
    return 0;
}

/* Validate key (non-empty, max 511 bytes) */
long long xlmdb_valid_key(const char *key) {
    int len = (int)strlen(key);
    return (len >= 1 && len <= 511) ? 1 : 0;
}

/* Calculate database size estimate */
long long xlmdb_db_size(long long num_entries, long long avg_key_len, long long avg_val_len) {
    long long entry_size = 2 + avg_key_len + 4 + avg_val_len + 8; /* overhead */
    long long total = num_entries * entry_size;
    long long pages = (total + 4095) / 4096;
    return pages * 4096;
}

/* Transaction flags */
long long xlmdb_flag_rdonly(void) { return 0x20000; }
long long xlmdb_flag_nosync(void) { return 0x10000; }
long long xlmdb_flag_nometasync(void) { return 0x40000; }

/* Environment flags */
long long xlmdb_env_fixedmap(void) { return 0x01; }
long long xlmdb_env_nosubdir(void) { return 0x4000; }
long long xlmdb_env_readonly(void) { return 0x20000; }

/* Max key size */
long long xlmdb_max_key_size(void) { return 511; }

/* Max databases default */
long long xlmdb_max_dbs_default(void) { return 128; }
