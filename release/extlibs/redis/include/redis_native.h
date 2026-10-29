#ifndef XL_EXTLIB_REDIS_NATIVE_H
#define XL_EXTLIB_REDIS_NATIVE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xvalue xvalue;
typedef struct xstrview xstrview;
typedef struct xbytesview xbytesview;

/* 输入借用，返回 xvalue 由调用方 xrtValueRelease；错误由 XRT 通道报告。 */
xvalue* xr_encode(xvalue* args);
/* 支持 string/bytes 参数；返回 xbuffer*，由调用方 xrtBufferDestroy。 */
void* xr_encode_bytes(xvalue* args);
xvalue* xr_decode(xvalue* reply);
bool xr_is_complete(xvalue* reply);
xvalue* xr_decode_raw(void* data, size_t size);
bool xr_is_complete_raw(void* data, size_t size);
xvalue* xr_decode_text_span(xstrview reply);
bool xr_is_complete_text_span(xstrview reply);
xvalue* xr_decode_bytes_span(xbytesview reply);
bool xr_is_complete_bytes_span(xbytesview reply);
xvalue* xr_args_create(void);
bool xr_args_append(xvalue* args, xvalue* item);
bool xr_args_native(xvalue* args);

/* 增量检查首个 RESP 帧；data 必须是同一缓冲的追加前缀，允许重新分配地址。
   返回 false 且无 XRT 错误表示数据未到齐；失败时销毁状态，不再继续追加。 */
void* xr_frame_create(void);
bool xr_frame_check(void* frame, void* data, size_t size);
void xr_frame_destroy(void* frame);

#ifdef __cplusplus
}
#endif

#endif
