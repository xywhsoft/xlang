/*
 * ansi_native.c - Terminal ANSI escape sequence builders.
 *
 * All functions return strings without side effects; they never touch the
 * console themselves. Static ring buffers keep several results alive in one
 * expression. Escape-aware helpers skip CSI sequences when measuring or
 * stripping text.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define XANSI_RING 8
#define XANSI_TEXT 512

static char xansi_ring[XANSI_RING][XANSI_TEXT];
static int xansi_ring_next = 0;

static char *xansi_slot(void) {
    char *slot = xansi_ring[xansi_ring_next];
    xansi_ring_next = (xansi_ring_next + 1) % XANSI_RING;
    slot[0] = '\0';
    return slot;
}

/* Wrap text in "\x1b[Nm" ... "\x1b[0m". */
static const char *xansi_wrap(const char *text, const char *code) {
    char *slot = xansi_slot();
    snprintf(slot, XANSI_TEXT, "\x1b[%sm%s\x1b[0m", code,
        text ? text : "");
    return slot;
}

/* Wrap with numeric parameters, e.g. fg 256-color: 38;5;N. */
static const char *xansi_wrap_num3(const char *text, const char *prefix,
    long long a, long long b, long long c) {
    char *slot = xansi_slot();
    snprintf(slot, XANSI_TEXT, "\x1b[%s;%lld;%lld;%lldm%s\x1b[0m", prefix,
        a, b, c, text ? text : "");
    return slot;
}

static const char *xansi_sgr1(const char *code) {
    char *slot = xansi_slot();
    snprintf(slot, XANSI_TEXT, "\x1b[%sm", code);
    return slot;
}

static const char *xansi_csi1(const char *seq) {
    char *slot = xansi_slot();
    snprintf(slot, XANSI_TEXT, "\x1b[%s", seq);
    return slot;
}

static const char *xansi_csi_num(const char *suffix, long long n) {
    char *slot = xansi_slot();
    if (n < 0) n = 0;
    snprintf(slot, XANSI_TEXT, "\x1b[%lld%s", n, suffix);
    return slot;
}

const char *xansi_reset(void) {
    return xansi_sgr1("0");
}

const char *xansi_bold(const char *text) { return xansi_wrap(text, "1"); }
const char *xansi_dim(const char *text) { return xansi_wrap(text, "2"); }
const char *xansi_italic(const char *text) { return xansi_wrap(text, "3"); }
const char *xansi_underline(const char *text) { return xansi_wrap(text, "4"); }
const char *xansi_blink(const char *text) { return xansi_wrap(text, "5"); }
const char *xansi_inverse(const char *text) { return xansi_wrap(text, "7"); }
const char *xansi_hidden(const char *text) { return xansi_wrap(text, "8"); }
const char *xansi_strike(const char *text) { return xansi_wrap(text, "9"); }

/* Basic 8/16 color: code 30-37 foreground, 40-47 background, 90-97/100-107 bright. */
const char *xansi_fg(const char *text, long long code) {
    char *slot = xansi_slot();
    if (code < 30 || (code > 37 && code < 90) || code > 97) {
        slot[0] = '\0';
        return slot;
    }
    snprintf(slot, XANSI_TEXT, "\x1b[%lldm%s\x1b[0m", code, text ? text : "");
    return slot;
}

const char *xansi_bg(const char *text, long long code) {
    char *slot = xansi_slot();
    if (code < 40 || (code > 47 && code < 100) || code > 107) {
        slot[0] = '\0';
        return slot;
    }
    snprintf(slot, XANSI_TEXT, "\x1b[%lldm%s\x1b[0m", code, text ? text : "");
    return slot;
}

const char *xansi_fg256(const char *text, long long n) {
    char *slot = xansi_slot();
    if (n < 0 || n > 255) {
        slot[0] = '\0';
        return slot;
    }
    snprintf(slot, XANSI_TEXT, "\x1b[38;5;%lldm%s\x1b[0m", n, text ? text : "");
    return slot;
}

const char *xansi_bg256(const char *text, long long n) {
    char *slot = xansi_slot();
    if (n < 0 || n > 255) {
        slot[0] = '\0';
        return slot;
    }
    snprintf(slot, XANSI_TEXT, "\x1b[48;5;%lldm%s\x1b[0m", n, text ? text : "");
    return slot;
}

const char *xansi_fg_true(const char *text, long long r, long long g, long long b) {
    char *slot = xansi_slot();
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        slot[0] = '\0';
        return slot;
    }
    snprintf(slot, XANSI_TEXT, "\x1b[38;2;%lld;%lld;%lldm%s\x1b[0m", r, g, b,
        text ? text : "");
    return slot;
}

const char *xansi_bg_true(const char *text, long long r, long long g, long long b) {
    char *slot = xansi_slot();
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        slot[0] = '\0';
        return slot;
    }
    snprintf(slot, XANSI_TEXT, "\x1b[48;2;%lld;%lld;%lldm%s\x1b[0m", r, g, b,
        text ? text : "");
    return slot;
}

