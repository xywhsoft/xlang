/*
 * slugify_native.c - URL slug generation with Latin transliteration.
 *
 * ASCII text lowercases, collapses separators, and strips anything
 * outside [a-z0-9]. Latin-1/Latin-Extended letters and common symbols
 * map through a transliteration table (é->e, ü->u, &->and ...).
 * CJK and other non-Latin scripts are preserved as-is between
 * separators so Chinese titles stay readable; callers wanting pure
 * ASCII can check isAscii first.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "xl_runtime.h"

#define XSL_DOMAIN "xlang.extlib.slugify"

static void xsl_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSL_DOMAIN, 0, message);
}

/* Transliterate one Latin-1/Latin-Extended code point; returns the
   replacement ASCII (as malloc'd string) or NULL when none. */
static const char *xsl_latin1(unsigned int cp) {
    static const struct { unsigned int cp; const char *rep; } table[] = {
        {0xE0,"a"},{0xE1,"a"},{0xE2,"a"},{0xE3,"a"},{0xE4,"a"},{0xE5,"a"},
        {0xE6,"ae"},{0xE7,"c"},{0xE8,"e"},{0xE9,"e"},{0xEA,"e"},
        {0xEB,"e"},{0xEC,"i"},{0xED,"i"},{0xEE,"i"},{0xEF,"i"},
        {0xF1,"n"},{0xF2,"o"},{0xF3,"o"},{0xF4,"o"},{0xF5,"o"},
        {0xF6,"o"},{0xF8,"o"},{0xF9,"u"},{0xFA,"u"},{0xFB,"u"},
        {0xFC,"u"},{0xFD,"y"},{0xFF,"y"},
        {0xC0,"a"},{0xC1,"a"},{0xC2,"a"},{0xC3,"a"},{0xC4,"a"},{0xC5,"a"},
        {0xC6,"ae"},{0xC7,"c"},{0xC8,"e"},{0xC9,"e"},{0xCA,"e"},
        {0xCB,"e"},{0xCC,"i"},{0xCD,"i"},{0xCE,"i"},{0xCF,"i"},
        {0xD1,"n"},{0xD2,"o"},{0xD3,"o"},{0xD4,"o"},{0xD5,"o"},
        {0xD6,"o"},{0xD8,"o"},{0xD9,"u"},{0xDA,"u"},{0xDB,"u"},
        {0xDC,"u"},{0xDD,"y"},
        {0x100,"a"},{0x102,"a"},{0x104,"a"},{0x106,"c"},{0x108,"c"},
        {0x10A,"c"},{0x10C,"c"},{0x10E,"d"},{0x110,"d"},{0x112,"e"},
        {0x114,"e"},{0x116,"e"},{0x118,"e"},{0x11A,"e"},{0x11C,"g"},
        {0x11E,"g"},{0x120,"g"},{0x122,"g"},{0x124,"h"},{0x128,"i"},
        {0x12A,"i"},{0x12C,"i"},{0x12E,"i"},{0x130,"i"},{0x132,"ij"},
        {0x134,"j"},{0x136,"k"},{0x139,"l"},{0x13B,"l"},{0x13D,"l"},
        {0x13F,"l"},{0x141,"l"},{0x143,"n"},{0x145,"n"},{0x147,"n"},
        {0x14C,"o"},{0x14E,"o"},{0x150,"o"},{0x152,"oe"},{0x154,"r"},
        {0x156,"r"},{0x158,"r"},{0x15A,"s"},{0x15C,"s"},{0x15E,"s"},
        {0x160,"s"},{0x162,"t"},{0x164,"t"},{0x166,"t"},{0x168,"u"},
        {0x16A,"u"},{0x16C,"u"},{0x16E,"u"},{0x170,"u"},{0x172,"u"},
        {0x174,"w"},{0x176,"y"},{0x178,"y"},{0x179,"z"},{0x17B,"z"},
        {0x17D,"z"},
        {0x1CD,"a"},{0x1CF,"i"},{0x1D1,"o"},{0x1D3,"u"},{0x1E5,"g"},
        {0x1E7,"g"},{0x1E9,"k"},{0x1EB,"e"},{0x1ED,"o"},{0x1EF,"y"},
        {0x1F1,"dz"},{0x1F3,"dz"},{0x1F5,"n"},{0x1F9,"n"},{0x1FB,"a"},
        {0x1FD,"ae"},{0x1FF,"o"},{0x201,"o"},{0x203,"o"},{0x205,"o"},
        {0x207,"o"},{0x209,"o"},{0x20B,"o"},{0x20D,"o"},{0x20F,"o"},
        {0x211,"o"},{0x213,"r"},{0x215,"r"},{0x217,"r"},
        {0x165,"t"},{0x16F,"u"},{0x101,"a"},{0x103,"a"},{0x105,"a"},
        {0x107,"c"},{0x109,"c"},{0x10B,"c"},{0x10D,"c"},{0x10F,"d"},
        {0x111,"d"},{0x113,"e"},{0x115,"e"},{0x117,"e"},{0x119,"e"},
        {0x11B,"e"},{0x11D,"g"},{0x11F,"g"},{0x121,"g"},{0x123,"g"},
        {0x125,"h"},{0x129,"i"},{0x12B,"i"},{0x12D,"i"},{0x12F,"i"},
        {0x131,"i"},{0x133,"ij"},{0x135,"j"},{0x137,"k"},{0x13A,"l"},
        {0x13C,"l"},{0x13E,"l"},{0x140,"l"},{0x142,"l"},{0x144,"n"},
        {0x146,"n"},{0x148,"n"},{0x14D,"o"},{0x14F,"o"},{0x151,"o"},
        {0x153,"oe"},{0x155,"r"},{0x157,"r"},{0x159,"r"},{0x15B,"s"},
        {0x15D,"s"},{0x15F,"s"},{0x161,"s"},{0x163,"t"},{0x167,"t"},
        {0x169,"u"},{0x16B,"u"},{0x16D,"u"},{0x171,"u"},{0x173,"u"},
        {0x175,"w"},{0x177,"y"},{0x17A,"z"},{0x17C,"z"},{0x17E,"z"},
        {0x268,"i"},{0x1E01,"a"},{0x1E03,"b"},{0x1E05,"b"},
        {0x1E07,"b"},{0x1E0B,"d"},{0x1E0F,"d"},{0x1E15,"e"},
        {0x1E17,"e"},{0x1E1B,"e"},{0x1E1D,"e"},{0x1E21,"h"},
        {0x1E25,"h"},{0x1E2B,"h"},{0x1E2F,"i"},{0x1E31,"k"},
        {0x1E37,"l"},{0x1E3B,"l"},{0x1E41,"m"},{0x1E43,"m"},
        {0x1E45,"n"},{0x1E47,"n"},{0x1E49,"n"},{0x1E4B,"n"},
        {0x1E4D,"o"},{0x1E4F,"o"},{0x1E51,"o"},{0x1E53,"o"},
        {0x1E55,"p"},{0x1E57,"p"},{0x1E59,"r"},{0x1E5B,"r"},
        {0x1E5D,"r"},{0x1E5F,"r"},{0x1E61,"s"},{0x1E63,"s"},
        {0x1E65,"s"},{0x1E67,"s"},{0x1E69,"s"},{0x1E6B,"t"},
        {0x1E6D,"t"},{0x1E6F,"t"},{0x1E71,"t"},{0x1E73,"u"},
        {0x1E75,"u"},{0x1E77,"u"},{0x1E79,"u"},{0x1E7B,"u"},
        {0x1E7D,"v"},{0x1E7F,"v"},{0x1E81,"w"},{0x1E83,"w"},
        {0x1E85,"w"},{0x1E87,"w"},{0x1E89,"w"},{0x1E8B,"x"},
        {0x1E8D,"x"},{0x1E8F,"y"},{0x1E91,"z"},{0x1E93,"z"},
        {0x1E95,"z"},{0x1E97,"t"},{0x1E99,"y"},
        {0x2018,"'"},{0x2019,"'"},{0x201C,"\""},{0x201D,"\""},
        {0x2013,"-"},{0x2014,"-"},{0x2026,""},{0xA9,"c"},
        {0xAE,"r"},{0xB0,"deg"},{0xB5,"u"},{0xD7,"x"},{0xF7,"/"},
        {0x2665,""},{0x2666,""},{0x2663,""},{0x2660,""}
    };
    size_t i;
    for (i = 0; i < sizeof(table) / sizeof(table[0]); i++) {
        if (table[i].cp == cp) return table[i].rep;
    }
    return NULL;
}

