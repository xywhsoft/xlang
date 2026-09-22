/*
 * qr_native.c - QR code generation with Reed-Solomon error correction.
 *
 * Implements the full encoding pipeline: mode selection (numeric,
 * alphanumeric, byte), version selection 1-40, data interleaving across
 * RS blocks, function module drawing, zigzag data placement, all eight
 * mask patterns with penalty scoring, and format/version BCH coding.
 * Level names follow the standard: L (7%), M (15%), Q (25%), H (30%).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"

#define XQR_DOMAIN "xlang.extlib.qr"
#define XQR_MAX_VERSION 40
#define XQR_MAX_SIZE 177

static void xqr_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XQR_DOMAIN, 0, message);
}

/* ------------------------------------------------------------------ */
/* Spec tables                                                         */
/* ------------------------------------------------------------------ */

/* Error correction codewords per block, indexed [level][version]. */
static const int xqr_ecc_per_block[4][XQR_MAX_VERSION + 1] = {
    {0, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28,
     30, 28, 28, 28, 28, 30, 30, 26, 28, 28, 28, 28, 28, 28, 28, 28, 30,
     30, 30, 30, 30, 30, 30},
    {0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28,
     26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
     28, 28, 28, 28, 28, 28},
    {0, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28,
     28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30,
     30, 30, 30, 30, 30, 30},
    {0, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28,
     28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
     30, 30, 30, 30, 30, 30},
};

/* Number of error correction blocks, indexed [level][version]. */
static const int xqr_num_blocks[4][XQR_MAX_VERSION + 1] = {
    {0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8, 8, 9,
     9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16,
     17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43,
     45, 47, 49},
    {0, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21,
     20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56,
     59, 62, 65, 68},
    {0, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25,
     25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66,
     70, 74, 77, 81},
};

/* Alignment pattern center coordinates; row 0 is unused. */
static const int xqr_align_centers[][7] = {
    {0},
    {6, 0}, {6, 18}, {6, 22}, {6, 26}, {6, 30}, {6, 34},
    {6, 22, 38}, {6, 24, 42}, {6, 26, 46}, {6, 28, 50},
    {6, 30, 54}, {6, 32, 58}, {6, 34, 62},
    {6, 26, 46, 66}, {6, 26, 48, 70}, {6, 26, 50, 74},
    {6, 30, 54, 78}, {6, 30, 56, 82}, {6, 30, 58, 86}, {6, 34, 62, 90},
    {6, 28, 50, 72, 94}, {6, 26, 50, 74, 98}, {6, 30, 54, 78, 102},
    {6, 28, 54, 80, 106}, {6, 32, 58, 84, 110}, {6, 30, 58, 86, 114},
    {6, 34, 62, 90, 118},
    {6, 26, 50, 74, 98, 122}, {6, 30, 54, 78, 102, 126},
    {6, 26, 52, 78, 104, 130}, {6, 30, 56, 82, 108, 134},
    {6, 34, 60, 86, 112, 138}, {6, 30, 58, 86, 114, 142},
    {6, 34, 62, 90, 118, 146},
    {6, 30, 54, 78, 102, 126, 150}, {6, 24, 50, 76, 102, 128, 154},
    {6, 28, 54, 80, 106, 132, 158}, {6, 32, 58, 84, 110, 136, 162},
    {6, 26, 54, 82, 110, 138, 166}, {6, 30, 58, 86, 114, 142, 170},
};

static const char *xqr_alnum_charset =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

/* Format info bits per level: L, M, Q, H. */
static const int xqr_ecl_format_bits[4] = {1, 0, 3, 2};

/* ------------------------------------------------------------------ */
/* GF(256) and Reed-Solomon                                            */
/* ------------------------------------------------------------------ */

static int xqr_gf_mul(int x, int y) {
    int z = 0;
    int i;
    for (i = 7; i >= 0; i--) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return z & 0xFF;
}

/* Generator polynomial of the given degree, big-endian including the
   leading 1: g(x) = prod_{i=0}^{degree-1} (x + alpha^i). */
