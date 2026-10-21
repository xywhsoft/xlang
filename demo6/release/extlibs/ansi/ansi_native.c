/* ANSI 文本构件：只生成字节序列，不修改终端状态。结果由调用方持有。 */
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#ifndef XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_STRING_FORMAT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "include/ansi_native.h"
#include <limits.h>
#include <string.h>

#define XANSI_DOMAIN "xlang.extlib.ansi"

static void xansi_error(const char* message)
{
    xrtClearError();
    xrtSetErrorInfo(XERR_VALUE, XANSI_DOMAIN, 0, message);
}

static void* xansi_copy(const char* text)
{
    xrtClearError();
    return xrtStrDup(text);
}

static const char* xansi_text(const char* text)
{
    return text != NULL ? text : "";
}

/* 双向控制符会改变终端文本的可见顺序，不能作为安全标题或列宽输入。 */
static bool xansi_bidi_control(uint32 scalar)
{
    return scalar == 0x061cu || (scalar >= 0x200eu && scalar <= 0x200fu) ||
        (scalar >= 0x202au && scalar <= 0x202eu) ||
        (scalar >= 0x2066u && scalar <= 0x206fu);
}

static bool xansi_line_break(uint32 scalar)
{
    return scalar == 0x2028u || scalar == 0x2029u;
}

static void* xansi_wrap(const char* text, const char* code)
{
    xrtClearError();
    return xrtFormat("\x1b[%sm%s\x1b[0m", code, xansi_text(text));
}

static void* xansi_color(const char* text, int64_t code)
{
    xrtClearError();
    return xrtFormat("\x1b[%lldm%s\x1b[0m", (long long)code, xansi_text(text));
}

static void* xansi_palette(const char* text, int foreground, int64_t index)
{
    xrtClearError();
    if (index < 0 || index > 255) {
        xansi_error("palette index must be 0..255");
        return NULL;
    }
    return xrtFormat("\x1b[%d;5;%lldm%s\x1b[0m", foreground ? 38 : 48,
        (long long)index, xansi_text(text));
}

static void* xansi_rgb(const char* text, int foreground,
    int64_t red, int64_t green, int64_t blue)
{
    xrtClearError();
    if (red < 0 || red > 255 || green < 0 || green > 255 ||
        blue < 0 || blue > 255) {
        xansi_error("RGB channel must be 0..255");
        return NULL;
    }
    return xrtFormat("\x1b[%d;2;%lld;%lld;%lldm%s\x1b[0m",
        foreground ? 38 : 48, (long long)red, (long long)green,
        (long long)blue, xansi_text(text));
}

static void* xansi_move(char direction, int64_t count)
{
    xrtClearError();
    if (count < 1) {
        xansi_error("cursor count or column must be positive");
        return NULL;
    }
    return xrtFormat("\x1b[%lld%c", (long long)count, direction);
}

void* xansi_reset(void) { return xansi_copy("\x1b[0m"); }
void* xansi_bold(const char* text) { return xansi_wrap(text, "1"); }
void* xansi_dim(const char* text) { return xansi_wrap(text, "2"); }
void* xansi_italic(const char* text) { return xansi_wrap(text, "3"); }
void* xansi_underline(const char* text) { return xansi_wrap(text, "4"); }
void* xansi_blink(const char* text) { return xansi_wrap(text, "5"); }
void* xansi_inverse(const char* text) { return xansi_wrap(text, "7"); }
void* xansi_hidden(const char* text) { return xansi_wrap(text, "8"); }
void* xansi_strike(const char* text) { return xansi_wrap(text, "9"); }

void* xansi_fg(const char* text, int64_t code)
{
    if (!((code >= 30 && code <= 37) || (code >= 90 && code <= 97))) {
        xansi_error("foreground SGR code must be 30..37 or 90..97");
        return NULL;
    }
    return xansi_color(text, code);
}

