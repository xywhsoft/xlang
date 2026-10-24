#ifndef XL_JWT_NATIVE_H
#define XL_JWT_NATIVE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 文本借用当前线程的结果槽；连续八次结果调用后可能失效。 */
const char* xjwt_encode(const char* payload_json, const char* secret);
const char* xjwt_decode(const char* token);
bool xjwt_verify(const char* token, const char* secret);
const char* xjwt_header(const char* token);
const char* xjwt_error(void);

#ifdef __cplusplus
}
#endif

#endif
