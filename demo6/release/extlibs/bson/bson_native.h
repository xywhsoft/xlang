#ifndef XL_BSON_NATIVE_H
#define XL_BSON_NATIVE_H

#include <stdint.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"

/* 返回的 xvalue 由调用方 xrtValueRelease；失败返回 null 值并设置 XRT 错误。 */
xvalue* xbson_decode(xvalue* data);
xvalue* xbson_decode_all(xvalue* data);
xvalue* xbson_concat(xvalue* a, xvalue* b);
xvalue* xbson_encode(xvalue* document);
xvalue* xbson_load(const char* path);
int64_t xbson_save(const char* path, xvalue* document);

/* 返回字符串由调用方 xrtFree；失败返回 NULL。 */
char* xbson_to_hex(xvalue* data);
char* xbson_gen_oid(void);
void* xbson_to_hex_abi(xvalue* data);
void* xbson_gen_oid_abi(void);
int64_t xbson_doc_length(xvalue* data);
int64_t xbson_oid_time(const char* hex24);

#endif
