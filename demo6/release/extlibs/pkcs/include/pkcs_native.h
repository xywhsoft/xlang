#ifndef PKCS_NATIVE_H
#define PKCS_NATIVE_H

#include <stdint.h>
#include "xrt.h"

typedef struct xvalue xvalue;

#ifdef __cplusplus
extern "C" {
#endif

/* 文本结果由 xrtFree 释放；NULL 表示失败，详情见 xrtGetError。 */
int64_t xpkcs_get_tag(const char* hex);
int64_t xpkcs_get_length(const char* hex);
void* xpkcs_get_content(const char* hex);
void* xpkcs_encode_int(int64_t number);
int xpkcs_decode_int(const char* hex, int64_t* output);
/* XLang 桥接结果；C 调用方以 xrtValueRelease 释放。 */
xvalue* xpkcs_decode_int_value(const char* hex);
void* xpkcs_encode_oid(const char* dotted);
void* xpkcs_decode_oid(const char* hex);
void* xpkcs_wrap_sequence(const char* content_hex);
void* xpkcs_wrap_set(const char* content_hex);
void* xpkcs_encode_octet(const char* content_hex);
void* xpkcs_encode_null(void);
void* xpkcs_encode_bitstring(const char* content_hex);
void* xpkcs_concat(const char* left_hex, const char* right_hex);
int64_t xpkcs_der_len(const char* hex);
int64_t xpkcs_next_offset(const char* hex, int64_t offset);
void* xpkcs_tag_name(const char* hex);

#ifdef __cplusplus
}
#endif
#endif