void* xansi_bg(const char* text, int64_t code)
{
    if (!((code >= 40 && code <= 47) || (code >= 100 && code <= 107))) {
        xansi_error("background SGR code must be 40..47 or 100..107");
        return NULL;
    }
    return xansi_color(text, code);
}

void* xansi_fg256(const char* text, int64_t index)
{ return xansi_palette(text, 1, index); }
void* xansi_bg256(const char* text, int64_t index)
{ return xansi_palette(text, 0, index); }
void* xansi_fg_true(const char* text, int64_t r, int64_t g, int64_t b)
{ return xansi_rgb(text, 1, r, g, b); }
void* xansi_bg_true(const char* text, int64_t r, int64_t g, int64_t b)
{ return xansi_rgb(text, 0, r, g, b); }

void* xansi_up(int64_t count) { return xansi_move('A', count); }
void* xansi_down(int64_t count) { return xansi_move('B', count); }
void* xansi_right(int64_t count) { return xansi_move('C', count); }
void* xansi_left(int64_t count) { return xansi_move('D', count); }
void* xansi_next_line(int64_t count) { return xansi_move('E', count); }
void* xansi_prev_line(int64_t count) { return xansi_move('F', count); }
void* xansi_column(int64_t column) { return xansi_move('G', column); }

void* xansi_clear_screen(void) { return xansi_copy("\x1b[2J"); }
void* xansi_clear_line(void) { return xansi_copy("\x1b[2K"); }
void* xansi_clear_to_end(void) { return xansi_copy("\x1b[K"); }
void* xansi_hide_cursor(void) { return xansi_copy("\x1b[?25l"); }
void* xansi_show_cursor(void) { return xansi_copy("\x1b[?25h"); }

void* xansi_title(const char* text)
{
    size_t i = 0u, size;
    xrtClearError();
    text = xansi_text(text);
    size = strlen(text);
    while (i < size) {
        size_t read = 0u;
        uint32 scalar;
        if (xrtUtf8Decode((xstrview){text + i, size - i},
            &scalar, &read) != XUTF_OK) {
            xansi_error("title must be valid UTF-8");
            return NULL;
        }
        if (scalar < 32u || (scalar >= 127u && scalar <= 159u) ||
            xansi_bidi_control(scalar) || xansi_line_break(scalar)) {
            xansi_error("title cannot contain terminal or bidi controls");
            return NULL;
        }
        i += read;
    }
    return xrtFormat("\x1b]0;%s\x07", text);
}

static bool xansi_osc_text(const char* text, size_t size)
{
    size_t i = 0u;
    while (i < size) {
        uint32 scalar;
        size_t read = 0u;
        if (xrtUtf8Decode((xstrview){text + i, size - i},
            &scalar, &read) != XUTF_OK || read == 0u || scalar < 32u ||
            (scalar >= 127u && scalar <= 159u) ||
            xansi_bidi_control(scalar) || xansi_line_break(scalar)) return false;
        i += read;
    }
    return true;
}

/* 只识别完整的 CSI 与 OSC；残缺或未知 ESC 不做静默吞噬。 */
static bool xansi_escape(const char* text, size_t size, size_t* consumed)
{
    size_t i = 2;
    if (size < 3u) return false;
    if (text[1] == '[') {
        while (i < size && (unsigned char)text[i] >= 0x30u &&
            (unsigned char)text[i] <= 0x3fu) ++i;
        while (i < size && (unsigned char)text[i] >= 0x20u &&
            (unsigned char)text[i] <= 0x2fu) ++i;
        if (i == size || (unsigned char)text[i] < 0x40u ||
            (unsigned char)text[i] > 0x7eu) return false;
        *consumed = i + 1u;
        return true;
    }
    if (text[1] == ']') {
        while (i < size) {
            if (text[i] == '\a') {
                if (!xansi_osc_text(text + 2, i - 2u)) return false;
                *consumed = i + 1u;
                return true;
            }
            if (text[i] == '\x1b' && i + 1u < size && text[i + 1u] == '\\') {
                if (!xansi_osc_text(text + 2, i - 2u)) return false;
                *consumed = i + 2u;
                return true;
            }
            if ((unsigned char)text[i] < 32u) return false;
            ++i;
        }
    }
    return false;
}