static void xqr_rs_generator(int degree, unsigned char *gen) {
    int len = 2;
    int i;
    int k;
    unsigned char alpha = 1;
    gen[0] = 1;
    gen[1] = 1;
    for (i = 1; i < degree; i++) {
        alpha = (unsigned char)xqr_gf_mul(alpha, 2);
        gen[len] = 0;
        for (k = len; k > 0; k--) {
            gen[k] = (unsigned char)(gen[k] ^ xqr_gf_mul(alpha, gen[k - 1]));
        }
        len++;
    }
}

/* Remainder of data(x) * x^degree modulo the generator, via synthetic
   division over [data, zeros] with the full generator polynomial. */
static void xqr_rs_remainder(const unsigned char *data, int dataLen,
    int degree, unsigned char *out) {
    unsigned char gen[70];
    unsigned char buf[3800];
    int i;
    int j;
    xqr_rs_generator(degree, gen);
    memset(buf, 0, sizeof(buf));
    memcpy(buf, data, (size_t)dataLen);
    for (i = 0; i < dataLen; i++) {
        int coef = buf[i];
        if (coef != 0) {
            for (j = 0; j <= degree; j++) {
                buf[i + j] ^= (unsigned char)xqr_gf_mul(gen[j], coef);
            }
        }
    }
    memcpy(out, buf + dataLen, (size_t)degree);
}

/* ------------------------------------------------------------------ */
/* Version capacity                                                    */
/* ------------------------------------------------------------------ */

static int xqr_raw_data_modules(int ver) {
    int result = (16 * ver + 128) * ver + 64;
    if (ver >= 2) {
        int numAlign = ver / 7 + 2;
        result -= (25 * numAlign - 10) * numAlign - 55;
        if (ver >= 7) {
            result -= 36;
        }
    }
    return result;
}

static int xqr_total_codewords(int ver) {
    return xqr_raw_data_modules(ver) / 8;
}

static int xqr_data_codewords(int ver, int level) {
    return xqr_total_codewords(ver) -
        xqr_num_blocks[level][ver] * xqr_ecc_per_block[level][ver];
}

static int xqr_char_count_bits(int mode, int ver) {
    if (ver <= 9) {
        return mode == 0 ? 10 : (mode == 1 ? 9 : 8);
    }
    if (ver <= 26) {
        return mode == 0 ? 12 : (mode == 1 ? 11 : 16);
    }
    return mode == 0 ? 14 : (mode == 1 ? 13 : 16);
}

/* ------------------------------------------------------------------ */
/* Bit buffer                                                          */
/* ------------------------------------------------------------------ */

typedef struct {
    unsigned char bits[4 * 3706];
    int length;
} XqrBits;

static void xqr_append_bits(XqrBits *b, unsigned int value, int count) {
    int i;
    for (i = count - 1; i >= 0; i--) {
        b->bits[b->length >> 3] &= (unsigned char)~(1 << (b->length & 7));
        b->bits[b->length >> 3] |= (unsigned char)(((value >> i) & 1) <<
            (b->length & 7));
        b->length++;
    }
}

/* Detect the best single mode for the whole text. */
static int xqr_detect_mode(const char *text, int *numeric, int *alnum) {
    const unsigned char *p = (const unsigned char *)text;
    int mode = 0;
    int i;
    *numeric = 1;
    *alnum = 1;
    for (i = 0; text[i] != '\0'; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            *numeric = 0;
        }
        if (strchr(xqr_alnum_charset, p[i]) == NULL) {
            *alnum = 0;
        }
    }
    if (text[0] == '\0') {
        *numeric = 0;
        *alnum = 0;
    }
    if (*numeric) mode = 0;
    else if (*alnum) mode = 1;
    else mode = 2;
    return mode;
}

static int xqr_payload_bits(const char *text, int mode) {
    int n = (int)strlen(text);
    if (mode == 0) {
        return (n / 3) * 10 + (n % 3 == 2 ? 7 : (n % 3 == 1 ? 4 : 0));
    }
    if (mode == 1) {
        return (n / 2) * 11 + (n % 2) * 6;
    }
    {
        /* UTF-8 byte count equals strlen for the raw buffer */
        return n * 8;
    }
}

