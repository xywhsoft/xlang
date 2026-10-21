#ifndef XL_EXTLIB_PINYIN_NATIVE_H
#define XL_EXTLIB_PINYIN_NATIVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 返回 XRT 独立字符串，调用方用 xrtFree；错误返回 NULL。 */
void *xpinyin_convert_n(const char *text, size_t size);
void *xpinyin_initials_n(const char *text, size_t size);
void *xpinyin_convert(const char *text);
void *xpinyin_initials(const char *text);

#ifdef __cplusplus
}
#endif
#endif
