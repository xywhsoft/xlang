#ifndef XL_JWT_NATIVE_H
#define XL_JWT_NATIVE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 返回文本由调用方使用 xrtFree 释放；失败返回 NULL。 */
char* xjwt_encode(const char* payload_json, const char* secret);
char* xjwt_decode(const char* token);
bool xjwt_verify(const char* token, const char* secret);
char* xjwt_header(const char* token);
const char* xjwt_error(void);

#ifdef __cplusplus
}
#endif

#endif