static int xqr_pick_version(const char *text, int mode, int level,
    int *capacityBits) {
    int v;
    for (v = 1; v <= XQR_MAX_VERSION; v++) {
        int dataBits = 4 + xqr_char_count_bits(mode, v) +
            xqr_payload_bits(text, mode);
        int cap = xqr_data_codewords(v, level) * 8;
        if (dataBits <= cap) {
            *capacityBits = cap;
            return v;
        }
    }
    return 0;
}

static void xqr_encode_payload(XqrBits *b, const char *text, int mode,
    int ver) {
    int n = (int)strlen(text);
    int i;
    xqr_append_bits(b, (unsigned int)(mode == 0 ? 1 : (mode == 1 ? 2 : 4)),
        4);
    xqr_append_bits(b, (unsigned int)n, xqr_char_count_bits(mode, ver));
    if (mode == 0) {
        for (i = 0; i < n; i += 3) {
            int group = n - i;
            if (group >= 3) {
                xqr_append_bits(b,
                    (unsigned int)((text[i] - '0') * 100 +
                        (text[i + 1] - '0') * 10 + (text[i + 2] - '0')), 10);
            } else if (group == 2) {
                xqr_append_bits(b,
                    (unsigned int)((text[i] - '0') * 10 +
                        (text[i + 1] - '0')), 7);
            } else {
                xqr_append_bits(b, (unsigned int)(text[i] - '0'), 4);
            }
        }
    } else if (mode == 1) {
        for (i = 0; i < n; i += 2) {
            int a = (int)(strchr(xqr_alnum_charset, text[i]) -
                xqr_alnum_charset);
            if (i + 1 < n) {
                int c = (int)(strchr(xqr_alnum_charset, text[i + 1]) -
                    xqr_alnum_charset);
                xqr_append_bits(b, (unsigned int)(a * 45 + c), 11);
            } else {
                xqr_append_bits(b, (unsigned int)a, 6);
            }
        }
    } else {
        for (i = 0; i < n; i++) {
            xqr_append_bits(b, (unsigned char)text[i], 8);
        }
    }
}

/* Full codeword sequence: data blocks interleaved with EC codewords. */
static int xqr_build_codewords(const char *text, int level,
    unsigned char *out) {
    int mode;
    int version;
    int capacityBits = 0;
    XqrBits bits;
    int dataCap;
    int i;
    int j;
    int numBlocks;
    int eccLen;
    int dataLen;
    int smallDc;
    int largeBlocks;
    int pos = 0;
    unsigned char *allData;
    unsigned char ecc[81 * 30];

    mode = xqr_detect_mode(text, &i, &j);
    (void)i;
    (void)j;
    version = xqr_pick_version(text, mode, level, &capacityBits);
    if (version == 0) {
        xqr_error("text is too long for any qr version at this level");
        return 0;
    }
    memset(&bits, 0, sizeof(bits));
    xqr_encode_payload(&bits, text, mode, version);
    /* terminator */
    xqr_append_bits(&bits, 0,
        (bits.length + 4 <= capacityBits) ? 4 : (capacityBits - bits.length));
    /* pad to byte boundary with zeros (already zero) */
    bits.length = (bits.length + 7) / 8 * 8;
    dataCap = xqr_data_codewords(version, level);
    /* pad codewords */
    for (i = bits.length / 8; i < dataCap; i++) {
        xqr_append_bits(&bits, (i & 1) ? 0x11 : 0xEC, 8);
    }

    numBlocks = xqr_num_blocks[level][version];
    eccLen = xqr_ecc_per_block[level][version];
    dataLen = dataCap;
    smallDc = dataLen / numBlocks;
    largeBlocks = dataLen % numBlocks;
    allData = bits.bits;

    /* collect EC codewords per block */
    {
        int block = 0;
        int offset = 0;
        for (block = 0; block < numBlocks; block++) {
            int dc = smallDc + (block >= numBlocks - largeBlocks ? 1 : 0);
            xqr_rs_remainder(allData + offset, dc, eccLen,
                ecc + block * eccLen);
            offset += dc;
        }
    }
    /* interleave data */
    {
        int maxDc = smallDc + (largeBlocks > 0 ? 1 : 0);
        int block = 0;
        int idx = 0;
        int offset = 0;
        for (idx = 0; idx < maxDc; idx++) {
            offset = 0;
            for (block = 0; block < numBlocks; block++) {
                int dc = smallDc + (block >= numBlocks - largeBlocks ? 1 : 0);
                if (idx < dc) {
                    out[pos++] = allData[offset + idx];
                }
                offset += dc;
            }
        }
    }
    /* interleave EC */
    {
        int block = 0;
        int idx = 0;
        for (idx = 0; idx < eccLen; idx++) {
            for (block = 0; block < numBlocks; block++) {
                out[pos++] = ecc[block * eccLen + idx];
            }
        }
    }
    return version;
}

