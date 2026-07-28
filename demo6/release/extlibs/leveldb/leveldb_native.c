/* leveldb_native.c - LevelDB SSTable format helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static char hex_buf[8192];
static char str_buf[4096];

static void to_hex(const unsigned char *data, int len, char *out) {
    int i; for (i = 0; i < len; i++) sprintf(out+i*2, "%02x", data[i]); out[len*2] = '\0';
}

/* Encode internal key: user_key + sequence(7 bytes) + type(1 byte) */
const char *xleveldb_internal_key(const char *user_key, long long sequence, long long ktype) {
    unsigned char pkt[512]; int pos = 0;
    int klen = (int)strlen(user_key);
    memcpy(pkt + pos, user_key, klen); pos += klen;
    /* Pack sequence (56 bits) + type (8 bits) into 8 bytes big-endian */
    long long packed = (sequence << 8) | (ktype & 0xFF);
    pkt[pos++] = (unsigned char)(packed >> 56);
    pkt[pos++] = (unsigned char)(packed >> 48);
    pkt[pos++] = (unsigned char)(packed >> 40);
    pkt[pos++] = (unsigned char)(packed >> 32);
    pkt[pos++] = (unsigned char)(packed >> 24);
    pkt[pos++] = (unsigned char)(packed >> 16);
    pkt[pos++] = (unsigned char)(packed >> 8);
    pkt[pos++] = (unsigned char)(packed & 0xFF);
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Extract user key from internal key hex */
const char *xleveldb_user_key(const char *ikey_hex) {
    int total_len = (int)strlen(ikey_hex) / 2;
    int user_len = total_len - 8; /* last 8 bytes are seq+type */
    int i;
    for (i = 0; i < user_len; i++) {
        unsigned int v; sscanf(ikey_hex + i*2, "%02x", &v);
        str_buf[i] = (char)v;
    }
    str_buf[user_len] = '\0';
    return str_buf;
}

/* Extract sequence number from internal key hex */
long long xleveldb_sequence(const char *ikey_hex) {
    int total_len = (int)strlen(ikey_hex) / 2;
    int seq_off = (total_len - 8) * 2; /* hex offset */
    unsigned char bytes[8]; int i;
    for (i = 0; i < 8; i++) {
        unsigned int v; sscanf(ikey_hex + seq_off + i*2, "%02x", &v);
        bytes[i] = (unsigned char)v;
    }
    long long packed = 0;
    for (i = 0; i < 8; i++) packed = (packed << 8) | bytes[i];
    return packed >> 8;
}

/* Extract type from internal key hex */
long long xleveldb_key_type(const char *ikey_hex) {
    int total_len = (int)strlen(ikey_hex) / 2;
    int type_off = (total_len - 1) * 2;
    unsigned int v; sscanf(ikey_hex + type_off, "%02x", &v);
    return (long long)v;
}

/* Key type names */
const char *xleveldb_type_name(long long ktype) {
    switch(ktype) {
        case 0: return "DELETION";
        case 1: return "VALUE";
        default: return "UNKNOWN";
    }
}

/* Encode block entry (shared:varint + non_shared:varint + value_len:varint + key_delta + value) */
/* Simplified: shared(1) + non_shared(1) + vlen(1) + key + value */
const char *xleveldb_block_entry(const char *key, const char *value, long long shared) {
    unsigned char pkt[4096]; int pos = 0;
    int klen = (int)strlen(key);
    int vlen = (int)strlen(value);
    int non_shared = klen - (int)shared;
    pkt[pos++] = (unsigned char)shared;
    pkt[pos++] = (unsigned char)non_shared;
    pkt[pos++] = (unsigned char)vlen;
    memcpy(pkt + pos, key + shared, non_shared); pos += non_shared;
    memcpy(pkt + pos, value, vlen); pos += vlen;
    to_hex(pkt, pos, hex_buf);
    return hex_buf;
}

/* Calculate approximate SSTable size */
long long xleveldb_sst_size(long long num_entries, long long avg_key, long long avg_val) {
    long long data_size = num_entries * (avg_key + avg_val + 16);
    long long index_size = num_entries * 32;
    long long meta_size = 4096;
    return data_size + index_size + meta_size;
}

/* Compression type names */
const char *xleveldb_compression_name(long long ctype) {
    switch(ctype) {
        case 0: return "NONE";
        case 1: return "SNAPPY";
        case 2: return "ZLIB";
        default: return "UNKNOWN";
    }
}

/* Default block size */
long long xleveldb_block_size(void) { return 4096; }

/* Default write buffer size */
long long xleveldb_write_buffer_size(void) { return 4 * 1024 * 1024; }

/* Max open files default */
long long xleveldb_max_open_files(void) { return 1000; }

/* Validate user key */
long long xleveldb_valid_key(const char *key) {
    int len = (int)strlen(key);
    return (len >= 0 && len <= 65535) ? 1 : 0;
}