const char *xsl_slugify(const char *text) {
    static char ring[2][4096];
    static int next;
    char *out = ring[next];
    size_t pos = 0;
    const char *p;
    int lastSep = 1;
    next = (next + 1) % 2;
    out[0] = '\0';
    if (text == NULL) return out;
    for (p = text; *p != '\0';) {
        unsigned char c = (unsigned char)*p;
        unsigned int cp = c;
        int len = 1;
        /* decode UTF-8 code point */
        if ((c & 0xE0) == 0xC0 && p[1] != '\0') {
            cp = ((c & 0x1F) << 6) | (p[1] & 0x3F);
            len = 2;
        } else if ((c & 0xF0) == 0xE0 && p[1] != '\0' && p[2] != '\0') {
            cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            len = 3;
        } else if ((c & 0xF8) == 0xF0 && p[1] != '\0' && p[2] != '\0' &&
                   p[3] != '\0') {
            cp = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) |
                 ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            len = 4;
        }
        if (cp < 0x80) {
            char lower = (char)tolower(c);
            if ((lower >= 'a' && lower <= 'z') ||
                (lower >= '0' && lower <= '9')) {
                if (pos < 4095) out[pos++] = lower;
                lastSep = 0;
            } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
                       c == '-' || c == '_' || c == '.' || c == '/' ||
                       c == '+' || c == '=' || c == '&' || c == '@') {
                /* & -> and, @ -> at handled below; others separate */
                if (c == '&') {
                    if (pos + 3 < 4095) {
                        out[pos++] = 'a';
                        out[pos++] = 'n';
                        out[pos++] = 'd';
                    }
                    lastSep = 0;
                    p += len;
                    continue;
                }
                if (c == '@') {
                    if (pos + 2 < 4095) {
                        out[pos++] = 'a';
                        out[pos++] = 't';
                    }
                    lastSep = 0;
                    p += len;
                    continue;
                }
                if (!lastSep && pos < 4095) {
                    out[pos++] = '-';
                }
                lastSep = 1;
            }
            /* other ASCII punctuation is dropped */
        } else {
            const char *rep = xsl_latin1(cp);
            if (rep != NULL) {
                const char *q;
                for (q = rep; *q != '\0'; q++) {
                    if (pos < 4095) out[pos++] = *q;
                }
                lastSep = 0;
            } else if ((cp >= 0x3000 && cp <= 0x303F) ||
                       (cp >= 0xFF00 && cp <= 0xFFEF)) {
                /* CJK and fullwidth punctuation act as separators */
                if (!lastSep && pos < 4095) {
                    out[pos++] = '-';
                }
                lastSep = 1;
            } else if (cp >= 0x2E80) {
                /* CJK and other scripts: keep the raw UTF-8 bytes */
                int k;
                for (k = 0; k < len; k++) {
                    if (pos < 4095) out[pos++] = p[k];
                }
                lastSep = 0;
            }
            /* Latin letters outside the table and unknown symbols drop */
        }
        p += len;
    }
    while (pos > 0 && out[pos - 1] == '-') pos--;
    out[pos] = '\0';
    return out;
}

long long xsl_is_ascii(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (text == NULL) return 1;
    while (*p != '\0') {
        if (*p >= 0x80) return 0;
        p++;
    }
    return 1;
}
