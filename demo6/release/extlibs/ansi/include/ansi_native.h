#ifndef XANSI_NATIVE_H
#define XANSI_NATIVE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 文本结果由调用方用 xrtFree 释放；NULL 表示错误。 */
void* xansi_reset(void);
void* xansi_bold(const char* text);
void* xansi_dim(const char* text);
void* xansi_italic(const char* text);
void* xansi_underline(const char* text);
void* xansi_blink(const char* text);
void* xansi_inverse(const char* text);
void* xansi_hidden(const char* text);
void* xansi_strike(const char* text);
void* xansi_fg(const char* text, int64_t code);
void* xansi_bg(const char* text, int64_t code);
void* xansi_fg256(const char* text, int64_t index);
void* xansi_bg256(const char* text, int64_t index);
void* xansi_fg_true(const char* text, int64_t r, int64_t g, int64_t b);
void* xansi_bg_true(const char* text, int64_t r, int64_t g, int64_t b);
void* xansi_up(int64_t count);
void* xansi_down(int64_t count);
void* xansi_right(int64_t count);
void* xansi_left(int64_t count);
void* xansi_next_line(int64_t count);
void* xansi_prev_line(int64_t count);
void* xansi_column(int64_t column);
void* xansi_clear_screen(void);
void* xansi_clear_line(void);
void* xansi_clear_to_end(void);
void* xansi_hide_cursor(void);
void* xansi_show_cursor(void);
void* xansi_title(const char* text);
void* xansi_strip(const char* text);
int64_t xansi_display_width(const char* text);

#ifdef __cplusplus
}
#endif
#endif