/* ------------------------------------------------------------------ */
/* Matrix                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    int size;
    unsigned char modules[XQR_MAX_SIZE][XQR_MAX_SIZE];
    unsigned char isFunction[XQR_MAX_SIZE][XQR_MAX_SIZE];
} XqrMatrix;

static void xqr_set_fn(XqrMatrix *m, int x, int y, int dark) {
    m->modules[y][x] = (unsigned char)(dark ? 1 : 0);
    m->isFunction[y][x] = 1;
}

static void xqr_draw_finder(XqrMatrix *m, int x, int y) {
    int dy;
    int dx;
    for (dy = -4; dy <= 4; dy++) {
        for (dx = -4; dx <= 4; dx++) {
            int dist = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < m->size && py >= 0 && py < m->size) {
                xqr_set_fn(m, px, py, dist != 2 && dist != 4);
            }
        }
    }
}

static void xqr_draw_align(XqrMatrix *m, int x, int y) {
    int dy;
    int dx;
    for (dy = -2; dy <= 2; dy++) {
        for (dx = -2; dx <= 2; dx++) {
            int dist = abs(dx) > abs(dy) ? abs(dx) : abs(dy);
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < m->size && py >= 0 && py < m->size) {
                xqr_set_fn(m, px, py, dist != 1);
            }
        }
    }
}

static void xqr_draw_format(XqrMatrix *m, int eclBits, int mask) {
    int data = (eclBits << 3) | mask;
    int rem = data;
    int i;
    int bits;
    for (i = 0; i < 10; i++) {
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    }
    bits = ((data << 10) | rem) ^ 0x5412;
    /* first copy */
    for (i = 0; i <= 5; i++) {
        xqr_set_fn(m, 8, i, (bits >> i) & 1);
    }
    xqr_set_fn(m, 8, 7, (bits >> 6) & 1);
    xqr_set_fn(m, 8, 8, (bits >> 7) & 1);
    xqr_set_fn(m, 7, 8, (bits >> 8) & 1);
    for (i = 9; i < 15; i++) {
        xqr_set_fn(m, 14 - i, 8, (bits >> i) & 1);
    }
    /* second copy */
    for (i = 0; i < 8; i++) {
        xqr_set_fn(m, m->size - 1 - i, 8, (bits >> i) & 1);
    }
    for (i = 8; i < 15; i++) {
        xqr_set_fn(m, 8, m->size - 15 + i, (bits >> i) & 1);
    }
    xqr_set_fn(m, 8, m->size - 8, 1); /* always-dark module */
}

static void xqr_draw_version(XqrMatrix *m, int version) {
    int rem = version;
    int bits;
    int i;
    if (version < 7) return;
    for (i = 0; i < 12; i++) {
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    }
    bits = (version << 12) | rem;
    for (i = 0; i < 18; i++) {
        int bit = (bits >> i) & 1;
        int a = m->size - 11 + i % 3;
        int b = i / 3;
        xqr_set_fn(m, a, b, bit);
        xqr_set_fn(m, b, a, bit);
    }
}

static int xqr_mask_bit(int mask, int x, int y) {
    switch (mask) {
        case 0: return (x + y) % 2 == 0;
        case 1: return y % 2 == 0;
        case 2: return x % 3 == 0;
        case 3: return (x + y) % 3 == 0;
        case 4: return (x / 3 + y / 2) % 2 == 0;
        case 5: return (x * y) % 2 + (x * y) % 3 == 0;
        case 6: return ((x * y) % 2 + (x * y) % 3) % 2 == 0;
        default: return ((x + y) % 2 + (x * y) % 3) % 2 == 0;
    }
}

