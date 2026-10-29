#ifndef XANSI_NATIVE_H
#define XANSI_NATIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 文本结果为 XRT 拥有的 UTF-8 char*，调用方用 xrtFree 释放；NULL 表示错误。 */
char* xansi_reset(void);
char* xansi_bold(const char* text);
char* xansi_dim(const char* text);
char* xansi_italic(const char* text);
char* xansi_underline(const char* text);
char* xansi_blink(const char* text);
char* xansi_inverse(const char* text);
char* xansi_hidden(const char* text);
char* xansi_strike(const char* text);
char* xansi_fg(const char* text, int64_t code);
char* xansi_bg(const char* text, int64_t code);
char* xansi_fg256(const char* text, int64_t index);
char* xansi_bg256(const char* text, int64_t index);
char* xansi_fg_true(const char* text, int64_t r, int64_t g, int64_t b);
char* xansi_bg_true(const char* text, int64_t r, int64_t g, int64_t b);
char* xansi_up(int64_t count);
char* xansi_down(int64_t count);
char* xansi_right(int64_t count);
char* xansi_left(int64_t count);
char* xansi_next_line(int64_t count);
char* xansi_prev_line(int64_t count);
char* xansi_column(int64_t column);
char* xansi_clear_screen(void);
char* xansi_clear_line(void);
char* xansi_clear_to_end(void);
char* xansi_hide_cursor(void);
char* xansi_show_cursor(void);
char* xansi_title(const char* text);
char* xansi_strip(const char* text);
int64_t xansi_display_width(const char* text);

#ifdef __cplusplus
}
#endif
#endif