void* xansi_strip(const char* text)
{
    size_t size, read = 0u, written = 0u;
    char* result;
    xrtClearError();
    text = xansi_text(text);
    size = strlen(text);
    if (!xrtUtf8Valid((xstrview){text, size}, NULL)) {
        xansi_error("input must be valid UTF-8");
        return NULL;
    }
    result = (char*)xrtMalloc(size + 1u);
    if (result == NULL) return NULL;
    while (read < size) {
        size_t escaped;
        if (text[read] == '\x1b') {
            if (!xansi_escape(text + read, size - read, &escaped)) goto invalid;
            read += escaped;
        } else {
            result[written++] = text[read++];
        }
    }
    result[written] = '\0';
    return result;
invalid:
    xrtFree(result);
    xansi_error("invalid or incomplete ANSI escape sequence");
    return NULL;
}

static int xansi_cell_width(uint32 scalar)
{
    if ((scalar >= 0x0300u && scalar <= 0x036fu) ||
        (scalar >= 0x1ab0u && scalar <= 0x1affu) ||
        (scalar >= 0x1dc0u && scalar <= 0x1dffu) ||
        (scalar >= 0x20d0u && scalar <= 0x20ffu) ||
        (scalar >= 0xfe00u && scalar <= 0xfe0fu) ||
        (scalar >= 0xfe20u && scalar <= 0xfe2fu) ||
        (scalar >= 0xe0100u && scalar <= 0xe01efu) ||
        (scalar >= 0x200bu && scalar <= 0x200du) ||
        (scalar >= 0x2060u && scalar <= 0x2064u) ||
        scalar == 0xfeffu) return 0;
    if ((scalar >= 0x1100u && scalar <= 0x115fu) ||
        (scalar >= 0x2329u && scalar <= 0x232au) ||
        (scalar >= 0x2e80u && scalar <= 0xa4cfu) ||
        (scalar >= 0xac00u && scalar <= 0xd7a3u) ||
        (scalar >= 0xf900u && scalar <= 0xfaffu) ||
        (scalar >= 0xfe10u && scalar <= 0xfe19u) ||
        (scalar >= 0xfe30u && scalar <= 0xfe6fu) ||
        (scalar >= 0xff00u && scalar <= 0xff60u) ||
        (scalar >= 0xffe0u && scalar <= 0xffe6u) ||
        (scalar >= 0x1f300u && scalar <= 0x1faffu) ||
        (scalar >= 0x20000u && scalar <= 0x3fffdu)) return 2;
    return 1;
}

int64_t xansi_display_width(const char* text)
{
    size_t size, position = 0u;
    int64_t width = 0;
    xrtClearError();
    text = xansi_text(text);
    size = strlen(text);
    while (position < size) {
        size_t consumed = 0u;
        uint32 scalar;
        if (text[position] == '\x1b') {
            if (!xansi_escape(text + position, size - position, &consumed)) goto invalid;
            position += consumed;
            continue;
        }
        if (xrtUtf8Decode((xstrview){text + position, size - position},
            &scalar, &consumed) != XUTF_OK || scalar < 32u ||
            (scalar >= 127u && scalar <= 159u) ||
            xansi_bidi_control(scalar) || xansi_line_break(scalar))
            goto invalid;
        if (width > INT64_MAX - 2) goto invalid;
        width += xansi_cell_width(scalar);
        position += consumed;
    }
    return width;
invalid:
    xansi_error("displayWidth requires single-line UTF-8 without bidi controls and valid ANSI sequences");
    return -1;
}