static void xqr_draw_function_patterns(XqrMatrix *m, int version, int eclBits,
    int mask) {
    int i;
    int n = m->size;
    /* version 1 has no alignment patterns; v2+ has version/7+2 of them */
    int numAlign = (version == 1) ? 0 : version / 7 + 2;
    const int *centers = xqr_align_centers[version];
    for (i = 0; i < n; i++) {
        xqr_set_fn(m, 6, i, i % 2 == 0);
        xqr_set_fn(m, i, 6, i % 2 == 0);
    }
    xqr_draw_finder(m, 3, 3);
    xqr_draw_finder(m, n - 4, 3);
    xqr_draw_finder(m, 3, n - 4);
    for (i = 0; i < numAlign; i++) {
        int j;
        for (j = 0; j < numAlign; j++) {
            if (!((i == 0 && j == 0) || (i == 0 && j == numAlign - 1) ||
                  (i == numAlign - 1 && j == 0))) {
                xqr_draw_align(m, centers[j], centers[i]);
            }
        }
    }
    xqr_draw_format(m, eclBits, mask);
    xqr_draw_version(m, version);
    /* reserve format areas before data placement by drawing twice is not
       needed: format is drawn last with the chosen mask, and the areas
       were already marked as function modules by the first draw. */
}

static void xqr_draw_codewords(XqrMatrix *m, const unsigned char *data,
    int dataLen, int mask) {
    int i = 0;
    int right;
    int n = m->size;
    for (right = n - 1; right >= 1; right -= 2) {
        int vert;
        if (right == 6) right = 5;
        for (vert = 0; vert < n; vert++) {
            int j;
            for (j = 0; j < 2; j++) {
                int x = right - j;
                int upward = ((right + 1) & 2) == 0;
                int y = upward ? n - 1 - vert : vert;
                if (!m->isFunction[y][x] && i < dataLen * 8) {
                    m->modules[y][x] = (unsigned char)
                        (((data[i >> 3] >> (7 - (i & 7))) & 1) ^
                         (xqr_mask_bit(mask, x, y) ? 1 : 0));
                    i++;
                }
            }
        }
    }
}

static long xqr_penalty(const XqrMatrix *m) {
    int n = m->size;
    long result = 0;
    int y;
    int x;
    /* rule 1: runs */
    for (y = 0; y < n; y++) {
        int runColor = m->modules[y][0];
        int runLen = 1;
        for (x = 1; x < n; x++) {
            if (m->modules[y][x] == runColor) {
                runLen++;
            } else {
                if (runLen >= 5) result += runLen - 2;
                runColor = m->modules[y][x];
                runLen = 1;
            }
        }
        if (runLen >= 5) result += runLen - 2;
    }
    for (x = 0; x < n; x++) {
        int runColor = m->modules[0][x];
        int runLen = 1;
        for (y = 1; y < n; y++) {
            if (m->modules[y][x] == runColor) {
                runLen++;
            } else {
                if (runLen >= 5) result += runLen - 2;
                runColor = m->modules[y][x];
                runLen = 1;
            }
        }
        if (runLen >= 5) result += runLen - 2;
    }
    /* rule 2: 2x2 blocks */
    for (y = 0; y < n - 1; y++) {
        for (x = 0; x < n - 1; x++) {
            int c = m->modules[y][x];
            if (c == m->modules[y][x + 1] &&
                c == m->modules[y + 1][x] &&
                c == m->modules[y + 1][x + 1]) {
                result += 3;
            }
        }
    }
    /* rule 3: finder-like patterns (simplified 1:1:3:1:1 ratios) */
    {
        static const int pattern[] = {1, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0};
        int k;
        for (y = 0; y < n; y++) {
            for (x = 0; x <= n - 11; x++) {
                int match1 = 1;
                int match2 = 1;
                for (k = 0; k < 11; k++) {
                    if (m->modules[y][x + k] != pattern[k]) match1 = 0;
                    if (m->modules[y][x + k] != pattern[10 - k]) match2 = 0;
                }
                if (match1 || match2) result += 40;
            }
        }
        for (x = 0; x < n; x++) {
            for (y = 0; y <= n - 11; y++) {
                int match1 = 1;
                int match2 = 1;
                for (k = 0; k < 11; k++) {
                    if (m->modules[y + k][x] != pattern[k]) match1 = 0;
                    if (m->modules[y + k][x] != pattern[10 - k]) match2 = 0;
                }
                if (match1 || match2) result += 40;
            }
        }
    }
    /* rule 4: dark ratio */
    {
        long dark = 0;
        long total = (long)n * n;
        for (y = 0; y < n; y++) {
            for (x = 0; x < n; x++) {
                if (m->modules[y][x]) dark++;
            }
        }
        {
            long k = (dark * 100 / total - 50) / 5;
            if (k < 0) k = -k;
            result += k * 10;
        }
    }
    return result;
}

