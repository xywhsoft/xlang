/* chardet_native.c - Character encoding detection (simplified uchardet) */
#include <string.h>
#include <stdlib.h>

static char result_buf[256];

/* Check if buffer is valid UTF-8, return confidence 0-100 */
static int check_utf8(const unsigned char *data, int len) {
    int i = 0, multibyte = 0, errors = 0;
    while (i < len) {
        unsigned char c = data[i];
        int expect = 0;
        if (c < 0x80) { i++; continue; }
        else if ((c & 0xE0) == 0xC0) { expect = 1; }
        else if ((c & 0xF0) == 0xE0) { expect = 2; }
        else if ((c & 0xF8) == 0xF0) { expect = 3; }
        else { errors++; i++; continue; }
        multibyte++;
        int j;
        for (j = 0; j < expect; j++) {
            if (i + 1 + j >= len || (data[i+1+j] & 0xC0) != 0x80) { errors++; break; }
        }
        i += 1 + expect;
    }
    if (errors > 0) return 0;
    if (multibyte == 0) return 0; /* pure ASCII, not interesting */
    /* higher confidence with more multibyte sequences */
    if (multibyte > 10) return 99;
    if (multibyte > 3) return 90;
    return 75;
}

/* Check GBK encoding validity */
static int check_gbk(const unsigned char *data, int len) {
    int i = 0, pairs = 0, errors = 0;
    while (i < len) {
        unsigned char c = data[i];
        if (c < 0x80) { i++; continue; }
        if (c >= 0x81 && c <= 0xFE) {
            if (i + 1 < len) {
                unsigned char t = data[i+1];
                if ((t >= 0x40 && t <= 0x7E) || (t >= 0x80 && t <= 0xFE)) {
                    pairs++; i += 2; continue;
                }
            }
            errors++; i++;
        } else { errors++; i++; }
    }
    if (errors > 2 || pairs == 0) return 0;
    if (pairs > 10) return 85;
    if (pairs > 3) return 70;
    return 50;
}

/* Check Shift_JIS validity */
static int check_sjis(const unsigned char *data, int len) {
    int i = 0, pairs = 0, katakana = 0, errors = 0;
    while (i < len) {
        unsigned char c = data[i];
        if (c < 0x80) { i++; continue; }
        if (c >= 0xA1 && c <= 0xDF) { katakana++; i++; continue; }
        if ((c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xEF)) {
            if (i + 1 < len) {
                unsigned char t = data[i+1];
                if ((t >= 0x40 && t <= 0x7E) || (t >= 0x80 && t <= 0xFC)) {
                    pairs++; i += 2; continue;
                }
            }
            errors++; i++;
        } else { errors++; i++; }
    }
    if (errors > 2) return 0;
    if (pairs == 0 && katakana == 0) return 0;
    if (pairs + katakana > 10) return 80;
    if (pairs + katakana > 3) return 65;
    return 45;
}

/* Check EUC-KR validity */
static int check_euckr(const unsigned char *data, int len) {
    int i = 0, pairs = 0, errors = 0;
    while (i < len) {
        unsigned char c = data[i];
        if (c < 0x80) { i++; continue; }
        if (c >= 0xA1 && c <= 0xFE) {
            if (i + 1 < len) {
                unsigned char t = data[i+1];
                if (t >= 0xA1 && t <= 0xFE) { pairs++; i += 2; continue; }
            }
            errors++; i++;
        } else { errors++; i++; }
    }
    if (errors > 2 || pairs == 0) return 0;
    if (pairs > 10) return 80;
    if (pairs > 3) return 60;
    return 40;
}

/* Check if all bytes are ASCII */
static int check_ascii(const unsigned char *data, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (data[i] >= 0x80) return 0;
    }
    return 100;
}

/*
 * xchardet_detect - detect encoding of data
 * data: input bytes (as string)
 * len: length
 * Returns: encoding name string
 */
long long xchardet_detect(const char *data, long long len, char *out) {
    const unsigned char *d = (const unsigned char *)data;
    int n = (int)len;
    int conf;

    /* BOM detection */
    if (n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF) {
        strcpy(out, "UTF-8-BOM");
        return 100;
    }
    if (n >= 2 && d[0] == 0xFF && d[1] == 0xFE) {
        strcpy(out, "UTF-16LE");
        return 100;
    }
    if (n >= 2 && d[0] == 0xFE && d[1] == 0xFF) {
        strcpy(out, "UTF-16BE");
        return 100;
    }

    /* ASCII check */
    conf = check_ascii(d, n);
    if (conf > 0) {
        strcpy(out, "ASCII");
        return conf;
    }

    /* UTF-8 check */
    int utf8_conf = check_utf8(d, n);
    int gbk_conf = check_gbk(d, n);
    int sjis_conf = check_sjis(d, n);
    int euckr_conf = check_euckr(d, n);

    /* Pick highest confidence */
    int best = utf8_conf;
    const char *best_name = "UTF-8";
    if (gbk_conf > best) { best = gbk_conf; best_name = "GBK"; }
    if (sjis_conf > best) { best = sjis_conf; best_name = "Shift_JIS"; }
    if (euckr_conf > best) { best = euckr_conf; best_name = "EUC-KR"; }

    if (best > 0) {
        strcpy(out, best_name);
        return best;
    }

    /* Fallback */
    strcpy(out, "ISO-8859-1");
    return 30;
}

/* Wrapper: detect encoding, return name via static buffer */
static char detect_name_buf[64];
const char *xchardet_detect_name(const char *data) {
    long long len = (long long)strlen(data);
    char tmp[64];
    xchardet_detect(data, len, tmp);
    strcpy(detect_name_buf, tmp);
    return detect_name_buf;
}

long long xchardet_detect_conf(const char *data) {
    long long len = (long long)strlen(data);
    char tmp[64];
    return xchardet_detect(data, len, tmp);
}
