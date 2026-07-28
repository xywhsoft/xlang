/* archive_native.c - Archive format helpers (tar/zip header parsing) */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Archive format name */
const char* xarch_format_name(long long fmt) {
    switch (fmt) {
        case 0: return "tar";
        case 1: return "zip";
        case 2: return "7z";
        case 3: return "rar";
        case 4: return "gz";
        case 5: return "bz2";
        case 6: return "xz";
        case 7: return "zstd";
        case 8: return "cpio";
        case 9: return "ar";
        default: return "unknown";
    }
}

/* Detect format from magic bytes hex (first 8 bytes) */
long long xarch_detect(const char *magic_hex) {
    unsigned int b[8];
    int n = sscanf(magic_hex, "%02x%02x%02x%02x%02x%02x%02x%02x",
                   &b[0],&b[1],&b[2],&b[3],&b[4],&b[5],&b[6],&b[7]);
    if (n < 2) return -1;
    /* gzip: 1f 8b */
    if (b[0] == 0x1f && b[1] == 0x8b) return 4;
    /* zip: 50 4b 03 04 */
    if (n >= 4 && b[0] == 0x50 && b[1] == 0x4b && b[2] == 0x03 && b[3] == 0x04) return 1;
    /* 7z: 37 7a bc af 27 1c */
    if (n >= 6 && b[0] == 0x37 && b[1] == 0x7a && b[2] == 0xbc && b[3] == 0xaf) return 2;
    /* rar: 52 61 72 21 */
    if (n >= 4 && b[0] == 0x52 && b[1] == 0x61 && b[2] == 0x72 && b[3] == 0x21) return 3;
    /* bz2: 42 5a 68 */
    if (n >= 3 && b[0] == 0x42 && b[1] == 0x5a && b[2] == 0x68) return 5;
    /* xz: fd 37 7a 58 5a 00 */
    if (n >= 6 && b[0] == 0xfd && b[1] == 0x37 && b[2] == 0x7a && b[3] == 0x58) return 6;
    /* zstd: 28 b5 2f fd */
    if (n >= 4 && b[0] == 0x28 && b[1] == 0xb5 && b[2] == 0x2f && b[3] == 0xfd) return 7;
    /* cpio: 30 37 30 37 (070701 or 070707) */
    if (n >= 4 && b[0] == 0x30 && b[1] == 0x37 && b[2] == 0x30 && b[3] == 0x37) return 8;
    /* ar: 21 3c 61 72 63 68 3e */
    if (n >= 7 && b[0] == 0x21 && b[1] == 0x3c && b[2] == 0x61 && b[3] == 0x72) return 9;
    return -1;
}

/* tar file type flag name */
const char* xarch_tar_type(long long flag) {
    switch (flag) {
        case '0': return "regular";
        case '1': return "hardlink";
        case '2': return "symlink";
        case '3': return "chardev";
        case '4': return "blockdev";
        case '5': return "directory";
        case '6': return "fifo";
        case '7': return "contiguous";
        case 'g': return "global_header";
        case 'x': return "extended_header";
        default: return "unknown";
    }
}

/* tar octal size parse (from 12-char octal field as string) */
long long xarch_tar_parse_size(const char *octal_str) {
    long long val = 0;
    const char *p = octal_str;
    while (*p == ' ' || *p == '0') p++;
    while (*p >= '0' && *p <= '7') {
        val = val * 8 + (*p - '0');
        p++;
    }
    return val;
}

/* tar checksum calculation: sum of all bytes in 512-byte header, with checksum field as spaces */
long long xarch_tar_checksum(const char *header_hex) {
    unsigned int b[512];
    int n = 0;
    const char *p = header_hex;
    while (*p && n < 512) {
        unsigned int v;
        if (sscanf(p, "%02x", &v) != 1) break;
        b[n++] = v;
        p += 2;
    }
    /* Checksum field is at offset 148, length 8 - treat as spaces (0x20) */
    long long sum = 0;
    for (int i = 0; i < n; i++) {
        if (i >= 148 && i < 156) sum += 0x20;
        else sum += b[i];
    }
    return sum;
}

/* ZIP compression method name */
const char* xarch_zip_method(long long method) {
    switch (method) {
        case 0: return "stored";
        case 1: return "shrunk";
        case 6: return "imploded";
        case 8: return "deflated";
        case 9: return "deflate64";
        case 10: return "pkware";
        case 12: return "bzip2";
        case 14: return "lzma";
        case 93: return "zstd";
        case 95: return "xz";
        default: return "unknown";
    }
}

/* ZIP version needed to extract */
const char* xarch_zip_version(long long ver) {
    switch (ver) {
        case 10: return "1.0";
        case 11: return "1.1";
        case 20: return "2.0";
        case 21: return "2.1";
        case 45: return "4.5";
        case 46: return "4.6";
        case 50: return "5.0";
        case 51: return "5.1";
        case 61: return "6.1";
        case 62: return "6.2";
        case 63: return "6.3";
        default: return "unknown";
    }
}

/* Parse ZIP local file header from hex, returns compression method; sizes via getters */
static long long zip_comp_size, zip_uncomp_size, zip_fname_len;
long long xarch_zip_local_header(const char *hex) {
    unsigned int b[30];
    int n = 0;
    const char *p = hex;
    while (*p && n < 30) {
        unsigned int v;
        if (sscanf(p, "%02x", &v) != 1) break;
        b[n++] = v;
        p += 2;
    }
    if (n < 30) return -1;
    /* Verify signature 50 4b 03 04 */
    if (b[0] != 0x50 || b[1] != 0x4b || b[2] != 0x03 || b[3] != 0x04) return -1;
    long long method = (long long)(b[8] | (b[9] << 8));
    zip_comp_size = (long long)(b[18] | (b[19] << 8) | (b[20] << 16) | (b[21] << 24));
    zip_uncomp_size = (long long)(b[22] | (b[23] << 8) | (b[24] << 16) | (b[25] << 24));
    zip_fname_len = (long long)(b[26] | (b[27] << 8));
    return method;
}
long long xarch_zip_comp_size(void) { return zip_comp_size; }
long long xarch_zip_uncomp_size(void) { return zip_uncomp_size; }
long long xarch_zip_fname_len(void) { return zip_fname_len; }

/* Compression ratio percentage */
double xarch_ratio(long long compressed, long long uncompressed) {
    if (uncompressed <= 0) return 0;
    return (1.0 - (double)compressed / (double)uncompressed) * 100.0;
}