/* Build the full matrix; returns the version or 0 on failure. */
static int xqr_build_matrix(const char *text, int level, XqrMatrix *m) {
    unsigned char data[3706];
    int version;
    int mask;
    int bestMask = 0;
    long bestPenalty = -1;
    memset(m, 0, sizeof(*m));
    version = xqr_build_codewords(text, level, data);
    if (version == 0) {
        return 0;
    }
    m->size = version * 4 + 17;
    for (mask = 0; mask < 8; mask++) {
        memset(m->modules, 0, sizeof(m->modules));
        memset(m->isFunction, 0, sizeof(m->isFunction));
        xqr_draw_function_patterns(m, version, xqr_ecl_format_bits[level],
            mask);
        xqr_draw_codewords(m, data, xqr_total_codewords(version), mask);
        {
            long penalty = xqr_penalty(m);
            if (bestPenalty < 0 || penalty < bestPenalty) {
                bestPenalty = penalty;
                bestMask = mask;
            }
        }
    }
    /* redraw with the best mask */
    memset(m->modules, 0, sizeof(m->modules));
    memset(m->isFunction, 0, sizeof(m->isFunction));
    xqr_draw_function_patterns(m, version, xqr_ecl_format_bits[level],
        bestMask);
    xqr_draw_codewords(m, data, xqr_total_codewords(version), bestMask);
    return version;
}

static int xqr_parse_level(const char *level) {
    if (level == NULL) return -1;
    if (strcmp(level, "L") == 0 || strcmp(level, "l") == 0) return 0;
    if (strcmp(level, "M") == 0 || strcmp(level, "m") == 0) return 1;
    if (strcmp(level, "Q") == 0 || strcmp(level, "q") == 0) return 2;
    if (strcmp(level, "H") == 0 || strcmp(level, "h") == 0) return 3;
    return -1;
}

