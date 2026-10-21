#ifndef XDIFF_NATIVE_H
#define XDIFF_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 返回独立文本，调用方以 xrtFree 释放；NULL 表示失败。 */
void* xdiff_lines(const char* old_text, const char* new_text);
void* xdiff_stats(const char* old_text, const char* new_text);

#ifdef __cplusplus
}
#endif
#endif