const char *xansi_up(long long n) { return xansi_csi_num("A", n); }
const char *xansi_down(long long n) { return xansi_csi_num("B", n); }
const char *xansi_right(long long n) { return xansi_csi_num("C", n); }
const char *xansi_left(long long n) { return xansi_csi_num("D", n); }
const char *xansi_next_line(long long n) { return xansi_csi_num("E", n); }
const char *xansi_prev_line(long long n) { return xansi_csi_num("F", n); }
const char *xansi_column(long long n) { return xansi_csi_num("G", n); }

const char *xansi_clear_screen(void) { return xansi_csi1("2J"); }

const char *xansi_clear_line(void) { return xansi_csi1("2K"); }
const char *xansi_clear_to_end(void) { return xansi_csi1("K"); }
const char *xansi_hide_cursor(void) { return xansi_csi1("?25l"); }
const char *xansi_show_cursor(void) { return xansi_csi1("?25h"); }

const char *xansi_title(const char *text) {
    char *slot = xansi_slot();
    snprintf(slot, XANSI_TEXT, "\x1b]0;%s\x07", text ? text : "");
    return slot;
}

/* Skip one escape sequence starting at s (s[0] == 0x1b); returns pointer
   past the sequence end, or NULL when the sequence is incomplete. */
static const char *xansi_skip_sequence(const char *s) {
    const char *p = s + 1;
    if (*p == '[') {
        p++;
        while (*p != '\0' && *p != '@' && !(*p >= 'A' && *p <= 'Z') &&
               !(*p >= 'a' && *p <= 'z')) {
            p++;
        }
        return *p == '\0' ? NULL : p + 1;
    }
    if (*p == ']') {
        p++;
        while (*p != '\0' && *p != '\x07') {
            p++;
        }
        return *p == '\0' ? NULL : p + 1;
    }
    return p + 1;
}

const char *xansi_strip(const char *text) {
    char *slot = xansi_slot();
    const char *p = text;
    size_t pos = 0;
    if (!text) return slot;
    while (*p != '\0') {
        if (*p == '\x1b') {
            const char *next = xansi_skip_sequence(p);
            if (!next) break;
            p = next;
        } else {
            if (pos < XANSI_TEXT - 1) {
                slot[pos++] = *p;
            }
            p++;
        }
    }
    slot[pos] = '\0';
    return slot;
}

long long xansi_width(const char *text) {
    const char *p = text;
    long long width = 0;
    if (!text) return 0;
    while (*p != '\0') {
        if (*p == '\x1b') {
            const char *next = xansi_skip_sequence(p);
            if (!next) break;
            p = next;
        } else {
            width++;
            p++;
        }
    }
    return width;
}

/* Visible width in terminal cells: count UTF-8 code points and treat
   East Asian wide ranges as two cells. */
static long long xansi_utf8_width(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    long long width = 0;
    if (!text) return 0;
    while (*p != '\0') {
        unsigned int cp;
        int len = 1;
        if (*p == 0x1b) {
            const char *next = xansi_skip_sequence((const char *)p);
            if (!next) break;
            p = (const unsigned char *)next;
            continue;
        }
        if (*p < 0x80) {
            cp = *p;
            len = 1;
        } else if ((*p & 0xE0) == 0xC0) {
            cp = *p & 0x1F;
            len = 2;
        } else if ((*p & 0xF0) == 0xE0) {
            cp = *p & 0x0F;
            len = 3;
        } else if ((*p & 0xF8) == 0xF0) {
            cp = *p & 0x07;
            len = 4;
        } else {
            cp = *p;
            len = 1;
        }
        {
            int i;
            for (i = 1; i < len && p[i] != '\0'; i++) {
                cp = (cp << 6) | (p[i] & 0x3F);
            }
            if (p[len - 1] == '\0' && len > 1) {
                len = 1;
            }
        }
        /* CJK wide ranges: approximate common blocks (CJK ideographs,
           Hangul, Kana, fullwidth forms, CJK punctuation). */
        if ((cp >= 0x1100 && cp <= 0x115F) ||
            (cp >= 0x2E80 && cp <= 0x303E) ||
            (cp >= 0x3041 && cp <= 0x33FF) ||
            (cp >= 0x3400 && cp <= 0x4DBF) ||
            (cp >= 0x4E00 && cp <= 0x9FFF) ||
            (cp >= 0xA000 && cp <= 0xA4CF) ||
            (cp >= 0xAC00 && cp <= 0xD7A3) ||
            (cp >= 0xF900 && cp <= 0xFAFF) ||
            (cp >= 0xFE30 && cp <= 0xFE4F) ||
            (cp >= 0xFF00 && cp <= 0xFF60) ||
            (cp >= 0xFFE0 && cp <= 0xFFE6) ||
            (cp >= 0x20000 && cp <= 0x3FFFD)) {
            width += 2;
        } else {
            width += 1;
        }
        p += len;
    }
    return width;
}

long long xansi_display_width(const char *text) {
    return xansi_utf8_width(text);
}
