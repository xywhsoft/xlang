#ifndef XDIFF_NATIVE_H
#define XDIFF_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 输入必须是有效的零结尾文本，空指针报 XERR_ARGUMENT；返回值由调用方 xrtFree。 */
char* xdiff_lines(const char* old_text, const char* new_text);
char* xdiff_stats(const char* old_text, const char* new_text);

#ifdef __cplusplus
}
#endif
#endif