static int xqr_build_checked(const char *text, const char *level,
    XqrMatrix *m) {
    int lv = xqr_parse_level(level);
    int version;
    if (lv < 0) {
        xqr_error("level must be one of L, M, Q, H");
        return 0;
    }
    if (text == NULL || text[0] == '\0') {
        xqr_error("qr text must not be empty");
        return 0;
    }
    version = xqr_build_matrix(text, lv, m);
    return version;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

static xvalue *xqr_null(void) {
    return xrtValueNull();
}

xvalue *xqr_matrix(const char *text, const char *level) {
    XqrMatrix m;
    xvalue *rows;
    int version;
    int y;
    version = xqr_build_checked(text, level, &m);
    if (version == 0) {
        return xqr_null();
    }
    rows = xrtValueArray();
    if (rows == NULL) return xqr_null();
    for (y = 0; y < m.size; y++) {
        xvalue *row = xrtValueArray();
        int x;
        if (row == NULL ||
            !xrtValueArrayAppendNew(rows, row)) {
            return xqr_null();
        }
        for (x = 0; x < m.size; x++) {
            if (!xrtValueArrayAppendNew(row,
                    xrtValueInt(m.modules[y][x]))) {
                return xqr_null();
            }
        }
    }
    return rows;
}

long long xqr_version(const char *text, const char *level) {
    XqrMatrix m;
    return xqr_build_checked(text, level, &m);
}

const char *xqr_ascii(const char *text, const char *level,
    const char *dark, const char *light) {
    static char ring[2][70000];
    static int next;
    XqrMatrix m;
    char *out;
    int version;
    size_t pos = 0;
    int y;
    int x;
    int border = 2;
    int stride;
    next = (next + 1) % 2;
    out = ring[next];
    out[0] = '\0';
    version = xqr_build_checked(text, level, &m);
    if (version == 0) return out;
    if (dark == NULL || dark[0] == '\0') dark = "##";
    if (light == NULL || light[0] == '\0') light = "  ";
    stride = (int)strlen(dark) > (int)strlen(light)
        ? (int)strlen(dark) : (int)strlen(light);
    for (y = -border; y < m.size + border; y++) {
        for (x = -border; x < m.size + border; x++) {
            int inRange = x >= 0 && x < m.size && y >= 0 && y < m.size;
            int darkCell = inRange && m.modules[y][x];
            const char *cell = darkCell ? dark : light;
            if (pos + stride + 2 < sizeof(ring[0])) {
                strcpy(out + pos, cell);
                pos += strlen(cell);
            }
        }
        if (pos + 2 < sizeof(ring[0])) {
            out[pos++] = '\n';
        }
    }
    out[pos] = '\0';
    return out;
}

const char *xqr_utf8(const char *text, const char *level) {
    /* half-block rendering: two rows per output line using U+2580/2584 */
    static char ring[2][60000];
    static int next;
    XqrMatrix m;
    char *out;
    int version;
    size_t pos = 0;
    int y;
    int x;
    int border = 2;
    next = (next + 1) % 2;
    out = ring[next];
    out[0] = '\0';
    version = xqr_build_checked(text, level, &m);
    if (version == 0) return out;
    for (y = -border; y < m.size + border; y += 2) {
        for (x = -border; x < m.size + border; x++) {
            int inTop = x >= 0 && x < m.size && y >= 0 && y < m.size;
            int inBot = x >= 0 && x < m.size && y + 1 >= 0 &&
                        y + 1 < m.size;
            int top = inTop && m.modules[y][x];
            int bot = inBot && m.modules[y + 1][x];
            const char *cell;
            if (top && bot) cell = "\xE2\x96\x88";
            else if (top) cell = "\xE2\x96\x80";
            else if (bot) cell = "\xE2\x96\x84";
            else cell = " ";
            if (pos + 4 < sizeof(ring[0])) {
                strcpy(out + pos, cell);
                pos += strlen(cell);
            }
        }
        if (pos + 2 < sizeof(ring[0])) {
            out[pos++] = '\n';
        }
    }
    out[pos] = '\0';
    return out;
}

const char *xqr_svg(const char *text, const char *level,
    long long border, long long scale) {
    static char ring[2][80000];
    static int next;
    XqrMatrix m;
    char *out;
    int version;
    size_t pos = 0;
    int y;
    int x;
    char header[512];
    next = (next + 1) % 2;
    out = ring[next];
    out[0] = '\0';
    if (border < 0) border = 0;
    if (scale < 1) scale = 1;
    version = xqr_build_checked(text, level, &m);
    if (version == 0) return out;
    {
        long long dim = (m.size + border * 2) * scale;
        snprintf(header, sizeof(header),
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
            "viewBox=\"0 0 %lld %lld\" shape-rendering=\"crispEdges\">\n"
            "  <rect width=\"100%%\" height=\"100%%\" fill=\"#ffffff\"/>\n"
            "  <path fill=\"#000000\" d=\"",
            dim, dim);
        strcpy(out, header);
        pos = strlen(out);
    }
    for (y = 0; y < m.size; y++) {
        for (x = 0; x < m.size; x++) {
            if (m.modules[y][x]) {
                char cell[64];
                snprintf(cell, sizeof(cell), "M%lld,%lldh%lldv%lldh-%lldz",
                    (long long)(x + border) * scale,
                    (long long)(y + border) * scale,
                    scale, scale, scale);
                if (pos + strlen(cell) + 1 < sizeof(ring[0]) - 40) {
                    strcpy(out + pos, cell);
                    pos += strlen(cell);
                }
            }
        }
    }
    strcpy(out + pos, "\"/>\n</svg>\n");
    return out;
}
