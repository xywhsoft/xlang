#ifndef XL_EXTLIB_REDIS_NATIVE_H
#define XL_EXTLIB_REDIS_NATIVE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xvalue xvalue;

/* 输入借用，返回 xvalue 由调用方 xrtValueRelease；错误由 XRT 通道报告。 */
xvalue* xr_encode(xvalue* args);
/* 支持 string/bytes 参数；返回 xbuffer*，由调用方 xrtBufferDestroy。 */
void* xr_encode_bytes(xvalue* args);
xvalue* xr_decode(xvalue* reply);
bool xr_is_complete(xvalue* reply);
xvalue* xr_decode_raw(void* data, size_t size);
bool xr_is_complete_raw(void* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
