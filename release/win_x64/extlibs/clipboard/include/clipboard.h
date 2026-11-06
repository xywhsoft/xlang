#ifndef XL_CLIPBOARD_H
#define XL_CLIPBOARD_H

#include "xrt.h"

typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* xvalue* 结果由调用方调用 xrtValueRelease 释放；错误文本仅借用到下一次错误更新。 */
int64_t x_clipboard_available(void);
const char* x_clipboard_version(void);
const char* x_clipboard_last_error(void);
int64_t x_clipboard_clear(void);
/* 返回拥有型快照；必须调用 x_clipboard_snapshot_release。 */
void* x_clipboard_snapshot(void);
int64_t x_clipboard_restore(void* snapshot);
int64_t x_clipboard_snapshot_release(void* snapshot);
int64_t x_clipboard_sequence_number(void);
int64_t x_clipboard_count_formats(void);
xvalue* x_clipboard_formats(void);
xvalue* x_clipboard_formats_json(void);
xvalue* x_clipboard_format_name(int64_t format_id);
int64_t x_clipboard_register_format(const char* name);
int64_t x_clipboard_is_format_available(int64_t format_id);
int64_t x_clipboard_has_text(void);
int64_t x_clipboard_has_image(void);
int64_t x_clipboard_has_files(void);
xvalue* x_clipboard_get_text(void);
int64_t x_clipboard_set_text(const char* text);
xvalue* x_clipboard_get_files(void);
xvalue* x_clipboard_get_file(void);
int64_t x_clipboard_set_files(xvalue* paths);
int64_t x_clipboard_set_file(const char* path);
int64_t x_clipboard_save_image(const char* path);
int64_t x_clipboard_set_image(const char* path);
xvalue* x_clipboard_image_info_json(void);

#ifdef __cplusplus
}
#endif

#endif
