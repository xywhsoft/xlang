#ifndef XL_SLUGIFY_NATIVE_H
#define XL_SLUGIFY_NATIVE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 返回独立 XRT 字符串；失败返回 NULL，C 调用方用 xrtFree。 */
char* xsl_slugify(const char* text, long long length);
/* 1 ASCII、0 非 ASCII、-1 参数错误。 */
int xsl_is_ascii(const char* text, long long length);

#ifdef __cplusplus
}
#endif
#endif
